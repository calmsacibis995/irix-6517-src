
//////////////////////////////////////////////////////////////
//
// Source file for CviewMainWindow
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
#include "CviewMainWindow.h"

#include <Vk/VkApp.h>
#include <Vk/VkFileSelectionDialog.h>
#include <Vk/VkSubMenu.h>
#include <Vk/VkRadioSubMenu.h>
#include <Vk/VkMenuItem.h>
#include <Vk/VkMenuBar.h>
#include <Vk/VkResource.h>


// Externally defined classes referenced by this class: 

#include "DeckTabbedDeck.h"

extern void VkUnimplemented ( Widget, const char * );

//---- Start editable code block: headers and declarations


//---- End editable code block: headers and declarations



// These are default resources for widgets in objects of this class
// All resources will be prepended by *<name> at instantiation,
// where <name> is the name of the specific instance, as well as the
// name of the baseWidget. These are only defaults, and may be overriden
// in a resource file by providing a more specific resource name

String  CviewMainWindow::_defaultCviewMainWindowResources[] = {
        "*closeButton.accelerator:  Ctrl<Key>W",
        "*closeButton.acceleratorText:  Ctrl+W",
        "*closeButton.labelString:  Close",
        "*closeButton.mnemonic:  C",
        "*copyButton.accelerator:  Ctrl<Key>C",
        "*copyButton.acceleratorText:  Ctrl+C",
        "*copyButton.labelString:  Copy",
        "*copyButton.mnemonic:  C",
        "*cutButton.accelerator:  Ctrl<Key>X",
        "*cutButton.acceleratorText:  Ctrl+X",
        "*cutButton.labelString:  Cut",
        "*cutButton.mnemonic:  t",
        "*editPane.labelString:  Edit",
        "*editPane.mnemonic:  E",
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
        "*newButton.accelerator:  Ctrl<Key>N",
        "*newButton.acceleratorText:  Ctrl+N",
        "*newButton.labelString:  New",
        "*newButton.mnemonic:  N",
        "*openButton.accelerator:  Ctrl<Key>O",
        "*openButton.acceleratorText:  Ctrl+O",
        "*openButton.labelString:  Open...",
        "*openButton.mnemonic:  O",
        "*optionsPane.labelString:  Options",
        "*optionsPane.mnemonic:  O",
        "*pasteButton.accelerator:  Ctrl<Key>V",
        "*pasteButton.acceleratorText:  Ctrl+V",
        "*pasteButton.labelString:  Paste",
        "*pasteButton.mnemonic:  P",
        "*printButton.accelerator:  Ctrl<Key>P",
        "*printButton.acceleratorText:  Ctrl+P",
        "*printButton.labelString:  Print",
        "*printButton.mnemonic:  P",
        "*saveButton.accelerator:  Ctrl<Key>S",
        "*saveButton.acceleratorText:  Ctrl+S",
        "*saveButton.labelString:  Save",
        "*saveButton.mnemonic:  S",
        "*saveasButton.labelString:  Save As...",
        "*saveasButton.mnemonic:  A",
        "*viewPane.labelString:  View",
        "*viewPane.mnemonic:  V",

        //---- Start editable code block: CviewMainWindow Default Resources

        //---- End editable code block: CviewMainWindow Default Resources

        (char*)NULL
};


//---- Class declaration

