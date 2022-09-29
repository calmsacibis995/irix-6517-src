/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#if defined(EVEREST) || defined(SN0) || defined(IP30)

/*
 * Frame Scheduler Implementation
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
#include <sys/prctl.h>
#include <sys/systm.h>
#include <sys/atomic_ops.h>
#include <sys/rtmon.h>
#include <sys/frs.h>
#include <sys/kthread.h>
#include <sys/idbg.h>

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


/**************************************************************************
 *                            Forward Declarations                        *
 **************************************************************************/

static int
frs_fsched_setup_master(frs_fsched_t* frs,
			frs_fsched_info_t* info,
			frs_intr_info_t* intr_info);
static int
frs_fsched_setup_slave(frs_fsched_t* frs,
		       frs_fsched_info_t* info);

static void
frs_fsched_free_resources(frs_fsched_t* frs);
static void
frs_fsched_destroy_master(frs_fsched_t* frs);
static void
frs_fsched_destroy_slave(frs_fsched_t* frs);
static int
frs_fsched_state_transition(frs_fsched_t* frs,
                            frs_state_t fromstate,
                            frs_state_t tostate);
static int
frs_fsched_state_transition_all(frs_fsched_t* frs,
				frs_state_t fromstate,
				frs_state_t tostate);

static frs_state_t
frs_fsched_state_get(frs_fsched_t* frs);

static int
frs_fsched_state_verify_canenqueue(frs_fsched_t* frs);
static int
frs_fsched_state_verify_canaccessqueues(frs_fsched_t* frs);
static int
frs_fsched_state_verify_canstop(frs_fsched_t* frs);
static int
frs_fsched_state_verify_canresume(frs_fsched_t* frs);
static int
frs_fsched_state_initialsync(frs_fsched_t* frs);
static int
frs_fsched_state_verify_canaccessattr(frs_fsched_t* frs);


/**************************************************************************
 *                            frs_fsched_t                                *
 **************************************************************************/

frs_fsched_t*
frs_fsched_create(frs_fsched_info_t* info,
		  frs_intr_info_t* intr_info,
		  int* retp)
{
	frs_fsched_t* frs;
	cpuid_t cpu;

        ASSERT (info != NULL);
        ASSERT (retp != NULL);

	cpu = info->cpu;

        /*
         * We check if the requested cpu is available,
         * and if so we reserve it.
         */
        if ((*retp = frs_cpu_reservepda(cpu)) != 0)
                return (NULL);

	/*
	 * Allocate FRS object
	 */ 
        frs = (frs_fsched_t*) kmem_zalloc(sizeof(frs_fsched_t),
					  KM_CACHEALIGN | KM_SLEEP);
        if (frs == NULL) {
                frs_cpu_unreservepda(cpu);
                *retp = ENOMEM;
                return (NULL);
        }

	bzero(frs, sizeof(frs_fsched_t));

	/*
	 * Initialize local synchronizers
	 */
        spinlock_init(&frs->state_lock, "frs_state_lock");
	init_sema(&frs->term_sema, 0, "frs_term_sema", cpu);

	/*
         * We split initialization into two exclusive sections:
	 * one for masters, and the other for slaves.
	 */
	if (info->controller == FRS_SYNC_MASTER)
		*retp = frs_fsched_setup_master(frs, info, intr_info);
        else
		*retp = frs_fsched_setup_slave(frs, info);

        if (*retp != 0) {
                frs_cpu_unreservepda(cpu);
                frs_fsched_free_resources(frs);
                return (NULL);
        }

        /*
         * The barrier's number of clients minus the barrier's in_counter
         * represents the reference count.
         */
        frs->references = semabarrier_create(0);
        if (frs->references == NULL) {
                frs_cpu_unreservepda(cpu);
                frs_fsched_free_resources(frs);
                *retp = ENOMEM;
                return (NULL);
        }

        frs->state = FRS_STATE_CREATED;
        frs->prev_state = FRS_STATE_ANY;
        frs->cpu = cpu;

        frs->control_thread = NULL;
	frs_threadid_clear(&frs->control_id);
        
        /*
         * Link the current thread to the FRS as a controller thread.
         * This has to be done before we make this FRS public by inserting
	 * it into the sdesc list.
	 * ref++
         */
        frs_thread_controller_linkme(frs);
        
        /*
         * CPU isolation
         */
        if ((*retp = frs_cpu_isolate(cpu)) != 0) {
                frs_thread_controller_unlinkme(frs);
                frs_cpu_unreservepda(cpu);
                frs_fsched_free_resources(frs);
                return (NULL);                      
        }

        /*
         * Link the reserved cpu/pda to this frs.
         * The reservation is cleared.
	 * ref++
         */
        if ((*retp = frs_cpu_linkpda(cpu, frs)) != 0) {
                frs_cpu_unisolate(cpu);
                frs_thread_controller_unlinkme(frs);
                frs_cpu_unreservepda(cpu);
                frs_fsched_free_resources(frs);
                return (NULL);     
        }

	if (_isMasterFrs(frs)) {
                /*
                 * Insert this frs in sdesc.
                 */
                if ((*retp = frs_fsched_sdesc_insertmaster(frs)) != 0) {
                        frs_cpu_unlinkpda(cpu);
                        frs_cpu_unisolate(cpu);
                        frs_thread_controller_unlinkme(frs);
                        frs_fsched_free_resources(frs);
                        return (NULL);
                }
	} else {
                /*
                 * Insert this frs in slave list.
                 */
                if ((*retp = frs_fsched_sdesc_insertslave(frs)) != 0) {
                        frs_cpu_unlinkpda(cpu);
                        frs_cpu_unisolate(cpu);
                        frs_thread_controller_unlinkme(frs);
                        frs_fsched_free_resources(frs);
                        return (NULL);
                }
        }

        ASSERT (*retp == 0);
        
        return (frs);
}

void
frs_fsched_destroy(frs_fsched_t* frs)
{
        syncdesc_t	*sdesc;
        int		broadcast;
        int		i;
	int		s;

        ASSERT (frs != NULL);
	sdesc = frs->sdesc;
	ASSERT (sdesc != NULL);
        
        /*
	 * All participating controllers call this routine upon exit, but
	 * only one will broadcast the termination interrupt to all FRS
	 * scheduled cpus.
	 */

	broadcast = frs_fsched_sdesc_settermination(frs);
	ASSERT (sdesc->sl_term);

        if (broadcast == 0) {

		frs_fsched_state_transition_all(sdesc->master_frs,
						FRS_STATE_ANY,
						FRS_STATE_TERMINATION);

		/*
		 * Broadcast the termination interrupt to all CPUs scheduled
		 * by this FRS, forcing them to terminate now.
		 */

		s = splhi();

		ASSERT (sdesc->master_frs);
		nested_cpuaction(sdesc->master_frs->cpu,
				 (cpuacfunc_t) frs_handle_termination,
				 0, 0, 0, 0);

		/*
		 * Reading the list of frs's is safe at this point because
		 * sl_term is set, and nobody can touch the list while this
		 * flag is set.
		 */

		for (i = 0; i < sdesc->sl_index; i++) {
			ASSERT (sdesc->sl_list[i]);
			nested_cpuaction(sdesc->sl_list[i]->cpu,
					 (cpuacfunc_t) frs_handle_termination,
					 0, 0, 0, 0);
		}

		splx(s);
	}

        /*
         * We cannot disconnect this frs from its cpu until after the
         * cpus event interrupt has been cleared.
	 */
	frs_cpu_eventintr_clearwait(frs);

        /*
         * Disconnect the controller's link to this frs
         * ref--
         */
        frs_thread_controller_unlinkme(frs);

        /*
         * We unisolate the frs's cpu
         */
        frs_cpu_unisolate(frs->cpu);

        /*
         * Disconnect the pda's link.
         * ref--
         */
        (void) frs_cpu_unlinkpda(frs->cpu);

        /*
         * Disconnect all processes from the frame scheduler.
         */
        frs_major_unframesched_allprocs(frs->major);

        if (_isMasterFrs(frs)) {
                /*
                 * MASTER
                 * destroy_master waits for all slaves to be destroyed.
                 */
                frs_fsched_destroy_master(frs);
        } else {
                /*
                 * SLAVE
                 * destroy_slave decrements the master's remote ref count
		 * at the end of its destruction procedure.
                 */
                frs_fsched_destroy_slave(frs);
        }        
       
	/*
	 * All references have been dropped
	 *
	 * Free FRS resources
	 */
        frs_fsched_free_resources(frs);
}
        
