/*
 * FILE: irix/kern/os/scheduler/miser.c
 *
 * DESCRIPTION:
 *	Implements miser sysmp calls.
 */

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


#include <ksys/fdt.h>		/* getf() */ 
#include <ksys/sthread.h>	/* sthread_create(), st_func_t */
#include <ksys/vm_pool.h>	/* vm_pool_t, vm_pool_update_guaranteed_mem(),
				   vm_pool_resize(), vm_pool_create() */ 
#include <ksys/vproc.h>		/* miser_request_t, job_t */

#include <sys/batch.h>		/* batch_queue, batch_push_queue_locked(),
				   batch_create(), batch_init_cpus() */
#include <sys/cred.h>		/* get_current_cred() */
#include <sys/errno.h>		/* EACCES, EBUSY, EFAULT, EINTR, EINVAL, 
				   ENOMEM, EPERM, ESRCH, */
#include <sys/idbgentry.h>	/* qprintf() */
#include <sys/par.h>		/* RESCHED_Y */
#include <sys/sysmp.h>		/* MPTS_MISER_CPU, MPTS_MISER_MEMORY */
#include <sys/sysmacros.h>	/* btoc */

#include "os/proc/pproc_private.h" /* prs_flagclr(), PRSBATCH, prs_flagset(),
				   user_resched(), proc_t */	


static miser_pid = -1;
static lock_t	miser_lock;
static lock_t   miser_batch_lock;
static sv_t	miser_sv;
static quanta_t	time_quantum;
static int	miser_thread = 0;
vm_pool_t	*miser_pool;

extern	int 	batch_freecpus;
extern 	int 	batch_oldcpus;


typedef enum miser_enum {
	MISER_NEEDRESPONSE,
	MISER_NORESPONSE
} miser_response_t;


typedef struct miser_q {
	struct q_element_s mq_queue;
	job_t*		   mq_job;
	miser_response_t   mq_response;
	struct cred*	   mq_cred;
	lock_t		   mq_lock;
	sv_t		   mq_sv;
	int		   mq_error;
	int		   mq_holds;
	int 		   mq_lbolt;	/* time in HZ since last boot */
	int		   mq_alive;
	vpagg_t*	   mq_vpagg;
	miser_request_t	   mq_request;
} miser_queue_t;


struct q_element_s miser_queue;	/* being processed by daemon, not yet batch */
struct q_element_s miser_batch;	/* daemon processed and queued */


/* MACRO Definitions */
#define MISER		  ((miser_job_t *)mq->mq_request.mr_req_buffer.md_data)
#define SEG		  (MISER->mj_segments)
#define IS_SUBMIT(mq)     ((mq)->mq_request.mr_req_buffer.md_request_type \
						== MISER_USER_JOB_SUBMIT)
#define FIRST_MISER_QUEUE (miser_queue.qe_forw->qe_parent)
#define FIRST_BATCH_QUEUE (miser_batch.qe_forw->qe_parent)
#define BATCH(mq)	  (mq == (miser_queue_t*)miser_batch.qe_parent)
#define NEXT(mq)	  ((miser_queue_t*)(mq)->mq_queue.qe_forw->qe_parent)

extern int miser_cpuset_process(miser_data_t* req, sysarg_t arg, sysarg_t file);


#ifdef DEBUG
int miser_debug = 0;	/* 0 - off; 1 - on */

/* Turns on miser sysmp call tracing on console and SYSLOG */
#define MISER_DEBUG(string) if (miser_debug) \
	{ qprintf("MISER: %s (cpu %d line %d)\n", string, cpuid(), __LINE__); }
#else
#define MISER_DEBUG(void);
#endif


void
miser_init()
/*
 * Initializes global variables: miser_lock, miser_batch_lock, miser_sv, 
 * miser_queue, and miser_batch.
 *
 * Called By:
 *	batch.c        init_batchsys
 */
{
	spinlock_init(&miser_lock, "miserq");
	spinlock_init(&miser_batch_lock, "miserb");

	sv_init(&miser_sv, SV_DEFAULT, "miserg");

	init_q_element(&miser_queue, (void *)&miser_queue);
	init_q_element(&miser_batch, (void *)&miser_batch);

} /* miser_init */


miser_queue_t*
miser_find_bid(bid_t bid) 
/*
 * Find the miser_q data for the specified job id in the miser_batch queue. 
 *
 * Called By:
 *	miser.c miser_reset_job
 *	miser.c miser_respond
 *	miser.c miser_send_request_scall
 *	miser.c miser_time_expire
 *	miser.c miser_exit
 */
{
	miser_queue_t *mq;	/* miser_batch */

	ASSERT(spinlock_islocked(&miser_batch_lock));

	for (mq = FIRST_BATCH_QUEUE; !BATCH(mq); mq = NEXT(mq)) {
		if (mq->mq_request.mr_bid == bid) { 
			return mq;
		}
	}

	return 0;	/* not found */

} /* miser_find_bid */	 


static void
miser_adjust_time(miser_queue_t *mq)
/*
 * Convert the job end time secs to absolute end time in lbolt.
 *
 * Called By:
 *	miser.c miser_reset_job
 *	miser.c miser_send_request_scall
 */
{
	int i;

	for (i = 0; i < MISER->mj_count; i++) {
		SEG[i].ms_etime = SEG[i].ms_etime * HZ + mq->mq_lbolt;
	}

} /* miser_adjust_time */ 


