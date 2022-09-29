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
 * Frame Scheduler - Minor Frames
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
#include <sys/rtmon.h>
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
 *                                          ***Minor Frames***
 *                                                -----  HEAD   DLL OF Procd's 
 *                                             -->|   |-->[ ]-->[ ]-->[ ]-->
 *                           Minor Pointers   /   |   |   [ ]<--[ ]<--[ ]<--
 * -------------    --------    ----         /    -----
 * | Frame     |--->|Major |--->|  |--------/ ***frs_minor_t***   frs_prod_t
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
 *                             <(frs_minor_t)>                            *
 **************************************************************************/

frs_minor_t*
frs_minor_create(intrdesc_t* idesc)
{
	frs_minor_t* minor;

	minor = (frs_minor_t*)kmem_zalloc(sizeof(frs_minor_t), KM_SLEEP);
	if (minor == NULL)
		return (NULL);

        bzero(minor, sizeof(frs_minor_t));

        /*
         * An ampty procd object acting as the queue header.
         * This procd object is only used as a header, never
         * as a process descriptor.
         */
	minor->qhead = frs_procd_create_header();
	if (minor->qhead == NULL) {
		kmem_free(minor, sizeof(frs_minor_t));
		return (NULL);
	}

        /*
         * We set the "current process" pointer to
         * point to the head, meaning there are
         * no processes in the queue.
         */
        minor->qcurr = minor->qhead;

        /*
         * No processes in the queue.
         */
        minor->nprocs = 0;

        /*
         * The private minor intr descriptor
         */
        minor->idesc = *idesc;
	if (_isIntTimerIdescSource(idesc))
		minor->base_period = idesc->q.period;
	else
		minor->base_period = 0;	

        /*
         * The recovery state is set to
         * STANDARD, that is, "this minor
         * is currently in a normal processing
         * mode".
         */
        minor->dpstate = DPS_STANDARD;
	
	return (minor);
}

void
frs_minor_destroy(frs_minor_t* minor)
{
	ASSERT (minor != NULL);

        /*
         * We can only get to this point if all references
         * on the frame scheduler have gone away.
         * In addition, nobody can acquire a new ref after
         * sl_term has been set. Hence, we really don't need
         * any locking here.
         */
        
        ASSERT (minor->qhead != NULL);
        ASSERT (minor->qcurr != NULL);
        /*
         * Verify the queue is empty
         */
        ASSERT (minor->qhead->fp == minor->qhead);
        ASSERT (minor->qhead->bp == minor->qhead);
        ASSERT (minor->qcurr == minor->qhead);

        frs_procd_destroy_header(minor->qhead);

	kmem_free(minor, sizeof(frs_minor_t));
}

/*
 * Enqueue a process at the tail of the minor queue
 */

void
frs_minor_enqueue(frs_minor_t* minor, frs_procd_t* procd)
{
	frs_procd_t* qhead;
	
	ASSERT(minor != 0);
	ASSERT(procd != 0);
        ASSERT(procd->thread != 0);       

	qhead = minor->qhead;
	procd->fp = qhead;
	procd->bp = qhead->bp;
	qhead->bp->fp = procd;
        frs_procd_incrref(procd);
	qhead->bp = procd;
        frs_procd_incrref(procd);
	minor->nprocs++;
}

/*
 * Dequeue the process at the head of the queue
 */

frs_procd_t*
frs_minor_dequeue(frs_minor_t* minor)
{
	frs_procd_t* qhead;
	frs_procd_t* procd;
	
	ASSERT(minor != 0);

	qhead = minor->qhead;
	procd = qhead->bp;

        if (procd == qhead) {
                /*
                 * The queue is empty
                 */
                return (NULL);
        }
        
	if (minor->qcurr == procd) {
                /*
                 * We were pointing to our victim,
                 * change curr pointer to next in queue
                 */
		minor->qcurr = procd->bp;
	}
        
	procd->bp->fp = qhead;
        frs_procd_decrref(procd);        
	qhead->bp = procd->bp;
        /* frs_procd_decrref(procd); Optimize, see below*/       
	procd->fp = 0;
	procd->bp = 0;
	minor->nprocs--;

        ASSERT (procd != NULL);
        ASSERT (procd->thread != NULL);

        /* frs_procd_incrref(procd); Optimize, see above */

	return (procd);
}


/*
 * Methods to insert/remove tasks in any position
 */

static void
frs_minor_insert(frs_minor_t* minor,
		 frs_procd_t* baseprocd,
		 frs_procd_t* newprocd)
{
        
	ASSERT(minor != 0);
	ASSERT(baseprocd != 0);
	ASSERT(newprocd != 0);
        ASSERT(newprocd->thread != 0);

	newprocd->fp = baseprocd->fp;
	newprocd->bp = baseprocd;
	baseprocd->fp->bp = newprocd;
        frs_procd_incrref(newprocd);
	baseprocd->fp = newprocd;
        frs_procd_incrref(newprocd);
	minor->nprocs++;
}

