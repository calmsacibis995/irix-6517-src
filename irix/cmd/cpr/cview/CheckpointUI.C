
/////////////////////////////////////////////////////////////
//
// Source file for CheckpointUI
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


#include "CheckpointUI.h" // Generated header file for this class

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
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


//---- End editable code block: headers and declarations


// These are default resources for widgets in objects of this class
// All resources will be prepended by *<name> at instantiation,
// where <name> is the name of the specific instance, as well as the
// name of the baseWidget. These are only defaults, and may be overriden
// in a resource file by providing a more specific resource name

String  CheckpointUI::_defaultCheckpointUIResources[] = {
        "*but_file.labelString:  STEP II: Push Here if You Need to Change Checkpoint Options",
        "*but_ok.labelString:  STEP III: OK, Go Checkpoint!",
        "*but_ps.labelString:  STEP I: Display and Select a Checkpoint Target Owned by User",
        "*button.labelString:  No, Cancel Checkpoint",
        "*cont.labelString:  Continue",
        "*exit.labelString:  Exit",
        "*fileMenu.labelString:  Set the following open file dispositions to ",
        "*idMenu.labelString:  Checkpoint the selected job as a ",
        "*label1.labelString:  CHECKPOINT CONTROL PANEL",
        "*label2.labelString:  Enter a filename to save your job:",
        "*opt_append.labelString:  APPEND (no save & reopen to append)",
        "*opt_ash.labelString:  Array Session",
        "*opt_gid.labelString:  Process Group",
        "*opt_hid.labelString:  Process Hierarchy",
        "*opt_ignore.labelString:  IGNORE (no save & reopen as new)",
        "*opt_merge.labelString:  MERGE (no save & reopen at current location)",
        "*opt_pid.labelString:  Individual Process",
        "*opt_replace.labelString:  REPLACE (save a copy to replace this file)",
        "*opt_sgp.labelString:  Sproc Shared Group",
        "*opt_sid.labelString:  Unix Session",
        "*opt_substitute.labelString:  SUBSTITUTE (save a copy & use the copy)",
	"*pstitle.labelString:  USER        PID    PPID    PGID    SID                ASH     COMMAND          ",
        "*tabLabel:  Checkpoint Control Panel",
        "*tog_upgrade.labelString:  Checkpoint for system upgrade",
        "*willMenu.labelString:  After checkpoint, the selected job will",
        "+*pstitle.fontList:  SGI_DYNAMIC SmallFixedWidthFont",

        //---- Start editable code block: CheckpointUI Default Resources

        //---- End editable code block: CheckpointUI Default Resources

        (char*)NULL
};

CheckpointUI::CheckpointUI ( const char *name ) : VkComponent ( name ) 
{ 
    // No widgets are created by this constructor.
    // If an application creates a component using this constructor,
    // It must explictly call create at a later time.
    // This is mostly useful when adding pre-widget creation
    // code to a derived class constructor.

    //---- Start editable code block: Checkpoint constructor 2

    //---- End editable code block: Checkpoint constructor 2
}    // End Constructor


CheckpointUI::CheckpointUI ( const char *name, Widget parent ) : VkComponent ( name ) 
{ 
    //---- Start editable code block: Checkpoint pre-create

    //---- End editable code block: Checkpoint pre-create

    // Call creation function to build the widget tree.

     create ( parent );

    //---- Start editable code block: Checkpoint constructor

    //---- End editable code block: Checkpoint constructor
}    // End Constructor


CheckpointUI::~CheckpointUI() 
{
    // Base class destroys widgets

    //---- Start editable code block: CheckpointUI destructor


    //---- End editable code block: CheckpointUI destructor
}    // End destructor



