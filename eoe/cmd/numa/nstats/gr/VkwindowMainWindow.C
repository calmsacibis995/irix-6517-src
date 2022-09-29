
//////////////////////////////////////////////////////////////
//
// Source file for VkwindowMainWindow
//
//    This class is a subclass of VkWindow
//
//
// Normally, very little in this file should need to be changed.
// Create/add/modify menus using RapidApp.
//
// Try to restrict any changes to the bodies of functions
// corresponding to menu items, the constructor and destructor.
//
// Restrict changes to those sections between
// the "//--- Start/End editable code block" markers
//
// Doing so will allow you to make changes using RapidApp
// without losing any changes you may have made manually
//
// Avoid gratuitous reformatting and other changes that might
// make it difficult to integrate changes made using RapidApp
//////////////////////////////////////////////////////////////
#include "VkwindowMainWindow.h"

#include <Vk/VkApp.h>
#include <Vk/VkFileSelectionDialog.h>
#include <Vk/VkSubMenu.h>
#include <Vk/VkRadioSubMenu.h>
#include <Vk/VkMenuItem.h>
#include <Vk/VkMenuBar.h>
#include <Vk/VkResource.h>


// Externally defined classes referenced by this class: 

#include "BulletinBoard.h"

extern void VkUnimplemented ( Widget, const char * );

//---- Start editable code block: headers and declarations
BulletinBoard *gBulletinBoard;

//---- End editable code block: headers and declarations



// These are default resources for widgets in objects of this class
// All resources will be prepended by *<name> at instantiation,
// where <name> is the name of the specific instance, as well as the
// name of the baseWidget. These are only defaults, and may be overriden
// in a resource file by providing a more specific resource name

String  VkwindowMainWindow::_defaultVkwindowMainWindowResources[] = {
        "*closeButton.accelerator:  Ctrl<Key>W",
        "*closeButton.acceleratorText:  Ctrl+W",
        "*closeButton.labelString:  Close",
        "*closeButton.mnemonic:  C",
        "*exitButton.accelerator:  Ctrl<Key>Q",
        "*exitButton.acceleratorText:  Ctrl+Q",
        "*exitButton.labelString:  Exit",
        "*exitButton.mnemonic:  x",
        "*filePane.labelString:  File",
        "*filePane.mnemonic:  F",
        "*helpPane.labelString:  Help",
        "*helpPane.mnemonic:  H",
        "*help_click_for_help.accelerator:  Shift<Key>F1",
        "*help_click_for_help.acceleratorText:  Shift+F1",
        "*help_click_for_help.labelString:  Click For Help",
        "*help_click_for_help.mnemonic:  C",
        "*help_index.labelString:  Index",
        "*help_index.mnemonic:  I",
        "*help_keys_and_short.labelString:  Keys and Shortcuts",
        "*help_keys_and_short.mnemonic:  K",
        "*help_overview.labelString:  Overview",
        "*help_overview.mnemonic:  O",
        "*help_prod_info.labelString:  Product Information",
        "*help_prod_info.mnemonic:  P",
        "*optionsPane.labelString:  Options",
        "*optionsPane.mnemonic:  O",
        "*radMD10.labelString:  10",
        "*radMD100.labelString:  100",
        "*radMD1000.labelString:  1000",
        "*radUI10.labelString:  10 ms",
        "*radUI100.labelString:  100 ms",
        "*radUI1000.labelString:  1 s",
        "*radioInterval.labelString:  Update Interval",
        "*radioMaxDiff.labelString:  Max Diff Value",
        "*title:  gr_nstats",
        "*toggle.labelString:  View total activity",
        "*viewPane1.labelString:  View",
        "*viewPane1.mnemonic:  V",

        //---- Start editable code block: VkwindowMainWindow Default Resources


        //---- End editable code block: VkwindowMainWindow Default Resources

        (char*)NULL
};


//---- Class declaration

