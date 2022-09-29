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
 *	$Id: io.c,v 1.1 1996/12/24 17:47:49 avr Exp $
 */
#ident	"$Revision: 1.1 $"

#include "otcsort.h"
#include "merge.h"
#include <fcntl.h>
#include <sys/cachectl.h>
#include <sys/mman.h>
#include <sys/times.h>
#include <math.h>

char *print_copy_task(copy_task_t *task, char *buf);	/* in nsort.c */

/* next_input_file - set up next input file for reading.
 *
 * 	Returns TRUE if there is another input file.
 * 	Returns FALSE if there is NOT another input file.
 */
int next_input_file(sort_t *sort)
{
    fileinfo_t	*file;

    /* if we have already recognized we are done with all input files
     */
    if ((file = IN.curr_file) == IN.files + IN.n_files)
	return (FALSE);

    /* if we already have a current input file, close it.
     */
    if (file >= IN.files)
    {
	close_aio_fds(sort, IN.aiocb, IN.aio_cnt, 
			    file->name, NSORT_CLOSE_FAILED);
    }
    else
	file = IN.files - 1;

    IN.begin_run_offset = 0;
    IN.target_size = sort->in_run_data;

    /* if we still have more input files from which to read start next
     */
    while (++file != IN.files + IN.n_files)
    {
	if (Print_task)
	    fprintf(stderr, "switching to input file %d: %s\n", 
			    file - IN.files, file->name);
	    
	/* if file has a non-zero size or is not a normal file, use it.  
	 */
	if (file->stbuf.st_size || !SIZE_VALID(file->stbuf.st_mode))
	{
	    IN.read_size = file->io_size;
	    IN.aio_cnt = file->aio_count;

	    if (!sort->api_invoked)
	    {
		if (file->io_mode == IOMODE_MAPPED)
		    sort->read_input = ord_map_copyonwrite;
		else if (file->io_mode == IOMODE_SERIAL)
		    sort->read_input = ord_read;
		else
		    sort->read_input = ordaio_read;

		obtain_aio_fds(sort,
			       IN.aiocb,
			       IN.aio_cnt,
			       file,
			       NSORT_INPUT_OPEN);
	    }

	    IN.curr_file = file;
	    return (TRUE);
	}
    }
    IN.curr_file = file;

    /* if we fell out of the above while loop, we are done with all input files
     */
    ASSERT(IN.aiocb[0].aio_fildes == -1);
    
    /* if there is no carryover data to the next in_run, 
     * we are done with file input.
     */
    TIMES.read_first = IN.files[0].stats.start_first - TIMES.begin;
    TIMES.read_done = IN.files[IN.n_files - 1].stats.finish_last - TIMES.begin;

    IN.end_of_input = TRUE;
    
    /* let sorting sprocs know when to quit input sorting.
     */
    IN.part_final = IN.runs_read * IN_RUN_PARTS;

    return (FALSE);
}


/* chk_input - check if there is any io sproc activity necessary for
 *		reading from input files, and perform it if necessary.
 *
 *	returns: TRUE if there was nothing to do.
 */
int chk_input(sort_t *sort)
{
    int			idle;		/* return value for this function */
    aiocb_t		*aiocb;
    int			bytes_read;
    unsigned		read_size;
    fileinfo_t		*input = IN.curr_file;
    int			err;
    in_run_t		*run;
    int			i;
    unsigned		time;
    char		tstr[40];

    idle = TRUE;	/* assume boredom until proved otherwise */

    run = &sort->in_run[IN.runs_read % N_IN_RUNS];

    /* while there are outstanding aio_read()'s
     */
    while (IN.reads_done < IN.reads_issued)
    {
	/* if the first issued aio_read() has not yet completed, break out.
	 */
	aiocb = &IN.aiocb[IN.reads_done % IN.aio_cnt];
	if ((err = (sort->get_error)(aiocb)) == EINPROGRESS)
	    break;
	
	idle = FALSE;

	if (err != 0)
	    die("chk_input: read #%d failed %d @ %lld: %s",
		IN.reads_done, aiocb->aio_nbytes, (i8) aiocb->aio_offset,
		strerror(err));
	if ((bytes_read = (int) (sort->get_return)(aiocb)) < 0)
	{
	    die("aio_return read error: %s on i/o %d", 
		strerror((sort->get_error)(aiocb)), IN.reads_done);
	}
	if (Print_task)
	    fprintf(stderr, "aio_read %d %d @ %lld done at %s\n", 
		    IN.reads_done, bytes_read, (i8) aiocb->aio_offset,
		    get_time_str(sort, tstr));
	
	IN.reads_done++;

	/* Are there now no reads still active? If so end a busy period
	 */
	if (IN.reads_done == IN.reads_issued)
	{
	    time = get_time();

	    input->stats.busy += time;
	    input->stats.finish_last = time;
	}

	/* if there is no remainder data for this run, and this is the
         * first read for this run
	 */
	if (!IN.run_remainder && run->read_start == run->read_end)
	{
	    /* if read is non-zero, allow sort sprocs to arbitrate for 
	     * sorting responsibilities.  
	     */
	    if (bytes_read)
		IN.part_avail += IN_RUN_PARTS;
	    else
	    {
		/* there is no remainder data for this in_run, there is no
		 * more data in the current input file, and the sorting sprocs
		 * have never even heard (via IN.part_avail) about this in_run.
		 *
		 * attempt to get next input file
		 */
		if (!next_input_file(sort))
		    return (FALSE);	/* no additional input file, return 
					 * (we are done with input). */
		else
		    break;	/* break out of read done while().
				 * the reading for the in_run will continue
				 * with the new input file */
	    }
	}


	/* Update read_end to let partition sorters know where we are.
	 * Don't update it if a sorting sproc has already faked an eof
	 * because the maximum record count was reached - the sorting sproc
	 * will have set read_end to just past the last record.
	 * XXX now irrelevent? - prepare_part no longer sets read_end
	 *
	 * Also if mapped, we touch each page before allowing the sorting
	 * sprocs to 'see' it. This uses less sys cpu than everyone touching.
	 */
	if (!run->eof)
	{
	    if (input->io_mode != IOMODE_MAPPED)
		run->read_end += bytes_read;
	    else
	    {
		byte		*p;
		byte		*end = run->read_end + bytes_read;
		int		touch_sum = 0;
		int		count;
		int		user, sys, us;
		int		touchsize = getpagesize();
		struct tms	mapping;

		times(&mapping);
		us = -get_time_us();
		user = mapping.tms_utime;
		sys = mapping.tms_stime;

		for (p = run->read_end, count = bytes_read / touchsize;
		     count > 0;
		     p += touchsize << 1, count -= 2)
		{
		    touch_sum += *p + p[touchsize];
		    if ((count & 0xfff) == 0)
			run->read_end = p;
		}
		run->read_end = end;
		SYS.touch_sum = touch_sum;

		if (Print_task)
		{
		    times(&mapping);
		    us += get_time_us();
		    fprintf(stderr, "chk_input: faulting in %d pages took u %d, sys %d, %d.%03d ms\n",
				    bytes_read / SYS.page_size,
				    mapping.tms_utime - user,
				    mapping.tms_stime - sys, us / 1000, us % 1000);
		}
	    }
	}

	if (input->io_mode == IOMODE_SERIAL)
	    IN.intra_run_offset += bytes_read;
	
	if (bytes_read == 0 || 
	    (input->io_mode != IOMODE_SERIAL && bytes_read < aiocb->aio_nbytes)
	    ||
	    (SIZE_VALID(input->stbuf.st_mode) &&
	     run->read_end - run->read_start + IN.begin_run_offset == 
	         input->stbuf.st_size/*XXX never true if IN.n_files>1*/))
	{
	    run->eof = TRUE;
	}
    }

    /* if there are no aio_read()s outstanding and the sorting sprocs have
     * indicated they are done scanning data for the current in_run, we are
     * done with this in_run.
     */
    if (IN.runs_read < IN.runs_init &&
	IN.reads_done == IN.reads_issued && 
	run->scan_complete)
    {
	IN.begin_run_offset += run->read_end - run->read_start;
	input->stats.bytes += run->read_end - run->read_start;

	if (Print_task)
	    fprintf(stderr, "in_run %d read done at %s end %x\n", 
		    IN.runs_read, get_time_str(sort, tstr), run->read_end);
	if (sort->two_pass && (IN.runs_read == TEMP.run_limit))
	{
	    runtime_error(sort, NSORT_INPUT_TOO_LARGE);
	    return (FALSE);		/* not idle; quit sort asap */
	}
	IN.runs_read++;

	/* calculate remainder data at the end of this in_run.
	 */
	IN.run_remainder = run->read_end - run->rec_used;

	/* if sorting sprocs told us they have exhausted the record limit
	 */
	if (run->final_run)
	{
	    /* skip over all remaining file.  This is necessary to properly
	     * record statistics for the current input file, and to properly
	     * initialize the output file.
	     */
	    while (next_input_file(sort) == TRUE)
		continue;
	    return (FALSE);
	}
	
	/* if there is no more data to read from the current input file
	 */
	if (run->eof)
	{
	    /* if there is remainder data, do another in_run (ostensibly with
	     * the current input file reaching eof immediately) to use the 
	     * remainder data.
	     */
	    if (IN.run_remainder)
		IN.eof_remainder = TRUE;
	    else
	    {
		/* if not another input file, return (we are done with input).
		 */
		if (!next_input_file(sort))
		    return (FALSE);
	    }
	}
	
	/* if the number of runs read is now N_IN_RUNS, force this sort
	 * to two passes.  If the user hasn't specified any temporary files,
	 * create one in the default tmp directory.
	 */
	if (IN.runs_read == N_IN_RUNS && sort->two_pass == FALSE)
	{
	    if (Print_task)
		fprintf(stderr, "switching to a two pass sort\n");
	    sort->two_pass = TRUE;	
	    verify_temp_file(sort);
	}

	run = &sort->in_run[IN.runs_read % N_IN_RUNS];
    }

    /* if we can initialize a new run, do so.
     */
    if (IN.runs_init == IN.runs_read &&
	IN.runs_init < IN.runs_filled + N_IN_RUNS)
    {
	byte	*normal_start;

	for (i = 0; i < IN_RUN_PARTS; i++)
	    run->part_lists[i] = NULL;
	run->scan_complete = FALSE;
	run->final_run = FALSE;
	run->eof = FALSE;

	/* set first_rec to be REM_BUF_SIZE past read_buf (the normal read
	 * start position) if there is no remainder, otherwise set it so
	 * that it starts in the first REM_BUF_SIZE bytes, and so the
	 * remainder data ends on a REM_BUF_SIZE boundry.  Hence read_start
	 * will be at least REM_BUF_SIZE bytes from read_buf, and will be
	 * a REM_BUF_SIZE multiple away from read_buf.
	 */
	normal_start = run->read_buf + REM_BUF_SIZE;
	run->read_start = normal_start;
	if (IN.run_remainder)
	    run->read_start = 
		run->read_buf + ROUND_UP_COUNT(IN.run_remainder, REM_BUF_SIZE);
	run->first_rec = run->read_start - IN.run_remainder;
	run->read_end = run->read_start;

	read_size = IN.target_size;

	/* subtract any remainder data past the normal starting place from
	 * the read_size, potentially even reducing read_size to 0.
	 */
	if (read_size >= run->read_start - normal_start)
	    read_size -= run->read_start - normal_start;
	else
	    read_size = 0;

	/* if we have a limit on the number of records read.
	 */
	if (sort->n_recs > 0)
	{
	    i8	maxbytesleft;

	    /* calculate the maximum number of bytes left.  if it is more than
	     * the remainder data, but less than the remainder plus read_size,
	     * reduce read_size so that we don't read extraneous data.
	     */
	    maxbytesleft = RECORD.maxlength * sort->recs_left;
	    if (maxbytesleft > IN.run_remainder &&
		maxbytesleft < IN.run_remainder + read_size)
	    {
		read_size = maxbytesleft - IN.run_remainder;
		read_size = ROUND_UP_COUNT(read_size, IN.dio_io);
	    }
	}

	run->read_target = run->read_start + read_size;
	run->file_num = (short) (input - IN.files);
	IN.intra_run_offset = 0;
	IN.begin_reads_issued = IN.reads_issued;

	IN.runs_init++;

	/* If there is remainder data from the previous run
	 */
	if (IN.run_remainder)
	{
	    /* immediately allow sorting sprocs to arbitrate for sorting duties
	     */	
	    IN.part_avail += IN_RUN_PARTS;

	    /* if we have reached eof on the previous run, but not all the
	     * consumed data (accepted by sort sprocs) was used, then the next
	     * run shall consist solely of the remainder data.  As such we are
	     * done with input for this run right off the bat.  
	     */
	    if (IN.eof_remainder)
	    {
		run->eof = TRUE;
		IN.eof_remainder = FALSE;
	    }
	}
	/* else without remainder data, we will wait until a non-zero read
         * is completed before letting sorting sprocs arbitrate for sorting
	 * duties.  (Empty runs are NOT allowed!) */

	if (Print_task)
	    fprintf(stderr, "init in_run %d (%s bytes) cpu %d at %s\n", 
		    IN.runs_read, print_number(read_size), get_cpu_time(),
		    get_time_str(sort, tstr));
    }

    /* While there is an aio_read structure available and
     * we have already initialized the current run and
     * we haven't yet reached the end of input for the current in_run.
     */
    while (IN.reads_issued < IN.reads_done + IN.aio_cnt && 
	   IN.runs_read < IN.runs_init &&
	   IN.intra_run_offset < run->read_target - run->read_start && 
	   !run->eof &&
	   (!SIZE_VALID(input->stbuf.st_mode) ||
	    IN.intra_run_offset + IN.begin_run_offset < input->stbuf.st_size))
    {
	aiocb = &IN.aiocb[IN.reads_issued % IN.aio_cnt];

	/* schedule a read of the buffer
	 */
	aiocb->aio_offset = IN.intra_run_offset + IN.begin_run_offset;
	aiocb->aio_buf = run->read_start + IN.intra_run_offset;
	read_size = (run->read_target - run->read_start) - IN.intra_run_offset;
	if (read_size > IN.read_size && input->io_mode != IOMODE_MAPPED)
	    read_size = IN.read_size;
	aiocb->aio_nbytes = read_size;

	/* if the end of this read is beyond the point that we have touched
	 * memory up to, then skip the read.  Wait until the touching has
	 * progressed past the end of the prospective read.
	 */
	if (run->read_start + IN.intra_run_offset + read_size > SYS.end_touched)
	{
	    if (Print_task)
		fprintf(stderr, "chk_input: read %d deferred, waiting for touch @ 0x%x\n",
				IN.reads_issued, run->read_start + IN.intra_run_offset + read_size);
	    break;
	}

	idle = FALSE;

	if (input->io_mode != IOMODE_SERIAL)
	    IN.intra_run_offset += aiocb->aio_nbytes;
	if (Print_task)
	    fprintf(stderr, "aio_read %d: %08x, %lld, %d at %s\n", 
		    IN.reads_issued, aiocb->aio_buf, (i8) aiocb->aio_offset, 
		    aiocb->aio_nbytes, get_time_str(sort, tstr));
	
	/* If there are no reads going on, then it's time to update stats -
	 * start the busy meter running, perhaps set file's first read time.
	 */
	if (IN.reads_issued == IN.reads_done)
	{
	    time = get_time();
	    if (input->stats.ios == 0)
		input->stats.start_first = time;
	    input->stats.busy -= time;
	}
	input->stats.ios++;
	
	if ((sort->read_input)(aiocb) < 0)
	    die("aio_read failure: %s", strerror(errno));

	IN.reads_issued++;
    }

    /* return TRUE if there was nothing interesting to do
     */
    return (idle);
}


