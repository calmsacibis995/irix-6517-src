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
 *	$Ordinal-Id: process.c,v 1.23 1996/11/06 18:55:02 chris Exp
 */
#ident	"$Revision: 1.1 $"

#include "otcsort.h"
#include "merge.h"
#include <sys/times.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/cachectl.h>
#include <fcntl.h>
#include <malloc.h>
#include <fcntl.h>
#include <math.h>

/*#define IDLE_TIME 1*/

void print_total_time(sort_t *sort, const char *title);
unsigned get_time_us(void);

int SprocDelay = 0;
int SortpartLoss = 0;

void nsort_init(sort_t *sort)
{
    if (sort->io_sproc == 0)
    {
	sort->io_sproc = getpid();

	catch_signals(SIGINT, SIGQUIT, ordinal_exit);
	catch_signals(SIGILL, SIGPIPE, sigdeath);
    }
}

/* do_sort	- run a standalone nsort program sort. Move to nsort.c?
 */
void do_sort(sort_t *sort)
{
    sortsproc_t	*sp;
    short	*ssps;
    int		i;
    char	tstr[40];

    prepare_shared(sort);		/* (*sort->preparer)() */

    nsort_init(sort);

    if (Print_task)
	fprintf(stderr, "creating %d sorting sprocs cpu %d @ %s\n",
			(sort->single_process ? 0 : sort->n_sprocs),
			get_cpu_time(),
			get_time_str(sort, tstr));

    /* Allocate other sprocs. Include the current process as the one i/o sproc
     */
    ssps = (short *) malloc((1 + sort->n_sprocs) * sizeof(short));
    sort->sprocs = ssps;
    sp = (sortsproc_t *) malloc((1 + sort->n_sprocs) * sizeof(sortsproc_t));
    SYS.sprocs = sp;
    SYS.n_sprocs_cached = sort->n_sprocs + 1;
    ssps[0] = 0;
    sp[0].spid = 0;
    sp[0].sort = sort;
    sp[0].sproc_pid = getpid();
    if (STATS.wizard)
	SprocDelay = -(get_time() - get_cpu_time());
    if (!sort->single_process)
    {
	for (i = 1; i <= sort->n_sprocs; i++)
	{

	    if (Print_task)
		fprintf(stderr, "creating sorting sproc %d cpu %d @ %s\n",
			i, get_cpu_time(),
			get_time_str(sort, tstr));
	    ssps[i] = i;
	    sp[i].spid = i;
	    sp[i].sort = sort;
	    sp[i].sproc_pid =
		sprocsp((void (*)(void *arg, size_t stack)) sort->worker_main,
			PR_SADDR, (void *) &sp[i],
			(caddr_t) 0, MERGE_PART_DATA + STK_SZ);
	    if (sp[i].sproc_pid < 0)
		die("do_sort():sprocsp failure (brk: %08x) : %s", sbrk(0),
		    strerror(errno));
	}
    }

    if (STATS.wizard)
	SprocDelay += (get_time() - get_cpu_time());
    if (Print_task)
	print_total_time(sort, "done creating sprocs");

    init_aio(sort);

    /* all sprocs wait until the sort->go_sprocs variable is set so that their
     * page pinning activity doesn't delay the creation of other sprocs.  
     * Since all the sprocs have now been created, let 'em go.
     */
    sort->go_sprocs = TRUE;

    if (sort->single_process)
    {
	io_input_init(sort);
	sort_input(sort, 0);
    }
    else
	(*sort->reader)(sort);	/* e.g. read_input() */

    if (Print_task)
	print_total_time(sort, "done with input phase");

    if (sort->n_errors == 0)
    {
	if (sort->single_process)
	{
	    io_output_init(sort);
	    sort_output(sort, 0);
	    io_output_end(sort);
	}
	else
	    (*sort->writer)(sort); /* e.g. write_output() */
    }

    /* wait for other sprocs to complete by blocking self for each other
     * sproc created.
     */
    if (!sort->single_process)
	for (i = 1; i <= sort->n_sprocs; i++)
	    blockproc(sort->io_sproc);

    TIMES.done = get_time() - TIMES.begin;


    CONDITIONAL_FREE(sort->sprocs);
    CONDITIONAL_FREE(SYS.sprocs);
}