void CheckpointUI::create ( Widget parent )
{
    Arg      args[13];
    Cardinal count;
    count = 0;

    // Load any class-defaulted resources for this object

    setDefaultResources ( parent, _defaultCheckpointUIResources  );


    // Create an unmanaged widget as the top of the widget hierarchy

    _baseWidget = _checkpoint = XtVaCreateWidget ( _name,
                                                   sgRubberBoardWidgetClass,
                                                   parent, 
                                                   XmNinitialParentHeight, 623, 
                                                   XmNfinalParentHeight, 908, 
                                                   XmNfinalParentWidth, 930, 
                                                   XmNinitialParentWidth, 557, 
                                                   (XtPointer) NULL ); 

    // install a callback to guard against unexpected widget destruction

    installDestroyHandler();


    // Create widgets used in this component
    // All variables are data members of this class

    _scrolledWindow = XtVaCreateManagedWidget  ( "scrolledWindow",
                                                  xmScrolledWindowWidgetClass,
                                                  _baseWidget, 
                                                  XmNscrollBarDisplayPolicy, XmSTATIC, 
                                                  XmNinitialX, 30, 
                                                  XmNinitialY, 124, 
                                                  XmNinitialWidth, 535, 
                                                  XmNinitialHeight, 55, 
                                                  XmNfinalX, 32, 
                                                  XmNfinalY, 122, 
                                                  XmNfinalWidth, 827, 
                                                  XmNfinalHeight, 150, 
                                                  (XtPointer) NULL ); 

    _psList = XtVaCreateManagedWidget  ( "psList",
                                                xmListWidgetClass,
                                                _scrolledWindow, 
                                          XmNlistSizePolicy, XmCONSTANT, 
                                          XmNwidth, 664, 
                                          XmNheight, 118, 
                                                (XtPointer) NULL ); 

    XtAddCallback ( _psList,
                    XmNbrowseSelectionCallback,
                    &CheckpointUI::cv_select_oneCallback,
                    (XtPointer) this ); 

    _label1 = XtVaCreateManagedWidget  ( "label1",
                                          xmLabelWidgetClass,
                                          _baseWidget, 
                                          XmNlabelType, XmSTRING, 
                                          XmNinitialX, 110, 
                                          XmNinitialY, 10, 
                                          XmNinitialWidth, 390, 
                                          XmNinitialHeight, 29, 
                                          XmNfinalX, 110, 
                                          XmNfinalY, 10, 
                                          XmNfinalWidth, 609, 
                                          XmNfinalHeight, 29, 
                                          (XtPointer) NULL ); 


    _separator1 = XtVaCreateManagedWidget  ( "separator1",
                                              xmSeparatorWidgetClass,
                                              _baseWidget, 
                                              XmNinitialX, 10, 
                                              XmNinitialY, 30, 
                                              XmNinitialWidth, 550, 
                                              XmNinitialHeight, 20, 
                                              XmNfinalX, 10, 
                                              XmNfinalY, 30, 
                                              XmNfinalWidth, 860, 
                                              XmNfinalHeight, 20, 
                                              (XtPointer) NULL ); 


    _but_ps = XtVaCreateManagedWidget  ( "but_ps",
                                          xmPushButtonWidgetClass,
                                          _baseWidget, 
                                          XmNlabelType, XmSTRING, 
                                          XmNinitialX, 10, 
                                          XmNinitialY, 55, 
                                          XmNinitialWidth, 400, 
                                          XmNinitialHeight, 30, 
                                          XmNfinalX, 10, 
                                          XmNfinalY, 57, 
                                          XmNfinalWidth, 470, 
                                          XmNfinalHeight, 30, 
                                          (XtPointer) NULL ); 

    XtAddCallback ( _but_ps,
                    XmNactivateCallback,
                    &CheckpointUI::cv_psCallback,
                    (XtPointer) this ); 


    _finderUid = XtVaCreateManagedWidget  ( "finderUid",
                                             sgFinderWidgetClass,
                                             _baseWidget, 
                                             XmNinitialX, 420, 
                                             XmNinitialY, 50, 
                                             XmNinitialWidth, 130, 
                                             XmNinitialHeight, 55, 
                                             XmNfinalX, 550, 
                                             XmNfinalY, 52, 
                                             XmNfinalWidth, 280, 
                                             XmNfinalHeight, 55, 
                                             (XtPointer) NULL ); 

    XtAddCallback ( _finderUid,
                    XmNactivateCallback,
                    &CheckpointUI::cv_uidCallback,
                    (XtPointer) this ); 

    XtAddCallback ( _finderUid,
                    XmNvalueChangedCallback,
                    &CheckpointUI::cv_uid_changeCallback,
                    (XtPointer) this ); 

    _idMenu = new VkOptionMenu ( _baseWidget, "idMenu");
    _opt_pid =  _idMenu->addAction ( "opt_pid", 
                                     &CheckpointUI::doOpt_pidCallback, 
                                     (XtPointer) this );

    _opt_gid =  _idMenu->addAction ( "opt_gid", 
                                     &CheckpointUI::doOpt_gidCallback, 
                                     (XtPointer) this );

    _opt_sid =  _idMenu->addAction ( "opt_sid", 
                                     &CheckpointUI::doOpt_sidCallback, 
                                     (XtPointer) this );

    _opt_ash =  _idMenu->addAction ( "opt_ash", 
                                     &CheckpointUI::doOpt_ashCallback, 
                                     (XtPointer) this );

    _opt_hid =  _idMenu->addAction ( "opt_hid", 
                                     &CheckpointUI::doOpt_hidCallback, 
                                     (XtPointer) this );

    _opt_sgp =  _idMenu->addAction ( "opt_sgp", 
                                     &CheckpointUI::doOpt_sgpCallback, 
                                     (XtPointer) this );

    _finderStatef = XtVaCreateManagedWidget  ( "finderStatef",
                                                sgFinderWidgetClass,
                                                _baseWidget, 
                                                XmNinitialX, 275, 
                                                XmNinitialY, 225, 
                                                XmNinitialWidth, 282, 
                                                XmNinitialHeight, 60, 
                                                XmNfinalX, 285, 
                                                XmNfinalY, 320, 
                                                XmNfinalWidth, 596, 
                                                XmNfinalHeight, 60, 
                                                (XtPointer) NULL ); 

    XtAddCallback ( _finderStatef,
                    XmNactivateCallback,
                    &CheckpointUI::cv_statefCallback,
                    (XtPointer) this ); 

    XtAddCallback ( _finderStatef,
                    XmNvalueChangedCallback,
                    &CheckpointUI::cv_statef_changeCallback,
                    (XtPointer) this ); 

    _label2 = XtVaCreateManagedWidget  ( "label2",
                                          xmLabelWidgetClass,
                                          _baseWidget, 
                                          XmNlabelType, XmSTRING, 
                                          XmNinitialX, 22, 
                                          XmNinitialY, 235, 
                                          XmNinitialWidth, 247, 
                                          XmNinitialHeight, 29, 
                                          XmNfinalX, 18, 
                                          XmNfinalY, 330, 
                                          XmNfinalWidth, 257, 
                                          XmNfinalHeight, 29, 
                                          (XtPointer) NULL ); 


    _separator2 = XtVaCreateManagedWidget  ( "separator2",
                                              xmSeparatorWidgetClass,
                                              _baseWidget, 
                                              XmNinitialX, 10, 
                                              XmNinitialY, 300, 
                                              XmNinitialWidth, 550, 
                                              XmNinitialHeight, 20, 
                                              XmNfinalX, 10, 
                                              XmNfinalY, 410, 
                                              XmNfinalWidth, 851, 
                                              XmNfinalHeight, 20, 
                                              (XtPointer) NULL ); 


    _but_file = XtVaCreateManagedWidget  ( "but_file",
                                            xmPushButtonWidgetClass,
                                            _baseWidget, 
                                            XmNlabelType, XmSTRING, 
                                            XmNinitialX, 10, 
                                            XmNinitialY, 323, 
                                            XmNinitialWidth, 430, 
                                            XmNinitialHeight, 32, 
                                            XmNfinalX, 10, 
                                            XmNfinalY, 436, 
                                            XmNfinalWidth, 430, 
                                            XmNfinalHeight, 32, 
                                            (XtPointer) NULL ); 

    XtAddCallback ( _but_file,
                    XmNactivateCallback,
                    &CheckpointUI::cv_fileCallback,
                    (XtPointer) this ); 


    _fileMenu = new VkOptionMenu ( _baseWidget, "fileMenu");
    _opt_merge =  _fileMenu->addAction ( "opt_merge", 
                                         &CheckpointUI::doOpt_mergeCallback, 
                                         (XtPointer) this );

    _opt_ignore =  _fileMenu->addAction ( "opt_ignore", 
                                          &CheckpointUI::doOpt_ignoreCallback, 
                                          (XtPointer) this );

    _opt_append =  _fileMenu->addAction ( "opt_append", 
                                          &CheckpointUI::doOpt_appendCallback, 
                                          (XtPointer) this );

    _opt_replace =  _fileMenu->addAction ( "opt_replace", 
                                           &CheckpointUI::doOpt_replaceCallback, 
                                           (XtPointer) this );

    _opt_substitute =  _fileMenu->addAction ( "opt_substitute", 
                                              &CheckpointUI::doOpt_substituteCallback, 
                                              (XtPointer) this );

    _scrolledWindow1 = XtVaCreateManagedWidget  ( "scrolledWindow1",
                                                   xmScrolledWindowWidgetClass,
                                                   _baseWidget, 
                                                   XmNscrollBarDisplayPolicy, XmSTATIC, 
                                                   XmNinitialX, 30, 
                                                   XmNinitialY, 429, 
                                                   XmNinitialWidth, 535, 
                                                   XmNinitialHeight, 96, 
                                                   XmNfinalX, 30, 
                                                   XmNfinalY, 591, 
                                                   XmNfinalWidth, 836, 
                                                   XmNfinalHeight, 190, 
                                                   (XtPointer) NULL ); 

    _fileList = XtVaCreateManagedWidget  ( "fileList",
                                            xmListWidgetClass,
                                            _scrolledWindow1, 
                                            XmNselectionPolicy, XmEXTENDED_SELECT, 
                                            XmNlistSizePolicy, XmCONSTANT, 
                                            XmNwidth, 668, 
                                            XmNheight, 152, 
                                            (XtPointer) NULL ); 

    XtAddCallback ( _fileList,
                    XmNextendedSelectionCallback,
                    &CheckpointUI::cv_selectCallback,
                    (XtPointer) this ); 


    _separator3 = XtVaCreateManagedWidget  ( "separator3",
                                              xmSeparatorWidgetClass,
                                              _baseWidget, 
                                              XmNinitialX, 10, 
                                              XmNinitialY, 526, 
                                              XmNinitialWidth, 550, 
                                              XmNinitialHeight, 20, 
                                              XmNfinalX, 10, 
                                              XmNfinalY, 793, 
                                              XmNfinalWidth, 859, 
                                              XmNfinalHeight, 20, 
                                              (XtPointer) NULL ); 


    _but_ok = XtVaCreateManagedWidget  ( "but_ok",
                                          xmPushButtonWidgetClass,
                                          _baseWidget, 
                                          XmNlabelType, XmSTRING, 
                                          XmNinitialX, 10, 
                                          XmNinitialY, 546, 
                                          XmNinitialWidth, 210, 
                                          XmNinitialHeight, 32, 
                                          XmNfinalX, 10, 
                                          XmNfinalY, 822, 
                                          XmNfinalWidth, 210, 
                                          XmNfinalHeight, 32, 
                                          (XtPointer) NULL ); 

    XtAddCallback ( _but_ok,
                    XmNactivateCallback,
                    &CheckpointUI::cv_okCallback,
                    (XtPointer) this ); 


    _willMenu = new VkOptionMenu ( _baseWidget, "willMenu");
    _exit =  _willMenu->addAction ( "exit", 
                                    &CheckpointUI::doExitCallback, 
                                    (XtPointer) this );

    _cont =  _willMenu->addAction ( "cont", 
                                    &CheckpointUI::doContCallback, 
                                    (XtPointer) this );

    _pstitle = XtVaCreateManagedWidget  ( "pstitle",
                                          xmLabelWidgetClass,
                                          _baseWidget, 
                                          XmNlabelType, XmSTRING, 
                                          XmNinitialX, 30, 
                                          XmNinitialY, 104, 
                                          XmNinitialWidth, 560, 
                                          XmNinitialHeight, 20, 
                                          XmNfinalX, 33, 
                                          XmNfinalY, 104, 
                                          XmNfinalWidth, 560, 
                                          XmNfinalHeight, 20, 
                                          (XtPointer) NULL ); 


    _but_abort = XtVaCreateManagedWidget  ( "but_abort",
                                          xmPushButtonWidgetClass,
                                          _baseWidget, 
                                          XmNlabelType, XmSTRING, 
                                          XmNinitialX, 350, 
                                          XmNinitialY, 546, 
                                          XmNinitialWidth, 210, 
                                          XmNinitialHeight, 32, 
                                          XmNfinalX, 550, 
                                          XmNfinalY, 822, 
                                          XmNfinalWidth, 210, 
                                          XmNfinalHeight, 32, 
                                          (XtPointer) NULL ); 

    XtAddCallback ( _but_abort,
                    XmNactivateCallback,
                    &CheckpointUI::cv_abortCallback,
                    (XtPointer) this ); 

    _tog_upgrade = XtVaCreateManagedWidget  ( "tog_upgrade",
                                               xmToggleButtonWidgetClass,
                                               _baseWidget, 
                                               XmNalignment, XmALIGNMENT_BEGINNING, 
                                               XmNlabelType, XmSTRING, 
					       XmNinitialX, 25, 
                    			       //XmNinitialY, 371, 
                    			       XmNinitialY, 275, 
                    			       XmNinitialWidth, 320, 
                    			       XmNinitialHeight, 28, 
                    			       XmNfinalX, 28, 
                    			       //XmNfinalY, 510, 
                    			       XmNfinalY, 375, 
                    			       XmNfinalWidth, 320, 
                    			       XmNfinalHeight, 28, 
                                               (XtPointer) NULL ); 

    XtAddCallback ( _tog_upgrade,
                    XmNvalueChangedCallback,
                    &CheckpointUI::cv_upgradeCallback,
                    (XtPointer) this ); 

    XtVaSetValues ( _idMenu->baseWidget(),
                    XmNinitialX, 23, 
                    XmNinitialY, 185, 
                    XmNinitialWidth, 470, 
                    XmNinitialHeight, 32, 
                    XmNfinalX, 25, 
                    XmNfinalY, 280, 
                    XmNfinalWidth, 470, 
                    XmNfinalHeight, 32, 
                    (XtPointer) NULL );
    XtVaSetValues ( _fileMenu->baseWidget(),
                    XmNinitialX, 25, 
                    XmNinitialY, 398, 
                    XmNinitialWidth, 543, 
                    XmNinitialHeight, 32, 
                    XmNfinalX, 27, 
                    XmNfinalY, 541, 
                    XmNfinalWidth, 862, 
                    XmNfinalHeight, 32, 
                    (XtPointer) NULL );
    XtVaSetValues ( _willMenu->baseWidget(),
                    XmNinitialX, 25, 
                    XmNinitialY, 365, 
                    XmNinitialWidth, 413, 
                    XmNinitialHeight, 30, 
                    XmNfinalX, 28, 
                    XmNfinalY, 498, 
                    XmNfinalWidth, 447, 
                    XmNfinalHeight, 42, 
                    (XtPointer) NULL );

    //---- Start editable code block: CheckpointUI create

    //---- End editable code block: CheckpointUI create
}

