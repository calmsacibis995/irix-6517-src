
//////////////////////////////////////////////////////////////
//
// Header file for CviewMainWindow
//
//    This class is a subclass of VkWindow
//
// Normally, very little in this file should need to be changed.
// Create/add/modify menus using RapidApp.
//
// Restrict changes to those sections between
// the "//--- Start/End editable code block" markers
// Doing so will allow you to make changes using RapidApp
// without losing any changes you may have made manually
//
//////////////////////////////////////////////////////////////
#ifndef CVIEWMAINWINDOW_H
#define CVIEWMAINWINDOW_H
#include <Vk/VkWindow.h>


class VkMenuItem;
class VkMenuToggle;
class VkMenuConfirmFirstAction;
class VkSubMenu;
class VkRadioSubMenu;

//---- Start editable code block: headers and declarations


//---- End editable code block: headers and declarations


//---- CviewMainWindow class declaration

class CviewMainWindow: public VkWindow {

  public:

    CviewMainWindow( const char * name, 
                     ArgList args = NULL,
                     Cardinal argCount = 0 );
    ~CviewMainWindow();
    const char *className();
    virtual Boolean okToQuit();

    //---- Start editable code block: CviewMainWindow public


    //---- End editable code block: CviewMainWindow public


  protected:



    // Classes created by this class

    class DeckTabbedDeck *_deck;


    // Widgets created by this class


    // Menu items created by this class
    VkSubMenu  *_filePane;
    VkMenuItem *_newButton;
    VkMenuItem *_openButton;
    VkMenuItem *_saveButton;
    VkMenuItem *_saveasButton;
    VkMenuItem *_printButton;
    VkMenuItem *_separator1;
    VkMenuItem *_closeButton;
    VkMenuItem *_exitButton;
    VkSubMenu  *_editPane;
    VkMenuItem *_cutButton;
    VkMenuItem *_copyButton;
    VkMenuItem *_pasteButton;
    VkSubMenu  *_viewPane;
    VkMenuItem *_viewControl1;
    VkMenuItem *_viewControl2;
    VkMenuItem *_viewControl3;
    VkSubMenu  *_optionsPane;
    VkMenuToggle *_option1;
    VkMenuToggle *_option2;

    // Member functions called from callbacks

    virtual void close ( Widget, XtPointer );
    virtual void copy ( Widget, XtPointer );
    virtual void cut ( Widget, XtPointer );
    virtual void newFile ( Widget, XtPointer );
    virtual void openFile ( Widget, XtPointer );
    virtual void paste ( Widget, XtPointer );
    virtual void print ( Widget, XtPointer );
    virtual void quit ( Widget, XtPointer );
    virtual void save ( Widget, XtPointer );
    virtual void saveas ( Widget, XtPointer );


    //---- Start editable code block: CviewMainWindow protected


    //---- End editable code block: CviewMainWindow protected


  private:


    // Callbacks to interface with Motif

    static void closeCallback ( Widget, XtPointer, XtPointer );
    static void copyCallback ( Widget, XtPointer, XtPointer );
    static void cutCallback ( Widget, XtPointer, XtPointer );
    static void newFileCallback ( Widget, XtPointer, XtPointer );
    static void openFileCallback ( Widget, XtPointer, XtPointer );
    static void pasteCallback ( Widget, XtPointer, XtPointer );
    static void printCallback ( Widget, XtPointer, XtPointer );
    static void quitCallback ( Widget, XtPointer, XtPointer );
    static void saveCallback ( Widget, XtPointer, XtPointer );
    static void saveasCallback ( Widget, XtPointer, XtPointer );

    static String  _defaultCviewMainWindowResources[];


    //---- Start editable code block: CviewMainWindow private


    //---- End editable code block: CviewMainWindow private


};
//---- Start editable code block: End of generated code


//---- End editable code block: End of generated code

#endif