/*
 * gather_run_stats	- record statistics generated by this now-completed run
 */
void gather_run_stats(sort_t *sort, in_run_t *run)
{
    part_t	*part;
    int		i;

    for (i = 0, part = run->partitions; i < run->n_parts; i++, part++)
    {
	STATS.sort_partition += part->sort_time;
	STATS.genline_time += part->genline_time;
	STATS.hash_partition += part->hash_time;
	STATS.hash_deletes += part->hash_deletes;
	STATS.hash_replaces += part->hash_replaces;
	STATS.hash_stopsearches += part->hash_stopsearches;
    }
    STATS.n_parts += run->n_parts;
}


/* chk_temp_write - check if there is any io sproc activity necessary for
 *		    writing to temp files, and perform it if necessary.
 *
 *	returns: TRUE if there was nothing to do.
 */
int chk_temp_write(sort_t *sort)
{
    int			idle;		/* return value for this function */
    aiocb_t		*aiocb;
    in_run_t		*run;
    char		tstr[40];
    int			i;
    copy_task_t		*copy;
    blockheader_t	*head;
    unsigned		total;
    unsigned		target;
    unsigned		n_recs;
    unsigned		items_left;
    ptr_out_buf_t	*ptr_part;
    rec_out_buf_t	*rob;
    byte		**curr;
    byte		**limit;
    byte		*last_position;
    unsigned		rec_size;
    tempfile_t		*tfile;
    out_blk_t		*adv_blk;
    out_blk_t		*out_blk;
    adv_info_t		*adv_info;
    u4			loff;
    char		copystr[100];
    unsigned		time;
    unsigned		run_blk_limit;
    unsigned		run_blks_filled;
    unsigned		prior_blks;
    unsigned		run_writes_issued;
    
    idle = TRUE;	/* assume boredom until proven otherwise */

    /* if the number of runs recognized as sorted but not yet filled
     * (merged and copied into block buffers) is less than the number of
     * runs read, and there are no partially filled runs, and we have a
     * block available for filling.
     */
    if (IN.runs_sorted < IN.runs_read && 
	IN.runs_sorted == IN.runs_filled &&
	TEMP.blks_filled != TEMP.writes_done + TEMP.out_blk_cnt)
    {
	run = &sort->in_run[IN.runs_sorted % N_IN_RUNS];
	
	/* transfer list of completed partition line-list (if complete)
	 * to merge list.
	 */
	for (i = 0; i < run->n_parts; i++)
	{
	    if (run->part_lists[i] == NULL)
		break;
	    MERGE.run_list[i] = run->part_lists[i];
	}
	/* 
	 * if the sorting of all partitions in the in_run has completed,
	 * then prepare the in_run for filling.
	 */
	if (i == run->n_parts)
	{
	    unsigned	f_index;

	    idle = FALSE;

	    if (Print_task)
		fprintf(stderr, "in_run %d sorted\n", IN.runs_sorted);

	    /* if this is the first run and its a record sort
	     */
	    if (IN.runs_sorted == 0 && sort->method == SortMethRecord)
	    {
		/* make free list of blocks
		 */
		head = (blockheader_t *)MERGE.rec_bufs;
		TEMP.free_blk = head;
		for (i = 0; i < TEMP.out_blk_cnt; i++)
		{
		    blockheader_t	*next_head;

		    next_head =
			(blockheader_t *)((byte *)head + TEMP.blk_size);
		    head->u.next = 
			(i == TEMP.out_blk_cnt - 1) ? NULL : next_head;
		    head = next_head;
		}
		TEMP.free_blk_cnt = TEMP.out_blk_cnt;
	    }

	    /* put the first block of the run one temp file up from where the
	     * previous run's first block was placed.  this ensures the first
	     * blocks for all runs are spread out evenly among the temp files. 
	     *
	     * remember beginning temp file and offset for this run.
	     */
	    f_index = IN.runs_sorted % TEMP.n_files;
	    TEMP.run[IN.runs_sorted].tfile = f_index;
	    TEMP.run[IN.runs_sorted].offset = TEMP.files[f_index].write_offset;
	    TEMP.run[IN.runs_sorted].n_blks = 0;
	    TEMP.next_tfile = f_index;
	    TEMP.run_filled = FALSE;
	    TEMP.run_planned = FALSE;

	    /* note the number of blocks generated so far so that we can
	     * determine run block numbers.
	     */
	    TEMP.begin_run_blks = TEMP.blks_filled;

	    MERGE.merge_scanned = 0;
	    COPY.buf_position = NULL;
	    COPY.curr_rec = NULL;
	    MERGE.tails_done = 0;
	    MERGE.tails_scanned = 0;

	    MERGE.width = run->n_parts;
    
	    /* set the merge skip interval to twice the square root of the
	     * number of records (or record pointers) consumed in each merge
	     * partition.
	     */
	    MERGE.skip_interval = (unsigned)
		(2.0 * 
		 sqrt((sort->method == SortMethPointer ? 
		     (double)MERGE.part_recs :
		     (double)OUT.buf_size / sort->edited_size) / MERGE.width));

	    /* carefully reset the merge_avail and merge_taken counts
	     * for next in_run merge
	     */
	    MERGE.merge_avail = 0;	/* make sure avail count is not more
					 * than current taken count before we
					 * reset taken count to zero */
	    MERGE.merge_taken = 0;
	    MERGE.skip_done = 0;
	    MERGE.merge_done = 0;
	    MERGE.recs_consumed = 0;

	    if (sort->method == SortMethRecord)
	    {
		TEMP.part_block_sched = 0;
		TEMP.next_part_blk = NULL;
	    }
	    
	    /* if we are doing optimal reads, make a fake first block with only
	     * advance block information.
	     */
	    if (TEMP.optimal_reads)
	    {
		out_blk = &TEMP.out_blk[TEMP.blks_filled % TEMP.out_blk_cnt];

		if (sort->method == SortMethRecord)
		{
		    /* allocate block from free list and assign it to first
		     * out_blk struct.
		     */
		    head = TEMP.free_blk;
		    TEMP.free_blk = head->u.next;
		    TEMP.free_blk_cnt--;
		    out_blk->head = head;
		}
		else
		{
		    /* ptr sort: blks are already allocated to out_blk structs.
		     */
		    head = out_blk->head;
		}

		head->first_offset = MAX_I4;	/* guaranteed to fail */
		head->last_offset = MAX_I4;	/* guaranteed to fail */
		head->n_recs = 0;
		
		head->run = IN.runs_filled;
		head->block = 0;
		
		out_blk->tfile = f_index;
		tfile = TEMP.files + f_index;
		out_blk->off = tfile->write_offset;
		tfile->write_offset += TEMP.blk_size;
		if (++f_index == TEMP.n_files)
		    f_index = 0;
		TEMP.next_tfile = f_index;
		
		TEMP.blks_filled++;
		COPY.bufs_planned++;
	    }

	    /* let there be merging
	     */
	    if (sort->method == SortMethPointer)
		MERGE.merge_avail = MERGE.merge_cnt;

	    /* Gather partition sorting statistics here.  This is a good point
	     * to gather them since this io sproc code is naturally serialized,
	     * whereas having the sort sprocs accumulate the statistics is
	     * trickier and slower because of serialization considerations.
	     */
	    if (sort->statistics)
		gather_run_stats(sort, run);
	    
	    IN.runs_sorted++;
	}
    }

    /* while there are outstanding aio_write()'s
     */
    while (TEMP.writes_done < TEMP.writes_issued)
    {
	/* if the first issued aio_write has not yet completed, break out.
	 */
	out_blk = &TEMP.out_blk[TEMP.writes_done % TEMP.out_blk_cnt];
	tfile = TEMP.files + out_blk->tfile;
	aiocb = tfile->aiocb + (tfile->io_done % TEMP.aio_cnt);
	if (aiocb->aio_nbytes && aio_error(aiocb) == EINPROGRESS)
	    break;

	idle = FALSE;

	if (aiocb->aio_nbytes && aio_return(aiocb) != aiocb->aio_nbytes)
	    die("chk_temp_write, aio_return write error: %s", 
		strerror(aio_error(aiocb)));

	if (Print_task)
	    fprintf(stderr, "temp aio_write %d done at %s\n", 
		    TEMP.writes_done, get_time_str(sort, tstr));

	/* determine number of blocks that were generated for the previous runs.
	 */
	prior_blks = IN.runs_flushed > 0 ?
	    TEMP.run[IN.runs_flushed - 1].n_blks : 0;

	if (out_blk->head->run != IN.runs_flushed ||
	    out_blk->head->block != TEMP.writes_done - prior_blks)
	{
	    die("temp write done failure: %d %d, %d %d\n",
		out_blk->head->run, IN.runs_flushed,
		out_blk->head->block, TEMP.writes_done - prior_blks);
	}

	TEMP.writes_done++;
	tfile->io_done++;

	/* If we were in a waiting period
	 */
	if (TEMP.waiting != NULL)
	{
	    /* if there is still at least one sproc idling, credit
	     * (debit?) offending temp file with the wait, otherwise
	     * ignore whole wait.
	     */
	    if (TEMP.n_idling)
		TEMP.waiting->write_stats.wait += get_time() - TEMP.start_wait;
	    if (Print_task > 1)
		fprintf(stderr, "temp wait done, idle %d at %s\n",
			TEMP.n_idling, get_time_str(sort, tstr));
	    TEMP.waiting = NULL;
	}

	/* if there is another temp disk write pending, and at least one
	 * idle sproc.
	 */
	if (TEMP.writes_done < TEMP.writes_issued && TEMP.n_idling)
	{
	    /* Start accounting for a temp file write wait
	     */
	    TEMP.waiting = TEMP.files +
		(TEMP.out_blk + (TEMP.writes_done %
				 TEMP.out_blk_cnt))->tfile;
	    if (Print_task > 1)
		fprintf(stderr, "temp wait restart at %s\n",
			get_time_str(sort, tstr));
	    TEMP.start_wait = get_time();
	}

	/* If there are no more writes active then end a busy period */
	if (tfile->io_done == tfile->io_issued)
	{
	    time = get_time();

	    tfile->write_stats.busy += time;
	    tfile->write_stats.finish_last = time;
	}

	/* if this was the last write for a run, increment IN.runs_flushed
	 */
	if (TEMP.writes_done == TEMP.run[IN.runs_flushed].n_blks)
	    IN.runs_flushed++;

	/* if this is the last temp blk write
	 */
	if (TEMP.writes_done == TEMP.writes_final)
	{
	    unsigned	prev_accum;
	    unsigned	temp;
	    
	    /* Adjust the run block counts.  For each run we have
	     * recorded the number of blocks generated up to and
	     * including that run.  We will adjust those counts to be
	     * simply the number of of blocks for each run.
	     */
	    prev_accum = TEMP.run[0].n_blks;
	    for (i = 1; i < IN.runs_write_sched; i++)
	    {
		temp = TEMP.run[i].n_blks;
		TEMP.run[i].n_blks -= prev_accum;
		prev_accum = temp;
	    }
	}
	
	if (sort->method == SortMethRecord)
	{
	    /* recycle block
	     */
	    head = out_blk->head;
	    head->u.next = TEMP.free_blk;
	    TEMP.free_blk = head;
	    TEMP.free_blk_cnt++;
	}	    
    }

    /* if we haven't yet recognized a sorted run as filled, yet we have
     * filled a run and all the merges for it (including extraneous ones)
     * have completed, then we have completely filled another run.
     */
    if (IN.runs_filled < IN.runs_sorted &&
	TEMP.run_filled && (MERGE.merge_scanned == MERGE.merge_avail))
    {
	idle = FALSE;

	/* remember the total number of blocks filled at the end of 
	 * number of block that had been filled at the beginning of the
	 * run.
	 */
	TEMP.run[IN.runs_filled].n_blks = TEMP.blks_filled;

	if (Print_task)
	    fprintf(stderr, "in_run %d filled %d blocks\n",
		    IN.runs_filled, TEMP.run[IN.runs_filled].n_blks);

	IN.runs_filled++;
    }

    if (sort->method == SortMethRecord)
    {
	/* Record Sort */

	/* while there are completed merges that haven't been completely scanned
	 */
	while (MERGE.merge_scanned < MERGE.merge_done)
	{
	    rob = MERGE.rob + (MERGE.merge_scanned % MERGE.merge_cnt);
	    
	    /* if we have already scheduled a merge partition that was the
	     * end of the merge, ignore any subsequent (and empty) merge
	     * partitions.  But do recycle the blocks allocated to them.
	     */
	    if (TEMP.run_filled)
	    {
		head = rob->u.blks;
		while (head != NULL)
		{
		    blockheader_t	*next_head;

		    next_head = head->u.next;
		    head->u.next = TEMP.free_blk;
		    TEMP.free_blk = head;
		    TEMP.free_blk_cnt++;
		    head = next_head;
		}
		MERGE.merge_scanned++;
		break;
	    }

	    /* If we don't have an out_blk struct available, break out.
	     * (There really is no need for this test as long as the total
	     * number of out_blks is out_blk_cnt.)
	     */
	    if (TEMP.blks_filled == TEMP.writes_done + TEMP.out_blk_cnt)
		break;

	    idle = FALSE;

	    /* if this is the first time we are scheduling blocks for this
	     * merge partition.
	     */
	    if (TEMP.part_block_sched == 0)
	    {
		/* Free first unused blocks from the previous merge, if any.
		 * The freeing of this block is delayed until this merge has
		 * completed because the block MAY contain the tail rec of the
		 * previous merge.
		 */
		if ((head = TEMP.next_part_blk) != NULL)
		{
		    head->u.next = TEMP.free_blk;
		    TEMP.free_blk = head;
		    TEMP.free_blk_cnt++;
		}

		head = rob->u.blks;	/* schedule first block of partition */
	    }
	    else
		head = TEMP.next_part_blk;	/* schedule next block */

	    TEMP.next_part_blk = head->u.next;

	    out_blk = TEMP.out_blk + (TEMP.blks_filled % TEMP.out_blk_cnt);
	    tfile = TEMP.files + TEMP.next_tfile;

	    head->run = IN.runs_filled;
	    head->block = TEMP.blks_filled - TEMP.begin_run_blks;

	    out_blk->head = head;
	    out_blk->tfile = TEMP.next_tfile;
	    out_blk->off = tfile->write_offset;
	    
	    tfile->write_offset += TEMP.blk_size;

	    if (++TEMP.next_tfile == TEMP.n_files)
		TEMP.next_tfile = 0;

	    TEMP.blks_filled++;

	    /* if this was the last block for this rec_out_buf (merge), use
	     * another rec_out_buf next time.
	     */
	    if (++TEMP.part_block_sched == rob->n_temp_blocks)
	    {
		/* TEMP.next_part_blk points to the first unused block of
		 * the merge partition, if any.  If this isn't the last
		 * merge with data, the first unused block isn't freed here
		 * because it MAY contain the tail rec of the merge, which
		 * is needed by the subsequent merge.  It will be freed
		 * when we begin to scan the subsequent merge.
		 * 
		 * if there are any unused blocks after the first unused block
		 * (if any), free them now.
		 */
		if (TEMP.next_part_blk != NULL)
		{
		    /* if the next merge (if it exists) is empty, and
		     * therefore can't possibly require the first unused
		     * block.
		     */
		    if (rob->end_of_merge)
		    {
			head = TEMP.next_part_blk;	/* free first unused */
			TEMP.next_part_blk = NULL;
		    }
		    else
			head = TEMP.next_part_blk->u.next; /* delay freedom for
							      first for now */
		    while (head != NULL)
		    {
			blockheader_t	*next_head;

			next_head = head->u.next;
			head->u.next = TEMP.free_blk;
			TEMP.free_blk = head;
			TEMP.free_blk_cnt++;
			head = next_head;
		    }
		}
		TEMP.part_block_sched = 0;
		MERGE.merge_scanned++;
		if (rob->end_of_merge)
		    TEMP.run_filled = TRUE;
	    }
	}

	/* while we haven't yet filled all the blocks for this in_run
	 * and we could make another merge available (if we had enough
	 * blocks to allocate to it).
	 */
	while (!TEMP.run_filled &&
	       TEMP.free_blk_cnt >= TEMP.merge_part_blks &&
	       MERGE.merge_avail < MERGE.merge_scanned + MERGE.merge_cnt - 1)
	{
	    idle = FALSE;

	    rob = &MERGE.rob[MERGE.merge_avail % MERGE.merge_cnt];
	    head = TEMP.free_blk;
	    rob->u.blks = head;
	    for (i = 1; i < TEMP.merge_part_blks; i++)
		head = head->u.next;
	    TEMP.free_blk = head->u.next;
	    head->u.next = NULL;
	    TEMP.free_blk_cnt -= TEMP.merge_part_blks;
	    MERGE.merge_avail++;
	}
    }
    else
    {
	/* Pointer Sort */

	/* while there are completed copy tasks...
	 */
	while (COPY.copy_done < COPY.copy_avail &&
	       (copy = &COPY.copy[COPY.copy_done % COPY.copy_cnt])->copy_done)
	{
	    idle = FALSE;

	    /* if the copy task was the last one for an output buffer,
	     * write the output buffer.
	     */
	    if ((head = (blockheader_t *)copy->out_buf) != NULL)
	    {
		out_blk = TEMP.out_blk + (TEMP.blks_filled % TEMP.out_blk_cnt);
		if (head != out_blk->head)
		    die("chk_temp_write, block head inconsistency: %08x %08x",
			head, out_blk->head);
		tfile = TEMP.files + TEMP.next_tfile;
		
		head->run = IN.runs_filled;
		head->block = TEMP.blks_filled - TEMP.begin_run_blks;
		
		out_blk->head = head;
		out_blk->tfile = TEMP.next_tfile;
		out_blk->off = tfile->write_offset;
		
		tfile->write_offset += TEMP.blk_size;
		
		if (++TEMP.next_tfile == TEMP.n_files)
		    TEMP.next_tfile = 0;
	
		TEMP.blks_filled++;
	    }
	    
	    /* if the copy task was the last one for a pointer array,
	     * make the one before it available for a merge.
	     * (The reason the current one is not made available is that its
	     * tail record info may still be needed by the next merge, and
	     * we wouldn't want to a new merge to overwrite the tail rec info.)
	     */
	    if (copy->merge_part_done)
		MERGE.merge_avail++;

	    COPY.copy_done++;

	    if (copy->end_of_merge)
		TEMP.run_filled = TRUE;
	}
	
	/* while there are unused copy task structures
	 */
	while (COPY.copy_avail < COPY.copy_done + COPY.copy_cnt)
	{
	    /* if we have already planned all copies for the entire run.
	     */
	    if (TEMP.run_planned)
	    {
		/* ignore any subsequent (and empty) merge partitions.
		 */
		if (MERGE.merge_scanned < MERGE.merge_done)
		    MERGE.merge_scanned++;
		break;
	    }

	    /* if there is no merge pointer array available for scanning, 
	     * break out.
	     */
	    if (MERGE.merge_scanned == MERGE.merge_done &&
		MERGE.tails_scanned == MERGE.tails_done)
	    {
		break;
	    }

	    /* If there is no output buffer available to plan a copy to,
	     * break out.  Begin an output wait period if it looks like
	     * this is a bottleneck, holding up cpus with nothing else to do.
	     */
	    if (COPY.bufs_planned == TEMP.writes_done + TEMP.out_blk_cnt)
		break;

	    idle = FALSE;

	    /* get ptr_part for the merge partition we are scanning.  this is
	     * always indexed by MERGE.tails_scanned because if
	     * MERGE.merge_scanned != MERGE.tails_scanned, we are scanning the
	     * tail portion of the array indexed by MERGE.tails_scanned.  if
	     * MERGE.merge_scanned == MERGE.tails_scanned, then we are scanning
	     * the main portion of the array indexed by MERGE.merge_scanned,
	     * but MERGE.tails_scanned will do just as well.
	     */
	    ptr_part = MERGE.ptr_part + (MERGE.tails_scanned % MERGE.merge_cnt);

	    head = TEMP.out_blk[COPY.bufs_planned % TEMP.out_blk_cnt].head;

	    /* if this is first copy for this temp block, start at beginning
	     * of block (otherwise use last ending place inside the block).
	     */
	    if (COPY.buf_position == NULL)
	    {
		head->first_offset = (int) sizeof(blockheader_t) +
		    (TEMP.optimal_reads ? RECORD.max_intern_bytes : 0);
		/* XXX initialize the block as if it were empty, since it is
		 * possible for a temp block to be empty (e.g. an empy run).
		 * It would be better to avoid writing the temp block at all.
		 */
		head->last_offset = head->first_offset;	/* XXX was MAX_I4 */
		head->n_recs = 0;
		COPY.buf_position = TEMPBLOCK_FIRST(head);

		if (!(RECORD.flags & RECORD_FIXED))
		{
		    /* compute the maximum number of items for the block.  While
		     * we want to fill blocks with records, too many smaller
		     * than expected variable length records in a block can lead
		     * to not enough item space in its tbuf during the output
		     * phase.  So we calculate the maximum number of items here
		     * and will stop filling the block with records if the limit
		     * is reached.
		     */
		    TEMP.items_left = (u4) (TEMP.tbuf_size - 
		       (sizeof(blockheader_t) + TEMP.blk_size + NULL_ITEM_SIZE))
							      / sizeof(item_t);
		}
	    }

	    /* if this if the first time we are scanning either part of this
	     * merge partition.
	     */
	    if (COPY.curr_rec == NULL)
	    {
		/* if we haven't scanned the main (pre-tail) portion of the 
		 * merge partition.
		 */
		if (MERGE.merge_scanned == MERGE.tails_scanned)
		{
		    if (ptr_part->end_of_merge)
		    {
			/* if there are no pointers for the merge partition,
			 * then skip over the entire merge partition without
			 * generating any copy tasks.  this case can happen
			 * if only one record pointer is output by the merge
			 * partition, but it is deleted relative to the tail
			 * record of the previous merge.  
			 */
			if (ptr_part->begin_ptr == ptr_part->limit_ptr)
			{
			    MERGE.merge_scanned++; /* done with merge part */
			    break;
			}

			/* output the tail record too since there is no
			 * subsequent merge to increment MERGE.tails_done
			 */
			COPY.limit_rec = (byte **)ptr_part->limit_ptr;
		    }
		    else
			COPY.limit_rec = (byte **)ptr_part->tail_ptr;
		    COPY.curr_rec = (byte **)ptr_part->begin_ptr;
		}
		else
		{
		    /* set up to scan the tail portion of the ptr array.
		     */
		    COPY.curr_rec = (byte **)ptr_part->tail_ptr;
		    COPY.limit_rec = (byte **)ptr_part->limit_ptr;
		    if (COPY.curr_rec == NULL)
			COPY.curr_rec = (byte **) ptr_part->limit_ptr;
		}
	    }

	    copy = COPY.copy + (COPY.copy_avail % COPY.copy_cnt);
	    copy->dest_buf = COPY.buf_position;
	    copy->src_rec = COPY.curr_rec;
	    copy->out_buf = NULL;
	    copy->merge_part_done = FALSE;
	    copy->copy_done = FALSE;
	    copy->end_of_merge = FALSE;

	    last_position = (byte *)head + head->last_offset;

	    /* set the target copy size to the minimum of the number of bytes
	     * left in the output buffer, and the target copy size.
	     */
	    target = (unsigned) (((byte *) head + TEMP.blk_size) -
				 COPY.buf_position);
	    if (target > COPY.target_size)
		target = COPY.target_size;

	    /* if the records are fixed length
	     */
	    if (RECORD.flags & RECORD_FIXED)
	    {
		/* round down target to a multiple of the record size.
		 */
		target = (target / sort->edited_size) * sort->edited_size;

		/* calculate the number of bytes left in the current merge 
		 * partition.
		 */
		total = (unsigned) ((COPY.limit_rec - COPY.curr_rec) *
				    sort->edited_size);

		/* if the total number of record bytes left in the merge
		 * partition is less than the target number of bytes, just
		 * copy the total. 
		 */
		if (total < target)
		{
		    COPY.curr_rec = COPY.limit_rec;
		}
		else
		{
		    /* else we have filled the block
		     */
		    COPY.curr_rec += target / sort->edited_size;
		    total = target;
		}

		/* if the copy will leave less than a record size at the
		 * end of the buffer, then indicate that this copy
		 * completes the buffer.
		 */
		if (COPY.buf_position + total + sort->edited_size >
		    ((byte *)head + TEMP.blk_size))
		{
		    copy->out_buf = (byte *)head;	/* indicate complete */
		}

		/* remember position of last record
		 */
		last_position = COPY.buf_position + total - sort->edited_size;
		
		/* increment count of records in the temp block header
		 */
		head->n_recs += total / sort->edited_size;
	    }
	    /*
	     * else the records are variable-length (variable or stream)
	     */
	    else
	    {
		int	var_hdr_extra = RECORD.var_hdr_extra;

		curr = COPY.curr_rec;
		limit = COPY.limit_rec;
		n_recs = 0;
		total = 0;
		items_left = TEMP.items_left;
		while (curr < limit)
		{
		    rec_size = *(u2 *)
			((byte *)curr + sizeof(unsigned *)) + var_hdr_extra;

		    if (total + rec_size > target)
		    {
			/* if the copy will leave less than a record size
			 * at the end of the buffer, then indicate that
			 * this copy completes the buffer.
			 */
			if (COPY.buf_position + total + rec_size >
			    ((byte *)head + TEMP.blk_size))
			{
			    copy->out_buf = (byte *)head;
			}

			/* The current record will not entirely fit in the
			 * copy.  Since we only transfer whole records to
			 * temp file blocks, forget this record for now.
			 */
			break;
		    }

		    /* remember position of last record before adding its
		     * size to total.
		     * XXX move this outside the loop, subtract final rec_size
		     * from total to get start of last record.
		     */
		    last_position = COPY.buf_position + total;

		    total += rec_size;
		    items_left--;
		    n_recs++;
		    curr = (byte **)
			((byte *)curr + sizeof(unsigned *) + sizeof(u2));

		    /* if no item space is left, don't add any more records.
		     */
		    if (items_left == 0)
		    {
			if (Print_task)
				fprintf(stderr,
					"chk_temp_write:items_left ran out: "
					"copy %x, outbuf %x\n",
					copy, head);
			copy->out_buf = (byte *)head;
			break;
		    }
		}
		COPY.curr_rec = curr;
		head->n_recs += n_recs;
	    }
	    copy->copy_bytes = total;
	    if (RECORD.flags & RECORD_FIXED)
		rec_size = sort->edited_size;
	    else
		rec_size = *(u2 *) ((byte *)copy->src_rec + sizeof(unsigned *))
			   + RECORD.var_hdr_extra;
	    if (copy->skip_bytes != 0 &&	/* XXX varlen all1M bug */
		rec_size - copy->skip_bytes > copy->copy_bytes)
		    die("chk_temp_write:short skip %d for %s", rec_size,
						print_copy_task(copy, copystr));
	    COPY.buf_position += total;
	    TEMP.items_left = items_left;
	    head->last_offset = (u4) (last_position - (byte *)head);

	    /* set copy task limit
	     */
	    copy->limit_rec = (byte **)COPY.curr_rec;

	    /* if this the last copy task for the merge pointer output array
	     */
	    if (COPY.curr_rec == COPY.limit_rec)
	    {
		copy->end_of_merge = ptr_part->end_of_merge;
		COPY.curr_rec = NULL;
		COPY.skip_bytes = 0;
		if (MERGE.merge_scanned == MERGE.tails_scanned)
		    MERGE.merge_scanned++;
		else
		{
		    MERGE.tails_scanned++;
		    copy->merge_part_done = TRUE; /* free merge part when
						   * copy done */
		}
	    }

	    /* if we already determined this copy task will completely fill
	     * the block, or if it is the last copy for the in_run merge.
	     */
	    if (copy->out_buf != NULL || copy->end_of_merge)
	    {
		copy->out_buf = (byte *)head;
		COPY.buf_position = NULL;
		COPY.bufs_planned++;
		if (copy->end_of_merge)
		    TEMP.run_planned = TRUE;
	    }

	    COPY.copy_avail++;  /* this allows a sort sproc to take this task */
	}
    }

    /* loop for writing blocks
     */
    for (;;)
    {
	/* Get the total number of blocks that had been filled when we
	 * finished filling blocks for the run we are currently scheduling
	 * for writing.  
	 */
	run_blk_limit = TEMP.run[IN.runs_write_sched].n_blks;
	if (run_blk_limit == 0)		/* if not done filling the run */
	    run_blk_limit = MAX_U4;

	/* if it's NOT the case that we have enough filled blocks to perform
	 * a write, then break out.  We have enough blocks if we have more
	 * than "advance" blocks filled but not yet issued for writing, or
	 * we are done filling blocks for this run and have not yet written
	 * them all out.
	 */
	if (!(TEMP.blks_filled - TEMP.writes_issued > TEMP.advance ||
	      (TEMP.blks_filled >= run_blk_limit &&
	       TEMP.blks_filled > TEMP.writes_issued)))
	{
	    break;
	}

	out_blk = &TEMP.out_blk[TEMP.writes_issued % TEMP.out_blk_cnt];
	tfile = TEMP.files + out_blk->tfile;

	/* if no aiocb available, punt.  XXX statistics for this?  */
	if (tfile->io_issued == tfile->io_done + TEMP.aio_cnt)
	    break;

	idle = FALSE;

	aiocb = tfile->aiocb + (tfile->io_issued % TEMP.aio_cnt);
	aiocb->aio_offset = out_blk->off;
	aiocb->aio_buf = out_blk->head;
	aiocb->aio_nbytes = TEMP.blk_size;

	/* determine number of blocks that were generated for the previous runs.
	 */
	prior_blks = IN.runs_write_sched > 0 ?
	    TEMP.run[IN.runs_write_sched - 1].n_blks : 0;

	/* determine number of writes issued for this run.
	 */
        run_writes_issued = TEMP.writes_issued - prior_blks;

	/* get the number of blocks filled for THIS run.  This is tricky
	 * because some of the filled blocks can be for the SUBSEQUENT run.
	 */
	run_blks_filled = (TEMP.blks_filled > run_blk_limit ?
			   run_blk_limit : TEMP.blks_filled) - prior_blks;

	if (out_blk->head->run != IN.runs_write_sched ||
	    out_blk->head->block != run_writes_issued)
	{
	    die("temp write sched failure: %d %d, %d %d\n",
		out_blk->head->run, IN.runs_write_sched,
		out_blk->head->block, run_writes_issued);
	}

	/* if this is the first non-advance block for the run, 
	 * mark own first ovc
	 */
	if ((!TEMP.optimal_reads && run_writes_issued == 0) ||
	    (TEMP.optimal_reads && run_writes_issued == 1))
	{
	    out_blk->head->ovc = sort->virgin_ovc;
	}

	/* if this is not the first, phony, advance info block for the run
	 * and it is not the last block for the run, set next block's first
	 * ovc.
	 */
	if (!(run_writes_issued == 0 && TEMP.optimal_reads) &&
	    TEMP.writes_issued + 1 != run_blk_limit)
	{
	    recode_args_t	ra;
	    blockheader_t	*next_head;
	    int			swap;
	    item_t		last_item, next_item;

	    next_head = TEMP.out_blk
		[(TEMP.writes_issued + 1) % TEMP.out_blk_cnt].head;

	    if (next_head->run != IN.runs_write_sched ||
		next_head->block != run_writes_issued + 1)
	    {
		die("temp write sched next failure: %d %d, %d %d\n",
		    next_head->run, IN.runs_write_sched,
		    next_head->block, run_writes_issued + 1);
	    }

	    ra.win_id = 0;
	    ra.curr_id = 1;
	    ra.summarize = FALSE;
	    if (sort->method == SortMethRecord)
	    {
		ra.win_ovc = sort->virgin_ovc;
		ra.curr_ovc = sort->virgin_ovc;
		swap = (*sort->recode)(sort, &ra, 
				       (item_t *) ((byte *)out_blk->head + 
						    out_blk->head->last_offset),
				       (item_t *) ((byte *)next_head + 
						    next_head->first_offset));
		next_head->ovc = ra.curr_ovc;
	    }
	    else
	    {
		last_item.ovc = sort->virgin_ovc;
		last_item.rec = (byte *)out_blk->head + 
		    out_blk->head->last_offset;
		next_item.ovc = sort->virgin_ovc;
		next_item.rec = (byte *)next_head + next_head->first_offset;

		if (RECORD.flags & RECORD_FIXED)
		{
		    swap = (*sort->recode)(sort, &ra, &last_item, &next_item);
		}
		else
		{
		    if (RECORD.flags & RECORD_STREAM)
		    {
			byte	*eorec;

			/* scan to find record lengths
			 */
			eorec = memchr(last_item.rec + RECORD.minlength - 1,
				       RECORD.delimiter, 
				       MAXRECORDSIZE);
			last_item.extra.var_extra.var_length = 1 + 
						(u2) (eorec - last_item.rec);
			eorec = memchr(next_item.rec + RECORD.minlength - 1,
				       RECORD.delimiter,
				       MAXRECORDSIZE);
			next_item.extra.var_extra.var_length = 1 +
					    	(u2) (eorec - next_item.rec);
		    }
		    else /* variable-length */
		    {
			/* record lengths are in first two bytes
			 */
			last_item.extra.var_extra.var_length = 
							    ulh(last_item.rec);
			next_item.extra.var_extra.var_length = 
							    ulh(next_item.rec);
		    }
		    swap = (*sort->recode)(sort, &ra, &last_item, &next_item);
		}
		next_head->ovc = next_item.ovc;
		next_head->u.extra = next_item.extra;
	    }
	    if (swap)
	    {
		die("inter-blk ovc recode swap, run %d, blk %d:ra %x win %x curr %x", 
		    IN.runs_write_sched, run_writes_issued, &ra,
			((byte *)out_blk->head + out_blk->head->last_offset),
			((byte *)next_head + next_head->first_offset));
	    }
	}
		
	/* if this is the first block of the run and we are planning on
	 * optimal reads during the output phase.
	 */
	if (run_writes_issued == 0 && TEMP.optimal_reads)
	{
	    /* get tfile index and offset for first block
	     */
	    adv_blk =
		&TEMP.out_blk[(TEMP.writes_issued + 1) % TEMP.out_blk_cnt];
	    out_blk->head->adv_tfile = adv_blk->tfile;
	    out_blk->head->adv_off = adv_blk->off;

	    if (adv_blk->head->run != IN.runs_write_sched ||
		adv_blk->head->block != 1)
	    {
		die("temp write sched 0-1 failure: %d %d, %d %d\n",
		    adv_blk->head->run, IN.runs_write_sched,
		    adv_blk->head->block, 1);
	    }

	    adv_info = (adv_info_t *)(out_blk->head + 1);
	    for (i = 2; i < min(TEMP.advance + 1, run_blks_filled); i++)
	    {
		u4	loff;

		/* copy last record of PREVIOUS block
		 */
		loff = adv_blk->head->last_offset;
		bcopy((byte *)adv_blk->head + loff,
		      adv_info->rec,
		      min(RECORD.max_intern_bytes, TEMP.blk_size - loff));

		adv_blk =				/* get adv block */
		    &TEMP.out_blk[(TEMP.writes_issued + i) % TEMP.out_blk_cnt];

		if (adv_blk->head->run != IN.runs_write_sched ||
		    adv_blk->head->block != i)
		{
		    die("temp write sched 0-i failure: %d %d, %d %d\n",
			adv_blk->head->run, IN.runs_write_sched,
			adv_blk->head->block, i);
		}

		/* get the tfile index and offset for block
		 */
		adv_info->tfile = adv_blk->tfile;
		adv_info->off = adv_blk->off;

		adv_info = (adv_info_t *)
		    ((byte *)adv_info + TEMP.adv_info_size);
	    }
	}
	/* 
	 * else if we need to record advance block info, do so.
	 */
	else if (run_blks_filled - run_writes_issued > TEMP.advance)
	{
	    /* get file number and offset of advance block.
	     * if we are NOT doing optimal reads, this is just one block ahead.
	     */
	    adv_blk = TEMP.out_blk + 
		((TEMP.writes_issued + TEMP.advance) % TEMP.out_blk_cnt);
	    out_blk->head->adv_tfile = adv_blk->tfile;
	    out_blk->head->adv_off = adv_blk->off;

	    if (TEMP.optimal_reads)
	    {
		/* copy last record of previous block.
		 * the key value of the last record determines when the
		 * block will be needed.
		 */
		adv_blk = TEMP.out_blk + 
		    ((TEMP.writes_issued + TEMP.advance - 1) % TEMP.out_blk_cnt);

		if (adv_blk->head->run != IN.runs_write_sched ||
		    adv_blk->head->block != run_writes_issued + TEMP.advance - 1)
		{
		    die("temp write sched next failure: %d %d, %d %d\n",
			adv_blk->head->run, IN.runs_write_sched,
			adv_blk->head->block, run_writes_issued + TEMP.advance - 1);
		}

		loff = adv_blk->head->last_offset;
		bcopy((byte *)adv_blk->head + loff,
		      (byte *)(out_blk->head + 1),
		      min(RECORD.max_intern_bytes, TEMP.blk_size - loff));
	    }
	}
	else
	{
	    out_blk->head->adv_tfile = -1;
	    out_blk->head->adv_off = -1;
	}
	    
	if (Print_task)
	    fprintf(stderr, 
		    "temp aio_write %d(%d-%d) %08x tfile %d, %d @ %lld at %s\n",
		    TEMP.writes_issued, IN.runs_write_sched,
		    run_writes_issued, aiocb->aio_buf, out_blk->tfile,
		    aiocb->aio_nbytes, (i8) aiocb->aio_offset, 
		    get_time_str(sort, tstr));

	/* if there isn't another write pending, start the busy meter.
	 */
	if (tfile->io_issued == tfile->io_done)
	{
	    time = get_time();
	    if (tfile->io_issued == 0)
		tfile->write_stats.start_first = time;
	    tfile->write_stats.busy -= time;
	}

	/* write out buffer
	 */
	if (aio_write(aiocb) != 0)
	    die("chk_temp_write, aio_write @ %lld failure: %s", 
		(i8) aiocb->aio_offset, strerror(errno));

	tfile->io_issued++;
	TEMP.writes_issued++;

	/* if there is at least one idle sproc
	 */
	if (TEMP.n_idling)
	{
	    /* Start accounting for a temp file write wait
	     */
	    TEMP.waiting = TEMP.files +
		(TEMP.out_blk + (TEMP.writes_done %
				 TEMP.out_blk_cnt))->tfile;
	    if (Print_task > 1)
		fprintf(stderr, "temp wait start at %s\n",
			get_time_str(sort, tstr));
	    TEMP.start_wait = get_time();
	}

	/* if this was the last write for the run
	 */
	if (TEMP.writes_issued == run_blk_limit)
	{
	    IN.runs_write_sched++;	/* we are done scheduling the run */

	    /* if this was the last run, declare the final number of writes!
	     */
	    if (IN.runs_write_sched * IN_RUN_PARTS == IN.part_final)
		TEMP.writes_final = TEMP.writes_issued;
	}
    }

    /* return TRUE if there was nothing interesting to do
     */
    return (idle);
}