int
frs_fsched_enqueue(frs_fsched_t* frs,
                   frs_threadid_t *thread_id,
                   int minor_index,
                   int discipline)
{
	frs_procd_t	*procd;
	uthread_t	*ut;
        int		 refs;
        int		 ret;
	
	ASSERT(frs != 0);

        /*
         * Pocesses can only be enqueued while
         * the frame scheduler is in the CREATED
         * state.
         */
        if (ret = frs_fsched_state_verify_canenqueue(frs))
                goto error1;

        /*
         * Verify the specified destination minor frame process queue
         * really exists.
         */
        if (ret = frs_major_verifyminor(frs->major, minor_index))
                goto error1;

        /*
         * The base discipline is always FRS_DISC_RT
         */
        discipline |= FRS_DISC_RT;

        /*
         * A continuable process must also be overrunnable and underrun
         */
	if (discipline & FRS_DISC_CONT)
                discipline |=  FRS_DISC_OVERRUNNABLE | FRS_DISC_UNDERRUNNABLE;
	
	/*
	 * Acquire a reference for the uthread represented by thread_id
	 */
	if ((ut = frs_thread_ref_acquire(thread_id)) == NULL) {
                ret = FRS_ERROR_PROCESS_NEXIST;
                goto error1;
	}

	/*
	 * Make sure the thread qualifies for FRS access
	 */
	if (ret = frs_thread_access(ut))
		goto error2;

	/*
	 * Link the thread to the FRS
	 */
        if (ret = frs_thread_linkfrs(ut, frs, &refs))
                goto error2;

	/*
	 * we create procd
	 */
        if ((procd = frs_procd_create(ut,
				      thread_id,
                                      discipline,
                                      frs->sigdesc->sig_dequeue,
                                      frs->sigdesc->sig_unframesched)) == NULL)
	{
                frs_thread_unlinkfrs(ut, NULL);
                ret = ENOMEM;
                goto error2;
        }
                      
	frs_major_enqueue(frs->major, minor_index, procd);

        /*
         * If this was the first time this process was enqueued
         * on a frame scheduler minor frame queue, then we have to incr
         * the start_sync barrier.
         */
        if (refs == 1) {
                semabarrier_inc(frs->sdesc->start_sync);
        }
        
	frs_thread_ref_release(ut);
	return (0);
        
  error2:
	frs_thread_ref_release(ut);
        
  error1:
        return (ret);
}

int
frs_fsched_join(frs_fsched_t* frs, long* cminor)
{
	uthread_t *ut = curuthread;
	int ret;
        int s;

	ASSERT(frs != NULL);

	/*
	 * Set FRS join bit if it hasn't already been set
	 */
	s = frs_thread_lock(ut);
	if (!_isFrameScheduled(ut)) {
		frs_thread_unlock(ut, s);
		return (FRS_ERROR_PROCESS_NFSCHED);
	}

	if (_isFSJoin(ut)) {
		frs_thread_unlock(ut, s);
		return (FRS_ERROR_PROCESS_ISJOINED);
	}

	_makeFSJoin(ut);
        frs_thread_unlock(ut, s);

	/*
         * Set real-time priority
         */
	setinfoRunq(UT_TO_KT(curuthread),
		    &curuthread->ut_pproxy->prxy_sched,
		    RQRTPRI,
		    NDPHIMAX + 2);

	/*
	 * Set the current minor return value
	 */
	*cminor = 0;

        /*
         * Wait on the start synchronization barrier
         * if we are synchronizing at the beginning.
         * We don't wait for dynamic joins.
         */
        if (frs_fsched_state_initialsync(frs)) {
                if (semabarrier_wait(frs->sdesc->start_sync, PZERO+1) != 0) {
                        ret = FRS_ERROR_FSCHED_JOININTR;
			goto error;
		}
        }

	/*
         * We mustrun the thread to the FRS cpu
         */
        if ((ret = mp_mustrun(0, frs->cpu, 0, 1)) != 0)
		goto error;

	/*
	 * When this point is reached, we're executing within our cpu
	 * and we have been activated by the frame scheduler.
	 * It's effectively this process' turn to run in it's first frame.
	 */

	return (0);

error:
	s = frs_thread_lock(ut);
	ut->ut_frsflags &= ~FRS_FLAG_JOIN;
	frs_thread_unlock(ut, s);

	(void) setinfoRunq(curthreadp,
			   &curuthread->ut_pproxy->prxy_sched,
			   RQRTPRI,
			   0);

	return (ret);
}

int
frs_fsched_start(frs_fsched_t* frs)
{
	int ret;

	ASSERT(frs != 0);

        if (frs_fsched_sdesc_terminating(frs)) {
                ret = FRS_ERROR_FSCHED_DESTROYED;
                goto error;
        }

	if (frs_fsched_state_get(frs) != FRS_STATE_CREATED) {
		ret = FRS_ERROR_FSCHED_INVSTATE;
		goto error;
	}

	/*
	 * Only a non-frame-scheduled thread can start a frame scheduler.
         * If a frame-scheduled thread were to be allowed to start a
         * frame scheduler, the starting thread could end up waiting for
         * itself to join, forever.
	 *
         * In addition, it can't be a mustrun thread. Users can easily make
	 * the mistake of mustrunning this thread on the cpu where we are
	 * going to be running the frame scheduler we are starting.
	 */
        if (ret = frs_thread_starter_test(curuthread))
                goto error;

	/*
	 * Transition state
	 */
        if (ret = frs_fsched_state_transition(frs,
					      FRS_STATE_CREATED,
					      FRS_STATE_STARTING)) {
                goto error;
        }

        /*
         * Initialize state for this frs's major.
         */
        frs_major_start(frs->major);

	/*
	 * Arm interrupt sources
	 */
        frs_cpu_eventintr_arm(frs);

	/*
	 * Wait for all processes in queues to join in.
         * We have a semabarrier in sdesc for synchronizing
         * startup. This semabarrier is originally initizalized
         * to one by the sync-master frs, and then incremented
         * once by every sync-slave frame scheduler succesfully
         * created. Later, during the process enqueueing procedure,
         * this semabarrier is incremented everytime a *new* process
         * is enqueued on any frs.
         * Later, every joining process does a semabarrier_wait,
         * and every starter also does a semabarrier_wait.
         * The starter "waits" match the initial semabarrier increments,
         * and the join "waits" match the enqueue increments. When
         * every process has joined and every frame scheduler has
         * been started, the semabarrier is released, and the
         * game starts. Play Ball!
         */

        if (semabarrier_wait(frs->sdesc->start_sync, PZERO+1) != 0) {
                ret = FRS_ERROR_FSCHED_JOININTR;
                goto error;
        }

        if (frs_fsched_sdesc_terminating(frs)) {
                ret = FRS_ERROR_FSCHED_DESTROYED;
                goto error;
        }

	if (_isMasterFrs(frs)) {
		/*
		 * Transition the Master and all participating Slave frame
		 * schedulers to the SYNCINTR state.
		 */
		ret = frs_fsched_state_transition_all(frs,
						      FRS_STATE_STARTING,
						      FRS_STATE_SYNCINTR);
		if (ret)
			goto error;

		/*
		 * Fire the sync interrupt!
		 */
		frs_cpu_eventintr_fire(frs);
	}
        
	return (0);

  error:

	/*
	 * This hack is required to force the termination interrupt to
	 * fire on our cpu.  We cannot rely on frs_fsched_destroy() to
	 * do this, as someone else may have beat us into frs_fsched_destroy()
	 * and already attempted to send our cpu the interrupt.  Usually this
	 * is fine, but if this happened before we even armed FRS interrupts
	 * above, then the initial attempt would have been ignored.
	 */

	frs->sdesc->sl_term_ack = 1;
	cpuaction(frs->cpu, (cpuacfunc_t) frs_handle_termination, A_NOW);

        frs_fsched_destroy(frs);

        return (ret);
}