int
miser_reset_job(sysarg_t arg)
/*
 * Called By:
 *	sysmp -> MP_MISER_SETRESOURCE
 *			MPTS_MISER_JOB		(queue.C repack_reschedule_job)
 */
{
	miser_request_t mr;
	miser_queue_t *mq;
	miser_job_t *mj;
	int s;
	int error = EINVAL;	/* invalid argument */

	if (miser_pid != current_pid())	{
		return EPERM;	/* operation not permitted */
	}

	if (copyin((caddr_t)(__psint_t) arg, &mr, sizeof(mr))) {
		return EFAULT;	/* bad address */
	}

 	mj = (miser_job_t *) &mr.mr_req_buffer.md_data;

	s = mutex_spinlock(&batch_queue.b_lock);
	nested_spinlock(&miser_batch_lock);
	mq = miser_find_bid(mj->mj_bid);

	if (!mq) {
		nested_spinunlock(&miser_batch_lock);
		mutex_spinunlock(&batch_queue.b_lock, s);
		return error;
	}

	nested_spinlock(&mq->mq_lock);	

	if (!mq->mq_job) {
		nested_spinunlock(&mq->mq_lock);
		nested_spinunlock(&miser_batch_lock);
		mutex_spinunlock(&batch_queue.b_lock, s);
		return error;
	} 

	rmq(&mq->mq_queue);
	job_nested_lock(mq->mq_job);

	if (mq->mq_job->b_time > lbolt) {
		rmq(&mq->mq_job->j_queue);
		bcopy(mr.mr_req_buffer.md_data, 
			mq->mq_request.mr_req_buffer.md_data,
			sizeof(mr.mr_req_buffer.md_data));
		
		mq->mq_lbolt = lbolt;	
		miser_adjust_time(mq);
		batch_set_params(mq->mq_job, SEG);
		batch_push_queue_locked(mq->mq_job);
		error = 0;	/* success */
	}

	q_insert_before(&miser_batch, &mq->mq_queue);

	job_nested_unlock(mq->mq_job);
	nested_spinunlock(&mq->mq_lock);
	nested_spinunlock(&miser_batch_lock);
	mutex_spinunlock(&batch_queue.b_lock, s);	

	return error;

} /* miser_reset_job */


static void
miser_queue_release(miser_queue_t *mq)
/*
 * Destroy lock and signal, and free memory occupied by the miser_queue.
 *
 * Called By:
 *	miser.c mq_is_okay
 *	miser.c miser_respond
 *	miser.c miser_send_request_scall
 *	miser.c miser_return
 */
{
	MISER_DEBUG("[miser_queue_release]");

	spinlock_destroy(&mq->mq_lock);
	sv_destroy(&mq->mq_sv);
	kmem_free(mq, sizeof(*mq));

} /* miser_queue_release */


INLINE int
mq_is_okay(miser_queue_t *mq)
/*
 * Called By:
 *	miser.c miser_get_request
 */
{
	do {
		nested_spinlock(&mq->mq_lock);

		if (mq->mq_holds == 1 
				&& mq->mq_response == MISER_NEEDRESPONSE) {
			rmq(&mq->mq_queue);
			mq->mq_holds--;
			nested_spinunlock(&mq->mq_lock);
			miser_queue_release(mq);
			mq = FIRST_MISER_QUEUE;
			continue;
		} else {
			return 1;	/* error */
		}

	} while(!q_empty(&miser_queue));

	return 0;	/* success */

} /* mq_is_okay */


static void 
miser_sched_preprocess(miser_queue_t *mq, pid_t pgid)
/*
 * Assign passed in pgid to the miser submit request.
 *
 * Called By:
 *	miser.c miser_send_request_scall
 */
{
	mq->mq_request.mr_bid = pgid;
	PID_BATCH_JOIN(pgid);

} /* miser_sched_preprocess */


static void
miser_sched_postprocess(miser_queue_t *mq)
/*
 * Set mq_lbolt value for miser submit request.
 *
 * Called By:
 *	miser.c miser_return
 */
{
	if (mq->mq_request.mr_req_buffer.md_request_type 
			== MISER_USER_JOB_SUBMIT) {
		mq->mq_lbolt = lbolt;	/* time in HZ since last boot */
	}

} /* miser_sched_postprocess */


static int
miser_return(miser_queue_t *mq)
/*
 * Called By:
 *	miser.c miser_respond
 *	miser.c miser_return_error
 */
{
	int s = mutex_spinlock(&mq->mq_lock);
	
	MISER_DEBUG("[miser_return]");

	mq->mq_holds--;

	if (mq->mq_holds == 1) {
		miser_sched_postprocess(mq);
		sv_signal(&mq->mq_sv);
		mq->mq_error = 0;
		mutex_spinunlock(&mq->mq_lock, s);
	} else {
		miser_queue_release(mq);
		splx(s);
	}

	return 0;	/* success */

} /* miser_return */


