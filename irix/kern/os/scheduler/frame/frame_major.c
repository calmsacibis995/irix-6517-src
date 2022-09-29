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
 * Frame Scheduler Major
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
#include <sys/space.h>
#include <sys/kthread.h>

#include "frame_barrier.h"
#include "frame_base.h"
#include "frame_cpu.h"
#include "frame_process.h"
#include "frame_procd.h"
#include "frame_minor.h"
#include "frame_major.h"
#include "frame_sched.h"
#include "frame_debug.h"

/**************************************************************************
 *                            frs_major_t                                 *
 **************************************************************************/

int
frs_major_create(frs_fsched_t* frs,
		 frs_fsched_info_t* info,
		 frs_intr_info_t* intr_info)
{
	frs_major_t* major;
	int minor_index;
	int n_minors;
	intrdesc_t idesc;

	/*
	 * We have to verify the number of minor frames
	 * is reasonable.
	 */
	n_minors = info->n_minors;
	if (n_minors < 1)
		return FRS_ERROR_MINOR_ZERO;

	major = (frs_major_t*) kmem_zalloc(sizeof(frs_major_t), KM_SLEEP);
	if (major == NULL)
		return (ENOMEM);

	bzero(major, sizeof(frs_major_t));

	major->n_minors = n_minors;
	major->p_minors = (frs_minor_t**)
		kmem_zalloc(n_minors*sizeof(frs_minor_t*), KM_SLEEP);

	if (major->p_minors == NULL) {
		kmem_free(major, sizeof(frs_major_t));
		return (ENOMEM);
	}

	bzero(major->p_minors, n_minors*sizeof(frs_minor_t*));

	if (intr_info == NULL) {
		idesc.source = info->intr_source;
		idesc.q.value = info->intr_qualifier;
		idesc.intrgroup = frs_get_intrgroup(frs);
	}

	for (minor_index = 0; minor_index < n_minors; minor_index++) {

		if (intr_info) {
			idesc.source = intr_info[minor_index].intr_source;
			idesc.q.value = intr_info[minor_index].intr_qualifier;
			idesc.intrgroup = frs_get_intrgroup(frs);
		}

		major->p_minors[minor_index] = frs_minor_create(&idesc);
		if (major->p_minors[minor_index] == NULL) {
			for (minor_index = 0; minor_index < n_minors; minor_index++) {
				if (major->p_minors[minor_index] != NULL) {
                                        frs_minor_destroy(major->p_minors[minor_index]);
				}
			}

			kmem_free(major->p_minors,
				  major->n_minors * sizeof(frs_minor_t*));
			kmem_free(major, sizeof(frs_major_t));
			return (ENOMEM);
		}
	}
	
	major->c_minor = major->p_minors[0];
	major->i_minor = 0;
        major->mfbe_cerr = 0;

        spinlock_init(&major->lock, "major_lock");

	frs->major = major;
	return (0);
}

int
frs_major_clone(frs_fsched_t* frs_src,
		frs_fsched_t* frs_des)
{
	frs_major_t* major;
	int minor_index;
	int n_minors;
	intrdesc_t *idesc;
	frs_minor_t **minors_src;

	ASSERT(frs_src);
	ASSERT(frs_des);
	ASSERT(frs_src->major);

	n_minors = frs_src->major->n_minors;

	major = (frs_major_t*) kmem_zalloc(sizeof(frs_major_t), KM_SLEEP);
	if (major == NULL)
		return (ENOMEM);

	bzero(major, sizeof(frs_major_t));

	major->n_minors = n_minors;
	major->p_minors = (frs_minor_t**)
		kmem_zalloc(n_minors*sizeof(frs_minor_t*), KM_SLEEP);

	if (major->p_minors == NULL) {
		kmem_free(major, sizeof(frs_major_t));
		return (ENOMEM);
	}

	bzero(major->p_minors, n_minors*sizeof(frs_minor_t*));

	minors_src = frs_src->major->p_minors;
	for (minor_index = 0; minor_index < n_minors; minor_index++) {
		idesc = &minors_src[minor_index]->idesc;
		major->p_minors[minor_index] = frs_minor_create(idesc);
		if (major->p_minors[minor_index] == NULL) {
			for (minor_index = 0; minor_index < n_minors; minor_index++) {
				if (major->p_minors[minor_index] != NULL) {
                                        frs_minor_destroy(major->p_minors[minor_index]);
				}
			}
			kmem_free(major->p_minors,
				  major->n_minors * sizeof(frs_minor_t*));
			kmem_free(major, sizeof(frs_major_t));
			return (ENOMEM);
		}
	}
	
	major->c_minor = major->p_minors[0];
	major->i_minor = 0;
        major->mfbe_cerr = 0;

        spinlock_init(&major->lock, "major_lock");

	frs_des->major = major;
	return (0);
}