int
frs_fsched_yield(frs_fsched_t* frs, long* cminor)
{ 
	uthread_t *ut = curuthread;
        int ret;
        int s;

	ASSERT(frs != NULL);
	
	LOG_TSTAMP_EVENT(RTMON_FRAMESCHED, TSTAMP_EV_YIELD,
			 NULL, NULL, NULL, NULL);

	/*
	 * Make sure thread is eligable for yielding
	 */
	s = frs_thread_lock(ut);
	if (!_isFrameScheduled(ut)) {
		frs_thread_unlock(ut, s);
		return (FRS_ERROR_PROCESS_NFSCHED);
	}

	if (!_isFSJoin(ut)) {
		frs_thread_unlock(ut, s);
		return (FRS_ERROR_PROCESS_NJOINED);
	}
      	_makeFSYield(ut);  
	frs_thread_unlock(ut, s);

	/*
	 * Make sure state transition is valid
	 */
        if (ret = frs_fsched_state_transition(frs,
					      FRS_STATE_EXEC,
					      FRS_STATE_DISPATCH)) {
		return (ret);
        }

	/*
	 * Yield
	 */
	frs_cpu_resched();
	
	*cminor = frs_major_getcminor(frs->major);
        
	LOG_TSTAMP_EVENT(RTMON_FRAMESCHED,
			 TSTAMP_EV_INTREXIT, NULL, NULL, NULL, NULL);
	return (0);
}

int
frs_fsched_getqueuelen(frs_fsched_t* frs, int minor, long* len)
{
        int ret;
        
	ASSERT(frs != 0);
	ASSERT(len != 0);

        if ((ret = frs_fsched_state_verify_canaccessqueues(frs)) != 0) {
                return (ret);
        }
        
	return (frs_major_getqueuelen(frs->major, minor, len));
}

int
frs_fsched_readqueue(frs_fsched_t* frs, int minor, pid_t *pids, int maxlen)
{
        int ret;
        
	ASSERT(frs != 0);
	ASSERT(pids != 0);
        
        if ((ret = frs_fsched_state_verify_canaccessqueues(frs)) != 0) {
                return (ret);
        }
        
	return (frs_major_readqueue(frs->major, minor, pids, maxlen));
}

int
frs_fsched_premove(frs_fsched_t *frs, int minor, frs_threadid_t *thread_id)
{
	uthread_t	*ut;
        int		 ret;
        
        ASSERT(frs != 0);

        if ((ret = frs_fsched_state_verify_canaccessqueues(frs)) != 0)
                return (ret);

        if ((ut = frs_thread_ref_acquire(thread_id)) == NULL)
		return (FRS_ERROR_PROCESS_NEXIST);
        	
	ret = frs_major_premove(frs->major, minor, ut);

	frs_thread_ref_release(ut);

        return (ret);
}

int
frs_fsched_pinsert(frs_fsched_t *frs,
		   int minor_index,
		   tid_t basetid,
		   frs_threadid_t *thread_id,
		   int discipline)
{
	frs_procd_t	*procd;
	uthread_t	*ut;
	int 	 	 refs;
	int		 ret;
        
	ASSERT(frs != 0);

        /*
         * Process can only be pinserted while
         * the frame scheduler is running normally.
         */
        if (ret = frs_fsched_state_verify_canaccessqueues(frs))
                return (ret);

        /*
         * Verify the specified destination minor frame process queue
         * really exists.
         */
        if (ret = frs_major_verifyminor(frs->major, minor_index))
                return (ret);

        /*
         * The base discipline is always FRS_DISC_RT
         */
        discipline |= FRS_DISC_RT;

        /*
         * A continuable process must also be overrunnable and underrunnable
         */
	if (discipline & FRS_DISC_CONT)
                discipline |=  FRS_DISC_OVERRUNNABLE | FRS_DISC_UNDERRUNNABLE;
	
	/*
	 * Acquire a reference on the target thread
	 */
	if ((ut = frs_thread_ref_acquire(thread_id)) == NULL)
                return (FRS_ERROR_PROCESS_NEXIST);

	/*
	 * Make sure the thread qualifies for FRS access
	 */
	if (ret = frs_thread_access(ut))
		goto done;

	/*
	 * Link the thread to the FRS
	 */
        if (ret = frs_thread_linkfrs(ut, frs, &refs))
                goto done;

	/*
	 * we create procd
	 */
        if ((procd = frs_procd_create(ut,
				      thread_id,
                                      discipline,
                                      frs->sigdesc->sig_dequeue,
                                      frs->sigdesc->sig_unframesched)) == NULL) {
                frs_thread_unlinkfrs(ut, NULL);
                ret = ENOMEM;
                goto done;
        }

        if ((ret = frs_major_pinsert(frs->major,
				     minor_index,
				     basetid,
				     procd)) != 0) {
                /*
                 * procd_destroy invokes framesched_unlinkfrs
                 */
                frs_procd_destroy(procd);
                goto done;
        }

        /*
         * This proces will continue to be  scheduled normally,
         * until it is joined (that is, the standard IRIX scheduler
         * will be scheduling it).
	 *
         * If it is joined alrady, it will be scheduled as soon as
         * this queue is read by the dispatcher.
         */

done:                      
	frs_thread_ref_release(ut);
        return (ret);
}        

int
frs_fsched_stop(frs_fsched_t* frs)
{
        int sl_ospl;
        int ret;
        
        ASSERT (frs != NULL);

        if (ret = frs_fsched_state_verify_canstop(frs))
                return (ret);
        
        sl_ospl = frs_fsched_sllock(frs);
        if (frs->sdesc->sl_term) {
                /*
                 * We are terminating.
                 */
                frs_fsched_slunlock(frs, sl_ospl);
                return (FRS_ERROR_FSCHED_DESTROYED);
        }

        frs->sdesc->sl_stop = 1;
        frs_fsched_slunlock(frs, sl_ospl);
        return (0);
}

int
frs_fsched_resume(frs_fsched_t* frs)
{
        int sl_ospl;
        int ret;
        
        ASSERT (frs != NULL);
        
        if (ret = frs_fsched_state_verify_canresume(frs))
                return (ret);
        
        sl_ospl = frs_fsched_sllock(frs);
        if (frs->sdesc->sl_term) {
                /*
                 * We are terminating.
                 */
                frs_fsched_slunlock(frs, sl_ospl);
                return (FRS_ERROR_FSCHED_DESTROYED);
        }

        if (!frs->sdesc->sl_stop) {
                /*
                 * This frs has not been stopped
                 */
                frs_fsched_slunlock(frs, sl_ospl);
                return (FRS_ERROR_PROCESS_NSTOPPED);
        }

        frs->sdesc->sl_stop = 0;
        frs_fsched_slunlock(frs, sl_ospl);
        return (0);
}

int
frs_fsched_setrecovery(frs_fsched_t* frs, frs_recv_info_t* recv_info)
{
        int ret;
	intrdesc_t *idesc;

        if (ret = frs_fsched_state_verify_canaccessattr(frs)) {
                return (ret);
        }
        
	if (recv_info->rmode <= MFBERM_INVALID ||
	    recv_info->rmode >= MFBERM_LAST ) {
		return (FRS_ERROR_RECOVERY_INVMODE);
	}
	
	if (recv_info->rmode == MFBERM_EXTENDFRAME_STRETCH ||
	    recv_info->rmode == MFBERM_EXTENDFRAME_STEAL) {
		idesc = frs_major_getidesc(frs->major, FRS_IDESC_CURRENT);
		if (!_isIntTimerIdescSource(idesc)) {
			return (FRS_ERROR_RECOVERY_CANTEXTEND);
		}
		if (recv_info->xtime < FRS_TIMER_SHORTEST ||
		    recv_info->xtime > FRS_TIMER_LONGEST) {
			return (FRS_ERROR_RECOVERY_INVEXTTIME);
		}
		if (recv_info->tmode != EFT_FIXED) {
			return (FRS_ERROR_RECOVERY_INVEXTMODE);
		}
	}

        /*
         * FRS-XXXX What's the locking here?
         */
	frs_set_rmode(frs, recv_info->rmode);
	frs_set_tmode(frs, recv_info->tmode);
	frs_set_mcerr(frs, recv_info->maxcerr);
	frs_set_xtime(frs, recv_info->xtime);

	return (0);
}

