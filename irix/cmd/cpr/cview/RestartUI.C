/////////////////////////////////////////////////////////////
//
// Source file for RestartUI
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


#include <stdio.h>
#include "RestartUI.h" // Generated header file for this class
#include "CviewMainWindow.h" // Generated header file for this class
#include <Vk/VkApp.h>
#include <Vk/VkFileSelectionDialog.h>
#include <Vk/VkSubMenu.h>
#include <Vk/VkRadioSubMenu.h>
#include <Vk/VkMenuItem.h>
#include <Vk/VkMenuBar.h>
#include <Vk/VkResource.h>


#include "RmDialog.h"
#include <unistd.h>
#include <Sgm/Finder.h> 
#include <Sgm/RubberBoard.h> 
#include <Xm/Label.h> 
#include <Xm/List.h> 
#include <Xm/PushB.h> 
#include <Xm/ScrolledW.h> 
#include <Xm/Separator.h> 
#include <Xm/ToggleB.h> 
#include <Vk/VkResource.h>
#include <Vk/VkOptionMenu.h>
#include <Vk/VkMenuItem.h>
//---- Start editable code block: headers and declarations

int RestFlags = 0;

//---- End editable code block: headers and declarations


// These are default resources for widgets in objects of this class
// All resources will be prepended by *<name> at instantiation,
// where <name> is the name of the specific instance, as well as the
// name of the baseWidget. These are only defaults, and may be overriden
// in a resource file by providing a more specific resource name

String  RestartUI::_defaultRestartUIResources[] = {
        "*anypid.labelString:  Any PID",
        "*but_ls.labelString:  Display and Select Restart Statefiles and Options",
        "*but_restart.labelString:  OK, Go Restart!",
        "*forkMenu.labelString:  Restart Jobs using",
        "*info.labelString:  Or, Tell Me More About This Statefile",
        "*label.labelString:  RESTART CONTROL PANEL",
        "*origpid.labelString:  Original Process ID (PID)",
        "*remove.labelString:  Remove This Statefile",
        "*tabLabel:  Restart Control Panel",
        "*tog_cdir.labelString:  Don't restore the original cwd (current working directory)",
        "*tog_rdir.labelString:  Don't restore the original the root directory",

        //---- Start editable code block: RestartUI Default Resources


        //---- End editable code block: RestartUI Default Resources

        (char*)NULL
};

RestartUI::RestartUI ( const char *name ) : VkComponent ( name ) 
{ 
    // No widgets are created by this constructor.
    // If an application creates a component using this constructor,
    // It must explictly call create at a later time.
    // This is mostly useful when adding pre-widget creation
    // code to a derived class constructor.

    //---- Start editable code block: Restart constructor 2

    //---- End editable code block: Restart constructor 2
}    // End Constructor

RestartUI::RestartUI ( const char *name, Widget parent ) : VkComponent ( name ) 
{ 
    //---- Start editable code block: Restart pre-create

    //---- End editable code block: Restart pre-create

    // Call creation function to build the widget tree.

     create ( parent );

    //---- Start editable code block: Restart constructor

    //---- End editable code block: Restart constructor
}    // End Constructor


RestartUI::~RestartUI() 
{
    // Base class destroys widgets

    //---- Start editable code block: RestartUI destructor


    //---- End editable code block: RestartUI destructor
}    // End destructor