const char * CheckpointUI::className()
{
    return ("CheckpointUI");
}    // End className()



/////////////////////////////////////////////////////////////// 
// The following functions are static member functions used to 
// interface with Motif.
/////////////////////////////////// 

void CheckpointUI::cv_abortCallback ( Widget    w,
                                      XtPointer clientData,
                                      XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    obj->cv_abort ( w, callData );
    obj->cv_file ( obj->_fileList, 0 );
    sleep(2);
    obj->cv_ps ( obj->_psList, callData );
}

void CheckpointUI::cv_fileCallback ( Widget    w,
                                     XtPointer clientData,
                                     XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    obj->cv_file ( obj->_fileList, callData );
}

void CheckpointUI::cv_upgradeCallback ( Widget    w,
                                        XtPointer clientData,
                                        XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    obj->cv_upgrade ( w, callData );
}

void CheckpointUI::cv_okCallback ( Widget    w,
                                   XtPointer clientData,
                                   XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    obj->cv_ok ( w, callData );
    obj->cv_file ( obj->_fileList, 0 );
    sleep(2);
    obj->cv_ps ( obj->_psList, callData );
}

void CheckpointUI::cv_psCallback ( Widget    w,
                                   XtPointer clientData,
                                   XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    // JIA obj->cv_uid ( obj->_finderUid, callData );
    obj->cv_ps ( obj->_psList, callData );
}

