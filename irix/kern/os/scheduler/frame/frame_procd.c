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
 * Frame Scheduler - Task Descriptors
 */

#include <sys/types.h>
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
#include <sys/atomic_ops.h>
#include <sys/frs.h>

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
 *                                              Minor Frames
 *                                                -----  HEAD   DLL OF Procd's 
 *                                             -->|   |-->[ ]-->[ ]-->[ ]-->
 *                           Minor Pointers   /   |   |   [ ]<--[ ]<--[ ]<--
 * -------------    --------    ----         /    -----
 * | Frame     |--->|Major |--->|  |--------/  frs_minor_t   ***frs_prod_t***
 * | Scheduler |    |Frame |    |--|              -----
 * -------------    |      |    |  |--------------|   |-->[ ]-->[ ]-->[ ]-->
 *                  |      |    |--|              |   |   [ ]<--[ ]<--[ ]<--
 *                  --------    |  |              -----
 *                              |--|
 *                frs_major_t   |  | etc.
 *                              |--|
 *                              
 */


/**************************************************************************
 *                            frs_procd_t                                 *
 **************************************************************************/

tid_t
frs_procd_gettid(frs_procd_t *procd)
{
	tid_t tid;

	ASSERT (frs_threadid_valid(&procd->thread_id));
	tid = frs_threadid_to_tid(&procd->thread_id);
	return (tid);
}

/*
 * Create a procd object to represent a thread in a minor frame queue.
 */
frs_procd_t*
frs_procd_create(uthread_t *ut,
		 frs_threadid_t *thread_id,
                 frs_disc_t disc,
                 int sig_dequeue,
                 int sig_unframesched)
{
	frs_procd_t* procd;
	
	ASSERT(ut != 0);
	
	procd = (frs_procd_t*) kmem_zalloc(sizeof(frs_procd_t), KM_SLEEP);
	if (procd == NULL)
		return (NULL);

        bzero(procd, sizeof(frs_procd_t));

        procd->references = semabarrier_create(0);
        if (procd->references == NULL) {
                kmem_free(procd, sizeof(frs_procd_t));
                return (NULL);
        }

	/*
         * Link the thread to the procd
         */
        procd->thread = ut;
	procd->thread_id.pid = thread_id->pid;   /* XXX HACK */
	procd->thread_id.utid = thread_id->utid; /* XXX HACK */

	/*
	 * Now we check overrunability
	 */
	if (disc & FRS_DISC_OVERRUNNABLE)
		_makeFSDOverrunnable(procd);
	else
		_unFSDOverrunnable(procd);
	
	/*
	 * Check underderrunability
	 */
	if (disc & FRS_DISC_UNDERRUNNABLE)
		_makeFSDUnderrunnable(procd);
	else
		_unFSDUnderrunnable(procd);
	
	/*
	 * Check Continuability
	 */
	if (disc & FRS_DISC_CONT) {
		_makeFSDCont(procd);
		_makeFSDOverrunnable(procd);
		_makeFSDUnderrunnable(procd);
	} else
		_unFSDCont(procd);

        /*
         * Setting up the unlink signals
         */
        procd->ptsignal.sig_dequeue = sig_dequeue;
        procd->ptsignal.sig_unframesched = sig_unframesched;

        /*
         * Setting up the underrun/overrun counters
         */
	procd->underrun_counter = 0;
	procd->overrun_counter = 0;

        /*
         * Clearing the queue links
         */
	procd->fp = 0;
	procd->bp = 0;

	ASSERT (ut->ut_frs);
	procd->frs = ut->ut_frs;

	return (procd);
}

/*
 * Create an empty procd object to act
 * as a minor frame queue header.
 */

frs_procd_t*
frs_procd_create_header(void)
{
	frs_procd_t* procd;
	
	procd = (frs_procd_t*) kmem_zalloc(sizeof(frs_procd_t), KM_SLEEP);
	if (procd == NULL)
		return (NULL);

        bzero(procd, sizeof(frs_procd_t));
	
	procd->thread = 0;
	procd->underrun_counter = 0;
	procd->overrun_counter = 0;
	procd->fp = procd;
	procd->bp = procd;
	
	return (procd);
}

/*
 * Destroy a procd. Before getting rid of it
 * we have to unlink the associated process.
 */

