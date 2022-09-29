
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
#include <Xm/ScrolledW.h> 
#include <Vk/VkResource.h>


// Externally defined classes referenced by this class: 

#include <Vk/VkTickMarks.h>
#include <Vk/VkVuMeter.h>
//---- Start editable code block: headers and declarations
#include <stdio.h>

//---- End editable code block: headers and declarations



// These are default resources for widgets in objects of this class
// All resources will be prepended by *<name> at instantiation,
// where <name> is the name of the specific instance, as well as the
// name of the baseWidget. These are only defaults, and may be overriden
// in a resource file by providing a more specific resource name

String  BulletinBoardUI::_defaultBulletinBoardUIResources[] = {
        "*lAMF.labelString:  AfN",
        "*lAMS.labelString:  AS",
        "*lAMT.labelString:  AtN",
        "*lCI.labelString:  CI",
        "*lCMF.labelString:  CfN",
        "*lCMS.labelString:  CS",
        "*lCMT.labelString:  CtN",
        "*lChange.labelString:  Change Per Time Interval",
        "*lFP.labelString:  FP",
        "*lMP.labelString:  MP",
        "*lNode.labelString:  Node",
        "*lUMF.labelString:  UfN",
        "*lUMS.labelString:  US",
        "*lUMT.labelString:  UtN",
        "*label.labelString:  Migration Threshold:",
        "*label1.labelString:  Counter Interrupts:",
        "*label10.labelString:  CoalD Migration Skips:",
        "*label11.labelString:  Frozen Pages In Node:",
        "*label12.labelString:  Melted Pages In Node:",
        "*label2.labelString:  Auto Migrations To Node:",
        "*label3.labelString:  Auto Migrations From Node:",
        "*label4.labelString:  Auto Migration Skips:",
        "*label5.labelString:  User Migrations To Node:",
        "*label6.labelString:  User Migrations From Node:",
        "*label7.labelString:  User Migration Skips:",
        "*label8.labelString:  CoalD Migrations To Node:",
        "*label9.labelString:  CoalD Migrations From Node:",
        "*valAMF.labelString:  00000000",
        "*valAMS.labelString:  00000000",
        "*valAMT.labelString:  00000000",
        "*valCI.labelString:  00000000",
        "*valCMF.labelString:  00000000",
        "*valCMS.labelString:  00000000",
        "*valCMT.labelString:  00000000",
        "*valFP.labelString:  00000000",
        "*valMP.labelString:  00000000",
        "*valMT.labelString:  00000000",
        "*valUMF.labelString:  00000000",
        "*valUMS.labelString:  00000000",
        "*valUMT.labelString:  00000000",

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
    delete _meterMP;

    delete _meterFP;

    delete _meterCMS;

    delete _meterCMF;

    delete _meterCMT;

    delete _meterUMS;

    delete _meterUMF;

    delete _meterUMT;

    delete _meterAMS;

    delete _meterAMF;

    delete _meterAMT;

    delete _meterCI;

    delete _tickmarks;


    //---- Start editable code block: BulletinBoardUI destructor


    //---- End editable code block: BulletinBoardUI destructor
}    // End destructor