static void
miser_return_error(int error)
/*
 * Set the passed in error value to mq_error and return.
 * 
 * Called By:
 *	miser.c miser_get_request
 *	miser.c miser_respond
 */
{
	int s = mutex_spinlock(&miser_lock);
	miser_queue_t *mq = FIRST_MISER_QUEUE;

	MISER_DEBUG("[miser_return_error]");

	rmq(&mq->mq_queue);
	mutex_spinunlock(&miser_lock,s);

	mq->mq_error = error;
	(void)miser_return(mq);

} /* miser_return_error */


int
miser_get_request(sysarg_t arg)
/*
 * Miser daemon waits on MP_MISER_GETREQUEST in a for loop.  This function
 * waits on sv_wait_sig until it finds something in miser_queue, which it
 * sends to the miser daemon in the mq_request.
 *
 * Called By:
 *	sysmp -> MP_MISER_GETREQUEST	(main.C  main)
 */
{
	miser_queue_t *mq;
	int s;

	MISER_DEBUG("[miser_get_request]");

	if (miser_pid == -1) {
		return ESRCH;	/* no such process */
	}

	if (miser_pid != current_pid()) {
		return EBUSY;	/* resource busy */
	}

	s = mutex_spinlock(&miser_lock);

	while (q_empty(&miser_queue) || !mq_is_okay(FIRST_MISER_QUEUE)) {

		if (sv_wait_sig(&miser_sv, PZERO + 1, &miser_lock, s) == -1) {
			return EINTR;	/* interrupted function call */
		}

		s = mutex_spinlock(&miser_lock);
	}

	mq = FIRST_MISER_QUEUE;

        if (copyout(&mq->mq_request, (caddr_t)(__psint_t)arg,
			sizeof(mq->mq_request))) {
                nested_spinunlock(&mq->mq_lock);
                miser_return_error(EFAULT);
                mutex_spinunlock(&miser_lock, s);
                return EFAULT;	/* bad address */
        }

        nested_spinunlock(&mq->mq_lock);
        mutex_spinunlock(&miser_lock, s);

	return 0;	/* success */

} /* miser_get_request */


static void
miser_to_weightless(miser_queue_t *mq)
/*
 * Release a miser job from its batch pool.
 *
 * Called By:
 *	miser.c miser_handle_exception
 */
{
	vpgrp_t *vpgrp = VPGRP_LOOKUP(mq->mq_request.mr_bid);

	MISER_DEBUG("[miser_to_weightless]");

	if (vpgrp) {
		VPGRP_CLEARBATCH(vpgrp);
		VPGRP_RELE(vpgrp);
	}

	batch_to_weightless(mq->mq_job);

} /* miser_to_weightless */


static void
miser_handle_exception(miser_queue_t *mq)
/*
 * Only exception handling policy available is to make the miser job 
 * weightless.
 *
 * Called By:
 *	miser.c miser_respond
 */
{
	MISER_DEBUG("[miser_handle_exception]");

	ASSERT(SEG);

	if (SEG->ms_flags & MISER_EXCEPTION_WEIGHTLESS) {
		miser_to_weightless(mq);
	}

} /* miser_handle_exception */


int
miser_respond(sysarg_t arg)
/*
 * This function is initiated by the miser daemon via sysmp call with
 * flag MP_MISER_RESPOND once it has finished processing the service
 * requested and result is passed back in arg.
 *
 * Called By:
 *	sysmp -> MP_MISER_RESPOND	(main.C  main)
 */
{
	int s;
	miser_queue_t *mq, *mqbid; 

	MISER_DEBUG("[miser_respond]");

        if (miser_pid != current_pid()) {
                return EBUSY;	/* resource busy */
	}

	s  = mutex_spinlock(&miser_lock);
	mq = FIRST_MISER_QUEUE;

	ASSERT(mq);
	nested_spinlock(&mq->mq_lock);

        if (mq->mq_response == MISER_NEEDRESPONSE && mq->mq_holds == 1) {
                rmq(&mq->mq_queue);
                nested_spinunlock(&mq->mq_lock);
                mutex_spinunlock(&miser_lock, s);
                return EINVAL;	/* invalid argument */
        }

	if (copyin((caddr_t)(__psint_t)arg, &mq->mq_request, 
			sizeof(mq->mq_request))) {
                nested_spinunlock(&mq->mq_lock);
		miser_return_error(EFAULT);
		mutex_spinunlock(&miser_lock, s);
		return EFAULT;	/* bad address */
	}

	rmq(&mq->mq_queue);
	nested_spinunlock(&mq->mq_lock);
	mutex_spinunlock(&miser_lock, s);

	if (mq->mq_request.mr_req_buffer.md_request_type 
			== MISER_KERN_TIME_EXPIRE) {
		miser_handle_exception(mq);
	}

	if (mq->mq_request.mr_req_buffer.md_request_type == MISER_KERN_EXIT) {
		s = mutex_spinlock(&miser_batch_lock);
		mqbid = miser_find_bid(mq->mq_request.mr_bid);
		ASSERT(mq);
		rmq(&mqbid->mq_queue);
		mutex_spinunlock(&miser_batch_lock, s);
		PID_BATCH_LEAVE(mqbid->mq_request.mr_bid);
		miser_queue_release(mqbid);
	}
				
	return miser_return(mq);	/* 0 - success */

} /* miser_respond */