void RestartUI::create ( Widget parent )
{
    Arg      args[12];
    Cardinal count;
    count = 0;

    // Load any class-defaulted resources for this object
    setDefaultResources ( parent, _defaultRestartUIResources  );

    // Create an unmanaged widget as the top of the widget hierarchy

    _baseWidget = _restart = XtVaCreateWidget ( _name,
                                                sgRubberBoardWidgetClass,
                                                parent, 
                                                XmNinitialParentHeight, 659, 
                                                XmNfinalParentHeight, 904, 
                                                XmNfinalParentWidth, 925, 
                                                XmNinitialParentWidth, 571, 
                                                (XtPointer) NULL ); 

    // install a callback to guard against unexpected widget destruction

    installDestroyHandler();


    // Create widgets used in this component
    // All variables are data members of this class

    _label = XtVaCreateManagedWidget  ( "label",
                                         xmLabelWidgetClass,
                                         _baseWidget, 
                                         XmNlabelType, XmSTRING, 
                                         XmNinitialX, 167, 
                                         XmNinitialY, 0, 
                                         XmNinitialWidth, 188, 
                                         XmNinitialHeight, 29, 
                                         XmNfinalX, 170, 
                                         XmNfinalY, 0, 
                                         XmNfinalWidth, 633, 
                                         XmNfinalHeight, 29, 
                                         (XtPointer) NULL ); 


    _separator1 = XtVaCreateManagedWidget  ( "separator1",
                                              xmSeparatorWidgetClass,
                                              _baseWidget, 
                                              XmNinitialX, 10, 
                                              XmNinitialY, 20, 
                                              XmNinitialWidth, 520, 
                                              XmNinitialHeight, 20, 
                                              XmNfinalX, 10, 
                                              XmNfinalY, 20, 
                                              XmNfinalWidth, 900, 
                                              XmNfinalHeight, 20, 
                                              (XtPointer) NULL ); 


    _but_ls = XtVaCreateManagedWidget  ( "but_ls",
                                          xmPushButtonWidgetClass,
                                          _baseWidget, 
                                          XmNlabelType, XmSTRING, 
                                          XmNinitialX, 10, 
                                          XmNinitialY, 50, 
                                          XmNinitialWidth, 330, 
                                          XmNinitialHeight, 30, 
                                          XmNfinalX, 10, 
                                          XmNfinalY, 50, 
                                          XmNfinalWidth, 330, 
                                          XmNfinalHeight, 30, 
                                          (XtPointer) NULL ); 

    XtAddCallback ( _but_ls,
                    XmNactivateCallback,
                    &RestartUI::cv_lsCallback,
                    (XtPointer) this ); 


    _scrolledWindow = XtVaCreateManagedWidget  ( "scrolledWindow",
                                                   xmScrolledWindowWidgetClass,
                                                   _baseWidget, 
                                                   XmNscrollBarDisplayPolicy, XmSTATIC, 
                                                   XmNinitialX, 40, 
                                                   XmNinitialY, 142, 
                                                   XmNinitialWidth, 523, 
                                                   XmNinitialHeight, 245, 
                                                   XmNfinalX, 40, 
                                                   XmNfinalY, 154, 
                                                   XmNfinalWidth, 852, 
                                                   XmNfinalHeight, 409, 
                                                   (XtPointer) NULL ); 


    _lsList = XtVaCreateManagedWidget  ( "lsList",
                                                xmListWidgetClass,
                                                _scrolledWindow, 
					  XmNlistSizePolicy, XmCONSTANT,
                                          XmNwidth, 664, 
                                          XmNheight, 353, 
                                                (XtPointer) NULL ); 

    XtAddCallback ( _lsList,
                    XmNbrowseSelectionCallback,
                    &RestartUI::ls_select_oneCallback,
                    (XtPointer) this ); 


    _finder_statef = XtVaCreateManagedWidget  ( "finder_statef",
                                                 sgFinderWidgetClass,
                                                 _baseWidget, 
                                                 XmNinitialX, 40, 
                                                 XmNinitialY, 90, 
                                                 XmNinitialWidth, 523, 
                                                 XmNinitialHeight, 52, 
                                                 XmNfinalX, 40, 
                                                 XmNfinalY, 90, 
                                                 XmNfinalWidth, 850, 
                                                 XmNfinalHeight, 52, 
                                                 (XtPointer) NULL ); 

    XtAddCallback ( _finder_statef,
                    XmNactivateCallback,
                    &RestartUI::cv_statefCallback,
                    (XtPointer) this ); 

    XtAddCallback ( _finder_statef,
                    XmNvalueChangedCallback,
                    &RestartUI::cv_statef_changeCallback,
                    (XtPointer) this ); 


    _tog_cdir = XtVaCreateManagedWidget  ( "tog_cdir",
                                            xmToggleButtonWidgetClass,
                                            _baseWidget, 
                                            XmNalignment, XmALIGNMENT_BEGINNING, 
                                            XmNlabelType, XmSTRING, 
                                            XmNinitialX, 40, 
                                            XmNinitialY, 463, 
                                            XmNinitialWidth, 420, 
                                            XmNinitialHeight, 28, 
                                            XmNfinalX, 40, 
                                            XmNfinalY, 662, 
                                            XmNfinalWidth, 420, 
                                            XmNfinalHeight, 28, 
                                            (XtPointer) NULL ); 

    XtAddCallback ( _tog_cdir,
                    XmNvalueChangedCallback,
                    &RestartUI::setTog_cdirCallback,
                    (XtPointer) this ); 


    _tog_rdir = XtVaCreateManagedWidget  ( "tog_rdir",
                                            xmToggleButtonWidgetClass,
                                            _baseWidget, 
                                            XmNalignment, XmALIGNMENT_BEGINNING, 
                                            XmNlabelType, XmSTRING, 
                                            XmNinitialX, 40, 
                                            XmNinitialY, 496, 
                                            XmNinitialWidth, 320, 
                                            XmNinitialHeight, 28, 
                                            XmNfinalX, 40, 
                                            XmNfinalY, 702, 
                                            XmNfinalWidth, 320, 
                                            XmNfinalHeight, 28, 
                                              (XtPointer) NULL ); 

    XtAddCallback ( _tog_rdir,
                    XmNvalueChangedCallback,
                    &RestartUI::setTog_rdirCallback,
                    (XtPointer) this ); 


    _but_restart = XtVaCreateManagedWidget  ( "but_restart",
                                          xmPushButtonWidgetClass,
                                          _baseWidget, 
                                               XmNlabelType, XmSTRING, 
                                               XmNinitialX, 12, 
                                               XmNinitialY, 538, 
                                               XmNinitialWidth, 176, 
                                               XmNinitialHeight, 30, 
                                               XmNfinalX, 16, 
                                               XmNfinalY, 756, 
                                               XmNfinalWidth, 176, 
                                               XmNfinalHeight, 30, 
                                          (XtPointer) NULL ); 

    XtAddCallback ( _but_restart,
                    XmNactivateCallback,
                    &RestartUI::doBut_restartCallback,
                    (XtPointer) this ); 


    _forkMenu = new VkOptionMenu ( _baseWidget, "forkMenu");
    _origpid =  _forkMenu->addAction ( "origpid", 
                                       &RestartUI::doOrigpidCallback, 
                                       (XtPointer) this );

    _anypid =  _forkMenu->addAction ( "anypid", 
                                      &RestartUI::doAnypidCallback, 
                                      (XtPointer) this );

    _separator2 = XtVaCreateManagedWidget  ( "separator2",
                                              xmSeparatorWidgetClass,
                                              _baseWidget, 
                                              XmNinitialX, 13, 
                                              XmNinitialY, 555, 
                                              XmNinitialWidth, 520, 
                                              XmNinitialHeight, 20, 
                                              XmNfinalX, 13, 
                                              XmNfinalY, 800, 
                                              XmNfinalWidth, 900, 
                                              XmNfinalHeight, 20, 
                                              (XtPointer) NULL ); 

    _info = XtVaCreateManagedWidget  ( "info",
                                        xmPushButtonWidgetClass,
                                        _baseWidget, 
                                        XmNlabelType, XmSTRING, 
                                        XmNinitialX, 12, 
                                        XmNinitialY, 598, 
                                        XmNinitialWidth, 252, 
                                        XmNinitialHeight, 30, 
                                        XmNfinalX, 16, 
                                        XmNfinalY, 820, 
                                        XmNfinalWidth, 252, 
                                        XmNfinalHeight, 30, 
                                        (XtPointer) NULL ); 

    XtAddCallback ( _info,
                    XmNactivateCallback,
                    &RestartUI::doInfoCallback,
                    (XtPointer) this ); 


    _remove = XtVaCreateManagedWidget  ( "remove",
                                          xmPushButtonWidgetClass,
                                          _baseWidget, 
                                          XmNlabelType, XmSTRING, 
                                          XmNinitialX, 330, 
                                          XmNinitialY, 598, 
                                          XmNinitialWidth, 176, 
                                          XmNinitialHeight, 30, 
                                          XmNfinalX, 338, 
                                          XmNfinalY, 820, 
                                          XmNfinalWidth, 176, 
                                          XmNfinalHeight, 30, 
                                          (XtPointer) NULL ); 

    XtAddCallback ( _remove,
                    XmNactivateCallback,
                    &RestartUI::doRemoveCallback,
                    (XtPointer) this ); 


    XtVaSetValues ( _forkMenu->baseWidget(),
                    XmNinitialX, 40, 
                    XmNinitialY, 422, 
                    XmNinitialWidth, 391, 
                    XmNinitialHeight, 32, 
                    XmNfinalX, 36, 
                    XmNfinalY, 618, 
                    XmNfinalWidth, 365, 
                    XmNfinalHeight, 32, 
                    (XtPointer) NULL );

    //---- Start editable code block: RestartUI create

    //---- End editable code block: RestartUI create
}

