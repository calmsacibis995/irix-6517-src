/*
 ****************************************************************************
 *    Copyright © 1996 Ordinal Technology Corp.  All Rights Reserved.       *
 *                                                                          *
 *    DO NOT DISCLOSE THIS SOURCE CODE OUTSIDE OF SILICON GRAPHICS, INC.    *
 *  This source code contains unpublished proprietary information of        *
 *  Ordinal Technology Corp.  The copyright notice above is not evidence    *
 *  of any actual or intended publication of this information.              *
 *      This source code is licensed to Silicon Graphics, Inc. for          *
 *  its personal use in developing and maintaining products for the IRIX    *
 *  operating system.  This source code may not be disclosed to any third   *
 *  party outside of Silicon Graphics, Inc. without the prior written       *
 *  consent of Ordinal Technology Corp.                                     *
 ****************************************************************************
 *
 *
 * Merge_part_ptr - merge partition code for pointer sorts
 *
 *	$Ordinal-Id: merge_part_ptr.c,v 1.17 1996/11/01 22:58:28 charles Exp $
 */
#ident	"$Revision: 1.1 $"

#include	"otcsort.h"
#include	"merge.h"

char		Merge_ptr_part_id[] = "$Ordinal-Revision: 1.17 $";

void merge_part_ptr(sort_t *sort, unsigned merge_index, unsigned sproc_id)
{
    ptr_out_buf_t	*curr;
    merge_args_t	args;
    rec_out_t		rec_out[2];
    unsigned		skip_target;
    unsigned		not_skipped;
    unsigned		remainder_skip;
    unsigned		begin_consume;
    char		tstr[40];
    const u4		entry_size = PTR_PART_ENTRY_SIZE;
    
    curr = MERGE.ptr_part + (merge_index % MERGE.merge_cnt);
    args.n_inputs = MERGE.width;
    args.node_buf = curr->tree;
    args.recode.summarize = 0;
    args.recode.n_hard = 0;
    args.recode.n_soft = 0;
    args.sproc_id = sproc_id;
    args.merge_index = merge_index;
    args.ptr_buf = curr;
    args.norm = NULL;
    args.in_list = MERGE.run_list;

    /* Calculate the beginning consumption point.
     * Each merge partition will consume MERGE.part_recs records.
     * If there are no deletions and we don't reach the of the merge,
     * exactly MERGE.part_recs record pointers will be output.  Otherwise,
     * fewer record pointers will be output.
     * 
     * External pointer sorts always copy the records, so there is no need
     * to shift the pointers output by a merge partition to meet
     * the block alignment requirements of direct i/o.
     *
     * Internal pointer sorts (like all kinds of records sorts)
     * needs to shift the results of this merge if the previous merge
     * is still active and actually deletes duplicates.
     * 
     * The last record of the merge (tail_rec) should be used by the 
     * subsequent merge to determine if its first record should be deleted 
     * or summarized.  The io sproc cannot set up the tail_rec for copying
     * to an output buffer until MERGE.tails_done is greater than the merge
     * index for the merge partition.  A merge partition will increment
     * MERGE.tails_done when it has determined that it is done with the
     * previous partition's tail_rec.
     */
    begin_consume = MERGE.part_recs * merge_index;

    /* get starting point for merge.
     *
     * Must get spinlock for the merge list because it is possible a previous
     * merge is currently updating the list.
     */
    while (!compare_and_swap(&MERGE.run_list_lock, FALSE, TRUE, sort))
	sginap(0);

    if (Print_task)
	fprintf(stderr, "merge part %d recs %d - %d from %d @ %s\n", 
		merge_index, begin_consume, begin_consume + MERGE.part_recs,
		MERGE.recs_consumed, get_time_str(sort, tstr));

    /* calculate the minimum number of remainder records we can have
     * left AFTER skipping (otherwise we may skip over a record that
     * belongs in the merge partition).  
     */
    remainder_skip = (MERGE.skip_interval - 1) * (MERGE.width - 1);
    
    /* if there are at least as many records left to skip as the
     * target remainder skip, then do a skip merge.
     */
    if (begin_consume - MERGE.recs_consumed >= remainder_skip)
    {
	skip_target = begin_consume - MERGE.recs_consumed - remainder_skip;
	not_skipped = merge_ptr_skip(sort, &args,
					   skip_target,
					   MERGE.skip_interval);
	MERGE.recs_consumed += skip_target - not_skipped;
	remainder_skip += not_skipped;
    }
    else
	remainder_skip = begin_consume - MERGE.recs_consumed;

    if (Print_task)
	fprintf(stderr, "merge part %d rem %d @ %s\n", 
		merge_index, remainder_skip, get_time_str(sort, tstr));

    if (sort->internal)
    {
	ptr_out_buf_t	*prev = MERGE.ptr_part +
				((merge_index - 1) % MERGE.merge_cnt);

	/* Swiped from merge_part() (record sort version):
	 * Find the current best guess as to where the previous merge
	 * will finish filling in ptrs.  This merge will start right there.
	 * If it turns out that the previous merge didn't get that far
	 * because duplicates were deleted we'll shift the results down.
	 */
	if (merge_index == 0)			/* first merge, none before */
	    curr->begin_ptr = (volatile byte **) curr->ptrs;
	else if (MERGE.merge_done < merge_index)/* prev still going, shift? */
	    curr->begin_ptr = (volatile byte **) prev->guess_tail_ptr;
	else					/* prev merge done */
	    curr->begin_ptr = (volatile byte **) prev->tail_ptr + 1;

	/* XXX There is strong off-by-one potential here
	 */
	curr->guess_tail_ptr = (byte **) curr->begin_ptr + MERGE.part_recs;

    }
    else
    {
	/* initialize begin and tail ptrs so that the io sproc can start
	 * polling for the output of this merge partition once MERGE.skip_done
	 * is incremented.  
	 */
	curr->begin_ptr = (volatile byte **) curr->ptrs;
    }

    curr->tail_ptr = (volatile byte **)curr->begin_ptr;
    curr->limit_ptr = (volatile byte **)curr->begin_ptr;

    curr->end_of_merge = FALSE;		/* assume no end_of_merge yet */

    /* Now do merge, potentially skipping over some remainder records left
     * left over from a merge skip.
     */
    rec_out[0].first = (byte *) curr->begin_ptr;
    rec_out[0].limit = (byte *) curr->begin_ptr + entry_size * MERGE.part_ptr_cnt;
    rec_out[0].end = NULL;
    /* define second bogus array so that an overflow from first will fail */
    rec_out[1].first = NULL;
    rec_out[1].limit = NULL;
    rec_out[1].end = NULL;
    args.end_consume = MERGE.part_recs * (merge_index + 1);
    curr->end_of_merge = (sort->merge_out)(sort, &args,
					         rec_out,
					         remainder_skip,
					         MERGE.part_recs);
	
    /* if end_of_merge, ward off other sprocs from taking a merge task.
     */
    if (curr->end_of_merge)
	MERGE.end_of_merge = TRUE;

    /* if this is not the first merge, and merge_ptr_out() didn't yet
     * resolve whether the first record of this merge should be deleted
     * relative to the tail record of the previous merge.
     */
    if (merge_index != 0 && MERGE.tails_done < merge_index)
    {
	/* wait for previous merge to complete
	 */
	while (MERGE.merge_done < merge_index)
	    ordnap(1, "merge_part_ptr prev merge");

	/* if there is output for this merge partition
	 */
	if (rec_out[0].end != (byte *) curr->begin_ptr)
	{
	    /* process list of tbufs normally consumed, if any.
	     */
	    if (args.norm != NULL)
		process_norms(sort, args.norm, merge_index);
	    
	    tail_ptr_check(sort, merge_index);
	}
    }

    /* Add this sproc's recode counts to the sums in the sort struct.
     *
     * This can be done here without explicit serialization because we know
     * the previous merge has completed, and we haven't yet incremented
     * merge_done (thereby indicating this merge partition is done).
     */
    TIMES.hard_recodes += args.recode.n_hard;
    TIMES.soft_recodes += args.recode.n_soft;

    /* Increment merge_done count to indicate this merge has completed.
     */
    MERGE.merge_done++;

    if (Print_task)
	fprintf(stderr, "merge part %d, recs %d done at %s\n", 
		merge_index, 
		((byte *)curr->limit_ptr-(byte *)curr->begin_ptr) / entry_size,
		get_time_str(sort, tstr));
}