int Merge12;

void sortproc_main(sortsproc_t *ssp, size_t stack_size)
{
    unsigned	foo;
    unsigned	api;
    unsigned	sproc_id;
    unsigned	io_sproc;
    sort_t	*sort;
    char	tstr[40];

    sproc_id = ssp->spid;
    sort = ssp->sort;

    if (Print_task)
	fprintf(stderr, "sp: %08x and size %x in sproc %d (pid %d) cpu %d at %s\n", 
	       &foo, stack_size, sproc_id, getpid(), get_cpu_time(),
	       get_time_str(sort, tstr));

    api = sort->api_invoked;

    /* lower process priority so that (a?)i/o sproc's have priority over sorting
    if (!api)
	nice(1);
     */

    while (sort->go_sprocs == 0)
	ordnap(1, "go_sprocs");

    io_sproc = sort->io_sproc;

    /* help pin the record buffer
     */
    if (!sort->api_invoked && sort->touch_memory == FlagTrue)
	touch_mem(sort, sproc_id);

    sort_input(sort, sproc_id);

    if (STATS.usages != NULL)
    {
	getrusage(RUSAGE_SELF, &STATS.usages[sproc_id].in_usage);
	STATS.usages[sproc_id].in_elapsed = get_time() - TIMES.begin;
    }
    
    if (sort->n_errors == 0)
	sort_output(sort, sproc_id);

    if (STATS.usages != NULL)
    {
	getrusage(RUSAGE_SELF, &STATS.usages[sproc_id].total_usage);
	STATS.usages[sproc_id].total_elapsed =  get_time() - TIMES.begin;
    }

    if (Print_task)
	fprintf(stderr, "sproc %d unblocproc(%d)\n", sproc_id, io_sproc);

    if (Print_task)
	print_total_time(sort, "sproc exiting");

    if (unblockproc(io_sproc) < 0)	/* let originator know we're done */
	die("sortproc_main:unblockproc main %d: %s", io_sproc, strerror(errno));

    if (!api)
	exit(0);
}

int chk_merge(sort_t *sort, unsigned sproc_id, unsigned *idle_total)
{
    ptrdiff_t	taken;
    char	tstr[40];

    if ((taken = MERGE.merge_taken) < MERGE.merge_avail)
    {
	if (MERGE.skip_done != taken)
	{
#if defined(SHOWNAPS)
	    if (Print_task)
		fprintf(stderr,
			"chk_merge: merge %d avail, but skip only %d @ %s\n",
			taken, MERGE.skip_done, get_time_str(sort, tstr));
#endif

	}
	/* if we can successfully increment merge_taken via
	 * compare-and-swap, perform merge.
	 */
	else if (compare_and_swap(&MERGE.merge_taken, 
			     taken, taken + 1, SYS.ua))
	{
	    if (Print_task)
		fprintf(stderr, 
			"sproc %d merging part %d at %s\n", 
			sproc_id, taken, get_time_str(sort, tstr));
#if defined(IDLE_TIME)
	    *idle_total += get_time_us();
#endif
	    if (sort->input_phase)
		STATS.serial_in_merge -= get_cpu_time();
	    else
		STATS.serial_out_merge -= get_cpu_time();

	    if (sort->method == SortMethPointer)
		merge_part_ptr(sort, (unsigned) taken, sproc_id);
	    else
		merge_part(sort, (unsigned) taken, sproc_id);

	    if (MERGE.end_of_merge && sort->output_phase && !TIMES.merge_done)
	    {
		if (sort->internal)
		    OUT.end_sort_output = TRUE;

		TIMES.merge_done = get_time() - TIMES.begin;
	    }
	    if (sort->internal)
		MERGE.merge_avail++;	/* make another merge available */

#if defined(IDLE_TIME)
	    *idle_total -= get_time_us();
#endif

	    return (FALSE);	/* return indication of idleness */
	}
    }
    return (TRUE);	/* return indication of idleness */
}

