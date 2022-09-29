
/////////////////////////////////////////////////////////////
//
// Source file for BulletinBoardUI
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


#include "BulletinBoardUI.h" // Generated header file for this class

#include <Xm/BulletinB.h> 
#include <Xm/LabelG.h> 
#include <Xm/List.h> 
#include <Xm/RowColumn.h> 
#include <Xm/Scale.h> 
#include <Xm/ScrolledW.h> 
#include <Xm/TextF.h> 
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

String  BulletinBoardUI::_defaultBulletinBoardUIResources[] = {
        "*dampTogOff.labelString:  ",
        "*dampTogOn.labelString:  ",
        "*freezeTogOff.labelString:  ",
        "*freezeTogOn.labelString:  ",
        "*label.labelString:  On",
        "*label1.labelString:  Off",
        "*label10.labelString:  Melting:",
        "*label11.labelString:  Threshold:",
        "*label12.labelString:  Threshold:",
        "*label13.labelString:  Factor:",
        "*label14.labelString:  Threshold:",
        "*label15.labelString:  Threshold:",
        "*label16.labelString:  MaxAbsThreshold:",
        "*label17.labelString:  UpdateAbsThreshold:",
        "*label18.labelString:  MinDistance:",
        "*label19.labelString:  MemoryLowThreshold:",
        "*label2.labelString:  On",
        "*label20.labelString:  Node",
        "*label21.labelString:  Interval:",
        "*label22.labelString:  Vehicle:",
        "*label3.labelString:  Off",
        "*label4.labelString:  Norm.",
        "*label5.labelString:  Migration:",
        "*label6.labelString:  Refcnt:",
        "*label7.labelString:  Dampening:",
        "*label8.labelString:  Unpegging:",
        "*label9.labelString:  Freezing:",
        "*meltTogOff.labelString:  ",
        "*meltTogOn.labelString:  ",
        "*migrTogNormOff.labelString:  ",
        "*migrTogNormOn.labelString:  ",
        "*migrTogOff.labelString:  ",
        "*migrTogOn.labelString:  ",
        "*optionA.labelString:  BTE",
        "*optionB.labelString:  TLBsync",
        "*refcntTogNormOff.labelString:  ",
        "*refcntTogNormOn.labelString:  ",
        "*refcntTogOff.labelString:  ",
        "*refcntTogOn.labelString:  ",
        "*unpegTogOff.labelString:  ",
        "*unpegTogOn.labelString:  ",

        //---- Start editable code block: BulletinBoardUI Default Resources


        //---- End editable code block: BulletinBoardUI Default Resources

        (char*)NULL
};

BulletinBoardUI::BulletinBoardUI ( const char *name ) : VkComponent ( name ) 
{ 
    // No widgets are created by this constructor.
    // If an application creates a component using this constructor,
    // It must explictly call create at a later time.
    // This is mostly useful when adding pre-widget creation
    // code to a derived class constructor.

    //---- Start editable code block: BulletinBoard constructor 2


    //---- End editable code block: BulletinBoard constructor 2


}    // End Constructor




BulletinBoardUI::BulletinBoardUI ( const char *name, Widget parent ) : VkComponent ( name ) 
{ 
    //---- Start editable code block: BulletinBoard pre-create


    //---- End editable code block: BulletinBoard pre-create



    // Call creation function to build the widget tree.

     create ( parent );

    //---- Start editable code block: BulletinBoard constructor


    //---- End editable code block: BulletinBoard constructor


}    // End Constructor


BulletinBoardUI::~BulletinBoardUI() 
{
    // Base class destroys widgets

    //---- Start editable code block: BulletinBoardUI destructor


    //---- End editable code block: BulletinBoardUI destructor
}    // End destructor