void
frs_major_destroy(frs_major_t* major)
{
	int minor_index;
	
	ASSERT(major != 0);

        /*
         * Nobody should try to access
         * a frame scheduler's major if
         * sl_term is set, which must
         * be the case when we get to this
         * point. And we can get here only
         * after all refs on this frame scheduler
         * have been released. Hence no locking is
         * necessary.
         */
        
	for (minor_index = 0; minor_index < major->n_minors; minor_index++) {
                frs_minor_destroy(major->p_minors[minor_index]);
	}

        spinlock_destroy(&major->lock);

	kmem_free(major->p_minors, major->n_minors * sizeof(frs_minor_t*));
	kmem_free(major, sizeof(frs_major_t));
}

void
frs_major_enqueue(frs_major_t* major, int minor_index, frs_procd_t* procd)
{
	int major_ospl;
	
	ASSERT(major != 0);
	ASSERT(procd != 0);

	major_ospl = frs_major_lock(major);
	frs_minor_enqueue(major->p_minors[minor_index], procd);
	frs_major_unlock(major, major_ospl);
}

void
frs_major_start(frs_major_t* major)
{
	int minor_index;

	ASSERT(major != 0);
	ASSERT(major->n_minors != 0);
	ASSERT(major->p_minors != 0);

	for (minor_index = 0;  minor_index < major->n_minors; minor_index++) {
                frs_minor_reset(major->p_minors[minor_index]);
	}

	major->c_minor = major->p_minors[0];
	major->i_minor = 0;

	LOG_TSTAMP_EVENT(RTMON_FRAMESCHED, TSTAMP_EV_START_OF_MAJOR,
			 major->c_minor->base_period,
			 major->c_minor->idesc.source,
			 NULL, NULL);
}

void
frs_major_unframesched(frs_major_t* major, uthread_t *ut)
{
        frs_procd_t* procd;
	int min_idx;
        int s;
        
	ASSERT(major != 0);
        ASSERT(ut != 0);

        /*
         * frs_minor_delproc  does one process at a time.
         * It's return value tells us whether it found a procd
         * to destroy or not. If it found one, we need to re-scan
         * for other possible matches; otherwise, if it found none,
         * we know the operation is done. This is done this way so that
         * we can release the locks everytime we call frs_minor_unframesched.
         */

        do {
                s = frs_major_lock(major);
                procd = NULL;
                for (min_idx = 0; min_idx < major->n_minors; min_idx++) {
                        procd = frs_minor_delproc(major->p_minors[min_idx], ut);
			if (procd != NULL)
                                break;
                }
                frs_major_unlock(major, s);
                if (procd != NULL) {
                        /*
                         * delproc got a ref to this procd.
                         */
                        frs_procd_destroy(procd);
                }
        } while (procd != NULL);
}

void
frs_major_unframesched_allprocs(frs_major_t* major)
{
	int minor_index;
        int major_ospl;
        frs_procd_t* procd;
        
	ASSERT(major != 0);

        /*
         * frs_minor_dequeue dequeues one process at a time.
         * It's return value tells us whether it reached the end
         * of the queue. Every time a valid procd is returned,
         * we destroy it, and re-initialize the loop. The process
         * completes when we iterate over all minors and find them
         * all empty.
         */

        do {
                major_ospl = frs_major_lock(major);
                procd = NULL;
                for (minor_index = 0;  minor_index < major->n_minors; minor_index++) {
                        if ((procd = frs_minor_dequeue(major->p_minors[minor_index])) != NULL) {
                                break;
                        }
                }
                frs_major_unlock(major, major_ospl);
                if (procd != NULL) {
                        /*
                         * delproc got a ref to this procd.
                         */
                        frs_procd_destroy(procd);
                }
        } while (procd != NULL);
}        

