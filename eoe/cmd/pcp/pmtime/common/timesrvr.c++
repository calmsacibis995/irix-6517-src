
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
// $Id: timesrvr.c++,v 1.1 1999/04/30 01:44:04 kenmcd Exp $
//
#include "timesrvr.h"

#if defined(IRIX6_5) 
#include <optional_sym.h>
#endif

/*
 * register XTB data for the client
 */
void
__timsrvrRegisterClient(int fromClient, pmTime *initParams)
{
    // This will ultimately have a call to a conditional __pmTimeRegisterClient
    // symbol. The symbol will be in libpcp.
    // All of the code here will be moved into libpcp into the new symbol
    // __pmTimeRegisterClient.
#if defined(IRIX6_5)
    if (_MIPS_SYMBOL_PRESENT(__pmTimeRegisterClient))
	__pmTimeRegisterClient(fromClient, initParams);
    else {
	pmprintf("Fatal Error: pmtime attempted to use an incompatible version of libpcp\n");
	pmflush();
	exit(1);
    }
#else
    __pmTimeRegisterClient(fromClient, initParams);
#endif
}