void BulletinBoardUI::create ( Widget parent )
{
    Arg      args[9];
    Cardinal count;
    count = 0;

    // Load any class-defaulted resources for this object

    setDefaultResources ( parent, _defaultBulletinBoardUIResources  );


    // Create an unmanaged widget as the top of the widget hierarchy

    _baseWidget = _bulletinBoard = XtVaCreateWidget ( _name,
                                                      xmBulletinBoardWidgetClass,
                                                      parent, 
                                                      XmNresizePolicy, XmRESIZE_GROW, 
                                                      (XtPointer) NULL ); 

    // install a callback to guard against unexpected widget destruction

    installDestroyHandler();


    // Create widgets used in this component
    // All variables are data members of this class

    _meltThreshField = XtVaCreateManagedWidget  ( "meltThreshField",
                                                   xmTextFieldWidgetClass,
                                                   _baseWidget, 
                                                   XmNcolumns, 3, 
                                                   XmNx, 640, 
                                                   XmNy, 190, 
                                                   XmNheight, 35, 
                                                   (XtPointer) NULL ); 

    XtAddCallback ( _meltThreshField,
                    XmNactivateCallback,
                    &BulletinBoardUI::meltThreshFieldChangedCallback,
                    (XtPointer) this ); 


    _freezeThreshField = XtVaCreateManagedWidget  ( "freezeThreshField",
                                                     xmTextFieldWidgetClass,
                                                     _baseWidget, 
                                                     XmNcolumns, 3, 
                                                     XmNx, 640, 
                                                     XmNy, 160, 
                                                     XmNheight, 35, 
                                                     (XtPointer) NULL ); 

    XtAddCallback ( _freezeThreshField,
                    XmNactivateCallback,
                    &BulletinBoardUI::freezeThreshFieldChangedCallback,
                    (XtPointer) this ); 


    _unpegThreshField = XtVaCreateManagedWidget  ( "unpegThreshField",
                                                    xmTextFieldWidgetClass,
                                                    _baseWidget, 
                                                    XmNcolumns, 3, 
                                                    XmNx, 640, 
                                                    XmNy, 130, 
                                                    XmNheight, 35, 
                                                    (XtPointer) NULL ); 

    XtAddCallback ( _unpegThreshField,
                    XmNactivateCallback,
                    &BulletinBoardUI::unpegThreshFieldChangedCallback,
                    (XtPointer) this ); 


    _dampFactorField = XtVaCreateManagedWidget  ( "dampFactorField",
                                                   xmTextFieldWidgetClass,
                                                   _baseWidget, 
                                                   XmNcolumns, 3, 
                                                   XmNx, 640, 
                                                   XmNy, 100, 
                                                   XmNheight, 35, 
                                                   (XtPointer) NULL ); 

    XtAddCallback ( _dampFactorField,
                    XmNactivateCallback,
                    &BulletinBoardUI::dampFactorFieldChangedCallback,
                    (XtPointer) this ); 


    _refcntUAThreshField = XtVaCreateManagedWidget  ( "refcntUAThreshField",
                                                        xmTextFieldWidgetClass,
                                                        _baseWidget, 
                                                        XmNcolumns, 4, 
                                                        XmNx, 632, 
                                                        XmNy, 70, 
                                                        XmNheight, 32, 
                                                        (XtPointer) NULL ); 

    XtAddCallback ( _refcntUAThreshField,
                    XmNactivateCallback,
                    &BulletinBoardUI::refcntUAThreshFieldChangedCallback,
                    (XtPointer) this ); 


    _label22 = XtVaCreateManagedWidget  ( "label22",
                                           xmLabelGadgetClass,
                                           _baseWidget, 
                                           XmNx, 1050, 
                                           XmNy, 50, 
                                           XmNwidth, 60, 
                                           XmNheight, 20, 
                                           (XtPointer) NULL ); 


    _optionMenu = new VkOptionMenu ( _baseWidget, "optionMenu");
    _optionA =  _optionMenu->addAction ( "optionA", 
                                         &BulletinBoardUI::VehicleBTECallback, 
                                         (XtPointer) this );

    _optionB =  _optionMenu->addAction ( "optionB", 
                                         &BulletinBoardUI::VehicleTLBsyncCallback, 
                                         (XtPointer) this );

    _unpegIntervalField = XtVaCreateManagedWidget  ( "unpegIntervalField",
                                                        xmTextFieldWidgetClass,
                                                        _baseWidget, 
                                                        XmNcolumns, 4, 
                                                        XmNx, 760, 
                                                        XmNy, 130, 
                                                        XmNheight, 35, 
                                                        (XtPointer) NULL ); 

    XtAddCallback ( _unpegIntervalField,
                    XmNactivateCallback,
                    &BulletinBoardUI::unpegIntervalFieldChangedCallback,
                    (XtPointer) this ); 


    _label21 = XtVaCreateManagedWidget  ( "label21",
                                           xmLabelGadgetClass,
                                           _baseWidget, 
                                           XmNx, 700, 
                                           XmNy, 140, 
                                           XmNwidth, 61, 
                                           XmNheight, 20, 
                                           (XtPointer) NULL ); 


    _label20 = XtVaCreateManagedWidget  ( "label20",
                                           xmLabelGadgetClass,
                                           _baseWidget, 
                                           XmNx, 90, 
                                           XmNy, 10, 
                                           XmNwidth, 41, 
                                           XmNheight, 20, 
                                           (XtPointer) NULL ); 


    _dampMLThreshField = XtVaCreateManagedWidget  ( "dampMLThreshField",
                                              xmTextFieldWidgetClass,
                                              _baseWidget, 
                                              XmNcolumns, 3, 
                                              XmNx, 960, 
                                              XmNy, 100, 
                                              XmNheight, 35, 
                                              (XtPointer) NULL ); 

    XtAddCallback ( _dampMLThreshField,
                    XmNactivateCallback,
                    &BulletinBoardUI::dampMLThreshFieldChangedCallback,
                    (XtPointer) this ); 


    _dampMLThreshScale = XtVaCreateManagedWidget  ( "dampMLThreshScale",
                                          xmScaleWidgetClass,
                                          _baseWidget, 
                                          XmNorientation, XmHORIZONTAL, 
                                          XmNshowValue, False, 
                                          XmNx, 860, 
                                          XmNy, 110, 
                                          XmNwidth, 100, 
                                          XmNheight, 15, 
                                          (XtPointer) NULL ); 

    XtAddCallback ( _dampMLThreshScale,
                    XmNvalueChangedCallback,
                    &BulletinBoardUI::dampMLThreshScaleChangedCallback,
                    (XtPointer) this ); 

    XtAddCallback ( _dampMLThreshScale,
                    XmNdragCallback,
                    &BulletinBoardUI::dampMLThreshScaleDynamicCallback,
                    (XtPointer) this ); 

    _label19 = XtVaCreateManagedWidget  ( "label19",
                                           xmLabelGadgetClass,
                                           _baseWidget, 
                                           XmNx, 700, 
                                           XmNy, 110, 
                                           XmNwidth, 162, 
                                           XmNheight, 20, 
                                           (XtPointer) NULL ); 


    _migrMinDisField = XtVaCreateManagedWidget  ( "migrMinDisField",
                                              xmTextFieldWidgetClass,
                                              _baseWidget, 
                                              XmNcolumns, 2, 
                                                   XmNx, 1000, 
                                              XmNy, 40, 
                                              XmNheight, 35, 
                                              (XtPointer) NULL ); 

    XtAddCallback ( _migrMinDisField,
                    XmNactivateCallback,
                    &BulletinBoardUI::migrMinDisFieldChangedCallback,
                    (XtPointer) this ); 


    _label18 = XtVaCreateManagedWidget  ( "label18",
                                           xmLabelGadgetClass,
                                           _baseWidget, 
                                           XmNx, 900, 
                                           XmNy, 50, 
                                           XmNwidth, 96, 
                                           XmNheight, 20, 
                                           (XtPointer) NULL ); 


    _migrMAThreshField = XtVaCreateManagedWidget  ( "migrMAThreshField",
                                             xmTextFieldWidgetClass,
                                             _baseWidget, 
                                             XmNcolumns, 4, 
                                                     XmNx, 840, 
                                             XmNy, 40, 
                                             XmNheight, 35, 
                                             (XtPointer) NULL ); 

    XtAddCallback ( _migrMAThreshField,
                    XmNactivateCallback,
                    &BulletinBoardUI::migrMAThreshFieldChangedCallback,
                    (XtPointer) this ); 


    _label17 = XtVaCreateManagedWidget  ( "label17",
                                           xmLabelGadgetClass,
                                           _baseWidget, 
                                           XmNx, 460, 
                                           XmNy, 80, 
                                           XmNwidth, 156, 
                                           XmNheight, 20, 
                                           (XtPointer) NULL ); 


    _label16 = XtVaCreateManagedWidget  ( "label16",
                                           xmLabelGadgetClass,
                                           _baseWidget, 
                                           XmNx, 700, 
                                           XmNy, 50, 
                                           XmNwidth, 134, 
                                           XmNheight, 20, 
                                           (XtPointer) NULL ); 


    _meltThreshScale = XtVaCreateManagedWidget  ( "meltThreshScale",
                                          xmScaleWidgetClass,
                                          _baseWidget, 
                                          XmNorientation, XmHORIZONTAL, 
                                          XmNshowValue, False, 
                                          XmNx, 540, 
                                          XmNy, 200, 
                                          XmNwidth, 100, 
                                          XmNheight, 15, 
                                          (XtPointer) NULL ); 

    XtAddCallback ( _meltThreshScale,
                    XmNvalueChangedCallback,
                    &BulletinBoardUI::meltThreshScaleChangedCallback,
                    (XtPointer) this ); 

    XtAddCallback ( _meltThreshScale,
                    XmNdragCallback,
                    &BulletinBoardUI::meltThreshScaleDynamicCallback,
                    (XtPointer) this ); 


    _migrThreshField = XtVaCreateManagedWidget  ( "migrThreshField",
                                             xmTextFieldWidgetClass,
                                             _baseWidget, 
                                             XmNcolumns, 3, 
                                             XmNx, 640, 
                                             XmNy, 40, 
                                             XmNheight, 35, 
                                             (XtPointer) NULL ); 

    XtAddCallback ( _migrThreshField,
                    XmNactivateCallback,
                    &BulletinBoardUI::migrThreshFieldChangedCallback,
                    (XtPointer) this ); 


    _freezeThreshScale = XtVaCreateManagedWidget  ( "freezeThreshScale",
                                          xmScaleWidgetClass,
                                          _baseWidget, 
                                          XmNorientation, XmHORIZONTAL, 
                                          XmNshowValue, False, 
                                          XmNx, 540, 
                                          XmNy, 170, 
                                          XmNwidth, 100, 
                                          XmNheight, 15, 
                                          (XtPointer) NULL ); 

    XtAddCallback ( _freezeThreshScale,
                    XmNvalueChangedCallback,
                    &BulletinBoardUI::freezeThreshScaleChangedCallback,
                    (XtPointer) this ); 

    XtAddCallback ( _freezeThreshScale,
                    XmNdragCallback,
                    &BulletinBoardUI::freezeThreshScaleDynamicCallback,
                    (XtPointer) this ); 


    _unpegThreshScale = XtVaCreateManagedWidget  ( "unpegThreshScale",
                                          xmScaleWidgetClass,
                                          _baseWidget, 
                                          XmNorientation, XmHORIZONTAL, 
                                          XmNshowValue, False, 
                                          XmNx, 540, 
                                          XmNy, 140, 
                                          XmNwidth, 100, 
                                          XmNheight, 15, 
                                          (XtPointer) NULL ); 

    XtAddCallback ( _unpegThreshScale,
                    XmNvalueChangedCallback,
                    &BulletinBoardUI::unpegThreshScaleChangedCallback,
                    (XtPointer) this ); 

    XtAddCallback ( _unpegThreshScale,
                    XmNdragCallback,
                    &BulletinBoardUI::unpegThreshScaleDynamicCallback,
                    (XtPointer) this ); 


    _dampFactorScale = XtVaCreateManagedWidget  ( "dampFactorScale",
                                          xmScaleWidgetClass,
                                          _baseWidget, 
                                          XmNorientation, XmHORIZONTAL, 
                                          XmNshowValue, False, 
                                          XmNx, 540, 
                                          XmNy, 110, 
                                          XmNwidth, 100, 
                                          XmNheight, 15, 
                                          (XtPointer) NULL ); 

    XtAddCallback ( _dampFactorScale,
                    XmNvalueChangedCallback,
                    &BulletinBoardUI::dampFactorScaleChangedCallback,
                    (XtPointer) this ); 

    XtAddCallback ( _dampFactorScale,
                    XmNdragCallback,
                    &BulletinBoardUI::dampFactorScaleDynamicCallback,
                    (XtPointer) this ); 


    _migrThreshScale = XtVaCreateManagedWidget  ( "migrThreshScale",
                                          xmScaleWidgetClass,
                                          _baseWidget, 
                                          SgNslanted, False, 
                                          XmNorientation, XmHORIZONTAL, 
                                          XmNshowValue, False, 
                                          XmNx, 540, 
                                          XmNy, 50, 
                                          XmNwidth, 100, 
                                          XmNheight, 15, 
                                          (XtPointer) NULL ); 

    XtAddCallback ( _migrThreshScale,
                    XmNvalueChangedCallback,
                    &BulletinBoardUI::migrThreshScaleChangedCallback,
                    (XtPointer) this ); 

    XtAddCallback ( _migrThreshScale,
                    XmNdragCallback,
                    &BulletinBoardUI::migrThreshScaleDynamicCallback,
                    (XtPointer) this ); 


    _label15 = XtVaCreateManagedWidget  ( "label15",
                                           xmLabelGadgetClass,
                                           _baseWidget, 
                                           XmNx, 460, 
                                           XmNy, 170, 
                                           XmNwidth, 79, 
                                           XmNheight, 20, 
                                           (XtPointer) NULL ); 


    _label14 = XtVaCreateManagedWidget  ( "label14",
                                           xmLabelGadgetClass,
                                           _baseWidget, 
                                           XmNx, 460, 
                                           XmNy, 140, 
                                           XmNwidth, 79, 
                                           XmNheight, 20, 
                                           (XtPointer) NULL ); 


    _label13 = XtVaCreateManagedWidget  ( "label13",
                                           xmLabelGadgetClass,
                                           _baseWidget, 
                                           XmNx, 460, 
                                           XmNy, 110, 
                                           XmNwidth, 54, 
                                           XmNheight, 20, 
                                           (XtPointer) NULL ); 


    _label12 = XtVaCreateManagedWidget  ( "label12",
                                           xmLabelGadgetClass,
                                           _baseWidget, 
                                           XmNx, 460, 
                                           XmNy, 200, 
                                           XmNwidth, 80, 
                                           XmNheight, 20, 
                                           (XtPointer) NULL ); 


    _label11 = XtVaCreateManagedWidget  ( "label11",
                                           xmLabelGadgetClass,
                                           _baseWidget, 
                                           XmNx, 460, 
                                           XmNy, 50, 
                                           XmNwidth, 79, 
                                           XmNheight, 20, 
                                           (XtPointer) NULL ); 


    _radiobox5 = XtVaCreateManagedWidget  ( "radiobox5",
                                             xmRowColumnWidgetClass,
                                             _baseWidget, 
                                             XmNorientation, XmHORIZONTAL, 
                                             XmNpacking, XmPACK_COLUMN, 
                                             XmNradioBehavior, True, 
                                             XmNradioAlwaysOne, True, 
                                             XmNx, 330, 
                                             XmNy, 200, 
                                             XmNwidth, 63, 
                                             XmNheight, 32, 
                                             (XtPointer) NULL ); 


    _meltTogOn = XtVaCreateManagedWidget  ( "meltTogOn",
                                             xmToggleButtonWidgetClass,
                                             _radiobox5, 
                                             XmNlabelType, XmSTRING, 
                                             (XtPointer) NULL ); 

    XtAddCallback ( _meltTogOn,
                    XmNvalueChangedCallback,
                    &BulletinBoardUI::meltSetOnCallback,
                    (XtPointer) this ); 


    _meltTogOff = XtVaCreateManagedWidget  ( "meltTogOff",
                                              xmToggleButtonWidgetClass,
                                              _radiobox5, 
                                              XmNlabelType, XmSTRING, 
                                              (XtPointer) NULL ); 

    XtAddCallback ( _meltTogOff,
                    XmNvalueChangedCallback,
                    &BulletinBoardUI::meltSetOffCallback,
                    (XtPointer) this ); 


    _label10 = XtVaCreateManagedWidget  ( "label10",
                                           xmLabelGadgetClass,
                                           _baseWidget, 
                                           XmNx, 240, 
                                           XmNy, 200, 
                                           XmNwidth, 61, 
                                           XmNheight, 20, 
                                           (XtPointer) NULL ); 


    _radiobox4 = XtVaCreateManagedWidget  ( "radiobox4",
                                             xmRowColumnWidgetClass,
                                             _baseWidget, 
                                             XmNorientation, XmHORIZONTAL, 
                                             XmNpacking, XmPACK_COLUMN, 
                                             XmNradioBehavior, True, 
                                             XmNradioAlwaysOne, True, 
                                             XmNx, 330, 
                                             XmNy, 170, 
                                             XmNwidth, 63, 
                                             XmNheight, 32, 
                                             (XtPointer) NULL ); 


    _freezeTogOn = XtVaCreateManagedWidget  ( "freezeTogOn",
                                               xmToggleButtonWidgetClass,
                                               _radiobox4, 
                                               XmNlabelType, XmSTRING, 
                                               (XtPointer) NULL ); 

    XtAddCallback ( _freezeTogOn,
                    XmNvalueChangedCallback,
                    &BulletinBoardUI::freezeSetOnCallback,
                    (XtPointer) this ); 


    _freezeTogOff = XtVaCreateManagedWidget  ( "freezeTogOff",
                                                xmToggleButtonWidgetClass,
                                                _radiobox4, 
                                                XmNlabelType, XmSTRING, 
                                                (XtPointer) NULL ); 

    XtAddCallback ( _freezeTogOff,
                    XmNvalueChangedCallback,
                    &BulletinBoardUI::freezeSetOffCallback,
                    (XtPointer) this ); 


    _label9 = XtVaCreateManagedWidget  ( "label9",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 240, 
                                          XmNy, 170, 
                                          XmNwidth, 68, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _label8 = XtVaCreateManagedWidget  ( "label8",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 240, 
                                          XmNy, 140, 
                                          XmNwidth, 86, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _label7 = XtVaCreateManagedWidget  ( "label7",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 240, 
                                          XmNy, 110, 
                                          XmNwidth, 88, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _label6 = XtVaCreateManagedWidget  ( "label6",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 240, 
                                          XmNy, 80, 
                                          XmNwidth, 54, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _label5 = XtVaCreateManagedWidget  ( "label5",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 240, 
                                          XmNy, 50, 
                                          XmNwidth, 76, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _label4 = XtVaCreateManagedWidget  ( "label4",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 395, 
                                          XmNy, 10, 
                                          XmNwidth, 46, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _label3 = XtVaCreateManagedWidget  ( "label3",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 420, 
                                          XmNy, 30, 
                                          XmNwidth, 24, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _label2 = XtVaCreateManagedWidget  ( "label2",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 390, 
                                          XmNy, 30, 
                                          XmNwidth, 25, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _label1 = XtVaCreateManagedWidget  ( "label1",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 360, 
                                          XmNy, 30, 
                                          XmNwidth, 24, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _label = XtVaCreateManagedWidget  ( "label",
                                         xmLabelGadgetClass,
                                         _baseWidget, 
                                         XmNx, 330, 
                                         XmNy, 30, 
                                         XmNwidth, 25, 
                                         XmNheight, 20, 
                                         (XtPointer) NULL ); 


    _radiobox3 = XtVaCreateManagedWidget  ( "radiobox3",
                                            xmRowColumnWidgetClass,
                                            _baseWidget, 
                                            XmNorientation, XmHORIZONTAL, 
                                            XmNpacking, XmPACK_COLUMN, 
                                             XmNradioBehavior, True, 
                                             XmNradioAlwaysOne, True, 
                                             XmNx, 330, 
                                             XmNy, 140, 
                                             XmNwidth, 63, 
                                             XmNheight, 32, 
                                             (XtPointer) NULL ); 


    _unpegTogOn = XtVaCreateManagedWidget  ( "unpegTogOn",
                                            xmToggleButtonWidgetClass,
                                            _radiobox3, 
                                            XmNlabelType, XmSTRING, 
                                            (XtPointer) NULL ); 

    XtAddCallback ( _unpegTogOn,
                    XmNvalueChangedCallback,
                    &BulletinBoardUI::unpegSetOnCallback,
                    (XtPointer) this ); 


    _unpegTogOff = XtVaCreateManagedWidget  ( "unpegTogOff",
                                            xmToggleButtonWidgetClass,
                                            _radiobox3, 
                                            XmNlabelType, XmSTRING, 
                                            (XtPointer) NULL ); 

    XtAddCallback ( _unpegTogOff,
                    XmNvalueChangedCallback,
                    &BulletinBoardUI::unpegSetOffCallback,
                    (XtPointer) this ); 


    _radiobox2 = XtVaCreateManagedWidget  ( "radiobox2",
                                             xmRowColumnWidgetClass,
                                             _baseWidget, 
                                             XmNorientation, XmHORIZONTAL, 
                                             XmNpacking, XmPACK_COLUMN, 
                                             XmNradioBehavior, True, 
                                             XmNradioAlwaysOne, True, 
                                             XmNx, 330, 
                                             XmNy, 110, 
                                             XmNwidth, 63, 
                                             XmNheight, 32, 
                                             (XtPointer) NULL ); 


    _dampTogOn = XtVaCreateManagedWidget  ( "dampTogOn",
                                            xmToggleButtonWidgetClass,
                                            _radiobox2, 
                                            XmNlabelType, XmSTRING, 
                                            (XtPointer) NULL ); 

    XtAddCallback ( _dampTogOn,
                    XmNvalueChangedCallback,
                    &BulletinBoardUI::dampSetOnCallback,
                    (XtPointer) this ); 


    _dampTogOff = XtVaCreateManagedWidget  ( "dampTogOff",
                                            xmToggleButtonWidgetClass,
                                            _radiobox2, 
                                            XmNlabelType, XmSTRING, 
                                            (XtPointer) NULL ); 

    XtAddCallback ( _dampTogOff,
                    XmNvalueChangedCallback,
                    &BulletinBoardUI::dampSetOffCallback,
                    (XtPointer) this ); 


    _radiobox1 = XtVaCreateManagedWidget  ( "radiobox1",
                                             xmRowColumnWidgetClass,
                                             _baseWidget, 
                                             XmNorientation, XmHORIZONTAL, 
                                             XmNpacking, XmPACK_COLUMN, 
                                            XmNradioBehavior, True, 
                                            XmNradioAlwaysOne, True, 
                                             XmNx, 330, 
                                             XmNy, 80, 
                                            XmNwidth, 123, 
                                            XmNheight, 32, 
                                            (XtPointer) NULL ); 


    _refcntTogOn = XtVaCreateManagedWidget  ( "refcntTogOn",
                                           xmToggleButtonWidgetClass,
                                           _radiobox1, 
                                           XmNlabelType, XmSTRING, 
                                           (XtPointer) NULL ); 

    XtAddCallback ( _refcntTogOn,
                    XmNvalueChangedCallback,
                    &BulletinBoardUI::refcntSetOnCallback,
                    (XtPointer) this ); 


    _refcntTogOff = XtVaCreateManagedWidget  ( "refcntTogOff",
                                           xmToggleButtonWidgetClass,
                                           _radiobox1, 
                                                XmNshadowThickness, 0, 
                                           XmNlabelType, XmSTRING, 
                                                XmNset, False, 
                                           (XtPointer) NULL ); 

    XtAddCallback ( _refcntTogOff,
                    XmNvalueChangedCallback,
                    &BulletinBoardUI::refcntSetOffCallback,
                    (XtPointer) this ); 


    _refcntTogNormOn = XtVaCreateManagedWidget  ( "refcntTogNormOn",
                                           xmToggleButtonWidgetClass,
                                           _radiobox1, 
                                           XmNlabelType, XmSTRING, 
                                           (XtPointer) NULL ); 

    XtAddCallback ( _refcntTogNormOn,
                    XmNvalueChangedCallback,
                    &BulletinBoardUI::refcntSetNormOnCallback,
                    (XtPointer) this ); 


    _refcntTogNormOff = XtVaCreateManagedWidget  ( "refcntTogNormOff",
                                           xmToggleButtonWidgetClass,
                                           _radiobox1, 
                                           XmNlabelType, XmSTRING, 
                                           (XtPointer) NULL ); 

    XtAddCallback ( _refcntTogNormOff,
                    XmNvalueChangedCallback,
                    &BulletinBoardUI::refcntSetNormOffCallback,
                    (XtPointer) this ); 


    _radiobox = XtVaCreateManagedWidget  ( "radiobox",
                                            xmRowColumnWidgetClass,
                                            _baseWidget, 
                                            XmNorientation, XmHORIZONTAL, 
                                            XmNpacking, XmPACK_COLUMN, 
                                            XmNnumColumns, 1, 
                                            XmNradioBehavior, True, 
                                            XmNradioAlwaysOne, True, 
                                            XmNx, 330, 
                                            XmNy, 50, 
                                            XmNwidth, 123, 
                                            XmNheight, 32, 
                                            (XtPointer) NULL ); 


    _migrTogOn = XtVaCreateManagedWidget  ( "migrTogOn",
                                           xmToggleButtonWidgetClass,
                                           _radiobox, 
                                           XmNlabelType, XmSTRING, 
                                           (XtPointer) NULL ); 

    XtAddCallback ( _migrTogOn,
                    XmNvalueChangedCallback,
                    &BulletinBoardUI::migrSetOnCallback,
                    (XtPointer) this ); 


    _migrTogOff = XtVaCreateManagedWidget  ( "migrTogOff",
                                           xmToggleButtonWidgetClass,
                                           _radiobox, 
                                           XmNlabelType, XmSTRING, 
                                           (XtPointer) NULL ); 

    XtAddCallback ( _migrTogOff,
                    XmNvalueChangedCallback,
                    &BulletinBoardUI::migrSetOffCallback,
                    (XtPointer) this ); 


    _migrTogNormOn = XtVaCreateManagedWidget  ( "migrTogNormOn",
                                           xmToggleButtonWidgetClass,
                                           _radiobox, 
                                           XmNlabelType, XmSTRING, 
                                           (XtPointer) NULL ); 

    XtAddCallback ( _migrTogNormOn,
                    XmNvalueChangedCallback,
                    &BulletinBoardUI::migrSetNormOnCallback,
                    (XtPointer) this ); 


    _migrTogNormOff = XtVaCreateManagedWidget  ( "migrTogNormOff",
                                          xmToggleButtonWidgetClass,
                                           _radiobox, 
                                           XmNlabelType, XmSTRING, 
                                          (XtPointer) NULL ); 

    XtAddCallback ( _migrTogNormOff,
                    XmNvalueChangedCallback,
                    &BulletinBoardUI::migrSetNormOffCallback,
                    (XtPointer) this ); 


    _scrolledWindow = XtVaCreateManagedWidget  ( "scrolledWindow",
                                                  xmScrolledWindowWidgetClass,
                                          _baseWidget, 
                                                  XmNscrollBarDisplayPolicy, XmSTATIC, 
                                                  XmNx, 10, 
                                                  XmNy, 40, 
                                                  XmNwidth, 220, 
                                                  XmNheight, 200, 
                                          (XtPointer) NULL ); 


    _scrolledList = XtVaCreateManagedWidget  ( "scrolledList",
                                                xmListWidgetClass,
                                                _scrolledWindow, 
                                                XmNselectionPolicy, XmMULTIPLE_SELECT, 
                                                XmNwidth, 214, 
                                                XmNheight, 194, 
                                                (XtPointer) NULL ); 

    XtAddCallback ( _scrolledList,
                    XmNmultipleSelectionCallback,
                    &BulletinBoardUI::nodeSelectCallback,
                    (XtPointer) this ); 


    XtVaSetValues ( _optionMenu->baseWidget(),
                    XmNx, 1110, 
                    XmNy, 40, 
                    XmNwidth, 119, 
                    XmNheight, 32, 
                    (XtPointer) NULL );

    //---- Start editable code block: BulletinBoardUI create


    //---- End editable code block: BulletinBoardUI create
}

const char * BulletinBoardUI::className()
{
    return ("BulletinBoardUI");
}    // End className()




/////////////////////////////////////////////////////////////// 
// The following functions are static member functions used to 
// interface with Motif.
/////////////////////////////////// 

void BulletinBoardUI::VehicleBTECallback ( Widget    w,
                                           XtPointer clientData,
                                           XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->VehicleBTE ( w, callData );
}

void BulletinBoardUI::VehicleTLBsyncCallback ( Widget    w,
                                               XtPointer clientData,
                                               XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->VehicleTLBsync ( w, callData );
}

void BulletinBoardUI::dampFactorFieldChangedCallback ( Widget    w,
                                                       XtPointer clientData,
                                                       XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->dampFactorFieldChanged ( w, callData );
}

void BulletinBoardUI::dampFactorScaleChangedCallback ( Widget    w,
                                                       XtPointer clientData,
                                                       XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->dampFactorScaleChanged ( w, callData );
}

void BulletinBoardUI::dampFactorScaleDynamicCallback ( Widget    w,
                                                       XtPointer clientData,
                                                       XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->dampFactorScaleDynamic ( w, callData );
}

void BulletinBoardUI::dampMLThreshFieldChangedCallback ( Widget    w,
                                                         XtPointer clientData,
                                                         XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->dampMLThreshFieldChanged ( w, callData );
}

void BulletinBoardUI::dampMLThreshScaleChangedCallback ( Widget    w,
                                                         XtPointer clientData,
                                                         XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->dampMLThreshScaleChanged ( w, callData );
}

void BulletinBoardUI::dampMLThreshScaleDynamicCallback ( Widget    w,
                                                         XtPointer clientData,
                                                         XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->dampMLThreshScaleDynamic ( w, callData );
}

void BulletinBoardUI::dampSetOffCallback ( Widget    w,
                                           XtPointer clientData,
                                           XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->dampSetOff ( w, callData );
}

void BulletinBoardUI::dampSetOnCallback ( Widget    w,
                                          XtPointer clientData,
                                          XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->dampSetOn ( w, callData );
}

void BulletinBoardUI::freezeSetOffCallback ( Widget    w,
                                             XtPointer clientData,
                                             XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->freezeSetOff ( w, callData );
}

void BulletinBoardUI::freezeSetOnCallback ( Widget    w,
                                            XtPointer clientData,
                                            XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->freezeSetOn ( w, callData );
}

void BulletinBoardUI::freezeThreshFieldChangedCallback ( Widget    w,
                                                         XtPointer clientData,
                                                         XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->freezeThreshFieldChanged ( w, callData );
}

void BulletinBoardUI::freezeThreshScaleChangedCallback ( Widget    w,
                                                         XtPointer clientData,
                                                         XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->freezeThreshScaleChanged ( w, callData );
}

void BulletinBoardUI::freezeThreshScaleDynamicCallback ( Widget    w,
                                                         XtPointer clientData,
                                                         XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->freezeThreshScaleDynamic ( w, callData );
}

void BulletinBoardUI::meltSetOffCallback ( Widget    w,
                                           XtPointer clientData,
                                           XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->meltSetOff ( w, callData );
}

void BulletinBoardUI::meltSetOnCallback ( Widget    w,
                                          XtPointer clientData,
                                          XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->meltSetOn ( w, callData );
}

void BulletinBoardUI::meltThreshFieldChangedCallback ( Widget    w,
                                                       XtPointer clientData,
                                                       XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->meltThreshFieldChanged ( w, callData );
}

void BulletinBoardUI::meltThreshScaleChangedCallback ( Widget    w,
                                                       XtPointer clientData,
                                                       XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->meltThreshScaleChanged ( w, callData );
}

void BulletinBoardUI::meltThreshScaleDynamicCallback ( Widget    w,
                                                       XtPointer clientData,
                                                       XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->meltThreshScaleDynamic ( w, callData );
}

void BulletinBoardUI::migrMAThreshFieldChangedCallback ( Widget    w,
                                                         XtPointer clientData,
                                                         XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->migrMAThreshFieldChanged ( w, callData );
}

void BulletinBoardUI::migrMinDisFieldChangedCallback ( Widget    w,
                                                       XtPointer clientData,
                                                       XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->migrMinDisFieldChanged ( w, callData );
}

void BulletinBoardUI::migrSetNormOffCallback ( Widget    w,
                                               XtPointer clientData,
                                               XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->migrSetNormOff ( w, callData );
}

void BulletinBoardUI::migrSetNormOnCallback ( Widget    w,
                                              XtPointer clientData,
                                              XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->migrSetNormOn ( w, callData );
}

void BulletinBoardUI::migrSetOffCallback ( Widget    w,
                                           XtPointer clientData,
                                           XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->migrSetOff ( w, callData );
}

void BulletinBoardUI::migrSetOnCallback ( Widget    w,
                                          XtPointer clientData,
                                          XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->migrSetOn ( w, callData );
}

void BulletinBoardUI::migrThreshFieldChangedCallback ( Widget    w,
                                                       XtPointer clientData,
                                                       XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->migrThreshFieldChanged ( w, callData );
}

void BulletinBoardUI::migrThreshScaleChangedCallback ( Widget    w,
                                                       XtPointer clientData,
                                                       XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->migrThreshScaleChanged ( w, callData );
}

void BulletinBoardUI::migrThreshScaleDynamicCallback ( Widget    w,
                                                       XtPointer clientData,
                                                       XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->migrThreshScaleDynamic ( w, callData );
}

void BulletinBoardUI::nodeSelectCallback ( Widget    w,
                                           XtPointer clientData,
                                           XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->nodeSelect ( w, callData );
}

void BulletinBoardUI::refcntSetNormOffCallback ( Widget    w,
                                                 XtPointer clientData,
                                                 XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->refcntSetNormOff ( w, callData );
}

void BulletinBoardUI::refcntSetNormOnCallback ( Widget    w,
                                                XtPointer clientData,
                                                XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->refcntSetNormOn ( w, callData );
}

void BulletinBoardUI::refcntSetOffCallback ( Widget    w,
                                             XtPointer clientData,
                                             XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->refcntSetOff ( w, callData );
}

void BulletinBoardUI::refcntSetOnCallback ( Widget    w,
                                            XtPointer clientData,
                                            XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->refcntSetOn ( w, callData );
}

void BulletinBoardUI::refcntUAThreshFieldChangedCallback ( Widget    w,
                                                           XtPointer clientData,
                                                           XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->refcntUAThreshFieldChanged ( w, callData );
}

void BulletinBoardUI::unpegIntervalFieldChangedCallback ( Widget    w,
                                                          XtPointer clientData,
                                                          XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->unpegIntervalFieldChanged ( w, callData );
}

void BulletinBoardUI::unpegSetOffCallback ( Widget    w,
                                            XtPointer clientData,
                                            XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->unpegSetOff ( w, callData );
}

void BulletinBoardUI::unpegSetOnCallback ( Widget    w,
                                           XtPointer clientData,
                                           XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->unpegSetOn ( w, callData );
}

void BulletinBoardUI::unpegThreshFieldChangedCallback ( Widget    w,
                                                        XtPointer clientData,
                                                        XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->unpegThreshFieldChanged ( w, callData );
}

void BulletinBoardUI::unpegThreshScaleChangedCallback ( Widget    w,
                                                        XtPointer clientData,
                                                        XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->unpegThreshScaleChanged ( w, callData );
}

void BulletinBoardUI::unpegThreshScaleDynamicCallback ( Widget    w,
                                                        XtPointer clientData,
                                                        XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->unpegThreshScaleDynamic ( w, callData );
}



/////////////////////////////////////////////////////////////// 
// The following functions are called from the menu items 
// in this window.
/////////////////////////////////// 

void BulletinBoardUI::VehicleBTE ( Widget, XtPointer ) 
{
    // This virtual function is called from VehicleBTECallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::VehicleTLBsync ( Widget, XtPointer ) 
{
    // This virtual function is called from VehicleTLBsyncCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::dampFactorFieldChanged ( Widget, XtPointer ) 
{
    // This virtual function is called from dampFactorFieldChangedCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::dampFactorScaleChanged ( Widget, XtPointer ) 
{
    // This virtual function is called from dampFactorScaleChangedCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::dampFactorScaleDynamic ( Widget, XtPointer ) 
{
    // This virtual function is called from dampFactorScaleDynamicCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::dampMLThreshFieldChanged ( Widget, XtPointer ) 
{
    // This virtual function is called from dampMLThreshFieldChangedCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::dampMLThreshScaleChanged ( Widget, XtPointer ) 
{
    // This virtual function is called from dampMLThreshScaleChangedCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::dampMLThreshScaleDynamic ( Widget, XtPointer ) 
{
    // This virtual function is called from dampMLThreshScaleDynamicCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::dampSetOff ( Widget, XtPointer ) 
{
    // This virtual function is called from dampSetOffCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::dampSetOn ( Widget, XtPointer ) 
{
    // This virtual function is called from dampSetOnCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::freezeSetOff ( Widget, XtPointer ) 
{
    // This virtual function is called from freezeSetOffCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::freezeSetOn ( Widget, XtPointer ) 
{
    // This virtual function is called from freezeSetOnCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::freezeThreshFieldChanged ( Widget, XtPointer ) 
{
    // This virtual function is called from freezeThreshFieldChangedCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::freezeThreshScaleChanged ( Widget, XtPointer ) 
{
    // This virtual function is called from freezeThreshScaleChangedCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::freezeThreshScaleDynamic ( Widget, XtPointer ) 
{
    // This virtual function is called from freezeThreshScaleDynamicCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::meltSetOff ( Widget, XtPointer ) 
{
    // This virtual function is called from meltSetOffCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::meltSetOn ( Widget, XtPointer ) 
{
    // This virtual function is called from meltSetOnCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::meltThreshFieldChanged ( Widget, XtPointer ) 
{
    // This virtual function is called from meltThreshFieldChangedCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::meltThreshScaleChanged ( Widget, XtPointer ) 
{
    // This virtual function is called from meltThreshScaleChangedCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::meltThreshScaleDynamic ( Widget, XtPointer ) 
{
    // This virtual function is called from meltThreshScaleDynamicCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::migrMAThreshFieldChanged ( Widget, XtPointer ) 
{
    // This virtual function is called from migrMAThreshFieldChangedCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::migrMinDisFieldChanged ( Widget, XtPointer ) 
{
    // This virtual function is called from migrMinDisFieldChangedCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::migrSetNormOff ( Widget, XtPointer ) 
{
    // This virtual function is called from migrSetNormOffCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::migrSetNormOn ( Widget, XtPointer ) 
{
    // This virtual function is called from migrSetNormOnCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::migrSetOff ( Widget, XtPointer ) 
{
    // This virtual function is called from migrSetOffCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::migrSetOn ( Widget, XtPointer ) 
{
    // This virtual function is called from migrSetOnCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::migrThreshFieldChanged ( Widget, XtPointer ) 
{
    // This virtual function is called from migrThreshFieldChangedCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::migrThreshScaleChanged ( Widget, XtPointer ) 
{
    // This virtual function is called from migrThreshScaleChangedCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::migrThreshScaleDynamic ( Widget, XtPointer ) 
{
    // This virtual function is called from migrThreshScaleDynamicCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::nodeSelect ( Widget, XtPointer ) 
{
    // This virtual function is called from nodeSelectCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::refcntSetNormOff ( Widget, XtPointer ) 
{
    // This virtual function is called from refcntSetNormOffCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::refcntSetNormOn ( Widget, XtPointer ) 
{
    // This virtual function is called from refcntSetNormOnCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::refcntSetOff ( Widget, XtPointer ) 
{
    // This virtual function is called from refcntSetOffCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::refcntSetOn ( Widget, XtPointer ) 
{
    // This virtual function is called from refcntSetOnCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::refcntUAThreshFieldChanged ( Widget, XtPointer ) 
{
    // This virtual function is called from refcntUAThreshFieldChangedCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::unpegIntervalFieldChanged ( Widget, XtPointer ) 
{
    // This virtual function is called from unpegIntervalFieldChangedCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::unpegSetOff ( Widget, XtPointer ) 
{
    // This virtual function is called from unpegSetOffCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::unpegSetOn ( Widget, XtPointer ) 
{
    // This virtual function is called from unpegSetOnCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::unpegThreshFieldChanged ( Widget, XtPointer ) 
{
    // This virtual function is called from unpegThreshFieldChangedCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::unpegThreshScaleChanged ( Widget, XtPointer ) 
{
    // This virtual function is called from unpegThreshScaleChangedCallback.
    // This function is normally overriden by a derived class.

}

void BulletinBoardUI::unpegThreshScaleDynamic ( Widget, XtPointer ) 
{
    // This virtual function is called from unpegThreshScaleDynamicCallback.
    // This function is normally overriden by a derived class.

}



//---- Start editable code block: End of generated code


//---- End editable code block: End of generated code
