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
 * $Ordinal-Id: genline.c,v 1.1 1996/07/12 00:47:58 charles Exp $
 */
#ident	"$Revision: 1.1 $"

#include	"otcsort.h"
#include	"merge.h"

typedef item_t *(*backward_sort_t)(sort_t *sort, recode_args_t *args, item_t *begin, item_t *end);

/* GEN_LINE - generate a single-line line-list of n_recs items.
 */
byte *gen_line(sort_t		*sort,
	       unsigned		n_recs, 
	       byte		*in_rec,
	       item_line_t	*out_list,
	       part_t		*part,
	       merge_args_t	*args)
{
    item_line_t	*list[8];
    unsigned	i;
    item_line_t	*pseudo_free;
    byte	*begin;
    byte	*ret;
    /* items in a subline, and the remaining items in the last subline */
    const int	item_size = sort->item_size;	/* would be ITEM_SIZE(sort) */
						/* if this file has ptr sort */
						/* and record sort versions */
    const int	sub_items = sort->sub_items;
    const int	n_sub_lists = sort->n_sub_lists;
    /* space for one linelist worth of data + 6 (max size) eol's */
    byte	scratch[LL_BYTES + 7 * (sizeof(ovc_t) + MAXRECORDSORT)];

    ARENACHECK;
    /* if the number of records is an odd size.
     */
    if (n_recs != sort->ll_items)
    {
	/* simply init the items directly to the output list and insertion
	 * sort.  This will be ineffecient for relative large lists ( > 20),
	 * but there should only be one such odd size list in a sort, so the
	 * overall inefficiency should be nil.
	 * For a hashed sort init_item() is a nop- it returns null just to
	 * catch accidential references.
	 */
	ret = (*sort->init_item)(sort, n_recs, in_rec, out_list->item, n_recs, part);
	(*sort->insert_sort)(sort, &args->recode, out_list->item,
				 item_math(out_list->item, n_recs * item_size));
	return (ret);
    }
    
    /* generate initial items in the scratch buffer, inserting a blank
     * every sub_items places (for an eol item).  This will produce 3 
     * groups (soon to be ordered lists) of length sub_items and one 
     * group of length rem_items.
     */
    ret = (*sort->init_item)(sort, sort->ll_items, in_rec, (item_t *) scratch,
			     sub_items, part);

    /* insertion sort the 4 groups of items into (ordered) lists.
     * The temporary 'lines' need to be fully pointer aligned, so that the
     * record and nextline pointers can be fetched without a trap.
     */
    begin = ROUND_UP(scratch, sizeof(item_line_t *));
    for (i = 0; i != n_sub_lists; i++)
    {
	(*sort->insert_sort)(sort, &args->recode,
			     (item_t *) begin,
			     item_math(begin, sub_items * item_size));
	list[i] = (item_line_t *) begin;
	begin += sub_items * item_size + NULL_ITEM_SIZE;
    }
    (*sort->insert_sort)(sort, &args->recode, (item_t *) begin,
			       item_math(begin, sort->rem_items * item_size));
    list[i] = (item_line_t *) begin;

    /* merge the four lists directly into the output list
     */
    pseudo_free = out_list;
    args->n_inputs = n_sub_lists + 1;
    args->in_list = list;
    (*sort->merge_line)(sort, args, &pseudo_free, NULL);

    return (ret);
}

/* GEN_N_LINES - generate a multiple-line list of n_recs items from records.
 */