char *print_copy_task(copy_task_t *task, char *buf)
{
    tbuf_t	*tbuf;
    extern sort_t *OTC_Latest;

    if ((tbuf = task->tbuf) == NULL)
	sprintf(buf, "skip %d srcptrs %x->%x dest %x->%x",
		 task->skip_bytes, task->src_rec, task->limit_rec,
		task->dest_buf, task->dest_buf + task->copy_bytes);
    else
	sprintf(buf, "%x srcptrs %x->%x tbuf %x cons:norm %d skip %d rem %d recycle '%c'",
		task, task->src_rec, task->limit_rec, tbuf,
		tbuf->norm_consumer, tbuf->skip_consumer, tbuf->rem_consumers,
		tbuf_recyclable(OTC_Latest, tbuf) ? 'T' : 'F');
    return (buf);
}

int chk_copy(sort_t *sort, unsigned sproc_id, unsigned *idle_total)
{
    ptrdiff_t		taken;
    char		tstr[40];
    char		copystr[100];

    while ((taken = COPY.copy_taken) < COPY.copy_avail)
    {
	/* if we can increment copy_taken via compare-and-swap, perform
	 * copy task.
	 */
	if (compare_and_swap(&COPY.copy_taken,
			     taken, taken + 1, SYS.ua))
	{
	    if (Print_task)
		fprintf(stderr, 
			"sproc %d taking copy %d %s at %s\n", 
			sproc_id, taken,
			print_copy_task(&COPY.copy[taken % COPY.copy_cnt],
					copystr),
			get_time_str(sort, tstr));
#if defined(IDLE_TIME)
	    *idle_total += get_time_us();
#endif
		
	    ASSERT(sort->input_phase || !sort->api_invoked);
	    if (taken == 0)
		TIMES.copy_first = get_time() - TIMES.begin;
	    copy_output(sort, &COPY.copy[taken % COPY.copy_cnt]);
	    
#if defined(IDLE_TIME)
	    *idle_total -= get_time_us();
#endif
	    return (FALSE);	/* return indication of idleness */
	}
    }
    return (TRUE);	/* return indication of idleness */
}

