/*
 * Copyright 1997, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

//
// $Id: unimplemented.c++,v 1.3 1997/08/21 04:32:53 markgw Exp $
//

///////////////////////////////////////////////////////////////////

// This file contains a single global function "VkUnimplemented".
// This function supports development using Fix+Continue.
// You can simply set a breakpoint in this function to stop in
// all unimplemented callback functions in a program.
///////////////////////////////////////////////////////////////////

#include <iostream.h>

#include <Xm/Xm.h>
#include "pmapi.h"
#include "impl.h"

void VkUnimplemented(Widget w, const char *str)
{
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL) {
    cerr << "pmtime unimplemented : member function " << str << "()  was invoked";
    if ( w )
        cerr << " by " << XtName(w);
    cerr << endl << flush; 
}
#endif
}
