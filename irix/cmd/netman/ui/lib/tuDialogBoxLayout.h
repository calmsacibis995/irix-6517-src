/* Layout file created by tedit */

#ifndef TUDONTAUTOINCLUDE
#include "tuBitmapTile.h"
#include "tuRowColumn.h"
#include "tuFrame.h"
#include "tuGap.h"
#endif /* TUDONTAUTOINCLUDE */

#ifdef TUREGISTERGADGETS
#undef TUREGISTERGADGETS
#endif /* TUREGISTERGADGETS */
#define TUREGISTERGADGETS { \
    tuBitmapTile::registerForPickling(); \
    tuRowColumn::registerForPickling(); \
    tuFrame::registerForPickling(); \
    tuGap::registerForPickling(); \
}

static char* layoutstr =
"(RowColumn "
 "(rows 2) "
 "(cols 1) "
 "(defaultAlignment 0) "
 "(resources "
  "((topInset 8) "
   "(bottomInset 8) "
   "(leftInset 8) "
   "(rightInset 8) "
   "(horizontalOffset 8) "
   "(verticalOffset 12))) "
 "(child "
  "(RowColumn "
   "(rows 1) "
   "(cols 2) "
   "(defaultAlignment 0) "
   "(resources "
    "((horizontalOffset 4))) "
   "(child "
    "(BitmapTile "
     "(instanceName _bitmap) "
     "(resources "
      "((bitmap ))))) "
   "(constraint R0C0A0) "
   "(child "
    "(Frame "
     "(child "
      "(RowColumn "
       "(instanceName _text_container) "
       "(rows 1) "
       "(cols 1) "
       "(growdir vertical) "
       "(defaultAlignment 0) "
       "(resources "
        "((topInset 8) "
         "(bottomInset 8) "
         "(leftInset 12) "
         "(rightInset 12) "
         "(verticalOffset 8))))))) "
   "(constraint R0C1A0))) "
 "(constraint R0C0A0) "
 "(child "
  "(RowColumn "
   "(instanceName _button_container) "
   "(rows 1) "
   "(cols 1) "
   "(defaultAlignment 0) "
   "(resources "
    "((horizontalOffset 8))) "
   "(child "
    "(Gap "
     "(orientation horizontal) "
     "(resources "
      "((minWidth 50) "
       "(minHeight 32))))) "
   "(constraint R0C0A0))) "
 "(constraint R1C0A0)) ";
