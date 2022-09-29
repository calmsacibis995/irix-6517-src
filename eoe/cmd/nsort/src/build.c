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
 *	build.c 	- build items from temp buffers for pass two
 *
 *	$Ordinal-Id: build.c,v 1.12 1996/07/12 00:47:58 charles Exp $
 */
#ident	"$Revision: 1.1 $"

#include "otcsort.h"
#include "ovc.h"   /* record, fixed-length and variable pointer sort versions */

/*
 * build_tbuf_items -	create a list of items from an ordered set of records
 *
 * Parameters:
 *	sort -	two pass sort in the output phase (a.k.a pass 2)
 *	tbuf -	tbuf describing a temp block and space for items.
 *		The temp block is before the start of the tbuf:
 *		    (byte *) tbuf + sizeof(tbuf) - TEMP.tbuf_size
 *		The tbuf descriptor 
 *
 * Returns:
 *	The head of a linelist containing items for all the temp block.
 *
 *	The item list is layed out straight in memory. There is a single
 *	NULL_OVC at the end of the list.  Following this is a pointer back to
 *	the tbuf's start.  
 *
 * What It Does:
 * <var>loop through the records, assigning the lengths and pointers to items
 * <fix>Build items for the records. The data in a record sort is copied
 *	'down' to the item space, freeing the space contained by records
 *	for use by subsequent items.
 * -	Set the first item's ovc from the tbuf ovc, which was copied
 *	from the temp block header
 * -	Figure the minimum ovc of the records in this tbuf's temp block
 *	by calling recode with the first and last records.
 * -	Subtract one from the offset of this ovc to use in calling recode to
 *	assign the correct ovcs of each remaining (non-first) item. The recode
 *	loop runs backwards, from the last item to the first; here's why:
 *		The losing item will have its ovc set.  We want all the items'
 *		ovcs to be set.  Making each item a loser is a way to set the
 *		ovcs, but looping forward would require having the loser of
 *		one comparison being the winner of the next comparison.
 *		It would have to save the losing ovc, set the item/args's ovc
 *		to the 'base ovc' found above, and then restoring the saved ovc.
 *		By looping backwards an item is treated as a winner (with a
 *		base ovc) before it becomes the loser and its real ovc is set.
 *		No save/restore is needed.
 */
