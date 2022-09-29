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
 * Copytask - copy records from a merge results ptr array code for pointer sorts
 *
 *	$Ordinal-Id: copytask.c,v 1.15 1996/11/01 23:09:35 charles Exp $
 */
#ident	"$Revision: 1.1 $"

#include	"otcsort.h"
#include	"merge.h"

char			Copy_output_id[] = "$Ordinal-Revision: 1.15 $";

#if defined(DEBUG1)
extern byte	*DebugPtr;
#endif

/*
** copy_output - copy records indicated by an array of pointers to an output buf
**
** Parameters:
**
**	It is possible for a task to be very small, e.g. it may need to copy
**	only the last byte of a record, after having handled the earlier part of
**	the record in a prior task. In particular this means that the skip_bytes
**	part of the copy is the only part; avoid the loop in that case.
**
**	The length for RECORD_VARIABLE records does not include the two-byte
**	for the length field itself, so we add length_extra (either 2 or 0)
**	to the length in the pointer&length pairs to get the number of bytes
**	to copy.
*/
void copy_output(sort_t *sort, copy_task_t *task)
{
#if 1
    byte	dest_buf[MERGE_PART_DATA];
    byte	*out = sort->output_phase && OUT.file->copies_write ? dest_buf
							     : task->dest_buf;
#else
    byte	*out = task->dest_buf;
#endif
    copyfunc_t	copier = sort->copier;
    int		remaining = task->copy_bytes;
    unsigned	fixed = sort->record.flags & RECORD_FIXED;
    int		length_extra = sort->record.var_hdr_extra;
    unsigned	length;
    byte	**p = task->src_rec;
    byte	**limit = task->limit_rec;
    byte	*rec;
    unsigned	skip_bytes = task->skip_bytes;
    int		touch_sum, touch_sum2;
    tbuf_t	*tbuf;
    u4		start_time = get_cpu_time();
#if defined(DEBUG1)
    byte	*prev = NULL;
#endif

    ASSERT(task->copy_done == FALSE);

    if (fixed)
	length = sort->edited_size;

    if (skip_bytes != 0)	/* has some of the first record */
    {				/* already been copied? */
	if (fixed)
	    rec = *p++;
	else
	{
	    rec = ulp(p);
	    /* the length (a short) directly follows the pointer */
	    length = *((u2 *) (p + 1)) + length_extra;
	    p = (byte **) ((ptrdiff_t) p + VAR_RESULT_SIZE);
	}
#if defined(DEBUG1)
	if (DebugPtr == rec)
	    fprintf(stderr, "skip copy at %x splits rec into %d and %d\n",
		    task, length - skip_bytes, skip_bytes);
#endif
	ASSERT(skip_bytes < length);
	memmove(out, rec + skip_bytes, length - skip_bytes);
	remaining -= length - skip_bytes;
	ASSERT(remaining >= 0);
	out += length - skip_bytes;
    }

    if (!fixed)		/* variable length - get each pointer and record len */
    {
	while (p < limit)
	{
	    /* fetch pointer to the rec, and then the length
	     * (which is a short immediately after the pointer
	     */
	    rec = ulp(p);
	    length = *((u2 *) ((byte **) p + 1)) + length_extra;
#if defined(DEBUG1)
	    if (DebugPtr == rec)
		fprintf(stderr, "copy %d of task %x does rec %x len %d\n",
			((ptrdiff_t) p - (ptrdiff_t) task->src_rec) /
			 VAR_RESULT_SIZE,
			task, rec, length);
#endif
	    p = (byte **) ((ptrdiff_t) p + VAR_RESULT_SIZE);

	    if ((ptrdiff_t)rec > 0)
	    {
#if defined(DEBUG1)
		if (prev && compare_data(sort, prev, rec) > 0)
		    die("copy_output:misordered %x %x", prev, rec);
		prev = rec;
#endif

		ASSERT(length != 0 && length < MAXRECORDSIZE + 2);
		if (length <= remaining)
		{
		    (*copier)(out, rec, length);
		    remaining -= length;
		    out += length;
		}
		else
		    goto copy_remaining;
	    }
	    else
	    {
		/* record ptr is really tbuf ptr with the high order bit set.
		 * Link tbuf onto a list of tbufs completed by this copy task.
		 */
		tbuf = (tbuf_t *)~(ptrdiff_t)rec;
#if defined(DEBUG1)
		if (Print_task)
		    fprintf(stderr, "copy %x finds tbuf %x/%d/%d f_next %x at %x\n",
			    task, tbuf, tbuf->run, tbuf->block, task->tbuf, p);
#endif
		tbuf->f_next = task->tbuf;
		task->tbuf = tbuf;
	    }
	}
    }
    else if ((length % 4) == 0)	/* fixed length, edited size good for MEMMOVE */
    {
	while (p < limit)
	{
	    rec = *p++;

	    if ((ptrdiff_t)rec > 0)
	    {
#if defined(DEBUG2)
		if (prev && compare_data(sort, prev, rec) > 0)
		    die("copy_output:misordered %x %x", prev, rec);
		prev = rec;
#endif

		if (length <= remaining)
		{
		    MEMMOVE/*memmove*/(out, rec, length);
		    remaining -= length;
		    out += length;
		    if (&p[2] < limit)	/* prefetch two records ahead */
		    {
			/* the next record ought to be in the tlb now */
#pragma			prefetch_ref= p[1][0], kind=rd
#pragma			prefetch_ref= p[1][96], kind=rd
#pragma			prefetch_ref= out[256], kind=wr
			if ((ptrdiff_t)p[2] > 0)
			    touch_sum2 = p[2][0];	/* fill in tlb slot */
		    }
		}
		else
		{
copy_remaining:
		    if (remaining)
		    {
#if defined(DEBUG2)
			if (DebugPtr == rec)
			    fprintf(stderr, "final copy of task %x splits rec %x %d\n",
				    task, rec, remaining);
#endif

#if defined(DEBUG1)
			if (prev && compare_data(sort, prev, rec) > 0)
			    die("copy_output:remaining misordered %x %x", prev, rec);
#endif
			memmove(out, rec, remaining);
			out += remaining;
		    }
		    break;
		}
	    }
	    else
	    {
		/* record ptr is really tbuf ptr with the high order bit set.
		 * Link tbuf onto a list of tbufs completed by this copy task.
		 */
		tbuf = (tbuf_t *)~(ptrdiff_t)rec;
#if defined(DEBUG1)
		if (Print_task)
		    fprintf(stderr, "copy %x finds tbuf %x/%d/%d f_next %x at %x\n",
			    task, tbuf, tbuf->run, tbuf->block, task->tbuf, p);
#endif
		tbuf->f_next = task->tbuf;
		task->tbuf = tbuf;
	    }
	}
    }
    else	/* fixed length, but not an aligned size - probably memmove() */
    {
	while (p < limit)
	{
	    rec = *p++;

	    if ((ptrdiff_t)rec > 0)
	    {
#if defined(DEBUG2)
		if (prev && compare_data(sort, prev, rec) > 0)
		    die("copy_output:misordered %x %x", prev, rec);
		prev = rec;
#endif

#if defined(DEBUG2)
		if (DebugPtr == rec)
		    fprintf(stderr, "task %x copy %d does rec %x to %x\n",
			    task, p - task->src_rec, rec, out);
#endif
		if (length <= remaining)
		{
		    (*copier)(out, rec, length);
		    remaining -= length;
		    out += length;
		}
		else
		    goto copy_remaining;
	    }
	    else
	    {
		/* record ptr is really tbuf ptr with the high order bit set.
		 * Link tbuf onto a list of tbufs completed by this copy task.
		 */
		tbuf = (tbuf_t *)~(ptrdiff_t)rec;
#if defined(DEBUG1)
		if (Print_task)
		    fprintf(stderr, "copy %x finds tbuf %x/%d/%d f_next %x at %x\n",
			    task, tbuf, tbuf->run, tbuf->block, task->tbuf, p);
#endif
		tbuf->f_next = task->tbuf;
		task->tbuf = tbuf;
	    }
	}
    }
#if defined(DEBUG1)
    task->final_rec = p;
#endif
    if (sort->output_phase && OUT.file->copies_write)
    {
#if defined(WRITETIMES)
	int	duration;
#endif

	if (task->end_of_merge)
	{
#if defined(WRITETIMES)
	    fprintf(stderr, "end of merge at write position 0x%llx\n", task->write_position);
#endif
	    /* flush the last write(s?) to disk, so a direct read will see it
	     */
	    set_file_flags(OUT.file, FSYNC);
	}
#if defined(WRITETIMES)
	duration = -get_time();
#endif
	if (pwrite(OUT.file->fd, out - task->copy_bytes, task->copy_bytes, task->write_position) < 0)
	{
	    if (errno == EPIPE || errno == ENOSPC)
	    {
		fprintf(stderr, "nsort: %s\n", strerror(errno));
		ordinal_exit();
	    }
	    die("copy_task: write (0x%x, %d) @ %lld failed: %s", task->dest_buf,
		    task->copy_bytes, task->write_position, strerror(errno));
	}
#if defined(WRITETIMES)
	duration += get_time();
	if (duration > 1)
	{
	    fprintf(stderr, "copies_write @0x%llx : %d ticks\n",
			    task->write_position, duration);
	    if (FALSE && task->end_of_merge)
	    {
		duration = -get_time();
		fsync(OUT.file->fd);
		duration += get_time();
		fprintf(stderr, "\textra sync took %d ticks\n", duration);
	    }
	}
#endif
    }
	
    task->copy_done = TRUE;

    atomic_add((sort->input_phase ? &STATS.input_copy : &STATS.output_copy),
		get_cpu_time() - start_time,
		SYS.ua);
    SYS.touch_sum = touch_sum + touch_sum2;
}