void io_input_init(sort_t *sort)
{
    sort->input_phase = TRUE;

    /* if no non-zero input file, it's all over
     */
    if (next_input_file(sort) == FALSE)
    {
	IN.end_of_input = TRUE;
	IN.part_final = 0;
    }
}


void io_input(sort_t *sort)
{
    unsigned		idle;

    io_input_init(sort);
    
    while ((!sort->two_pass && !IN.end_of_input) ||
	   (sort->two_pass &&
	    !(TEMP.writes_done != 0 && TEMP.writes_done == TEMP.writes_final)))
    {
	idle = TRUE;

	if (!IN.end_of_input)
	    idle &= chk_input(sort);

	if (sort->two_pass)
	    idle &= chk_temp_write(sort);

	if (idle)
	    sginap(1);
    }
}

void build_adv_info(sort_t *sort, tbuf_t *tbuf)
{
    tmp_run_t		*run;
    unsigned		i;
    byte		*blk_adv_info;
    adv_info_t		*adv_info;
    unsigned		run_index;

    run_index = tbuf->run;
    run = TEMP.run + run_index;

    /* the initial block contains advance info for blocks 2 through "advance".
     * copy the advance info entries in the block into this run's advance info.
     */
    blk_adv_info = (byte *)(tbuf + 1) + sizeof(blockheader_t);
    for (i = 2; i < min(TEMP.advance + 1, run->n_blks); i++)
    {
	adv_info = (adv_info_t *)
	    (run->adv_info + TEMP.adv_info_size * (i % TEMP.advance));
	bcopy(blk_adv_info + TEMP.adv_info_size * (i - 2),
	      (byte *)adv_info,
	      TEMP.adv_info_size);
	adv_info->valid = TRUE;
    }

    /* initialize the adv info for block 1.  there is no record associated with
     * block 1 it must be read immediately to start the merge.
     */
    adv_info = (adv_info_t *)(run->adv_info + TEMP.adv_info_size);
    adv_info->off = ((blockheader_t *)(tbuf + 1))->adv_off;
    adv_info->tfile = ((blockheader_t *)(tbuf + 1))->adv_tfile;
    adv_info->valid = TRUE;

    /* go up the merge tree towards the root, insert an entry for this run
     * at first uninitialized node (ovc == EOL_OVC).
     */
    for (i = (MERGE.width + run_index) / 2; ; i /= 2) 
    {    
	if (TEMP.adv_tree[i].ovc == EOL_OVC)	/* if node is uninitialized */
	{
	    TEMP.adv_tree[i].id = run_index;
	    TEMP.adv_tree[i].ovc = NULL_OVC + run_index;
	    break;
	}
	ASSERT(i != 0);
    }

    /* recycle tbuf for block 0
     */
    tbuf->r_next = TEMP.free;
    TEMP.free = tbuf;
    run->old = run->new = NULL;
}