void CheckpointUI::cv_selectCallback ( Widget    w,
                                       XtPointer clientData,
                                       XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    obj->cv_select ( w, callData );
}

void CheckpointUI::cv_select_oneCallback ( Widget    w,
                                           XtPointer clientData,
                                           XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    obj->cv_select_one ( w, callData );
}

void CheckpointUI::cv_statefCallback ( Widget    w,
                                       XtPointer clientData,
                                       XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    obj->cv_statef ( w, callData );
}

void CheckpointUI::cv_statef_changeCallback ( Widget    w,
                                              XtPointer clientData,
                                              XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    obj->cv_statef_change ( w, callData );
}

void CheckpointUI::cv_uidCallback ( Widget    w,
                                    XtPointer clientData,
                                    XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    obj->cv_uid ( w, callData );
    obj->cv_ps ( obj->_psList, callData );
}

void CheckpointUI::cv_uid_changeCallback ( Widget    w,
                                           XtPointer clientData,
                                           XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    obj->cv_uid_change ( w, callData );
}

void CheckpointUI::doContCallback ( Widget    w,
                                    XtPointer clientData,
                                    XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    obj->doCont ( w, callData );
}

void CheckpointUI::doExitCallback ( Widget    w,
                                    XtPointer clientData,
                                    XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    obj->doExit ( w, callData );
}