void sort_input(sort_t *sort, unsigned sproc_id)
{
    int			i;
    ptrdiff_t		pi;
    item_line_t		*ll_free, *ll;
    part_t		*part;
    u4			prev_run_index;
    u4			run_index;
    u4			time_us;
    int			loss;
    in_run_t		*run;
    char		tstr[40];
    unsigned		idle;
    unsigned		idle_total;

    ARENACHECK;

    /* initialize previous run index to an impossible number
     */
    prev_run_index = MAX_U4;
#if defined(IDLE_TIME)
    idle_total = -get_time_us();
#endif

    /* while this is a one pass sort and no all input partitions are taken,
     * or this is a two pass sort and not all runs are flushed to temp disks
     */
    while ((!sort->two_pass && IN.part_taken < IN.part_final) ||
	   (sort->two_pass && 
	    !(TEMP.writes_done != 0 && TEMP.writes_done == TEMP.writes_final)))
    {
	idle = TRUE;

	/* if there is a partition sort available, and the scan for the 
         * previous partition sort has completed, and we can take the next
	 * available partition.
	 */
	if ((pi = IN.part_taken) < IN.part_avail && pi == IN.part_scanned &&
	    compare_and_swap(&IN.part_taken, pi, pi + 1, SYS.ua))
	{
	    if (pi == 0)
		TIMES.list_first = get_time() - TIMES.begin;

	    idle = FALSE;

#if defined(IDLE_TIME)
	    idle_total += get_time_us();
#endif
	    /* get in_run and part structs for this input partition
	     */
	    run_index = (u4) (pi / IN_RUN_PARTS);

	    /* if this is the first partition in this in_run for this sproc,
	     * initialize this sproc's free list from in_run memory.
	     */
	    if (run_index != prev_run_index)
	    {
		run = &sort->in_run[run_index % N_IN_RUNS];
		prev_run_index = run_index;

		/* if this is the first partition for this input run, 
		 * initialize this input run.
		 */
		if ((pi % IN_RUN_PARTS) == 0)
		    prepare_in_run(sort, run_index);

		if (sort->single_process)
		    ll = run->ll_free;
		else
		    ll = run->ll_free + sort->n_ll_free * (sproc_id - 1);
		ll_free = ll;
		for (i = 0; i < sort->n_ll_free; i++)
		{
#if defined(INITIAL_FREELIST)
		    *((item_line_t **) ll) = 
#else
		    item_t *end_item;

		    end_item = (item_t *) item_math(ll, sort->ll_item_space);
		    end_item->ovc = NULL_OVC;
		    ITEM_NEXTLINE(end_item) = 
#endif
			(i == sort->n_ll_free - 1) ? NULL : ll + 1;
		    ll++;
		}
	    }

	    part = &run->partitions[pi % IN_RUN_PARTS];

#if defined(DEBUG1)
	    check_free(sort, ll_free);
#endif

	    if (!IN.ptr_input)
		ASSERT(run->rec_used > sort->begin_in_run);

	    /* Initialize the current partition for sorting.
	     * For fixed length records this is very quick.
	     * For stream and variable length records it takes
	     * a little while to scan the partition and save the
	     * lengths in the refuge.
	     * If it returns true then the partition is empty -
	     * the last portion of the run was sorted in a prior partition.
	     */
	    if ((IN.ptr_input ? prepare_part_ptr : prepare_part)(sort, run,
								       part))
	    {
		/* print the "empty" message just once per run */
		if (Print_task && ((pi % IN_RUN_PARTS) == 0 ||
				   run->part_lists[(pi % IN_RUN_PARTS) - 1]))
		    fprintf(stderr, 
			    "sproc %d finds part %d empty\n", sproc_id, pi);
		    run->part_lists[pi % IN_RUN_PARTS] = (item_line_t *) NULL;
#if defined(IDLE_TIME)
		idle_total -= get_time_us();
#endif
		continue;
	    }

	    if (Print_task)
		fprintf(stderr, "sproc %d sortpart %d (%d recs) at %s\n", 
			sproc_id, pi, part->n_recs, get_time_str(sort, tstr));
	    time_us = get_time_us();

	    if (!IN.ptr_input)
	    {
		ASSERT(part->rec_start > sort->begin_in_run);
		ASSERT(run->rec_used > sort->begin_in_run);
	    }
	    part->sproc_id = sproc_id;
	    run->part_lists[pi % IN_RUN_PARTS] = (sort->radix
				? gen_radix_list /*test_radix*/ : gen_list)
				(sort, part, &ll_free);
	    ARENACHECK;

	    time_us = get_time_us() - time_us;
	    loss = time_us / 1000 - part->sort_time * 10;
	    SortpartLoss += loss;
	    if (Print_task)
		fprintf(stderr, "sproc %d sortpart %d done u+s %d in %d.%02d ms lost %d ms at %s\n", 
			sproc_id, pi, part->sort_time,
			time_us / 1000, (time_us % 1000) / 10, loss,
			get_time_str(sort, tstr));

#if defined(DEBUG1)
	    check_free(sort, ll_free);
#endif
#if defined(IDLE_TIME)
	    idle_total -= get_time_us();
#endif
	}

	/* if there is a merge partition available for the taking,
	 * and the previous merge has completed its skip...
	 */
	idle &= chk_merge(sort, sproc_id, &idle_total);

	/* if there is a copy task available (pointer method only)
	 */
	if (sort->method == SortMethPointer)
	    idle &= chk_copy(sort, sproc_id, &idle_total);

	/* if not api invoked and single-process mode, handle io duties also
	 */
	if (!sort->api_invoked && sort->single_process)
	{
	    if (!IN.end_of_input)
		idle &= chk_input(sort);
	}

	/* if there was nothing to do, take a nap before trying again
	 */
	if (idle)
	{
	    atomic_add(&TEMP.n_idling, 1, SYS.ua);
	    ordnap(1, "input sorting loop");
	    atomic_add(&TEMP.n_idling, -1, SYS.ua);
	}
    }
#if defined(IDLE_TIME)
    if (STATS.wizard)
    {
	idle_total += get_time_us();
	STATS.usages[sproc_id].in_idle = idle_total / 10000;
	if (Print_task)
	    fprintf(stderr, "sproc %d input had idle time of %f ms\n",
			    sproc_id, idle_total / 1000.0);
    }
#endif
}


