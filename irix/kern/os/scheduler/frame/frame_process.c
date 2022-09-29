/**************************************************************************
 *									  *
 *	       Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#if defined(EVEREST) || defined(SN0) || defined(IP30)

/*
 * Frame Scheduler - Thread Management
 */

#include <sys/types.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/kmem.h>
#include <sys/runq.h>
#include <sys/sysmp.h>
#include <sys/schedctl.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/pda.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/prctl.h>
#include <sys/atomic_ops.h>
#include <sys/frs.h>

#include "os/proc/pproc_private.h"

#include "frame_barrier.h"
#include "frame_semabarrier.h"
#include "frame_base.h"
#include "frame_cpu.h"
#include "frame_process.h"
#include "frame_procd.h"
#include "frame_minor.h"
#include "frame_major.h"
#include "frame_sched.h"
#include "frame_debug.h"

/*
 * The Global Data Structure View:
 *
 *					     Minor Frames
 *                                                -----  HEAD   DLL OF Procd's 
 *					       -->|   |-->[ ]-->[ ]-->[ ]-->
 *			   Minor Pointers     /   |   |   [ ]<--[ ]<--[ ]<--
 * -------------    --------    ----	     /    -----
 * | Frame     |--->|Major |--->|  |--------/ frs_minor_t   frs_prod_t
 * | Scheduler |    |Frame |    |--|              -----
 * -------------    |      |    |  |--------------|   |-->[ ]-->[ ]-->[ ]-->
 *                  |      |    |--|              |   |   [*]<--[ ]<--[ ]<--
 *                  --------    |  |              -----    \
 *                              |--|			    \
 *                frs_major_t   |  | etc.                    --> *uthread_t
 *                              |--|
 *			      
 */

/*
 * frs_thread_ref_acquire: Acquires a reference on a thread
 *
 * Successful return guarantees that the target uthread
 * its process will not exit.
 */
uthread_t*
frs_thread_ref_acquire(frs_threadid_t *thread_id)
{
	uthread_t *ut;
	proc_proxy_t *prxy;
	vproc_t *vpr;
	proc_t *p;

	ASSERT (thread_id->pid);

	if (thread_id->pid < 2)
		return (NULL);

	if ((vpr = VPROC_LOOKUP(thread_id->pid)) == NULL)
		return (NULL);

	VPROC_GET_PROC(vpr, &p);
	prxy = &p->p_proxy;

	if (thread_id->utid == UT_ID_NULL) {
		uscan_hold(prxy);
		ASSERT(prxy->prxy_nthreads == 1);
		ut = prxy_to_thread(prxy);
		ASSERT (ut);
	} else {
		if ((ut = uthread_lookup(prxy, thread_id->utid)) == NULL)
			VPROC_RELE(vpr);
	}

	return (ut);
}

/* frs_thread_ref_release: Drop thread reference
 *
 * Callers of this function must have a reference on the target thread
 */
void
frs_thread_ref_release(uthread_t *ut)
{
	ASSERT (ut);
	uscan_rele(ut->ut_pproxy);
	VPROC_RELE(UT_TO_VPROC(ut));
}

/*
 * frs_thread_linkfrs: Link a thread to a procd
 *
 * Callers of this function must have a reference on the target thread
 */
int
frs_thread_linkfrs(uthread_t *ut, frs_fsched_t *frs, int *refs)
{
	int s;
	
	ASSERT (ut != NULL);
	ASSERT (frs != NULL);
	ASSERT (refs != NULL);

	s = frs_thread_lock(ut);

	if (ut->ut_frsrefs == 0) {
		/*
		 * First reference:
		 * This thread cannot possibly be frame scheduled.
		 */
		ASSERT (!_isFrameScheduled(ut));
			
		/*
		 * It can't be a controller for another frs
		 */
		if (_isFRSController(ut)) {
			frs_thread_unlock(ut, s);
			return (FRS_ERROR_PROCESS_ISCONTROLLER);
		}

		/*
		 * It cannot be a mustrun thread
		 */
		if (KT_ISMR(UT_TO_KT(ut))) {
			frs_thread_unlock(ut, s);
			return (FRS_ERROR_PROCESS_ISMUSTRUN);
		}

		_initFrameScheduled(ut);

		/*
		 * And we link this thread to its frame scheduler
		 */
		ut->ut_frs = frs;
	} else {
		/*
		 * This is just another reference.
		 * This thread must already be framescheduled.
		 */
		ASSERT (_isFrameScheduled(ut));

		/*
		 * And we verify this is indeed the same frame scheduler.
		 */
		if (ut->ut_frs != (void*)frs) {
			frs_thread_unlock(ut, s);
			return (FRS_ERROR_PROCESS_ISFSCHED);
		}
	}

	/*
	 * Now we incr the threads local reference count
	 */
	*refs = ++ut->ut_frsrefs;

	frs_fsched_incrref(frs);

	frs_thread_unlock(ut, s);
	return (0);
}
	