CviewMainWindow::CviewMainWindow ( const char *name,
                                  ArgList args,
                                  Cardinal argCount) : 
                            VkWindow ( name, args, argCount )
{
    // Load any class-default resources for this object
    setDefaultResources ( baseWidget(), _defaultCviewMainWindowResources  );


    // Create the view component contained by this window
    _deck = new DeckTabbedDeck ( "deck",mainWindowWidget() );

    XtVaSetValues ( _deck->baseWidget(),
                    XmNwidth, 738, 
                    XmNheight, 878, 
                    (XtPointer) NULL );

    // Add the component as the main view
    addView ( _deck );

    // Create the panes of this window's menubar. The menubar itself
    // is created automatically by ViewKit

    // The filePane menu pane
    _filePane =  addMenuPane ( "filePane" );

#ifdef NONEEDYET
    _newButton =  _filePane->addAction ( "newButton", 
                                        &CviewMainWindow::newFileCallback, 
                                        (XtPointer) this );
    _openButton =  _filePane->addAction ( "openButton", 
                                        &CviewMainWindow::openFileCallback, 
                                        (XtPointer) this );
    _saveButton =  _filePane->addAction ( "saveButton", 
                                        &CviewMainWindow::saveCallback, 
                                        (XtPointer) this );
    _saveasButton =  _filePane->addAction ( "saveasButton", 
                                        &CviewMainWindow::saveasCallback, 
                                        (XtPointer) this );
    _printButton =  _filePane->addAction ( "printButton", 
                                        &CviewMainWindow::printCallback, 
                                        (XtPointer) this );
    _separator1 =  _filePane->addSeparator();
#endif

    _closeButton =  _filePane->addAction ( "closeButton", 
                                        &CviewMainWindow::closeCallback, 
                                        (XtPointer) this );
    _exitButton =  _filePane->addAction ( "exitButton", 
                                        &CviewMainWindow::quitCallback, 
                                        (XtPointer) this );
#ifdef NONEEDYET
    // The editPane menu pane
    _editPane =  addMenuPane ( "editPane" );

    _cutButton =  _editPane->addAction ( "cutButton", 
                                        &CviewMainWindow::cutCallback, 
                                        (XtPointer) this );
    _copyButton =  _editPane->addAction ( "copyButton", 
                                        &CviewMainWindow::copyCallback, 
                                        (XtPointer) this );
    _pasteButton =  _editPane->addAction ( "pasteButton", 
                                        &CviewMainWindow::pasteCallback, 
                                        (XtPointer) this );
    // The viewPane menu pane
    _viewPane =  addMenuPane ( "viewPane" );

    _viewControl1 =  _viewPane->addAction ( "viewControl1",
                                        NULL, // No callback function given
                                        (XtPointer) this );
    _viewControl2 =  _viewPane->addAction ( "viewControl2",
                                        NULL, // No callback function given
                                        (XtPointer) this );
    _viewControl3 =  _viewPane->addAction ( "viewControl3",
                                        NULL, // No callback function given
                                        (XtPointer) this );

    // The optionsPane menu pane
    _optionsPane =  addMenuPane ( "optionsPane" );

    _option1 =  _optionsPane->addToggle ( "option1", 
                                        NULL, // No Callback given
                                        (XtPointer) this );
    _option2 =  _optionsPane->addToggle ( "option2", 
                                        NULL, // No Callback given
                                        (XtPointer) this );
#endif /* NONEEDYET */

    //---- Start editable code block: CviewMainWindow constructor

    //---- End editable code block: CviewMainWindow constructor

}    // End Constructor


CviewMainWindow::~CviewMainWindow()
{
    delete _deck;
    //---- Start editable code block: CviewMainWindow destructor


    //---- End editable code block: CviewMainWindow destructor
}    // End destructor


const char *CviewMainWindow::className()
{
    return ("CviewMainWindow");
}    // End className()


Boolean CviewMainWindow::okToQuit()
{
    //---- Start editable code block: CviewMainWindow okToQuit



    // This member function is called when the user quits by calling
    // theApplication->terminate() or uses the window manager close protocol
    // This function can abort the operation by returning FALSE, or do some.
    // cleanup before returning TRUE. The actual decision is normally passed on
    // to the view object


    // Query the view object, and give it a chance to cleanup

    return ( _deck->okToQuit() );

    //---- End editable code block: CviewMainWindow okToQuit
}    // End okToQuit()



/////////////////////////////////////////////////////////////// 
// The following functions are static member functions used to 
// interface with Motif.
/////////////////////////////////// 

void CviewMainWindow::closeCallback ( Widget    w,
                                      XtPointer clientData,
                                      XtPointer callData ) 
{ 
    CviewMainWindow* obj = ( CviewMainWindow * ) clientData;
    obj->close ( w, callData );
}

void CviewMainWindow::copyCallback ( Widget    w,
                                     XtPointer clientData,
                                     XtPointer callData ) 
{ 
    CviewMainWindow* obj = ( CviewMainWindow * ) clientData;
    obj->copy ( w, callData );
}

void CviewMainWindow::cutCallback ( Widget    w,
                                    XtPointer clientData,
                                    XtPointer callData ) 
{ 
    CviewMainWindow* obj = ( CviewMainWindow * ) clientData;
    obj->cut ( w, callData );
}

void CviewMainWindow::newFileCallback ( Widget    w,
                                        XtPointer clientData,
                                        XtPointer callData ) 
{ 
    CviewMainWindow* obj = ( CviewMainWindow * ) clientData;
    obj->newFile ( w, callData );
}

void CviewMainWindow::openFileCallback ( Widget    w,
                                         XtPointer clientData,
                                         XtPointer callData ) 
{ 
    CviewMainWindow* obj = ( CviewMainWindow * ) clientData;
    obj->openFile ( w, callData );
}

void CviewMainWindow::pasteCallback ( Widget    w,
                                      XtPointer clientData,
                                      XtPointer callData ) 
{ 
    CviewMainWindow* obj = ( CviewMainWindow * ) clientData;
    obj->paste ( w, callData );
}

void CviewMainWindow::printCallback ( Widget    w,
                                      XtPointer clientData,
                                      XtPointer callData ) 
{ 
    CviewMainWindow* obj = ( CviewMainWindow * ) clientData;
    obj->print ( w, callData );
}

