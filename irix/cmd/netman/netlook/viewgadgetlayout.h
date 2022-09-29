/* Layout file created by tedit */

#ifndef TUDONTAUTOINCLUDE
#include "tuFrame.h"
#include "tuScrollBar.h"
#include "tuRowColumn.h"
#endif /* TUDONTAUTOINCLUDE */

#ifdef TUREGISTERGADGETS
#undef TUREGISTERGADGETS
#endif /* TUREGISTERGADGETS */
#define TUREGISTERGADGETS { \
    tuFrame::registerForPickling(); \
    tuScrollBar::registerForPickling(); \
    tuRowColumn::registerForPickling(); \
}

static char* layoutstr =
"(RowColumn "
 "(rows 2) "
 "(cols 1) "
 "(defaultAlignment 0) "
 "(child "
  "(Frame "
   "(instanceName menuparent) "
   "(resources "
    "((frame Outie))))) "
 "(constraint R0C0A0) "
 "(child "
  "(RowColumn "
   "(rows 2) "
   "(cols 3) "
   "(defaultAlignment 0) "
   "(child "
    "(ScrollBar "
     "(instanceName ScrollZ) "
     "(orientation vertical) "
     "(resources "
      "((dragCallBack View_moveZ) "
       "(pageIncCallBack View_pageIncZ) "
       "(pageDecCallBack View_pageDecZ) "
       "(incCallBack View_incZ) "
       "(decCallBack View_decZ))))) "
   "(constraint R0C0A11) "
   "(child "
    "(Frame "
     "(instanceName GLparent))) "
   "(constraint R0C1A16) "
   "(child "
    "(ScrollBar "
     "(instanceName ScrollY) "
     "(orientation vertical) "
     "(resources "
      "((dragCallBack View_moveY) "
       "(pageIncCallBack View_pageIncY) "
       "(pageDecCallBack View_pageDecY) "
       "(incCallBack View_incY) "
       "(decCallBack View_decY))))) "
   "(constraint R0C2A18) "
   "(child "
    "(ScrollBar "
     "(instanceName ScrollX) "
     "(orientation horizontal) "
     "(resources "
      "((dragCallBack View_moveX) "
       "(pageIncCallBack View_pageIncX) "
       "(pageDecCallBack View_pageDecX) "
       "(incCallBack View_incX) "
       "(decCallBack View_decX))))) "
   "(constraint R1C1A11))) "
 "(constraint R1C0A0))";