void
frs_procd_destroy(frs_procd_t* procd)
{
	uthread_t *ut;

        ASSERT (procd != NULL);
        ASSERT (procd->thread != NULL);

        /*
         * This procd must be dequeued.
         */
        ASSERT (procd->fp == NULL);
        ASSERT (procd->bp == NULL);

        /*
         * Wait for all possible refs to be released. The wait
         * takes care of the ref acquired to get to this point.
         */
        frs_procd_waitref(procd);

        /*
         * We unlink the process. The process unlinking method has
         * callbacks into this module to send the transition signals.
	 *
	 * There is a race where the target thread and the controller
	 * thread are exiting at the same time, but the controller got
	 * to the frs tear-down code first.
	 *
	 * In this case the controller is tearing down state and
	 * performing the unlink on behalf of the target thread, which
	 * may have already exited.
	 */
	if ((ut = frs_thread_ref_acquire(&procd->thread_id)) != NULL) {
		/*
		 * The thread is still alive. Unlink it!
		 */
		ASSERT(ut == procd->thread);
		frs_thread_unlinkfrs(ut, procd);
		frs_thread_ref_release(ut);
	} else {
		/*
		 * The thread already exited, so drop the FRS reference
		 * that frs_thread_unlinkfrs would have, had the thread
		 * been alive.
		 */
		frs_fsched_decrref(procd->frs);	
	}

        semabarrier_destroy(procd->references);
        
        kmem_free(procd, sizeof(frs_procd_t));
}

/*
 * Destroy an empty procd object being used as
 * a minor queue header.
 */

void
frs_procd_destroy_header(frs_procd_t* procd)
{
        ASSERT (procd != NULL);

        /*
         * This procd must be dequeued.
         */
        ASSERT (procd->fp == procd);
        ASSERT (procd->bp == procd);        

        /*
         * All other  fields must be zero.
         */
        ASSERT (procd->thread == NULL);
        ASSERT (procd->discipline == 0);
        ASSERT (procd->ptsignal.sig_unframesched == 0);
        ASSERT (procd->ptsignal.sig_dequeue == 0);
        ASSERT (procd->underrun_counter == 0);
        ASSERT (procd->overrun_counter == 0);

        kmem_free(procd, sizeof(frs_procd_t));
}

/*
 * Callback to be used by the frs_process_framesched_unlinkfrs method
 * to send a "dequeue" transition signal to the thread being unlinked.
 */

void
frs_procd_signaldequeue(frs_procd_t *procd)
{
        ASSERT (procd != NULL);
        ASSERT (frs_threadid_valid(&procd->thread_id));

        if (procd->ptsignal.sig_dequeue)
		frs_thread_sendsig(&procd->thread_id,
				   procd->ptsignal.sig_dequeue);
}

/*
 * Callback to be used by the frs_process_framesched_unlinkfrs method
 * to send an "unframesched" transition signal to the thread being unlinked.
 */

void
frs_procd_signalunframesched(frs_procd_t *procd)
{
        ASSERT (procd != NULL);
        ASSERT (frs_threadid_valid(&procd->thread_id));

        if (procd->ptsignal.sig_unframesched)
		frs_thread_sendsig(&procd->thread_id,
				   procd->ptsignal.sig_unframesched);
}

void
frs_procd_incrref(frs_procd_t* procd)
{
        semabarrier_inc(procd->references);
}

void
frs_procd_decrref(frs_procd_t* procd)
{
        semabarrier_dec(procd->references);
}

void
frs_procd_waitref(frs_procd_t* procd)
{
        (void) semabarrier_wait(procd->references, PZERO);
}

void
frs_procd_print(frs_procd_t* procd)
{
        ASSERT (procd != NULL);
        
        printf("PROCD: uthread 0x%x tid %d\n",
	       procd->thread, frs_threadid_to_tid(&procd->thread_id));
        printf("Discipline: 0x%x\n", procd->discipline);
        printf("Sig_dequeue: %d, Sig_unframesched: %d\n",
               procd->ptsignal.sig_dequeue, procd->ptsignal.sig_unframesched);
        printf("Underruns: %d, Overruns: %d\n",
               procd->underrun_counter, procd->overrun_counter);
        printf("fp=0x%d, bp=0x%x\n",
               procd->fp, procd->bp);
        printf("References: %d\n", semabarrier_getclients(procd->references));
}
        
#endif /* EVEREST || SN0 || IP30 */ 
