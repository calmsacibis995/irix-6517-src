/*
 * FILE: eoe/cmd/miser/cmd/miser/default_policy.C
 *
 * DESCRIPTION:
 *      Defines miser queue default policy related structure and templates
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


#include "private.h"
#include "queue.h"               


/* 
 * Definition of the sched_segment structure and its operator() 
 */
struct sched_segment 
{
	miser_queue	*queue;
	long		cur_block_start;

	sched_segment(miser_queue *Queue)
		: queue(Queue), cur_block_start(Queue->last_accessed_time) {}

	bool operator() (miser_seg_t& seg)
	{
		SYMBOL_PRINT(seg.ms_multiple);

		long etime = seg.ms_etime == 0 ? 0 :
			 cur_block_start + seg.ms_etime / time_quantum;

                while (seg.ms_resources.mr_cpus > 0) {

                        long stime = queue_find_block(queue, cur_block_start, 
					etime, seg);

                        if (stime != -1) {
				cur_block_start = seg.ms_etime
					= stime + seg.ms_rtime /
						seg.ms_resources.mr_cpus;
				return false;
                        }

			seg.ms_resources.mr_cpus -= seg.ms_multiple;

                } /* while */

		SYMBOL_PRINT(seg.ms_etime);
		SYMBOL_PRINT(seg.ms_rtime);
		STRING_PRINT("[sched_segment] Failed to find a segment "
				"in default_policy");

		return true;

	} /* operator() */

}; /* sched_segment */


/*
 * Find a resource segment using the default first fit policy and 
 * allocate those segment blocks from the queue.
 */
extern "C" error_t
default_policy(void* v, bid_t bid, miser_seg_t* bseg, miser_seg_t* eseg,
	miser_time_t * end_time)
{
	miser_queue* queue = (miser_queue*)v;

	if (find_if(bseg, eseg, sched_segment(queue)) != eseg) {
		return MISER_ERR_RESOURCES_INSUFF;
	}

        STRING_PRINT("Found a segment in default_policy");

        queue_allocate_blocks(queue, bseg, eseg);

        *end_time = eseg[-1].ms_etime;

        return MISER_ERR_OK;

} /* default_policy */