void start_tbuf_read(sort_t *sort, tbuf_t *tbuf, 
		     tempfile_t *tfile, tmp_run_t *run)
{
    /* the tbuf's offset and block have already been set by caller.
     * initialize rest of tbuf.
     */
    tbuf->r_next = NULL;	
    tbuf->run = (u2) (run - TEMP.run);
    tbuf->item = NULL;
    tbuf->norm_consumer = MAX_U4;
    tbuf->rem_consumers = 0;
    tbuf->skip_consumer = MAX_U4;
    tbuf->copy_done = FALSE;
    tbuf->block = run->blks_read++;
    tbuf->file = (u2) (tfile - TEMP.files);
    if (run->old != NULL)	/* if not first block for run */
    {
	run->new->r_next = tbuf;
	run->new = tbuf;
    }		
    else
	run->new = run->old = tbuf;
    
    /* link tbuf onto read queue for relevant temp file
     */
    tbuf->f_next = NULL;	
    if (tfile->head == NULL)
	tfile->head = tfile->tail = tbuf;
    else
    {
	tfile->tail->f_next = tbuf;
	tfile->tail = tbuf;
    }
}

void new_adv_winner(sort_t *sort, byte *prev_rec, byte *new_rec)
{
    recode_args_t	ra;
    int			swap;
    item_t		curr_item, win_item;
    ovc_t		win_ovc;
    unsigned		win_id;
    unsigned		i;
    tmp_run_t		*win_run, *curr_run;
    unsigned		temp;
    node_t		*node;

    ra.summarize = FALSE;
    
    win_id = TEMP.adv_tree[0].id;
    win_run = TEMP.run + win_id;

    /* calculate new ovc for last winner
     */
    if (win_run->blks_read == win_run->n_blks)
	win_ovc = EOL_OVC;
    else if (win_run->blks_read == 2) /* if 1st blk associated w/ rec */
	win_ovc = sort->virgin_ovc;
    else
    {
	ra.win_id = win_id;
	ra.curr_id = win_id;

	/* compute new block's ovc relative to previous winner's.
	 */
	if (sort->method == SortMethRecord)
	{
	    ra.win_ovc = sort->virgin_ovc;
	    ra.curr_ovc = sort->virgin_ovc;
	    swap = (*sort->recode)(sort, &ra, 
				   (item_t *)prev_rec, (item_t *)new_rec);
	    win_ovc = ra.curr_ovc;
	}
	else
	{
	    win_item.ovc = sort->virgin_ovc;
	    win_item.rec = prev_rec;
	    curr_item.ovc = sort->virgin_ovc;
	    curr_item.rec = new_rec;

	    if (RECORD.flags & RECORD_FIXED)
	    {
		swap = (*sort->recode)(sort, &ra, 
				       &win_item, &curr_item);
	    }
	    else
	    {
		if (RECORD.flags & RECORD_STREAM)
		{
		    byte	*eorec;

		    /* scan to find record lengths
		     */
		    eorec = memchr(win_item.rec + RECORD.minlength - 1,
				   RECORD.delimiter, 
				   MAXRECORDSIZE - 1);
		    win_item.extra.var_extra.var_length = 1 +
						(u2) (eorec - win_item.rec);
		    eorec = memchr(curr_item.rec + RECORD.minlength - 1,
				   RECORD.delimiter,
				   MAXRECORDSIZE - 1);
		    curr_item.extra.var_extra.var_length = 1 +
						(u2) (eorec - curr_item.rec);
		}
		else		/* variable-length */
		{
		    /* record lengths are in first two bytes
		     */
		    win_item.extra.var_extra.var_length = 
			ulh(win_item.rec);
		    curr_item.extra.var_extra.var_length = 
			ulh(curr_item.rec);
		}
		swap = (*sort->recode)(sort, &ra, 
				       &win_item, &curr_item);
	    }
	    win_run->eov = curr_item.extra.eov;
	    win_ovc = curr_item.ovc;
	}
	if (swap)
	{
	    die("chk_temp_read, inter-blk ovc recode swap, "
		"run %d, blk %d", IN.runs_write_sched, TEMP.writes_issued);
	}
    }
    
    /* go up tree to the root in order to find a new winner
     */
    for (i = (win_id + MERGE.width) / 2; i != 0; i /= 2)
    {
	node = TEMP.adv_tree + i;
	if (node->ovc > win_ovc)
	    continue;
	else if (node->ovc < win_ovc)
	{
	  do_swap:
	    temp = node->ovc;
	    node->ovc = win_ovc;
	    win_ovc = temp;
	    temp = node->id;
	    node->id = win_id;
	    win_id = temp;
	    continue;
	}
	else if (win_ovc == EOL_OVC)	/* both ovcs are EOL_OVC */
	    continue;

	/* the two ovc's are equal.  Call recode routine to determine
	 * the winner record and to assign a new ovc to the loser.
	 */
	ra.win_id = win_id;
	ra.curr_id = node->id;
	win_run = TEMP.run + win_id;
	curr_run = TEMP.run + node->id;
	win_item.rec = 
	    ((adv_info_t *)(win_run->adv_info + TEMP.adv_info_size * 
			    (win_run->blks_read % TEMP.advance)))->rec;
	curr_item.rec = 
	    ((adv_info_t *)(curr_run->adv_info + TEMP.adv_info_size * 
			    (curr_run->blks_read % TEMP.advance)))->rec;
	if (sort->method == SortMethRecord)
	{
	    ra.win_ovc = win_ovc;
	    ra.curr_ovc = node->ovc;
	    swap = (*sort->recode)(sort, &ra, 
				   (item_t *)win_item.rec, 
				   (item_t *)curr_item.rec);
	    win_ovc = ra.win_ovc;
	    node->ovc = ra.curr_ovc;
	}
	else
	{
	    win_item.ovc = win_ovc;
	    win_item.extra.eov = win_run->eov;
	    curr_item.ovc = node->ovc;
	    curr_item.extra.eov = curr_run->eov;

	    swap = (*sort->recode)(sort, &ra, &win_item, &curr_item);

	    win_ovc = win_item.ovc;
	    win_run->eov = win_item.extra.eov;
	    node->ovc = curr_item.ovc;
	    curr_run->eov = curr_item.extra.eov;
	}
	if (swap)
	    goto do_swap;
    }

    /* verify correct order
     */
    if (win_ovc != EOL_OVC && (TEMP.adv_tree[0].ovc & ~MAX_U2) != NULL_OVC)
    {
	byte 		*prev_rec, *win_rec;
	tmp_run_t	*prev_run;
	int		cmp;

	prev_run = TEMP.run + TEMP.adv_tree[0].id;
	win_run = TEMP.run + win_id;
	prev_rec = 
	    ((adv_info_t *)(prev_run->adv_info + TEMP.adv_info_size * 
			    ((prev_run->blks_read - 1) % TEMP.advance)))->rec;
	win_rec = 
	    ((adv_info_t *)(win_run->adv_info + TEMP.adv_info_size * 
			    (win_run->blks_read % TEMP.advance)))->rec;
	cmp = compare_data(sort, prev_rec, win_rec);
	if (cmp > 0 || (cmp == 0 && TEMP.adv_tree[0].id > win_id))
	    die("advance data misordered\n");
    }

    TEMP.adv_tree[0].ovc = win_ovc;
    TEMP.adv_tree[0].id = win_id;
}