void BulletinBoardUI::create ( Widget parent )
{
    Arg      args[8];
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

    _lNode = XtVaCreateManagedWidget  ( "lNode",
                                         xmLabelGadgetClass,
                                         _baseWidget, 
                                         XmNx, 110, 
                                         XmNy, 20, 
                                         XmNwidth, 41, 
                                         XmNheight, 20, 
                                         (XtPointer) NULL ); 


    _lChange = XtVaCreateManagedWidget  ( "lChange",
                                           xmLabelGadgetClass,
                                           _baseWidget, 
                                           XmNx, 710, 
                                           XmNy, 20, 
                                           XmNwidth, 178, 
                                           XmNheight, 20, 
                                           (XtPointer) NULL ); 


    _lMP = XtVaCreateManagedWidget  ( "lMP",
                                       xmLabelGadgetClass,
                                          _baseWidget, 
                                       XmNx, 960, 
                                       XmNy, 250, 
                                       XmNwidth, 27, 
                                       XmNheight, 20, 
                                       (XtPointer) NULL ); 


    _lFP = XtVaCreateManagedWidget  ( "lFP",
                                       xmLabelGadgetClass,
                                       _baseWidget, 
                                       XmNx, 930, 
                                       XmNy, 250, 
                                       XmNwidth, 23, 
                                       XmNheight, 20, 
                                       (XtPointer) NULL ); 


    _lCMS = XtVaCreateManagedWidget  ( "lCMS",
                                        xmLabelGadgetClass,
                                        _baseWidget, 
                                        XmNx, 900, 
                                        XmNy, 250, 
                                        XmNwidth, 25, 
                                        XmNheight, 20, 
                                        (XtPointer) NULL ); 


    _lCMF = XtVaCreateManagedWidget  ( "lCMF",
                                        xmLabelGadgetClass,
                                        _baseWidget, 
                                        XmNx, 865, 
                                        XmNy, 250, 
                                        XmNwidth, 30, 
                                        XmNheight, 20, 
                                        (XtPointer) NULL ); 


    _lCMT = XtVaCreateManagedWidget  ( "lCMT",
                                        xmLabelGadgetClass,
                                        _baseWidget, 
                                        XmNx, 835, 
                                        XmNy, 250, 
                                        XmNwidth, 31, 
                                        XmNheight, 20, 
                                        (XtPointer) NULL ); 


    _lUMS = XtVaCreateManagedWidget  ( "lUMS",
                                        xmLabelGadgetClass,
                                        _baseWidget, 
                                        XmNx, 810, 
                                        XmNy, 250, 
                                        XmNwidth, 25, 
                                        XmNheight, 20, 
                                        (XtPointer) NULL ); 


    _lUMF = XtVaCreateManagedWidget  ( "lUMF",
                                        xmLabelGadgetClass,
                                        _baseWidget, 
                                        XmNx, 775, 
                                        XmNy, 250, 
                                        XmNwidth, 30, 
                                        XmNheight, 20, 
                                        (XtPointer) NULL ); 


    _lUMT = XtVaCreateManagedWidget  ( "lUMT",
                                        xmLabelGadgetClass,
                                        _baseWidget, 
                                        XmNx, 745, 
                                        XmNy, 250, 
                                        XmNwidth, 31, 
                                        XmNheight, 20, 
                                        (XtPointer) NULL ); 


    _lAMS = XtVaCreateManagedWidget  ( "lAMS",
                                        xmLabelGadgetClass,
                                        _baseWidget, 
                                        XmNx, 720, 
                                        XmNy, 250, 
                                        XmNwidth, 24, 
                                        XmNheight, 20, 
                                        (XtPointer) NULL ); 


    _lAMF = XtVaCreateManagedWidget  ( "lAMF",
                                        xmLabelGadgetClass,
                                        _baseWidget, 
                                        XmNx, 685, 
                                        XmNy, 250, 
                                        XmNwidth, 29, 
                                        XmNheight, 20, 
                                        (XtPointer) NULL ); 


    _lAMT = XtVaCreateManagedWidget  ( "lAMT",
                                        xmLabelGadgetClass,
                                        _baseWidget, 
                                        XmNx, 655, 
                                        XmNy, 250, 
                                        XmNwidth, 30, 
                                        XmNheight, 20, 
                                        (XtPointer) NULL ); 


    _lCI = XtVaCreateManagedWidget  ( "lCI",
                                       xmLabelGadgetClass,
                                       _baseWidget, 
                                       XmNx, 630, 
                                       XmNy, 250, 
                                       XmNwidth, 19, 
                                       XmNheight, 20, 
                                       (XtPointer) NULL ); 


    _meterMP = new VkVuMeter( "meterMP", _baseWidget  );
    _meterMP->show();


    _meterFP = new VkVuMeter( "meterFP", _baseWidget  );
    _meterFP->show();


    _meterCMS = new VkVuMeter( "meterCMS", _baseWidget  );
    _meterCMS->show();


    _meterCMF = new VkVuMeter( "meterCMF", _baseWidget  );
    _meterCMF->show();


    _meterCMT = new VkVuMeter( "meterCMT", _baseWidget  );
    _meterCMT->show();


    _meterUMS = new VkVuMeter( "meterUMS", _baseWidget  );
    _meterUMS->show();


    _meterUMF = new VkVuMeter( "meterUMF", _baseWidget  );
    _meterUMF->show();


    _meterUMT = new VkVuMeter( "meterUMT", _baseWidget  );
    _meterUMT->show();


    _meterAMS = new VkVuMeter( "meterAMS", _baseWidget  );
    _meterAMS->show();


    _meterAMF = new VkVuMeter( "meterAMF", _baseWidget  );
    _meterAMF->show();


    _meterAMT = new VkVuMeter( "meterAMT", _baseWidget  );
    _meterAMT->show();


    _meterCI = new VkVuMeter( "meterCI", _baseWidget  );
    _meterCI->show();


    _tickmarks = new VkTickMarks( "tickmarks", _baseWidget  );
    _tickmarks->show();


    _valMP = XtVaCreateManagedWidget  ( "valMP",
                                         xmLabelGadgetClass,
                                         _baseWidget, 
                                         XmNx, 480, 
                                         XmNy, 290, 
                                         XmNwidth, 68, 
                                         XmNheight, 20, 
                                         (XtPointer) NULL ); 


    _valFP = XtVaCreateManagedWidget  ( "valFP",
                                         xmLabelGadgetClass,
                                         _baseWidget, 
                                         XmNx, 480, 
                                         XmNy, 270, 
                                         XmNwidth, 68, 
                                         XmNheight, 20, 
                                         (XtPointer) NULL ); 


    _valCMS = XtVaCreateManagedWidget  ( "valCMS",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 480, 
                                          XmNy, 250, 
                                          XmNwidth, 68, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _valCMF = XtVaCreateManagedWidget  ( "valCMF",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 480, 
                                          XmNy, 230, 
                                          XmNwidth, 68, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _valCMT = XtVaCreateManagedWidget  ( "valCMT",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 480, 
                                          XmNy, 210, 
                                          XmNwidth, 68, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _valUMS = XtVaCreateManagedWidget  ( "valUMS",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 480, 
                                          XmNy, 190, 
                                          XmNwidth, 68, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _valUMF = XtVaCreateManagedWidget  ( "valUMF",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 480, 
                                          XmNy, 170, 
                                          XmNwidth, 68, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _valUMT = XtVaCreateManagedWidget  ( "valUMT",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 480, 
                                          XmNy, 150, 
                                          XmNwidth, 68, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _valAMS = XtVaCreateManagedWidget  ( "valAMS",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 480, 
                                          XmNy, 130, 
                                          XmNwidth, 68, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _label12 = XtVaCreateManagedWidget  ( "label12",
                                           xmLabelGadgetClass,
                                           _baseWidget, 
                                           XmNx, 260, 
                                           XmNy, 290, 
                                           XmNwidth, 161, 
                                           XmNheight, 20, 
                                           (XtPointer) NULL ); 


    _label11 = XtVaCreateManagedWidget  ( "label11",
                                           xmLabelGadgetClass,
                                           _baseWidget, 
                                           XmNx, 260, 
                                           XmNy, 270, 
                                           XmNwidth, 161, 
                                           XmNheight, 20, 
                                           (XtPointer) NULL ); 


    _label10 = XtVaCreateManagedWidget  ( "label10",
                                           xmLabelGadgetClass,
                                           _baseWidget, 
                                           XmNx, 260, 
                                           XmNy, 250, 
                                           XmNwidth, 166, 
                                           XmNheight, 20, 
                                           (XtPointer) NULL ); 


    _label9 = XtVaCreateManagedWidget  ( "label9",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 260, 
                                          XmNy, 230, 
                                          XmNwidth, 212, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _label8 = XtVaCreateManagedWidget  ( "label8",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 260, 
                                          XmNy, 210, 
                                          XmNwidth, 193, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _label7 = XtVaCreateManagedWidget  ( "label7",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 260, 
                                          XmNy, 190, 
                                          XmNwidth, 156, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _label6 = XtVaCreateManagedWidget  ( "label6",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 260, 
                                          XmNy, 170, 
                                          XmNwidth, 202, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _label5 = XtVaCreateManagedWidget  ( "label5",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 260, 
                                          XmNy, 150, 
                                          XmNwidth, 183, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _label4 = XtVaCreateManagedWidget  ( "label4",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 260, 
                                          XmNy, 130, 
                                          XmNwidth, 156, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _valAMF = XtVaCreateManagedWidget  ( "valAMF",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 480, 
                                          XmNy, 110, 
                                          XmNwidth, 68, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _valAMT = XtVaCreateManagedWidget  ( "valAMT",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 480, 
                                          XmNy, 90, 
                                          XmNwidth, 68, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _valCI = XtVaCreateManagedWidget  ( "valCI",
                                         xmLabelGadgetClass,
                                         _baseWidget, 
                                         XmNx, 480, 
                                         XmNy, 70, 
                                         XmNwidth, 68, 
                                         XmNheight, 20, 
                                         (XtPointer) NULL ); 


    _valMT = XtVaCreateManagedWidget  ( "valMT",
                                         xmLabelGadgetClass,
                                         _baseWidget, 
                                         XmNx, 480, 
                                         XmNy, 50, 
                                         XmNwidth, 68, 
                                         XmNheight, 20, 
                                         (XtPointer) NULL ); 


    _label3 = XtVaCreateManagedWidget  ( "label3",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 260, 
                                          XmNy, 110, 
                                          XmNwidth, 202, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _label2 = XtVaCreateManagedWidget  ( "label2",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 260, 
                                          XmNy, 90, 
                                          XmNwidth, 183, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _label1 = XtVaCreateManagedWidget  ( "label1",
                                          xmLabelGadgetClass,
                                          _baseWidget, 
                                          XmNx, 260, 
                                          XmNy, 70, 
                                          XmNwidth, 139, 
                                          XmNheight, 20, 
                                          (XtPointer) NULL ); 


    _label = XtVaCreateManagedWidget  ( "label",
                                         xmLabelGadgetClass,
                                         _baseWidget, 
                                         XmNx, 260, 
                                         XmNy, 50, 
                                         XmNwidth, 150, 
                                         XmNheight, 20, 
                                         (XtPointer) NULL ); 


    _scrolledWindow = XtVaCreateManagedWidget  ( "scrolledWindow",
                                                  xmScrolledWindowWidgetClass,
                                                  _baseWidget, 
                                                  XmNscrollBarDisplayPolicy, XmSTATIC, 
                                                  XmNx, 10, 
                                                  XmNy, 50, 
                                                  XmNwidth, 240, 
                                                  XmNheight, 200, 
                                                  (XtPointer) NULL ); 


    _scrolledList = XtVaCreateManagedWidget  ( "scrolledList",
                                                xmListWidgetClass,
                                                _scrolledWindow, 
                                                XmNwidth, 234, 
                                                XmNheight, 194, 
                                                (XtPointer) NULL ); 

    XmString *scrolledListItems = new XmString [0];
    XtVaSetValues ( _scrolledList, XmNitems, scrolledListItems,
                    XmNitemCount, 0, NULL );
    for ( int scrolledListCounter = 0; scrolledListCounter < 0; scrolledListCounter++ )
        XmStringFree ( scrolledListItems[scrolledListCounter] );
    delete scrolledListItems;

    XtAddCallback ( _scrolledList,
                    XmNbrowseSelectionCallback,
                    &BulletinBoardUI::nodeSelectCallback,
                    (XtPointer) this ); 


    XtVaSetValues ( _meterMP->baseWidget(),
                    XmNx, 960, 
                    XmNy, 50, 
                    XmNwidth, 20, 
                    XmNheight, 200, 
                    (XtPointer) NULL );
    XtVaSetValues ( _meterFP->baseWidget(),
                    XmNx, 930, 
                    XmNy, 50, 
                    XmNwidth, 20, 
                    XmNheight, 200, 
                    (XtPointer) NULL );
    XtVaSetValues ( _meterCMS->baseWidget(),
                    XmNx, 900, 
                    XmNy, 50, 
                    XmNwidth, 20, 
                    XmNheight, 200, 
                    (XtPointer) NULL );
    XtVaSetValues ( _meterCMF->baseWidget(),
                    XmNx, 870, 
                    XmNy, 50, 
                    XmNwidth, 20, 
                    XmNheight, 200, 
                    (XtPointer) NULL );
    XtVaSetValues ( _meterCMT->baseWidget(),
                    XmNx, 840, 
                    XmNy, 50, 
                    XmNwidth, 20, 
                    XmNheight, 200, 
                    (XtPointer) NULL );
    XtVaSetValues ( _meterUMS->baseWidget(),
                    XmNx, 810, 
                    XmNy, 50, 
                    XmNwidth, 20, 
                    XmNheight, 200, 
                    (XtPointer) NULL );
    XtVaSetValues ( _meterUMF->baseWidget(),
                    XmNx, 780, 
                    XmNy, 50, 
                    XmNwidth, 20, 
                    XmNheight, 200, 
                    (XtPointer) NULL );
    XtVaSetValues ( _meterUMT->baseWidget(),
                    XmNx, 750, 
                    XmNy, 50, 
                    XmNwidth, 20, 
                    XmNheight, 200, 
                    (XtPointer) NULL );
    XtVaSetValues ( _meterAMS->baseWidget(),
                    XmNx, 720, 
                    XmNy, 50, 
                    XmNwidth, 20, 
                    XmNheight, 200, 
                    (XtPointer) NULL );
    XtVaSetValues ( _meterAMF->baseWidget(),
                    XmNx, 690, 
                    XmNy, 50, 
                    XmNwidth, 20, 
                    XmNheight, 200, 
                    (XtPointer) NULL );
    XtVaSetValues ( _meterAMT->baseWidget(),
                    XmNx, 660, 
                    XmNy, 50, 
                    XmNwidth, 20, 
                    XmNheight, 200, 
                    (XtPointer) NULL );
    XtVaSetValues ( _meterCI->baseWidget(),
                    XmNx, 630, 
                    XmNy, 50, 
                    XmNwidth, 20, 
                    XmNheight, 200, 
                    (XtPointer) NULL );
    XtVaSetValues ( _tickmarks->baseWidget(),
                    XmNx, 570, 
                    XmNy, 50, 
                    XmNwidth, 50, 
                    XmNheight, 216, 
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

void BulletinBoardUI::nodeSelectCallback ( Widget    w,
                                           XtPointer clientData,
                                           XtPointer callData ) 
{ 
    BulletinBoardUI* obj = ( BulletinBoardUI * ) clientData;
    obj->nodeSelect ( w, callData );
}



/////////////////////////////////////////////////////////////// 
// The following functions are called from the menu items 
// in this window.
/////////////////////////////////// 

void BulletinBoardUI::nodeSelect ( Widget, XtPointer ) 
{
    // This virtual function is called from nodeSelectCallback.
    // This function is normally overriden by a derived class.

}



//---- Start editable code block: End of generated code

//---- End editable code block: End of generated code
