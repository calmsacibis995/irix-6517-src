/* Layout file created by tedit */

#ifndef TUDONTAUTOINCLUDE
#include "tuLabel.h"
#include "tuList.h"
#include "tuFrame.h"
#include "tuTextField.h"
#include "tuGap.h"
#include "tuDefaultButton.h"
#include "tuLabelButton.h"
#include "tuRowColumn.h"
#endif /* TUDONTAUTOINCLUDE */

#ifdef TUREGISTERGADGETS
#undef TUREGISTERGADGETS
#endif /* TUREGISTERGADGETS */
#define TUREGISTERGADGETS { \
    tuLabel::registerForPickling(); \
    tuList::registerForPickling(); \
    tuFrame::registerForPickling(); \
    tuTextField::registerForPickling(); \
    tuGap::registerForPickling(); \
    tuDefaultButton::registerForPickling(); \
    tuLabelButton::registerForPickling(); \
    tuRowColumn::registerForPickling(); \
}

static char* layoutstr =
"(RowColumn "
 "(rows 4) "
 "(cols 1) "
 "(growdir vertical) "
 "(defaultAlignment 0) "
 "(resources "
  "((topInset 8) "
   "(bottomInset 8) "
   "(leftInset 8) "
   "(rightInset 8) "
   "(horizontalOffset 8) "
   "(verticalOffset 8))) "
 "(child "
  "(Label "
   "(resources "
    "((labelString Hidden\\ objects:) "
     "(font -*-helvetica-bold-r-normal--14-*-*-*-p-*-iso8859-1))))) "
 "(constraint R0C0A0) "
 "(child "
  "(Frame "
   "(child "
    "(List "
     "(instanceName Hide_list) "
     "(resources "
      "((callback Hide_select) "
       "(othercallback Hide_pick))))))) "
 "(constraint R1C0A3) "
 "(child "
  "(Frame "
   "(resources "
    "((frame Innie))) "
   "(child "
    "(TextField "
     "(instanceName Hide_textfield) "
     "(text ) "
     "(resources "
      "((terminatorCallBack Hide_text) "
       "(keepAllTextVisible False))))))) "
 "(constraint R2C0A0) "
 "(child "
  "(RowColumn "
   "(rows 1) "
   "(cols 5) "
   "(defaultAlignment 0) "
   "(resources "
    "((horizontalOffset 8) "
     "(fixedHeight True))) "
   "(child "
    "(Gap "
     "(orientation horizontal) "
     "(resources "
      "((minWidth 50))))) "
   "(constraint R0C0A0) "
   "(child "
    "(DefaultButton "
     "(resources "
      "((callback Hide_hide) "
       "(labelString Hide))))) "
   "(constraint R0C1A0) "
   "(child "
    "(LabelButton "
     "(resources "
      "((callback Hide_unhide) "
       "(labelString Unhide))))) "
   "(constraint R0C2A0) "
   "(child "
    "(LabelButton "
     "(resources "
      "((callback Hide_close) "
       "(accelerator Alt\\ c) "
       "(labelString Close))))) "
   "(constraint R0C3A0) "
   "(child "
    "(LabelButton "
     "(resources "
      "((callback Hide_help) "
       "(accelerator Alt\\ h) "
       "(labelString Help))))) "
   "(constraint R0C4A0))) "
 "(constraint R3C0A0))";