const char * RestartUI::className()
{
    return ("RestartUI");
}    // End className()

/////////////////////////////////////////////////////////////// 
// The following functions are static member functions used to 
// interface with Motif.
/////////////////////////////////// 

void RestartUI::cv_lsCallback ( Widget    w,
                                XtPointer clientData,
                                XtPointer callData ) 
{ 
    RestartUI* obj = ( RestartUI * ) clientData;
    obj->cv_ls ( obj->_lsList, callData );
}

void RestartUI::cv_statefCallback ( Widget    w,
                                    XtPointer clientData,
                                    XtPointer callData ) 
{ 
    RestartUI* obj = ( RestartUI * ) clientData;
    obj->cv_statef ( w, callData );
    obj->cv_ls ( obj->_lsList, callData );
}

void RestartUI::cv_statef_changeCallback ( Widget    w,
                                           XtPointer clientData,
                                           XtPointer callData ) 
{ 
    RestartUI* obj = ( RestartUI * ) clientData;
    obj->cv_statef_change ( w, callData );
}

void RestartUI::doAnypidCallback ( Widget    w,
                                   XtPointer clientData,
                                   XtPointer callData ) 
{ 
    RestartUI* obj = ( RestartUI * ) clientData;
    obj->doAnypid ( w, callData );
}