int
frs_fsched_getrecovery(frs_fsched_t* frs, frs_recv_info_t* recv_info)
{
        int ret;
	
        if (ret = frs_fsched_state_verify_canaccessattr(frs))
                return (ret);

	recv_info->rmode = frs_get_rmode(frs);
	recv_info->tmode = frs_get_tmode(frs);
	recv_info->maxcerr = frs_get_mcerr(frs);
	recv_info->xtime = frs_get_xtime(frs);

	return (0);
}

int
frs_fsched_set_signals(frs_fsched_t* frs, frs_signal_info_t* signal_info)
{
        int ret;
		
        if (ret = frs_fsched_state_verify_canaccessattr(frs))
                return (ret);
        
	if (signal_info->sig_underrun)
		frs->sigdesc->sig_underrun = signal_info->sig_underrun;
	if (signal_info->sig_overrun)
		frs->sigdesc->sig_overrun = signal_info->sig_overrun;
        if (signal_info->sig_dequeue)
		frs->sigdesc->sig_dequeue = signal_info->sig_dequeue;
        if (signal_info->sig_unframesched)
		frs->sigdesc->sig_unframesched = signal_info->sig_unframesched;

	/*
	 * Setting of sig_terminate and sig_isequence is not allowed
	 */

	return (0);
}

int
frs_fsched_get_signals(frs_fsched_t* frs, frs_signal_info_t* signal_info)
{
        int ret;
		
        if (ret = frs_fsched_state_verify_canaccessattr(frs))
                return (ret);

        signal_info->sig_underrun = frs->sigdesc->sig_underrun;
        signal_info->sig_overrun = frs->sigdesc->sig_overrun;     
        signal_info->sig_terminate = SIGKILL;
        signal_info->sig_dequeue = frs->sigdesc->sig_dequeue;
        signal_info->sig_unframesched = frs->sigdesc->sig_unframesched;     

        return(0);
}

int
frs_fsched_getoverruns(frs_fsched_t* frs,
                       int minor,
                       tid_t tid,
                       frs_overrun_info_t* overrun_info)
{
        int ret;
        
        ASSERT (frs != 0);
        ASSERT (overrun_info != 0);

        if (ret = frs_fsched_state_verify_canaccessattr(frs)) {
                return (ret);
        }
        
        return (frs_major_getoverruns(frs->major, minor, tid, overrun_info));
}

void
frs_fsched_isequence_error(frs_fsched_t* frs)
{
	ASSERT(frs != 0);

#if 0
	/*
	 * XXX Need to define a new event type
	 */
	LOG_TSTAMP_EVENT(RTMON_FRAMESCHED,
			 TSTAMP_EV_XRUN, NULL, NULL, NULL, NULL);
#endif

	frs_thread_sendsig(&frs->control_id,
			   frs->sigdesc->sig_isequence);
}

/**************************************************************************
 *                      frs_fsched_t tools                                *
 **************************************************************************/

static int
frs_fsched_setup_master(frs_fsched_t* frs,
			frs_fsched_info_t* info,
			frs_intr_info_t* intr_info)
{
	int ret;
	syncdesc_t *sdesc;
	sigdesc_t *sigdesc;

	/* Mark as the Master */
	_setMasterFrs(frs);

	/*
	 * Validate interrupt source parameters
	 */ 
	if (ret = frs_cpu_eventintr_validate(frs, info, intr_info))
		return (ret);

	/*
	 * Validate the requested number of Slaves
	 */
	if (info->num_slaves < 0 || frs_cpu_verifyvalid(info->num_slaves+1))
		return (FRS_ERROR_FSCHED_TOOMANYSLAVES);
	    
        /*
         * Allocate and initialize the Synchronization Descriptor
         */
        sdesc = (syncdesc_t*) kmem_zalloc(sizeof(syncdesc_t), KM_SLEEP);
	if (sdesc == NULL)
		return (ENOMEM);

	bzero(sdesc, sizeof(syncdesc_t)); 

	frs->sdesc = sdesc;
        spinlock_init(&sdesc->sl_lock, "sdescsllock");

	/* Interrupt handler barrier */
	frs->sb_intr_main = syncbar_create(info->num_slaves + 1);
	if (frs->sb_intr_main == NULL)
		return (ENOMEM);

	/* Frame step clear barrier */
	frs->sb_intr_clear = syncbar_create(info->num_slaves + 1);
	if (frs->sb_intr_clear == NULL)
		return (ENOMEM);

	/* First interrupt barrier */
	frs->sb_intr_first = syncbar_create(info->num_slaves + 1);
	if (frs->sb_intr_first == NULL)
		return (ENOMEM);
        
	/*
         * Barrier to synchronously start all frame schedulers
         * in a Synchronous Set.
         */
	sdesc->start_sync = semabarrier_create(1);
	if (sdesc->start_sync == NULL)
		return (ENOMEM);

	/*
         * We allocate memory to hold references to Slaves
	 */
	if (info->num_slaves > 0) {
                sdesc->sl_num = info->num_slaves;
                sdesc->sl_index = 0;
                sdesc->sl_list = (frs_fsched_t**) kmem_zalloc(
                        info->num_slaves * sizeof(frs_fsched_t**),
                        KM_SLEEP);

                if (sdesc->sl_list == NULL)
			return (ENOMEM);

		bzero(sdesc->sl_list,
		      sdesc->sl_num * sizeof(frs_fsched_t**));
        } else {
                sdesc->sl_num = 0;
                sdesc->sl_index = 0;
                sdesc->sl_list = NULL;
        }

        sdesc->sl_term = 0;
        sdesc->sl_stop = 0;

        /*
         * The link to the master-frs is NULL'ed.
         * It has to be set later using the link
         * methods.
         */
        sdesc->master_frs = NULL;

	/*
	 * The recovery descriptor field in the sync desc is
	 * set to a "non-recovery" default value. After creation
	 * the user may choose to activate automatic error
	 * recovery at Minor Frame Boudary Errors (MFBE).
	 */
	sdesc->rdesc.rmode = MFBERM_NORECOVERY;
	sdesc->rdesc.tmode = EFT_FIXED;
	sdesc->rdesc.maxcerr = 0;

	/*
	 * The MFBE state field is initialized to "no-error"
	 */
	sdesc->mfbestate = 0;

        /*
         * Allocation and Initialization of
         * the signal descriptor.
         */
        sigdesc = (sigdesc_t*) kmem_zalloc(sizeof(sigdesc_t), KM_SLEEP);
	if (sigdesc == NULL)
		return (ENOMEM);

	bzero(sigdesc, sizeof(sigdesc_t));    
	frs->sigdesc = sigdesc;
        
	/*
	 * Finally, we initialize the signal descriptor
	 */
	sigdesc->sig_underrun = FRS_SIGNAL_UNDERRUN;
	sigdesc->sig_overrun = FRS_SIGNAL_OVERRUN;
        sigdesc->sig_dequeue = FRS_SIGNAL_DEQUEUE;
        sigdesc->sig_unframesched = FRS_SIGNAL_UNFRAMESCHED;
        sigdesc->sig_isequence = FRS_SIGNAL_ISEQUENCE;

	/*
	 * Create major frame
	 */
        if ((ret = frs_major_create(frs, info, intr_info)) != 0)
		return (ret);
	 
	/*
	 * Voila! we're all set.
	 */
	return (0);
}