kthread_t*
frs_major_choosethread(frs_major_t* major)
{
	uthread_t* ut = NULL;
	frs_procd_t* procd;
	int nprocs;
	int ready = 0;
	int i;

	ASSERT (major != NULL);
        ASSERT (major->c_minor != 0);

       	frs_major_nested_lock(major);

        /*
         * In order to prevent priority inversion the minor frame
	 * queue is rewound every time there's a dispatch.
         * This dispatch procedure is forced whenever
         * a thread is put back into the local runq.
         */

	frs_minor_rewind(major->c_minor);
        nprocs = frs_minor_getnprocs(major->c_minor);
 
        for (i = 0; i < nprocs; i++) {
                procd = frs_minor_getprocd(major->c_minor);
                if (procd == NULL) {
                        /*
                         * We have an empty queue, return NULL
                         */
                        break;
                }

                ut = procd->thread;
                ASSERT(ut);
                
		if (_isFSYield(ut) || !_isFSJoin(ut))
			continue;

                /*
                 * We have a thread on the minor frame queue:
                 * Is it ready to run?
                 */
                ready = cpu_frs_dequeue(&pdaindr[cpuid()].pda->p_cpu,
					UT_TO_KT(ut));

		if (ready) {
			/*
			 * Set thread frsrun flag
			 */
			_makeFSRun(ut);
			break;
		}
        }
	
	frs_major_nested_unlock(major);

	return (ready ? UT_TO_KT(ut) : NULL);
}

int
frs_major_getnminors(frs_major_t* major)
{
	int len;
	int s;
	
	ASSERT(major != 0);

	s = frs_major_lock(major);
	len = major->n_minors;
	frs_major_unlock(major, s);

	return (len);
}

int
frs_major_verifyminor(frs_major_t* major, int minor_index)
{
        int major_ospl;

        major_ospl = frs_major_lock(major);
        if (minor_index < 0 || minor_index >= major->n_minors) {
                frs_major_unlock(major, major_ospl);
                return (FRS_ERROR_MINOR_NEXIST);
        } else {
                frs_major_unlock(major, major_ospl);
                return (0);
        }
}

int
frs_major_getcminor(frs_major_t* major)
{
	ASSERT(major != 0);

	/* snapshot */
	return (major->i_minor);
}