/* chk_temp_read - check if any temporary file reads need to be started,
 *		or if any temp file reads have completed.
 */
int chk_temp_read(sort_t *sort)
{
    int			idle;		/* return value for this function */
    tempfile_t		*tfile;		/* temp file structure pointer */
    tbuf_t		*tbuf;
    int			i, j;
    int			err;
    int			bytes_read;
    unsigned		time;
    aiocb_t		*aiocb;
    char		tstr[40];

    idle = TRUE;	/* assume boredom until proved otherwise */

    /* while there are done item builds, increment done count to allow
     * more item build to be made available later.
     */
    while (TEMP.build_done < TEMP.build_avail &&
	   (tbuf = TEMP.build[TEMP.build_done % TEMP.build_cnt])->item != NULL)
    {
	idle = FALSE;

	/* if we are not doing optimal reads and this is the only tbuf for this
	 * run, put run on attn list as it is now eligible to have its
	 * next block read.
	 */
	if (!TEMP.optimal_reads && tbuf->r_next == NULL)
	    cause_run_attn(sort, tbuf->run);

	TEMP.build_done++;
    }

    if (TEMP.optimal_reads)
    {
	/* while there is a free tbuf to schedule and we are not either in
	 * the startup stage of the output phase (adv_tree[0].ovc == EOL_OVC)
	 * or have read all blocks of all runs (adv_tree[0].ovc == EOL_OVC),
	 * schedule the previous winner of the advance read tree for reading
	 * and pick a new block winner for next time.
	 */
	while (TEMP.free != NULL && TEMP.adv_tree[0].ovc != EOL_OVC)
	{
	    unsigned		win_id;
	    tmp_run_t		*win_run;
	    adv_info_t		*win_info, *prev_info;

	    win_id = TEMP.adv_tree[0].id;
	    win_run = TEMP.run + win_id;

	    /* if the block AFTER the winner block of the tournament exists
	     */
	    if (win_run->blks_read + 1 < win_run->n_blks)
	    {
		/* if the advance info for the next block of the previous
		 * winner run is not yet present, forget about doing an advance
		 * write for now.
		 */
		win_info = (adv_info_t *)
		    (win_run->adv_info + TEMP.adv_info_size * 
		     ((win_run->blks_read + 1) % TEMP.advance));
		if (win_info->valid == FALSE)
		    break;
	    }

	    idle = FALSE;

	    /* get advance info for previous winner block
	     */
	    prev_info = (adv_info_t *)
		(win_run->adv_info + TEMP.adv_info_size * 
		 (win_run->blks_read % TEMP.advance));

	    /* mark advance info block for tbuf we are about to read as
	     * invalid.  The block we read will contain advance info for
	     * the block TEMP.advance blocks ahead of it.  Thus the advance
	     * info for that block will also reside in the same advance
	     * info entry. 
	     */
	    prev_info->valid = FALSE;

	    /* allocate and initialize tbuf for previous winner 
	     */
	    tbuf = TEMP.free;
	    TEMP.free = tbuf->r_next;
	    tbuf->offset = prev_info->off;

	    if (Print_task)
		fprintf(stderr, "advance read run %d, blk %d, ovc: %08x\n",
			win_id, win_run->blks_read, TEMP.adv_tree[0].ovc);

	    /* schedule read of tbuf for previous winner.
	     *
	     * The actual read will not happen until after the next winner
	     * is chosen.  Therefore the next block to read is always
	     * chosen by calculating the first block to be exhausted of the
	     * last-scheduled-to-be-read blocks for each run.  At least one
	     * of these blocks (the one we are scheduling for a read here),
	     * will not have completed its read at the time the next winner
	     * is chosen.
	     *
	     * note that start_tbuf_read() increments win_run->blks_read
	     */
	    start_tbuf_read(sort, tbuf, TEMP.files + prev_info->tfile, win_run);

	    new_adv_winner(sort, prev_info->rec, win_info->rec);
	}
    }

    for (i = 0; i < TEMP.n_files; i++)
    {
	tfile = &TEMP.files[i];

	/* while there are outstanding aio_read()'s, and we can make an item
	 * build available.
	 */
	while (tfile->io_done < tfile->io_issued &&
	       TEMP.build_avail < TEMP.build_done + TEMP.build_cnt)
	{
	    /* if the first issued aio_read() has not yet completed, break out.
	     */
	    aiocb = &tfile->aiocb[tfile->io_done % TEMP.aio_cnt];
	    if ((err = aio_error(aiocb)) == EINPROGRESS)
		break;
	    
	    idle = FALSE;

	    if (err != 0)
		die("chk_temp_read: read #%d failed, file %d, %d @ %lld: %s",
		    tfile->io_done, i, aiocb->aio_nbytes,
		    (i8) aiocb->aio_offset, strerror(err));
	    if ((bytes_read = (int) aio_return(aiocb)) < 0)
	    {
		die("aio_return read error: %s on temp read %d, file %d", 
		    strerror(aio_error(aiocb)), tfile->io_done, i);
	    }
	    tbuf = tfile->head;
	    if (Print_task)
		fprintf(stderr,
			"temp read %d, tfile %d, %d @ %lld run %d blk %d done at %s\n",
			tfile->io_done, i, bytes_read, (i8) aiocb->aio_offset,
			tbuf->run, tbuf->block,
			get_time_str(sort, tstr));

	    /* Note that the read has completed so that next_tbuf() can tell
	     * that it is out of a 'waiting for temp read' wait period.
	     */
	    tbuf->read_done = TRUE;

	    /* remove tbuf from tfile queue
	     */
	    tfile->head = tbuf->f_next;
	    if (tfile->head == NULL)
		tfile->tail = NULL;

	    if (TEMP.optimal_reads && tbuf->block == 0)
		build_adv_info(sort, tbuf);
	    else
	    {
		if (TEMP.optimal_reads)
		{
		    blockheader_t	*head;
		    adv_info_t		*adv_info;

		    /* Copy the advance info for the block TEMP.advance blocks
		     * ahead into its advance info slot.  Mark the info as
		     * valid so that it may be used to schedule new reads in
		     * the optimal order.
		     */
		    head = (blockheader_t *)(tbuf + 1);
		    adv_info = (adv_info_t *)
			(TEMP.run[tbuf->run].adv_info + 
			 TEMP.adv_info_size * (head->block % TEMP.advance));
		    adv_info->off = head->adv_off;
		    adv_info->tfile = head->adv_tfile;
		    bcopy((byte *)(head + 1), adv_info->rec, 
			  RECORD.max_intern_bytes);
		    adv_info->valid = TRUE;
		}

		/* place tbuf on build list so that an item list can be built
		 * for it.
		 */
		TEMP.build[TEMP.build_avail % TEMP.build_cnt] = tbuf;
		TEMP.build_avail++;
	    }

	    tfile->io_done++;

	    /* If there are no more reads active then end a busy period
	     */
	    if (tfile->io_done == tfile->io_issued)
	    {
		time = get_time();

		tfile->fileinfo.stats.busy += time;
		tfile->fileinfo.stats.finish_last = time;
	    }
	}
	
	/* if we don't already have the maximum number of aio_read()'s in 
	 * progress.
	 */
	if (tfile->io_issued < tfile->io_done + TEMP.aio_cnt)
	{
	    tbuf = tfile->head;

	    /* skip over those tbuf's that have already had reads issued.
	     */
	    for (j = 0; j < (tfile->io_issued - tfile->io_done); j++)
		if ((tbuf = tbuf->f_next) == NULL)
		    break;

	    while (tbuf != NULL && 
		   tfile->io_issued < tfile->io_done + TEMP.aio_cnt)
	    {
		idle = FALSE;
		
		aiocb = &tfile->aiocb[tfile->io_issued % TEMP.aio_cnt];
		
		/* schedule a read of the buffer
		 */
		aiocb->aio_offset = tbuf->offset;
		aiocb->aio_buf = (char *)(tbuf + 1);	/* block follows tbuf */
		aiocb->aio_nbytes = TEMP.blk_size;
		if (Print_task)
		    fprintf(stderr, 
			    "temp read %d, tfile %d, run %d, blk %d: %08x, %lld, %d at %s\n", 
			    tfile->io_issued, i, tbuf->run, tbuf->block,
			    aiocb->aio_buf, (i8) aiocb->aio_offset, 
			    aiocb->aio_nbytes, get_time_str(sort, tstr));

		/* if there isn't another read pending,
		 * start the busy meter running.
		 */
		if (tfile->io_issued == tfile->io_done)
		{
		    time = get_time();
		    if (tfile->io_issued == 0)
			tfile->fileinfo.stats.start_first = time;
		    tfile->fileinfo.stats.busy -= time;
		}
		
		if (aio_read(aiocb) < 0)
		    die("aio_read failure for tfile %d, %lldx: %s", 
			i, (i8) aiocb->aio_offset, strerror(errno));
		
		tfile->io_issued++;

		tbuf = tbuf->f_next;
	    }
	}
    }

    return (idle);
}