static int
frs_fsched_setup_slave(frs_fsched_t *frs,
		       frs_fsched_info_t* info)
{
	frs_fsched_t	*mfrs;
	frs_threadid_t	 control_thread_id;
	int		 ret;
	
	/* Mark as a Slave */
	_setSlaveFrs(frs);

        /*
         * frs_thread_controller_getfrs() increments the ref count on
	 * the Master frame scheduler.
	 *
         * This ref is kept throughout the life of this slave frs, to prevent
	 * the shared resources from disappearing out from under our noses.
         */
        
	frs_tid_to_threadid(info->controller, &control_thread_id);
        mfrs = frs_thread_controller_getfrs(&control_thread_id, &ret);
	if (mfrs == NULL)
		return (ret);

        ASSERT (_isMasterFrs(mfrs));
	ASSERT (mfrs->major != NULL);

	/*
	 * Slaves inherit all interrupt and signal attributes
	 * from its Master.
	 */
	frs->intrgroup = mfrs->intrgroup;

	frs->intrmask = mfrs->intrmask;
	ASSERT (frs->intrmask);

	frs->sdesc = mfrs->sdesc;
	ASSERT (frs->sdesc != NULL);
	ASSERT (frs->sdesc->master_frs == mfrs);

	frs->sigdesc = mfrs->sigdesc;
	ASSERT (frs->sigdesc != NULL);

        /*
         * The insertion of this slave into the slave list happens later.
         * For now, we just check whether the slave list has an available
	 * slot.
         */
        if ((ret = frs_fsched_sdesc_spaceavail(mfrs)) != FRS_OK_SPACE_AVAIL) {
                frs_fsched_decrref(mfrs);
                return (ret);
        }

	/*
	 * Clone the Master's major frame
	 */
        if ((ret = frs_major_clone(mfrs, frs)) != 0) {
		frs_fsched_decrref(mfrs);
                return (ret);
        }

	/*
         * Add this slave to the start-up barrier.
	 */
	semabarrier_inc(frs->sdesc->start_sync);

	/*
	 * Register sync barriers
	 */
	frs->sb_intr_main  = syncbar_register(mfrs->sb_intr_main);
	frs->sb_intr_clear = syncbar_register(mfrs->sb_intr_clear);
	frs->sb_intr_first = syncbar_register(mfrs->sb_intr_first);

	ASSERT (frs->sb_intr_main);
	ASSERT (frs->sb_intr_clear);
	ASSERT (frs->sb_intr_first);

	return (0);
}

/**************************************************************************
 *                          Destruction Management                        *
 **************************************************************************/

static void
frs_fsched_destroy_master(frs_fsched_t* frs)
{
        syncdesc_t* sdesc;

        ASSERT (frs != NULL);
        ASSERT (frs->sdesc != NULL);

        sdesc = frs->sdesc;

        semabarrier_dec(sdesc->start_sync);

        /*
         * We eliminate the local temporary ref
         * we acquired in frs_thread_exit.
         */
        frs_fsched_decrref(frs);

        /*
         * At this point we should have:
	 *
         *    - ZERO LOCAL PERMANENT references,
         *    - ZERO LOCAL TEMPORARY reference,
         *    - SOME REMOTE PERMANENT references from slaves,
	 *    - and maybe some activity threads have temporary refs
	 *
         * The wait method increments the in_counter to one, which takes
	 * care of the ref from the sdesc.
	 *
         * Waitref will return when the number of clients goes down to 1.
	 * This means every slave referencing the sdesc will be gone by the
         * time waitref returns.
         */
        frs_fsched_waitref(frs);

        /*
         * Eliminate the ref from our own sdesc
         * ref--
         */
        frs_fsched_sdesc_clear_masterentry(frs);        
}

static void
frs_fsched_destroy_slave(frs_fsched_t* frs)
{
	syncdesc_t *sdesc;

        ASSERT (frs != NULL);
        ASSERT (frs->sdesc != NULL);

	sdesc = frs->sdesc;

	semabarrier_dec(sdesc->start_sync);

	/*
	 * Unregister from synchronization barriers
	 */
	syncbar_unregister(frs->sb_intr_main);
	syncbar_unregister(frs->sb_intr_clear);
	syncbar_unregister(frs->sb_intr_first);

        /*
         * Eliminate the reference from the sdesc to this frs.
	 * ref--
         */
        frs_fsched_sdesc_clear_slentry(frs);

	/*
	 * decrement the reference count on the master frs
	 * ref--
	 */
	ASSERT (sdesc->master_frs != NULL);
	frs_fsched_decrref(sdesc->master_frs);

        /*
         * At this point we should have:
	 *
         *    - ZERO LOCAL PERMANENT references,
         *    - ONE LOCAL TEMPORARY reference (our local ref),
         *    - ZERO REMOTE PERMANENT references (slaves don't have remote refs)
	 *    - and maybe some activity threads have temporary refs
	 *
         * The wait method increments the in_counter to one, which takes
	 * care of our local temporary reference.
	 *
	 * Waitref will return when the number of clients goes down to 1,
	 * which means that references == (nclients - incounter) == 0.
         */
        frs_fsched_waitref(frs);

        /*
         * We eliminate the local temporary ref
         * we acquired in frs_thread_exit.
	 * ref--
         */
        frs_fsched_decrref(frs);
}      

/*
 * Free resources allocated during creation
 */
static void
frs_fsched_free_resources(frs_fsched_t *frs)
{
        if (frs->flags & FRS_FLAGS_MASTER) {
                /*
                 * Deallocate common interrupt group
                 */
                if (frs->intrgroup)
			intrgroup_free(frs->intrgroup);

		/*
		 * Deallocate Barriers
		 */
		if (frs->sb_intr_main)
			syncbar_destroy(frs->sb_intr_main);

		if (frs->sb_intr_clear)
			syncbar_destroy(frs->sb_intr_clear);

		if (frs->sb_intr_first)
			syncbar_destroy(frs->sb_intr_first);

                /*
                 * Deallocate synchronization descriptor resources
                 */
                if (frs->sdesc) {
			syncdesc_t *sdesc = frs->sdesc;
			spinlock_destroy(&sdesc->sl_lock);

                        if (sdesc->sl_list) {
                                ASSERT (sdesc->sl_num != 0);
                                kmem_free(sdesc->sl_list,
                                          sdesc->sl_num*sizeof(frs_fsched_t**));
                        }

                        if (sdesc->start_sync)
                                semabarrier_destroy(sdesc->start_sync);

                        kmem_free(sdesc, sizeof(syncdesc_t));
                }

                if (frs->sigdesc)
                        kmem_free(frs->sigdesc, sizeof(sigdesc_t));
        }

	if (frs->major)
                frs_major_destroy(frs->major);

        if (frs->references) {
		ASSERT (frs->references->nclients == 0);
		semabarrier_destroy(frs->references);
	}

	spinlock_destroy(&frs->state_lock);
	freesema(&frs->term_sema);

	bzero(frs, sizeof(frs_fsched_t));
	kmem_free(frs, sizeof(frs_fsched_t));
}
       

/**************************************************************************
 *                     frs reference count management                     *
 **************************************************************************/

void
frs_fsched_incrref(frs_fsched_t* frs)
{
        semabarrier_inc(frs->references);
}

void
frs_fsched_decrref(frs_fsched_t* frs)
{
        semabarrier_dec(frs->references);
}

void
frs_fsched_waitref(frs_fsched_t* frs)
{
        (void)semabarrier_wait(frs->references, PZERO);
}
                
/**************************************************************************
 *                     frs state management                               *
 **************************************************************************/

static int
frs_fsched_state_transition(frs_fsched_t* frs,
                            frs_state_t fromstate,
                            frs_state_t tostate)
{
        int ret, s;

        ASSERT (frs != NULL);

        s = frs_fsched_statelock(frs);

	if (frs->state == FRS_STATE_TERMINATION) {
                 ret = 0;
        } else if (fromstate == frs->state ||
                   tostate == frs->state ||
                   fromstate == FRS_STATE_ANY) {
                frs->state = tostate;
                ret = 0;
        } else {
                ret = FRS_ERROR_FSCHED_INVSTATE;
        }

	frs_fsched_stateunlock(frs, s);

	return (ret);
}

static int
frs_fsched_state_transition_nested(frs_fsched_t* frs,
				   frs_state_t fromstate,
				   frs_state_t tostate)
{
        int ret;

        ASSERT (frs != NULL);

        frs_fsched_nested_statelock(frs);

	if (frs->state == FRS_STATE_TERMINATION) {
                 ret = 0;
        } else if (fromstate == frs->state ||
                   tostate == frs->state ||
                   fromstate == FRS_STATE_ANY) {
                frs->state = tostate;
                ret = 0;
        } else {
                ret = FRS_ERROR_FSCHED_INVSTATE;
        }

	frs_fsched_nested_stateunlock(frs);

	return (ret);
}