void
miser_cleanup(vpagg_t *vpagg)
/*
 * Do necessary cleanup after the last reference to the object goes away.
 *
 * Called By:
 *	ppag.c ppag_destroy
 */
{
	pgno_t rss_limit;

	MISER_DEBUG("[miser_cleanup]");

	VPAG_GET_VM_RESOURCE_LIMITS(vpagg, &rss_limit);
	vm_pool_update_guaranteed_mem(miser_pool, -rss_limit);

} /* miser_cleanup */


void 
miser_send_request_error(miser_queue_t *mq)
/*
 * In the event of an error during miser job submit, clear the flags and
 * resources set in the submission process.
 *
 * Called By:
 *	miser.c miser_send_request_scall
 */
{
	vpgrp_t *vpgrp;

	MISER_DEBUG("[miser_send_request_error]");

	if (mq->mq_request.mr_req_buffer.md_request_type
			== MISER_USER_JOB_SUBMIT) {
		prs_flagclr(&curuthread->ut_pproxy->prxy_sched, PRSBATCH);

		vpgrp = VPGRP_LOOKUP(mq->mq_request.mr_bid);

		if (vpgrp) {
			VPGRP_CLEARBATCH(vpgrp);
			VPGRP_RELE(vpgrp);
		}

		PID_BATCH_LEAVE(mq->mq_request.mr_bid);
		mq->mq_vpagg = 0;
	}

} /* miser_send_request_error */


static miser_queue_t *
miser_queue_get(enum miser_enum type)
/*
 * Returns a pointer to newly allocated memory of miser_queue_t structure,
 * initialized.
 *
 * Called By:
 *	miser.c miser_send_request_scall
 *	miser.c miser_send_request
 */
{
	miser_queue_t *mq = kmem_zalloc(sizeof(*mq), KM_SLEEP);

	spinlock_init(&mq->mq_lock, "miserq");

	sv_init(&mq->mq_sv, SV_DEFAULT, "miserq");
	init_q_element(&mq->mq_queue, mq);

	mq->mq_alive    = 1;
	mq->mq_response = type;

	if (type == MISER_NEEDRESPONSE) {
		mq->mq_holds = 2;
		mq->mq_error = 0;
	} else {
		mq->mq_holds = 1;
	}

	return mq;

} /* miser_queue_get */


static void
miser_drop_request(miser_queue_t *mq)
/*
 * Drop a request to miser daemon, wake it up. 
 *
 * NOTE: Any miser request that requires credentials needs to acquire
 * them prior to getting to drop request, since drop request is not 
 * a place you can go to sleep in.
 * 
 * Called By:
 *	miser.c miser_send_request_scall
 *	miser.c miser_send_request
 */
{
	int s = mutex_spinlock(&miser_lock);

	if (q_empty(&miser_queue)) {
		sv_signal(&miser_sv);
	}

	q_insert_before(&miser_queue, &mq->mq_queue);
	mutex_spinunlock(&miser_lock, s);

} /* miser_drop_request */