void CheckpointUI::doOpt_appendCallback ( Widget    w,
                                          XtPointer clientData,
                                          XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    obj->doOpt_append ( obj->_fileList, callData );
}

void CheckpointUI::doOpt_ashCallback ( Widget    w,
                                       XtPointer clientData,
                                       XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    obj->doOpt_ash ( w, callData );
}

void CheckpointUI::doOpt_gidCallback ( Widget    w,
                                       XtPointer clientData,
                                       XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    obj->doOpt_gid ( w, callData );
}

void CheckpointUI::doOpt_hidCallback ( Widget    w,
                                       XtPointer clientData,
                                       XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    obj->doOpt_hid ( w, callData );
}

void CheckpointUI::doOpt_ignoreCallback ( Widget    w,
                                          XtPointer clientData,
                                          XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    obj->doOpt_ignore ( obj->_fileList, callData );
}

void CheckpointUI::doOpt_mergeCallback ( Widget    w,
                                         XtPointer clientData,
                                         XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    obj->doOpt_merge ( obj->_fileList, callData );
}

void CheckpointUI::doOpt_pidCallback ( Widget    w,
                                       XtPointer clientData,
                                       XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    obj->doOpt_pid ( w, callData );
}

void CheckpointUI::doOpt_replaceCallback ( Widget    w,
                                           XtPointer clientData,
                                           XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    obj->doOpt_replace ( obj->_fileList, callData );
}

