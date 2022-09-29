/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* NOTES
 *
 * This module implements debugging support mostly for the DEBUG mode.
 */

#include "common.h"
#include "pt.h"
#include "ptdbg.h"
#include "vp.h"
#include "sys.h"


static ulong_t unique_identity(void);
static void splhi(void);
static void spl0(void);

/* rld->pthread callbacks
 */
jt_rld_pt_t	jt_rld_pt = {
	JT_RLD_PT_VERSION,
	{
		(jt_func_t)unique_identity,
		(jt_func_t)splhi,
		(jt_func_t)spl0,
	}
};

jt_pt_ss_t	*jt_pt_ss;		/* pt->SpeedShop callbacks */


/*
 * unique_identity()
 *
 * Provide a unique name for locking purposes regardless of context.
 * We assume a 32bit cast of a pointer is sufficient.
 */
static ulong_t
unique_identity(void)
{
	pt_t	*pt_self = PT;

	ASSERT(pt_self || sched_entered());
	return ((pt_self) ? (ulong_t)pt_self : (ulong_t)VP);
}


static void splhi(void)
{
	sched_enter();
}


static void spl0(void)
{
	sched_leave();
}


/*
 * ss_thread_init(in, out)
 *
 * Special name known by SpeedShop and called to provide pthreads
 * with performance callbacks table.
 */
void
ss_thread_init(jt_pt_ss_t *in, void **out) 
{
	if (in->jt_vers != JT_PT_SS_VERSION) {
		panic("ss_thread_init", "bad trace version");
	}
	jt_pt_ss = in;

	*out = 0;
}