/*
 * frs_thread_unlinkfrs: unlink a thread from a procd
 *
 * Callers of this function must have a reference on the target thread
 */
void
frs_thread_unlinkfrs(uthread_t *ut, frs_procd_t *procd)
{
	int s;
	int exiting;
	int joined;

	ASSERT (ut);

	s = frs_thread_lock(ut);

	ASSERT (_isFrameScheduled(ut));
	ASSERT (ut->ut_frsrefs > 0);

	/*
	 * First we decr the threads local reference count
	 */
	ut->ut_frsrefs--;

	frs_fsched_decrref(ut->ut_frs);

	if (ut->ut_frsrefs == 0) {
		/*
		 * This was the last reference
		 *
		 * Fully disconnect this thread from the frame scheduler
		 */
		exiting = _isFSExiting(ut);
		joined = _isFSJoin(ut);
		_endFrameScheduled(ut);

		ASSERT (ut->ut_frs != NULL);
		ut->ut_frs = NULL;

		frs_thread_unlock(ut, s);

		if (!joined)
			return;

		/*
		 * At this point the uthread is no longer frame scheduled,
		 * but the FRS may still be active.  We need to get this
		 * uthread off of the frame scheduler's isolated cpu.
		 *
		 * We temporarily change the uthreads mustrun attribute
		 * to cpu 0.  This way if the thread should block in
		 * mp_runthread_anywhere() before the operation completes,
		 * it will be rescheduled on cpu 0 when it is unblocked.
		 * See bug #623306.
		 */
		if (ut == curuthread) {
			kthread_t *kt = UT_TO_KT(ut);
			s = kt_lock(kt);
			if (kt->k_mustrun != PDA_RUNANYWHERE) {
				kt->k_mustrun = 0;
				kt->k_binding = 0;
			}
			kt_unlock(kt, s);
		}

		/*
		 * We only need to return the thread to normal scheduling
		 * if it is not exiting.  Otherwise, don't bother.
		 */
		if (!exiting) {
			mp_runthread_anywhere(UT_TO_KT(ut));

			setinfoRunq(UT_TO_KT(ut),
				    &ut->ut_pproxy->prxy_sched,
				    RQRTPRI,
				    0);
		}

		/*
		 * We have to signal the transition.
		 * Full disconnection.
		 */
		if (procd != NULL)
			frs_procd_signalunframesched(procd);

	} else {
		/*
		 * Just a ref decr. The thread is still
		 * enqueued in some other frs queue.
		 */
		frs_thread_unlock(ut, s);

		/*
		 * We have to signal the transition.
		 * Just a non-disconnecting dequeue.
		 */
		if (procd != NULL)
			frs_procd_signaldequeue(procd);
	}
}

/*
 * frs_thread_controller_access:
 *
 * Check if thread qualifies to be an FRS controller:
 *
 * - If the thread is a pthread, then it must be system scope
 * - cannot be a frame scheduled thread
 * - cannot already be a controller for another frs
 * - cannot be a mustrun thread
 *
 * If all the above criteria is met, then mark the thread as a controller
 *
 * Callers of this function must have a reference on the target thread
 */
