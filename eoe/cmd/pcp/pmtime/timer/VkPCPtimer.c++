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
// $Id: VkPCPtimer.c++,v 1.3 1997/08/21 04:42:51 markgw Exp $
//
#include "VkPCPtimer.h"
#include <Vk/VkApp.h>
#include <Vk/VkCallbackList.h>

#include <time.h>
#include <stdio.h>
#include "pmapi.h"
#include "impl.h"

const char * const VkPCPtimer::timerCallback = "timerCallback";

VkPCPtimer::VkPCPtimer() : VkCallbackObject()
{
    _id       = NULL;
}

VkPCPtimer::~VkPCPtimer ( )
{
    stop();
}

void
VkPCPtimer::start(int milliseconds)
{
    if (milliseconds < 0)
	milliseconds = 0;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_TIMECONTROL)
	fprintf(stderr, "VkPCPtimer::start(%dms)\n", milliseconds);
#endif

    // If a previous callback is still in effect, remove it
    if (_id) {
	XtRemoveTimeOut(_id);
	_id = NULL;
    }

    if (milliseconds == 0) {
	callCallbacks(timerCallback, NULL);
    }
    else {
	// Register a function to be called
	_id = XtAppAddTimeOut(theApplication->appContext(), milliseconds,
		&VkPCPtimer::tickCallback, (XtPointer)this);
    }
}

void
VkPCPtimer::stop()
{
    // Remove the current timeout function, if any
    if ( _id )
	XtRemoveTimeOut(_id);
    _id = NULL;
}

void
VkPCPtimer::tickCallback(XtPointer clientData, XtIntervalId *)
{
    // Get the object pointer and call the corresponding tick function
    VkPCPtimer *obj = (VkPCPtimer *) clientData;
    obj->callCallbacks(timerCallback, NULL);
    obj->tick();
}

void
VkPCPtimer::tick()
{
    // virtual
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_TIMECONTROL)
	fprintf(stderr, "VkPCPtimer::tick()\n");
#endif
}