void RestartUI::doBut_restartCallback ( Widget    w,
                                        XtPointer clientData,
                                        XtPointer callData ) 
{ 
    RestartUI* obj = ( RestartUI * ) clientData;
    obj->doBut_restart ( w, callData );
}

void RestartUI::doInfoCallback ( Widget    w,
                                 XtPointer clientData,
                                 XtPointer callData ) 
{ 
    RestartUI* obj = ( RestartUI * ) clientData;
    obj->doInfo ( w, callData );
}

void RestartUI::doOrigpidCallback ( Widget    w,
                                      XtPointer clientData,
                                      XtPointer callData ) 
{ 
    RestartUI* obj = ( RestartUI * ) clientData;
    obj->doOrigpid ( w, callData );
}

void RestartUI::doRemoveCallback ( Widget    w,
                                   XtPointer clientData,
                                   XtPointer callData ) 
{ 
    RestartUI* obj = ( RestartUI * ) clientData;
    
    RmDialog(obj->_baseWidget, "remove");

    if (!(RestFlags & CKPT_CONFIRM_REMOVE))
        return;

    obj->doRemove ( w, callData );
    RestFlags &= ~CKPT_CONFIRM_REMOVE;
    sync(); sync();
    obj->cv_ls ( obj->_lsList, callData );
}

void RestartUI::ls_select_oneCallback ( Widget    w,
                                        XtPointer clientData,
                                        XtPointer callData ) 
{ 
    RestartUI* obj = ( RestartUI * ) clientData;
    obj->ls_select_one ( w, callData );
}

void RestartUI::setTog_cdirCallback ( Widget    w,
                                      XtPointer clientData,
                                      XtPointer callData ) 
{ 
    RestartUI* obj = ( RestartUI * ) clientData;
    obj->setTog_cdir ( w, callData );
}

void RestartUI::setTog_rdirCallback ( Widget    w,
                                      XtPointer clientData,
                                      XtPointer callData ) 
{ 
    RestartUI* obj = ( RestartUI * ) clientData;
    obj->setTog_rdir ( w, callData );
}



/////////////////////////////////////////////////////////////// 
// The following functions are called from the menu items 
// in this window.
/////////////////////////////////// 

void RestartUI::cv_ls ( Widget, XtPointer ) 
{
    // This virtual function is called from cv_lsCallback.
    // This function is normally overriden by a derived class.

}

void RestartUI::cv_statef ( Widget, XtPointer ) 
{
    // This virtual function is called from cv_statefCallback.
    // This function is normally overriden by a derived class.

}

void RestartUI::cv_statef_change ( Widget, XtPointer ) 
{
    // This virtual function is called from cv_statef_changeCallback.
    // This function is normally overriden by a derived class.

}

void RestartUI::doAnypid ( Widget, XtPointer ) 
{
    // This virtual function is called from doAnypidCallback.
    // This function is normally overriden by a derived class.

}

void RestartUI::doBut_restart ( Widget, XtPointer ) 
{
    // This virtual function is called from doBut_restartCallback.
    // This function is normally overriden by a derived class.

}

void RestartUI::doInfo ( Widget, XtPointer ) 
{
    // This virtual function is called from doInfoCallback.
    // This function is normally overriden by a derived class.

}

void RestartUI::doOrigpid ( Widget, XtPointer ) 
{
    // This virtual function is called from doOrigpidCallback.
    // This function is normally overriden by a derived class.

}

void RestartUI::doRemove ( Widget, XtPointer ) 
{
    // This virtual function is called from doRemoveCallback.
    // This function is normally overriden by a derived class.

}

void RestartUI::ls_select_one ( Widget, XtPointer ) 
{
    // This virtual function is called from ls_select_oneCallback.
    // This function is normally overriden by a derived class.

}

void RestartUI::setTog_cdir ( Widget, XtPointer ) 
{
    // This virtual function is called from setTog_cdirCallback.
    // This function is normally overriden by a derived class.

}

void RestartUI::setTog_rdir ( Widget, XtPointer ) 
{
    // This virtual function is called from setTog_rdirCallback.
    // This function is normally overriden by a derived class.

}



//---- Start editable code block: End of generated code


//---- End editable code block: End of generated code