static int
frs_fsched_state_transition_all(frs_fsched_t *frs,
				frs_state_t fromstate,
				frs_state_t tostate)
{
	frs_fsched_t	*slavefrs;
	syncdesc_t	*sdesc;
	int		ret;
	int		i;

	ASSERT(frs != 0);
	ASSERT(_isMasterFrs(frs));

	ret = frs_fsched_state_transition(frs,
					  fromstate,
					  tostate);
	if (ret)
		return (ret);
	
	ASSERT (frs->sdesc != 0);
	sdesc = frs->sdesc;
	for (i = 0; i < frs->sdesc->sl_index; i++) {
		ASSERT (sdesc->sl_list != 0);
		slavefrs = sdesc->sl_list[i];
		ASSERT (slavefrs != 0);
		ret = frs_fsched_state_transition(slavefrs,
						  fromstate,
						  tostate);
		if (ret)
			return (ret);
	}

	return 0;
}
	
static frs_state_t
frs_fsched_state_get(frs_fsched_t* frs)
{
        ASSERT (frs != NULL);

	/* snapshot */
        return (frs->state);
}

/*
 * Verify the reference frame scheduler is in a state where
 * it can receive interrupts.
 */

int
frs_fsched_state_verify_canintr(frs_fsched_t* frs)
{
        int state_ospl;

        ASSERT (frs != NULL);

        state_ospl = frs_fsched_statelock(frs);
        switch (frs->state) {
        case FRS_STATE_CREATED:
        case FRS_STATE_STARTING:
        case FRS_STATE_TERMINATION:
                frs_fsched_stateunlock(frs, state_ospl);
                return (FRS_ERROR_FSCHED_INVSTATE);
        case FRS_STATE_SYNCINTR:
        case FRS_STATE_FIRSTINTR:
        case FRS_STATE_DISPATCH:
        case FRS_STATE_EXEC:
        case FRS_STATE_EXEC_IDLE:
                break;
        }
        frs_fsched_stateunlock(frs, state_ospl);
        return (0); 
}
                
/*
 * Verify the reference frame scheduler is in a state where
 * a thread attached to it can yield.
 */

int
frs_fsched_state_verify_canyield(frs_fsched_t* frs)
{
        int state_ospl;

        ASSERT (frs != NULL);

        state_ospl = frs_fsched_statelock(frs);

        switch (frs->state) {
        case FRS_STATE_CREATED:
        case FRS_STATE_STARTING:
        case FRS_STATE_TERMINATION:
                frs_fsched_stateunlock(frs, state_ospl);
                return (FRS_ERROR_FSCHED_INVSTATE);
        case FRS_STATE_SYNCINTR:
        case FRS_STATE_FIRSTINTR:
        case FRS_STATE_DISPATCH:
        case FRS_STATE_EXEC:
        case FRS_STATE_EXEC_IDLE:
                break;
        }
        frs_fsched_stateunlock(frs, state_ospl);
        return (0);
}

/*
 * Verify the reference frame scheduler is in a state where
 * a thread can be enqueued
 */
static int
frs_fsched_state_verify_canenqueue(frs_fsched_t* frs)
{
        int state_ospl;

        ASSERT (frs != NULL);

        state_ospl = frs_fsched_statelock(frs);
        if (frs->state != FRS_STATE_CREATED) {
                frs_fsched_stateunlock(frs, state_ospl);
                return (FRS_ERROR_FSCHED_INVSTATE);
        }
        frs_fsched_stateunlock(frs, state_ospl);
        return (0);
}

/*
 * Verify users can dynamically manage queues
 */
static int
frs_fsched_state_verify_canaccessqueues(frs_fsched_t* frs)
{
        int state_ospl;

        ASSERT (frs != NULL);

        state_ospl = frs_fsched_statelock(frs);
        switch (frs->state) {
        case FRS_STATE_CREATED:
        case FRS_STATE_STARTING:
        case FRS_STATE_TERMINATION:
                frs_fsched_stateunlock(frs, state_ospl);
                return (FRS_ERROR_FSCHED_INVSTATE);
        case FRS_STATE_SYNCINTR:
        case FRS_STATE_FIRSTINTR:                        
        case FRS_STATE_DISPATCH:
        case FRS_STATE_EXEC:
        case FRS_STATE_EXEC_IDLE:
                break;
        }
        frs_fsched_stateunlock(frs, state_ospl);
        return (0);
}

/*
 * Verify users can stop
 */
static int
frs_fsched_state_verify_canstop(frs_fsched_t* frs)
{
        int state_ospl;

        ASSERT (frs != NULL);

        state_ospl = frs_fsched_statelock(frs);
        switch (frs->state) {
        case FRS_STATE_CREATED:
        case FRS_STATE_STARTING:
        case FRS_STATE_TERMINATION:
        case FRS_STATE_SYNCINTR:
        case FRS_STATE_FIRSTINTR:        
                frs_fsched_stateunlock(frs, state_ospl);
                return (FRS_ERROR_FSCHED_INVSTATE);
        case FRS_STATE_DISPATCH:
        case FRS_STATE_EXEC:
        case FRS_STATE_EXEC_IDLE:
                break;
        }
        frs_fsched_stateunlock(frs, state_ospl);
        return (0);
}

/*
 * Verify users can resume
 */
static int
frs_fsched_state_verify_canresume(frs_fsched_t* frs)
{
        int state_ospl;

        ASSERT (frs != NULL);

        state_ospl = frs_fsched_statelock(frs);
        switch (frs->state) {
        case FRS_STATE_CREATED:
        case FRS_STATE_STARTING:
        case FRS_STATE_TERMINATION:
        case FRS_STATE_SYNCINTR:
        case FRS_STATE_FIRSTINTR:        
                frs_fsched_stateunlock(frs, state_ospl);
                return (FRS_ERROR_FSCHED_INVSTATE);
        case FRS_STATE_DISPATCH:
        case FRS_STATE_EXEC:
        case FRS_STATE_EXEC_IDLE:
                break;
        }
        frs_fsched_stateunlock(frs, state_ospl);
        return (0);
}

/*
 * Verify users can access attributes
 */
static int
frs_fsched_state_verify_canaccessattr(frs_fsched_t* frs)
{
        int state_ospl;

        ASSERT (frs != NULL);

        state_ospl = frs_fsched_statelock(frs);
        switch (frs->state) {
        case FRS_STATE_TERMINATION:
                frs_fsched_stateunlock(frs, state_ospl);
                return (FRS_ERROR_FSCHED_INVSTATE);
        case FRS_STATE_SYNCINTR:
        case FRS_STATE_FIRSTINTR:              
        case FRS_STATE_CREATED:
        case FRS_STATE_STARTING:         
        case FRS_STATE_DISPATCH:
        case FRS_STATE_EXEC:
        case FRS_STATE_EXEC_IDLE:
                break;
        }
        frs_fsched_stateunlock(frs, state_ospl);
        return (0);
}

/*
 * Is this frs in the initial sync state
 */
static int
frs_fsched_state_initialsync(frs_fsched_t* frs)
{

	frs_state_t state;

	state = frs_fsched_state_get(frs);

        switch (state) {
        case FRS_STATE_CREATED:
        case FRS_STATE_STARTING:
        case FRS_STATE_SYNCINTR:
        case FRS_STATE_FIRSTINTR:        
                return (1);
        case FRS_STATE_TERMINATION:
        case FRS_STATE_DISPATCH:
        case FRS_STATE_EXEC:
        case FRS_STATE_EXEC_IDLE:
                break;
        }
        return (0);
}

/**************************************************************************
 *                     frs sdesc  management                              *
 **************************************************************************/

/*
 * Verify availability of space in
 * the slave list. This routine
 * doesn't guarantee that space will
 * be available when needed, but it
 * detects and guarantees lack of space.
 */

int
frs_fsched_sdesc_spaceavail(frs_fsched_t* frs)
{
        int sl_ospl;

        sl_ospl = frs_fsched_sllock(frs);
        if (frs->sdesc->sl_term) {
                /*
                 * We are terminating.
                 */
                frs_fsched_slunlock(frs, sl_ospl);
                return (FRS_ERROR_FSCHED_DESTROYED);
        }
        if (frs->sdesc->sl_index < frs->sdesc->sl_num) {
                /*
                 * Space available
                 */
                frs_fsched_slunlock(frs, sl_ospl);
                return (FRS_OK_SPACE_AVAIL);
        } else {
                /*
                 * No space
                 */
                frs_fsched_slunlock(frs, sl_ospl);
                return (FRS_ERROR_FSCHED_TOOMANYSLAVES);
        }
}