static void
frs_major_error_recovery(frs_fsched_t* frs, int errorcode)
{
	mfbe_rdesc_t* prdesc;
	intrdesc_t *idesc;
	int target_idesc = FRS_IDESC_NEXT;
	frs_major_t* major;
	syncdesc_t* sdesc;

	major = frs->major;
	sdesc = frs->sdesc;
	prdesc = &sdesc->rdesc;

	/*
	 * Somebody either underrun or overrun
	 */

	/*
	 * Clear the global error flag after we are certain
	 * everybody has seen it.
	 */
	syncbar_wait(frs->sb_intr_clear);
	sdesc->mfbestate = 0;

	/*
	 * Now we need the frs discipline...
	 * mfberror_handling_mode....
	 * recovery
	 *     inject-frame (max_consecutive_recoveries)
	 *
	 *     extend-frame (max_consecutive_recoveries)
	 *         stretch
	 *             fixed-time 
	 *             [Maybe future: until-all-have-yielded/max-time]
	 *         steal (if time > XXX then skip or stretch)
	 *             fixed-time
	 *             [Maybe future: until-all-have-yielded/max-time]
	 *
	 * non-recovery
	 * classic send-signal approach.
	 */

	frs_major_nested_lock(major);
                
	switch (prdesc->rmode) {
	case MFBERM_NORECOVERY:
		/*
		 * We're not recovering, we abort and
		 * return an error code.
		 */
		LOG_TSTAMP_EVENT(RTMON_FRAMESCHED, TSTAMP_EV_RECV_NORECOVERY,
				 NULL, NULL, NULL, NULL);
		break;

	case MFBERM_INJECTFRAME:
		/*
		 * We have to inject a frame
		 */
		LOG_TSTAMP_EVENT(RTMON_FRAMESCHED, TSTAMP_EV_RECV_INJECTFRAME,
				 major->c_minor->base_period, 
				 NULL, NULL, NULL);
		/*
		 * First we increment the number
		 * of consecutive mfberrors, and
		 * abort recovery if this number
		 * is greater than the max.
		 */
		major->mfbe_cerr++;  
		if ((prdesc->maxcerr > 0) &&
		    (major->mfbe_cerr > prdesc->maxcerr)) {
			/*
			 * At this point we've had too many
			 * consecutive mfberrors, so we follow
			 * the "non-recovery" path.
			 */
			LOG_TSTAMP_EVENT(RTMON_FRAMESCHED,
					 TSTAMP_EV_RECV_TOOMANY,
					 major->mfbe_cerr, NULL,
					 NULL, NULL);
			break;
		}
		/*
		 * Recovered
		 */
		target_idesc = FRS_IDESC_CURRENT;
		errorcode = 0;
		break;

	case MFBERM_EXTENDFRAME_STRETCH:
		/*
		 * We have to introduce a short frame of
		 * length prdesc->xtime.
		 */
		LOG_TSTAMP_EVENT(RTMON_FRAMESCHED,
				 TSTAMP_EV_RECV_EFRAME_STRETCH,
				 prdesc->xtime, NULL, NULL, NULL);
		/*
		 * First we increment the number
		 * of consecutive mfberrors, and
		 * abort recovery if this number
		 * is greater than the max.
		 */
		major->mfbe_cerr++; 
		if ((prdesc->maxcerr > 0) &&
		    (major->mfbe_cerr > prdesc->maxcerr)) {
			/*
			 * At this point we've had too many
			 * consecutive mfberrors, so we follow
			 * the "non-recovery" path.
			 */
			LOG_TSTAMP_EVENT(RTMON_FRAMESCHED,
					 TSTAMP_EV_RECV_TOOMANY,
					 major->mfbe_cerr, NULL,
					 NULL, NULL);
			break;
		}

		major->c_minor->idesc.q.period = prdesc->xtime;
		major->c_minor->dpstate = DPS_STRETCHER;

		/*
		 * Recovered
		 */
		target_idesc = FRS_IDESC_CURRENT;
		errorcode = 0;
		break;

	case MFBERM_EXTENDFRAME_STEAL:
		/*
		 * We have to introduce a short frame of
		 * length prdesc->xtime.
		 */
		LOG_TSTAMP_EVENT(RTMON_FRAMESCHED,
				 TSTAMP_EV_RECV_EFRAME_STEAL,
				 prdesc->xtime, NULL,
				 NULL, NULL);
		/*
		 * First we increment the number
		 * of consecutive mfberrors, and
		 * abort recovery if this number
		 * is greater than the max.
		 */
		major->mfbe_cerr++; 
		if ((prdesc->maxcerr > 0) &&
		    (major->mfbe_cerr > prdesc->maxcerr)) {
			/*
			 * At this point we've had too many
			 * consecutive mfberrors, so we follow
			 * the "non-recovery" path.
			 */
			LOG_TSTAMP_EVENT(RTMON_FRAMESCHED,
					 TSTAMP_EV_RECV_TOOMANY,
					 major->mfbe_cerr, NULL,
					 NULL, NULL);
			break;
		}

		/*
		 * Are we trying to steal more than what we have?
		 */
		if ((major->mfbe_cerr * prdesc->xtime) >=
		    major->c_minor->base_period) {
			/*
			 * Yep, too much already
			 * We follow the non-recovery path
			 */
			break;
		}

		major->c_minor->idesc.q.period = prdesc->xtime;
		major->c_minor->dpstate = DPS_STEALER;

		/*
		 * Recovered
		 */
		target_idesc = FRS_IDESC_CURRENT;
		errorcode = 0;
		break;
			
	default:
		cmn_err_tag(1777, CE_NOTE,
			"[frs_major_error_recovery]: Invalid recovery mode");
	}

	idesc = frs_major_getidesc(major, target_idesc);
	frs_cpu_eventintr_reset(cpuid(), idesc);

	frs_major_nested_unlock(major);

	if (errorcode != 0) {
		/*
		 * Unrecoverable: overrun/underrun error
		 */	
		LOG_TSTAMP_EVENT(RTMON_FRAMESCHED, TSTAMP_EV_XRUN,
				 NULL, NULL, NULL, NULL);

		if (errorcode & FRS_ERROR_OVERRUN) {
			frs_thread_sendsig(&frs->control_id,
					   frs->sigdesc->sig_overrun);
		} else if (errorcode & FRS_ERROR_UNDERRUN) {
			frs_thread_sendsig(&frs->control_id,
					   frs->sigdesc->sig_underrun);
		} else {
			cmn_err(CE_PANIC,
				"[frs_fsched_eventintr]: Unknown error");
		}
	}
}

