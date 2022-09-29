
///////////////////////////////////////////////////////////////////

// This file contains a single global function "VkUnimplemented".
// This function supports development using Fix+Continue.
// You can simply set a breakpoint in this function to stop in
// all unimplemented callback functions in a program.
///////////////////////////////////////////////////////////////////

#include <iostream.h>

#include <Xm/Xm.h>

void VkUnimplemented(Widget w, const char *str)
{
    cerr << "The member function " << str << "()  was invoked";
    if ( w )
        cerr << " by " << XtName(w);
    cerr << endl << flush; 
}

