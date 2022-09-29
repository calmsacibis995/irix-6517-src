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
 *	funcs.c	- set a sort's method functions
 *
 *	$Ordinal-Id: funcs.c,v 1.5 1996/10/03 00:16:25 charles Exp $
 */
#ident	"$Revision: 1.1 $"

#include "otcsort.h"
#include "merge.h"

extern unsigned Merge12;

unsigned  merge_ptr_keep_out(sort_t          *sort,
			    merge_args_t    *args,
			    rec_out_t       *out_buf,
			    int             recs_to_skip,
			    int             recs_to_consume);

unsigned  merge_rec_keep_out(sort_t		*sort,
			     merge_args_t	*args,
			     rec_out_t		*out_buf,
			     int		recs_to_skip,
			     int		recs_to_consume);

void insert_sort_rs_12(sort_t *sort, recode_args_t *args, item_t *begin,
							  item_t *end);

unsigned nrecode_record(sort_t *sort, recode_args_t *args, byte *win_item,
			byte *curr_item);
unsigned nrecode_pointer(sort_t *sort, recode_args_t *args, item_t *win_item,
			item_t *curr_item);
unsigned nrecode_variable(sort_t *sort, recode_args_t *args, item_t *win_item,
			item_t *curr_item);
unsigned fixrecode_record(sort_t *sort, recode_args_t *args, byte *,byte *);
unsigned fixrecode_pointer(sort_t *sort, recode_args_t *args, item_t *,item_t *);

unsigned recode_pointer_cmp(sort_t *sort,
			recode_args_t *args,
			item_t *win_item,
			item_t *curr_item)
{
    recode_args_t	args_good = *args, args_new = *args;
    item_t		win_good = *win_item, win_new = *win_item;
    item_t		curr_good = *curr_item, curr_new = *curr_item;
    int			swap_good, swap_new;

    swap_good = nrecode_pointer(sort, &args_good, &win_good, &curr_good);
    if (sort->keycount == 1 && (sort->keys->type == typeFixedString ||
			        IsUnsignedType(sort->keys->type)))
	swap_new = fixrecode_pointer(sort, &args_new, &win_new, &curr_new);
    else
	swap_new = recode_pointer(sort, &args_new, &win_new, &curr_new);
    if (swap_good != swap_new)
	die("recode_cmp:swap wrong: %d bad %d", swap_good, swap_new);
    if (memcmp(&args_good, &args_new, offsetof(recode_args_t, n_hard)) != 0)
	die("recode_cmp:args wrong");
    if (memcmp(&win_good, &win_new, sizeof(win_good)) != 0)
	die("recode_cmp:win_item wrong");
    if (memcmp(&curr_good, &curr_new, sizeof(curr_good)) != 0)
	die("recode_cmp:curr_item wrong");

    *args = args_good;
    *win_item = win_good;
    *curr_item = curr_good;
    return (swap_good);
}

unsigned recode_variable_cmp(sort_t *sort,
			recode_args_t *args,
			item_t *win_item,
			item_t *curr_item)
{
    recode_args_t	args_good = *args, args_new = *args;
    item_t		win_good = *win_item, win_new = *win_item;
    item_t		curr_good = *curr_item, curr_new = *curr_item;
    int			swap_good, swap_new;

    swap_good = recode_variable(sort, &args_good, &win_good, &curr_good);
    swap_new = nrecode_variable(sort, &args_new, &win_new, &curr_new);
    if (swap_good != swap_new)
	die("recode_cmp:swap wrong: %d bad %d", swap_good, swap_new);
    if (memcmp(&args_good, &args_new, offsetof(recode_args_t, n_hard)) != 0)
	die("recode_cmp:args wrong");
    if (memcmp(&win_good, &win_new, sizeof(win_good)) != 0)
	die("recode_cmp:win_item wrong");
    if (memcmp(&curr_good, &curr_new, sizeof(curr_good)) != 0)
	die("recode_cmp:curr_item wrong");

    *args = args_good;
    *win_item = win_good;
    *curr_item = curr_good;
    return (swap_good);
}