int
frs_thread_controller_access(uthread_t *ut)
{
	int s;

	/*
	 * If the thread is a pthread, then it must be system scope
	 */
	if ((ut->ut_flags & UT_PTHREAD) && (ut->ut_flags & UT_PTPSCOPE))
		return (FRS_ERROR_PROCESS_NPERM);

	s = frs_thread_lock(ut);
	
	/*
	 *  It cannot be a frame scheduled thread
	 */
	if (_isFrameScheduled(ut)) {
		frs_thread_unlock(ut, s);
		return (FRS_ERROR_PROCESS_ISFSCHED);
	}    

	/*
	 * It can't be a controller for another frs
	 */
	if (_isFRSController(ut)) {
		frs_thread_unlock(ut, s);
		return (FRS_ERROR_PROCESS_ISCONTROLLER);
	}
	
	/*
	 * It cannot be a mustrun thread.
	 */
	if (KT_ISMR(UT_TO_KT(ut))) {
		frs_thread_unlock(ut, s);
		return (FRS_ERROR_PROCESS_ISMUSTRUN);
	}

	/*
	 * The thread has met the requirements,
	 * we set it to be a Controller Thread.
	 */
	_makeFRSController(ut);

	frs_thread_unlock(ut, s);

	return (0);
}

/*
 * frs_thread_access: Check if thread qualifies to access the frs
 *
 * Callers of this function must have a reference on the target thread
 */
int
frs_thread_access(uthread_t *ut)
{
	int ret = 0;

	/*
	 * If the thread is a pthread, then it must be system scope
	 */
	if ((ut->ut_flags & UT_PTHREAD) && (ut->ut_flags & UT_PTPSCOPE))
		ret = FRS_ERROR_PROCESS_NPERM;

	return (ret);
}

/*
 * frs_thread_controller_unset: unset a controller thread
 *
 * Callers of this function must have a reference on the target thread
 */
void
frs_thread_controller_unset(uthread_t *ut)
{
	int s;
	
	s = frs_thread_lock(ut);
	_unFRSController(ut);
	frs_thread_unlock(ut, s);
}

/*
 * Make link from current control thread to frs
 */
void
frs_thread_controller_linkme(frs_fsched_t *frs)
{
	uthread_t* ut = curuthread;
	int s;
	
	ASSERT (ut != 0);
	ASSERT (frs != 0);

	/*
	 * XXX Locking could be better here...
	 * XXX need lock nesting macros
	 */

        s = frs_fsched_statelock(frs);
        frs->control_thread = ut;
	frs->control_id.pid = UT_TO_PROC(ut)->p_pid;
	/*
	 * Typically the utid is only set when the application is using
	 * pthreads, but for contoller threads we *always* set the utid.
	 */
	frs->control_id.utid = ut->ut_id;
        frs_fsched_stateunlock(frs, s);
	       
	s = frs_thread_lock(ut);
	ASSERT (_isFRSController(ut));
	frs_fsched_incrref(frs);
	ut->ut_frs = frs;
	frs_thread_unlock(ut, s);
}

/*
 * Remove link from current control thread to frs
 */
void
frs_thread_controller_unlinkme(frs_fsched_t *frs)
{
	uthread_t* ut = curuthread;
	int s;
	
	ASSERT (ut != 0);
	ASSERT (frs != 0);

	/*
	 * XXX Locking could be better here...
	 * XXX need lock nesting macros
	 */

        s = frs_fsched_statelock(frs);
	ASSERT(frs->control_thread == ut);
        ASSERT(frs->control_id.pid == ut->ut_proc->p_pid);
        frs->control_thread = NULL;
	frs_threadid_clear(&frs->control_id);
        frs_fsched_stateunlock(frs, s);

	s = frs_thread_lock(ut);
	ASSERT (_isFRSController(ut));
	ASSERT (ut->ut_frs == frs);
	ut->ut_frs = NULL;
	_endFRSController(ut);
	frs_fsched_decrref(frs);
	frs_thread_unlock(ut, s);
}

uthread_t*
frs_thread_controller_getthread(frs_fsched_t *frs)
{
        int s;
        uthread_t* ut;

        s = frs_fsched_statelock(frs);
        ut = frs->control_thread;
        frs_fsched_stateunlock(frs, s);
        return (ut);
}	
	
/*
 * Get a reference to the frs from a controller thread.
 */
