#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* Eflxx */
#include <elementaryxx/Elementaryxx.h>

/* local */
#include "EdjeWidget.h"
#include "stringUtil.h"

using namespace std;

EdjeWidget::EdjeWidget(View &view, const std::string &name) :
  mLogger("stateval.plugins.viewmanager.edje.EdjeWidget"),
  Widget(name),
  mView(dynamic_cast<EdjeView*>(&view))
{
  mUpdateDataDispatcher.signalDispatch.connect(sigc::mem_fun(this, &EdjeWidget::updateDataDispatched));
}

Variable *EdjeWidget::getProperty(const std::string &name)
{
  LOG4CXX_TRACE(mLogger, "+getValue()");

  mUpdateDataDispatcher.emit();

  mMutexUpdateData.lock();
  mCondUpdateData.wait(mMutexUpdateData);
  mMutexUpdateData.unlock();
  
  LOG4CXX_TRACE(mLogger, "-getValue()");

  Variable *var = mProperties[name];
  
  return var;
}

void EdjeWidget::updateData()
{

}

void EdjeWidget::updateDataDispatched(int missedEvents)
{
  LOG4CXX_TRACE(mLogger, "updateData: " << mName);

  // TODO: this will crash if calling a not visible view => fix later 
  Eflxx::CountedPtr <Edjexx::Object> edjeObj(mView->mLayout->getEdje());

  try
  {
    Edjexx::Part &part = edjeObj->getPart(mName);
    
    // TODO: data type introspection by definion in XML?
    Edjexx::ExternalParam paramHours ("hours", (int) 0); 
    Edjexx::ExternalParam paramMinues ("minutes", (int) 0);
    Edjexx::ExternalParam paramSeconds ("seconds", (int) 0);
    bool validParamHours = part.getParam (paramHours);
    bool validParamMinutes = part.getParam (paramMinues);
    bool validParamSeconds = part.getParam (paramSeconds);


    Edje_External_Param_Type paramType = part.getParamType("hours");
    switch(paramType)
    {
      case EDJE_EXTERNAL_PARAM_TYPE_INT:
        LOG4CXX_DEBUG(mLogger, "INT");
        break;
      case EDJE_EXTERNAL_PARAM_TYPE_DOUBLE:
        LOG4CXX_DEBUG(mLogger, "DOUBLE");
        break;
      case EDJE_EXTERNAL_PARAM_TYPE_STRING:
        LOG4CXX_DEBUG(mLogger, "STRING");
        break;
      case EDJE_EXTERNAL_PARAM_TYPE_BOOL:
        LOG4CXX_DEBUG(mLogger, "BOOL");
        break;
      case EDJE_EXTERNAL_PARAM_TYPE_CHOICE:
        LOG4CXX_DEBUG(mLogger, "CHOICE");
        break;
      case EDJE_EXTERNAL_PARAM_TYPE_MAX:
        LOG4CXX_DEBUG(mLogger, "MAX");
        break;  
      default:
        LOG4CXX_DEBUG(mLogger, "unknown type");
    }

    
    
    string str = toString(paramHours.mParam.i) + string(":") + toString(paramMinues.mParam.i) + string("h");
    LOG4CXX_TRACE(mLogger, "Clock: " << str);
#if 0
    if(mValue)
    {
      delete mValue; 
    }

    mValue = new String(str);
#endif
  }
  catch (Edjexx::ExternalNotExistingException ene)
  {
    LOG4CXX_ERROR(mLogger, ene.what());
  }

  mCondUpdateData.signal();
}


void EdjeWidget::updateContent()
{
#if 0
  LOG4CXX_TRACE(mLogger, "updateContent: " << mName);

  Eflxx::CountedPtr <Edjexx::Object> edjeObj(mView->mLayout->getEdje());

  try
  {
    Edjexx::Part &part = edjeObj->getPart(mName);
  
    if(mValue)
    {
      if(mValue->getType() == Variable::TYPE_STRING)
      {
        String *str = static_cast <String *>(mValue);

        part.setText(str->getData());
      }
      else if (mValue->getType() == Variable::TYPE_LIST)
      {
        List *ls = static_cast <List *>(mValue);

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
      else
      {
        LOG4CXX_WARN(mLogger, "Currently not supported Variable Type!");
      }
    }
  }
  catch (Edjexx::ExternalNotExistingException ene)
  {
    LOG4CXX_ERROR(mLogger, ene.what());
  }
#endif
}