int
frs_major_step(frs_fsched_t* frs)
{
	int rv;
	int errorcode;
	mfbe_rdesc_t* prdesc;
	volatile uint* pmfbestate;
	intrdesc_t *idesc;
	dpstate_t c_dpstate;
	uint txtime;
	frs_major_t* major;
	syncdesc_t* sdesc;

	ASSERT (issplhi(getsr()) || issplprof(getsr())); 
	ASSERT (frs);

	major = frs->major;
	sdesc = frs->sdesc;

	ASSERT (major);
	ASSERT (sdesc);

	prdesc = &sdesc->rdesc;
	pmfbestate = (volatile uint*) &sdesc->mfbestate;

	if (sdesc->sl_term) {
		/*
		 * Termination detected
		 */
		sdesc->sl_term_ack = 1;
	} else if (sdesc->sl_stop)
		/*
		 * Stop detected
		 */
		sdesc->sl_stop_ack = 1;
	else {
		frs_major_nested_lock(major);
		rv = frs_minor_check(major->c_minor);
		frs_major_nested_unlock(major);
		if (rv != 0) {
			/*
			 * Overrun/Underrun error detected
			 */
			atomicSetInt((int*)pmfbestate, rv);
		}
	}

	syncbar_wait(frs->sb_intr_main);

	if (sdesc->sl_term_ack) {
		/*
		 * FRS termination acknowledged
		 */
		return (-1);
	}

        if (sdesc->sl_stop_ack) {
		/*
		 * FRS stop acknowledged. Do not step frame.
		 *
		 * Clear the public acknowledgment after we are certain
		 * everybody has seen it.
		 */
		syncbar_wait(frs->sb_intr_clear);

		sdesc->sl_stop_ack = 0;
		sdesc->mfbestate = 0;
		idesc = frs_major_getidesc(major, FRS_IDESC_CURRENT);
		frs_cpu_eventintr_reset(cpuid(), idesc);
		return (0);
	}

        if (errorcode = *pmfbestate) {
		/*
		 * Overrun/Underrun error acknowledged
		 */
		frs_major_error_recovery(frs, errorcode);
		return (0);
	}

	frs_major_nested_lock(major);
                
	if (prdesc->rmode == MFBERM_EXTENDFRAME_STRETCH ||
	    prdesc->rmode == MFBERM_EXTENDFRAME_STEAL) {
		/*
		 * Save current dpstate
		 */
		c_dpstate = major->c_minor->dpstate;
		txtime = major->mfbe_cerr * prdesc->xtime;
		
		/*
		 * Restore current minor's idesc period
		 */
		major->c_minor->idesc.q.period = major->c_minor->base_period;
		major->c_minor->dpstate = DPS_STANDARD;
	}

	/*
	 * End of frame stuff
	 */
	frs_minor_eoframe(major->c_minor);
	
	/*
	 * We have to reset the adjacent
	 * recovery operation counter.
	 */
	major->mfbe_cerr = 0;

	/*
	 * And now we advance the minor frame
	 */
	major->i_minor++;
		
	if (major->i_minor == major->n_minors) {
		/*
		 * End of Major Frame.
		 */
		frs_major_start(major);
	} else {
		/*
		 * End of Minor Frame.
		 */
		major->c_minor = major->p_minors[major->i_minor];
		ASSERT(major->c_minor != 0);
		frs_minor_start(major->c_minor);
	}

	if (prdesc->rmode == MFBERM_EXTENDFRAME_STRETCH ||
	    prdesc->rmode == MFBERM_EXTENDFRAME_STEAL) {
		/*
		 * Adjust the next minor's idesc period.
		 */
		if (c_dpstate == DPS_STEALER) {
			/*
			 * If the current minor was a stealer,
			 * we need to shrink our next minor.
			 */
			int period = major->c_minor->base_period - txtime;
			if (period <= 0)
				period = 1;
			major->c_minor->idesc.q.period = period;
			major->c_minor->dpstate = DPS_SHRANK;
		} else {
			major->c_minor->dpstate = DPS_STANDARD;
		}
	}
			
	/*
	 * Before leaving we set the event interrupt of the
	 * next interrupt source.
	 */
	idesc = frs_major_getidesc(major, FRS_IDESC_NEXT);
	frs_cpu_eventintr_reset(cpuid(), idesc);

	frs_major_nested_unlock(major);
	return (0);
}