/* chk_ptr_output - check if there is any io sproc activity necessary for
 *		writing pointer sort output, and perform it if necessary.
 *
 * returns: TRUE if there was nothing to do.
 */
int chk_ptr_output(sort_t *sort)
{
    int			idle;		/* return value for this function */
    aiocb_t		*aiocb;
    copy_task_t		*copy;
    byte		*out_buf;
    unsigned		write_size;
    ptr_out_buf_t	*ptr_part;
    unsigned		target;
    unsigned		total;
    byte		**curr;
    byte		**limit;
    unsigned		rec_size;
    byte		*rec;
    unsigned		time;
    char		tstr[40];
    char		copystr[80];

    idle = TRUE;	/* assume boredom until proved otherwise */

    /* while there are outstanding aio_write()'s process them
     * An api_invoked pointer sort always has writes_done == writes_issued
     * because it doesn't do output 'writes', rather nsort_return_ptrs()
     * 'consumes' the copy tasks.
     */
    while (OUT.writes_done < OUT.writes_issued)
    {
	/* if the first issued aio_write has not yet completed, break out.
	 */
	aiocb = &OUT.aiocb[OUT.writes_done % OUT.aio_cnt];
	if (aiocb->aio_nbytes && (sort->get_error)(aiocb) == EINPROGRESS)
	    break;

	idle = FALSE;

	if (aiocb->aio_nbytes && aio_return(aiocb) != aiocb->aio_nbytes)
	{
	    /* Exit gracefully on broken pipe and disk full errors
	     */
	    int err = (sort->get_error)(aiocb);

	    if (err == EPIPE || err == ENOSPC)
	    {
		fprintf(stderr, "nsort: %s\n", strerror(err));
		ordinal_exit();
	    }
	    die("aio_return write error: %s", strerror(err));
	}

	if (Print_task)
	    fprintf(stderr, "aio_write %d done at %s\n", 
		   OUT.writes_done, get_time_str(sort, tstr));

	OUT.writes_done++;	/* free up output buffer for reuse */
	/* 
	 * If there are no other writes outstanding, 
	 * then stop running the output busy meter.
	 */
	if (OUT.writes_done == OUT.writes_issued)
	    OUT.file->stats.busy += get_time();

	/* if we just finished the last direct write
	 */
	if (OUT.writes_final == OUT.writes_done)
	{
	    /* if there is a non-direct, remainder write to be done, do it.
	     */
	    OUT.file->stats.ios = OUT.writes_final;
	    if (OUT.nondirect_size)
	    {
		/* either seek and perform a non-FDIRECT write or
		 * fill up the last nsort_return_recs() buffer
		 */
		(*sort->final_write)(sort, OUT.nondirect_begin,
					   OUT.nondirect_size);
	    }

	    if (OUT.file->sync)
	    {
		OUT.file->stats.busy -= get_time();
		fsync(OUT.file->fd);
		OUT.file->stats.busy += get_time();
	    }

	    OUT.file->stats.bytes = OUT.write_offset + OUT.nondirect_size;
	    OUT.file->stats.finish_last = get_time();

	    OUT.end_io_output = TRUE;
	}
    }

    /* while there are completed copy tasks...
     */
    while (COPY.copy_done < COPY.copy_avail &&
	   (copy = &COPY.copy[COPY.copy_done % COPY.copy_cnt])->copy_done)
    {
	/* if the copy task was the last one for an output buffer,
	 * write the output buffer.
	 */
	if ((out_buf = copy->out_buf) != NULL)
	{
	    /* if we don't have an aiocb structure available to perform the
	     * aio_write(), break out and try again later. 
	     */
	    if (OUT.writes_done + OUT.aio_cnt == OUT.writes_issued)
	    {
		if (Print_task)
		    fprintf(stderr,
			   "chk_ptr_output: copy %d done, no outaio avail@%s\n",
			   COPY.copy_done, get_time_str(sort, tstr));
		break;
	    }

	    /* if this was the last copy task of the sort
	     */
	    if (copy->end_of_merge)
	    {
		total = (unsigned)(copy->dest_buf - out_buf + copy->copy_bytes);
		write_size = (total / OUT.dio_io) * OUT.dio_io;
		OUT.nondirect_size = total - write_size;
		OUT.nondirect_begin = out_buf + write_size;
		OUT.end_sort_output = TRUE;   /* make sorting sprocs go away */
		/*
		 * An api pointer sort has completed its output in the last copy
		 * so tell everyone (e.g. the i/o sproc) the good news.
		 */
		if (sort->api_invoked)
		    OUT.end_io_output = TRUE;
	    }
	    else
		write_size = OUT.buf_size;

	    if (sort->api_invoked)
	    {
		/* The client has called nsort_return_ptrs() and 'seen' all
		 * the records contained in the copy buf; not much to do here,
		 * the return_ptrs() call replaces the write to the output.
		 * Bump writes_done so that COPY.bufs_planned is unstuck.
		 */
		OUT.writes_done++;
	    }
	    else
	    {
		if (out_buf != OUT.buf[OUT.writes_issued % OUT.buf_cnt])
		    die("chk_ptr_output, out_buf inconsistency: %08x %08x",
			    out_buf, OUT.buf[OUT.writes_issued % OUT.buf_cnt]);
		
		aiocb = &OUT.aiocb[OUT.writes_issued % OUT.aio_cnt];
		/* if there isn't another write pending, start busy meter
		 */
		if (OUT.writes_issued == OUT.writes_done)
		{
		    time = get_time();
		    if (OUT.writes_issued == 0)
			OUT.file->stats.start_first = time;
		    OUT.file->stats.busy -= time;
		}

		/* write out buffer
		 */
		aiocb->aio_buf = out_buf;
		aiocb->aio_nbytes = write_size;
		aiocb->aio_offset = OUT.write_offset;
		if (Print_task)
		    fprintf(stderr, "aio_write %d, %08x, %d @ %lld at %s\n", 
			    OUT.writes_issued, aiocb->aio_buf, aiocb->aio_nbytes, 
			    (i8) aiocb->aio_offset, get_time_str(sort, tstr));
		if ((sort->write_output)(aiocb) != 0)
		    die("aio_write @ %lld failure: %s", (i8) aiocb->aio_offset,
						   strerror(errno));
		OUT.write_offset += write_size;
	    }
	    OUT.writes_issued++;
	}
	
	idle = FALSE;

	/* if there are tbufs whose copying from was completed by this copy,
	 * mark them as copy done.
	 */
	if (copy->tbuf != NULL)
	{
	    tbuf_t	*tbuf, *next_tbuf;
	    int		run_index;
	    
	    tbuf = copy->tbuf;
	    while (tbuf != NULL)
	    {
		ASSERT(tbuf->copy_done == FALSE);
		if (Print_task)
		    fprintf(stderr, "copy %s done tbuf %08x\n",
				    print_copy_task(copy, copystr),
				    tbuf);
		next_tbuf = tbuf->f_next;
		run_index = tbuf->run;
		tbuf->copy_done = TRUE;
		if (tbuf_recyclable(sort, tbuf))
		{
		    if (Print_task)
			fprintf(stderr, 
				"chk_ptr_output: recycling tbuf %08x/%d/%d f_next %x\n",
				tbuf, tbuf->run, tbuf->block, tbuf->f_next);
		    cause_run_attn(sort, run_index);
		}
		else if (Print_task)
		    fprintf(stderr, 
			    "chk_ptr_output: !recycling tbuf %08x/%d/%d f_next %x\n",
			    tbuf, tbuf->run, tbuf->block, tbuf->f_next);
		tbuf = next_tbuf;
	    }
	}

	/* if the copy task was the last one for a pointer array,
	 * make the one before it available for a merge.
	 * (The reason the current one is not made available is that its
         * tail record info may still be needed by the next merge, and
	 * we wouldn't want to a new merge to overwrite the tail rec info.)
	 */
	if (copy->merge_part_done)
	    MERGE.merge_avail++;

	COPY.copy_done++;
    }

    /* while there are unused copy task structures fill them up with the
     * info describing a set of pointers to copy to the file output buffers
     */
    while (COPY.copy_avail < COPY.copy_done + COPY.copy_cnt)
    {
	/* if we have already seen a merge partition that was the end of
	 * the merge, ignore any subsequent (and empty) merge partitions.
	 */
	if (OUT.writes_final)
	    break;

	/* get ptr_part for the merge partition we are scanning.  this is
	 * always indexed by MERGE.tails_scanned because if MERGE.merge_scanned
	 * != MERGE.tails_scanned, we are scanning the tail portion of the
	 * array indexed by MERGE.tails_scanned.  if MERGE.merge_scanned ==
	 * MERGE.tails_scanned, then we are scanning the main portion of the
	 * array indexed by MERGE.merge_scanned, but MERGE.tails_scanned will
	 * do just as well. 
	 */
	ptr_part = MERGE.ptr_part + (MERGE.tails_scanned % MERGE.merge_cnt);

	/* if (!(haven't completely scanned a finished main array portion) &&
	 *     !(haven't completely scanned a finished tail array portion) &&
	 *     !(there's a partially finished main array w/ new results))
	 * {
	 *     then break out of the loop;
	 * }
	 *
	 * the last condition of the if() is subdivided as follows:
	 *     !(the next scan is for a main portion &&
	 *       (this scan is for the first merge partition ?
	 *        {begin,limit}_ptr have been set : 
	 *          			{begin,limit}_ptr have been set) &&
	 *       (we have a current position in the array ?
	 *		the current postion : the beginning position) <
	 *	  the tail position which may increase until partition is done))
	 */
	if (!(MERGE.merge_scanned < MERGE.merge_done) &&
	    !(MERGE.tails_scanned < MERGE.tails_done) &&
	    !(MERGE.merge_scanned == MERGE.tails_scanned &&
	      (MERGE.merge_scanned == 0 ? 
	       MERGE.skip_done > 0 : MERGE.merge_scanned <= MERGE.tails_done) &&
	      (COPY.curr_rec != NULL ? 
	       (volatile byte **)COPY.curr_rec : ptr_part->begin_ptr) <
	      						ptr_part->tail_ptr))
	{
	    break;
	}

	/* If there is no output buffer available, break out.
	 * Begin an output wait period if it looks like this is the bottleneck.
	 */
	if (COPY.bufs_planned == OUT.writes_done + OUT.buf_cnt)
	{
	    if (MERGE.merge_taken == MERGE.merge_avail &&
		COPY.copy_taken == COPY.copy_avail &&
		TEMP.build_taken == TEMP.build_avail &&
		OUT.waiting == FALSE)
	    {
		/* Starting a 'wait for output write' period during a ptr sort
		 */
		OUT.waiting = TRUE;
		OUT.file->stats.wait -= get_time();
	    }
	    break;
	}

	if (OUT.waiting)
	{
	    OUT.file->stats.wait += get_time();
	    OUT.waiting = FALSE;
	}

	idle = FALSE;

	copy = COPY.copy + (COPY.copy_avail % COPY.copy_cnt);
#if defined(DEBUG2)
	copy->merge = MERGE;
	copy->copy = COPY;
	copy->pp = *ptr_part;
#endif

	/* set the limit for scanning the pointer array.  this is done
         * everytime we start a merge scan since we may be polling for new
         * results from this merge partition.  
	 */
	if (MERGE.merge_scanned == MERGE.tails_scanned)
	{
	    /* if end of merge, output the tail record too since there is no
	     * subsequent merge to increment MERGE.tails_done 
	     */
	    if (ptr_part->end_of_merge)
	    {
		/* if there are no pointers for the merge partition, then
		 * skip over the entire merge partition without generating
		 * any copy tasks.  this case can happen if only one record
		 * pointer is output by the merge partition, but it is
		 * deleted relative to the tail record of the previous merge.
		 */
		if (ptr_part->begin_ptr == ptr_part->limit_ptr)
		{
		    MERGE.merge_scanned++;	/* done with merge partition */
		    break;
		}

		/* since this is the last merge partition, include the tail
		 * ptr as we don't need to reserve it for deletion checking
		 * relative to the first record of the next partition.  
		 */
		limit = (byte **)ptr_part->limit_ptr;
	    }
	    else
		limit = (byte **)ptr_part->tail_ptr;
	}
	else
	    limit = (byte **)ptr_part->limit_ptr;

	/* if this if the first time we are scanning either part of this
	 * merge partition.
	 */
	if (COPY.curr_rec == NULL)
	{
	    /* if we haven't scanned the main (pre-tail) portion of the 
	     * merge partition.
	     */
	    if (MERGE.merge_scanned == MERGE.tails_scanned)
		COPY.curr_rec = (byte **)ptr_part->begin_ptr;
	    else
		COPY.curr_rec = (byte **)ptr_part->tail_ptr;
	    COPY.skip_bytes = 0;
	}

	/* if this is first copy for this output buffer
	 */
	out_buf = OUT.buf[COPY.bufs_planned % OUT.buf_cnt];
	if (COPY.buf_position == NULL)
	    COPY.buf_position = out_buf;

	/* fprintf(stderr, "begin: %08x, limit: %08x, skip:%d\n", 
		COPY.curr_rec, limit, COPY.skip_bytes); */

	copy->dest_buf = COPY.buf_position;
	ASSERT(COPY.skip_bytes < MAXRECORDSIZE);
	copy->skip_bytes = COPY.skip_bytes;
	copy->src_rec = COPY.curr_rec;
	copy->out_buf = NULL;
	copy->merge_part_done = FALSE;
	copy->copy_done = FALSE;
	copy->end_of_merge = FALSE;
	copy->tbuf = NULL;		/* tbufs completed by this copy */

	/* set the target copy size to the minimum of the number of bytes
	 * left in the output buffer, and the target copy size.
	 */
	target = (unsigned) (OUT.buf_size - (COPY.buf_position - out_buf));
	if (target > COPY.target_size)
	    target = COPY.target_size;

	/* if the records are fixed length
	 */
	if (RECORD.flags & RECORD_FIXED)
	{
	    rec_size = sort->edited_size;
	    if (sort->two_pass)
	    {
		/* two pass sort.  there may be tbuf pointers (indicated by the
		 * high-order bit of the pointer being set!) amongst the record
		 * pointers, even at curr_rec!
		 * hence we have to scan the pointers ignoring tbufs to find the
		 * correct copy end point.
		 */
		curr = COPY.curr_rec;
#if defined(DEBUG1)
		if (curr > limit)
		    die("chk_ptr_output: nothing here: %x (has %x), limit %x",
			curr, *curr, limit);
#endif
		while (curr < limit && (ptrdiff_t) (rec = *curr) < 0)
		{
		    curr++;
		}
		total = rec_size - COPY.skip_bytes;
		if (curr == limit)
		{
		    /* Nobody here but us tbufs - an empty copy.
		     * Skip_bytes had better be zero, because for skip_bytes
		     * to be non-zero implies that the first ptr is for a rec.
		     */
		    if (Print_task)
			fprintf(stderr, "copy %d finds no records %x->%x\n",
					COPY.copy_avail, COPY.curr_rec, curr);
		    ASSERT(COPY.skip_bytes == 0);
		    total = 0;
		    rec_size = 0;
		}
		/* if the first record (or the remaining part of it) won't fit
		 * in the remaining part of the outbuf
		 * target size 
		 */
		else if (target < total)
		{
		    ASSERT(target < MAXRECORDSIZE);
		    if (Print_task > 1)
			fprintf(stderr, "small copy %x for %x\n", copy, rec);
		    COPY.skip_bytes = target;
		    total = target;
		}
		/*
		 * else the current record (and probably many more) will be
		 * included in the copy.
		 */
		else
		{
		    while (++curr < limit)
		    {
			/* if the rec pointer is not a tbuf pointer
			 */
			rec = *curr;
			if ((ptrdiff_t)rec >= 0)
			{
			    if (total + rec_size > target)
			    {
				int till_eobuf = (int) (out_buf + OUT.buf_size -
						(COPY.buf_position + total));
				int last_size = min(till_eobuf, rec_size);

				/* The current record will not entirely fit in
				 * the copy.  Figure out how many bytes will be
				 * copied, and skip that many bytes in the
				 * record on the next copy. 
				 */
				total += last_size;
				if (last_size < rec_size)
				    COPY.skip_bytes = last_size;
				else
				{
				    /* There is enough space before the real end
				     * of the i/o buffer, so take the whole rec
				     * (and its tbuf, if there is one).
				     */
				    COPY.skip_bytes = 0;
				    curr++;
				}

				ASSERT(COPY.skip_bytes < MAXRECORDSIZE);
				break;
			    }
			    total += rec_size;
			}
#if defined(DEBUG1)
			else if (Print_task/* > 1*/)
			    fprintf(stderr, "tbuf %x run %d, blk %d in copy %s\n",
					    ~(ptrdiff_t) rec,
					    ((tbuf_t *) ~(ptrdiff_t) rec)->run,
					    ((tbuf_t *) ~(ptrdiff_t) rec)->block,
					    print_copy_task(copy, copystr));
#endif
		    }
		}
		COPY.curr_rec = curr;
	    }
	    else
	    {
                /* One pass sort.
		 *
		 * calculate the number of bytes left in the current merge 
		 * partition. 
		 */
		total = (unsigned) (limit - COPY.curr_rec) * rec_size -
				   COPY.skip_bytes;
		
		/* if the total number of record bytes left in the merge
		 * partition is less than the target number of bytes, just copy
		 * the total.
		 */
		if (total < target)
		{
		    COPY.curr_rec = limit;
		}
		else
		{
		    /* since the records are fixed length and there are no tbuf
		     * pointers amongst the record pointers, can calculate the
		     * ending points (record pointer, skip bytes).
		     */
		    COPY.curr_rec += (target + COPY.skip_bytes) / rec_size;
		    COPY.skip_bytes = (target + COPY.skip_bytes) % rec_size;
		    total = target;
		}
	    }
	}
	/*
	 * else the records are variable-length (variable or stream)
	 */
	else
	{
	    int var_hdr_extra = RECORD.var_hdr_extra;

	    curr = COPY.curr_rec;
	    while (curr < limit && (ptrdiff_t) (rec = ulp(curr)) < 0)
	    {
		curr = (byte **)
		    ((byte *) curr + sizeof(unsigned *) + sizeof(u2));
	    }
	    rec_size = *(u2 *) ((byte *)curr + sizeof(byte *)) + var_hdr_extra;
	    total = rec_size - COPY.skip_bytes;
	    if (curr == limit)
	    {
		/* Nobody here but us tbufs - an empty copy.
		 * Skip_bytes had better be zero, because for skip_bytes
		 * to be non-zero implies that the first ptr is for a rec.
		 */
		if (Print_task)
		    fprintf(stderr, "copy %d finds only tbufs\n",
				    COPY.copy_avail);
		ASSERT(COPY.skip_bytes == 0/* && sort->two_pass*/);
		total = 0;
		rec_size = 0;
	    }
	    /* if the first record (or the remaining part of it) won't fit
	     * in the remaining part of the outbuf
	     * target size 
	     */
	    else if (target < total)
	    {
		if (Print_task > 1)
		    fprintf(stderr, "small copy of %d @ %x for %x %d skip %d\n",
				target, copy, rec, rec_size, COPY.skip_bytes);
		ASSERT(target < MAXRECORDSIZE);
		COPY.skip_bytes = target;
		total = target;
	    }
	    /*
	     * else the current record (and probably many more) will be
	     * included in the copy.
	     */
	    else
	    {
		curr = (byte **)
		    ((byte *)curr + sizeof(unsigned *) + sizeof(u2));
		while (curr < limit)
		{
		    /* if the rec pointer is not a tbuf pointer
		     */
		    rec = ulp(curr);
		    if ((ptrdiff_t)rec >= 0)
		    {
			rec_size = *(u2 *)((byte *)curr + sizeof(unsigned *)) +
			    var_hdr_extra;
			if (total + rec_size > target)
			{
			    int till_eobuf = (int) (out_buf + OUT.buf_size -
						(COPY.buf_position + total));
			    int last_size = min(till_eobuf, rec_size);

			    /* The current record will not entirely fit in
			     * the copy.  Figure out how many bytes will be
			     * copied, and skip that many bytes in the
			     * record on the next copy. 
			     */
			    total += last_size;
			    if (last_size < rec_size)
				COPY.skip_bytes = last_size;
			    else
			    {
				/* There is enough space before the real end
				 * of the i/o buffer, so take the whole rec
				 * (and its tbuf, if there is one).
				 */
				COPY.skip_bytes = 0;
				curr = (byte **)
				    ((byte *)curr + sizeof(unsigned *) + sizeof(u2));
			    }

			    ASSERT(COPY.skip_bytes < MAXRECORDSIZE);
			    break;
			}
#if 0
			{
			    /* The current record will not entirely fit in the
			     * copy.  Figure out how many bytes will be copied,
			     * and skip that many bytes in the record on the
			     * next copy.
			     */
			    COPY.skip_bytes = target - total;
			    ASSERT(COPY.skip_bytes < MAXRECORDSIZE);
			    total = target;
			    break;
			}
#endif
			total += rec_size;
		    }
#if defined(DEBUG1)
		    else if (Print_task/* > 1*/)
			fprintf(stderr, "tbuf %x run %d, blk %d in copy %s\n",
					~(ptrdiff_t) rec,
					((tbuf_t *) ~(ptrdiff_t) rec)->run,
					((tbuf_t *) ~(ptrdiff_t) rec)->block,
					print_copy_task(copy, copystr));
#endif
		    curr = (byte **)
			((byte *)curr + sizeof(unsigned *) + sizeof(u2));
		}
	    }
	    COPY.curr_rec = curr;
	}
#if defined(DEBUG2)
	if (COPY.curr_rec < limit && (ptrdiff_t) ulp(COPY.curr_rec) <= 0)
	    fprintf(stderr, "chk_ptr_output: copy %d will probably start "
	    		    "with a tbuf of %x at %x\n",
			    COPY.copy_avail + 1,
			    ~ulp(COPY.curr_rec), COPY.curr_rec);
#endif
#if defined(DEBUG1)
	if (total == 0 && Print_task)
	    fprintf(stderr, "copy %d is empty\n", COPY.copy_avail);
#endif
	copy->copy_bytes = total;
	COPY.buf_position += total;
	copy->write_position = COPY.write_position;
	COPY.write_position += total;

	/* fprintf(stderr, "end:   %08x, limit: %08x, skip:%d, total: %d\n", 
		COPY.curr_rec, limit, COPY.skip_bytes, total); */

	/* if we reached the end of the ptr array, or there are no bytes to
	 * skip in the current record, the limit for the copy is the current
	 * record pointer, otherwise we ended in the middle of a record and
	 * we need to add another entry to get the correct copy limit.
	 */
	if (COPY.curr_rec == limit || COPY.skip_bytes == 0)
	    copy->limit_rec = (byte **)COPY.curr_rec;
	else if (RECORD.flags & RECORD_FIXED)
	    copy->limit_rec = (byte **)
		((byte *)COPY.curr_rec + sizeof(byte *));
	else
	    copy->limit_rec = (byte **)
		((byte *)COPY.curr_rec + sizeof(byte *) + sizeof(u2));
		
	if (!(RECORD.flags & RECORD_FIXED))
	    rec_size = *(u2 *) ((byte *)copy->src_rec + sizeof(unsigned *)) +
		       RECORD.var_hdr_extra;
	if (copy->skip_bytes != 0 &&
	    rec_size - copy->skip_bytes > copy->copy_bytes)
		die("chk_ptr_output:short skip %d for %s", rec_size,
						print_copy_task(copy, copystr));

	/* if we are scanning the main (non-tail) portion of the ptr array.
	 */
	if (MERGE.merge_scanned == MERGE.tails_scanned)
	{
	    /* if we reached the limit, reset skip bytes
	     */
	    if (COPY.curr_rec == limit)
		COPY.skip_bytes = 0;

	    /* if the merge partition in question has completed
	     */
	    if (MERGE.merge_scanned < MERGE.merge_done)
	    {
		/* recalculate the limit since it may have recently changed.
		 */
		limit = (byte **)(ptr_part->end_of_merge ?
				  ptr_part->limit_ptr : ptr_part->tail_ptr);

		/* if we have reached the limit
		 */
		if (COPY.curr_rec == limit)
		{
		    copy->end_of_merge = ptr_part->end_of_merge;
		    COPY.curr_rec = NULL;
		    COPY.skip_bytes = 0;
		    MERGE.merge_scanned++;
		}
	    }
	}
	else
	{
	    /* check if the scan of the tail portion is done.
	     */
	    if (COPY.curr_rec == limit)
	    {
		copy->merge_part_done = TRUE; /* free merge part when
					       * copy done */
		COPY.curr_rec = NULL;
		COPY.skip_bytes = 0;
		MERGE.tails_scanned++;
	    }
	}

	/* if this the last copy task for the output buffer
	 */
	if (COPY.buf_position == out_buf + OUT.buf_size || 
	    copy->end_of_merge)
	{
	    copy->out_buf = out_buf;
	    COPY.buf_position = NULL;
	    COPY.bufs_planned++;
	    if (copy->end_of_merge)
		OUT.writes_final = COPY.bufs_planned;
	}

	COPY.copy_avail++;   /* this allows a sort sproc to take this task */
    }

    if (Print_task && COPY.copy_avail == COPY.copy_done + COPY.copy_cnt)
	fprintf(stderr, "chk_ptr_output: all copies slots busy");
	

    /* return TRUE if there was nothing interesting to do
     */
    return (idle);
}