void CviewMainWindow::quitCallback ( Widget    w,
                                     XtPointer clientData,
                                     XtPointer callData ) 
{ 
    CviewMainWindow* obj = ( CviewMainWindow * ) clientData;
    obj->quit ( w, callData );
}

void CviewMainWindow::saveCallback ( Widget    w,
                                     XtPointer clientData,
                                     XtPointer callData ) 
{ 
    CviewMainWindow* obj = ( CviewMainWindow * ) clientData;
    obj->save ( w, callData );
}

void CviewMainWindow::saveasCallback ( Widget    w,
                                       XtPointer clientData,
                                       XtPointer callData ) 
{ 
    CviewMainWindow* obj = ( CviewMainWindow * ) clientData;
    obj->saveas ( w, callData );
}





/////////////////////////////////////////////////////////////// 
// The following functions are called from callbacks 
/////////////////////////////////// 

void CviewMainWindow::close ( Widget, XtPointer ) 
{
    //---- Start editable code block:  close


    // To close a window, just delete the object
    // checking first with the view object.
    // If this is the last window, ViewKit will exit

    if(okToQuit())
        delete this;

    //---- End editable code block:  close
}    // End CviewMainWindow::close()

void CviewMainWindow::copy ( Widget w, XtPointer callData ) 
{
    //---- Start editable code block: CviewMainWindow copy

    XmPushButtonCallbackStruct *cbs = (XmPushButtonCallbackStruct*) callData;

    // By default this member function passes control
    // to the component contained by this window

    _deck->copy();


    //---- End editable code block: CviewMainWindow copy

}    // End CviewMainWindow::copy()


void CviewMainWindow::cut ( Widget w, XtPointer callData ) 
{
    //---- Start editable code block: CviewMainWindow cut

    XmPushButtonCallbackStruct *cbs = (XmPushButtonCallbackStruct*) callData;

    // By default this member function passes control
    // to the component contained by this window

    _deck->cut();


    //---- End editable code block: CviewMainWindow cut

}    // End CviewMainWindow::cut()


void CviewMainWindow::newFile ( Widget w, XtPointer callData ) 
{
    //---- Start editable code block: CviewMainWindow newFile

    XmPushButtonCallbackStruct *cbs = (XmPushButtonCallbackStruct*) callData;

    // By default this member function passes control
    // to the component contained by this window

    _deck->newFile();


    //---- End editable code block: CviewMainWindow newFile

}    // End CviewMainWindow::newFile()


void CviewMainWindow::openFile ( Widget, XtPointer ) 
{
    //---- Start editable code block: CviewMainWindow openFile

    // This virtual function is called from openFileCallback
    // Use the blocking mode of the file selection dialog
    // to get a file

    if(theFileSelectionDialog->postAndWait() == VkDialogManager::OK)
    {
        _deck->openFile(theFileSelectionDialog->fileName());
    }

    //---- End editable code block: CviewMainWindow openFile

}    // End CviewMainWindow::openFile()

void CviewMainWindow::paste ( Widget w, XtPointer callData ) 
{
    //---- Start editable code block: CviewMainWindow paste

    XmPushButtonCallbackStruct *cbs = (XmPushButtonCallbackStruct*) callData;

    // By default this member function passes control
    // to the component contained by this window

    _deck->paste();


    //---- End editable code block: CviewMainWindow paste

}    // End CviewMainWindow::paste()


void CviewMainWindow::print ( Widget, XtPointer ) 
{
    //---- Start editable code block:  print


    // This virtual function is called from printCallback

    _deck->print(NULL);

    //---- End editable code block:  print

}    // End CviewMainWindow::print()

void CviewMainWindow::quit ( Widget, XtPointer ) 
{
    // Exit via quitYourself() to allow the application
    // to go through its normal shutdown routines, checking with
    // all windows, views, and so on.

    theApplication->quitYourself();

}    // End CviewMainWindow::quit()

void CviewMainWindow::save ( Widget w, XtPointer callData ) 
{
    //---- Start editable code block: CviewMainWindow save

    XmPushButtonCallbackStruct *cbs = (XmPushButtonCallbackStruct*) callData;

    // By default this member function passes control
    // to the component contained by this window

    _deck->save();


    //---- End editable code block: CviewMainWindow save

}    // End CviewMainWindow::save()


void CviewMainWindow::saveas ( Widget, XtPointer ) 
{
    //---- Start editable code block: CviewMainWindow saveas


    // This virtual function is called from saveasCallback
    // Use the blocking mode of the file selection dialog
    // to get a file

    if(theFileSelectionDialog->postAndWait() == VkDialogManager::OK)
    {
        _deck->saveas(theFileSelectionDialog->fileName());

    }

    //---- End editable code block: CviewMainWindow saveas

}    // End CviewMainWindow::saveAs()




//---- Start editable code block: End of generated code


//---- End editable code block: End of generated code


