/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1994 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef _UTILITY_H_
#define _UTILITY_H_

#include <sys/par.h>
#include <sys/atomic_ops.h>

/*****************************************************************************/
/*
 * These routines implement the checks necessary to determine if the
 * given process is actually "runnable" or not. Just because a process is
 * on the run queue does not mean that the current processor (or any
 * processor) can run it. Being on the run queue means that the process is
 * runnable from *it's own* point of view, not the kernel's.
 */
/*****************************************************************************/

/*
 * Base scheduling discipline (if p, then kt should be p's underlying kthread).
 */
#define base_sched(kt) (!kt->k_sqself || kt == curthreadp)


extern int _condRunchecks(kthread_t *);

/* Ok to run given kt? If kt is a process, p shd. be == kt->k_uproc. */
#define runcond_ok(kt)	\
	((kt)->k_runcond ? _condRunchecks((kt)) : base_sched((kt)))


/*****************************************************************************/
/*
 * Atomic increment/decrement.
 */
/*****************************************************************************/




extern zone_t *job_zone_public;
extern zone_t *job_zone_private;
#endif /* _UTILITY_H_ */