/* chk_rec_output - check if there is any io sproc activity necessary for
 *		writing record sort output, and perform it if necessary.
 *
 *	returns: TRUE if there was nothing to do.
 */
int chk_rec_output(sort_t *sort)
{
    int			idle;		/* return value for this function */
    aiocb_t		*aiocb;
    rec_out_buf_t	*rob;
    unsigned		size;
    unsigned		time;
    char		tstr[40];

#define	n_writes	n_temp_blocks

    idle = TRUE;	/* assume boredom until proved otherwise */

    /* while there are outstanding aio_write()'s
     */
    while (OUT.writes_done < OUT.writes_issued)
    {
	/* if the first issued aio_write has not yet completed, break out.
	 */
	aiocb = &OUT.aiocb[OUT.writes_done % OUT.aio_cnt];
	if (aiocb->aio_nbytes && (sort->get_error)(aiocb) == EINPROGRESS)
	    break;

	idle = FALSE;

	if (aiocb->aio_nbytes && (sort->get_return)(aiocb) != aiocb->aio_nbytes)
	{
	    /* Exit gracefully on broken pipe and disk full errors
	     */
	    int err = (sort->get_error)(aiocb);

	    /* XXX what to do in the api if output files are permitted?
	     */
	    if (err == EPIPE || err == ENOSPC)
	    {
		fprintf(stderr, "nsort: %s\n", strerror(err));
		ordinal_exit();
	    }
	    die("aio_return write error: %s", strerror(err));
	}

	/* Get pointer to the rec_out_buf for the block that has just
	 * completed.  Since merge_avail is always merge_cnt-1 ahead of
	 * the oldest unwritten rec_out_buf, the oldest unwritten 
	 * rec_out_buf is given by 
	 *		(merge_avail - (merge_cnt - 1)) % merge_cnt
	 * or
	 *		(merge_avail + 1) % merge_cnt
	 */
	rob = &MERGE.rob[(MERGE.merge_avail + 1) % MERGE.merge_cnt];

	if (Print_task)
	    fprintf(stderr, "aio_write %d, %d of %d, done at %s\n", 
		    OUT.writes_done, OUT.rob_writes_done, 
		    rob->n_writes, get_time_str(sort, tstr));

	OUT.writes_done++;
	/* 
	 * If there are no other writes outstanding stop the output busy meter
	 */
	if (OUT.writes_done == OUT.writes_issued)
	    OUT.file->stats.busy += get_time();
	
	/* if we just finished the last block write for a merge partition
	 */
	if (++OUT.rob_writes_done >= rob->n_writes)
	{
	    MERGE.merge_avail++;	/* make another merge available */
	    OUT.rob_writes_done = 0;
	}

	/* if we just finished the last direct write
	 */
	if (OUT.writes_final == OUT.writes_done)
	{
	    size = (unsigned) (rob->tail_rec - rob->remainder +
			       sort->edited_size);
	    if (Print_task)
		fprintf(stderr, "write %08x, %d @ %lld\n", rob->remainder,
				size, OUT.write_offset);

	    /* either seek and perform a non-FDIRECT write or
	     * fill up the last nsort_return_recs() buffer
	     */
	    (*sort->final_write)(sort, rob->remainder, size);

	    if (OUT.file->sync)
	    {
		OUT.file->stats.busy -= get_time();
		fsync(OUT.file->fd);
		OUT.file->stats.busy += get_time();
	    }

	    OUT.file->stats.bytes = OUT.write_offset + size;
	    OUT.file->stats.ios = OUT.writes_final + 1;
	    OUT.file->stats.finish_last = get_time();

	    /* tell io sproc (self) that it's done with output
	     */
	    OUT.end_io_output = TRUE;
	}
    }

    /* while there is a record output buffer available, try to write it out.
     */
    while (MERGE.merge_scanned < MERGE.merge_done)
    {
	/* if we have already scheduled a merge partition that was the
	 * end of the merge, ignore any subsequent (and empty) merge
	 * partitions.
	 */
	if (OUT.writes_final)
	    break;

	/* If there is no output buffer/aiocb available, break out.
	 * Begin an output wait period if it looks like this is the bottleneck.
	 * - no uncompleted merges
	 * - no uncompleted builds
	 */
	if (OUT.writes_issued == OUT.writes_done + OUT.aio_cnt)
	{
	    if (MERGE.merge_taken == MERGE.merge_avail &&
		TEMP.build_taken == TEMP.build_avail &&
		OUT.waiting == FALSE)
	    {
		/* Starting a 'wait for output write' period during a rec sort
		 */
		OUT.waiting = TRUE;
		OUT.file->stats.wait -= get_time();
	    }
	    break;
	}

	if (OUT.waiting)
	{
	    OUT.file->stats.wait += get_time();
	    OUT.waiting = FALSE;
	}
	idle = FALSE;
	
	rob = MERGE.rob + (MERGE.merge_scanned % MERGE.merge_cnt);

	/* if this is the first time we have seen this rob, determine 
	 * the number of writes it will take to write it out.
	 */
	if (OUT.rob_writes_issued == 0)
	    rob->n_writes = ROUND_UP_DIV(rob->out_size, OUT.write_size);

	aiocb = &OUT.aiocb[OUT.writes_issued % OUT.aio_cnt];
	aiocb->aio_offset = OUT.write_offset;
	aiocb->aio_buf = (void *)
		    (rob->begin_out + OUT.rob_writes_issued * OUT.write_size);
	aiocb->aio_nbytes = 
		    rob->out_size - OUT.rob_writes_issued * OUT.write_size;
	if (aiocb->aio_nbytes > OUT.write_size)
	    aiocb->aio_nbytes = OUT.write_size;

	/* if there are no other writes pending, start output busy meter.
	 */
	if (OUT.writes_done == OUT.writes_issued)
	{
	    time = get_time();
	    if (OUT.writes_done == 0)
		OUT.file->stats.start_first = time;
	    OUT.file->stats.busy -= time;
	}

	if (aiocb->aio_nbytes != 0)
	{
	    /* write out buffer
	     */
	    if ((sort->write_output)(aiocb) != 0)
		die("aio_write failure: %s", strerror(errno));
	    OUT.write_offset += aiocb->aio_nbytes;
	}
	if (Print_task)
	    fprintf(stderr, 
		    "aio_write %d, b %08x, %d @ %lld, %d of %d, eom %d at %s\n",
		    OUT.writes_issued, aiocb->aio_buf, aiocb->aio_nbytes, 
		    (i8) aiocb->aio_offset, OUT.rob_writes_issued,
		    rob->n_writes, rob->end_of_merge, get_time_str(sort, tstr));
	OUT.writes_issued++;

	/* if this was the last write for this rec_out_buf, use
	 * another rec_out_buf next time.
	 *   When the rob contains only a part of a block, then rob->out_size
	 * and n_writes will be zero;  making ++rob_writes_issued (1) be larger
	 * than n_writes (0) - hence the '>=' here.
	 */
	if (++OUT.rob_writes_issued >= rob->n_writes)
	{
	    OUT.rob_writes_issued = 0;
	    MERGE.merge_scanned++;
	
	    /* if this was the last merge partition, tell the sorting sprocs
	     * to exit.
	     */
	    if (rob->end_of_merge)
	    {
		OUT.end_sort_output = TRUE;
		OUT.writes_final = OUT.writes_issued;
	    }
	}
    }

    /* return TRUE if there was nothing interesting to do
     */
    return (idle);
}