void CheckpointUI::doOpt_sgpCallback ( Widget    w,
                                       XtPointer clientData,
                                       XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    obj->doOpt_sgp ( w, callData );
}

void CheckpointUI::doOpt_sidCallback ( Widget    w,
                                       XtPointer clientData,
                                       XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    obj->doOpt_sid ( w, callData );
}

void CheckpointUI::doOpt_substituteCallback ( Widget    w,
                                              XtPointer clientData,
                                              XtPointer callData ) 
{ 
    CheckpointUI* obj = ( CheckpointUI * ) clientData;
    obj->doOpt_substitute ( obj->_fileList, callData );
}

/////////////////////////////////////////////////////////////// 
// The following functions are called from the menu items 
// in this window.
/////////////////////////////////// 

void CheckpointUI::cv_abort ( Widget, XtPointer ) 
{
    // This virtual function is called from cv_abortCallback.
    // This function is normally overriden by a derived class.

}

void CheckpointUI::cv_file ( Widget, XtPointer ) 
{
    // This virtual function is called from cv_fileCallback.
    // This function is normally overriden by a derived class.

}

void CheckpointUI::cv_upgrade ( Widget, XtPointer ) 
{
    // This virtual function is called from cv_upgradeCallback.
    // This function is normally overriden by a derived class.

}

