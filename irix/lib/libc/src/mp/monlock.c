/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.4 $"

#include "ulocks.h"
#include "mp_extern.h"

/* The monitor(3X) routines in libprof1 need protection! */
void
_monlock()
{
	if (__monlock)
		ussetlock(__monlock); 
}

void
_monunlock()
{
	if (__monlock)
		usunsetlock(__monlock);
}
