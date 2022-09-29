
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
        "*editPane1.labelString:  Edit",
        "*editPane1.mnemonic:  E",
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
        "*selectAll1.accelerator:  Ctrl<Key>A",
        "*selectAll1.acceleratorText:  Ctrl+A",
        "*selectAll1.labelString:  Select All",
        "*selectAll1.mnemonic:  A",
        "*title:  gr_sn",
        "*unselectAll.accelerator:  Ctrl<Key>F",
        "*unselectAll.acceleratorText:  Ctrl+F",
        "*unselectAll.labelString:  Select First",
        "*unselectAll.mnemonic:  F",

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
                    XmNwidth, 1237, 
                    XmNheight, 259, 
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

    // The editPane1 menu pane

    _editPane1 =  addMenuPane ( "editPane1" );

    _selectAll1 =  _editPane1->addAction ( "selectAll1", 
                                        &VkwindowMainWindow::selectAllCallback, 
                                        (XtPointer) this );

    _unselectAll =  _editPane1->addAction ( "unselectAll", 
                                        &VkwindowMainWindow::selectFirstCallback, 
                                        (XtPointer) this );


    //---- Start editable code block: VkwindowMainWindow constructor


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

void VkwindowMainWindow::selectAllCallback ( Widget    w,
                                             XtPointer clientData,
                                             XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->selectAll ( w, callData );
}

void VkwindowMainWindow::selectFirstCallback ( Widget    w,
                                               XtPointer clientData,
                                               XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->selectFirst ( w, callData );
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

void VkwindowMainWindow::selectAll ( Widget w, XtPointer callData ) 
{
    //---- Start editable code block: VkwindowMainWindow selectAll

    XmPushButtonCallbackStruct *cbs = (XmPushButtonCallbackStruct*) callData;

    // By default this member function passes control
    // to the component contained by this window

    _bulletinBoard->selectAll( w, callData);


    //---- End editable code block: VkwindowMainWindow selectAll

}    // End VkwindowMainWindow::selectAll()


void VkwindowMainWindow::selectFirst ( Widget w, XtPointer callData ) 
{
    //---- Start editable code block: VkwindowMainWindow selectFirst

    XmPushButtonCallbackStruct *cbs = (XmPushButtonCallbackStruct*) callData;

    // By default this member function passes control
    // to the component contained by this window

    _bulletinBoard->selectFirst( w, callData);


    //---- End editable code block: VkwindowMainWindow selectFirst

}    // End VkwindowMainWindow::selectFirst()





//---- Start editable code block: End of generated code


//---- End editable code block: End of generated code