int
miser_send_request_scall(sysarg_t arg, sysarg_t file)
/* 
 * Identify the service request and take appropriate action by making 
 * function calls.
 *
 * Called By:
 *	sysmp	-> MP_MISER_SENDREQUEST		(miser cmds and third party)
 */
{
	int error = 0;
	int s;
	miser_queue_t *mq;

	vproc_t *vpr;
	vpgrp_t *vpgrp;
	vp_get_attr_t attr;

	MISER_DEBUG("[miser_send_request_scall]");

	vpr = VPROC_LOOKUP(current_pid());

	if (vpr == 0) { 
		return ESRCH;	/* no such process */
	}

	VPROC_GET_ATTR(vpr, VGATTR_PGID, &attr);
	VPROC_RELE(vpr);
	ASSERT(!issplhi(getsr()));

	mq = miser_queue_get(MISER_NEEDRESPONSE);
	mq->mq_cred = get_current_cred();

	if (copyin((caddr_t)(__psint_t)arg, &mq->mq_request.mr_req_buffer,
			sizeof(mq->mq_request.mr_req_buffer))) {
		miser_queue_release(mq);
		return EFAULT;	/* bad address */
	}

	switch (mq->mq_request.mr_req_buffer.md_request_type) 
	{	
		case MISER_CPUSET_CREATE:
		case MISER_CPUSET_DESTROY:
		case MISER_CPUSET_LIST_PROCS:
		case MISER_CPUSET_MOVE_PROCS:
		case MISER_CPUSET_ATTACH:
		case MISER_CPUSET_QUERY_CPUS:
		case MISER_CPUSET_QUERY_NAMES:
		case MISER_CPUSET_QUERY_CURRENT:
			error = miser_cpuset_process(
					&mq->mq_request.mr_req_buffer, 
					arg, file);
			miser_queue_release(mq);
			return error;
	}

	if (miser_pid == -1) {
		miser_queue_release(mq);
		return ESRCH;	/* no such process */
	}

	if (mq->mq_request.mr_req_buffer.md_request_type
			 == MISER_USER_JOB_SUBMIT) {
		/* 
		 * Try to set the process group id to be the bid. 
		 */
		vpgrp = VPGRP_LOOKUP(attr.va_pgid);

		if (vpgrp) {
			error = VPGRP_SETBATCH(vpgrp);
		} else {
			miser_queue_release(mq);
			return EFAULT;	/* bad address */
		}

		if (error) {
			miser_queue_release(mq);
			VPGRP_RELE(vpgrp);
			return error;
		}	

        	prs_flagset(&curuthread->ut_pproxy->prxy_sched, PRSBATCH);
		VPGRP_RELE(vpgrp);

		miser_sched_preprocess(mq, attr.va_pgid);

		s = kt_lock(curthreadp);

		if (!cpuset_isglobal(curthreadp->k_cpuset) || 
				KT_ISMR(curthreadp)) {
			error = EBUSY;
		}

		kt_unlock(curthreadp, s);

		if (error) {
			miser_send_request_error(mq);
			miser_queue_release(mq);
			return error;
		}

		/* Create a process aggregate object for miser */
		mq->mq_vpagg = vpag_miser_create(SEG->ms_resources.mr_memory);

		if (!mq->mq_vpagg) {
			miser_send_request_error(mq);
			miser_queue_release(mq);
			return ENOMEM;	/* not enough space */
		}

	} /* if */
	
	s = mutex_spinlock(&mq->mq_lock);

	miser_drop_request(mq);

	while (mq->mq_holds > 1) {
		if (sv_wait_sig(&mq->mq_sv, PZERO + 1, &mq->mq_lock, s) == -1) {
			int i = 1;

			s = mutex_spinlock(&miser_lock);
			nested_spinlock(&mq->mq_lock);
			i = --mq->mq_holds;
			nested_spinunlock(&mq->mq_lock);
			mutex_spinunlock(&miser_lock, s);

			if (!i) {
				miser_send_request_error(mq);
			}

			return EINTR;	/* interrupted function call */
		} /* if */	

		s = mutex_spinlock(&mq->mq_lock);

	} /* while */

	error = mq->mq_error;

	if (!(error = mq->mq_error)) {
		if (copyout(&mq->mq_request.mr_req_buffer, 
				(caddr_t)(__psint_t)arg,
				sizeof(mq->mq_request.mr_req_buffer))) {
			/* 
			 * oh well, failed to copy the buffer,
			 * no reason not to go on... 
			 */
			error = EFAULT;	/* bad address */
		}
	} else {
		mutex_spinunlock(&mq->mq_lock, s);
		miser_send_request_error(mq);
		miser_queue_release(mq);	
		/*
		 * message failed to be delivered, up to the
		 * daemon to figure out what to do. It already
		 * knows the message was failed. 
		 */
		return error;
	}

	if (mq->mq_request.mr_req_buffer.md_request_type 
			== MISER_USER_JOB_SUBMIT
			&& mq->mq_request.mr_req_buffer.md_error 
			== MISER_ERR_OK) {
 
		ASSERT(issplhi(getsr()));
		kt_nested_lock(curthreadp);	

		batch_create();
		ASSERT(issplhi(getsr()));

		mq->mq_job = curuthread->ut_job;
        	curuthread->ut_job->b_bid = mq->mq_request.mr_bid;
		miser_adjust_time(mq);

		nested_spinlock(&miser_batch_lock);
		q_insert_before(&miser_batch, &mq->mq_queue);
		nested_spinunlock(&miser_batch_lock);

		job_nested_lock(mq->mq_job);
		mq->mq_job->b_vpagg = mq->mq_vpagg;
		kt_nested_unlock(curthreadp);
		batch_set_params(mq->mq_job, SEG);
		batch_push_queue(mq->mq_job);
		job_nested_unlock(mq->mq_job);
		mutex_spinunlock(&mq->mq_lock,s);

		user_resched(RESCHED_Y);
		return error;

	} else if (mq->mq_request.mr_req_buffer.md_request_type 
			== MISER_USER_JOB_KILL && 
			mq->mq_request.mr_req_buffer.md_error 
			== MISER_ERR_OK) {

		bid_t *b = (bid_t*) mq->mq_request.mr_req_buffer.md_data;

		int s = mutex_spinlock(&miser_batch_lock);	
		miser_queue_t *mq2 = miser_find_bid(*b);

		if (mq2) {
			mq2->mq_alive = 0;
		}

		mutex_spinunlock(&miser_batch_lock, s);
	}	

	splx(s);
	miser_send_request_error(mq);
	miser_queue_release(mq);
	
	return error;

} /* miser_send_request_scall */


void
miser_send_request(pid_t bid, int type, caddr_t buffer, int length)
/*
 * Prepare message for miser daemon and call function to wake it up.
 *
 * Called By:
 *	miser.c miser_time_lock_expire
 *	miser.c miser_exit
 *	miser.c miser_inform
 */
{
	miser_queue_t *mq;

	MISER_DEBUG("[miser_send_request]");

	mq = miser_queue_get(MISER_NORESPONSE);
	mq->mq_request.mr_req_buffer.md_request_type = type;
	mq->mq_request.mr_bid = bid;
	bcopy(buffer, mq->mq_request.mr_req_buffer.md_data, length);
	miser_drop_request(mq);

} /* miser_send_request */