static void
frs_minor_remove(frs_minor_t* minor, frs_procd_t* procd)
{
	ASSERT(minor != 0);
        ASSERT(procd != 0);

        /*
         * Can't remove the header
         */
        ASSERT (procd != minor->qhead);
        
	if (minor->qcurr == procd) {
		minor->qcurr = procd->bp;
	}
	
	procd->bp->fp = procd->fp;
        frs_procd_decrref(procd);
	procd->fp->bp = procd->bp;
        frs_procd_decrref(procd);
	procd->fp = 0;
	procd->bp = 0;
	minor->nprocs--;
}

/*
 * Startup
 */
void
frs_minor_start(frs_minor_t* minor)
{
	ASSERT(minor != 0);

	ASSERT(minor->qhead != 0);        
	minor->qcurr = minor->qhead;
        
	LOG_TSTAMP_EVENT(RTMON_FRAMESCHED, TSTAMP_EV_START_OF_MINOR,
			 minor->base_period,
			 minor->idesc.source,
			 NULL, NULL);
}

frs_procd_t*
frs_minor_delproc(frs_minor_t* minor, uthread_t *ut)
{
        frs_procd_t* procd;
        
        ASSERT (minor != NULL);
        ASSERT (ut != NULL);
        
	for (procd=minor->qhead->fp; procd != minor->qhead; procd= procd->fp) {
		ASSERT(procd->thread != 0);
		if (procd->thread == ut) {
                        /*
                         * We found one
                         */
                        frs_procd_incrref(procd);
                        frs_minor_remove(minor, procd); 
                        return (procd);
		}
	}
	return (NULL);
}

/*
 * Queue scanning methods
 */

frs_procd_t*
frs_minor_getprocd(frs_minor_t* minor)
{
        frs_procd_t* procd;

	ASSERT(minor != 0);
	ASSERT(minor->qcurr != 0);
        
	minor->qcurr = minor->qcurr->fp;
	if (minor->qcurr == minor->qhead) {
		/* skip header */
		minor->qcurr = minor->qcurr->fp;
	}
	
	/* if it's still the head, we have an empty queue */
	if (minor->qcurr == minor->qhead) {
		/* empty queue */
                procd = NULL;
	} else {
                procd = minor->qcurr;
	}

#if 0
	/*
	 * We don't need to increment the procd ref here, since we
	 * know this routine is only called by the idle thread at
	 * splhi (via frs_major_choosethread), and the major frame
	 * lock is held while frs_major_choosethread fiddles with
	 * the procd struct.
	 */
        if (procd != NULL) {
                frs_procd_incrref(procd);
        }
#endif

        return (procd);
}
  
int
frs_minor_getnprocs(frs_minor_t* minor)
{
	ASSERT(minor != 0);
        
	return (minor->nprocs);
}

/*
 * Minor Frame Boundary Operations
 */

int
frs_minor_check(frs_minor_t* minor)
{
	uthread_t *ut;
	frs_procd_t *p;
	
	/*
	 * We can get away without locking each thread, since
	 * they are bound to this cpu, and we are currently
	 * running in the interrupt handler.
	 */

	ASSERT(issplhi(getsr()) || issplprof(getsr())); 
	ASSERT(minor != 0);
	ASSERT(minor->qhead != 0);
        
	for (p = minor->qhead->fp; p != minor->qhead; p = p->fp) {
		ASSERT(p->thread != 0);
		ut = p->thread;

		/*
		 * If this process is "Continuable", then
		 * we're supposed to ignore this minor frame boundary.
		 * For continuable processes we do not care
		 * about their FSRun or FSYield state at the end
		 * of the minor frame period. 
		 */

		if (_isFSDCont(p))
			continue;

		if (!_isFSRun(ut)) {
			/*
			 * We have an underrun error.
			 * We have to
			 * 1) Log the underrun error,
			 * 2) Check wheather this process is
			 *    catching or ignoring them.
			 * 3) Return with error if it's
			 *    supposed to be caught, or OK
			 *    otherwise.
			 */
			p->underrun_counter++;
			LOG_TSTAMP_EVENT(RTMON_FRAMESCHED,
					 TSTAMP_EV_DETECTED_UNDERRUN,
                                         p->underrun_counter, NULL, NULL, NULL);
		
			if (!_isFSDUnderrunnable(p))
				return (FRS_ERROR_UNDERRUN);

			continue;
		}

		/*
		 * At this point, the thread got to run,
		 * but it may have overrun.
		 */

		if (!_isFSYield(ut)) {
			/*
			 * We have an overrun error.
			 * WE have to
			 * 1) Log the overrun error,
			 * 2) Check wheather this process is
			 *    catching or ignoring them.
			 * 3) Return with error if it's
			 *    supposed to be caught, or OK
			 *    otherwise.
			 */
			p->overrun_counter++;
			LOG_TSTAMP_EVENT(RTMON_FRAMESCHED,
					 TSTAMP_EV_DETECTED_OVERRUN,
                                         p->overrun_counter, NULL, NULL, NULL);

			if (!_isFSDOverrunnable(p))
				return (FRS_ERROR_OVERRUN);

			continue;
		}
	}

	return (0);
}