VkwindowMainWindow::VkwindowMainWindow ( const char *name,
                                        ArgList args,
                                        Cardinal argCount) : 
                                  VkWindow ( name, args, argCount )
{
    // Load any class-default resources for this object

    setDefaultResources ( baseWidget(), _defaultVkwindowMainWindowResources  );


    // Create the view component contained by this window

    _bulletinBoard = new BulletinBoard ( "bulletinBoard",mainWindowWidget() );


    XtVaSetValues ( _bulletinBoard->baseWidget(),
                    XmNwidth, 1009, 
                    XmNheight, 320, 
                    (XtPointer) NULL );

    // Add the component as the main view

    addView ( _bulletinBoard );

    // Create the panes of this window's menubar. The menubar itself
    // is created automatically by ViewKit


    // The filePane menu pane

    _filePane =  addMenuPane ( "filePane" );

    _closeButton =  _filePane->addAction ( "closeButton", 
                                        &VkwindowMainWindow::closeCallback, 
                                        (XtPointer) this );

    _exitButton =  _filePane->addAction ( "exitButton", 
                                        &VkwindowMainWindow::quitCallback, 
                                        (XtPointer) this );

    // The viewPane1 menu pane

    _viewPane1 =  addMenuPane ( "viewPane1" );

    _toggle =  _viewPane1->addToggle ( "toggle", 
                                        &VkwindowMainWindow::setTotalCallback,
                                        (XtPointer) this );

    // The optionsPane menu pane

    _optionsPane =  addMenuPane ( "optionsPane" );

    // The radioInterval menu pane

    _radioInterval =  _optionsPane->addRadioSubmenu ( "radioInterval" );

    _radUI10 =  _radioInterval->addToggle ( "radUI10", 
                                        &VkwindowMainWindow::setInterval10Callback, 
                                        (XtPointer) this );

    _radUI100 =  _radioInterval->addToggle ( "radUI100", 
                                        &VkwindowMainWindow::setInterval100Callback, 
                                        (XtPointer) this );

    _radUI1000 =  _radioInterval->addToggle ( "radUI1000", 
                                        &VkwindowMainWindow::setInterval1000Callback, 
                                        (XtPointer) this );
_radUI1000->setVisualState(TRUE);

    // The radioMaxDiff menu pane

    _radioMaxDiff =  _optionsPane->addRadioSubmenu ( "radioMaxDiff" );

    _radMD10 =  _radioMaxDiff->addToggle ( "radMD10", 
                                        &VkwindowMainWindow::setDiff10Callback, 
                                        (XtPointer) this );
_radMD10->setVisualState(TRUE);

    _radMD100 =  _radioMaxDiff->addToggle ( "radMD100", 
                                        &VkwindowMainWindow::setDiff100Callback, 
                                        (XtPointer) this );

    _radMD1000 =  _radioMaxDiff->addToggle ( "radMD1000", 
                                        &VkwindowMainWindow::setDiff1000Callback, 
                                        (XtPointer) this );


    //---- Start editable code block: VkwindowMainWindow constructor
    gBulletinBoard = _bulletinBoard;

    //---- End editable code block: VkwindowMainWindow constructor


}    // End Constructor


VkwindowMainWindow::~VkwindowMainWindow()
{
    delete _bulletinBoard;
    //---- Start editable code block: VkwindowMainWindow destructor


    //---- End editable code block: VkwindowMainWindow destructor
}    // End destructor


const char *VkwindowMainWindow::className()
{
    return ("VkwindowMainWindow");
}    // End className()


Boolean VkwindowMainWindow::okToQuit()
{
    //---- Start editable code block: VkwindowMainWindow okToQuit



    // This member function is called when the user quits by calling
    // theApplication->terminate() or uses the window manager close protocol
    // This function can abort the operation by returning FALSE, or do some.
    // cleanup before returning TRUE. The actual decision is normally passed on
    // to the view object


    // Query the view object, and give it a chance to cleanup

    return ( _bulletinBoard->okToQuit() );

    //---- End editable code block: VkwindowMainWindow okToQuit
}    // End okToQuit()



/////////////////////////////////////////////////////////////// 
// The following functions are static member functions used to 
// interface with Motif.
/////////////////////////////////// 

void VkwindowMainWindow::closeCallback ( Widget    w,
                                         XtPointer clientData,
                                         XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->close ( w, callData );
}

void VkwindowMainWindow::quitCallback ( Widget    w,
                                        XtPointer clientData,
                                        XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->quit ( w, callData );
}

void VkwindowMainWindow::setDiff10Callback ( Widget    w,
                                             XtPointer clientData,
                                             XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->setDiff10 ( w, callData );
}

void VkwindowMainWindow::setDiff100Callback ( Widget    w,
                                              XtPointer clientData,
                                              XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->setDiff100 ( w, callData );
}

void VkwindowMainWindow::setDiff1000Callback ( Widget    w,
                                               XtPointer clientData,
                                               XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->setDiff1000 ( w, callData );
}

void VkwindowMainWindow::setInterval10Callback ( Widget    w,
                                                 XtPointer clientData,
                                                 XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->setInterval10 ( w, callData );
}

void VkwindowMainWindow::setInterval100Callback ( Widget    w,
                                                  XtPointer clientData,
                                                  XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->setInterval100 ( w, callData );
}

