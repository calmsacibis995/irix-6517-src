/*
 ****************************************************************************
 *    Copyright © 1996 Ordinal Technology Corp.  All Rights Reserved .      *
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
 *	Merge_part - merge partition code
 *
 *	$Ordinal-Id: merge_part.c,v 1.21 1996/11/01 22:58:28 charles Exp $
 */
#ident	"$Revision: 1.1 $"

#include	"otcsort.h"
#include	"merge.h"

char			Merge_part_id[] = "$Ordinal-Revision: 1.21 $";

void merge_part(sort_t *sort, unsigned merge_index, unsigned sproc_id)
{
    rec_out_buf_t	*curr;
    rec_out_buf_t	*prev;
    merge_args_t	args;
    off64_t		offset;
    unsigned		i;
    unsigned		begin_offset;
    rec_out_t		rec_out_struct;
    rec_out_t		*rec_out;
    blockheader_t	*head;
    byte		*first_rec;
    byte		*tail_rec;
    unsigned		skip_target;
    unsigned		not_skipped;
    unsigned		remainder_skip;
    unsigned		begin_consume;
    unsigned		end_consume;
    unsigned		prev_rem_size;
    unsigned		recs_merged;
    byte		*end_rem;
    ptrdiff_t		merge_out_size;
    char		tstr[40];
    
    curr = MERGE.rob + (merge_index % MERGE.merge_cnt);
    prev = MERGE.rob + ((merge_index - 1) % MERGE.merge_cnt);
    args.n_inputs = MERGE.width;
    args.node_buf = curr->tree;
    args.recode.summarize = 0;
    args.recode.n_hard = 0;
    args.recode.n_soft = 0;
    args.sproc_id = sproc_id;
    args.merge_index = merge_index;
    args.norm = NULL;

    /* Calculate beginning and ending merge record consumption points.
     * The beginning point is the number of records to be consumed (from
     * the beginning of the merge) before this merge's output begins.  The
     * ending is the point up to which a merge should consume records.  The
     * ending of a merge will match the beginning of the next merge.
     * 
     * The begin and end consumption points are based on the assumption that
     * no deletions will occur (the only safe assumption since otherwise we
     * may run out of output buffer space).  
     */

    /* if writing blocks to temp files
     */
    if (sort->input_phase)
    {
	/* The input phase (of a two pass sort) writes only entire records into
	 * temp file blocks (none are split across blocks).
	 * 
	 * Random notes:
	 *	- The merge_index is reset to zero at the end of each run.
	 *	- All the merges of one run are completed before the first 
	 *    	  merge of the next run starts. 
	 */

	/* The first merge will consume TEMP.merge_part_blks*TEMP.blk_recs 
	 * records.  All subsequent merges will consume one less record 
	 * (since they must allow space for the tail_rec of the previous merge).
	 */
	if (merge_index == 0)
	    begin_consume = 0;
	else
	    begin_consume = 
		1 + (TEMP.merge_part_blks * TEMP.blk_recs - 1) * merge_index;
	end_consume = 
	    1 + (TEMP.merge_part_blks * TEMP.blk_recs - 1) * (merge_index + 1);
    }
    else
    {
	/* We are writing to the output file.  
	 *
	 * Assuming no deletions, each merge (except the last) will produce
	 * enough records for an output buffer of size OUT.buf_size, and
	 * some remainder data which will include an entire record known as
	 * tail_rec.  This is the last record of the merge and should be
	 * used by the subsequent merge to determine if its first record
	 * should be deleted or summarized.  The subsequent merge is
	 * responsible for outputing the tail_rec and any additional
	 * remainder data.
	 *
	 * If deletions occur, output sizes less than OUT.buf_size are possible
	 * (including size 0), and output data may need to be shifted to meet
	 * the block alignment requirements of direct i/o.
	 *
	 * calculate beginning and ending consumption records based on
	 * producing an OUT.buf_size buffer of output, assuming no deletions
	 * occur (if they occur, the actual size of the data written may
         * be smaller). 
     	 */

	/* if this is not the first merge, make beginning consumption point
	 * where the last merge left off - it (and the merges before it)
	 * have consumed enough records (assuming no deletions) to
	 * completely fill merge_index buffers of size OUT.buf_size, plus
	 * an additional record that is the tail_rec.  
	 */
	if (merge_index != 0)
	    begin_consume = 
		ROUND_UP_DIV(OUT.buf_size * merge_index, sort->edited_size) + 1;
	else
	    begin_consume = 0;	/* first merge always starts at beginning */

	/* Consume enough records (assuming no deletions) to completely
	 * fill (merge_index+1) buffers of size OUT.buf_size, plus an
	 * additional record that is the tail_rec. 
	 */
	end_consume = 
	    ROUND_UP_DIV(OUT.buf_size * (merge_index+1), sort->edited_size) + 1;
    }

    if (Print_task)
	fprintf(stderr, "merge_part #%d will consume %d recs (%d -> %d)\n",
		merge_index, end_consume - begin_consume, begin_consume,
		end_consume);

    /* Must get spinlock for the merge list because it is possible a previous
     * merge is currently updating the list. Merge_rec_out() frees it.
     */
    while (!compare_and_swap(&MERGE.run_list_lock, FALSE, TRUE, sort))
	sginap(0);

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
	skip_target = 
	    begin_consume - MERGE.recs_consumed - remainder_skip;
	args.in_list = MERGE.run_list;
	not_skipped = (*sort->merge_skip)(sort,
					 &args,
					 skip_target,
					 MERGE.skip_interval);
	MERGE.recs_consumed += skip_target - not_skipped;
	remainder_skip += not_skipped;
    }
    else
	remainder_skip = begin_consume - MERGE.recs_consumed;

    /* determine starting place for first record output by the merge.
     */
    if (merge_index == 0)
    {
	begin_offset = 0;
    }
    else if (MERGE.merge_done < merge_index)
    {
	/* previous merge has not yet completed, use its guess as to where 
	 * its output will end (relative to a multiple of the output size).
	 * This guess will be the actual ending offset if the previous merge
	 * does not need to shift its output and does not delete any records.
	 */
	begin_offset = prev->guess_rem_offset;
    }
    else
    {
	/* Use the previous merge's exact remainder offset as this
	 * merge's beginning offset.  The output for this merge will
	 * NOT need to be shifted.
	 */
	begin_offset = (unsigned)
	    (prev->tail_rec - prev->u.buf + sort->edited_size) % OUT.dio_io;
    }    
    
    /* make guess at where the next merge should start its output.
     * For internal sorts this is an offset from the start of the whole
     * output buffer.  For other (i.e. i/o sproc) sorts this is an offset
     * from an OUT.dio_io boundary. In either case the purpose is to 
     * avoid shifting the results of the merge when possible.
     * When duplicates are actually deleted and merges are parallel then
     * some shifting is necessary.
     */
    curr->guess_rem_offset = (begin_offset + (end_consume - begin_consume) *
					     sort->edited_size) % OUT.dio_io;

    curr->tail_rec = NULL;	/* assign bogus tail_rec for now */
    curr->end_of_merge = FALSE;	/* assume no end_of_merge yet */

    if (Print_task > 1)
	fprintf(stderr, 
		"merge part %d merging after skip %d with rem skip %d at %s\n",
		merge_index, MERGE.skip_interval, remainder_skip, 
		get_time_str(sort, tstr));

    /* Prepare rec_out(s) for the merge
     */
    if (sort->input_phase)
    {
	/* Writing to temp files. This is a relatively narrow merge (128? 256?)
	 * Make a rec_out for each temp file to be written to by this merge.
	 * The first one may have fewer records for merge to fill, if it is
	 * the first block of a run, and so has space reserved
	 * for more lookahead key records.  All the blocks have the same
	 * actual length - it's just that some slots will be filled in later.
	 */
	for (i = 0, rec_out = curr->rec_outs, head = curr->u.blks;
	     i < TEMP.merge_part_blks;
	     i++, rec_out++)
	{
	    head->first_offset = (unsigned) sizeof(blockheader_t) +
		(TEMP.optimal_reads ? sort->edited_size : 0);
	    rec_out->first = (byte *) head + head->first_offset;
	    rec_out->limit = rec_out->first + TEMP.blk_recs * sort->edited_size;
	    rec_out->end = NULL;

	    /* if need room for tail_rec of previous merge
	     */
	    if (merge_index != 0 && i == 0)	
	    {
		head->first_offset += sort->edited_size;
		first_rec = (byte *)head + head->first_offset;
		rec_out->first = first_rec;
	    }
#if defined(DEBUG)
	    /* for debug: fill reserved space with a junk pattern */
	    memset((byte *) head + sizeof(blockheader_t), 0xdb, 
		   head->first_offset - sizeof(blockheader_t));
#endif
	    head = head->u.next;	/* get next block in link-list */

	    /* chk_temp_output() fills in head->run and head->block */
	}
	rec_out = curr->rec_outs;
    }
    else
    {
	first_rec = curr->u.buf + OUT.rec_buf_pad + begin_offset;
	rec_out = &rec_out_struct;
	rec_out[0].first = first_rec;
	rec_out[0].limit = first_rec +
			   sort->edited_size * (end_consume - begin_consume);
	rec_out[0].end = NULL;
    }
    /* Now do merge, potentially skipping over some remainder records left
     * left over from a merge skip.
     */
    args.in_list = MERGE.run_list;
    args.end_consume = end_consume;
    curr->end_of_merge = (*sort->merge_out)(sort,
					   &args,
					   &rec_out[0],
					   remainder_skip,
					   end_consume - begin_consume);
    if (Print_task > 1)
	fprintf(stderr, "merge part %d merge done at %s\n", 
		merge_index, get_time_str(sort, tstr));
    
    /* if end_of_merge, ward off other sprocs from taking a merge task.
     */
    if (curr->end_of_merge)
	MERGE.end_of_merge = 1;

    if (sort->input_phase)
    {
	/* find out how many records were put into the rec_out_buf.
	 * only the last rec_out_t has a non-null end.  the loop
	 * intentionally leaves 'rec_out' pointing to the last one filled.
	 */
	recs_merged = 0;
	head = curr->u.blks;
	for (i = 0; ; i++, rec_out++)
	{
	    if (rec_out->end == NULL)
	    {
		head->n_recs = (unsigned)
		    (rec_out->limit - rec_out->first) / sort->edited_size;
		recs_merged += head->n_recs;
		head->last_offset = (unsigned)
		    (rec_out->limit - (byte *)head) - sort->edited_size;
	    }
	    else
	    {
		head->n_recs = (unsigned)
		    (rec_out->end - rec_out->first) / sort->edited_size;
		recs_merged += head->n_recs;
		head->last_offset = (unsigned)
		    (rec_out->end - (byte *)head) - sort->edited_size;
		break;
	    }
	    head = head->u.next;	/* get next block in link-list */
	}
	/* tell chk_temp_output() how many temp bufs merge_rec_out() touched
	 */
	curr->n_temp_blocks = i + 1;
    }
    else
	recs_merged = (unsigned) (rec_out[0].end - first_rec) / sort->edited_size;

    if (merge_index == 0)
    {
	tail_rec = rec_out[0].end - sort->edited_size;

	/* relevant only in output_phase
	 */
	curr->begin_out = curr->u.buf + OUT.rec_buf_pad;
	offset = 0;
    }
    else
    {
	/* if prev merge has not yet completed, wait for it.
	 */
	while (MERGE.merge_done < merge_index)
	    ordnap(1, "merge_part prev merge");

	if (Print_task > 1)
	    fprintf(stderr, "merge part %d prev wait done at %s\n", 
		    merge_index, get_time_str(sort, tstr));

	offset = prev->end_offset;
	
	/* if end of merge has been reached and there is no output data
	 */
	if (curr->end_of_merge && rec_out[0].end == first_rec)
	{
	    /* set up buffer status for quick exit indicating no data
	     */
	    tail_rec = curr->begin_out = curr->u.buf;/* XXX OK for internal? */
	    curr->n_temp_blocks = 0;	/* relevant only in input phase */
	}
	else
	{
	    /* process list of tbufs normally consumed, if any.
	     */
	    if (args.norm != NULL)
		process_norms(sort, args.norm, merge_index);
	    
	    /* check if the first_rec from curr merge needs to be deleted
	     * relative to last_rec of prev merge.
	     */
	    if (sort->any_deletes)
	    {
		args.recode.summarize = sort->n_summarized;
		args.recode.win_id = 0;
		args.recode.curr_id = 1;
		args.recode.win_ovc = sort->virgin_ovc;
		args.recode.curr_ovc = sort->virgin_ovc;
		(*sort->recode)(sort, &args.recode, (item_t *) prev->tail_rec,
						    (item_t *) first_rec);
		if (args.recode.curr_ovc == (sort->del_dup_ovc + 1))
		{
		    if (sort->input_phase)
		    {
			head = curr->u.blks;
			head->n_recs--;
			head->first_offset += sort->edited_size;
		    }
		    else
			first_rec += sort->edited_size;
		}
	    }	
	    
	    if (sort->input_phase)
	    {
		/* copy tail_rec from previous merge into beginning of first
		 * block.
		 */
		head = curr->u.blks;
		head->n_recs++;
		head->first_offset -= sort->edited_size;
		bcopy(prev->tail_rec, (byte *)head + head->first_offset,
		      sort->edited_size);
	    }
	    else
	    {
		if (sort->internal)
		{
		    /* We may have to shift this merge's results towards the
		     * beginning of the result buffer if the previous merge
		     * did not actually end where it guessed it would
		     */
		    end_rem = prev->tail_rec + sort->edited_size;
		}
		else
		{
		    /* If the prev merge's remainder can fit between the
		     * beginning of the second block in the buffer and first_rec
		     * then start the output at the second block,
		     * else start the output at the first block. 
		     */
		    prev_rem_size = (unsigned) (prev->tail_rec - prev->remainder) +
				    sort->edited_size;
			    
		    if (prev_rem_size <= first_rec - (curr->u.buf + OUT.rec_buf_pad))
			curr->begin_out = curr->u.buf + OUT.rec_buf_pad;
		    else
			curr->begin_out = curr->u.buf;

		    /* copy prev merge's remainder into curr output buffer.
		     */
		    bcopy(prev->remainder, curr->begin_out, prev_rem_size);
		    end_rem = curr->begin_out + prev_rem_size;
		}
		
		/* if there is any output for the current merge and 
		 * the end of the just copied remainder is not equal to
		 * the first record for this merge, then shift the current
		 * output down to meet the tail of the remainder data.
		 */
		merge_out_size = rec_out[0].end - first_rec;
		if (merge_out_size && end_rem != first_rec)
		    bcopy(first_rec, end_rem, merge_out_size);
	    
		/* calculate the new tail_rec
		 */
		tail_rec = end_rem + merge_out_size - sort->edited_size;
	    }
	}
    }

    if (sort->input_phase)
    {
	/* if not end_of_merge, find tail_rec and decrement record count
	 * in last block so that it isn't a valid part of the block.
	 */
	if (!curr->end_of_merge)
	{
	    head = curr->u.blks;
	    for (i = 1; i < curr->n_temp_blocks; i++)
		head = head->u.next;

	    tail_rec = (byte *)head + head->last_offset;
	    head->last_offset -= sort->edited_size;
	    
	    /* if the tail_rec was the only record on the block, decrement
	     * count of blocks for this merge, since outputing the tail_rec
	     * is the responsibility of the following merge.
	     */
	    if (--head->n_recs == 0)
		curr->n_temp_blocks--;

	    /* check for no output blocks as chk_temp_write() cannot handle
	     * this case.  This should never happen unless the number of
	     * records consumed is so ridiculously small that it is not
	     * greater than the input phase merge width.  
	     */
	    ASSERT(curr->n_temp_blocks != 0); 
	}
    }
    else if (!sort->internal)	/* out_size, remainder are for the i/o sproc */
    {
	/* calculate the size of the output buffer as the largest multiple of
	 * OUT.dio_io which is equal to or less than the beginning of
	 * tail_rec.  (tail_rec cannot be included in the write because it may
	 * be modified as part of a summarization with the first record in the
	 * next merge.)
	 */
	curr->out_size = (unsigned)
		    ((tail_rec - curr->begin_out) / OUT.dio_io) * OUT.dio_io;
	curr->remainder = curr->begin_out + curr->out_size;

	/* if we are using buffered (non-direct) writes, write it out now.
	 */
	if (curr->out_size && OUT.file->copies_write)
	{
	    if (curr->end_of_merge)
		set_file_flags(OUT.file, FSYNC);
	    if (pwrite(OUT.file->fd, curr->begin_out, curr->out_size, offset) < 0)
	    {
		if (errno == EPIPE || errno == ENOSPC)
		{
		    fprintf(stderr, "nsort: %s\n", strerror(errno));
		    ordinal_exit();
		}
		
		die("merge_part: write (0x%x, %d) @ %lld failed: %s",
		    curr->begin_out, curr->out_size, offset, strerror(errno));
	    }
	}
	curr->end_offset = offset + curr->out_size;
    }

    if (Print_task)
	fprintf(stderr, "merge_part #%d: genned %d recs; size %d; remain %d\n",
		merge_index, recs_merged, 
		sort->input_phase ? curr->n_temp_blocks : curr->out_size,
		sort->input_phase ? 0 : tail_rec - curr->remainder);

    curr->tail_rec = tail_rec;

    /* Add this sproc's recode counts to the sums in the sort struct.
     *
     * This can be done here without explicit serialization because we
     * have waited for the previous merge partition to complete, and we
     * haven't yet incremented merge_done (thereby indicating this merge 
     * partition is done).
     */
    TIMES.hard_recodes += args.recode.n_hard;
    TIMES.soft_recodes += args.recode.n_soft;

    /* Increment merge_done count to indicate this merge has completed.
     */
    MERGE.merge_done++;

    if (Print_task > 1)
	fprintf(stderr, "merge part %d done at %s\n", 
		merge_index, get_time_str(sort, tstr));
}

/* allocate output data structures for a record sort.
 */
void alloc_out_lists(sort_t *sort)
{
    unsigned		i;
   
    /* Initialize the structure and tournament tree for each record output.
     * The actual record buffers will be initialized by map_io_buf() since
     * they must consist of pinned memory.
     */
    MERGE.rob = (rec_out_buf_t *) calloc(MERGE.merge_cnt, sizeof(MERGE.rob[0]));
    if (!sort->internal)
    {
	/* These are for writes to temp files in a two-pass sort
	 */
	for (i = 0; i < MERGE.merge_cnt; i++)
	{
	    MERGE.rob[i].rec_outs = (rec_out_t *)
			    malloc(TEMP.merge_part_blks * sizeof(rec_out_t));
	}
    }
}
