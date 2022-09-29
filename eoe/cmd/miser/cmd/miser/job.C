/*
 * FILE: eoe/cmd/miser/cmd/miser/job.C
 *
 * DESCRIPTION:
 *	Implements Miser job management functions.
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


#include "private.h"
#include "queue.h"
#include "debug.h"
#include "algo_ext.h"

extern error_t	G_error;		/* global miser error */
extern int	now;

error_t
job_deschedule(bid_t bid)
{
	error_t temp_error = MISER_ERR_OK;

	jobschedule::iterator j =  find(job_schedule_queue.begin(), 
					job_schedule_queue.end(), bid);

	if (j == job_schedule_queue.end()) {
		return MISER_ERR_JOB_ID_NOT_FOUND;
	}

	miser_queue* q = find_queue(j->queue->definition->queue, F_OK);

	if (!q) {
		return G_error;
	}
	
	quanta_t time = j->schedule[0].ms_etime;

	job_schedule_queue.erase(j);

	if (getmisertime() < time * time_quantum) {
		temp_error = q->policy->redo_policy(q, bid, 0,0,0);
	}

	return temp_error;

} /* job_deschedule */


inline void
fix_segment(miser_seg_t& seg)
{
	seg.ms_etime /= time_quantum;
	seg.ms_rtime /= time_quantum;

} /* fix_segment */


error_t 
job_schedule(id_type_t queue_id, bid_t bid, miser_seg_t* bseg,
	     miser_seg_t* eseg, miser_time_t *end_time, int ca)
{
        miser_queue* q = find_queue(queue_id, X_OK, ca);

        if (!q || !q->policy) {
		return G_error;
	}

        STRING_PRINT("[job_schedule] Found the Queue");

	if (ca && ((void *)eseg < start_request 
			|| (void *)eseg > end_request)) {
		return MISER_ERR_EFAULT;
	}
 
        queue_update(q);

	job_schedule_queue.push_front(miser_job_schedule());
	miser_job_schedule& j = *job_schedule_queue.begin();

	for_each(bseg, eseg, fix_segment);
	j.init(bid, bseg, eseg);

        error_t ret_val = q->policy->do_policy(q, bid, j.schedule.begin(), 
				j.schedule.end(), end_time);

#ifdef DEBUG
	q->verify_queue();
#endif

	*end_time *= time_quantum;

	if (ret_val != MISER_ERR_OK) {
		job_schedule_queue.erase(job_schedule_queue.begin());

		fprintf(stderr, "Error: No room in queue for job: %d.\n", bid);	
		ret_val = MISER_ERR_RESOURCES_INSUFF;

	} else {
        	STRING_PRINT("[job_schedule] Found something");
		j.queue = q;
		j.now = now; 
		copy(j.schedule.begin(), j.schedule.end(), bseg, eseg);

		for_each(bseg, eseg, fix_ctime());
	}

        return ret_val;

} /* job_schedule */


bool
job_reschedule(miser_job_schedule& job)
{
	miser_queue* q = find_queue(job.queue->definition->queue, F_OK);

	if (!q) {
		return true;
	}

	queue_allocate_blocks(q, job.schedule.begin(), job.schedule.end());

	return false;

} /* job_reschedule */


void
job_retarget(miser_job_schedule& job)
{
	job.queue = find_queue(job.queue->definition->queue, F_OK);

} /* job_retarget */


miser_job_schedule::~miser_job_schedule()
{
	if (!queue) {
		return;
	}

	SYMBOL_PRINT(schedule[0].ms_etime);

	for_each(schedule.begin(), schedule.end(),
		queue_resources<miser_mod<add> >(*queue));

#ifdef DEBUG
		queue->verify_queue();
#endif

} /* miser_job_schedule */


void
jobs_move(miser_queue *from, miser_queue *to)
{
	queue_update(to);

	for (jobschedule::iterator j = job_schedule_queue.begin();
		j != job_schedule_queue.end(); ++j) {

		if ((*j).queue != from) {
			continue;
		}

		queue_allocate_blocks(to, (*j).schedule.begin(),
					  (*j).schedule.end());
		(*j).queue = to;
	}

} /* jobs_move */


error_t
job_query_schedule(bid_t bid, int offset, miser_seg_t* begin, miser_seg_t*& end)
{
	if ((void *)end < start_request || (void*)end > end_request) {
		return MISER_ERR_EFAULT;
	}

	SYMBOL_PRINT(bid);
	SYMBOL_PRINT(offset);

	jobschedule::iterator i = find(job_schedule_queue.begin(),
					job_schedule_queue.end(), bid);

	if (i == job_schedule_queue.end()) {
                return MISER_ERR_JOB_ID_NOT_FOUND;
	}

	miser_job_schedule* job = &*i;

        if (offset > job->schedule.size()) {
		offset = job->schedule.size();
	}

	end = copy(job->schedule.begin() + offset, job->schedule.end(),
		   begin, end);

	for_each(begin, end, fix_ctime(0));

        return MISER_ERR_OK;

} /* job_query_schedule */