int chk_attn_list(sort_t *sort)
{
    tmp_run_t	*run;
    tmp_run_t	*next_run;
    tmp_run_t	*prev_run;
    tbuf_t	*tbuf;
    tbuf_t	*prev_tbuf;
    blockheader_t *prev_blk;
    unsigned	tfile_index;

    if (TEMP.attn == NULL)
	return (TRUE);	/* indicate there was nothing to do */

    TIMES.chk_attns++;
    /* read head and NULLify the attention list atomically
     */
    do
    {
	run = TEMP.attn;
    } while (!compare_and_swap((ptrdiff_t *) &TEMP.attn,
			       (ptrdiff_t) run,
			       (ptrdiff_t) NULL,
			       SYS.ua));
    
    /* reverse order of link-list so that runs will be attended to on
     * a first come, first serve basis
     */
    prev_run = NULL;
    for (;;)
    {
	next_run = run->next;
	run->next = prev_run;
	if (next_run == NULL)
	    break;
	prev_run = run;
	run = next_run;
    }
    
    /* for each run on the attention list (now in FCFS order), see if there
     * is a tbuf that needs to be freed, or (in the case of non-optimal reads)
     * a new block read needs to be scheduled.  It may be that there is nothing
     * to do (see Nota Bene below).
     */
    while (run != NULL)
    {
	/* get next run pointer, THEN clear attn flag in run structure.
	 * this order is necessary because once the attn flag is cleared,
	 * a sort sproc can relink the run on the attn list (using run->next).
	 */
	next_run = run->next;
	run->attn = FALSE;

	/* Nota Bene - At this point it is possible another tbuf for this run
	 * will become eligible for recycling and we recycle it here forthwith.
	 * At the same time the run may be placed back on the attention list
	 * so that the said tbuf will be recycled.  When we attend to the run
	 * the second time there may be nothing to do.  This situation is
	 * preferable to a run needing attention not getting it. */

	/* look for buffers to recycle 
	 */
	while ((tbuf = run->old) != NULL && tbuf_recyclable(sort, tbuf))
	{
	    /* take tbuf off run's list
	     */
	    run->old = tbuf->r_next;
	    ASSERT(run->old != NULL);	/* should always be at least one tbuf */
	    ASSERT(tbuf->block + 1 == run->old->block);	/* sequentiality chk */
	    ASSERT(sort->method == SortMethRecord || tbuf->copy_done);

	    /* recycle tbuf
	     */
	    tbuf->r_next = TEMP.free;
	    tbuf->run = TBUF_RUN_FREELIST;
	    tbuf->file = -1;
	    tbuf->read_done = FALSE;
	    TEMP.free = tbuf;
	}

	/* if there are no tbufs on the run's chain, or if we are not doing
         * optimal reads and there is only one tbuf on the run chain, it has
         * been read in and initialized, and there are more blocks to be read
         * for this run, allocate another * tbuf and schedule it for reading. 
	 */
	if (run->blks_read == 0 || (!TEMP.optimal_reads &&
				    ((prev_tbuf = run->new) == run->old && 
				     prev_tbuf->item != NULL &&
				     run->blks_read < run->n_blks)))
	{
	    /* allocate new tbuf
	     */
	    tbuf = TEMP.free;
	    ASSERT(tbuf != NULL);
	    TEMP.free = tbuf->r_next;
	    ASSERT(tbuf->run == TBUF_RUN_FREELIST);
	    ASSERT(tbuf->read_done == FALSE);

	    if (run->blks_read != 0)	/* if not first block for run */
	    {
		ASSERT(run->blks_read == prev_tbuf->block + 1);
		prev_blk = (blockheader_t *)(prev_tbuf + 1);
		tbuf->offset = prev_blk->adv_off;
		tfile_index = prev_blk->adv_tfile;		
	    }		
	    else
	    {
		/* get initial block info from run struct itself.
		 */
		tbuf->offset = run->offset;
		tfile_index = run->tfile;
	    }

	    start_tbuf_read(sort, tbuf, TEMP.files + tfile_index, run);
	}
	 
	run = next_run;		/* on to next run in FCFS attention list */
    }

    return (FALSE);
}

void io_output_init(sort_t *sort)
{
    int			i;
    byte		*p;

    /* officially change from input phase to output phase
     * Set output_phase first, so that they'll both be false
     * only after the sort is finished; do_cancel() expects this.
     */
    sort->output_phase = TRUE;
    sort->input_phase = FALSE;

    TIMES.list_done = get_time() - TIMES.begin;

    if (STATS.usages != NULL)
    {
	getrusage(RUSAGE_SELF, &STATS.usages[0].in_usage);
	STATS.usages[0].in_elapsed = get_time() - TIMES.begin;
	ordaio_getrusage(&STATS.usages[sort->n_sprocs + 1].in_usage);
    }
    
    if (Print_task)
	fprintf(stderr, "STARTING OUTPUT PHASE\n");

    if (IN.runs_read == 0)
    {
	OUT.end_io_output = TRUE;
	OUT.end_sort_output = TRUE;
	OUT.file->stats.finish_last = get_time();
	return;
    }
    
    if (sort->two_pass)
    {
	MERGE.merge_scanned = 0;
	COPY.bufs_planned = 0;
	COPY.buf_position = NULL;
	COPY.curr_rec = NULL;
	MERGE.merge_avail = 0;	/* clear merge_avail before merge_taken to
				 * avoid race condition */
	MERGE.merge_taken = 0;
	MERGE.tails_done = 0;
	MERGE.tails_scanned = 0;
	MERGE.skip_done = 0;
	MERGE.merge_done = 0;
	MERGE.recs_consumed = 0;

	MERGE.width = IN.runs_write_sched;

	for (i = 0; i < TEMP.n_files; i++)
	{
	    TEMP.files[i].write_stats.ios = TEMP.files[i].io_done;
	    TEMP.files[i].write_stats.bytes = TEMP.files[i].write_offset;
	    TEMP.files[i].io_done = 0;
	    TEMP.files[i].io_issued = 0;
	}
	
	map_output_pass(sort);

	/* place all runs on the attention list so that their first block may
	 * be read.
	 */
	for (i = 0; i < MERGE.width - 1; i++)
	{
	    TEMP.run[i].next = &TEMP.run[i + 1];
	}
	TEMP.run[MERGE.width - 1].next = NULL;
	TEMP.attn = TEMP.run;
    }
    else
    {
	/* starting output phase of a one-pass sort */

	/* if record sort, prepare record output buffers
	 */
	if (sort->method == SortMethRecord)
	{
	    p = MERGE.rec_bufs;
	    for (i = 0; i < MERGE.merge_cnt; i++)
	    {
		MERGE.rob[i].u.buf = p;
		p += OUT.buf_size + 2 * OUT.rec_buf_pad;
	    }
	}
	else
	{
	    p = OUT.buf[0];
	    for (i = 0; i < OUT.buf_cnt; i++)
	    {
		OUT.buf[i] = p;
		p += OUT.buf_size;
	    }
	}
    }
}

void io_output_end(sort_t *sort)
{
    if (sort->api_invoked)
	free_files(sort);
    else if (OUT.file->openmode != -1)
    {
	int	elapsed = -get_time();
	int	cpu = -get_cpu_time();
	char	tstr[20];

	if (ftruncate(OUT.file->fd, OUT.write_offset) < 0)
	    die("io_output_end: could not truncate '%s': %s",
			    OUT.file->name, strerror(errno));
	if (Print_task)
	{
	    elapsed += get_time();
	    cpu += get_cpu_time();
	    fprintf(stderr, "truncate took %d ticks, %d cpu at %s\n", elapsed, cpu,
			    get_time_str(sort, tstr));
	}
    }

    TIMES.write_done = OUT.file->stats.finish_last - TIMES.begin;
}

int chk_io_output(sort_t *sort)
{
    unsigned		idle;

    idle = TRUE;
    
    if (MERGE.merge_avail == 0)
	chk_output_merge(sort);
    
    if (sort->method == SortMethRecord)
	idle &= chk_rec_output(sort);
    else
	idle &= chk_ptr_output(sort);
    
    if (sort->two_pass)
    {
	TIMES.temp_reads++;
	idle &= chk_temp_read(sort);

	idle &= chk_attn_list(sort);
    }

    return (idle);
}

void io_output(sort_t *sort)
{
    io_output_init(sort);
    
    while (!OUT.end_io_output)
    {
	if (chk_io_output(sort))
	{
	    TIMES.io_output_naps++;
	    sginap(1);
	}
	else
	{
	    TIMES.io_output_busys++;
	}
    }

    io_output_end(sort);
}
