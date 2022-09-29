/* Layout file created by tedit */

#ifndef TUDONTAUTOINCLUDE
#include "tuList.h"
#include "tuLabelButton.h"
#include "tuRowColumn.h"
#endif /* TUDONTAUTOINCLUDE */

#ifdef TUREGISTERGADGETS
#undef TUREGISTERGADGETS
#endif /* TUREGISTERGADGETS */
#define TUREGISTERGADGETS { \
    tuList::registerForPickling(); \
    tuLabelButton::registerForPickling(); \
    tuRowColumn::registerForPickling(); \
}

static char* layoutstr =
"(RowColumn "
 "(rows 2) "
 "(cols 1) "
 "(defaultAlignment 3) "
 "(child "
  "(List "
   "(instanceName helpField))) "
 "(constraint R0C0A3) "
 "(child "
  "(RowColumn "
   "(rows 1) "
   "(cols 4) "
   "(defaultAlignment 0) "
   "(resources "
    "((topInset 10) "
     "(bottomInset 10) "
     "(leftInset 20) "
     "(horizontalOffset 20) "
     "(fixedHeight True))) "
   "(child "
    "(LabelButton "
     "(instanceName pageup) "
     "(resources "
      "((callback doPageUp) "
       "(labelString Page\\ Up))))) "
   "(constraint R0C0A0) "
   "(child "
    "(LabelButton "
     "(instanceName pagedown) "
     "(resources "
      "((callback doPageDown) "
       "(labelString Page\\ Down))))) "
   "(constraint R0C1A0) "
   "(child "
    "(LabelButton "
     "(instanceName quit) "
     "(resources "
      "((callback doQuit) "
       "(labelString Quit))))) "
   "(constraint R0C2A0))) "
 "(constraint R1C0A3))";
