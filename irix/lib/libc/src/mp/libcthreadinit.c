/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.2 $"

#pragma weak libc_threadinit = _libc_threadinit

#include "synonyms.h"
#include "priv_extern.h"
#include "mplib.h"

/*
 * initialize libc to start providing thread-safe semantics.
 * This basically just turns on the globals that govern the thread calls
 *
 * This routine shouldn't be preempted!
 */
void
libc_threadinit(void)
{
	__initperthread_errno();
	__us_rsthread_stdio = 1;
	__us_rsthread_misc = 1;
	__us_rsthread_malloc = 1;
	__us_rsthread_pmq = 1;
}