/* tbuf_recyclable -
 *
 * return TRUE if (the tbuf has been normally consumed &&
 *     tbuf has been skip consumed &&
 *     all merge partitions between the norm_consumer and skip_consumer 
 *		have remainder consumed the tbuf &&
 *	   (this is a record sort || all copying from this tbuf has completed))
 */
int tbuf_recyclable(sort_t *sort, tbuf_t *tbuf)
{
    return (tbuf->norm_consumer != MAX_U4 && 
	tbuf->skip_consumer != MAX_U4 && 
	tbuf->rem_consumers + 1 >= tbuf->skip_consumer - tbuf->norm_consumer &&
	(sort->method == SortMethRecord || tbuf->copy_done));
}


void cause_run_attn(sort_t *sort, unsigned run_index)
{
    tmp_run_t	*run;
    tmp_run_t	*run_head;

    /* abritrate for responsibility of placing run on list of runs needing
     * io sproc attention, as some other sproc may also be trying to do this.
     * An example of this race condition is when one sproc marks a tbuf as
     * normally consumed, another marks it as skip consumed, then they both
     * notice that the tbuf is recyclable and call this function for the run.
     */
    run = &TEMP.run[run_index];
    if (Print_task && run_index == 0 && run->blks_read > 3)
	fprintf(stderr, "cause_run0_attn: pid %d blks_read %d old %x new %x\n",
			getpid(), run->blks_read, run->old, run->new);
    if (compare_and_swap(&run->attn, FALSE, TRUE, SYS.ua))
    {
	/* keep trying to atomically add this run to head of the attention
	 * link-list until successful.
	 */
	do
	{
	    run_head = TEMP.attn;
	    run->next = run_head;
	} while (!compare_and_swap((ptrdiff_t *) &TEMP.attn,
				   (ptrdiff_t) run_head,
				   (ptrdiff_t) run,
				   SYS.ua));
    }
}


int chk_build(sort_t *sort, unsigned sproc_id)
{
    tbuf_t		*tbuf;
    ptrdiff_t		taken;
    unsigned		run;
    char		tstr[40];

    while ((taken = TEMP.build_taken) < TEMP.build_avail)
    {
	/* if we can increment copy_taken via compare-and-swap, perform
	 * copy task.
	 */
	if (compare_and_swap(&TEMP.build_taken,
			     taken, taken + 1, SYS.ua))
	{
	    if (Print_task)
		fprintf(stderr, 
			"sproc %d taking build %d at %s\n", 
			sproc_id, taken, get_time_str(sort, tstr));

	    tbuf = TEMP.build[taken % TEMP.build_cnt];
	    run = tbuf->run;

	    if (sort->method == SortMethRecord)
		build_tbuf_items_rs(sort, tbuf);	/* record */
	    else if (sort->record.flags & RECORD_FIXED)
		build_tbuf_items(sort, tbuf);		/* pointer, fixed */
	    else
		build_tbuf_items_vs(sort, tbuf);	/* pointer, variable */

	    /* if we are not doing optimal reads and this is the only tbuf
	     * on the run's chain of tbufs, link run on attention list as
	     * the run is now eligible to have its next block read in.
	     */
	    if (Print_task && run == 0 && tbuf->block >= 3)
		fprintf(stderr, "chk_build: tbuf %x run 0, blk %d r_next %x\n",
				tbuf, tbuf->block, tbuf->r_next);
	    if (!TEMP.optimal_reads && tbuf->r_next == NULL)
		cause_run_attn(sort, run);

	    return (FALSE);	/* return indication of idleness */
	}
    }
    return (TRUE);	/* return indication of idleness */
}


