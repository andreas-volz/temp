#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* local */
#include "EdjeView.h"
#include "EdjeContext.h"

/* Eflxx */
#include <elementaryxx/Elementaryxx.h>

/* stateval */
#include "stateval/stateval.h"
#include "stateval/private/stateval_private.h"
#include "EdjeWidget.h"

/* STD */
#include <iostream>

using namespace std;

EdjeView::EdjeView(EdjeContext *context, const std::string &dir, const std::map <std::string, std::string> &params) :
  mLogger("stateval.plugins.viewmanager.edje.EdjeView"),
  mEdjeContext(context),
  groupState(Unrealized),
  mEvent (-1)
{
  assert(context);
  
  std::map <std::string, std::string>::const_iterator param_it;

  param_it = params.find ("filename");
  if (param_it != params.end ())
  {
    if(dir.empty())
    {
      mFilename = param_it->second;
    }
    else
    {
      mFilename = dir + "/" + param_it->second;
    }    
  }
  else
  {
    // TODO: needed error handling
    assert (false);      
  }

  param_it = params.find ("groupname");
  if (param_it != params.end ())
  {
    mGroupname = param_it->second;
  }
  else
  {
    // TODO: needed error handling
    assert (false);      
  }

  LOG4CXX_TRACE(mLogger, "constructor: " << mFilename << "," << mGroupname);

  mRealizeDispatcher.signalDispatch.connect(sigc::mem_fun(this, &EdjeView::realizeDispatched));
  mUnrealizeDispatcher.signalDispatch.connect(sigc::mem_fun(this, &EdjeView::unrealizeDispatched));
  mPushEventDispatcher.signalDispatch.connect(sigc::mem_fun(this, &EdjeView::pushEventDispatched));
  mUpdateDispatcher.signalDispatch.connect(sigc::mem_fun(this, &EdjeView::updateDispatched));
}

void EdjeView::realize()
{
  LOG4CXX_DEBUG(mLogger, "+wait for realize");
  mRealizeDispatcher.emit();

  mMutexRealize.lock();
  mCondRealize.wait(mMutexRealize);
  mMutexRealize.unlock();
  LOG4CXX_DEBUG(mLogger, "-wait for realize");
}

void EdjeView::unrealize()
{
  LOG4CXX_TRACE(mLogger, "+unrealize ()");

  mUnrealizeDispatcher.emit();

  // wait for animation finished on statemachine thread
  mMutexUnrealize.lock();
  mCondUnrealize.wait(mMutexUnrealize);
  mMutexUnrealize.unlock();

  groupState = Unrealized;

  LOG4CXX_TRACE(mLogger, "-unrealize ()");
}