byte *gen_n_lines(sort_t	*sort,
		  unsigned	n_recs, 
		  byte		*in_rec,
		  item_line_t	*out_list,
		  part_t	*part,
		  merge_args_t	*args,
		  int		n_lines)
{
    item_line_t	*list[GEN_WIDTH];
    int		n_lists = 0;
    unsigned	i;
    unsigned	scratch_pos = 0;
    item_line_t	*pseudo_free;
    byte	*begin;
    byte	*ret;
    /* items in a subline, and the remaining items in the last subline */
    const int	item_size = sort->item_size;	/* would be ITEM_SIZE(sort) */
						/* if this file has ptr sort */
						/* and record sort versions */
    const int	sub_items = sort->sub_items;
    const int	n_sub_lists = sort->n_sub_lists;
    const int	sub_size = sub_items * item_size;
    const int	rem_size = sort->rem_items * item_size;
    /* space for several linelists worth of data, each with (max size) eol's */
    byte	scratch[MAX_GEN_N_LINES][LL_BYTES + 4 * (sizeof(ovc_t) + MAXRECORDSORT)];

    ARENACHECK;
    /* if the number of records is an odd size.
     * generate initial items in the scratch buffer, inserting a blank
     * every sub_items places (for an eol item).  This will usually produce
     *		(sub_items + 1) * n_lines
     * groups (soon to be ordered lists) of length sub_items and one 
     * group of length rem_items. The FIRST gen_n_lines() of a sort may have a
     * funny (short and not a multiple of ll_items) size.
     */
    ret = in_rec;
    if (n_recs != sort->ll_items * n_lines)
    {
	i = n_recs % sort->ll_items;
	begin = ROUND_UP(scratch[0], sizeof(item_line_t *));
	ret = (*sort->init_item)(sort, i, in_rec, (item_t *) begin, i, part);
	(*sort->insert_sort)(sort, &args->recode,
				   (item_t *) begin,
				   item_math(begin, i * item_size));
	list[n_lists++] = (item_line_t *) begin;
	scratch_pos++;
    }
/* #pragma prefetch_ref= ret[0], kind=rd
#pragma prefetch_ref= ret[128], kind=rd
#pragma prefetch_ref= ret[256], kind=rd
#pragma prefetch_ref= ret[384], kind=rd
*/
    for (; scratch_pos != n_lines; scratch_pos++)
    {
	begin = ROUND_UP(scratch[scratch_pos], sizeof(item_line_t *));
	ret = (*sort->init_item)(sort, sort->ll_items, ret, (item_t *) begin, sub_items, part);
/* #pragma prefetch_ref= ret[0], kind=rd
#pragma prefetch_ref= ret[128], kind=rd
#pragma prefetch_ref= ret[256], kind=rd
#pragma prefetch_ref= ret[384], kind=rd
*/
	/* insertion sort the 4 groups of items into (ordered) lists.
	 */
	for (i = 0; i != n_sub_lists; i++)
	{
	    list[n_lists++] = (item_line_t *) begin;
	    (*sort->insert_sort)(sort, &args->recode,
				       (item_t *) begin,
				       item_math(begin, sub_size));
	    begin += sub_size + NULL_ITEM_SIZE;
	}
	list[n_lists++] = (item_line_t *) begin;
	(*sort->insert_sort)(sort, &args->recode, (item_t *) begin,
			     item_math(begin, rem_size));
    }

    pseudo_free = out_list;

    /* prehashing ovc sorts pass the same value both in_rec and out_list.
     * This used to work okay when free lines were linked at ll_item_space
     * bytes from the start of the previous line - it was a bit skanky, though.
     * Now that free lines are linked at the start of the line (differently
     * from how lines which contains data a linked, after the last item)
     * we need to relink the input list (in_rec) to be a free ll (out_list). Ugh
     */
#if defined(INITIAL_FREELIST)
    if (in_rec == (byte *) out_list)
    {
	item_line_t	*ll = out_list;
	item_t		*item;

	for (i = 1; i != n_lines; i++)
	{
	    item = item_math(ll, sort->ll_item_space);
	    *((item_line_t **) ll) = ITEM_NEXTLINE(item);
	    ll = ITEM_NEXTLINE(item);
	}
    }
#endif  /* INITIAL_FREELIST */

    /* merge the four lists directly into the output list
     */
    args->n_inputs = n_lists;
    args->in_list = list;
    (*sort->merge_line)(sort, args, &pseudo_free, NULL);

    return (ret);
}

/* SORT_N_LINES - sort a multiple-line line-list of n_recs radix items,
 *		  appending the resulting ovc items to the existing result list
 */