int
frs_fsched_sdesc_insertmaster(frs_fsched_t* frs)
{
        int sl_ospl;

        ASSERT (frs != NULL);

        sl_ospl = frs_fsched_sllock(frs);
        if (frs->sdesc->sl_term) {
                /*
                 * We are terminating.
                 */
                frs_fsched_slunlock(frs, sl_ospl);
                return (FRS_ERROR_FSCHED_DESTROYED);
        }        
        frs->sdesc->master_frs = frs;
        frs_fsched_slunlock(frs, sl_ospl);
        frs_fsched_incrref(frs);
        return (0);
}

int
frs_fsched_sdesc_insertslave(frs_fsched_t* frs)
{
        int sl_ospl;

        ASSERT (frs != NULL);
                                  
        sl_ospl = frs_fsched_sllock(frs);
        if (frs->sdesc->sl_term) {
                /*
                 * We are terminating.
                 */
                frs_fsched_slunlock(frs, sl_ospl);
                return (FRS_ERROR_FSCHED_DESTROYED);
        }

        if (frs->sdesc->sl_index < frs->sdesc->sl_num) {
                /*
                 * Space available
                 */
                frs->sdesc->sl_list[frs->sdesc->sl_index] = frs;
                frs->sdesc->sl_index++;
                frs_fsched_slunlock(frs, sl_ospl);
                frs_fsched_incrref(frs);
                return (0);
        }

	frs_fsched_slunlock(frs, sl_ospl);
	return (FRS_ERROR_FSCHED_TOOMANYSLAVES);
}

void
frs_fsched_sdesc_clear_slentry(frs_fsched_t* frs)
{
        int sl_ospl;
        int i;

        ASSERT (frs != NULL);
        ASSERT (frs->sdesc != NULL);
                                  
        sl_ospl = frs_fsched_sllock(frs);
        ASSERT (frs->sdesc->sl_term);
        
        for (i = 0; i < frs->sdesc->sl_index; i++) {
                if (frs->sdesc->sl_list[i] == frs) {
                        frs->sdesc->sl_list[i] = NULL;
                        frs_fsched_slunlock(frs, sl_ospl);
                        frs_fsched_decrref(frs);
                        return;
                }
        }
        frs_fsched_slunlock(frs, sl_ospl);
        cmn_err(CE_PANIC, "[frs_fsched_sdesc_clear_slentry]: SL FRS not found");
}

void 
frs_fsched_sdesc_clear_masterentry(frs_fsched_t* frs)
{
        int sl_ospl;

        ASSERT (frs != NULL);
        ASSERT (frs->sdesc != NULL);
                                  
        sl_ospl = frs_fsched_sllock(frs);
        ASSERT (frs->sdesc->sl_term);
        frs->sdesc->master_frs = NULL;
        frs_fsched_slunlock(frs, sl_ospl);
        frs_fsched_decrref(frs);
}
                
int
frs_fsched_sdesc_settermination(frs_fsched_t* frs)
{
	int sl_ospl;
	int ret = 0;

	sl_ospl = frs_fsched_sllock(frs);
	if (frs->sdesc->sl_term)
		ret = 1;
	else
		frs->sdesc->sl_term = 1;
	frs_fsched_slunlock(frs, sl_ospl);

	return (ret);
}

int
frs_fsched_sdesc_terminating(frs_fsched_t* frs)
{
	return (frs->sdesc->sl_term);
}
        

/**************************************************************************
 *                           Event Intr                                   *
 **************************************************************************/

static void
frs_fsched_synchronize(frs_fsched_t* frs)
{
        int s;
        int i;
	__uint64_t basecc;

	ASSERT(frs != 0);
	ASSERT(private.p_frs_objp != 0);

	if (frs_get_intrgroup(frs)) {
		/*
		 * We are using groupintrs which are multicasted to all
		 * involved cpus, so we do not need send initial sync intrs
		 * to each slave.
		 *
		 * Just wait for all controllers to meet at the barrier
		 */
		syncbar_wait(frs->sb_intr_first);
		return;
	}

	if (!_isMasterFrs(frs)) {
		/*
		 * SLAVE
		 *
		 * Inronic... slaves don't need to do any work.
		 */
		return;
	}

	/*
	 * MASTER
	 *
	 * We are using internal timers, so we must start each SYNC slave cpu.
	 */

	basecc = frs_cpu_getcctimer();

	s = frs_fsched_sllock(frs);

	if (frs->sdesc->sl_term) {
		/*
		 * We are terminating.
		 */
		frs_fsched_slunlock(frs, s);
		return;
	}

	/* Set master basecc */
	((frs_pda_t*)private.p_frs_objp)->basecc = basecc;

	ASSERT (frs->sdesc != 0);
	for (i = 0; i < frs->sdesc->sl_index; i++) {
		cpuid_t slavecpu;
		frs_fsched_t* slavefrs;
		syncdesc_t* sdesc;
			
		ASSERT (frs != 0);
		sdesc = frs->sdesc;
		ASSERT (sdesc != 0);
		ASSERT (sdesc->sl_list != 0);
		slavefrs = sdesc->sl_list[i];
                                        
		/*
		 * This temporary reference to the remote
		 * frs is being protected by the sllock.
		 * As long lock as this lock is taken,
		 * the frs entry cannot be modified, and
		 * therefore the validity of the
		 * reference remains stable until we
		 * release sllock.
		 */
		ASSERT (slavefrs != 0);
		slavecpu = slavefrs->cpu;
		
		/* set slave basecc */
		((frs_pda_t*)pdaindr[slavecpu].pda->p_frs_objp)->basecc =
			basecc;

		nested_cpuaction(slavecpu,
				 (cpuacfunc_t) frs_fsched_eventintr,
				 0, 0, 0, 0);
	}

	frs_fsched_slunlock(frs, s);
}

void
frs_fsched_eventintr(void)
{
	frs_fsched_t* frs;
        frs_state_t state;
	intrdesc_t* idesc;
	int ret = 0;

	LOG_TSTAMP_EVENT(RTMON_FRAMESCHED, TSTAMP_EV_EVINTRENTRY,
			 NULL, NULL, NULL, NULL);

	frs = curfrs();
	ASSERT (frs);

        state = frs_fsched_state_get(frs);

	switch (state) {
	case FRS_STATE_CREATED:
	case FRS_STATE_STARTING:
		/*
		 * Ignore
		 */
		idesc = frs_major_getidesc(frs->major, FRS_IDESC_FIRST);
		frs_cpu_eventintr_reset(frs->cpu, idesc);
		break;

	case FRS_STATE_SYNCINTR:
		/*
		 * This phase of startup is primarily used to jump start the
		 * slaves, and synchronize all cooperating frame schedulers.
		 */
		frs_fsched_synchronize(frs);
		idesc = frs_major_getidesc(frs->major, FRS_IDESC_FIRST);
		frs_cpu_eventintr_reset(frs->cpu, idesc);
		state = FRS_STATE_FIRSTINTR;
		break;

	case FRS_STATE_FIRSTINTR:
		/*
		 * At this point all cooperating frame schedulers are in sync.
		 * FRS state gets set to dispatch, allowing the idle thread to
		 * begin scheduling the first minor frame NOW.
		 */
		idesc = frs_major_getidesc(frs->major, FRS_IDESC_NEXT);
		frs_cpu_eventintr_reset(frs->cpu, idesc);
		frs_cpu_resched();
		state = FRS_STATE_DISPATCH;
		break;

	case FRS_STATE_EXEC:
	case FRS_STATE_EXEC_IDLE:
	case FRS_STATE_DISPATCH:
	case FRS_STATE_TERMINATION:
		/*
		 * frs_major_step resets the next interrupt
		 */
		ret = frs_major_step(frs);
		if (ret) {
			/*
			 * The FRS has been terminated.
			 *
			 * Kill our controller and clear the interrupt.
			 * Note: Clearing the interrupt drops the interrupt
			 * handlers reference. So once complete we cannot
			 * touch the frs pointer.
			 */
			frs_thread_sendsig(&frs->control_id, SIGKILL);
			frs_cpu_eventintr_clear(frs);
		}

		frs_cpu_resched();
		state = FRS_STATE_DISPATCH;
		break;

	default:
		cmn_err(CE_PANIC, "[frs_fsched_eventintr]: Invalid State");
		break;
	}

	if (ret == 0)
		frs_fsched_state_transition_nested(frs, FRS_STATE_ANY, state);
        
	LOG_TSTAMP_EVENT(RTMON_FRAMESCHED, TSTAMP_EV_EVINTREXIT,
			 NULL, NULL, NULL, NULL);
}

