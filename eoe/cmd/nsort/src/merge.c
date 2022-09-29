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
 * Merge.c
 *
 * $Ordinal-Id: merge.c,v 1.26 1996/10/03 22:34:22 charles Exp $
 *
 * $Ordinal-Log: merge.c,v $
 * Revision 1.26  1996/10/03  22:34:22  charles
 * simplify ptr_out code. KEEPDUPS versions no longer bother checking for dups
 *
 * Revision 1.24  1996/07/12  00:47:58  charles
 * Support for: 64-bit address space; completed radix sort implementation;
 * FixedDecimalString data type; bug fixes in skipping; reduced spurious errors
 * during parsing; exit w/o "die()'ing" if output write fails due to disk full
 * or broken pipe errors
 *
 * Revision 1.23  1996/07/09  18:42:00  chris
 * moved tail_ptr_check() call from merge_ptr_{skip,out}() to next_tbuf()
 *
 * Revision 1.22  1996/05/31  17:19:44  charles
 * #if defined(INITIAL_FREELIST) controls whether free line lists are linked at
 * the beginning of the line or at sort->ll_item_space.
 * Also for DEBUG1 sorts: only check ll ordering in input phase, to speed up:
 * otherwise there's a quadratic slowdown with larger (esp 1 pass) sorts.
 *
 * Revision 1.21  1996/05/15  00:51:17  charles
 * Initial Radix Sort support
 *
 * Revision 1.20  1996/03/26  22:59:03  charles
 * stats time is now user+sys cpu, not elapsed.
 * support for compiled-in-constant item size
 *
 * Revision 1.19  1996/02/14  21:47:14  charles
 * update copyright message
 *
 * Revision 1.18  1996/02/09  23:27:38  charles
 * Added ordinal copyright message
 *
 * Revision 1.17  1996/01/29  19:22:38  charles
 * *** empty log message ***
 *
 * Revision 1.16  1996/01/21  19:29:21  charles
 * keep track of the counts for the -stat=wizard statistics
 *
 * Revision 1.15  1996/01/17  18:53:54  chris
 * cleaned up some obscure cases with tail_ptr_check, end of merge
 *
 * Revision 1.14  1996/01/16  01:54:38  charles
 * tbuf support in summarizing ptr sorts: remember previous record ptr in
 * previtem; use this in summarize_pointer(), rather than ulw(output-entry_size)
 *
 * Revision 1.13  1995/12/27  21:57:26  chris
 * multi-process two-pass implemented
 *
 * Revision 1.12  1995/12/21  19:20:59  charles
 * Add DebugKey/DebugPtr code to help following a record through a sort (DEBUG1)
 *
 * Revision 1.11  1995/12/18  17:43:28  chris
 * changes for single-sproc two-pass output-phase
 *
 * Revision 1.10  1995/11/09  17:38:27  chris
 * changes to write temporary files during the input phase
 *
 * Revision 1.9  1995/10/31  20:45:08  charles
 * Add soft_recodes and hard_recodes to recode_args
 * The various merges now pass in a merge_args containing
 * the recode_args for their recodes-- this allow correct
 * counts to be accumulated even in a multiprocessor environment
 *
 * Revision 1.8  1995/10/11  18:20:07  charles
 * rearrange some lines to help optimizer reduce load stalls
 *
 * Revision 1.7  1995/08/31  20:50:17  charles
 * check ll order only if defined(DEBUG1)
 * print messages to stderr, not stdout
 *
 * Revision 1.6  1995/08/23  18:03:58  chris
 * fixed bug where we skipped past first EOL_OVC record
 *
 * Revision 1.5  1995/08/22  17:42:30  chris
 * clarified some comments
 *
 * Revision 1.4  1995/08/18  17:32:37  charles
 * support fixed and variable length pointer merges.
 * add #ifdef DEBUG checks for the output stream being correctly ordered
 *
 * Revision 1.9  1995/08/02  23:19:30  chris
 * fixed problem in skipping and output where first ovc's were not VIRGIN_OVC
 *
 * Revision 1.8  1995/07/28  20:41:59  chris
 * parallel merge mods
 *
 * Revision 1.7  1995/07/12  19:40:51  chris
 * added deletion, keeping and summarization of duplicates
 *
 * Revision 1.6  1995/07/09  22:52:44  chris
 * made one source routine to generate either merge_rec_out() or
 * merge_rec_line(), depending on macro definitions
 *
 */
#ident	"$Revision: 1.1 $"

/*
** There are six different compiled versions of this file
**	{ PTR_ | REC_ }  { LINE | SKIP | OUT }
**
**	PTR_LINE	REC_LINE
**	PTR_SKIP	REC_SKIP
**	PTR_OUT		REC_OUT
**
*/
#include "otcsort.h"
#include "ovc.h"
#include "merge.h"

#if defined(CONST_EDITED_SIZE)
#define merge_rec_line merge_rec_line_12
#define merge_rec_skip merge_rec_skip_12
#define merge_rec_out merge_rec_out_12
#define Merge_rec_line_id Merge_rec_line_12_id
#define Merge_rec_skip_id Merge_rec_skip_12_id
#define Merge_rec_out_id Merge_rec_out_12_id
#endif

#if !defined(FIXED_ONLY) && !defined (VARIABLE_ONLY)
#define FIXED_ONLY 	0
#define VARIABLE_ONLY	0
#endif

/*
** merge_{ptr,rec}_{line,skip,out}
**
** Returns:
**	Always: the parameter 'in_list' is changed to
**		point the starting place for the next merge.
**
**	Output cases:
**		returns the number of bytes which were put into
**		the output iov-like iobuf array.  The output buffers
**		may not be entirely filled in cases which duplicates
**		are deleted (delete_dups or n_summarized > 0)
**	Skip cases:
**		returns the number of remaining records to skip???
**
**	Line cases:
**		the head of the linelist containing the merged input linelists
*/