void
frs_minor_eoframe(frs_minor_t* minor)
{
	frs_procd_t *p;
	uthread_t *ut;

	/*
	 * We can get away without locking each thread, since
	 * they are bound to this cpu, and we are currently
	 * running in the interrupt handler.
	 */

	ASSERT(issplhi(getsr()) || issplprof(getsr())); 
	ASSERT(minor != 0);
	ASSERT(minor->qhead != 0);
	
	for (p = minor->qhead->fp; p != minor->qhead; p = p->fp) {
		ASSERT(p->thread != 0);
		ut = p->thread;

		/*
		 * If this process is "Continuable", then
		 * we're supposed to ignore this minor frame boundary.
		 * For continuable processes we do not care
		 * about their FSRun or FSYield state at the end
		 * of the minor frame period. 
		 */

		if (_isFSDCont(p))
			continue;

		_unFSRun(ut);
		_unFSYield(ut);
	}
}

void
frs_minor_reset(frs_minor_t* minor)
{
	frs_procd_t *p;
	uthread_t *ut;
        int proc_ospl;
	
	ASSERT(minor != 0);
 	ASSERT(minor->qhead != 0);

	minor->qcurr = minor->qhead;
	
	/*
	 * process reference (VPROC_LOOKUP) acquired at enqueueing time
	 */
        
	for (p = minor->qcurr->fp; p != minor->qhead; p = p->fp) {
		ASSERT(p->thread != 0);
		ut = p->thread;
                proc_ospl = frs_thread_lock(ut);
		_unFSRun(ut);
		_unFSYield(ut);
		frs_thread_unlock(ut, proc_ospl);
	}
}

int
frs_minor_rewind(frs_minor_t* minor)
{
	ASSERT(minor != 0);
	ASSERT(minor->qhead != 0);

	minor->qcurr = minor->qhead;
        
	return (0);
}

/*
 * Dynamic Queue Management Methods
 */

int
frs_minor_readqueue(frs_minor_t* minor, tid_t* tids, int maxlen)
{
	frs_procd_t* p;
	int i;

	ASSERT (minor);
        ASSERT (tids);

	for (p = minor->qhead->fp, i = 0; p != minor->qhead; p = p->fp, i++) {
		ASSERT (p->thread != 0);
		if (i >= maxlen) {
			return (FRS_ERROR_MINOR_END);
		}
		tids[i] = frs_procd_gettid(p);
	}

	return (0);
}

int
frs_minor_pinsert(frs_minor_t* minor, tid_t basetid, frs_procd_t* newprocd)
{
	frs_procd_t* p;
	frs_procd_t* baseprocd = 0;

	ASSERT(minor != 0);

	if (basetid == 0) {
		baseprocd = minor->qhead->fp;
	} else {
		for (p = minor->qhead->fp; p != minor->qhead; p = p->fp) {
			if (basetid == frs_procd_gettid(p)) {
				baseprocd = p;
				break;
			}
		}
	}

	if (baseprocd) {
		frs_minor_insert(minor, baseprocd, newprocd);
		return (0);
	}

	return (FRS_ERROR_PROCESS_NFOUND);
}
		  
int
frs_minor_get_overruns(frs_minor_t *minor,
		       tid_t tid,
		       frs_overrun_info_t* overrun_info)
{
	frs_procd_t* p;
	frs_procd_t* procd = 0;
        
	ASSERT (minor != 0);
        ASSERT (overrun_info != 0);
	
	if (tid == 0) {
		procd = minor->qhead->fp;
	} else {
		for (p = minor->qhead->fp; p != minor->qhead; p = p->fp) {
			ASSERT(p->thread != 0);
			if (tid == frs_procd_gettid(p)) {
				procd = p;
				break;
			}
		}
	}

	if (procd) {
                overrun_info->overruns = procd->overrun_counter;
                overrun_info->underruns = procd->underrun_counter;
		return (0);
	}

	return (EINVAL);
}

void
frs_minor_print(frs_minor_t* minor)
{
	frs_procd_t* p;
        
        if (minor == 0) {
                qprintf("Null Minor\n");
                return;
        }

	qprintf("  Minor Frame\n");
	qprintf("    Base period %d\n", minor->base_period);
	qprintf("    Process List: nprocs %d\n", minor->nprocs);
        if (minor->qhead == 0) {
                qprintf("      Null qhead\n");
                return;
        }
	for (p = minor->qhead->fp; p != minor->qhead; p = p->fp) {
		qprintf("      [tid %d] ", frs_procd_gettid(p));
                if (p == 0) {
                        qprintf("Null procd pointer\n");
                        return;
                }
                if (_isFSRun(p->thread)) {
                        qprintf("FRun ");
                }
                if (_isFSYield(p->thread)) {
                        qprintf("FYield ");
                }
                qprintf("\n");
	}
	qprintf("\n");
}

#endif /* EVEREST || SN0 || IP30 */ 