void EdjeView::realizeDispatched(int missedEvents)
{
  LOG4CXX_TRACE(mLogger, "+realizeDispatched()");

  LOG4CXX_INFO(mLogger, "Filename: '" << mFilename << "', Groupname: " << mGroupname);

  mWindow = mEdjeContext->window;
  mLayout = Elmxx::Layout::factory(*mWindow);
  mLayout->setSizeHintWeight(EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  mWindow->addResizeObject(*mLayout);
  mLayout->show();

  mLayout->setFile(mFilename, mGroupname);

  LOG4CXX_INFO(mLogger, "Layer: " << getLayer());
  mLayout->setLayer(getLayer());

  Eflxx::CountedPtr <Edjexx::Object> edjeObj(mLayout->getEdje());

  // connect visible/invisible handler
  // --> TODO: while changing names connect both -> remove later deprecated names!
  edjeObj->connect("invisible_signal", "edje", sigc::mem_fun(this, &EdjeView::invisibleFunc));
  edjeObj->connect("visible_signal", "edje", sigc::mem_fun(this, &EdjeView::visibleFunc));
  //// <--

  // this is the new name of the spec!
  edjeObj->connect("animation,end", "invisible", sigc::mem_fun(this, &EdjeView::invisibleFunc));
  edjeObj->connect("animation,end", "visible", sigc::mem_fun(this, &EdjeView::visibleFunc));

  edjeObj->connect("*", "edje", sigc::mem_fun(this, &EdjeView::edjeFunc));
  edjeObj->connect("*", "stateval", sigc::mem_fun(this, &EdjeView::statevalFunc));

  edjeObj->connect("*", "*", sigc::mem_fun(this, &EdjeView::allFunc));

  mLayout->resize(mEdjeContext->resolution);

  // initial screen widget update after ralizing a screen
  updateDispatched(0);

  groupState = Realizing;
  edjeObj->emit("visible", "stateval");
  mEdjeContext->background->hide (); // make background "transparent"

  mCondRealize.signal();

  LOG4CXX_TRACE(mLogger, "-realizeDispatched()");
}

void EdjeView::unrealizeDispatched(int missedEvents)
{
  if (mLayout)
  {
    groupState = Unrealizing;
    Eflxx::CountedPtr <Edjexx::Object> edjeObj = mLayout->getEdje();

    // show background while view switch to prevent flickering of 
    // below composite layer
    mEdjeContext->background->show ();
    
    edjeObj->emit("invisible", "stateval");
  }
}

void EdjeView::update()
{  
  LOG4CXX_TRACE(mLogger, "+update()");

  mUpdateDispatcher.emit();

  LOG4CXX_TRACE(mLogger, "-update()");
}

void EdjeView::updateDispatched(int missedEvents)
{
  LOG4CXX_TRACE(mLogger, "updateDispatched()");
  
  for (WidgetIterator wl_it = beginOfWidgets();
       wl_it != endOfWidgets();
       ++wl_it)
  {
    Widget *w = wl_it->second;


    w->updateContent();
  }

  
#if 0
  // FIXME: seems first screen could not do a updateContent ()!!
  if (!mLayout)
    return;

  StateMachineAccessor &stateMachineAccessor = StateMachineAccessor::getInstance();
  Eflxx::CountedPtr <Edjexx::Object> edjeObj(mLayout->getEdje());

  /* FIXME: this logic below is not correct because if a variable is connected to two different widgets it
            may not be updated correct. TODO: check this!
   */
  for (WidgetIterator wl_it = beginOfWidgets();
       wl_it != endOfWidgets();
       ++wl_it)
  {
    const Widget *w = *wl_it;

    Variable *val = stateMachineAccessor.getVariable(w->getVariable());
    assert(val);

    // update widget data only if update is needed
    // always draw a widget for initial screen display
    if (val->needsUpdate () || initalDrawing)
    {
      try
      {
        Edjexx::Part &part = edjeObj->getPart(w->getName());

        if (val->getType() == Variable::TYPE_STRUCT)
        {
          Struct *st = static_cast <Struct *>(val);
          bool specialHandled = false;

          // TODO: is there a special handling for struct types needed?
          try
          {
            Evasxx::Object &ext_eo3 = part.getExternalObject();
            Evasxx::Object &eo3 = part.getSwallow();
            LOG4CXX_DEBUG(mLogger, "Edje External Widget type: " << ext_eo3.getType());
            LOG4CXX_DEBUG(mLogger, "Edje Part Widget type: " << eo3.getType());

            if (ext_eo3.getType() == "elm_widget")
            {
              Elmxx::Object &elm_object = *(static_cast <Elmxx::Object *>(&ext_eo3));

              LOG4CXX_DEBUG(mLogger, "Elm Widget type: " << elm_object.getWidgetType());
              // TODO: slider is now generic supported. But ElmList needs to be implemented...
              /*if (elm_object.getWidgetType () == "slider")
              {
                Elmxx::Slider &slider = *(static_cast <Elmxx::Slider*> (&elm_object));
                Variable *av1 = st->getData ("label");
                if (av1->getType () == Variable::TYPE_STRING)
                {
                  String *s1 = static_cast <String*> (av1);
                  slider.setLabel (s1->getData ());
                }

                Variable *av2 = st->getData ("value");
                if (av2->getType () == Variable::TYPE_FLOAT)
                {
                  Float *f1 = static_cast <Float*> (av2);
                  slider.setValue (f1->getData ());
                }
                specialHandled = true;
              }*/
            }
          }
          catch (Edjexx::ExternalNotExistingException ene)
          {
            cerr << ene.what() << endl;
          }

          // generic widget type handling
          if (!specialHandled)
          {
            for (Struct::Iterator s_it = st->begin();
                 s_it != st->end();
                 ++s_it)
            {
              const string &name = s_it->first;
              Variable *av = s_it->second;

              if (av)
              {
                if (av->getType() == Variable::TYPE_STRING)
                {
                  String *str = static_cast <String *>(av);
                  Edjexx::ExternalParam param(name, str->getData());
                  part.setParam(&param);
                }
                else if (av->getType() == Variable::TYPE_DOUBLE)
                {
                  Double *d = static_cast <Double *>(av);
                  Edjexx::ExternalParam param(name, d->getData());
                  part.setParam(&param);
                }
                else if (av->getType() == Variable::TYPE_BOOL)
                {
                  Bool *b = static_cast <Bool *>(av);
                  Edjexx::ExternalParam param(name, b->getData());
                  part.setParam(&param);
                }
              }
            }
          }
        }
        else if (val->getType() == Variable::TYPE_LIST)
        {
          try
          {
            List *ls = static_cast <List *>(val);

            Evasxx::Object &ext_eo3 = part.getExternalObject();
            Evasxx::Object &eo3 = part.getSwallow();
            LOG4CXX_DEBUG(mLogger, "Edje External Widget type: " << ext_eo3.getType());
            LOG4CXX_DEBUG(mLogger, "Edje Part Widget type: " << eo3.getType());

            if (ext_eo3.getType() == "elm_list")
            {
              Elmxx::Object &elm_object = *(static_cast <Elmxx::Object *>(&ext_eo3));

              LOG4CXX_DEBUG(mLogger, "Elm Widget type: " << elm_object.getWidgetType());

              if (elm_object.getWidgetType() == "Elm_List")
              {
                Elmxx::List &list = *(static_cast <Elmxx::List *>(&elm_object));

                // TODO: I think until the edited/merge feature is implemented it's the
                // best to clear the list before adding new elements...
                list.clear();
                for (List::Iterator ls_it = ls->begin();
                     ls_it != ls->end();
                     ++ls_it)
                {
                  Variable *av = *ls_it;

                  if (av->getType() == Variable::TYPE_STRING)
                  {
                    String *str = static_cast <String *>(av);
                    list.append(str->getData(), NULL, NULL);
                  }
                  list.go();
                }
              }
            }
          }
          catch (Edjexx::ExternalNotExistingException ene)
          {
            cerr << ene.what() << endl;
          }
        }
        else if (val->getType() == Variable::TYPE_STRING)
        {
          String *str = static_cast <String *>(val);

          part.setText(str->getData());
        }
        else
        {
          LOG4CXX_WARN(mLogger, "Currently not supported Variable Type!");
        }
      }
      catch (Edjexx::PartNotExistingException pne)
      {
        cerr << pne.what() << endl;
      }

      // widget is updated, so reset need for update
      val->setUpdateFlag (false);

      LOG4CXX_INFO(mLogger, "Widget name: " << w->getName());
      LOG4CXX_INFO(mLogger, "Widget variable: " << w->getVariable());
    }
    
  }
#endif
}

void EdjeView::invisibleFunc(const std::string emmision, const std::string source)
{
  LOG4CXX_TRACE(mLogger, "invisibleFunc");

  groupState = Unrealized;
  mWindow->delResizeObject(*mLayout);
  mLayout->destroy();
  mLayout = NULL;
  
  // signal the edje statemachine thread that the animation is finished
  mCondUnrealize.signal();
}

void EdjeView::visibleFunc(const std::string emmision, const std::string source)
{
  LOG4CXX_TRACE(mLogger, "visibleFunc");

  groupState = Realized;
}

void EdjeView::statevalFunc(const std::string emmision, const std::string source)
{
  LOG4CXX_TRACE(mLogger, "statevalFunc: " << emmision << ", " << source);
}

void EdjeView::edjeFunc(const std::string emmision, const std::string source)
{
  LOG4CXX_TRACE(mLogger, "edjeFunc: " << emmision << ", " << source);
}

void EdjeView::allFunc(const std::string emmision, const std::string source)
{
  if (source != "stateval")
  {
    StateMachineAccessor &StateMachineAccessor(StateMachineAccessor::getInstance());

    string event("edje," + source + "," + emmision);

    // => activate next line to get all events logged, but be warned as mouse events spam event queue much!
    //LOG4CXX_DEBUG(mLogger, "allFunc: " << event << " (" << mGroupname << ")");

    // only push new events for realized screens
    // FIXME: when I do this it leads into freezes as the invisible signal doesn't come
    //if (groupState != Realized) return;

    if (StateMachineAccessor.findMapingEvent(event) != -1)
    {
      LOG4CXX_DEBUG(mLogger, "mStateMachineAccessor->pushEvent");
      StateMachineAccessor.pushEvent(event);
    }
  }
}

void EdjeView::pushEventDispatched(int missedEvents)
{
  if (groupState == Realized)
  {
    StateMachineAccessor &stateMachineAccessor(StateMachineAccessor::getInstance());

    // TODO: think about defining some common events (update/exit,...) in stateval lib 
    static const int VIEW_UPDATE_EVENT = stateMachineAccessor.findMapingEvent("VIEW_UPDATE");

    string eventString = stateMachineAccessor.findMapingEvent(mEvent);

    LOG4CXX_DEBUG(mLogger, "EdjeView::smEvents: " << mEvent << " / " << eventString);

    if ((eventString.length() >= 4) && (eventString.substr(4) != "edje"))
    {
      Eflxx::CountedPtr <Edjexx::Object> edjeObj = mLayout->getEdje();
      edjeObj->emit(eventString, "stateval");
    }

    if (mEvent == VIEW_UPDATE_EVENT)
    {
      update();
    }
  }

  mCondPushEvent.signal ();
}

void EdjeView::pushEvent(int event)
{
  mEvent = event;
  
  mPushEventDispatcher.emit ();

  mMutexPushEvent.lock();  
  mCondPushEvent.wait(mMutexPushEvent);
  mMutexPushEvent.unlock();
}

Widget *EdjeView::createWidget(const std::string &name)
{
  Widget *widget = new EdjeWidget(*this, name);
  mWidgetMap[name] = widget;
  return widget;
}