#if defined(PTR_LINE)
char		Merge_ptr_line_id[] = "merge.c (PTR_LINE) $Ordinal-Revision: 1.26 $";

item_line_t *merge_ptr_line(sort_t	*sort,		/* saved */
			    merge_args_t *args,
			    item_line_t	**ll_free,	/* saved */
			    list_desc_t	*append)	/* saved */
#elif defined(PTR_SKIP)
char		Merge_ptr_skip_id[] = "merge.c (PTR_SKIP) $Ordinal-Revision: 1.26 $";

int merge_ptr_skip(sort_t	*sort, 		/* saved */
		   merge_args_t *args,
		   int		recs_to_skip,   /* saved */
		   int		skip_interval)	/* saved */
#elif defined(PTR_OUT)
#if defined(KEEPDUPS)
char		Merge_ptr_keep_out_id[] = "merge.c (PTR_KEEP_OUT) $Ordinal-Revision: 1.26 $";
unsigned  merge_ptr_keep_out(sort_t		*sort,		/* saved */
			merge_args_t	*args,
			rec_out_t	*out_buf,	/* stack */
			int		recs_to_skip,	/* saved */
			int		recs_to_consume)/* saved */
#else
char		Merge_ptr_out_id[] = "merge.c (PTR_OUT) $Ordinal-Revision: 1.26 $";
unsigned  merge_ptr_out(sort_t		*sort,		/* saved */
			merge_args_t	*args,
			rec_out_t	*out_buf,	/* stack */
			int		recs_to_skip,	/* saved */
			int		recs_to_consume)/* saved */
#endif /* !KEEPDUPS */
#elif defined(REC_LINE)
char		Merge_rec_line_id[] = "merge.c (REC_LINE) $Ordinal-Revision: 1.26 $";

item_line_t *merge_rec_line(sort_t	*sort,		/* saved */
			    merge_args_t *args,
			    item_line_t	**ll_free,	/* saved */
			    list_desc_t	*append)	/* saved */
#elif defined(REC_SKIP)
char		Merge_rec_skip_id[] = "merge.c (REC_SKIP) $Ordinal-Revision: 1.26 $";

int merge_rec_skip(sort_t	*sort, 		/* saved */
		   merge_args_t *args,
		   int		recs_to_skip,   /* saved */
		   int		skip_interval)	/* saved */
#elif defined(REC_OUT)
#if defined(KEEPDUPS)
char		Merge_rec_keep_out_id[] = "merge.c (REC_KEEP_OUT) $Ordinal-Revision: 1.26 $";

unsigned  merge_rec_keep_out(sort_t		*sort,		/* saved */
			     merge_args_t	*args,
			     rec_out_t		*out_buf,	/* stack */
			     int		recs_to_skip,	/* saved */
			     int		recs_to_consume)/* saved */
#else
char		Merge_rec_out_id[] = "merge.c (REC_OUT) $Ordinal-Revision: 1.26 $";

unsigned  merge_rec_out(sort_t		*sort,		/* saved */
			merge_args_t	*args,
			rec_out_t	*out_buf,	/* stack */
			int		recs_to_skip,	/* saved */
			int		recs_to_consume)/* saved */