#if defined(VARIABLE_SORT)
item_t *build_tbuf_items_vs(sort_t *sort, tbuf_t *tbuf)
#elif defined(POINTER_SORT)
item_t *build_tbuf_items(sort_t *sort, tbuf_t *tbuf)
#else	/* RECORD_SORT */
item_t *build_tbuf_items_rs(sort_t *sort, tbuf_t *tbuf)
#endif
{
    blockheader_t	*block = (blockheader_t *) (tbuf + 1);
    recode_args_t	args;
    byte		*rec;
    byte		*first_rec;
    item_t		*item;
    item_t		*prev;
    unsigned		swap;
    unsigned		base_ovc;
    int			count;
#if defined(POINTER_SORT) && defined(DEBUG1)
    byte		*end_of_data;
#endif
    const unsigned	item_size = ITEM_SIZE(sort);
    u4			start_time = get_cpu_time();
#if defined(VARIABLE_SORT)
    unsigned		length;
    byte		*eorec;
    int			delimiter = RECORD.delimiter;
#else
    const unsigned	length = sort->edited_size;
#endif

    /* Build items, without assigned the ovc.
     */
    first_rec = (byte *) block + block->first_offset;
    item = (item_t *) ((byte *) tbuf + TEMP.tbuf_size - NULL_ITEM_SIZE);
    if (Print_task)
	fprintf(stderr, "building %d recs from tbuf %x, run %d block %d\n",
		block->n_recs, tbuf, tbuf->run, tbuf->block);
    ASSERT(block->run == tbuf->run);
    ASSERT(block->block == tbuf->block);

#if defined(VARIABLE_SORT)
    /* Set the lengths and pointers for variable length records
     * These are simply calculated in the ovc-assignment loop for fixlen records
     */
    item = item_math(item, -(int) (block->n_recs * item_size));
    rec = first_rec;
    for (count = block->n_recs; count != 0; count--)
    {
	if (RECORD.flags & RECORD_STREAM)
	{
	    eorec = memchr(rec + RECORD.minlength -1, delimiter, MAXRECORDSIZE);
	    if (eorec == NULL)
		die("build_tbuf_item:stream delimiter missing");
	    length = 1 + (int) (eorec - rec);
	    item->extra.var_extra.var_length = /*(u2) */ length;
	}
	else
	{
	    length = 2 + ulh(rec);
	    item->extra.var_extra.var_length = ulh(rec);
	}
	item->rec = rec;
	rec += length;
	item = item_math(item, item_size);
    }
#else
    rec = first_rec + length * block->n_recs;
    ASSERT((block->n_recs - 1) * length == block->last_offset - block->first_offset);
#endif

#if defined(POINTER_SORT) && defined(DEBUG1)
    end_of_data = rec;
#endif

    ASSERT(end_of_data <= (byte *) item /* TEMP.tbuf_size + (byte *) tbuf */);
    ASSERT((byte *) item_math(item, NULL_ITEM_SIZE) == (byte *) tbuf + TEMP.tbuf_size);

    /* if not the last block for the run
     */
    if (tbuf->block + 1 != TEMP.run[tbuf->run].n_blks)
    {
	/* NULL_OVC marks the end of this tbuf's section of this run's 
	 * item line-list.
	 */
	item->ovc = NULL_OVC;
	ITEM_TBUF(item) = tbuf;
    }
    else
    {
	/* last block in run, EOL_OVC marks end of item list for this run.
	 */
	item->ovc = EOL_OVC;
	ITEM_NEXTLINE(item) = NULL;
    }

    item = item_math(item, -(int) item_size);
    rec -= length;
    ASSERT(rec == (byte *)block + block->last_offset);

    if (block->n_recs > 1)
    {
	args.win_id = 0;
	args.curr_id = 1;
	args.summarize = 0;
	args.n_hard = 0;	/* in which statistics do these belong ? XXX */
	args.n_soft = 0;
	
#if defined(POINTER_SORT)
	prev = item - (block->n_recs - 1);
#if !defined(VARIABLE_SORT)
	prev->rec = first_rec;
	item->rec = rec;
#endif
	prev->ovc = VIRGIN_OVC;
	item->ovc = VIRGIN_OVC;
	swap = (*sort->recode)(sort, &args, prev, item);
	base_ovc = item->ovc;
#else
	args.win_ovc = VIRGIN_OVC;
	args.curr_ovc = VIRGIN_OVC;
	swap = (*sort->recode)(sort, &args, 
			       (item_t *) first_rec, (item_t *) rec);
	base_ovc = args.curr_ovc;
#endif
	
	ASSERT(!swap);
	
	base_ovc &= OFFSET_MASK;	/* clear out the value code portion */
	
	/* duplicate handling: check for impossible (deleted dups should
	 * have been deleting in the input phase) and the improbable
	 * (the entire block is kept duplicates of the first record,
	 * kept either due to 'noduplicates' being left off
	 * or to prevent overflow in a summarization field
	 */
	ASSERT(base_ovc != DEL_DUP_OVC);
	if (base_ovc == KEPT_DUP_OVC)
	    base_ovc = block->ovc & OFFSET_MASK;
	if (base_ovc == KEPT_DUP_OVC)
	{
	    /* This entire block and the last record of the previous block may
	     * all have the same keys.  If so we give them all a KEPT_DUP_OVC
	     * (with an id of the run number?) 
	     */
	    ;			/* XXX what, if anything, for here? */
	}
	else if (base_ovc != VIRGIN_OVC)
	{
	    /* bump up offset by one, to 'move' the base_ovc closer to the
	     * start of the key(s).  This offset may be safely used in all the
	     * recodes in this block. If the first pass has done its job and
	     * ordered this block of records then they all are identical up
	     * through this offset. This may result in base_ovc getting past the
	     * minimal real ovc in the hash ovc range - if so change to virgin.
	     */
	    base_ovc += 1 << OFFSET_SHIFT;

#if defined(POINTER_SORT)
	    /* The extra offset value in the item has not be set. We must not
	     * tell recode to look at it - force the offset to be odd so that
	     * the adjusted offset will be even and force a 'full' recode.
	     */
	    if (!(base_ovc & (1 << OFFSET_SHIFT)))
		base_ovc += 1 << OFFSET_SHIFT;
#endif
	    /* is this ovc now in the "hash" range? If so drop back to virgin
	     * (XXX unless this is a hashed or randomizing sort???)
	     */
	    if (base_ovc >= BUILD_OVC(0, 0))
		base_ovc = VIRGIN_OVC;
	}
    }
	
#if defined(POINTER_SORT)
    item->ovc = base_ovc;
    item->rec = rec;
#else
    args.win_ovc = base_ovc;    /* unchanged by recode - prev always wins */
    (*sort->copier)(ITEM_REC(item), rec, length);
#endif
    
    for (count = block->n_recs - 1; count != 0; count--)
    {
	prev = item_math(item, -(int) item_size);
#if defined(RECORD_SORT)
	/* build record sort items.  setting curr_ovc is unnecessary; for
	 * recode, since recode_rec doesn't use curr_ovc as an input param
	 */
	rec -= length;
	(*sort->copier)(ITEM_REC(prev), rec, length);
#else
	prev->ovc = base_ovc;
#if !defined(VARIABLE_SORT)
	/* If working on fixed-length records we build the items in this
	 * loop.  Variable length item's record pointers and lengths were
	 * set by the prior, variable-sort-only, loop.
	 */
	rec -= length;
	prev->rec = rec;
#endif
#endif

	if ((base_ovc & OFFSET_MASK) == KEPT_DUP_OVC)
	{
#if defined(DEBUG1)
	    if (compare_data(sort, ITEM_REC(prev), ITEM_REC(item)) != 0)
		die("build_tbuf_items: misordered");
#endif
#if defined(RECORD_SORT)
	    item->ovc = base_ovc;
#endif
	}
	else
	{
	    swap = RECODE(sort, &args, prev, item);
#if defined(DEBUG)
	    if (swap)
		die("build_tbuf_items: run %d block %d pos %d",
		    block->run, block->block, ITEM_REC(prev) - (byte *) block);
#endif
#if defined(RECORD_SORT)
	    /* ptr sorts' recode sets the ovc in the item.
	     * for record sorts we have to save it explicitly.
	     */
	    item->ovc = args.curr_ovc;
#endif
	}

	item = prev;
    }

#if defined(POINTER_SORT)
    ASSERT((byte *) item >= end_of_data);
#endif

    /* Now we should be pointing to the first record's item
     */
    item->ovc = block->ovc;
#if defined(VARIABLE_SORT)
    /* copy var_eov, but leave var_length intact since its has been correctly
     * scanned in this routine, but is not correct in block->u.extra for the
     * case this is the first block of a run (and the ovc is virgin_ovc).
     */
    item->extra.var_extra.var_eov = block->u.extra.var_extra.var_eov;
#elif defined(POINTER_SORT)
    item->extra = block->u.extra;
#endif

#if defined(DEBUG1)
    {
	int	found;
	item_t	*term;
	unsigned last_ovc;

	term = item_math(item, block->n_recs * item_size);
	ASSERT((byte *) term < (byte *) tbuf + TEMP.tbuf_size);
	ASSERT((byte *) term + NULL_ITEM_SIZE == (byte *) tbuf + TEMP.tbuf_size);
	last_ovc = term->ovc;
	term->ovc = EOL_OVC;
	found = check_ll(sort, item);
	
	if (found != block->n_recs)
	    die("build_tbuf_items:%d records missing", block->n_recs - found);
	term->ovc = last_ovc;
    }
#endif

    /* Tell merges that this tbuf is ready to be consumed (yum yum)
     */
    tbuf->item = item;

    atomic_add(&STATS.build_tbufs, get_cpu_time() - start_time, SYS.ua);

    return (item);
}
