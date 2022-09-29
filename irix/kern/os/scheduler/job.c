/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/uthread.h>
#include <sys/kmem.h>
#include <sys/space.h>
#include <sys/debug.h>
#include <sys/par.h>
#include "utility.h"
#include <sys/runq.h>
#include <sys/sema_private.h>



void (*sched_init[])(job_t*, kthread_t *) = {
	(void (*)(job_t*, kthread_t*))null_func,
	wt_init,
	init_batch
};

#ifdef DEBUG
static int
job_thread_cnt(job_t *job)
{
	int i = 0;
	kthread_t *kt;

	for (kt = job->j_thread; kt; kt = kt->k_link)
		i++;
	return i;
}
#endif



/*
 * Create a job with possibly one thread, kt, in given state.
 */
job_t *
job_create(job_type_t type, kthread_t *kt)
{
	job_t *job;

	ASSERT(KT_ISUTHREAD(kt));
	ASSERT(!KT_TO_UT(kt)->ut_job);
	ASSERT(kt_islocked(kt));
	job = (job_t *) kmem_zone_alloc(job_zone_private, KM_NOSLEEP);
	ASSERT(job != NULL);

	ASSERT(kt->k_onrq != CPU_NONE
		|| kt->k_rflink == kt && kt->k_rblink == kt);
	KT_TO_UT(kt)->ut_job = job;

	init_q_element(&job->j_queue, job);
	job->j_thread = kt;
	kt->k_link = NULL;
	job->j_nthreads = 1;
	job->j_type = type;
	job->j_flags = 0;
	job->j_xxx_gdb = 0;	
	sv_init(&job->j_sv, SV_DEFAULT, "jobsv");
	sched_init[type](job, kt);

	return job;
}

void
job_reset_type(job_t *job, job_type_t type)
{
	job->j_type = type;
	sched_init[type](job, job->j_thread);
}

/*
 * Only called when forking an sproc.
 */
void
job_attach_thread(job_t *job, kthread_t *kt)
{
	ASSERT(KT_ISUTHREAD(kt));
	ASSERT(kt->k_rflink == kt && kt->k_rblink == kt);

	job_nested_lock(job);
	kt->k_link = job->j_thread;
	job->j_thread = kt;
	KT_TO_UT(kt)->ut_job = job;
	job->j_nthreads++;
	ASSERT(kt->k_onrq == CPU_NONE);	/* running on cpu is safe */
	ASSERT(job_thread_cnt(job) == job->j_nthreads);
	job_nested_unlock(job);
}


void
job_destroy(job_t *job)
{
	job_nested_unlock(job);
	sv_destroy(&job->j_sv);
	job->j_ops.jo_detach(job);
	kmem_zone_free(job_zone_private, job) ;
}

/* This is a convenience function, it is the responsibility of the caller
 * to ensure the job in use is locked if necessary.  This function is used
 * by modules (batchd in particular) that will not function properly if
 * the job lock is already acquired.  Therefore there is no ASSERT here
 * testing the state of the job_islocked() .
 */
void
job_for_each_thread(job_t *job, void (*func)(kthread_t *, job_t *))
{
	kthread_t *kt;

	for (kt = job->j_thread; kt; kt = kt->k_link)
		func(kt, job);
}

void
job_unlink_thread(kthread_t *kt)
{
	job_t *job;

	job = KT_TO_UT(kt)->ut_job;

	/*
	 * Gang scheduled threads must first be removed from the gang
	 * before job_unlink_thread() may be called on them.
	 */
	ASSERT(!(!is_batch(kt) && !is_bcritical(kt))
		|| KT_TO_UT(kt)->ut_gstate == GANG_UNDEF);

	job_nested_lock(job);
	job_locked_unlink_thread(job, kt);
	if (job->j_nthreads == 0) {
		if (!JOB_ISCACHED(job))
			job_destroy(job);
		else
			job_nested_unlock(job);
	} else {
		job_nested_unlock(job);
	}
}

void   
job_locked_unlink_thread(job_t *job, kthread_t *kt) 
{
	kthread_t **ppkt;
	ASSERT(job_islocked(job));
	ASSERT(job->j_nthreads > 0);

	job->j_nthreads--;
	ppkt = &job->j_thread;
	while (*ppkt != 0) {
		if (kt == *ppkt)
			break;
		ppkt = &((*ppkt)->k_link);
        }
	ASSERT (*ppkt == kt);   /* thread better be there */
	*ppkt = kt->k_link;     /* unlink */
	kt->k_link = 0;         /* unlink */
	ASSERT(job_thread_cnt(job) == job->j_nthreads);
	KT_TO_UT(kt)->ut_job = 0;
}

void
job_cache(job_t *job)
{
	/* Sync provided by caller.
	 */
	ASSERT(job->j_nthreads == 1);
	job_fset(job, JOB_CACHED);
}

void
job_uncache(job_t *job)
{
	int s;

	ASSERT(JOB_ISCACHED(job));

	s = job_lock(job);

	if (job->j_nthreads == 0) {
		job_destroy(job);
		splx(s);
	} else {
		job->j_flags &= ~JOB_CACHED;
		job_unlock(job, s);
	}
}

void null_func() {}