void sort_output(sort_t *sort, unsigned sproc_id)
{
    unsigned		idle;
    unsigned		idle_total;

    while (!OUT.end_sort_output ||
	   (sort->single_process && !OUT.end_io_output))
    {
	idle = TRUE;

	/* check if there is an item build task available for a tbuf and
	 * take it if possible.
	 */
	idle &= chk_build(sort, sproc_id);

	/* check if there a merge partition available and take it if possible
	 */
	idle &= chk_merge(sort, sproc_id, &idle_total);

	/* if there is a copy task available (pointer method only)
	 */
	if (sort->method == SortMethPointer && !sort->api_invoked)
	    idle &= chk_copy(sort, sproc_id, &idle_total);

	/* if not api invoked and single-process mode, handle io duties also
	 */
	if (!sort->api_invoked && sort->single_process)
	    idle &= chk_io_output(sort);

	/* if there was nothing to do, take a nap.
	 */
	if (idle)
	{
	    atomic_add(&TEMP.n_idling, 1, SYS.ua);
	    ordnap(1, "output sorting loop");
	    atomic_add(&TEMP.n_idling, -1, SYS.ua);
	}
    }
}


/* process_norms - 
 */
void process_norms(sort_t *sort, tbuf_t *tbuf, unsigned merge_index)
{
    tbuf_t	*next_tbuf, *prev_tbuf;
    unsigned	run_index;

    /* Reverse the tbuf list from lifo to fifo
     */
    prev_tbuf = NULL;
    for (;;)
    {
	next_tbuf = tbuf->f_next;
	tbuf->f_next = prev_tbuf;
	if (next_tbuf == NULL)
	    break;
	prev_tbuf = tbuf;
	tbuf = next_tbuf;
    }

    while (tbuf != NULL)
    {
	next_tbuf = tbuf->f_next;
	
	/* get run # before marking tbuf as normally consumed because the
	 * tbuf could instantly be recycled by another sproc once we mark
	 * the tbuf as consumed.  Its o.k. if the tbuf's run structure is
	 * put on the attention list twice.
	 */
	run_index = tbuf->run;

	tbuf->norm_consumer = merge_index;

	/* check if tbuf can now be recycled
	 */
	if (tbuf_recyclable(sort, tbuf))
	{
	    if (Print_task)
		fprintf(stderr, "next_tbuf: (process_norm) recycling tbuf %08x/%d/%d\n", tbuf, tbuf->run, tbuf->block);
	    cause_run_attn(sort, run_index);
	}
	else if (Print_task)
	    fprintf(stderr, "next_tbuf: (process_norm) !recycling tbuf %08x/%d/%d f_next %x\n",
			    tbuf, tbuf->run, tbuf->block, tbuf->f_next);

	tbuf = next_tbuf;
    }
}


/* next_tbuf - 
 */