int
miser_get_quantum(sysarg_t arg)
/*
 * Value of time_quantum is sent back.
 *
 * Called By:
 *	sysmp -> MP_MISER_GETRESOURCE
 *			MPTS_MISER_QUANTUM	(miser.c miser_init)
 * 						(main.C  clear_resources)
 *						(queue.C _qdef_alloc)
 */
{
	MISER_DEBUG("[miser_get_quantum]");

	if (copyout((caddr_t)&time_quantum, (caddr_t)(__psint_t)arg,
			sizeof(time_quantum))) {
		return EFAULT;	/* bad address */
	} else {
		return 0;	/* success */
	}

} /* miser_get_quantum */


int 
miser_get_bids(sysarg_t arg) 
/*
 * Returns a list of active bids attached any of the miser queues.
 *
 * Called By:	
 *	sysmp -> MP_MISER_GETRESOURCE
 *			MPTS_MISER_BIDS (main.C  clear_resources)  
 *					(queue.C miser_rebuild)
 */
{
	miser_request_t mgr;
	miser_queue_t   *mq;
	miser_bids_t    *mb;

	int start = 1;
	int s     = 0;

	MISER_DEBUG("[miser_get_bids]");

	if (miser_pid != current_pid()) {
		return EPERM;	/* operation not permitted */
	}

	if (copyin((caddr_t)(__psint_t)arg, &mgr, sizeof(mgr))) {
		return EFAULT;	/* bad address */
	}
	
	mb = (miser_bids_t*) &mgr.mr_req_buffer.md_data; 

	if (mb->mb_first) {
		start = 0;
	}

	s = mutex_spinlock(&miser_batch_lock); 

	for (mq = FIRST_BATCH_QUEUE; !BATCH(mq); mq = NEXT(mq)) {

		if (start && mq->mq_alive) {
			mb->mb_buffer[mb->mb_count] = MISER->mj_bid;
			mb->mb_count++;

		} else if (mb->mb_first == MISER->mj_bid) {
			start = 1;
		}
	}
	
	mutex_spinunlock(&miser_batch_lock, s); 

	if (!start) {
		return EINVAL;	/* invalid argument */
	}

	if (copyout(&mgr, (caddr_t)(__psint_t)arg, sizeof(mgr))) {
		return EFAULT;	/* bad address */
	}

	return 0;	/* success */

} /* miser_get_bids */


int 
miser_get_job(sysarg_t arg)
/*
 * Find send back miser job structure to the miser daemon.
 *
 * Called By:
 *	sysmp	-> MPTS_MISER_JOB	(queue.C reschedule_job)
 *					(queue.C repack_reschedule_job)
 */
{
	miser_request_t mgr;
	miser_queue_t   *mq;
	miser_job_t     *mj;	

	int s, i;

	MISER_DEBUG("[miser_get_job]");

	if (miser_pid != current_pid()) {
		return EPERM;	/* operation not permitted */
	}
	
	if (copyin((caddr_t)(__psint_t)arg, &mgr, sizeof(mgr)))	{
		return EFAULT;	/* bad address */
	}
	
	mj = (miser_job_t*) &mgr.mr_req_buffer.md_data;
	s  = mutex_spinlock(&miser_batch_lock);

	for (mq = FIRST_BATCH_QUEUE; !BATCH(mq); mq = NEXT(mq)) {

		if (mj->mj_bid == MISER->mj_bid) {
			nested_spinlock(&mq->mq_lock);
			bcopy(MISER, mj, sizeof(miser_job_t));
			nested_spinunlock(&mq->mq_lock);

			for (i = 0; i < MISER->mj_count; i++) {
				mj->mj_segments[i].ms_etime =
					(MISER->mj_segments[i].ms_etime 
					- mq->mq_lbolt) / HZ;
			}

			if (copyout(&mgr, (caddr_t)(__psint_t)arg, 
					sizeof(mgr))) {
				mutex_spinunlock(&miser_batch_lock, s);
				return EFAULT;	/* bad address */
			}

			mutex_spinunlock(&miser_batch_lock, s);
			return 0;	/* success */
		} /* if */

	} /* for */

	mutex_spinunlock(&miser_batch_lock, s);

	return ESRCH;	/* no such process */

} /* miser_get_job */


/*ARGSUSED*/
static void
release_miser(void *arg)
/*
 * Reinitialize global variables.
 *
 * Called By:
 *	miser.c miser_set_quantum
 */
{
	MISER_DEBUG("[release_miser]");

	time_quantum = 0;
	miser_pid    = -1;
	batch_cpus   = 0;

} /* release_miser */

		
int
miser_set_quantum(sysarg_t arg)
/*
 * Set quantum unit passed as parameter.
 *
 * Called By:	
 *	sysmp -> MP_MISER_SETRESOURCE
 *			MPTS_MISER_QUANTUM 	(main.C  clear_resources)
 *						(queue.C _qdef_alloc)
 */
{
	quanta_t quantum;
	int      error;
	int      s;

	extern int batchd_pri;

	MISER_DEBUG("[miser_set_quantum]");

	if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT)) {
		return EPERM;	/* operation not permitted */
	}

	if (miser_pid != -1) {
		return EBUSY;	/* Resource busy */
	}

	error = add_exit_callback(current_pid(), 0, release_miser, 0);

	if (error) {
		return error;
	}

	s = mutex_spinlock(&miser_lock);

	if (miser_pid == -1) {
		miser_pid = current_pid();
	}

	mutex_spinunlock(&miser_lock, s);

	if (!miser_thread) {
		miser_thread = 1;
		sthread_create("batchd", NULL, 4096, 0, batchd_pri, KT_PS,
				(st_func_t *)batchd, 0, 0, 0, 0);
	}

	/* start sched tick sthread right here */
	if (miser_pid != current_pid())	{
		return EBUSY;	/* resource busy */
	}

	if (copyin((void *)(__psint_t)arg, (caddr_t)&quantum, 
			sizeof(quantum))) {
		return EFAULT;	/* bad address */
	}

	if (quantum <= 0 || time_quantum != 0 && quantum != time_quantum) {
		return EINVAL;	/* invalid argument */
	}

	time_quantum = quantum;

	return 0;	/* success */

} /* miser_set_quantum */


