/* Layout file created by tedit */

#ifndef TUDONTAUTOINCLUDE
#include "tuFrame.h"
#include "tuGap.h"
#include "tuLabelButton.h"
#include "tuRowColumn.h"
#endif /* TUDONTAUTOINCLUDE */

#ifdef TUREGISTERGADGETS
#undef TUREGISTERGADGETS
#endif /* TUREGISTERGADGETS */
#define TUREGISTERGADGETS { \
    tuFrame::registerForPickling(); \
    tuGap::registerForPickling(); \
    tuLabelButton::registerForPickling(); \
    tuRowColumn::registerForPickling(); \
}

static char* layoutstr =
"(RowColumn "
 "(rows 2) "
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
  "(Frame "
   "(instanceName mapparent) "
   "(resources "
    "((frame Innie))))) "
 "(constraint R0C0A0) "
 "(child "
  "(RowColumn "
   "(rows 1) "
   "(cols 3) "
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
    "(LabelButton "
     "(resources "
      "((callback Map_close) "
       "(accelerator Alt\\ c) "
       "(labelString Close))))) "
   "(constraint R0C1A0) "
   "(child "
    "(LabelButton "
     "(resources "
      "((callback Map_help) "
       "(accelerator Alt\\ h) "
       "(labelString Help))))) "
   "(constraint R0C2A0))) "
 "(constraint R1C0A0))";