unsigned recode_record_cmp(sort_t *sort,
		   recode_args_t *args,
		   byte *win_rec,
		   byte *curr_rec)
{
    recode_args_t	args_good = *args, args_new = *args;
    int			swap_good, swap_new;

    swap_good = recode_record(sort, &args_good, win_rec, curr_rec);
    if (sort->keycount == 1 && (sort->keys->type == typeFixedString ||
				IsUnsignedType(sort->keys->type)))
	swap_new = fixrecode_record(sort, &args_new, win_rec, curr_rec);
    else
	swap_new = nrecode_record(sort, &args_new, win_rec, curr_rec);
    if (swap_good != swap_new)
	die("recode_cmp:swap wrong: %d bad %d", swap_good, swap_new);
    if (memcmp(&args_good, &args_new, offsetof(recode_args_t, n_hard)) != 0)
	die("recode_cmp:args wrong", swap_good, swap_new);
    *args = args_good;
    return (swap_good);
}

/*
** execute_edit	- perform record editing operations
**
**	This handles fixed length fields and one special case of variable
**	length records - copying the whole record so that a derived field
**	may be appended to it.
*/
void execute_edits(byte *result, byte *record, int record_length, edit_t *edit)
{
    int		length;
    byte	*source;

    while ((length = edit->length) != 0)
    {
	if (edit->is_field)
	{
	    /* a field edit is currently needed only when adding derived fields
	     * we need to copy the entire orginal record and then add the
	     * new fields.  Later we'll need to find the length of the
	     * particular field and copy only that part.
	     */
	    if (edit->is_variable && length == EDIT_ACTUAL_LENGTH)
	    {
		length = record_length;
		ASSERT(edit->position == 0);
	    }
	    source = record + edit->position;
	}
	else
	    source = edit->source.constant;
	memmove(result, source, length);
	edit++;
	result += length;
    }
}


/*
 * execute_edit_8_4	- append a four byte '1' field to an eight byte record
 */
void execute_edit_8_4(byte *result, byte *record, int record_length, edit_t *edit)
{
    u4		t1, t2;

    t1 = ((u4 *) record)[0];
    t2 = ((u4 *) record)[1];
    ((u4 *) result)[2] = 1;
    ((u4 *) result)[0] = t1;
    ((u4 *) result)[1] = t2;
}

void insert_sort_rs_12_m32(sort_t *sort, recode_args_t *args, item_t *begin, item_t *end)
{
    if (item_math(begin, sort->item_size) != end)
	insert_sort_rs_12(sort, args, begin, end);
    else
    {
	begin->ovc = 0xfe000000 | (ulw(&begin->extra.data) >> 8);
	end->ovc = EOL_OVC;
	ITEM_NEXTLINE(end) = NULL;
    }
}

/*
 * calc_hash_4 - compute a hash value based on a single 4 byte key fixed field
 *
 * Hash function semi-based on Knuth chapter 6.4
 * This calc is duplicated in recode() and init_item(), and hash.c
 */
unsigned calc_hash_4(sort_t *sort, const byte *rec)
{
    unsigned	hash;

    ASSERT(sort->keycount == 1 && sort->keys->length == 4);
    hash = ulw(rec + sort->keys->position) * KNUTH_GOLDEN;
    hash = (hash << 1) | (hash >> 31);

    return (hash);
}

/*
 * calc_hash_fixed - compute a hash value based on a single fixed length key
 *
 * Hash function semi-based on Knuth chapter 6.4
 * This calc is duplicated in recode() and init_item(), and hash.c
 */
