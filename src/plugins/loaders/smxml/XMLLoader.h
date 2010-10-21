/* STD */
#include <iostream>
#include <string>

/* XML */

#include <libxml++/libxml++.h>

/* stateval */
#include "stateval/stateval.h"

class XMLLoader : public Loader
{
public:  
  XMLLoader ();
  ~XMLLoader ();

  const std::string getType ();
  
  const unsigned int getMajorVersion ();

  const unsigned int getMinorVersion ();
  
  bool load (Context *context, const std::string &sm);
  
  void unload ();
    
protected:  
  void parseRootNode (const xmlpp::Node* node);
  void parseBlockNode (const xmlpp::Node* node);
  
  void parseEventsNode (const xmlpp::Node* node);
  void parseEventNode (const xmlpp::Node* node);

  void parseStatesNode (const xmlpp::Node* node);
  void parseStateNodeIndex (const xmlpp::Node* node, unsigned int i);
  void parseStateNode (const xmlpp::Node* node, unsigned int i);

  void parseTransitionsNode (const xmlpp::Node* node);
  void parseTransitionNode (const xmlpp::Node* node);

  void parseViewsNode (const xmlpp::Node* node);
  void parseViewNode (const xmlpp::Node* node, const Glib::ustring &plugin, unsigned int i);
  void parseViewMapNode (const xmlpp::Node* node, View *view);
  
private:
  std::map <Glib::ustring, unsigned int> mStateNameMapper;
  std::map <Glib::ustring, unsigned int> mViewNameMapper;
};