kthread_t*
frs_fsched_dispatch(void)
{
	kthread_t *kt = NULL;
	frs_fsched_t* frs;
        int do_full_sched = 0;
        frs_state_t state;
	int s;

	ASSERT (issplhi(getsr()));

	/*
	 * Idle thread dispatch routine.
	 *
	 * The idle thread has an implicit reference on its FRS,
	 * as long as the interrupt handler holds its reference.
	 * We wouldn't be in this routine if the interrupt handler
	 * didn't have a reference on the FRS.
	 */

#if IP30
	/*
	 * Exception to the above rule:
	 *
	 * IP30 Platforms run the cputimer interrupt at splprof.
	 * We cannot be sure the interrupt handler still holds its
	 * reference until we are running at splprof ourselves, and
	 * we verify that the interrupt is still active.
	 */
	splprof();
	if (private.p_frs_flags == 0)
		return (cpu_thread_search());
#endif

	ASSERT (private.p_frs_flags);

	frs = curfrs();
	ASSERT (frs);

        state = frs_fsched_state_get(frs);

	switch (state) {
	case FRS_STATE_CREATED:
		cmn_err(CE_PANIC,
			"[frs_fsched_dispatch]: Dispatch in CREATED state");
		break;

	case FRS_STATE_STARTING:
	case FRS_STATE_SYNCINTR:
		s = spl0();
		DELAY(30);
		splx(s);
		break;

	case FRS_STATE_FIRSTINTR:
		break;

	case FRS_STATE_TERMINATION:
		do_full_sched = 1;
		break;

	case FRS_STATE_EXEC:
	case FRS_STATE_EXEC_IDLE:
	case FRS_STATE_DISPATCH:
		kt = frs_major_choosethread(frs->major);
		frs_fsched_state_transition_nested(frs,
				FRS_STATE_ANY,
				kt ? FRS_STATE_EXEC : FRS_STATE_EXEC_IDLE);
		break;

	default:
		cmn_err(CE_PANIC, "[frs_fsched_dispatch]: Invalid state");
	}

	return do_full_sched ? cpu_thread_search() : kt;
}

void
frs_fsched_idesc_print(intrdesc_t* idesc)
{
	ASSERT (idesc != NULL);
	
	qprintf("\nInterrupt Descriptor: 0x%x\n", idesc);
	qprintf("  Source: %d, Qualifier: %d\n", idesc->source, idesc->q.value);
	if (idesc->intrgroup != NULL) {
		qprintf("  InterruptGroup CpuSet: 0x%llx\n",
		       intrgroup_get_cpubv(idesc->intrgroup));
		qprintf("  InterruptGroup Id: 0x%d\n",
		       intrgroup_get_groupid(idesc->intrgroup));
	} else {
		qprintf("  InterruptGroup: NULL\n");
	}
}       
        
void
frs_fsched_sdesc_print(syncdesc_t* sdesc)
{
        int i;
	ASSERT (sdesc != NULL);
	
	qprintf("\nSynchronization Descriptor: 0x%x\n", sdesc);
	qprintf("  Master frs: 0x%x\n", sdesc->master_frs);
        qprintf("  sl_lock: 0x%x\n", &sdesc->sl_lock);
        qprintf("  sl_num: %d sl_index: %d\n",
               sdesc->sl_num, sdesc->sl_index);
        qprintf("  sl_term: %d sl_term_ack %d\n",
		sdesc->sl_term, sdesc->sl_term_ack);
        qprintf("  sl_stop: %d sl_stop_ack %d\n",
		sdesc->sl_stop, sdesc->sl_stop_ack);

        qprintf("  Slave frs list: \n");
        for (i = 0; i < sdesc->sl_index; i++) {
                qprintf("    Slave[%d]: 0x%x\n", i, sdesc->sl_list[i]);
        }
	qprintf("  RecvDesc: Rmode:%d, Tmode:%d, MaxCerr%d, Xtime: %d\n",
	       sdesc->rdesc.rmode,
	       sdesc->rdesc.tmode,
               sdesc->rdesc.maxcerr,
               sdesc->rdesc.xtime);
        qprintf("  MTBE State: %d\n", sdesc->mfbestate);
        qprintf("  Semabarriers: start_sync 0x%x\n",
              sdesc->start_sync);
        semabarrier_print(sdesc->start_sync);
}

void
frs_fsched_sigdesc_print(sigdesc_t* sigdesc)
{
        ASSERT (sigdesc != NULL);

        qprintf("\nSignal Descriptor:\n");
        qprintf("  Underrun: %d, Overrun: %d\n",
		sigdesc->sig_underrun,
		sigdesc->sig_overrun);
        qprintf("  Dequeue: %d, Unframesched: %d Isequence: %d\n", 
		sigdesc->sig_dequeue,
		sigdesc->sig_unframesched,
		sigdesc->sig_isequence);
}
        
void
frs_fsched_print(frs_fsched_t* frs)
{
	char *tab_state[] = {
		"FRS_STATE_ANY",
		"FRS_STATE_CREATED",
		"FRS_STATE_STARTING",
		"FRS_STATE_SYNCINTR",
		"FRS_STATE_FIRSTINTR",
		"FRS_STATE_DISPATCH",
		"FRS_STATE_EXEC",
		"FRS_STATE_EXEC_IDLE",
		"FRS_STATE_TERMINATION",
		"Unknown"
	};

	ASSERT(frs != 0);

	qprintf("\n\nFrame Scheduler Object: 0x%x\n", frs);
        
        qprintf("  RefBarrier: 0x%x\n", frs->references);
	if (frs->references)
		semabarrier_print(frs->references);

        if (frs->control_thread) {
                qprintf("  Frs Controller: tid %d ut 0x%x\n",
			frs_threadid_to_tid(&frs->control_id),
			frs->control_thread);
        } else {
                qprintf("  Frs Controller: NOT ATTACHED\n");
        }

        qprintf("  Cpu: %d, Flags:0x%x\n", frs->cpu, frs->flags);

        qprintf("  State_lock: 0x%x, &State_lock: 0x%x\n",
               frs->state_lock, &frs->state_lock);
        qprintf("  State: %s, Previous State: %s\n",
               tab_state[frs->state], tab_state[frs->prev_state]);

	qprintf("  Interrupt Mask: %x\n", frs->intrmask);

	if (frs->intrgroup != NULL) {
		qprintf("  InterruptGroup CpuSet: 0x%llx\n",
		       intrgroup_get_cpubv(frs->intrgroup));
		qprintf("  InterruptGroup Id: 0x%d\n",
		       intrgroup_get_groupid(frs->intrgroup));
	} else {
		qprintf("  InterruptGroup: NULL\n");
	}

	if (frs->sb_intr_main) {
		qprintf("\n  Syncbar: Main\n");
		syncbar_print(frs->sb_intr_main);
	}

	if (frs->sb_intr_clear) {
		qprintf("\n  Syncbar: Clear\n");
		syncbar_print(frs->sb_intr_clear);
	}

	if (frs->sb_intr_first) {
		qprintf("\n  Syncbar: First\n");
		syncbar_print(frs->sb_intr_first);
	}

        if (frs->sdesc != NULL) {
                frs_fsched_sdesc_print(frs->sdesc);
        } else {
                qprintf("  Sdesc: NULL\n");
        }

        if (frs->sigdesc != NULL) {
                frs_fsched_sigdesc_print(frs->sigdesc);
        } else {
                qprintf("  Sigdesc: NULL\n");
        }

        if (frs->major != NULL) {
                frs_major_print(frs->major);
        } else {
                qprintf("  Major: NULL\n");
        }
}

#endif  /* EVEREST || SN0 || IP30 */