CONST item_t *next_tbuf(sort_t	*sort,
		  CONST item_t	*last_item,
		  consumetype_t type,
		  merge_args_t	*args)
{
    tbuf_t	*old_tbuf;
    tbuf_t	*new_tbuf;
    unsigned	idle;
    unsigned	skip_consumed;
    unsigned	old_run;
    unsigned	start_time;
    unsigned	waiting;
    item_t	*begin_item;
    unsigned	idle_total;
    
    old_tbuf = ITEM_TBUF(last_item);
    if ((byte *)last_item <= (byte *)old_tbuf || 
	(byte *)last_item >= (byte *)old_tbuf + TEMP.tbuf_size)
    {
	die("next_tbuf: last_item %08x is not inside old_tbuf %08x",
	    last_item, old_tbuf);
    }
	
    /* get run # before marking tbuf as consumed in any manner because the
     * tbuf could instantly be recycled by another sproc once we mark the
     * tbuf as consumed.  Its o.k. if the tbuf's run structure is put on
     * the attention list twice.  
     */
    old_run = old_tbuf->run;

    if (Print_task)
	fprintf(stderr,
		"next_tbuf: done with tbuf %08x, run %d, blk %d, "
		"merge %d, tail %08x\n",
		old_tbuf, old_run, old_tbuf->block,
		args->merge_index, args->ptr_buf->tail_ptr);

    /* while the next tbuf for this run has not been allocated, or it has
     * been allocated but its item list has not been built
     */
    waiting = FALSE;
    while ((new_tbuf = old_tbuf->r_next) == NULL || new_tbuf->item == NULL)
    {
	idle = TRUE;

	/* if the previous merge has completed and we have saved up a list
	 * of tbufs to be marked as normally consumed when the previous
	 * merge completes, now mark them as normally consumed.
	 */
	if (MERGE.merge_done == args->merge_index && args->norm != NULL)
	{
	    process_norms(sort, args->norm, args->merge_index);
	    args->norm = NULL;
	}

	/* if this is not the first merge, and the last record output for
	 * this merge partition is not the first record output for the
	 * merge partition, and the previous merge has completed, and we
	 * haven't yet done this check, then check if the first record of
	 * this merge partition should be deleted relative to the tail
	 * record from the previous merge partition.
	 *
	 * By waiting for a second record to be output by the partition, we
	 * insure that we are done with summarizing into the first record.
	 */
	if (sort->method == SortMethPointer &&
	    type == NormConsume &&
	    args->merge_index != 0 && 
	    args->ptr_buf->tail_ptr != (volatile byte **)args->ptr_buf->ptrs &&
	    MERGE.merge_done == args->merge_index &&
	    MERGE.tails_done < args->merge_index)
	{
	    tail_ptr_check(sort, args->merge_index);
	}

	/* check if there is an item build task available for a tbuf and
	 * take it if possible.
	 */
	idle &= chk_build(sort, args->sproc_id);

	/* no need to look for a merge partition to take up because we
	 * are stalled in the middle of one here. */

	/* if there is a copy task available (pointer method only)
	 */
	if (sort->method == SortMethPointer && !sort->api_invoked)
	    idle &= chk_copy(sort, args->sproc_id, &idle_total);

	/* if there was nothing to do, take a nap.
	 */
	if (idle)
	{
	    if (!waiting && new_tbuf && !new_tbuf->read_done)
	    {
		start_time = get_time();
		waiting = TRUE;
	    }
	    /* We don't adjust n_idling here because for reads a sorting sproc
	     * (rather than the i/o sproc) figures wait times.
	     */
	    ordnap(1, "next_tbuf");
	}
	else if (waiting && new_tbuf && new_tbuf->read_done)
	{
	    atomic_add(&TEMP.files[new_tbuf->file].fileinfo.stats.wait,
			get_time() - start_time, SYS.ua);
	    waiting = FALSE;
	}
    }
    
    if (type == NormConsume)
    {
	/* if the previous merge has completed
	 */
	if (MERGE.merge_done == args->merge_index)
	{
	    /* if we have been saving up a list of tbufs to be marked as
	     * normally consumed when the previous merge completes, now
	     * mark them as normally consumed.
	     */
	    if (args->norm != NULL)
	    {
		process_norms(sort, args->norm, args->merge_index);
		args->norm = NULL;
	    }
	    
	    /* mark the old tbuf as normally consumed.
	     */
	    old_tbuf->norm_consumer = args->merge_index;
	}
	else
	{
	    /* put tbuf on a list of tbufs to be marked as normally consumed
	     * when we notice the previous merge has completed.
	     */
	    old_tbuf->f_next = args->norm;
	    args->norm = old_tbuf;
	}
    }

    if (type == SkipConsume)
    {
	old_tbuf->skip_consumer = args->merge_index;

	/* fprintf(stderr, "tbuf %08x skip consumed\n", old_tbuf); */
    }
    else
    {
	/* this is either a remainder consumption or normal consumption.
	 * 
	 * if there is no subsequent active merge and we can get the run_list
	 * spinlock.
	 */
	if (args->merge_index + 1 == MERGE.merge_taken &&
	    compare_and_swap(&MERGE.run_list_lock, FALSE, TRUE, sort))
	{
	    /* if no subsequent merge partition has been taken, we can update
	     * the run_list.
	     */
	    if (args->merge_index + 1 == MERGE.merge_taken)
		skip_consumed = TRUE;
	    else
	    {
		/* the subsequent merge partition has been taken but it hasn't
		 * gotten around to gabbing spinlock.  Release spinlock and
		 * forget about updating the run list.
		 */
		MERGE.run_list_lock = FALSE;
		skip_consumed = FALSE;
	    }
	}
	else
	    skip_consumed = FALSE;

	if (skip_consumed)
	{
	    /* get the current item pointer for this run in the run_list
	     * and verify that it belongs to the tbuf.
	     */
	    begin_item = (item_t *)MERGE.run_list[old_run];
	    if ((byte *)begin_item <= (byte *)old_tbuf || 
		(byte *)begin_item >= (byte *)old_tbuf + TEMP.tbuf_size)
	    {
		die("next_tbuf: begin_item %08x is not inside old_tbuf %08x\n",
		    begin_item, old_tbuf);
	    }

	    /* update this run's item in the run_list to the first item in
	     * the new tbuf.
	     */
	    MERGE.run_list[old_run] = (item_line_t *)new_tbuf->item;

	    /* update the recs_consumed count by the number of items between
	     * begin_item and last_item.
	     */
	    MERGE.recs_consumed += 
		((byte *)last_item - (byte *)begin_item) / sort->item_size;

	    MERGE.run_list_lock = FALSE;	    /* release spinlock */

	    /* we just acted as the skip consumer by advancing the run_list
	     * past old_tbuf.
	     *
	     * Note that we don't want to act both as skip and rem consumer
	     * since the test for the correct number of rem consumers
	     * (rem_consumers + 1 >= skip_consumer - norm_consumer) assumes
	     * that the skip consumer is not also a rem consumer.
	     */
	    old_tbuf->skip_consumer = args->merge_index;
	}
	else if (type == RemConsume)
	{
	    ptrdiff_t	count;

	    /* fprintf(stderr, "tbuf %08x remainder consumed\n", old_tbuf); */

	    /* serially increment the rem_consumers count
	     */
	    do
	    {
		count = old_tbuf->rem_consumers;
	    } while (!compare_and_swap(&old_tbuf->rem_consumers,
				       count,
				       count + 1,
				       SYS.ua));
	}
    }

    /* check if tbuf can now be recycled
     */
    if (tbuf_recyclable(sort, old_tbuf))
    {
	if (Print_task)
	    fprintf(stderr,
		    "next_tbuf: recycling tbuf %08x/%d/%d f_next %x\n",
		    old_tbuf, old_tbuf->run, old_tbuf->block, old_tbuf->f_next);
	cause_run_attn(sort, old_run);
    }
    else if (Print_task)
	    fprintf(stderr,
		    "next_tbuf: !recycling tbuf %08x/%d/%d f_next %x\n",
		    old_tbuf, old_tbuf->run, old_tbuf->block, old_tbuf->f_next);

    /* return first item of new tbuf to the patiently waiting merge code
     */
    return (new_tbuf->item);
}


