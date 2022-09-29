
/////////////////////////////////////////////////////////////
//
// Source file for RmBDUI
//
//    This class implements the user interface created in 
//    RapidApp.
//
//    Restrict changes to those sections between
//    the "//--- Start/End editable code block" markers
//
//    This will allow RapidApp to integrate changes more easily
//
//    This class is a ViewKit user interface "component".
//    For more information on how components are used, see the
//    "ViewKit Programmers' Manual", and the RapidApp
//    User's Guide.
//
//
/////////////////////////////////////////////////////////////


#include "RmBDUI.h" // Generated header file for this class

#include <Xm/BulletinB.h> 
#include <Xm/Label.h> 
#include <Vk/VkResource.h>
//---- Start editable code block: headers and declarations


//---- End editable code block: headers and declarations


// These are default resources for widgets in objects of this class
// All resources will be prepended by *<name> at instantiation,
// where <name> is the name of the specific instance, as well as the
// name of the baseWidget. These are only defaults, and may be overriden
// in a resource file by providing a more specific resource name

String  RmBDUI::_defaultRmBDUIResources[] = {
        "*rmlabel.labelString:  Are you sure to remove the selected statefile?",

        //---- Start editable code block: RmBDUI Default Resources

        //---- End editable code block: RmBDUI Default Resources

        (char*)NULL
};

RmBDUI::RmBDUI ( const char *name ) : VkComponent ( name ) 
{ 
    // No widgets are created by this constructor.
    // If an application creates a component using this constructor,
    // It must explictly call create at a later time.
    // This is mostly useful when adding pre-widget creation
    // code to a derived class constructor.

    //---- Start editable code block: RmBD constructor 2

    //---- End editable code block: RmBD constructor 2
}    // End Constructor


RmBDUI::RmBDUI ( const char *name, Widget parent ) : VkComponent ( name ) 
{ 
    //---- Start editable code block: RmBD pre-create


    //---- End editable code block: RmBD pre-create

    // Call creation function to build the widget tree.

     create ( parent );

    //---- Start editable code block: RmBD constructor

    //---- End editable code block: RmBD constructor
}    // End Constructor

RmBDUI::~RmBDUI() 
{
    // Base class destroys widgets

    //---- Start editable code block: RmBDUI destructor

    //---- End editable code block: RmBDUI destructor
}    // End destructor


void RmBDUI::create ( Widget parent )
{
    Arg      args[6];
    Cardinal count;
    count = 0;

    // Load any class-defaulted resources for this object

    setDefaultResources ( parent, _defaultRmBDUIResources  );


    // Create an unmanaged widget as the top of the widget hierarchy

    _baseWidget = _rmBD = XtVaCreateWidget ( _name,
                                             xmBulletinBoardWidgetClass,
                                             parent, 
                                             XmNresizePolicy, XmRESIZE_GROW, 
                                             (XtPointer) NULL ); 

    // install a callback to guard against unexpected widget destruction

    installDestroyHandler();


    // Create widgets used in this component
    // All variables are data members of this class

    _rmlabel = XtVaCreateManagedWidget  ( "rmlabel",
                                           xmLabelWidgetClass,
                                           _baseWidget, 
                                           XmNlabelType, XmSTRING, 
                                           XmNx, 16, 
                                           XmNy, 14, 
                                           XmNwidth, 321, 
                                           XmNheight, 20, 
                                           (XtPointer) NULL ); 

    //---- Start editable code block: RmBDUI create

    //---- End editable code block: RmBDUI create
}

const char * RmBDUI::className()
{
    return ("RmBDUI");
}    // End className()

/////////////////////////////////////////////////////////////// 
// The following functions are called from the menu items 
// in this window.
/////////////////////////////////// 

//---- Start editable code block: End of generated code

//---- End editable code block: End of generated code