void CheckpointUI::cv_ok ( Widget, XtPointer ) 
{
    // This virtual function is called from cv_okCallback.
    // This function is normally overriden by a derived class.

}

void CheckpointUI::cv_ps ( Widget, XtPointer ) 
{
    // This virtual function is called from cv_psCallback.
    // This function is normally overriden by a derived class.
}

void CheckpointUI::cv_select ( Widget, XtPointer ) 
{
    // This virtual function is called from cv_selectCallback.
    // This function is normally overriden by a derived class.
}

void CheckpointUI::cv_select_one ( Widget, XtPointer ) 
{
    // This virtual function is called from cv_select_oneCallback.
    // This function is normally overriden by a derived class.
}

void CheckpointUI::cv_statef ( Widget, XtPointer ) 
{
    // This virtual function is called from cv_statefCallback.
    // This function is normally overriden by a derived class.
}

void CheckpointUI::cv_statef_change ( Widget, XtPointer ) 
{
    // This virtual function is called from cv_statef_changeCallback.
    // This function is normally overriden by a derived class.
}

void CheckpointUI::cv_uid ( Widget, XtPointer ) 
{
    // This virtual function is called from cv_uidCallback.
    // This function is normally overriden by a derived class.
}

void CheckpointUI::cv_uid_change ( Widget, XtPointer ) 
{
    // This virtual function is called from cv_uid_changeCallback.
    // This function is normally overriden by a derived class.
}

void CheckpointUI::doCont ( Widget, XtPointer ) 
{
    // This virtual function is called from doContCallback.
    // This function is normally overriden by a derived class.
}

void CheckpointUI::doExit ( Widget, XtPointer ) 
{
    // This virtual function is called from doExitCallback.
    // This function is normally overriden by a derived class.
}

void CheckpointUI::doOpt_append ( Widget, XtPointer ) 
{
    // This virtual function is called from doOpt_appendCallback.
    // This function is normally overriden by a derived class.
}

void CheckpointUI::doOpt_ash ( Widget, XtPointer ) 
{
    // This virtual function is called from doOpt_ashCallback.
    // This function is normally overriden by a derived class.
}

void CheckpointUI::doOpt_gid ( Widget, XtPointer ) 
{
    // This virtual function is called from doOpt_gidCallback.
    // This function is normally overriden by a derived class.
}

void CheckpointUI::doOpt_hid ( Widget, XtPointer ) 
{
    // This virtual function is called from doOpt_hidCallback.
    // This function is normally overriden by a derived class.
}

void CheckpointUI::doOpt_ignore ( Widget, XtPointer ) 
{
    // This virtual function is called from doOpt_ignoreCallback.
    // This function is normally overriden by a derived class.
}

void CheckpointUI::doOpt_merge ( Widget, XtPointer ) 
{
    // This virtual function is called from doOpt_mergeCallback.
    // This function is normally overriden by a derived class.
}

void CheckpointUI::doOpt_pid ( Widget, XtPointer ) 
{
    // This virtual function is called from doOpt_pidCallback.
    // This function is normally overriden by a derived class.
}

void CheckpointUI::doOpt_replace ( Widget, XtPointer ) 
{
    // This virtual function is called from doOpt_replaceCallback.
    // This function is normally overriden by a derived class.
}

void CheckpointUI::doOpt_sgp ( Widget, XtPointer ) 
{
    // This virtual function is called from doOpt_sgpCallback.
    // This function is normally overriden by a derived class.
}

void CheckpointUI::doOpt_sid ( Widget, XtPointer ) 
{
    // This virtual function is called from doOpt_sidCallback.
    // This function is normally overriden by a derived class.
}

void CheckpointUI::doOpt_substitute ( Widget, XtPointer ) 
{
    // This virtual function is called from doOpt_substituteCallback.
    // This function is normally overriden by a derived class.
}

//---- Start editable code block: End of generated code


//---- End editable code block: End of generated code