rad_line_t *sort_n_lines(sort_t	*sort,
			 unsigned	n_recs, 
			 rad_line_t	*in_list,
			 item_line_t	**ll_free,
			 merge_args_t	*args,
			 item_line_t	**headptr,
			 list_desc_t	*existing,
			 item_t		*prev_tail)
{
    item_line_t	*list[GEN_WIDTH + 1];
    int		n_lists = 0;
    unsigned	i;
    unsigned	scratch_pos = 0;
    rad_line_t	*next_in_line;
    byte	*begin;
    item_t	*curr;
    backward_sort_t sorter;
    /* items in a subline, and the remaining items in the last subline */
    const int	item_size = sort->item_size;	/* would be ITEM_SIZE(sort) */
						/* if this file has ptr sort */
						/* and record sort versions */
    const int	sub_items = sort->sub_items;
    const int	n_sub_lists = sort->n_sub_lists;
    const int	sub_size = sub_items * item_size;
    const int	rem_size = sort->rem_items * item_size;
    /* Space for several linelists worth of data, each with minimal eol's
     * The 6 limits n_sub_lists to 5
     */
    byte	scratch[MAX_GEN_N_LINES][LL_BYTES + 6 * NULL_ITEM_SIZE];

    sorter = (sort->method == SortMethRecord) ? backwards_sort_rs : backwards_sort;
    curr = (item_t *) FIRST_RADIX_ITEM(in_list);

    /*
     * Bizarre/Interesting hack to handle deletes and ovc correction in radix
     * sorts. If this sort_n_lines() call is to be the last for this
     * last_resort() invocation, and there are some records which have
     * already been appended to the output list, then the caller will pass in
     * a non-null prev_tail (== next_item - 1). The caller has removed next_item
     * from the existing output list by backing up one item size, and has put
     * and EOL_OVC after it, turning it into a single-item list.
     * Here we add it to the lists to be merged by the later merge_line() call.
     *
     * The merge_args' node_buf must be slightly larger than normal (GEN_WIDTH)
     * to allow for this extra tree node.
     *
     * XXX Verify that memory reservation isn't exceeded -- ASSERTS somewhere
     */
    if (prev_tail != NULL)
    {
	ASSERT(prev_tail == item_math(existing->next_item, 0/*-item_size*/));
	list[0] = (item_line_t *) prev_tail;
	n_lists++;
    }
    while (n_recs >= sort->ll_items)
    {
	/* insertion sort each subgroup of items into (ordered) lists.
	 * Copy the subgroup first, solely to allow for the eol_item
	 * appended by insert_sort and needed by merge_line.
	 */
	begin = ROUND_UP(scratch[scratch_pos], sizeof(item_line_t *));
	for (i = 0; i != n_sub_lists; i++)
	{
	    memmove(begin, curr, sub_size);
	    list[n_lists] = (item_line_t *) (*sorter)(sort, &args->recode, (item_t *) begin,
					   item_math(begin, sub_size));
	    begin += sub_size;
	    ((item_t *) begin)->ovc = EOL_OVC;
	    begin += NULL_ITEM_SIZE;
	    n_lists++;
	    curr = item_math(curr, sub_size);
	}

	memmove(begin, curr, rem_size);
	list[n_lists] = (item_line_t *) (*sorter)(sort, &args->recode,
						   (item_t *) begin,
						   item_math(begin, rem_size));
	item_math(begin, rem_size)->ovc = EOL_OVC;
	curr = item_math(curr, rem_size);
	ASSERT(FULL_RADIX_LINE(curr, sort->ll_item_space));
	n_lists++;

	/* This radix line's items have all been copied; push line on free list
	 */
#if defined(INITIAL_FREELIST)
	next_in_line = *((rad_line_t **) curr);
	*((item_line_t **) in_list) = *ll_free;
#else
	next_in_line = (rad_line_t *) ITEM_NEXTLINE(curr);
	ITEM_NEXTLINE(curr) = *ll_free;
#endif
#if defined(DEBUG2)
	memset((byte *) in_list + 4, 0xde, LL_BYTES - 116);
#endif
	*ll_free = (item_line_t *) in_list;
	in_list = next_in_line;


	curr = (item_t *) FIRST_RADIX_ITEM(in_list);
	scratch_pos++;
	n_recs -= sort->ll_items;
    }

    ASSERT(scratch_pos <= MAX_GEN_N_LINES);

    /* The LAST sort_n_lines() call of a last_resort() usually contains
     * a number of items which is not an exact multiple of ll_items.
     * It's slightly less efficient to sort the whole line at once, but
     * it doesn't happen often (we think) and so is insignificant.
     */
    if (n_recs != 0)
    {
	begin = ROUND_UP(scratch[scratch_pos], sizeof(item_line_t *));
	memmove(begin, curr, n_recs * item_size);
	list[n_lists] = (item_line_t *) (*sorter)(sort, &args->recode, (item_t *) begin,
				       item_math(begin, n_recs * item_size));
	item_math(begin, n_recs * item_size)->ovc = EOL_OVC;
	n_lists++;

#if defined(INITIAL_FREELIST)
	next_in_line = *((rad_line_t **) curr);
	*((item_line_t **) in_list) = *ll_free;
#else
	next_in_line = (rad_line_t *) LINE_NEXTLINE(in_list, sort->ll_item_space);
	LINE_NEXTLINE(in_list, sort->ll_item_space) = *ll_free;
#endif
#if defined(DEBUG2)
	memset((byte *) in_list + 4, 0xdd, LL_BYTES - 116);
#endif
	*ll_free = (item_line_t *) in_list;
    }

    /* merge the numerous small lists directly into the output list */
    args->n_inputs = n_lists;
    args->in_list = list;
    *headptr = (*sort->merge_line)(sort, args, ll_free, existing);

    return (next_in_line);
}