frs_fsched_t*
frs_thread_controller_getfrs(frs_threadid_t *thread_id, int *retp)
{
	frs_fsched_t *frs = NULL;
	uthread_t *ut;
	int s;

	ASSERT (retp != NULL);

	ut = frs_thread_ref_acquire(thread_id);
	if (ut == NULL) {
		*retp = FRS_ERROR_PROCESS_NEXIST;
		return (NULL);
	}

	s = frs_thread_lock(ut);

	if (!_isFRSController(ut)) {
		*retp = FRS_ERROR_PROCESS_NCONTROLLER;
		goto done;
	}

	if (ut->ut_frs == NULL) {
		*retp = FRS_ERROR_PROCESS_NCONTROLLER;
		goto done;
	}

	frs = ut->ut_frs;
	frs_fsched_incrref(frs);
	*retp = 0;

done:
	frs_thread_unlock(ut, s);
	frs_thread_ref_release(ut);
	return (frs);
}

/*
 * Get a reference to the FRS from the calling Frame Scheduled thread.
 */
frs_fsched_t*
frs_thread_getfrs(void)
{
	frs_fsched_t* frs = 0;
	uthread_t *ut = curuthread;
	int s;

	s = frs_thread_lock(ut);
	if (!_isFrameScheduled(ut))
		goto done;

	if (ut->ut_frs == NULL)
		goto done;

	frs = ut->ut_frs;
	frs_fsched_incrref(frs);

done:
	frs_thread_unlock(ut, s);
	return (frs);
}

void
frs_thread_sendsig(frs_threadid_t *thread_id, int sig)
{
	uthread_t *ut;

	ASSERT (thread_id->pid);

	if (sig == 0)
		return;

	if ((ut = frs_thread_ref_acquire(thread_id)) != NULL) {
		sigtouthread(ut, sig, NULL);
		frs_thread_ref_release(ut);
	}
}

/*
 * Verify thread can start frame schedulers.
 *
 * Callers of this function must have a reference on the target thread
 */
int
frs_thread_starter_test(uthread_t *ut)
{
	int s;
	
	s = frs_thread_lock(ut);
	
	/*
	 *  It cannot be a frame scheduled thread
	 */
	if (_isFrameScheduled(ut)) {
	     frs_thread_unlock(ut, s);
	     return (FRS_ERROR_PROCESS_ISFSCHED);
	}    
	
	/*
	 * It cannot be a mustrun thread.
	 */
	if (KT_ISMR(UT_TO_KT(ut))) {
		frs_thread_unlock(ut, s);
		return (FRS_ERROR_PROCESS_ISMUSTRUN);
	}

	frs_thread_unlock(ut, s);

	return (0);
}

void
frs_thread_init(uthread_t *ut)
{
	spinlock_init(&(ut)->ut_frslock,"utfrslck");
}

void
frs_thread_destroy(uthread_t *ut)
{
	spinlock_destroy(&(ut)->ut_frslock);
}

void
frs_thread_exit(uthread_t *ut)
{
	int s;

	/*
	 * If the application is sproc based, then each sproc calls
	 * this exit routine twice.
	 *
	 * If the application is pthread based, then the parent thread
	 * calls this exit routine twice, and the children only call it
	 * once.
	 *
	 * The _isFSExiting flag guarantees that the cleanup is really only
	 * executed once per uthread. We cannot rely on the _isFrameScheduled
	 * flags to determine this, since frs_thread_unlinkfrs() may not have
	 * been called for this thread (due to exit races with the controller).
	 *
	 * See frs_procd_destroy().
	 */

	s = frs_thread_lock(ut);
	if (_isFRSController(ut)) {
		frs_fsched_t *frs = ut->ut_frs;
		ASSERT (frs != NULL);
		frs_fsched_incrref(frs);
		frs_thread_unlock(ut, s);
		/*
		 * The destroy procedure gets rid of all refs,
		 * including the extra one we grabbed above.
		 */
		frs_fsched_destroy(frs);
		ASSERT (ut->ut_frs == NULL);
		ASSERT (ut->ut_frsflags == 0);
	} else if (_isFrameScheduled(ut) && !_isFSExiting(ut)) {
		frs_fsched_t *frs = ut->ut_frs;
		ASSERT (frs != NULL);
		_makeFSExiting(ut);
		frs_fsched_incrref(frs);
		frs_thread_unlock(ut, s);
		frs_major_unframesched(frs->major, ut);
		frs_fsched_decrref(frs);
	} else {
		frs_thread_unlock(ut, s);
	}
}

#endif /* EVEREST || SN0 || IP30 */