void chk_output_merge(sort_t *sort)
{
    int		i, j, k;
    tbuf_t	*tbuf;

    if (sort->two_pass)
    {
	for (i = 0; i < MERGE.width; i++)
	{
	    /* if the first tbuf for the run has not been allocated,
	     * or its item list has not yet been built, return as we
	     * are not yet ready to start the merge.
	     */
	    if ((tbuf = TEMP.run[i].old) == NULL || tbuf->item == NULL)
		return;
		
	    MERGE.run_list[i] = (item_line_t *)tbuf->item;
	}
    }
    else
    {
	/* nap until all input partitions have been sorted.
	 * XXX under what kind of wait should this be accumulated?
	 */
	k = 0;
	for (i = 0; i < IN.runs_read; i++)
	{
	    for (j = 0; j < sort->in_run[i].n_parts; j++)
	    {
		while (sort->in_run[i].part_lists[j] == NULL)
		{
		    TIMES.chk_output_merge_naps++;
		    ordnap(1, "chk_output_merge");
		}
		MERGE.run_list[k++] = sort->in_run[i].part_lists[j];
	    }
	}
	MERGE.width = k;
    }

    /* set the merge skip interval to twice the square root of the number
     * of records (or record pointers) consumed in each merge partition.
     */
    MERGE.skip_interval = (unsigned)
	(2.0 * 
	 sqrt((sort->method == SortMethPointer ? 
	       (double)MERGE.part_recs :
	       (double)OUT.buf_size / sort->edited_size) / MERGE.width));
    
    /* allow sorting sprocs to commence merging
     */
    MERGE.merge_avail = (sort->method == SortMethRecord) ? (MERGE.merge_cnt - 1)
							 : MERGE.merge_cnt;
}