intrdesc_t*
frs_major_getidesc(frs_major_t* major, int cmd)
{
	intrdesc_t *idesc;

	switch (cmd) {
	case FRS_IDESC_FIRST:
		idesc = &major->p_minors[0]->idesc;
		break;
	case FRS_IDESC_CURRENT:
		idesc = &major->p_minors[major->i_minor]->idesc;
		break;
	case FRS_IDESC_NEXT:
		if (major->i_minor == major->n_minors-1)
			idesc = &major->p_minors[0]->idesc;
		else
			idesc = &major->p_minors[major->i_minor+1]->idesc;
		break;
	}

	return (idesc);
}

int
frs_major_getqueuelen(frs_major_t* major, int minor, long* len)
{
	int s;
	
	ASSERT(major != 0);
	ASSERT(len != 0);

	s = frs_major_lock(major);
	
	if (minor >= major->n_minors) {
		frs_major_unlock(major, s);
		*len = 0;
		return (EINVAL);
	}

        *len = frs_minor_getnprocs(major->p_minors[minor]);
	frs_major_unlock(major, s);
	
	return (0);
}

int
frs_major_readqueue(frs_major_t* major, int minor, pid_t* pids, int maxlen)
{
	int rv;
	int s;
	
	ASSERT(major != 0);
	ASSERT(pids != 0);

	s = frs_major_lock(major);

	if (minor >= major->n_minors) {
		frs_major_unlock(major, s);
		return (FRS_ERROR_MINOR_NEXIST);
	}

	rv = frs_minor_readqueue(major->p_minors[minor], pids, maxlen);

	frs_major_unlock(major, s);

	return (rv);
}
	
int
frs_major_premove(frs_major_t* major, int minor_index, uthread_t *ut)
{
	int major_ospl;
        frs_procd_t* procd;

	ASSERT(major != 0);

        do {
                major_ospl = frs_major_lock(major);
                if (minor_index >= major->n_minors) {
                        frs_major_unlock(major, major_ospl);
                        return (FRS_ERROR_MINOR_NEXIST);
                }
                procd = frs_minor_delproc(major->p_minors[minor_index], ut);
                frs_major_unlock(major, major_ospl);
                if (procd != NULL) {
                        /*
                         * delproc got a ref to this procd.
                         */
                        frs_procd_destroy(procd);
                }
        } while (procd != NULL);

	return (0);
}

int
frs_major_pinsert(frs_major_t* major,
		  int minor,
		  pid_t basepid,
		  frs_procd_t* newprocd)
{
	int rv;
	int s;

	ASSERT(major != 0);
	ASSERT(newprocd != 0);

	s = frs_major_lock(major);

	if (minor >= major->n_minors) {
		frs_major_unlock(major, s);
		return (FRS_ERROR_MINOR_NEXIST);
	}

	rv = frs_minor_pinsert(major->p_minors[minor], basepid, newprocd);

	frs_major_unlock(major, s);

	return (rv);
}

int
frs_major_getoverruns(frs_major_t* major,
		      int minor,
		      tid_t tid,
		      frs_overrun_info_t* overrun_info)
{
        int s;
        int rv;
        
        ASSERT (major != 0);
        ASSERT (overrun_info != 0);

        s = frs_major_lock(major);
        if (minor >= major->n_minors) {
                frs_major_unlock(major, s);
                return (EINVAL);
        }

        rv = frs_minor_get_overruns(major->p_minors[minor], tid, overrun_info);

        frs_major_unlock(major, s);

        return (rv);
}
  
void
frs_major_print(frs_major_t* major)
{
	int i;
	
        if (major == 0) {
                qprintf("Null major\n");
                return;
        }
	
	qprintf("\nMajor Frame:\n");
	qprintf("  Minor Frames: %d, Current Minor: %d\n",
		major->n_minors, major->i_minor);

	for (i = 0; i < major->n_minors; i++) {
                if (major->p_minors[i] == 0) {
                        qprintf("  Null Minor (%d)\n", i);
                } else {
                        frs_minor_print(major->p_minors[i]);
                }
	}
}

#endif /* EVEREST || SN0 || IP30 */ 