int   
miser_set_resources(sysarg_t resource, sysarg_t arg) 
/*
 * Initialize/reset # of miser CPUs and amount of miser Memory.
 *
 * Called By:
 *	sysmp -> MP_MISER_SETRESOURCE
 *			MPTS_MISER_MEMORY
 *			MPTS_MISER_CPU		(main.C  clear_resources)
 *						(main.C  main)
 */
{
	int s, isolated = 0, i;
	miser_resources_t mr;

	MISER_DEBUG("[miser_set_resources]");

	if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT)) {
		return EPERM;	/* operation not permitted */
	}

	if (copyin((void*)(__psint_t) arg, (caddr_t)&mr, sizeof(mr))) {
		return EFAULT;	/* bad address */
	}

	if (resource == MPTS_MISER_CPU) {
			
		s = mutex_spinlock(&batch_isolatedlock);	

		for (i = 0; i < maxcpus; i++) {
			if (!cpu_enabled(i) || cpu_restricted(i)) {
				isolated++;
			}
		}

		if (mr.mr_cpus + isolated > maxcpus) {
			mutex_spinunlock(&batch_isolatedlock, s);
			return EINVAL;	/* invalid argument */ 
		}

		batch_init_cpus(mr.mr_cpus);
		mutex_spinunlock(&batch_isolatedlock, s);

	} else if (resource == MPTS_MISER_MEMORY) {
		if (miser_pool) {
			return vm_pool_resize(miser_pool, btoc(mr.mr_memory));
		}

		miser_pool = vm_pool_create(btoc(mr.mr_memory)); 

		if (!miser_pool) {
			return ENOMEM;	/* not enough space */
		}
	}

	return 0;	/* success */

} /* miser_set_resources */


int
miser_check_access(sysarg_t file, sysarg_t atype)
/*
 * Verify miser config file access as expected.
 *
 * Called By:
 *	sysmp -> MP_MISER_CHECKACCESS	(libmiser/miser.c miser_checkaccess)
 */
{
	vfile_t *fp;
	int error;
	int mode;
	miser_queue_t *mq;
	int s;

	MISER_DEBUG("[miser_check_access]");

	if (q_empty(&miser_queue)) {
		return EINVAL;	/* invalid argument */
	}

	if (error = getf(file, &fp)) {
		return error;
	}

	if (!VF_IS_VNODE(fp)) {
		return EACCES;	/* permission denied */
	}

	mode = ((atype & (R_OK|W_OK|X_OK)) << 6);
	s    = mutex_spinlock(&miser_lock);
	mq   = FIRST_MISER_QUEUE;

	nested_spinlock(&mq->mq_lock);

	if (mq->mq_holds == 1) {
		nested_spinunlock(&mq->mq_lock);
		mutex_spinunlock(&miser_lock, s);
		return ESRCH;	/* no such process */
	}

	nested_spinunlock(&mq->mq_lock);
	mutex_spinunlock(&miser_lock, s);
	VOP_ACCESS(VF_TO_VNODE(fp), mode, mq->mq_cred, error);

	return error;

} /* miser_check_access */


static void 
miser_time_lock_expire(miser_queue_t *mq)
/*
 * Notify miser daemon when time expires for a miser job.
 *
 * Called By:
 *	miser.c miser_time_expire
 *	miser.c miser_purge
 */
{
	job_t *j = mq->mq_job;

	MISER_DEBUG("[miser_time_lock_expire]");

	ASSERT(spinlock_islocked(&miser_batch_lock));

	if (--MISER->mj_count <= 0 && j) {
		job_nested_lock(j);

		if (MISER->mj_count >= 0) {
			batch_release_resources(j);
			miser_send_request(MISER->mj_bid,
				MISER_KERN_TIME_EXPIRE, (caddr_t)MISER,
				sizeof(*MISER));
		}	

		if (j->j_nthreads > 0) {
			sv_broadcast(&j->j_sv);
		}

		job_nested_unlock(j);

	} else {
		ASSERT(issplhi(getsr()));
		ASSERT(MISER->mj_count > 0);

		bcopy(SEG + 1, SEG, MISER->mj_count * sizeof(*SEG));
		job_nested_lock(j);
		batch_set_params(j, SEG);
		job_nested_unlock(j);
	}

} /* miser_time_lock_expire */


