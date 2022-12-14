/* Layout file created by tedit */

#ifndef TUDONTAUTOINCLUDE
#include "tuLabelMenuItem.h"
#include "tuSeparator.h"
#include "tuMenu.h"
#include "tuMenuBar.h"
#include "tuFrame.h"
#include "tuLabel.h"
#include "tuTextField.h"
#include "tuRowColumn.h"
#include "tuScrollView.h"
#include "tuGap.h"
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
    tuLabel::registerForPickling(); \
    tuTextField::registerForPickling(); \
    tuRowColumn::registerForPickling(); \
    tuScrollView::registerForPickling(); \
    tuGap::registerForPickling(); \
}

static char* layoutstr =
"(RowColumn "
 "(rows 2) "
 "(cols 1) "
 "(growdir vertical) "
 "(defaultAlignment 0) "
 "(resources "
  "((horizontalOffset 10) "
   "(verticalOffset 10))) "
 "(child "
  "(Frame "
   "(resources "
    "((frame Outie))) "
   "(child "
    "(MenuBar "
     "(rows 1) "
     "(cols 5) "
     "(defaultAlignment 0) "
     "(resources "
      "((horizontalOffset 5) "
       "(fixedHeight True))) "
     "(child "
      "(LabelMenuItem "
       "(subMenu "
        "(Menu "
         "(rows 5) "
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
           "(instanceName save)  "
           "(resources "
            "((accelerator Alt\\ s) "
             "(labelString Save\\ MIB\\ Values))))) "
         "(constraint R0C0A3) "
         "(child "
          "(LabelMenuItem "
           "(instanceName saveAs)  "
           "(resources "
            "((accelerator \\\"Alt-S\\\"\\ Shift\\ Alt\\ s) "
             "(labelString Save\\ MIB\\ Values\\ As...))))) "
         "(constraint R1C0A3) "
         "(child "
          "(Separator "
           "(orientation horizontal))) "
         "(constraint R2C0A3) "
         "(child "
          "(LabelMenuItem "
           "(instanceName openParent)  "
           "(resources "
            "((callback __bTop_openMain) "
             "(accelerator Alt\\ m) "
             "(labelString Pop\\ Main\\ Window))))) "
         "(constraint R3C0A3) "
         "(child "
          "(LabelMenuItem  "
           "(resources "
            "((callback __bTop_close) "
             "(accelerator Alt\\ c) "
             "(labelString Close))))) "
         "(constraint R4C0A3))) "
       "(resources "
        "((labelString File))))) "
     "(constraint R0C0A0) "
     "(child "
      "(LabelMenuItem "
       "(subMenu "
        "(Menu "
         "(rows 3) "
         "(cols 1) "
         "(defaultAlignment 3) "
         "(ownParent True) "
         "(title Edit) "
         "(resources "
          "((topInset 4) "
           "(bottomInset 4) "
           "(leftInset 4) "
           "(rightInset 4) "
           "(horizontalOffset 2))) "
         "(child "
          "(LabelMenuItem "
           "(instanceName clear)  "
           "(resources "
            "((accelerator Alt\\ l) "
             "(labelString Clear\\ Display))))) "
         "(constraint R0C0A3) "
         "(child "
          "(LabelMenuItem "
           "(instanceName getNext)  "
           "(resources "
            "((accelerator Alt\\ n) "
             "(labelString Get\\ Next\\ Row))))) "
         "(constraint R1C0A3) "
         "(child "
          "(LabelMenuItem "
           "(instanceName set)  "
           "(resources "
            "((labelString Set))))) "
         "(constraint R2C0A3))) "
       "(resources "
        "((labelString Edit))))) "
     "(constraint R0C1A0) "
     "(child "
      "(LabelMenuItem "
       "(instanceName navigateLabel) "
       "(subMenu "
        "(Menu "
         "(instanceName navMenu) "
         "(rows 1) "
         "(cols 1) "
         "(defaultAlignment 3) "
         "(ownParent True) "
         "(title Navigate) "
         "(resources "
          "((topInset 4) "
           "(bottomInset 4) "
           "(leftInset 4) "
           "(rightInset 4) "
           "(horizontalOffset 2))))) "
       "(resources "
        "((labelString Navigate))))) "
     "(constraint R0C2A0) "
     "(child "
      "(LabelMenuItem "
       "(subMenu "
        "(Menu "
         "(rows 2) "
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
           "(instanceName describe)  "
           "(resources "
            "((callback __Table_describe) "
             "(accelerator Alt\\ d) "
             "(labelString Description))))) "
         "(constraint R0C0A3) "
         "(child "
          "(LabelMenuItem  "
           "(resources "
            "((callback __bTop_help) "
             "(accelerator Alt\\ h) "
             "(labelString Help))))) "
         "(constraint R1C0A3))) "
       "(resources "
        "((labelString Help))))) "
     "(constraint R0C4A0))))) "
 "(constraint R0C0A3) "
 "(child "
  "(RowColumn "
   "(rows 4) "
   "(cols 1) "
   "(growdir vertical) "
   "(defaultAlignment 0) "
   "(resources "
    "((leftInset 10) "
     "(rightInset 10) "
     "(horizontalOffset 10) "
     "(verticalOffset 10))) "
   "(child "
    "(RowColumn "
     "(rows 3) "
     "(cols 2) "
     "(growdir vertical) "
     "(defaultAlignment 0) "
     "(resources "
      "((leftInset 10) "
       "(horizontalOffset 10) "
       "(verticalOffset 5) "
       "(fixedHeight True))) "
     "(child "
      "(Label "
       "(resources "
        "((labelString Node:) "
         "(font -*-helvetica-bold-r-normal--14-*-*-*-p-*-iso8859-1))))) "
     "(constraint R0C0A6) "
     "(child "
      "(Frame "
       "(resources "
        "((frame Innie))) "
       "(child "
        "(TextField "
         "(instanceName nodeField) "
         "(text ) "
         "(resources "
          "((background ReadOnlyBackground) "
           "(readOnly True) "
           "(keepAllTextVisible False))))))) "
     "(constraint R0C1A0) "
     "(child "
      "(Label "
       "(resources "
        "((labelString Object\\ ID:) "
         "(font -*-helvetica-bold-r-normal--14-*-*-*-p-*-iso8859-1))))) "
     "(constraint R1C0A2) "
     "(child "
      "(Frame "
       "(resources "
        "((frame Innie))) "
       "(child "
        "(TextField "
         "(instanceName idField) "
         "(text ) "
         "(resources "
          "((background ReadOnlyBackground) "
           "(readOnly True) "
           "(keepAllTextVisible False))))))) "
     "(constraint R1C1A0) "
     "(child "
      "(Label "
       "(resources "
        "((labelString Name:) "
         "(font -*-helvetica-bold-r-normal--14-*-*-*-p-*-iso8859-1))))) "
     "(constraint R2C0A2) "
     "(child "
      "(Frame "
       "(resources "
        "((frame Innie))) "
       "(child "
        "(TextField "
         "(instanceName nameField) "
         "(text ) "
         "(resources "
          "((background ReadOnlyBackground) "
           "(readOnly True) "
           "(keepAllTextVisible False))))))) "
     "(constraint R2C1A0))) "
   "(constraint R0C0A0) "
   "(child "
    "(Frame "
     "(resources "
      "((frame ditch))) "
     "(child "
      "(ScrollView "
       "(horizBar True) "
       "(vertBar True) "
       "(child "
        "(RowColumn "
         "(instanceName centralRC) "
         "(rows 2) "
         "(cols 1) "
         "(growdir vertical) "
         "(defaultAlignment 4) "
         "(resources "
          "((topInset 5) "
           "(bottomInset 5) "
           "(leftInset 5) "
           "(rightInset 5) "
           "(horizontalOffset 10) "
           "(verticalOffset 5))))))))) "
   "(constraint R1C0A3) "
   "(child "
    "(RowColumn "
     "(rows 2) "
     "(cols 2) "
     "(defaultAlignment 4) "
     "(resources "
      "((leftInset 10) "
       "(rightInset 10) "
       "(horizontalOffset 10) "
       "(verticalOffset 5) "
       "(fixedHeight True))) "
     "(child "
      "(Label "
       "(resources "
        "((labelString Read\\ at:) "
         "(font -*-helvetica-bold-r-normal--14-*-*-*-p-*-iso8859-1))))) "
     "(constraint R0C0A6) "
     "(child "
      "(Label "
       "(instanceName getTimeLbl))) "
     "(constraint R0C1A4) "
     "(child "
      "(Label "
       "(resources "
        "((labelString Set\\ at:) "
         "(font -*-helvetica-bold-r-normal--14-*-*-*-p-*-iso8859-1))))) "
     "(constraint R1C0A6) "
     "(child "
      "(Label "
       "(instanceName setTimeLbl))) "
     "(constraint R1C1A4))) "
   "(constraint R2C0A0) "
   "(child "
    "(Gap "
     "(orientation horizontal) "
     "(resources "
      "((minWidth 300))))) "
   "(constraint R3C0A0))) "
 "(constraint R1C0A19))";
