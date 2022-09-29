/* Layout file created by tedit */

#ifndef TUDONTAUTOINCLUDE
#include "tuLabelMenuItem.h"
#include "tuSeparator.h"
#include "tuMenu.h"
#include "tuMenuBar.h"
#include "tuFrame.h"
#include "tuScrollBar.h"
#include "tuRowColumn.h"
#endif /* TUDONTAUTOINCLUDE */

#ifdef TUREGISTERGADGETS
#undef TUREGISTERGADGETS
#endif /* TUREGISTERGADGETS */
#define TUREGISTERGADGETS { \
    tuLabelMenuItem::registerForPickling(); \
    tuSeparator::registerForPickling(); \
    tuMenu::registerForPickling(); \
    tuMenuBar::registerForPickling(); \
    tuFrame::registerForPickling(); \
    tuScrollBar::registerForPickling(); \
    tuRowColumn::registerForPickling(); \
}

static char* layoutstr =
"(RowColumn "
 "(instanceName viewMainRC) "
 "(rows 2) "
 "(cols 1) "
 "(defaultAlignment 0) "
 "(child "
  "(Frame "
   "(instanceName menuparent) "
   "(resources "
    "((frame Outie))) "
   "(child "
    "(MenuBar "
     "(instanceName menuBar) "
     "(rows 1) "
     "(cols 5) "
     "(defaultAlignment 3) "
     "(resources "
      "((horizontalOffset 5) "
       "(fixedHeight True))) "
     "(child "
      "(LabelMenuItem "
       "(subMenu "
        "(Menu "
         "(rows 4) "
         "(cols 1) "
         "(defaultAlignment 3) "
         "(ownParent True) "
         "(title File) "
         "(resources "
          "((topInset 4) "
           "(bottomInset 4) "
           "(leftInset 4) "
           "(rightInset 4) "
           "(horizontalOffset 2))) "
         "(child "
          "(LabelMenuItem "
           "(instanceName FileSaveItem)  "
           "(resources "
            "((callback __NetTop_save) "
             "(accelerator Control\\ !Shift\\ s) "
             "(labelString Save\\ Controls))))) "
         "(constraint R0C0A3) "
         "(child "
          "(LabelMenuItem "
           "(instanceName FileSaveAsItem)  "
           "(resources "
            "((callback __NetTop_saveAs) "
             "(accelerator \\\"Ctrl-S\\\"\\ Shift\\ Control\\ s) "
             "(labelString Save\\ Controls\\ As...))))) "
         "(constraint R1C0A3) "
         "(child "
          "(Separator "
           "(orientation horizontal))) "
         "(constraint R2C0A0) "
         "(child "
          "(LabelMenuItem "
           "(instanceName FileQuitItem)  "
           "(resources "
            "((callback __NetTop_quit) "
             "(accelerator Alt\\ q) "
             "(labelString Quit))))) "
         "(constraint R3C0A3))) "
       "(resources "
        "((labelString File))))) "
     "(constraint R0C0A0) "
     "(child "
      "(LabelMenuItem "
       "(subMenu "
        "(Menu "
         "(rows 2) "
         "(cols 1) "
         "(defaultAlignment 3) "
         "(ownParent True) "
         "(title Controls) "
         "(resources "
          "((topInset 4) "
           "(bottomInset 4) "
           "(leftInset 4) "
           "(rightInset 4) "
           "(horizontalOffset 2))) "
         "(child "
          "(LabelMenuItem "
           "(instanceName GizmoNodeItem)  "
           "(resources "
            "((callback __NetTop_openNodeGizmo) "
             "(accelerator Alt\\ n) "
             "(labelString Nodes...))))) "
         "(constraint R0C0A3) "
         "(child "
          "(LabelMenuItem "
           "(instanceName GizmoDataItem)  "
           "(resources "
            "((callback __NetTop_openDataGizmo) "
             "(accelerator Alt\\ t) "
             "(labelString Traffic...))))) "
         "(constraint R1C0A3))) "
       "(resources "
        "((labelString Controls))))) "
     "(constraint R0C1A0) "
     "(child "
      "(LabelMenuItem "
       "(subMenu "
        "(Menu "
         "(rows 5) "
         "(cols 1) "
         "(defaultAlignment 3) "
         "(ownParent True) "
         "(title Tools) "
         "(resources "
          "((topInset 4) "
           "(bottomInset 4) "
           "(leftInset 4) "
           "(rightInset 4) "
           "(horizontalOffset 2))) "
         "(child "
          "(LabelMenuItem  "
           "(resources "
            "((callback Tool_analyzer) "
             "(labelString Analyzer))))) "
         "(constraint R0C0A3) "
         "(child "
          "(LabelMenuItem  "
           "(resources "
            "((callback Tool_browser) "
             "(labelString Browser))))) "
         "(constraint R1C0A3) "
         "(child "
          "(LabelMenuItem  "
           "(resources "
            "((callback Tool_netgraph) "
             "(labelString NetGraph))))) "
         "(constraint R2C0A3) "
         "(child "
          "(LabelMenuItem  "
           "(resources "
            "((callback Tool_netlook) "
             "(labelString NetLook))))) "
         "(constraint R3C0A3) "
         "(child "
          "(LabelMenuItem  "
           "(resources "
            "((callback Tool_nettop) "
             "(labelString NetTop))))) "
         "(constraint R4C0A3))) "
       "(resources "
        "((labelString Tools))))) "
     "(constraint R0C2A3) "
     "(child "
      "(LabelMenuItem "
       "(subMenu "
        "(Menu "
         "(rows 1) "
         "(cols 1) "
         "(defaultAlignment 3) "
         "(ownParent True) "
         "(title Help) "
         "(resources "
          "((topInset 4) "
           "(bottomInset 4) "
           "(leftInset 4) "
           "(rightInset 4) "
           "(horizontalOffset 2))) "
         "(child "
          "(LabelMenuItem "
           "(instanceName HelpItem)  "
           "(resources "
            "((callback __NetTop_help) "
             "(accelerator Alt\\ h) "
             "(labelString Help))))) "
         "(constraint R0C0A3))) "
       "(resources "
        "((labelString Help))))) "
     "(constraint R0C4A0))))) "
 "(constraint R0C0A3) "
 "(child "
  "(RowColumn "
   "(instanceName viewSecondRC) "
   "(rows 2) "
   "(cols 3) "
   "(defaultAlignment 0) "
   "(child "
    "(ScrollBar "
     "(instanceName zoomScrollBar) "
     "(orientation vertical) "
     "(resources "
      "((dragCallBack __ViewGadget_zoomDrag) "
       "(pageIncCallBack __ViewGadget_zoomPageInc) "
       "(pageDecCallBack __ViewGadget_zoomPageDec) "
       "(lowerBound 600) "
       "(upperBound 60) "
       "(percentShown 0.1) "
       "(incCallBack __ViewGadget_zoomInc) "
       "(decCallBack __ViewGadget_zoomDec))))) "
   "(constraint R0C0A18) "
   "(child "
    "(Frame "
     "(instanceName GLparent))) "
   "(constraint R0C1A16) "
   "(child "
    "(ScrollBar "
     "(instanceName inclinScrollBar) "
     "(orientation vertical) "
     "(resources "
      "((dragCallBack __ViewGadget_inclinDrag) "
       "(pageIncCallBack __ViewGadget_inclinPageInc) "
       "(pageDecCallBack __ViewGadget_inclinPageDec) "
       "(upperBound 900) "
       "(percentShown 0.1) "
       "(incCallBack __ViewGadget_inclinInc) "
       "(decCallBack __ViewGadget_inclinDec))))) "
   "(constraint R0C2A11) "
   "(child "
    "(ScrollBar "
     "(instanceName azimScrollBar) "
     "(orientation horizontal) "
     "(resources "
      "((dragCallBack __ViewGadget_azimDrag) "
       "(pageIncCallBack __ViewGadget_azimPageInc) "
       "(pageDecCallBack __ViewGadget_azimPageDec) "
       "(lowerBound -450) "
       "(upperBound 3150) "
       "(percentShown 0.1) "
       "(incCallBack __ViewGadget_azimInc) "
       "(decCallBack __ViewGadget_azimDec))))) "
   "(constraint R1C1A11))) "
 "(constraint R1C0A3))";