void
miser_time_expire(job_t *j)
/*
 * Make call to miser_time_lock_expire to inform miser daemon about the
 * time expired job.  This can be used by other kernel function to send
 * a MISER_KERN_TIME_EXPIRE message to miser daemon for a specific job.
 *
 * Called By:
 *	NONE	(appears in the miser_public.h for other kernel functions)
 */
{
	miser_queue_t *mq;
	int s = mutex_spinlock(&miser_batch_lock);

	MISER_DEBUG("[miser_time_expire]");

	mq = miser_find_bid(j->b_bid);

	if (!mq) {
		mutex_spinunlock(&miser_batch_lock, s);
		return;
	}

	miser_time_lock_expire(mq);
	mutex_spinunlock(&miser_batch_lock, s);

} /* miser_time_expire */


void
miser_purge(void) 
/*
 * This function is called by the batchd in a fixed interval to check
 * for if any job has reached its deadline.  If found it calls 
 * miser_time_lock_expire to take appropriate action.
 *
 * Called By:
 *	batch.c batchd
 */
{
	miser_queue_t *mq;

	int s = mutex_spinlock(&miser_batch_lock);

	for (mq = FIRST_BATCH_QUEUE; !BATCH(mq); mq = NEXT(mq)) {
		nested_spinlock(&mq->mq_lock);

		if (mq->mq_job &&  mq->mq_job->j_nthreads 
				&& mq->mq_job->b_deadline < lbolt) {
			miser_time_lock_expire(mq);
		}

		nested_spinunlock(&mq->mq_lock);
	} /* for */

	mutex_spinunlock(&miser_batch_lock, s);

} /* miser_purge */


void
miser_exit(job_t *j)
/*
 * Exiting miser job structure is updates and reported to the miser daemon. 
 *
 * Called By:
 *	batch.c batch_detach
 */
{
	miser_queue_t *mq;

	MISER_DEBUG("[miser_exit]");
	
	nested_spinlock(&miser_batch_lock);
	mq = miser_find_bid(j->b_bid);
	nested_spinlock(&mq->mq_lock);
	mq->mq_holds--;
	mq->mq_job = 0;
	nested_spinunlock(&mq->mq_lock);
	nested_spinunlock(&miser_batch_lock);

	batch_release_resources(j);
	miser_send_request(j->b_bid, MISER_KERN_EXIT, (caddr_t) &(j->b_bid), 
				sizeof (j->b_bid));

} /* miser_exit */


void 
miser_inform(int16_t flags)
/*
 * Miser job MISER_KERN_MEMORY_OVERRUN/flag is sent to the miser daemon.
 *
 * Called By:
 *	fault.c        pas_pfault
 *	pas_addspace.c pas_addstackexec
 *	region.c       dupreg
 */
{
	job_t *j = curuthread->ut_job;

	miser_send_request(j->b_bid, flags, (caddr_t) &(j->b_bid),
				sizeof (j->b_bid));

} /* miser_inform */


void 
idbg_mqb_disp(struct q_element_s *qe)
/*
 * Called By:
 *	disp_idbg.c doq
 */
{
	struct miser_batch *mb = (struct miser_batch*)qe;

	MISER_DEBUG("[idbg_mqb_disp]");

	qprintf("	job 0x%x\n", ((miser_queue_t *)mb)->mq_job);

} /* idbg_mqb_disp */


void 
idbg_vpagg(__psint_t x)
/*
 * Called By:
 *	miser.c idbg_mq
 */
{
	vpagg_t *vpagg;

	MISER_DEBUG("[idbg_vpagg]");

	if (x == -1L) {
		return;
	}

	vpagg = (vpagg_t *) x;

	qprintf("Vpagg 0x%x \n", vpagg);
	qprintf("paggid %d, refcnt %d \n", 
			vpagg->vpag_paggid, vpagg->vpag_refcnt);

	return;

} /* idbg_vpagg */


void 
idbg_mq(__psint_t x)
/*
 * Prints contents of miser_queue - kernel debug information.
 */
{
        miser_queue_t *mq;
	miser_data_t  *rq; 
        miser_seg_t   *seg;

	MISER_DEBUG("[idbg_mq]");

        if (x == -1L) {
                return;
	}

        mq = (miser_queue_t*) x;
	rq = &mq->mq_request.mr_req_buffer;

	qprintf("mq associated to job 0x%x holds %d \n", mq->mq_job, 
			mq->mq_holds);

	if (mq->mq_response == MISER_NEEDRESPONSE) {
		qprintf("Need response\n");
	} else {
		qprintf("Need no response\n");
	}

	if (mq->mq_vpagg) {
		idbg_vpagg((__psint_t) mq->mq_vpagg);
	}

	if (rq->md_request_type == MISER_USER_JOB_SUBMIT) {
        	qprintf("Miser queue \n");
		qprintf("ID %d, bid %d, end %d nsegs %d\n start %d \n", 
			MISER->mj_queue, MISER->mj_bid, MISER->mj_etime, 
			MISER->mj_count, mq->mq_lbolt);

		for (seg = SEG; seg != SEG + MISER->mj_count; seg++) {
			 qprintf ("seg %x, time %d, end_time %d\n", 
					seg, seg->ms_rtime, seg->ms_etime);
		}

	} else { 
		qprintf("Miser command %d \n", rq->md_request_type);
	}

} /* idbg_mq */