unsigned calc_hash_fixed(sort_t *sort, const byte *rec)
{
    unsigned	hash = 0;
    unsigned	word;
    const byte	*data = rec + sort->keys->position;
    const byte	*end = data + sort->keys->length;

    ASSERT(sort->keycount == 1 /*&& sort->keys->length != 4*/);

    word = ulw(data);
    while ((data += 4) < end)
    {
	hash ^= word * KNUTH_GOLDEN;
	word = ulw(data);
	hash = (hash << 1) | (hash >> 31);
    }
    word &= ~0 << ((data - end) << 3);
    hash ^= word * KNUTH_GOLDEN;
    hash = (hash << 1) | (hash >> 31);
    ASSERT(hash == calc_hash(sort, rec, 0));
    return (hash);
}

void fill_sort_funcs(sort_t *sort)
{
    unsigned	fix_recode_ok = getenv("NOFIXRECODE") == NULL &&
				sort->keycount == 1 &&
				(sort->keys->type == typeFixedString ||
				 IsUnsignedType(sort->keys->type));

    sort->copier = (copyfunc_t) memmove;

    if (sort->method == SortMethRecord)
    {
	if (sort->test)
	    sort->recode = (recode_t) recode_record_cmp;
	else if (sort->old_recode)
	    sort->recode = (recode_t) recode_record;
	else if (fix_recode_ok)
	    sort->recode = (recode_t) fixrecode_record;
	else
	    sort->recode = (recode_t) nrecode_record;

	sort->init_item = sort->radix
			  ? (init_item_t) init_radix_rs
			  : (sort->hash_table_size
			     ? (init_item_t) init_hash_items_rs
			     : init_item_rs);
        sort->insert_sort = insert_sort_rs;
	if (sort->edited_size == 12 && Merge12)
	{
	   sort->insert_sort = getenv("m32") ? insert_sort_rs_12_m32
					     : insert_sort_rs_12;
	   if (sort->insert_sort == insert_sort_rs_12_m32)
	   { sort->sub_items = 1;
	       sort->n_sub_lists = sort->ll_items - 1;
	       sort->rem_items = 1;
	    }
	   sort->merge_line = merge_rec_line_12;
	   sort->merge_skip = merge_rec_skip_12;
	   sort->merge_out = merge_rec_out_12;
	}
	else
	{
	    sort->merge_line = merge_rec_line;
	    sort->merge_skip = merge_rec_skip;
	    sort->merge_out = sort->any_deletes ? merge_rec_out
						: merge_rec_keep_out;
	}

	switch (sort->item_size)
	{
	  case  8: sort->itemcopier = (copyitemfunc_t) copy_8; break;
	  case 12: sort->itemcopier = (copyitemfunc_t) copy_12; break;
	  case 16: sort->itemcopier = (copyitemfunc_t) copy_16; break;
	  case 20: sort->itemcopier = (copyitemfunc_t) copy_20; break;
	  case 24: sort->itemcopier = (copyitemfunc_t) copy_24; break;
	  case 28: sort->itemcopier = (copyitemfunc_t) copy_28; break;
	  case 32: sort->itemcopier = (copyitemfunc_t) copy_32; break;
	  case 36: sort->itemcopier = (copyitemfunc_t) copy_36; break;
	  default: sort->itemcopier = (copyitemfunc_t) memmove; break;
	}

	if (sort->n_edits == 0)
	{
	    /* The input-records-array might not be aligned in an internal sort.
	     * If unaligned then we can't use a simple word-only copy function.
	     */
	    if (sort->internal && !IN.recs_aligned)
		sort->edit_func = (edit_func_t) memmove;
	    else switch (RECORD.length)
	    {
	      case  4: sort->edit_func = (edit_func_t) copy_4; break;
	      case  8: sort->edit_func = (edit_func_t) copy_8; break;
	      case 12: sort->edit_func = (edit_func_t) copy_12; break;
	      case 16: sort->edit_func = (edit_func_t) copy_16; break;
	      case 20: sort->edit_func = (edit_func_t) copy_20; break;
	      case 24: sort->edit_func = (edit_func_t) copy_24; break;
	      case 28: sort->edit_func = (edit_func_t) copy_28; break;
	      case 32: sort->edit_func = (edit_func_t) copy_32; break;
	      case 36: sort->edit_func = (edit_func_t) copy_36; break;
	      default: sort->edit_func = (edit_func_t) memmove; break;
	    }
	}
    }
    else
    {
	sort->merge_line = merge_ptr_line;
	sort->merge_skip = merge_ptr_skip;
	sort->merge_out = sort->any_deletes ? merge_ptr_out
					    : merge_ptr_keep_out;

	/* pointer sorts barely use the itemcopier function;
	 * only in the (non-pointer/record-specific) file radix.c
	 */
	sort->itemcopier = (copyitemfunc_t) copy_12;
	if (RECORD.flags & RECORD_FIXED)
	{
	    if (sort->test)
		sort->recode = recode_pointer_cmp;
	    else if (sort->old_recode)
		sort->recode = recode_pointer;
	    else if (fix_recode_ok)
	    	sort->recode = fixrecode_pointer;
	    else
		sort->recode = nrecode_pointer;

	    if (sort->radix)
		sort->init_item = (init_item_t) init_radix;
	    else if (sort->hash_table_size)
		sort->init_item = (init_item_t) init_hash_items;
	    else if (IN.ptr_input)
		sort->init_item = (init_item_t) init_item_int;
	    else
		sort->init_item = (init_item_t) init_item;
	}
	else
	{
	    if (sort->test)
		sort->recode = recode_variable_cmp;
	    else if (sort->old_recode)
		sort->recode = recode_variable;
	    else if (fix_recode_ok)
	    	sort->recode = fixrecode_pointer;
	    else
		sort->recode = nrecode_variable;

	    if (sort->radix)
		sort->init_item = (init_item_t) init_radix_vs;
	    else if (sort->hash_table_size)
		sort->init_item = (init_item_t) init_hash_items;
	    else if (IN.ptr_input)
		sort->init_item = (init_item_t) init_item_vs_int;
	    else
		sort->init_item = (init_item_t) init_item_vs;
	}
	sort->insert_sort = insert_sort;
    }

    if (RECORD.flags & RECORD_FIXED)
    {
	/* The input records might not be aligned in an internal sort.
	 * If unaligned then we can't use a simple word-only copy function.
	 */
	if (sort->internal && !IN.recs_aligned)
	    sort->copier = (copyfunc_t) memmove;
	else switch (sort->edited_size)
	{
	  case  4: sort->copier = (copyfunc_t) copy_4; break;
	  case  8: sort->copier = (copyfunc_t) copy_8; break;
	  case 12: sort->copier = (copyfunc_t) copy_12; break;
	  case 16: sort->copier = (copyfunc_t) copy_16; break;
	  case 20: sort->copier = (copyfunc_t) copy_20; break;
	  case 24: sort->copier = (copyfunc_t) copy_24; break;
	  case 28: sort->copier = (copyfunc_t) copy_28; break;
	  case 32: sort->copier = (copyfunc_t) copy_32; break;
	  case 36: sort->copier = (copyfunc_t) copy_36; break;
	  default: sort->copier = (copyfunc_t) memmove; break;
	}
    }

    if (sort->keycount != 1 || IsVariableType(sort->keys->type))
    {
	if (sort->check || sort->api_invoked)
	    sort->calc_hash = calc_hash;
	else
	    sort->calc_hash = calc_hash_fast;
    }
#if 1
    else if (sort->keys->length == 4)
	sort->calc_hash = (calc_hash_func_t) calc_hash_4;
#endif
    else
	sort->calc_hash = (calc_hash_func_t) calc_hash_fixed;

    if (sort->n_edits != 0)
    {
	if (sort->n_edits == 2 &&
	    RECORD.length == 8 &&
	    sort->edits[0].is_field &&
	    sort->edits[0].position == 0 &&
	    sort->edits[0].length == 8 &&
	    !sort->edits[1].is_field &&
	    sort->edits[1].length == 4 &&
	    ulw(sort->edits[1].source.constant) == 1
	   )
	    sort->edit_func = execute_edit_8_4;
	else
	    sort->edit_func = execute_edits;
    }

}