#endif /* !KEEPDUPS */
#endif
{
    unsigned		i;		/* temporary */
    unsigned		curr_ovc;	/* temporary */
    unsigned		swap;		/* temporary */
    node_t		*node, *o_node;	/* saved */
    unsigned		p_off;		/* temporary */
    unsigned		win_id;		/* temporary */
    unsigned		curr_id;	/* temporary */
    CONST item_t	*item;		/* temporary */
    unsigned		win_ovc;	/* saved */
    const int		item_size = ITEM_SIZE(sort);	/* stack */
    unsigned		n_inputs;	/* temp */
    item_line_t		**in_list;	/* temporary */
#if defined(DEBUG1)
    CONST item_t	*prev_item = NULL;
#endif
#if defined(REC_OUT)
    const int		output_entry_size = EDITED_SIZE(sort);
#elif defined(PTR_OUT)
    /*const*/ int		output_entry_size = (int)
			     ((!VARIABLE_ONLY && RECORD.flags & RECORD_FIXED) ||
			       FIXED_ONLY || sort->internal
			      ? sizeof(byte *)
			      : sizeof(byte *) + sizeof(u2));
#if !defined(KEEP_DUPS)
    item_t		previtem = { 0, { 0 }, 0 };
#endif
#endif
#if defined(PTR_OUT) || defined(PTR_SKIP)
    item_t		win_item;		/* dummy items to build when */
    item_t		curr_item;		/* recoding in parallel merge */
#endif
#if !defined(REC_LINE) && !defined(PTR_LINE)
    unsigned		not_first;	/* temporary */
    			/* flag indicating input is contained in tbuf item
			 * lists that require recycling. */
    unsigned		tbuf_input = (sort->output_phase && sort->two_pass);
#endif
#if defined(REC_OUT)|| defined(PTR_OUT)
    byte		*output;	/* saved */
    byte		*limit;		/* stack */
    unsigned		end_of_merge;   /* temporary */
    unsigned		update_run_list; /* flag for updating MERGE.run_list at
					  * the end of the merge */
    int			item_diff;
    u4			parallel_start;	/* time merge enters parallel section */
    int			begin_recs_to_skip;
    unsigned		win_off;	/* temporary */
#elif defined(REC_LINE) || defined(PTR_LINE)
    item_t		*out;		/* saved (now stack?) */
    item_t		*eol_out;	/* saved (now stack?) */
    item_line_t		*free;		/* stack? */
    item_line_t		*ret_list;	/* stack? */
    item_line_t		*line;		/* temporary */
#endif
#if defined(REC_SKIP)    
    leaf_skip_t		*leaf_row;	/* saved */
    leaf_skip_t		*win_leaf;	/* temporary */
    unsigned		new_ovc;	/* temporary */
#elif defined(PTR_SKIP)    
    leaf_eov_skip_t	*leaf_row;	/* saved */
    leaf_eov_skip_t	*win_leaf;	/* temporary */
    unsigned		new_ovc;	/* temporary */
    unsigned            eov_ovc;        /* temporary */
#if defined(DEBUG1)
    item_t		*max_skip;
#endif
#elif defined(PTR_OUT)    
    byte		*tail = NULL;	/* stack */
    leaf_eov_t		*leaf_row;	/* saved */
    leaf_eov_t		*win_leaf;	/* temporary */
#else	/* REC_OUT, REC_LINE, PTR_LINE */
    leaf_t		*leaf_row;	/* saved */
    leaf_t		*win_leaf;	/* temporary */
#endif
#if defined(DEBUG2)
    unsigned		in_count = 0, out_count = 0;
#endif

    node = args->node_buf;
    n_inputs = args->n_inputs;

    if (n_inputs == 1)
    {
	node->next = 0;
	node->id = -1;
	node->ovc = EOL_OVC;
	node++;
    }
    else
    {
	/* create tree and fill it with dummies.
	 */
	for (i = 1; i != n_inputs; i++)
	{
	    node->next = i == 1 ? 0 : (i - (i >> 1)) * (u2) sizeof(node_t);
	    node->id = n_inputs - i;
	    node->ovc = NULL_OVC + node->id;
	    node++;
	}
    }
#if defined(REC_SKIP)    
    leaf_row = (leaf_skip_t *)node;
#elif defined(PTR_SKIP)    
    leaf_row = (leaf_eov_skip_t *)node;
#elif defined(PTR_OUT)    
    leaf_row = (leaf_eov_t *)node;
#else
    leaf_row = (leaf_t *)node;
#endif

    /* set up input items to be one item back from the true first items.
     * As each NULL_OVC is flushed from the tree, it's corresponding
     * item pointer will be incremented to the true first item for that
     * input.
     */
    in_list = args->in_list;
    for (i = 0; i != n_inputs; i++)
    {
	leaf_row[i].curr_item = item_math(in_list[i]->item, -item_size);
#if defined(DEBUG2)
	if (sort->input_phase)
	    in_count += check_ll(sort, in_list[i]->item);
#endif
#if defined(PTR_SKIP) || defined(PTR_OUT)
	leaf_row[i].curr_eov = leaf_row[i].curr_item->extra.eov;
#endif
    }

#if defined(REC_OUT) || defined(PTR_OUT)
    /* since we have now read the item values in MERGE.run_list into leaf_row,
     * release run list spinlock.
     */
    parallel_start = get_cpu_time();

    if (sort->input_phase)
	STATS.serial_in_merge += parallel_start;
    else
	STATS.serial_out_merge += parallel_start;
    begin_recs_to_skip = recs_to_skip;
    STATS.merge_outs++;
    MERGE.run_list_lock = FALSE;

    /* Increment count of completed skips so that another sproc can
     * allocate the next merge and commence skipping.  This also allows the
     * io sproc to start polling begin_ptr and tail_ptr to determine if
     * there are copy tasks it can schedule for completion.
     */
    MERGE.skip_done++;
#endif

#if defined(REC_LINE) || defined(PTR_LINE)
    free = *ll_free;
    if (append == NULL)
    {
	ret_list = free;
	out = free->item;
	eol_out = item_math(out, sort->ll_item_space);
#if defined(INITIAL_FREELIST)
	free = *((item_line_t **) free);
#else
	free = ITEM_NEXTLINE(eol_out);
#endif
    }
    else
    {
	ret_list = append->head;
	out = (item_t *) append->next_item;
	eol_out = item_math(append->tail, sort->ll_item_space);
    }
    STATS.merge_lines++;

#elif defined(REC_SKIP) || defined(PTR_SKIP)
    /* for each input stream, add skip_interval to the skip count because
     * inserting the first real record for each input stream will cause the
     * skip count to be decremented by the skip interval.
     */
    recs_to_skip += n_inputs * skip_interval;

#elif defined(REC_OUT) || defined(PTR_OUT)
    /* add 1 for each NULL_OVC so that we can start decrementing the skip
     * count while we are still flushing initial NULL_OVC's from the tree.
     */
    recs_to_skip += n_inputs - 1;

    /* add 1 for each skipped rec so that we can start decrementing the
     * consumption count right away.
     */
    recs_to_consume += recs_to_skip + 1;

    /*
     * Set the current and ending pointers into the output buffer.
     * limit is reduced by the size of the last entry to move the subtraction
     * outside of the main merge loop.  We switch to the next out_buf when
     * output >= limit, i.e., we are about write beyond the end of this buffer.
     */
    output = out_buf->first;
    limit = out_buf->limit;
#endif

    win_id = 0;
    win_leaf = leaf_row;
    item = win_leaf->curr_item;
    win_ovc = NULL_OVC;
    goto enter;
    for (;;)
    {
#if 0 && (defined(REC_OUT) || defined(PTR_OUT))
	recs_to_consume--;	/* gulp, consumed another one */
#endif
	/* if an end-of-list marker has made it to the top of tree, we're done.
	 */
	if (win_ovc == EOL_OVC)
	    break;

	win_leaf = leaf_row + win_id;
	item = win_leaf->curr_item;

#if defined(DEBUG1)
	/*
	 * For debugging: ensure output stream is ordered.
	 * But can't do this check if the input streams are from tbufs because
	 * the previous item can instantly disappear if we cross tbufs.
	 * Another danger is that prev_item is in the input linelists, and it
	 * might have just be recycled; fortunately merge won't have touched
	 * it yet, but it's chancy - for instance we can't debugging-erase
	 * a line as it is recycled - for it still may be used
	 * XXX remove the prev_item pointer, replacing with the previtem struct
	 * XXX used in summarization???
	 */
	if (win_ovc >> (VALUE_BITS + 1))
	{
	    if (DebugKey && memcmp(sort->keys->position + ITEM_REC(item),
				   DebugKey,
				   strlen((char *) DebugKey)) == 0)
	    {
		DebugPtr = ITEM_REC(item);
		fprintf(stderr, "merge hits debug key: item %x rec %x out %x\n",
				item, ITEM_REC(item),
#if defined(REC_OUT) || defined(PTR_OUT)
				output
#elif defined(REC_LINE) || defined(PTR_LINE)
				 out
#else	/* skip cases have no output */
				 0
#endif
			);
	    }
	    if (sort->check_ll && prev_item != NULL)
		check_pair(sort, prev_item, item);
	    prev_item = item;
	}
#endif	/* DEBUG1 */

#if defined(REC_OUT) || defined(PTR_OUT)
	/* maybe output winner unless this is skipped record, or this is a
         * DEL_DUP_OVC (deleted duplicate), and (at least) one of:
	 * we are not summarizing, or
	 * this is not a kept duplicate record, or
	 * this is the first record of output, or
	 * this record cannot be summarized with the previous record,
	 * then output the record.
	 *	Test Cases:
	 * doing this summary where the previous record spans an i/o buffer
	 * boundary, and so part of the record's summarizing fields are not
	 * adjacent in memory - ugh - copy previous record to local buf ?
	 *			    - defer output in record sort till end
	 *			      of that key grouping?? 
	 */
	if (recs_to_skip <= 0 && (win_off = (win_ovc >> (VALUE_BITS + 1))) != 0)
	{
#if !defined(KEEPDUPS)	/* if this is the keepdups version, don't waste time */
			/* doing pointless duplicate detection checks */
			/* this saves about 2% of the merge-parallel time */
	    if (!sort->n_summarized ||
		win_off != (KEPT_DUP_OVC >> (VALUE_BITS + 1)) ||
		recs_to_skip == 0 ||
#if defined(REC_OUT)
	     /* check if the summation would overflow; if so it is not deleted
	      */
	      !summarize_record(sort, output - output_entry_size,ITEM_REC(item))
#else
	     /*
	      *	summarize_pointer needs to take items rather than record
	      * pointers - the item has the record length and eventually
	      * may have the field position table too.
	      * Use the dummy item built when the previous record was output
	      */
	      !summarize_pointer(sort, &previtem, item)
#endif
	      )
#endif	/* of !KEEPDUPS */
	    {
		/* if we have reached the end of this output buffer, go on
                 * to next. 
		 * This had better be a temp file, not the final destination.
		 * A final destination output merge must not contain any gaps,
		 * and so it has a single out_buf.
		 */
		if (output >= limit)
		{
		    out_buf++;
		    output = out_buf->first;
		    limit = out_buf->limit;
		}
#if defined(REC_OUT)
		/*
		 * Copy the record into the output buffer - it can't be MEMMOVE
		 * because the actual record size may not be a word multiple.
		 */
# if defined(CONST_EDITED_SIZE) && (CONST_EDITED_SIZE % 4) == 0
		MEMMOVE(output, ITEM_REC(item), output_entry_size);
# else
		(*sort->copier)(output, (CONST byte *) ITEM_REC(item), output_entry_size);
# endif

#if defined(DEBUG2)
		if (Print_task > 1)
		    nsort_print_data(sort, output, stderr,
				     output == out_buf->first, Print_task > 2);
#endif

#else	/* PTR_OUT */
		/* For a summarizing pointer sort we must keep the previous rec
		 * around for the following record's summation.
		 */
		previtem.rec = item->rec;

		if (output_entry_size == sizeof(byte *))
		    *(byte **) output = item->rec;
		else
		{
		    usp(output, item->rec);
		    *(u2 *) (output + sizeof(byte *)) = item->extra.var_extra.var_length;
		}
		tail = output;
#if defined(DEBUG2)
		if (Print_task > 1)
		    nsort_print_data(sort, item->rec, stderr,
				     output == out_buf->first, Print_task > 2);
#endif

#endif /* else PTR_OUT */
		output += output_entry_size;
	    }
	}

	recs_to_skip--;
#elif defined(REC_LINE) || defined(PTR_LINE)
	/* output winner unless this is a NULL_OVC (flushed out of the tree
	 * at startup), or this is a DEL_DUP_OVC (deleted duplicate).
	 */
	if (win_ovc >> (VALUE_BITS + 1))
	{
	    /* if no more room on this output line, get new one.
	     */
	    if (out == eol_out)
	    {
		out->ovc = NULL_OVC;
		ITEM_NEXTLINE(out) = free;
#if defined(DEBUG2)
		if (Print_task && append)
		    fprintf(stderr,
			    "merge_line: appending to tail %x new tail %x (free head %x)\n",
			    item_math(out, -sort->ll_item_space),
			    free, *((item_line_t **) free));
#endif	/* DEBUG2 */
		out = free->item;
		eol_out = item_math(out, sort->ll_item_space);
#if defined(INITIAL_FREELIST)
		free = *((item_line_t **) free);
#else
		free = ITEM_NEXTLINE(eol_out);
#endif
	    }

	    out->ovc = win_ovc;
#if defined(REC_LINE)
	    MEMMOVE/*(sort->copier)*/(ITEM_REC(out), ITEM_REC(item), item_size - sizeof(ovc_t));
	    out = item_math(out, item_size);
#else
	    out->extra.eov = item->extra.eov;	/* the r/w/r/w are probably */
	    out->rec = item->rec;		/* serialized, not r/r/w/w */
	    out++;
#endif
	}
#endif	/* output of the results for {REC,PTR}_{LINE,OUT} */

	/* Replace winner in tree with next record in the same list.
	 * This will result in either a new winner percolating to the 
	 * top of tree, or a duplicate key being deleted.  A deleted
	 * key is similar to a winner in that it will be replaced by
	 * a new record from the same stream, but it is not output.
	 */
enter:
	item = item_math(item, item_size);
	node = (node_t *)leaf_row - ((win_id >> 1) + 1);

#if defined(REC_LINE) || defined(PTR_LINE)
	win_ovc = item->ovc;
#else	/* out or skip cases */
	/* if the previous win_ovc was a flushed NULL_OVC, then the new
	 * record is the first record for this input stream.  Since the
	 * item ovc for this record is relative to the previous record, and
	 * because this is the first stream record for this merge partition,
	 * we must use VIRGIN_OVC instead of the item ovc.
	 */
	not_first = (win_ovc >> VALUE_BITS);
	win_ovc = VIRGIN_OVC;
	if (item->ovc == EOL_OVC || not_first)
	    win_ovc = item->ovc;
#endif	/* of non-LINE cases */

	if (win_ovc == NULL_OVC)  /* if we have reached the end of the line */
	{
#if defined(REC_LINE) || defined(PTR_LINE)
	    /* put old line on free list
	     * go on to next line in input list
	     */
#if defined(INITIAL_FREELIST)
	    line = LINE_START(item);
	    *((item_line_t **) line) = free;
	    free = line;
	    item = ITEM_NEXTLINE(item)->item;
#else
	    line = ITEM_NEXTLINE(item);
	    ITEM_NEXTLINE(item) = free;
	    free = LINE_START(item);
	    item = line->item;
#endif
#if defined(DEBUG2)
	    if (Print_task && append)
		fprintf(stderr, "merge_line: returing %x to free list\n", line);
#endif

#if defined(DEBUG3)
	    memset((byte *) free + sizeof(item_line_t *),
		 0xee,
		 LL_BYTES - sizeof(item_line_t *) - NULL_ITEM_SIZE - item_size);
#endif
#else
	    /* if this is the output phase of a two pass sort, we have
	     * just reached the end of the item list for a tbuf.
	     */
	    if (tbuf_input)
	    {
#if defined(REC_SKIP) || defined(PTR_SKIP)
		/* we have just skipped over the end of the tbuf's list.
		 * since we don't allow a skip to span two tbufs, this last
		 * skip at the end of the tbuf may not have been of size
		 * skip_interval.  calculate the actual number of items
		 * skipped and decrement from recs_to_skip count.
		 */
		recs_to_skip -= 
		    ((byte *)item - (byte *)win_leaf->back_item) / item_size;
		/* call routine to do appropriate tbuf recycling, potentially
		 * wait for next tbuf to be read in and its item list built,
		 * and get pointer to first item in the next tbuf's item list.
		 */
		item = next_tbuf(sort, item, SkipConsume, args);

#else	/* defined(REC_OUT) || defined(PTR_OUT) */

		/* Don't include next_tbuf() (build) time in merge parallel
		 */
		parallel_start = get_cpu_time() - parallel_start;
#if defined(PTR_OUT)
		/* if we have produced pointer output (normal consumption)
		 */
		if (recs_to_skip < 0)
		{
		    /* should have at least one pointer of output by now 
		     */
		    ASSERT(output != (byte *)args->ptr_buf->ptrs);

		    /* should have defined the tail ptr by now.
		     */
		    ASSERT(tail != NULL);
		    
		    /* insert the complement of the address of the tbuf we just
		     * finished with into the output stream so that when a copy
		     * task processes it (and all previous copy tasks have
		     * completed) we will know we are done with the tbuf for
		     * copying from.  The tbuf pointer is inverted so that it
		     * may be distinguished from a record pointer.
		     */
		    ASSERT(output < limit);
		    if (RECORD.flags & RECORD_FIXED)
			*(byte **)output = (byte *)~(ptrdiff_t)ITEM_TBUF(item);
		    else
		    {
			usp(output, (byte *) ~(ptrdiff_t)ITEM_TBUF(item));
			*(u2 *)(output + sizeof(byte *)) = 0;
		    }
		    output += output_entry_size;

		    /* post the current tail ptr so that the io sproc can
		     * create a copy task that copies records and frees tbufs
		     * up to the tail ptr.  the reason the tail ptr is not
		     * included is that we need to reserve at least one valid
		     * record pointer for the tail of this merge partition,
		     * and the current tail ptr may in fact be the last
		     * record pointer output by this merge partition.  
		     */
		    args->ptr_buf->tail_ptr = (volatile byte **)tail;
		}
#endif	/* of PTR_OUT */

		/* call routine to do appropriate tbuf recycling, potentially
		 * wait for next tbuf to be read in and its item list built,
		 * and get pointer to first item in the next tbuf's item list.
		 */
		item = next_tbuf(sort, 
				 item, 
				 recs_to_skip < 0 ? NormConsume : RemConsume, 
				 args);
		parallel_start = get_cpu_time() - parallel_start;
#endif
	    }
	    /*
	     * else we are simply dealing with line-list input (no tbufs to 
	     * recycle).
	     */
	    else
	    { 
		/* follow next-line pointer to get its first item.
		 */
		item = ITEM_NEXTLINE(item)->item;
#if defined(REC_SKIP) || defined(PTR_SKIP)
		recs_to_skip -= skip_interval;
#endif
	    }
#endif
	    win_ovc = item->ovc;
	}
#if defined(REC_SKIP) || defined(PTR_SKIP)
	/*
	 * else
	 */
	else
	    recs_to_skip -= skip_interval;
#endif	/* of skip cases */

#if defined(PTR_SKIP)
	/* copy current eov, and calculate eov_ovc.  See long comment below
	 * on eov_ovc calculation.
	 */
	win_leaf->curr_eov = item->extra.eov;
	eov_ovc = ((((win_ovc >> VALUE_BITS) + 1) & ~1) - 1) << VALUE_BITS;
#endif

#if defined(REC_SKIP) || defined(PTR_SKIP)
	/* make the current item the back item (the item we will have to go
	 * back to if the skipped to item does never wins the tournament 
	 * during this skip).  then test to see if we are done skipping.
	 */
	win_leaf->back_item = (item_t *) item;
	if (recs_to_skip - skip_interval < 0)
	    break;

#if defined(PTR_SKIP) && defined(DEBUG1)
	max_skip = item;
#endif
	/* Now that we have gotten the ovc for the first item after the the
	 * previous winner, skip ahead skip_interval-1 items, retaining the
	 * maximum ovc along the way.  The maximum ovc is equal to the ovc
	 * for the skipped-to item relative to the previous winner.  This
	 * follows from the inductive use of the following theorem:
	 *      OVC(A > C) = MAX(OVC(A > B), OVC(B > C))
	 * I.e. the ovc of A relative to C is equal the maximum of the ovc
	 * of A relative to B, and the ovc of B relative to C.
	 */
	for (i = 1; win_ovc != EOL_OVC && i < skip_interval; i++)
	{
	    item = item_math(item, item_size);
	    new_ovc = item->ovc;
	    if (new_ovc == NULL_OVC)	/* if end of line, go to next line */
	    {
		if (tbuf_input)
		{
		    /* we have reached the end of the tbuf's list.  since
		     * we don't allow skipping between tbufs (would cause
		     * mondo tbuf recycling problems), back up the skip to
		     * the previous item, which may even be the back item 
		     * for the skip.  if the skipped to item (the one we are
		     * backing up to) wins the tournament during skipping,
		     * the correct skip count (not skip_interval) will be
		     * used to decrement the skip count.
		     */
		    item = item_math(item, -item_size);
		    break;
		}
		else
		{
		    item = ITEM_NEXTLINE(item)->item;
		    new_ovc = item->ovc;
		    ASSERT(new_ovc != NULL_OVC);
		}
	    }
#if defined(REC_SKIP)
	    if (new_ovc > win_ovc)
		win_ovc = new_ovc;
#else	/* PTR_SKIP */
	    if (new_ovc > win_ovc)
	    {
		win_ovc = new_ovc;
		win_leaf->curr_eov = item->extra.eov;
#if defined(DEBUG1)
		max_skip = item;
#endif

		/* Calculate the lowest possible offset value code where
		 * the offset is in the same eov group as win_ovc's offset.
		 * Any subsequent ovc that we are skipping over that is lower
		 * than win_ovc but greater or equal to this eov_ovc will
		 * contain the correct eov.  The problem we are trying to
		 * solve is illustrated by the following case:
		 *
		 *              Key offset values
		 *               with eov groups
		 *                 indicated
		 *                 |   |   |     OVC    EOV   Comment
		 * prev winner      4 7 2 F        X     X
		 *                  5 4 A 2       E5     4    Max ovc
		 *                  5 5 2 1       D5     5    Correct eov
		 * skipped-to rec   5 5 4 0       C4     0    Offset in diffrnt
		 *                                             eov group.
		 *        Correct skip result:    E5     5
		 *     (ovc and eov of skipped-to
		 *     rec relative to prev winner)
		 *
		 * Since the lower offset of each eov group is odd, the
		 * eov_ovc contains the first odd offset which is equal or
		 * less than win_ovc's offset.
		 */
		eov_ovc =
		    ((((win_ovc >> VALUE_BITS) + 1) & ~1) - 1) << VALUE_BITS;
	    }
	    /*
	     * else the offset for the new record must be bigger than
	     * the offset in win_ovc (i.e the inverted offset in the ovc
	     * must be smaller), because offset-values do not decrease
	     * when the offsets are the same.  Now check to see if the new
	     * offset is in the same eov group as win_ovc's offset, and if
	     * so get the correct eov from the new record.
	     */
	    else if (new_ovc >= eov_ovc)
	    {
		win_leaf->curr_eov = item->extra.eov;
#if defined(DEBUG1)
		max_skip = item;
#endif
	    }
#endif /* of PTR_SKIP */
	}
#if defined(PTR_SKIP)
	ASSERT(win_leaf->curr_eov == max_skip->extra.eov);
#endif
#endif	/* of skip cases */

	/* note current item before we jump into the tree
	 */
	ASSERT(win_leaf->curr_item != item);
	win_leaf->curr_item = (item_t *) item;
#if defined(PTR_OUT)
	win_leaf->curr_eov = item->extra.eov;
#endif

#if defined(REC_OUT) || defined(PTR_OUT)
	/* if we have consumed the preordained number of records.
	 */
	if (--recs_to_consume <= 0)
	    break;
#endif

	/* if win_ovc is a KEPT_DUP_OVC, give it an offset-value equal to the
	 * current stream number to insure stable order of duplicates.
	 */
	if ((win_ovc >> VALUE_BITS) == (KEPT_DUP_OVC >> VALUE_BITS))
	    win_ovc = KEPT_DUP_OVC + win_id;
	o_node = NULL;

	for (;;)
	{
	    curr_ovc = node->ovc;
	    p_off = node->next;
	    if (node == o_node)
		break;
	    o_node = (node_t *)((char *)node - p_off);
	    if (curr_ovc <= win_ovc)
	    {
		curr_id = node->id;

		/* if (curr_ovc < win_ovc), then swap ovc's and id's
		 */
		if (curr_ovc != win_ovc)
		{
		    node->ovc = win_ovc;
		    node->id = win_id;
		    win_id = curr_id;
		    win_ovc = curr_ovc;
		}
		else if (win_ovc != EOL_OVC)
		{
		    args->recode.win_id = win_id;
		    args->recode.curr_id = curr_id;
#if defined(REC_LINE) || defined(REC_SKIP) || defined(REC_OUT)
		    args->recode.win_ovc = win_ovc;
		    args->recode.curr_ovc = curr_ovc;
		    /*
		     * Note: the RECODE macro should be kept on a one line.
		     * Many versions of cpp fail to increment the line number
		     * when scanning the actual args of a macro.
		     */
		    swap = RECODE(sort, &args->recode, leaf_row[win_id].curr_item, leaf_row[curr_id].curr_item);
		    win_ovc = args->recode.win_ovc;
		    curr_ovc = args->recode.curr_ovc;
#elif defined(PTR_LINE)
		    /*
		    ** Merge is not parallel.  Use the items in the input
		    ** lists as arguments to the recode routine.  We can do
		    ** this since:
		    ** 1) we can update the items right in the input lists
		    **    since there aren't other processes reading the
		    **    input lists (not parallel).
		    ** 2) no need to worry about the leaf ovc's (also win_ovc
		    **    and curr_ovc) becoming inconsistent with the input
		    **    list item ovc's, since new values for the leaf ovc's
		    **    always come from input list items, either in getting
		    **    a replacement item for the winning input list or
		    **    after a recode.
		    */
		    swap = RECODE(sort, &args->recode, leaf_row[win_id].curr_item, leaf_row[curr_id].curr_item);
		    win_ovc = leaf_row[win_id].curr_item->ovc;
		    curr_ovc = leaf_row[curr_id].curr_item->ovc;
#else	/* defined(PTR_SKIP) || defined(PTR_OUT) */
		    /*
		    ** Parallel skipping or merging.  The items in the input
		    ** lists must NOT be updated since multiple processes
		    ** can be reading them.  We must fabricate win and curr
		    ** items locally, using the win_ovc and curr_ovc as the
		    ** item ovc's, and the leaf eov's and the eov's.  After
		    ** the recode we must insure the loser record's new ovc
		    ** is updated in the current node, and the loser record's
		    ** new eov is updated in its leaf.
		    */
		    win_item.ovc = win_ovc;
		    win_item.extra.eov = leaf_row[win_id].curr_eov;
		    win_item.rec = leaf_row[win_id].curr_item->rec;
		    curr_item.ovc = curr_ovc;
		    curr_item.extra.eov = leaf_row[curr_id].curr_eov;
		    curr_item.rec = leaf_row[curr_id].curr_item->rec;
		    if (!(RECORD.flags & RECORD_FIXED))
		    {
			win_item.extra.var_extra.var_length =
			leaf_row[win_id].curr_item->extra.var_extra.var_length;
			curr_item.extra.var_extra.var_length =
			leaf_row[curr_id].curr_item->extra.var_extra.var_length;
		    }
		    swap = RECODE(sort, &args->recode, &win_item, &curr_item);
		    win_ovc = win_item.ovc;
		    curr_ovc = curr_item.ovc;
		    leaf_row[win_id].curr_eov = win_item.extra.eov;
		    leaf_row[curr_id].curr_eov = curr_item.extra.eov;
#endif
		    win_id = args->recode.win_id;
		    node->ovc = curr_ovc;
		    if (swap)
		    {
			ASSERT(args->recode.curr_id == curr_id);
			node->id = win_id;
			win_id = args->recode.curr_id;
			node->ovc = win_ovc;
			win_ovc = curr_ovc;
		    }
		}
	    }
	    curr_ovc = o_node->ovc;
	    p_off = o_node->next;
	    if (node == o_node)
		break;
	    node = (node_t *)((char *)o_node - p_off);
	    if (curr_ovc <= win_ovc)
	    {
		curr_id = o_node->id;

		/* if (curr_ovc < win_ovc), then swap ovc's and id's
		 */
		if (curr_ovc != win_ovc)
		{
		    o_node->ovc = win_ovc;
		    o_node->id = win_id;
		    win_id = curr_id;
		    win_ovc = curr_ovc;
		}
		else if (win_ovc != EOL_OVC)
		{
		    args->recode.win_id = win_id;
		    args->recode.curr_id = curr_id;
#if defined(REC_LINE) || defined(REC_SKIP) || defined(REC_OUT)
		    args->recode.win_ovc = win_ovc;
		    args->recode.curr_ovc = curr_ovc;
		    swap = RECODE(sort, &args->recode, leaf_row[win_id].curr_item, leaf_row[curr_id].curr_item);
		    curr_ovc = args->recode.curr_ovc;
		    win_ovc = args->recode.win_ovc;
#elif defined(PTR_LINE)
		    /*
		    ** Merge is not parallel.  Use the items in the input
		    ** lists as arguments to the recode routine.  We can do
		    ** this since:
		    ** 1) we can update the items right in the input lists
		    **    since there aren't other processes reading the
		    **    input lists (not parallel).
		    ** 2) no need to worry about the leaf ovc's (also win_ovc
		    **    and curr_ovc) becoming inconsistent with the input
		    **    list item ovc's, since new values for the leaf ovc's
		    **    always come from input list items, either in getting
		    **    a replacement item for the winning input list or
		    **    after a recode.
		    */
		    swap = RECODE(sort, &args->recode, leaf_row[win_id].curr_item, leaf_row[curr_id].curr_item);
		    win_ovc = leaf_row[win_id].curr_item->ovc;
		    curr_ovc = leaf_row[curr_id].curr_item->ovc;
#else	/* defined(PTR_SKIP) || defined(PTR_OUT) */
		    /*
		    ** Parallel skipping or merging.  The items in the input
		    ** lists must NOT be updated since multiple processes
		    ** can be reading them.  We must fabricate win and curr
		    ** items locally, using the win_ovc and curr_ovc as the
		    ** item ovc's, and the leaf eov's and the eov's.  After
		    ** the recode we must insure the loser record's new ovc
		    ** is updated in the current node, and the loser record's
		    ** new eov is updated in its leaf.
		    */
		    win_item.ovc = win_ovc;
		    win_item.extra.eov = leaf_row[win_id].curr_eov;
		    win_item.rec = leaf_row[win_id].curr_item->rec;
		    curr_item.ovc = curr_ovc;
		    curr_item.extra.eov = leaf_row[curr_id].curr_eov;
		    curr_item.rec = leaf_row[curr_id].curr_item->rec;
		    if (!(RECORD.flags & RECORD_FIXED))
		    {
			win_item.extra.var_extra.var_length =
			leaf_row[win_id].curr_item->extra.var_extra.var_length;
			curr_item.extra.var_extra.var_length =
			leaf_row[curr_id].curr_item->extra.var_extra.var_length;
		    }
		    swap = RECODE(sort, &args->recode, &win_item, &curr_item);
		    win_ovc = win_item.ovc;
		    curr_ovc = curr_item.ovc;
		    leaf_row[win_id].curr_eov = win_item.extra.eov;
		    leaf_row[curr_id].curr_eov = curr_item.extra.eov;
#endif
		    win_id = args->recode.win_id;
		    o_node->ovc = curr_ovc;
		    if (swap)
		    {
			ASSERT(args->recode.curr_id == curr_id);
			o_node->id = win_id;
			win_id = args->recode.curr_id;
			o_node->ovc = win_ovc;
			win_ovc = curr_ovc;
		    }
		}
	    }
	}
    }

    /*
    ** Return back to the caller where the merge ended
    ** Coding hack: Re-assign to n_inputs and in_list, so that they will be dead
    ** in the body of merge above, and so will be assignable to temp registers.
    */
    n_inputs = args->n_inputs;
    in_list = args->in_list;
#if defined(REC_SKIP) || defined(PTR_SKIP)
    for (i = 0; i != n_inputs; i++)
	in_list[i] = (item_line_t *)leaf_row[i].back_item;
    return (recs_to_skip);
#endif

#if defined(REC_OUT) || defined(PTR_OUT)
    out_buf->end = output;	/* mark end of output */

    /* if there is no subsequent active merge and we can get the run_list
     * spinlock.
     */
    if (args->merge_index + 1 == MERGE.merge_taken &&
	compare_and_swap(&MERGE.run_list_lock, FALSE, TRUE, sort))
    {
	/* if no subsequent merge partition has been taken, we can update
	 * the run_list.
	 */
	if (args->merge_index + 1 == MERGE.merge_taken)
	    update_run_list = TRUE;
	else
	{
	    /* the subsequent merge partition has been taken but it hasn't
	     * gotten around to gabbing spinlock.  Release spinlock and
	     * forget about updating the run list.
	     */
	    MERGE.run_list_lock = FALSE;
	    update_run_list = FALSE;
	}
    }
    else
	update_run_list = FALSE;

    /* copy current positions in input lists, and return true (end of merge)
     * if all ovc's in the current items contain EOL_OVC.
     */
    end_of_merge = TRUE;
    item_diff = 0;
    for (i = 0; i < n_inputs; i++)
    {
	item = leaf_row[i].curr_item;
	if (update_run_list)
	{
#if defined(DEBUG1)
	    if (tbuf_input)
	    {
		/* update the item byte diff count
		 */
		item_diff += (byte *)item - (byte *)in_list[i];
	    }
#endif
	    in_list[i] = (item_line_t *)item;
	}
	if (item->ovc != EOL_OVC)
	    end_of_merge = FALSE;
    }

    if (update_run_list)
    {
#if defined(DEBUG1)
	/* if the merge input was from tbufs and we didn't reach end-of-merge
	 * and computed 
	 */
	if (tbuf_input && !end_of_merge &&
	    MERGE.recs_consumed + item_diff / sort->item_size != 
	    						args->end_consume)
	{
	    die("consumption mismatch: %d vs. %d\n", 
		MERGE.recs_consumed + item_diff / sort->item_size,
	    						args->end_consume);
	}
#endif
	/* since we are updating the run_list to be the ending items for this
	 * merge partition, update recs_consumed to be the target ending point
	 * for this merge.
	 */
	MERGE.recs_consumed = args->end_consume;
	MERGE.run_list_lock = FALSE;	    /* release spinlock */
    }

#if defined(PTR_OUT)
    /* We should have a valid tail pointer that does not point to the
     * first record of the merge, or we should have hit end of merge.
     *
     * There is no problem with the obscure case that we hit end of
     * merge and tail points to the first record pointer output
     * (tail == args->ptr_buf->ptrs) because the io sproc will
     * ignore the value of args->ptr_buf->tail_ptr if end of merge.  
     * But there is a problem of the first merge being entirely empty
     * (e.g. a file containing only a partial record): the tail_ptr
     * may be set to NULL, and then the i/o sproc blows up using the
     * tail_ptr? io.c handles tail_ptr being null - better is to ??? XXX
     */
    ASSERT((tail != NULL && tail != args->ptr_buf->ptrs) || end_of_merge);

    args->ptr_buf->tail_ptr = (volatile byte **)tail;
    args->ptr_buf->limit_ptr = (volatile byte **)output;
#endif

    atomic_add((sort->input_phase ? &STATS.parallel_in_merge
				  : &STATS.parallel_out_merge),
	       get_cpu_time() - parallel_start,
	       SYS.ua);
    
    atomic_add(&STATS.merge_skipped,
	       begin_recs_to_skip - (recs_to_skip < 0 ? 0 : recs_to_skip),
	       SYS.ua);

    return (end_of_merge);
#endif	/* of REC_OUT and PTR_OUT cases */

#if defined(REC_LINE) || defined(PTR_LINE)

    for (i = 0; i != n_inputs; i++)
	in_list[i] = (item_line_t *)leaf_row[i].curr_item;
    out->ovc = EOL_OVC;		/* indicate end-of-list in output */
    ITEM_NEXTLINE(out) = NULL;

    *ll_free = free;		/* return updated free list */
    if (append)
    {
	ASSERT(append->free == free);	/* ll_free should be &append->free */
	append->tail = (item_line_t *) ROUND_DOWN(out, LL_BYTES);
	append->next_item = (byte *) out;
    }

#if defined(DEBUG2)
    if (sort->input_phase)
    {
	out_count = check_ll(sort, ret_list->item);
	ASSERT(out_count == in_count || sort->any_deletes);
    }
#endif

    return (ret_list);		/* return merge output */
#endif
}