void tail_ptr_check(sort_t *sort, unsigned merge_index)
{
    recode_args_t	ra;
    item_t		tail_item;
    item_t		first_item;
    ptr_out_buf_t	*curr;
    ptr_out_buf_t	*prev;
    unsigned		swap;

    /* check if the first ptr from curr merge needs to be deleted
     * relative to last_ptr of prev merge.
     */
    if (sort->any_deletes)
    {
	curr = MERGE.ptr_part + (merge_index % MERGE.merge_cnt);
	prev = MERGE.ptr_part + ((merge_index - 1) % MERGE.merge_cnt);

	/* Determine if the first record of this merge partition should
	 * be deleted (or summarized) relative to the tail record of the
	 * the previous merge.
	 */
	ra.win_id = 0;
	ra.curr_id = 1;
	ra.summarize = sort->n_summarized;
	tail_item.ovc = sort->virgin_ovc;
	first_item.ovc = sort->virgin_ovc;
	if (sort->record.flags & RECORD_FIXED)
	{
	    tail_item.rec = (byte *) *prev->tail_ptr;
	    first_item.rec = (byte *) *curr->begin_ptr;
	}
	else
	{
	    tail_item.rec = ulp(prev->tail_ptr);
	    first_item.rec = ulp(curr->begin_ptr);
	    tail_item.extra.var_extra.var_length =
		*(u2 *) (prev->tail_ptr + 1);
	    first_item.extra.var_extra.var_length = 
		*(u2 *) ((byte **)curr->begin_ptr + 1);
	}

	swap = (sort->recode)(sort, &ra, &tail_item, &first_item);
	ASSERT(!swap);

	/* if first record should be deleted, bump up the begin ptr to
	 * effectively skip over it.
	 */
	if (first_item.ovc == (sort->del_dup_ovc + 1))
	{
	    if (sort->internal)
	    {
		/* The api ptr sort can't have a gap, so shift the ptrs down
		 * rather than move the starting position up.
		 */
		memmove(curr->begin_ptr,
			(byte *) curr->begin_ptr + PTR_PART_ENTRY_SIZE,
			(byte *) curr->tail_ptr - (byte *) curr->begin_ptr);
		curr->tail_ptr = (volatile byte **)
			    ((ptrdiff_t) curr->tail_ptr - PTR_PART_ENTRY_SIZE);
	    }
	    else
		curr->begin_ptr = (volatile byte **)
			    ((ptrdiff_t) curr->begin_ptr + PTR_PART_ENTRY_SIZE);
	    }
    }
    MERGE.tails_done++;
}


void alloc_ptr_out_lists(sort_t *sort)
{
    /* Initialize the structure and tournament tree for each record output.
     * The actual record buffers will be initialized by map_io_buf() since
     * they must consist of pinned memory.
     */
    MERGE.ptr_part = (ptr_out_buf_t *)
		     calloc(MERGE.merge_cnt, sizeof(MERGE.ptr_part[0]));

    /* allocate copy task structs. clear the mem for temp file copies -
     * chk_temp_output() doesn't bother re-setting skip_bytes to zero each time.
     */
    COPY.copy_cnt = 128;
    COPY.copy = (copy_task_t *) calloc(COPY.copy_cnt, sizeof(copy_task_t));

    OUT.buf = (byte **) calloc(OUT.buf_cnt, sizeof(OUT.buf[0]));
}
