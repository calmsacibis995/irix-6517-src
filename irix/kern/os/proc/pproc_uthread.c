/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.11 $"

#include <sys/types.h>
#include <sys/proc.h>
#include <ksys/vproc.h>
#include <sys/errno.h>
#include <sys/prctl.h>
#include <sys/poll.h>
#include <sys/systm.h>
#include "fs/procfs/prdata.h"
#include "pproc.h"
#include "pproc_private.h"

/*
 * Set/clear flag bit in all uthreads.
 */
void
pproc_flag_threads(
	bhv_desc_t *bhv,
	uint_t flag,
	int on)
{
	proc_t	*p = BHV_TO_PROC(bhv);
	proc_proxy_t *prxy;
	uthread_t *ut;
	int s;

	ASSERT(p != NULL);

	prxy = &p->p_proxy;

	uscan_access(prxy);

	uscan_forloop(prxy, ut) {
		s = ut_lock(ut);
		if (on == UTHREAD_FLAG_ON)
			ut->ut_flags |= flag;
		else
			ut->ut_flags &= ~flag;
		ut_unlock(ut, s);
	}

	uscan_unlock(prxy);
}

/*
 * Stop all threads.
 */
void
pproc_prstop_threads(bhv_desc_t	*bhv)
{
	proc_t	*p;

	p = BHV_TO_PROC(bhv);
	prstop_proc(p, 1);

}

/*
 * Wait for all uthreads to stop.
 */
int
pproc_prwstop_threads(
	bhv_desc_t	*bhv,
	struct prnode	*pnp)
{
	proc_t	*p;
	int error;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	error = prwstop_proc(p, pnp);
	return(error);
}


/*
 * Start all threads.
 */
void
pproc_prstart_threads(bhv_desc_t *bhv)
{
	proc_t	*p;
	proc_proxy_t *prxy;
	uthread_t *ut;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	prxy = &p->p_proxy;

	uscan_access(prxy);

	uscan_forloop(prxy, ut) {
		pr_start(ut);
	}

	uscan_unlock(prxy);
}

int
pproc_unblock_thread(bhv_desc_t *bhv, ushort_t id)
{
	proc_t	*p;
	proc_proxy_t *prxy;
	uthread_t *ut;
	int s;
	int rv = 0;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	prxy = &p->p_proxy;
	uscan_access(prxy);

	uscan_forloop(prxy, ut) {
		if (ut->ut_id == id && !(ut->ut_flags & UT_INACTIVE)) {
			s = ut_lock(ut);
			blockset(ut, 1, 1, s);	/* unlocks ut */
			rv = 1;
			break;
		}
	}

	uscan_unlock(prxy);

	return rv;
}

/*
 * Send a signal to a particular thread.
 */
int
pproc_sig_thread(bhv_desc_t *bhv, ushort_t id, int signal)
{
	proc_t	*p;
	proc_proxy_t *prxy;
	uthread_t *ut;
	int rv = 0;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	prxy = &p->p_proxy;
	uscan_access(prxy);

	/*
	 * XXX	This code relies on the fact that the target pthread won't get
	 * XXX	switched to another uthread.  If this changes, we'll have to
	 * XXX	hold sigvec lock and uthread lock, and call sigtouthread_common
	 * XXX	directly.
	 */

	uscan_forloop(prxy, ut) {
		if (ut->ut_id == id && !(ut->ut_flags & UT_INACTIVE)) {
			sigtouthread(ut, signal, (k_siginfo_t *)NULL);
			rv = 1;
			break;
		}
	}

	uscan_unlock(prxy);

	return rv;
}

int
pproc_thread_state(
	bhv_desc_t	*bhv,
	int		state)
{
	proc_t	*p;
	proc_proxy_t *prxy;
	uint	was;
        int     s;

	ASSERT(state == THRD_EXIT || state == THRD_EXEC);

	p = BHV_TO_PROC(bhv);
	prxy = &p->p_proxy;

	ASSERT(IS_THREADED(prxy));

	s = prxy_lock(prxy);
	was = prxy->prxy_flags;
	if (was & (PRXY_EXIT|PRXY_EXEC)) {
		prxy_unlock(prxy, s);
		return was;
	} 
	prxy->prxy_flags = was | (state == THRD_EXEC ? PRXY_EXEC : PRXY_EXIT);
	prxy_unlock(prxy, s);


	/*
	 * This will send kill signal to all the uthreads on
	 * this cell and wait for them to exit.
	 */
	uthread_reap();

	if (state == THRD_EXEC) {
		detachpshare(prxy);
		prxy_flagclr(prxy, PRXY_EXEC);
	}

	return 0;
}

/*
 * Send a signal to a particular thread.
 */
void
pproc_poll_wakeup_thread(bhv_desc_t *bhv, ushort_t id, ushort_t rotor)
{
	proc_t	*p;
	proc_proxy_t *prxy;
	uthread_t *ut;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	prxy = &p->p_proxy;
	uscan_access(prxy);

	uscan_forloop(prxy, ut) {
		if (ut->ut_id == id && !(ut->ut_flags & UT_INACTIVE)) {
			pollwakeup_thread (ut, rotor);
			break;
		}
	}

	uscan_unlock(prxy);

	return;
}