void check_free(sort_t *sort, item_line_t *ll_free)
{
    unsigned		i;
    const unsigned	item_space = sort->ll_item_space;

    for (i = 0; i < sort->n_ll_free; i++)
    {
	if (ll_free == NULL)
	    die("line %d on free list is NULL", i);
#if defined(INITIAL_FREELIST)
	ll_free = *((item_line_t **) ll_free);
#else
	ll_free = LINE_NEXTLINE(ll_free, item_space);
#endif
    }
    if (ll_free != NULL && !sort->any_deletes)
	die("line %d on free list is not NULL", i);
}

/*
** check_ll	- verify that a linelist is correctly ordered - die if not
**
*/
unsigned check_ll(sort_t *sort, CONST item_t *start)
{
    CONST item_t	*item;
    const int		item_size = sort->item_size;
    unsigned		count;
#if defined(DEBUG1)
    CONST item_t	*previtem = NULL;
    unsigned		do_cmp = sort->check_ll;
#endif

    for (count = 0, item = start; item->ovc != EOL_OVC; count++)
    {
	if (item->ovc == NULL_OVC)
	{
	    if (sort->two_pass && sort->output_phase)
		break;
	    item = ITEM_NEXTLINE(item)->item;
	}

#if defined(DEBUG1)
	if (do_cmp)
	    if (previtem != NULL)
		check_pair(sort, previtem, item);

	previtem = item;
#endif

	item = item_math(item, item_size);
    }

    return (count);
}

/*
** check_pair	- check that a ovc pair is ordered correctly
**		  (later?) verify that greater's ovc is correct
*/
void check_pair(sort_t *sort, CONST item_t *lesser, CONST item_t *greater)
{
    CONST byte	*next;
    CONST byte	*prev;
    int		cmp;

    if (lesser == NULL)
	return;

    ASSERT(lesser->ovc != NULL_OVC && lesser->ovc != EOL_OVC && greater->ovc != NULL_OVC && greater->ovc != EOL_OVC);

    if (sort->method == SortMethRecord)
    {
	prev = (byte *) &lesser->extra.data;
	next = (byte *) &greater->extra.data;
    }
    else
    {
	prev = lesser->rec;
	next = greater->rec;
    }

    cmp = compare_data(sort, prev, next);
    if (cmp > 0 || (cmp == 0 && sort->delete_dups))
    {
	fprintf(stderr, "\n** %s @ %08x %08x %08x"
			"\n** and          %08x %08x %08x\n",
		(cmp == 0 ? "duplicates" : "misordered"),
		lesser, lesser->ovc, lesser->extra.eov,
		greater, greater->ovc, greater->extra.eov);
	nsort_print_data(sort, prev, stderr, FALSE, FALSE);
	nsort_print_data(sort, next, stderr, FALSE, FALSE);
	die("check_ll/pair detected misordered records");
    }
#if 0
    if (cmp == 0 && ((lesser->ovc >> (sort->ovcsize * 8)) != 2) &&
		    ((greater->ovc >> (sort->ovcsize * 8)) != 2))
	die("check_ll/pair: equal records without one kept dup ovc:%x %x",
		lesser->ovc, greater->ovc);
#endif
}
