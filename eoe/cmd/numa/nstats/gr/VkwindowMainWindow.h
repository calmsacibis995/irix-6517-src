
//////////////////////////////////////////////////////////////
//
// Header file for VkwindowMainWindow
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
#ifndef VKWINDOWMAINWINDOW_H
#define VKWINDOWMAINWINDOW_H
#include <Vk/VkWindow.h>


class VkMenuItem;
class VkMenuToggle;
class VkMenuConfirmFirstAction;
class VkSubMenu;
class VkRadioSubMenu;

//---- Start editable code block: headers and declarations


//---- End editable code block: headers and declarations


//---- VkwindowMainWindow class declaration

class VkwindowMainWindow: public VkWindow {

  public:

    VkwindowMainWindow( const char * name, 
                        ArgList args = NULL,
                        Cardinal argCount = 0 );
    ~VkwindowMainWindow();
    const char *className();
    virtual Boolean okToQuit();

    //---- Start editable code block: VkwindowMainWindow public


    //---- End editable code block: VkwindowMainWindow public


  protected:



    // Classes created by this class

    class BulletinBoard *_bulletinBoard;


    // Widgets created by this class


    // Menu items created by this class
    VkSubMenu  *_filePane;
    VkMenuItem *_closeButton;
    VkMenuItem *_exitButton;
    VkSubMenu  *_viewPane1;
    VkMenuToggle *_toggle;
    VkSubMenu  *_optionsPane;
    VkSubMenu  *_radioInterval;
    VkMenuToggle *_radUI10;
    VkMenuToggle *_radUI100;
    VkMenuToggle *_radUI1000;
    VkSubMenu  *_radioMaxDiff;
    VkMenuToggle *_radMD10;
    VkMenuToggle *_radMD100;
    VkMenuToggle *_radMD1000;

    // Member functions called from callbacks

    virtual void close ( Widget, XtPointer );
    virtual void quit ( Widget, XtPointer );
    virtual void setDiff10 ( Widget, XtPointer );
    virtual void setDiff100 ( Widget, XtPointer );
    virtual void setDiff1000 ( Widget, XtPointer );
    virtual void setInterval10 ( Widget, XtPointer );
    virtual void setInterval100 ( Widget, XtPointer );
    virtual void setInterval1000 ( Widget, XtPointer );
    virtual void setTotal ( Widget, XtPointer );

    //---- Start editable code block: VkwindowMainWindow protected


    //---- End editable code block: VkwindowMainWindow protected


  private:


    // Callbacks to interface with Motif

    static void closeCallback ( Widget, XtPointer, XtPointer );
    static void quitCallback ( Widget, XtPointer, XtPointer );
    static void setDiff10Callback ( Widget, XtPointer, XtPointer );
    static void setDiff100Callback ( Widget, XtPointer, XtPointer );
    static void setDiff1000Callback ( Widget, XtPointer, XtPointer );
    static void setInterval10Callback ( Widget, XtPointer, XtPointer );
    static void setInterval100Callback ( Widget, XtPointer, XtPointer );
    static void setInterval1000Callback ( Widget, XtPointer, XtPointer );
    static void setTotalCallback ( Widget, XtPointer, XtPointer );

    static String  _defaultVkwindowMainWindowResources[];


    //---- Start editable code block: VkwindowMainWindow private


    //---- End editable code block: VkwindowMainWindow private


};
//---- Start editable code block: End of generated code


//---- End editable code block: End of generated code

#endif