void VkwindowMainWindow::setInterval1000Callback ( Widget    w,
                                                   XtPointer clientData,
                                                   XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->setInterval1000 ( w, callData );
}

void VkwindowMainWindow::setTotalCallback ( Widget    w,
                                                   XtPointer clientData,
                                                   XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->setTotal ( w, callData );
}

/////////////////////////////////////////////////////////////// 
// The following functions are called from callbacks 
/////////////////////////////////// 

void VkwindowMainWindow::close ( Widget, XtPointer ) 
{
    //---- Start editable code block:  close


    // To close a window, just delete the object
    // checking first with the view object.
    // If this is the last window, ViewKit will exit

    if(okToQuit())
        delete this;

    //---- End editable code block:  close
}    // End VkwindowMainWindow::close()

void VkwindowMainWindow::quit ( Widget, XtPointer ) 
{
    // Exit via quitYourself() to allow the application
    // to go through its normal shutdown routines, checking with
    // all windows, views, and so on.

    theApplication->quitYourself();

}    // End VkwindowMainWindow::quit()

void VkwindowMainWindow::setDiff10 ( Widget w, XtPointer callData ) 
{
    //---- Start editable code block: VkwindowMainWindow setDiff10

    XmToggleButtonCallbackStruct *cbs = (XmToggleButtonCallbackStruct*) callData;

    // By default this member function passes control
    // to the component contained by this window

    _bulletinBoard->setDiff10( w, callData);


    //---- End editable code block: VkwindowMainWindow setDiff10

}    // End VkwindowMainWindow::setDiff10()


void VkwindowMainWindow::setDiff100 ( Widget w, XtPointer callData ) 
{
    //---- Start editable code block: VkwindowMainWindow setDiff100

    XmToggleButtonCallbackStruct *cbs = (XmToggleButtonCallbackStruct*) callData;

    // By default this member function passes control
    // to the component contained by this window

    _bulletinBoard->setDiff100( w, callData);


    //---- End editable code block: VkwindowMainWindow setDiff100

}    // End VkwindowMainWindow::setDiff100()


void VkwindowMainWindow::setDiff1000 ( Widget w, XtPointer callData ) 
{
    //---- Start editable code block: VkwindowMainWindow setDiff1000

    XmToggleButtonCallbackStruct *cbs = (XmToggleButtonCallbackStruct*) callData;

    // By default this member function passes control
    // to the component contained by this window

    _bulletinBoard->setDiff1000( w, callData);


    //---- End editable code block: VkwindowMainWindow setDiff1000

}    // End VkwindowMainWindow::setDiff1000()


void VkwindowMainWindow::setInterval10 ( Widget w, XtPointer callData ) 
{
    //---- Start editable code block: VkwindowMainWindow setInterval10

    XmToggleButtonCallbackStruct *cbs = (XmToggleButtonCallbackStruct*) callData;

    // By default this member function passes control
    // to the component contained by this window

    _bulletinBoard->setInterval10( w, callData);


    //---- End editable code block: VkwindowMainWindow setInterval10

}    // End VkwindowMainWindow::setInterval10()


void VkwindowMainWindow::setInterval100 ( Widget w, XtPointer callData ) 
{
    //---- Start editable code block: VkwindowMainWindow setInterval100

    XmToggleButtonCallbackStruct *cbs = (XmToggleButtonCallbackStruct*) callData;

    // By default this member function passes control
    // to the component contained by this window

    _bulletinBoard->setInterval100( w, callData);


    //---- End editable code block: VkwindowMainWindow setInterval100

}    // End VkwindowMainWindow::setInterval100()


void VkwindowMainWindow::setInterval1000 ( Widget w, XtPointer callData ) 
{
    //---- Start editable code block: VkwindowMainWindow setInterval1000

    XmToggleButtonCallbackStruct *cbs = (XmToggleButtonCallbackStruct*) callData;

    // By default this member function passes control
    // to the component contained by this window

    _bulletinBoard->setInterval1000( w, callData);


    //---- End editable code block: VkwindowMainWindow setInterval1000

}    // End VkwindowMainWindow::setInterval1000()


void VkwindowMainWindow::setTotal ( Widget w, XtPointer callData ) 
{
    //---- Start editable code block: VkwindowMainWindow setTotal

    XmToggleButtonCallbackStruct *cbs = (XmToggleButtonCallbackStruct*) callData;

    // By default this member function passes control
    // to the component contained by this window

    _bulletinBoard->setTotal( w, callData);


    //---- End editable code block: VkwindowMainWindow setTotal

}    // End VkwindowMainWindow::setTotal()





//---- Start editable code block: End of generated code


//---- End editable code block: End of generated code


