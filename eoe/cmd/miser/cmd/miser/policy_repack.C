/*
 * FILE: eoe/cmd/miser/cmd/miser/policy_repack.C
 *
 * DESCRIPTION:
 *      Defines miser queue repack policy related structure and templates
 *	and appropriate functions.
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


#include "queue.h"               
#include "algo_ext.h"


static vector<bid_t> bids;


#ifdef DEBUG

/* Print out list of Jobs */
class Print_Job_Schedule 
{
  public:
	explicit Print_Job_Schedule() {}

	void operator() (miser_job_schedule& jst) { 
		SYMBOL_PRINT(jst);
	}

};

#endif /* DEBUG */


/*
 * Simple First fit method of finding a location in the queue
 */
struct repack_sched_segment 
{
	miser_queue 	*queue;
	long		cur_block_start;

	repack_sched_segment(miser_queue *Queue)
		: queue(Queue), cur_block_start(Queue->last_accessed_time) {}

	bool operator() (miser_seg_t& seg)
	{
		SYMBOL_PRINT(seg.ms_multiple);

		long etime = seg.ms_etime;

		assert(seg.ms_resources.mr_cpus > 0);

                while (seg.ms_resources.mr_cpus > 0) {
                        STRING_PRINT("[repack_sched_segment] Trying to "
					"find a segment");
			SYMBOL_PRINT(seg);
			SYMBOL_PRINT(seg.ms_etime);
			SYMBOL_PRINT(cur_block_start);
			SYMBOL_PRINT(etime);

                        long stime = queue_find_block(queue, cur_block_start, 
					etime, seg);

                        if (stime != -1) {
				cur_block_start = seg.ms_etime = stime + 
					seg.ms_rtime/seg.ms_resources.mr_cpus;

				return false;
                        }

			seg.ms_resources.mr_cpus -= seg.ms_multiple;
                }

		SYMBOL_PRINT(seg.ms_etime);
		SYMBOL_PRINT(seg.ms_rtime);
		STRING_PRINT("[repack_sched_segment] Failed to find");
		SYMBOL_PRINT(queue->space_available);

		return true;
	}

}; /* repack_sched_segment */


/*
 * Half of the policy used for rescheduling jobs in the static queues.
 * To avoid the idle time that the default policy causes when a job terminates
 * before its time is up. With this policy. It will reschedule all later jobs
 * to run earlier. Thus filling in the gab left behind by the terminating job.
 */
extern "C" error_t 
repack_remove(void *v, bid_t bid, miser_seg_t* bseg, miser_seg_t* eseg,
	miser_time_t* end_time)
{
	error_t error;

	STRING_PRINT("[repack_remove] Using repack policy");
	
	/* Erases the bid from the bid list */
	vector<bid_t>::iterator b 
		= find (bids.begin(), bids.end(), bid);

	if (b == bids.end()) {
		return MISER_ERROR;
	}

	bids.erase(b);
	queue_update((miser_queue*)v);

#ifdef DEBUG

	for_each (job_schedule_queue.begin(),
		  job_schedule_queue.end(),
                  Print_Job_Schedule());

#endif /* DEBUG */

	/* Iterators for later use */
	jobschedule::iterator j;

	b = bids.begin();

	while(b != bids.end()) {
		j = find(job_schedule_queue.begin(), 
				job_schedule_queue.end(), *b);

		/*
		 * Clean up bids vector since job can terminate correctly
		 * thus not ereasing its bid 
		 */
		if (j == job_schedule_queue.end()) {

			 bids.erase(b);

			 /* reset iterator so BAD things dont happen */
			 b = bids.begin();

			 continue; 
		}

		/* Check to see if the job should be rescheduled. */
		if (get_start_time(*(j->schedule.begin())) >
				 getmisertime()/time_quantum) {
			error =	repack_reschedule_job(j);
		}

		b++;

	} /* while */	

#ifdef DEBUG

	for_each(job_schedule_queue.begin(),
		 job_schedule_queue.end(),
                 Print_Job_Schedule());

#endif /* DEBUG */

	return error;

} /* repack_remove */


/*
 * The scheduling half of the policy. Per insertion it maitains a 
 * vector of bids in the order the jobs where submitted. This is then 
 * later used durning the rescheduling to insure the same order of jobs 
 * as prior to rescheduling.
 */
extern "C" error_t 
repack_schedule(void* v, bid_t bid, miser_seg_t* bseg, miser_seg_t* eseg,
	miser_time_t * end_time)
{
	miser_queue* queue = (miser_queue*)v;

	STRING_PRINT("[repack_schedule] Start");

	if (find_if(bseg, eseg, repack_sched_segment(queue)) != eseg) {
		return MISER_ERR_RESOURCES_INSUFF;
	}

        STRING_PRINT("[repack_schedule] Found a segment in repack_policy");

        queue_allocate_blocks(queue, bseg, eseg);

        *end_time = eseg[-1].ms_etime;

	vector<bid_t>::iterator iter = find(bids.begin(), bids.end(),bid);

	if(iter == bids.end()) {
		bids.push_back(bid);
	}

#ifdef DEBUG

	for_each(job_schedule_queue.begin(), 
		 job_schedule_queue.end(), 
		 Print_Job_Schedule());

#endif /* DEBUG */

	assert(bseg->ms_resources.mr_cpus > 0);

        return MISER_ERR_OK;

} /* repack_schedule */
