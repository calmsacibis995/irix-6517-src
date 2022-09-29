/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident "$Revision: 3.87 $"

#include "sys/types.h"
#include "sys/kthread.h"
#include "sys/sema.h"
#include "sys/debug.h"

/* svs for sleep()ing */

#ifdef SABLE_RTL
#define NUMSLPS		64
#define SLPHASH(x, y)	(((x) >> 6) + y)
#else
#define	NUMSLPS		512
#define SLPHASH(x, y)	(((x) >> 9) + y)
#endif

sv_t			slpsvs[NUMSLPS];

#define slphash_func(x) SLPHASH(SLPHASH(SLPHASH(x, x), x), x)
#define slphash(X)	(&slpsvs[slphash_func((__psunsigned_t)(X))&(NUMSLPS-1)])

sv_t *slpsv_end;

/*
 * sleep
 * returns: 0 if normal wakeup
 *	    1 if interrupted
 */
int
sleep(void *addr, int pri)
{
	register int retval;
	kthread_t *kt = private.p_curkthread;

	ASSERT(kt->k_w2chan == 0);
	kt->k_w2chan = (caddr_t)addr;
	retval = sv_wait_sig(slphash(addr), pri, 0, 0);
	kt->k_w2chan = 0;
	return !!retval;
}

/* ARGSUSED1 */
void
wakeup(void *addr)
{
	sv_broadcast(slphash(addr));
}

/* init arbitrary sleep sync vars... called very early in main.c */
void
slpinit(void)
{
	register int	i;
	for (i = 0; i < NUMSLPS; i++)
		init_sv(&slpsvs[i], 0, "slp", i);
	slpsv_end = &slpsvs[i];
}
