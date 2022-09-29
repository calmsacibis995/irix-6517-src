/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* Copyright 1988 Silicon Graphics, Inc. All rights reserved.	*/

/*LINTLIBRARY*/
/*
 * Suspend the process for `sleep_tm' seconds - using the sginap
 * system call.  The process may be aroused during the sleep by any
 * caught signal.  The (unsigned) difference between the requested
 * sleep time and the actual sleep time is returned to the user.
 */
#ifdef __STDC__
	#pragma weak sleep = _sleep
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <limits.h>
#include <unistd.h>		/* for prototyping */
#include "mplib.h"

#define HALF_SEC	(clk_tck/2)
#define ROUND(x)	((x%clk_tck < HALF_SEC) ? x/clk_tck : (x/clk_tck)+1)
static long clk_tck _INITBSS;

unsigned
sleep(unsigned sleep_tm)
{
	long early;
	extern long __sginap(long);

	if (clk_tck == 0)
		clk_tck = CLK_TCK; /* CLK_TCK is a system call */
	if (sleep_tm == 0) {
		MTLIB_CNCL_TEST();
		return(0);
	}
	/*
	 * Add one, since sginap sleeps until that many clock ticks pass.
	 * In the worst case, the sleep would be 1-epsilon tick short.
	 */
	MTLIB_BLOCK_CNCL_VAL_1( early, __sginap((long)sleep_tm * clk_tck + 1) );
	return((unsigned)(ROUND(early)));
}
