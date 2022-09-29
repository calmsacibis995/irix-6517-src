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
 * Sort a single partition -
 *	This is compiled with differing #defines to generate
 *	record, fixed pointer, and variable length object files.
 *
 *	There are several independent variables for this file's functions:
 *		variable vs. fixed pointer vs. fixed record sort
 *
 *	The tasks include:
 *		building items and performing record editing, possibly
 *			inserting the items into hash tables.
 *		assigning initial ovc's
 *	
 *	Organizing these tasks for efficiency and clarity is tricky, because
 *	the cases which build a hash table defer initial ovc assignment
 *	until after all the items have been built.
 *
 * $Ordinal-Id: sortpart.c,v 1.17 1996/10/03 00:01:02 charles Exp $
 *
 * $Ordinal-Log: sortpart.c,v $
 * Revision 1.17  1996/10/03  00:01:02  charles
 * sort hashing in merge (non-radix) sorts
 *
 * Revision 1.15  1996/07/12  00:47:58  charles
 * Support for: 64-bit address space; completed radix sort implementation;
 * FixedDecimalString data type; bug fixes in skipping; reduced spurious errors
 * during parsing; exit w/o "die()'ing" if output write fails due to disk full
 * or broken pipe errors
 *
 * Revision 1.14  1996/05/15  00:51:17  charles
 * Initial Radix Sort support
 *
 * Revision 1.13  1996/03/26  23:04:08  charles
 * init_item() touches memory more densely.
 * gen_n_lines() coordination
 *
 * Revision 1.12  1996/02/14  21:47:14  charles
 * update copyright message
 *
 * Revision 1.11  1996/02/09  23:48:11  charles
 * Added ordinal copyright message.
 *
 * Revision 1.10  1995/12/21  19:48:54  charles
 * support hashing for varlen keys.  use var_hdr_extra
 *
 * Revision 1.9  1995/11/21  23:49:36  charles
 * change for vms-style length-prefix records: the length halfword
 * is the number of bytes of data which follows (i.e. min is 0, not 2)
 *
 * Revision 1.8  1995/11/09  17:38:27  chris
 * changes to write temporary files during the input phase
 *
 * Revision 1.7  1995/10/31  20:48:17  charles
 * Moved hashing statistics into the partition structure to
 * enable correct counts with multiple processors.
 * Recode counts moved into recode_args as well
 *
 * Revision 1.6  1995/10/11  18:28:14  charles
 * use inline MEMOVE and BACKMOVE for record sorts.
 * Avoid a divide in insert_sort by keeping a counter for args.win_id
 *
 * Revision 1.5  1995/10/04  23:53:51  charles
 * cleaned up hashing code. Put initial ovc code into insert_sort{,_rs}
 * Use sort->itemcopier, sort->copier
 *
 * Revision 1.4  1995/08/31  20:24:32  charles
 * fix bug with variable length edits.  check ll ordering only #if defined(DEBUG1)
 *
 */
#ident	"$Revision: 1.1 $"

#include	"otcsort.h"
#include	"ovc.h"
#include	"merge.h"
#include	"radix.h"

/*#define TESTBACKWARDSORT*/
void execute_edits(byte *result, byte *rec, int recsize, edit_t *edit);

 
/*	make items containing the information for records
 *
 *	There are many variations on this basic function, which takes a number
 * of records from the input buffer and build the items to be partition-sorted.
 *
 * All variations:
 *	Share these parameters: sort, rec, part
 *	Return: the updated rec, the starting point for the item-building call
 * The plain variations:
 *	Have these addtional parameters: n_recs, (starting) item, skip
 * The pre-hashing variations:
 *	Have these addtional parameters: (starting) item, hashtab, tail
 * The radix variations:
 *	Have these addtional parameters: n_recs, result (the output bucket)
 *
 * init_radix_*()	generate radix items for the next n_recs @ rec
 *			putting the resulting linelist into *ret,
 *	A radix item has a hash value (or random number, if doing a random
 * sampling sort) in the "ovc" field.
 *	
 *	
 */
#if defined(RADIX) && defined(VARIABLE_SORT)
byte *init_radix_vs(sort_t *sort, unsigned n_recs, byte *rec,
		    bucket_t *result, part_t *part)
#elif defined(RADIX) && defined(POINTER_SORT)
byte *init_radix(sort_t *sort, unsigned n_recs, byte *rec,
		    bucket_t *result, part_t *part)
#elif defined(RADIX)
byte *init_radix_rs(sort_t *sort, unsigned n_recs, byte *rec,
		    bucket_t *result, part_t *part)
#elif defined(HASHING) && defined(VARIABLE_SORT)
byte *build_hash_vs(sort_t *sort, byte *rec, item_t *item, part_t *part,
		    item_t **hashtab, item_t **tail)
#elif defined(HASHING) && defined(POINTER_SORT)
byte *build_hash(sort_t *sort, byte *rec, item_t *item, part_t *part,
		 item_t **hashtab, item_t **tail)
#elif defined(HASHING) 
byte *build_hash_rs(sort_t *sort, byte *rec, item_t *item, part_t *part,
		    item_t **hashtab, item_t **tail)
#elif defined(VARIABLE_SORT)
#if defined(INTERNAL_SORT)
byte **init_item_vs_int(sort_t *sort, unsigned n_recs, byte **input_ptrs,
				      item_t *item, unsigned skip, part_t *part)
#else
byte *init_item_vs(sort_t *sort, unsigned n_recs, byte *rec, item_t *item,
				 unsigned skip, part_t *part)
#endif /* INTERNAL_SORT */
#elif defined(POINTER_SORT)
#if defined(INTERNAL_SORT)
byte **init_item_int(sort_t *sort, unsigned n_recs, byte **input_ptrs,
				   item_t *item, unsigned skip, part_t *part)
#else
byte *init_item(sort_t *sort, unsigned n_recs, byte *rec, item_t *item,
			      unsigned skip, part_t *part)
#endif /* INTERNAL_SORT */
#else 
byte *init_item_rs(sort_t *sort, unsigned n_recs, byte *rec, item_t *item,
			unsigned skip, part_t *part)
#endif
{
    const int	item_size = ITEM_SIZE(sort);
    edit_func_t	editor = sort->edit_func;
    edit_t	*edits = sort->edits;
    byte	*edited_rec;
#if defined(INTERNAL_SORT)
    byte	*rec = *input_ptrs;
#endif
#if defined(RADIX)
    item_t	*item = NULL;
    item_t	*ll_end = NULL;
    item_line_t	*ll;
#elif defined(HASHING)
    item_t	*search;
    byte	*search_rec;
    item_t	**prev;
    unsigned	keep_item;
    int		items_left = sort->ll_items;
#endif /* HASHING */
#if defined(RADIX) || defined(HASHING)
#if !defined(MOVABLE_KEYS)
    int		first_position = sort->keys->position;
#endif
    int		i;
    byte	*data;
    keydesc_t	*key;
    unsigned	hash;
#else	/* just plain ovc sorting */
    unsigned	skip_left = 0;
#endif
#if defined(VARIABLE_SORT)
#if defined(INTERNAL_SORT)
    byte	*eorec;
#else
    u2		*lengths = part->lengths;
#endif
    const int	header_size = RECORD.var_hdr_extra;
    int		recsize;	
    int		edited_size;
#else
    const int	recsize = RECORD.length;	
#if defined(POINTER_SORT)
    const int	edited_size = RECORD.length + sort->edit_growth;
#endif
#endif /* else !VARIABLE */

#if defined(HASHING)
    /*
     * If there is a list of hashed lines already then append the
     * about-to-be-filled line to the end of the list.
     * Tail points to the last item in the previously hashed line.
     * Later one we'll update tail to point to the last item in this line
     * for use in a subsequent init_item_hash().
     */
    if (*tail != NULL)
	ITEM_NEXTLINE(*tail) = (item_line_t *) item;
    ASSERT(item == (item_t *) ROUND_DOWN(item, LL_BYTES));
#endif

    ARENACHECK;

#if defined(POINTER_SORT)
/*#pragma	prefetch_ref= rec[0], kind=rd*/
#endif
#pragma	prefetch_ref= item[0].ovc, kind=wr
#pragma	prefetch_ref= item[6].ovc, kind=wr

#if defined(HASHING)
    /* Go no further than the end of the partition, but usually hashing loops
     * will stop when it has added sort->ll_items items.
     */
    while (rec < part->rec_end)
#else
    while (n_recs-- != 0)
#endif
    {
#if defined(RADIX)
	/* Is there no more room in the current output line? get another if so
	 * N.B.: the following ll allocation code was dup'd from gen_list()
	 */
	if (item == ll_end)
	{
#if defined(RECORD_SORT)
	    if (rec >= (byte *) part->local_ll)
	    {
		/*
		** The rec pointer has advanced to the end of an aligned
		** sizeof(item_line_t) region of free space in the 'low'
		** part of the partition. Use this space for a line.
		*/
		ll = part->local_ll - 1;
		part->local_ll++;
	    }
	    else
	    {
		ll = part->refuge_next;
		part->refuge_next--;
		ASSERT(ll >= part->refuge_start);
	    }
#else	/* now POINTER_SORT */
	    /*
	     * Item linelists will all be in the refuge.
	     * For variable length pointer sorts we might be able to reuse
	     * one of the 'length' blocks, if all of its lengths have been used.
	     */
#if defined(VARIABLE_SORT)
	    ASSERT(lengths != NULL);
	    if (lengths <= (u2 *) part->length_ll)
	    {
		ll = part->length_ll;
		part->length_ll--;
		ASSERT(ll >= part->refuge_end);
	    }
	    else
#endif
	    {
		ll = part->refuge_next;
		part->refuge_next--;
	    }
	    ASSERT(ll >= part->refuge_start);
#endif	/* of else POINTER_SORT */

	    /* prepare to build items into the new line. remember it if this is
	     * the first line, append it to the existing linelist if not.
	     */
	    if (item == NULL)
		result->head = result->tail = (rad_line_t *) ll;
	    else
	    {
#if defined(INITIAL_FREELIST)
		*((rad_line_t **) item) = (rad_line_t *) ll;
#else
		ITEM_NEXTLINE(item) = ll;
#endif
		ASSERT(item_math(result->tail, sort->ll_item_space) == item);
		result->tail = (rad_line_t *) ll;
	    }
	    result->line_count++;
	    item = (item_t *) FIRST_RADIX_ITEM((rad_line_t *) ll);
	    ll_end = (item_t *) item_math(item, sort->ll_item_space);
	}
#endif	/* of RADIX */

#if defined(RECORD_SORT)
	/*
	 * Copy the record data into the item, possibly editing the record
	 * along the way.  If there is no editing, the editor is just
	 * memmove() or one of copy_4(), copy_8(), ...
	 */
	(*editor)(ITEM_REC(item), rec, recsize, edits);
# if 0	/* turn on when debugging custom editors */
	if (edits != NULL && editor != execute_edits)
	{
	    byte	temp[MAXRECORDSORT];

	    execute_edits(&temp, rec, recsize, edits);
	    if (memcmp(temp, ITEM_REC(item), sort->edited_size) != 0)
		die("init_item:new editor failed");
	}
#endif	/* of customer editor debugging */
# else	/* now POINTER_SORT */
# if defined(VARIABLE_SORT)
#  if defined(INTERNAL_SORT)
	if (RECORD.flags & RECORD_STREAM)
	{
	    eorec = fmemchr(rec + RECORD.minlength - 1, RECORD.delimiter,
			    MAXRECORDSIZE);
	    if (eorec == NULL)
	    {
		runtime_error(sort, NSORT_DELIM_MISSING,
				    input_ptrs,
				    "<none - internal sort>");
		break;		/* end of this partition */
	    }
	    recsize = 1 + (unsigned) (eorec - rec);
	}
	else
	{
	    recsize = 2 + ulh(rec);
	}
#  else
	recsize = *--lengths;
#  endif /* of !INTERNAL_SORT */

	/*
	 * for future use: if editing variable length records we need to
	 * add edit_growth to the item's length.
	 */
	item->extra.var_extra.var_length = recsize;
	recsize += header_size;
# endif /* of VARIABLE_SORT */
	if (edits == NULL)
	{
	    item->rec = rec;
#if defined(HASHING)
	    edited_rec = rec;
#endif
	}
        else
	{
	    /*
	    ** When editing in a pointer sort the new version of the record
	    ** may reside in two places:
	    ** (1) in the refuge area, after this partition's highest-addressed
	    **     refuge linelist.  For variable length records, this area is
	    **     after the partition's linelists and in front of its lengths.
	    ** (2) in the record area, reusing space which has recently been
	    **     made available by a previous editing operation which
	    **     copied the record into the refuge area as under (1).
	    */
#if defined(VARIABLE_SORT)
	    edited_size = recsize + sort->edit_growth;
#endif
#if 1	/* use all the refuge space first, then use the record area - slower */
	    edited_rec = part->refuge_edit - edited_size;
	    if (edited_rec >= (byte *) part->refuge_end)
#else	/* use the 'local' record area as soon as possible -- probably faster */
	    if (rec < part->local_edit + edited_size)
#endif
	    {
		/* no local space,  so use the refuge */
		edited_rec = part->refuge_edit - edited_size;
		ASSERT(edited_rec >= (byte *) part->refuge_end);
#if !defined(HASHING)
		/* If prehashing we don't really keep the record until after
		 * it is inserted. Non-hashing sorts always keep it.
		 */
		part->refuge_edit = edited_rec;
#endif
	    }
	    else
	    {
		edited_rec = part->local_edit;
#if !defined(HASHING)
		part->local_edit += edited_size;
#endif
	    }
	    item->rec = edited_rec;
	    (*editor)(edited_rec, rec, recsize, edits);
	}
#endif	/* of pointer sort cases */

#if defined(INTERNAL_SORT)
	rec = *++input_ptrs;
#else
	rec += recsize;
#endif

#if defined(POINTER_SORT)		/* prefetch next record in ptr sorts */
#pragma	prefetch_ref= rec[0], kind=rd	/* so it'll be avail for insert_sort */
#endif

#if defined(RADIX) || defined(HASHING)	/* cases which overload the ovc field */
#if defined(RECORD_SORT) || defined(RADIX)
	edited_rec = ITEM_REC(item);
#else
	ASSERT(edited_rec == ITEM_REC(item));
#endif
	/*
	 * Compute hash 
	 */
	/* The length parameter is not used by special fixed-length calculators
	 * so we avoid calculating and passing it.
	 */
#if defined(RADIX)
	if (sort->compute_hash)
	{
#endif

# if defined(VARIABLE_SORT)
	hash = (*sort->calc_hash)(sort, edited_rec, header_size + item->extra.var_extra.var_length);
# else
	hash = (*((unsigned (*)(sort_t*,byte *)) sort->calc_hash))(sort, edited_rec);
# endif

#if defined(RADIX)
	/* Radix pointer sorts use all 32 bits of the hash value,
	 * Radix records sorts use only 31: clear out the bottom bit so
	 * that it won't be used to incorrectly order adjacent hash values? XXX
	 */
#if defined(POINTER_SORT)
	item->ovc = hash;
#else
	item->ovc = hash & ~1;
#endif
	}
	else	/* not compute_hash, use first 24(rec)/32(ptr) bits of key */
	{
	    ovc_init_t		*init;
	    byte		*ovcp;
	    int			oplen;
	    unsigned		val;
	    int			past_eor;

	    /*	item->ovc = BUILD_OVC(1, 0);	No offset for radix dispersion*/
#if defined(RECORD_SORT)		/* ensure that last 8 bits are zero */
	    item->ovc = 0;
#endif
	    ovcp = (byte *) &item->ovc;
	    init = sort->ovc_init;

	    do 
	    {
		oplen = init->length;
		
		if (init->op == opPad)
		    val = 0;
		else
		{
		    val = ulw(edited_rec + init->position);
		    switch (init->op)
		    {
		      case opFloat:
			if ((val << 1) == 0)		/* pos or neg 0? */
			    val = SIGN_BIT;		/* -> normalized 0.0 */
			else if (val & SIGN_BIT)	/* negative? */
			    val = ~val;			/* -> complement bits */
			else				/* else it's positive */
			    val ^= SIGN_BIT;		/* -> toggle sign */
			ASSERT(init[1].op == opEof);
			break;

		      case opDouble:
			if ((val << 1) == 0 &&		/* pos or neg 0? */
			    ulw(edited_rec + init->position + 4) == 0)
			    val = SIGN_BIT;		/* -> normalized 0.0 */
			else if (val & SIGN_BIT)	/* negative? */
			    val = ~val; 		/* -> complement bits */
			else				/* else it's positive */
			    val ^= SIGN_BIT;		/* -> toggle sign */
			ASSERT(init[1].op == opEof);
			break;

		      case opDecimal:
			val = next_decimal_value(edited_rec + init->position,
						 init->max_length,
						 init->max_length,
						 0,
						 OVC_DEC_DIGITS /* 7 or 9 */
						);
			break;

#if defined(VARIABLE_SORT)	/* record sorts can have only fixlen keys */
		      case opVariable:	
			/*
			 * Look for the delimiter in each of the possible positions
			 * in the value (3 for record sorts, all 4 for pointers).
			 * If a delimiter is seen replace that character and all
			 * the remaining ones with the key's pad. init->pad
			 * contains the key's pad duplicated in all 4 positions.
			 */
			if (!(sort->record.flags & RECORD_FIXED) &&
			    (past_eor = init->position + oplen -
			     (item->extra.var_extra.var_length + header_size) > 0))
			{
			    /*
			     * At least some of 'val' contains bytes from beyond
			     * the actual end of the record. Set the `too far'
			     * bytes to the key pad - perhaps all of the value
			     * or maybe just byte #0, bytes #0 & 1, or 0, 1, & 2.
			     */
			    if (past_eor >= 4)
			    {
				val = init->pad;
				break;
			    }
			    else
			    {
				/*
				 * The topmost 1, 2, or 3 bytes of val are valid.
				 * Set the first byte after the valid data to the
				 * field delimiter, just in case the field isn't
				 * properly terminated (and ends at eor instead).
				 */
				val = (val & ~((1 << (past_eor << 3)) - 1)) |
				      (init->field_delim << ((past_eor << 3) - 8));
			    }
			}
			if ((val >> 24) == init->field_delim)
			    val = init->pad;		/* value is all pad */
			else if (((val >> 16) & 0xff) == init->field_delim)
			    val = (val & 0xff000000) |	/* one char in str */
				  (init->pad >> 8);		/* three bytes pad */
			else if (((val >> 8) & 0xff) == init->field_delim)
			    val = (val & 0xffff0000) |	/* two chars of str */
				  (init->pad >> 16);	/* two bytes pad */
			else if ((val & 0xff) == init->field_delim)
			    val = (val & 0xffffff00) |	/* three chars in str */
				  (init->pad & 0xff);	/* one byte of pad */
			/* oplen always fills out the ovc for varstrs, so it
			 * will always be the last key in the initial ovc. 
			 */
			ASSERT(init[1].op == opEof);
			break;
#endif /* VARIABLE_SORT */
		    } 

		    /*
		     * Toggle sign bit for signed binary integers;
		     * Toggle all bits if this is an order_by:descending key.
		     */
		    val ^= init->xormask;
		}

		switch (oplen)
		{
#if defined(POINTER_SORT)
		  case 4: usw(ovcp, val);
			  goto item_done;
#endif
		  case 3: ush(ovcp, val >> 16);	/* top two bytes */
			  ovcp[2] = val >> 8;	/* next byte after the above */
#if defined(RECORD_SORT)
			  goto item_done;
#else
			  break;
#endif

		  case 2: ush(ovcp, val >> 16);
			  break;

		  case 1: ovcp[0] = val >> 24;
			  break;
		}
		ovcp += oplen;
		init++;
	    } while (init->op != opEof);
	} /* of assigning a 'data' ovc */
item_done:

#if defined(POINTER_SORT)
	if (edits)
	{
	    if (edited_rec != part->local_edit - edited_size)
		ASSERT(part->refuge_edit == edited_rec);
	}
#endif /* POINTER_SORT */

	item = item_math(item, item_size);

#else	/* no longer RADIX, now (pre-)HASHING */

	/* Fold bits in the upper part of the hash into the subscripting part
	 * It needs to be done at most twice because the minimum hash table
	 * size is 12 bits, and the hash size is 32 bits, < 36 (3 * 12).
	 */
	if ((hash >> sort->hash_table_bits) != 0)
	{
	    hash = (hash & (sort->hash_table_size - 1)) ^
		   (hash >> sort->hash_table_bits);
	    if ((hash >> sort->hash_table_bits) != 0)
	    {
		hash = (hash & (sort->hash_table_size - 1)) ^
		       (hash >> sort->hash_table_bits);
		ASSERT((hash >> sort->hash_table_bits) == 0); 
	    }
	}
	/*hash &= (1 << sort->hash_table_bits) - 1;*/

#if defined(DEBUG1)
	if (Print_task > 2)
	{
	    fprintf(stderr, "%08x Hashes to %04x\t", item, hash);
	    nsort_print_data(sort, edited_rec, stderr, FALSE, FALSE);
	}
#endif

	/*
	 * Look for the keys in the hash table
	 */
	for (prev = &hashtab[hash], i = 0, keep_item = TRUE;
	     (search = *prev) != NULL;
	     prev = (item_t **) &search->ovc, i++)
	{
	    if (i == 10)
	    {
		part->hash_stopsearches++;
		break;
	    }
	    search_rec = ITEM_REC(search);
#if !defined(MOVABLE_KEYS)
	    if (edited_rec[first_position] != search_rec[first_position])
		continue;
#endif
	    for (key = sort->keys; key->type != typeEOF; key++)
	    {
#if defined(VARIABLE_SORT)
		if (IsVariableType(key->type))
		{
		    if (compare_delim_string(key,
					     edited_rec,
					     search_rec,
					     header_size +
					     item->extra.var_extra.var_length,
					     header_size +
					     search->extra.var_extra.var_length)
			 != 0)
			goto not_a_match;
		}
		else
#endif

		    if (memcmp(edited_rec + key->position,
			       search_rec + key->position,
			       key->length) != 0)
			goto not_a_match;/* break out to try the next record */
	    }			   	 /* in the overflow chain, below */


#if defined(DEBUG1)
	    if (Print_task > 2)
		fprintf(stderr, "Existing key %x matches it: ", search);
#endif
	    /*
	     * Key fields are equal.  Can the new item be summarized
	     * with the one in the hash table?
	     */
#if defined(POINTER_SORT)
	    if (sort->delete_dups || summarize_pointer(sort, search, item))
#else
	    if (sort->delete_dups || summarize_record(sort, ITEM_REC(search),
							    ITEM_REC(item)))
#endif
	    {
		/*
		 * Key fields are equal, and the summarize succeeded.
		 * Forget about this item.
		 */
		keep_item = FALSE;
		part->hash_deletes++;
#if defined(DEBUG1)
		if (Print_task > 2)
		    fprintf(stderr, "summation succeeded -- not keeping\n");
#endif
	    }
	    else
	    {
		/*
		 * Summarize would have overflowed. Delete this item from
		 * the hash table, since this loop only looks at the first
		 * occurrence of the key.  We'll insert the current item
		 * as this key's new representative record.
		 */
#if defined(DEBUG1)
		if (Print_task > 2)
		    fprintf(stderr, "summation overflowed -- deleting existing\n");
#endif
		*prev = (item_t *) search->ovc;	/* XXX FAILS w/ 64-bit ptrs */
		part->hash_replaces++;
	    }
	    break;

not_a_match: ;
	}

	if (keep_item)
	{
	    /* The key wasn't present or it couldn't be summarized
	     * or there were at least 10 collisions on the hash value
	     * and so we gave up on trying to see if it was already there.
	     * Add this item to the hash table.
	     */
	    item->ovc = (unsigned) hashtab[hash];
	    hashtab[hash] = item;
#if defined(DEBUG1)
	    if (Print_task > 2)
		fprintf(stderr, "Adding item %x to hash table\n", item);
#endif

#if defined(POINTER_SORT)
	    if (edits)
	    {
		if (edited_rec == part->local_edit)
		    part->local_edit = edited_rec + edited_size;
		else
		{
		    ASSERT(part->refuge_edit == edited_rec + edited_size);
		    part->refuge_edit = edited_rec; /* i.e. -= edited_size */
		}
	    }
#endif /* POINTER_SORT */

	    item = item_math(item, item_size);

	    if (--items_left == 0)
		break;
	}
#endif	/* of (pre-)HASHING */

#else	/* neither RADIX nor (pre-)HASHING this sort */
	item = item_math(item, item_size);

	if (++skip_left == skip)	/* skip to allow for an eol item */
	{
	    item = item_math(item, NULL_ITEM_SIZE);
	    skip_left = 0;
	}
#endif	/* of non-RADIX and non-HASHING */
    }
    ARENACHECK;

    /*
     * Update the tail pointer to this (the end of line marker) item
     */
#if defined(HASHING)
    item->ovc = EOL_OVC;
    ITEM_NEXTLINE(item) = NULL;
    *tail = item;
#elif defined(RADIX)
    ASSERT(result->tail == (rad_line_t *) ll);
    ASSERT(result->tail == RADIX_LINE_START(item) || FULL_RADIX_LINE(item, sort->ll_item_space));
    *((rad_line_t **) item) = (rad_line_t *) 0xdeadbeef;
    result->next_item = (byte *) item;
#endif

#if defined(VARIABLE_SORT) && !defined(INTERNAL_SORT)
    part->lengths = lengths;
#endif

#if defined(INTERNAL_SORT)
    return (input_ptrs);
#else
    return (rec);
#endif
}

#if defined(HASHING) && !defined(VARIABLE_SORT)

/*
 * init_hash_items	- insert space for eol items while copying hashed items
 *
 */
#if defined(POINTER_SORT)
byte *init_hash_items(sort_t *sort, unsigned n_recs, item_t *item, item_t *scratch, unsigned skip, part_t *part)
#else
byte *init_hash_items_rs(sort_t *sort, unsigned n_recs, item_t *item, item_t *scratch, unsigned skip, part_t *part)
#endif
{
    const int	item_size = ITEM_SIZE(sort);
    int		copy;

    while (n_recs != 0)
    {
	copy = n_recs;
	if (n_recs >= skip)
	    copy = skip;
	memmove/*MEMMOVE*/(scratch, item, copy * item_size);
	item = item_math(item, copy * item_size);

	if ((n_recs -= copy) == 0)
	    break;
	/* advance past the items just copied, plus more space for an eol item
	 */
	scratch = item_math(scratch, copy * item_size + NULL_ITEM_SIZE);
    }

    ASSERT(item->ovc == EOL_OVC);
    return ((byte *) ITEM_NEXTLINE(item));
}
#endif

#if !defined(HASHING) && !defined(VARIABLE_SORT) && !defined(RADIX) && !defined(INTERNAL_SORT)
#if defined(POINTER_SORT)
void insert_sort(sort_t *sort, recode_args_t *args, item_t *begin, item_t *end)
#else
void insert_sort_rs(sort_t *sort, recode_args_t *args, item_t *begin, item_t *end)
#endif
{
    item_t		*new, *insert, *end_insert;
    unsigned		new_ovc, in_ovc;
    byte		*nextrec;
    byte		*new_rec;
    unsigned		swap;
    const unsigned	item_size = ITEM_SIZE(sort);
    ovc_init_t		*init;
    byte		*ovcp;
    int			oplen;
    unsigned		val;
    int			new_id;
#if defined(TESTBACKWARDSORT)
    byte		backbuf[1024];
    int			backsize;
    byte		reccopy[LL_ITEMS][MAXRECORDSIZE];
    item_t		*backbegin;
#endif
#if defined(POINTER_SORT)
    item_t		*shift;
    byte		*rec;
    unsigned		ovc;
    unsigned		eov, new_eov;
    int			past_eor;
    int			var_hdr_extra = RECORD.var_hdr_extra;
#else
    byte		rec_buf[MAXRECORDSORT];
    const unsigned	length = (unsigned) ROUND_UP_NUM(sort->edited_size, sizeof(u4));
#endif

#if defined(TESTBACKWARDSORT)
    backsize = (byte *) end - (byte *) begin;
    memcpy(backbuf, begin, backsize);
#if defined(POINTER_SORT)
    if (sort->n_summarized)
    {
	int i;

	for (i = 0; begin + i < end; i++)
	{
	    memmove(&reccopy[i][0], begin[i].rec, RECORD.maxlength);
	    ((item_t *) backbuf)[i].rec = &reccopy[i][0];
	}
    }

    backbegin = backwards_sort(sort, args, (item_t *) backbuf,
					   (item_t *) (backbuf + backsize));
#else
    backbegin = backwards_sort_rs(sort, args, (item_t *) backbuf,
					      (item_t *) (backbuf + backsize));
#endif /* of else RECORD_SORT */

    if (backbegin != begin)		/* i.e.: duplicates were deleted */
	memmove(begin, backbegin,
		       (byte *) end - (byte *) backbegin + NULL_ITEM_SIZE);
#endif /* TESTBACKWARDSORT */

#if defined(POINTER_SORT)
    nextrec = ITEM_REC(begin);
#pragma	prefetch_ref= nextrec[0], kind=rd
#else
#pragma prefetch_ref= begin[0].ovc, kind=rd
#pragma prefetch_ref= end[0].ovc, kind=rd
#endif

    args->curr_id = 0;

    for (new = end_insert = begin, new_id = 0;
	 new < end;
	 new = item_math(new, item_size), new_id++)
    {
	/* insert the new item in the list that precedes it.  this is best
	 * thought of as merging a 1-item list with an n-item list.
	 * Assign the items' initial offset value codes before sorting
	 */
#if defined(POINTER_SORT)
	new_rec = nextrec;
	nextrec = ITEM_REC(item_math(new, item_size));
#pragma	prefetch_ref= nextrec[0], kind=rd
#else
	new_rec = ITEM_REC(new);
#endif

	if (sort->compute_hash)
	{
#if defined(VARIABLE_SORT)
	    val = (*sort->calc_hash)(sort, new_rec, RECORD.var_hdr_extra +
					      new->extra.var_extra.var_length);
#else
	    val = (*((calc_hash_fixed_func_t) sort->calc_hash))(sort, new_rec);
#endif
#if defined(POINTER_SORT)
	    new->extra.var_extra.var_eov = val & 0xffff;
	    new_ovc = BUILD_OVC(HASH_INITIAL_OFFSET, val >> 16);
#else
	    new_ovc = BUILD_OVC(HASH_INITIAL_OFFSET, val >> 1);
#endif
	    new->ovc = new_ovc;
	}
	else
	{
	  new->ovc = BUILD_OVC(1, 0);
	  ovcp = sizeof(ovcoff_t) + (byte *) &new->ovc;
	  init = sort->ovc_init;

	  do 
	  {
	    oplen = init->length;
	    
	    if (init->op == opPad)
		val = 0;
	    else
	    {
		val = ulw(new_rec + init->position);
		switch (init->op)
		{
		  case opFloat:
		    if ((val << 1) == 0)		/* pos or neg 0? */
			val = SIGN_BIT;			/* -> normalized 0.0 */
		    else if (val & SIGN_BIT)		/* negative? */
			val = ~val;			/* -> complement bits */
		    else				/* else it's positive */
			val ^= SIGN_BIT;		/* -> toggle sign */
		    ASSERT(init[1].op == opEof);
		    break;

		  case opDouble:
		    if ((val << 1) == 0 &&		/* pos or neg 0? */
			ulw(new_rec + init->position + 4) == 0)
			val = SIGN_BIT;			/* -> normalized 0.0 */
		    else if (val & SIGN_BIT)		/* negative? */
			val = ~val; 			/* -> complement bits */
		    else				/* else it's positive */
			val ^= SIGN_BIT;		/* -> toggle sign */
		    ASSERT(init[1].op == opEof);
		    break;

		  case opDecimal:
		    val = next_decimal_value(new_rec + init->position,
					     init->max_length,
					     init->max_length,
					     0,
					     OVC_DEC_DIGITS /* 7 or 9 */
					    );
		    break;

#if 0
		  case opDelimDecimal:
		    val = next_decimal_value(new_rec + init->position,
				     get_delim_length(new_rec + init->position,
						      sort->keys + init->keyno,
						      TILL_EOREC),
					     init->max_length,
					     0,
					     OVC_DEC_DIGITS /* 7 or 9 */
					    );
		    break;
#endif

#if defined(POINTER_SORT)	/* record sorts can have only fixlen keys */
		  case opVariable:	
		    /*
		     * Look for the delimiter in each of the possible positions
		     * in the value (3 for record sorts, all 4 for pointers).
		     * If a delimiter is seen replace that character and all
		     * the remaining ones with the key's pad. init->pad
		     * contains the key's pad duplicated in all 4 positions.
		     */
		    if (!(RECORD.flags & RECORD_FIXED) &&
			(past_eor = init->position + oplen -
			 (new->extra.var_extra.var_length + var_hdr_extra) > 0))
		    {
			/*
			 * At least some of 'val' contains bytes from beyond
			 * the actual end of the record. Set the `too far'
			 * bytes to the key pad - perhaps all of the value
			 * or maybe just byte #0, bytes #0 & 1, or 0, 1, & 2.
			 */
			if (past_eor >= 4)
			{
			    val = init->pad;
			    break;
			}
			else
			{
			    /*
			     * The topmost 1, 2, or 3 bytes of val are valid.
			     * Set the first byte after the valid data to the
			     * field delimiter, just in case the field isn't
			     * properly terminated (and ends at eor instead).
			     */
			    val = (val & ~((1 << (past_eor << 3)) - 1)) |
				  (init->field_delim << ((past_eor << 3) - 8));
			}
		    }
		    if ((val >> 24) == init->field_delim)
			val = init->pad;		/* value is all pad */
		    else if (((val >> 16) & 0xff) == init->field_delim)
			val = (val & 0xff000000) |	/* one char in str */
			      (init->pad >> 8);		/* three bytes pad */
		    else if (((val >> 8) & 0xff) == init->field_delim)
			val = (val & 0xffff0000) |	/* two chars of str */
			      (init->pad >> 16);	/* two bytes pad */
		    else if ((val & 0xff) == init->field_delim)
			val = (val & 0xffffff00) |	/* three chars in str */
			      (init->pad & 0xff);	/* one byte of pad */
		    /* oplen always fills out the ovc for varstrs, so it
		     * will always be the last key in the initial ovc. 
		     */
		    ASSERT(init[1].op == opEof);
		    break;
#endif /* POINTER_SORT */
		} 

		/*
		 * Toggle sign bit for signed binary integers;
		 * Toggle all bits if this is an order_by:descending key.
		 */
		val ^= init->xormask;
	    }

	    switch (oplen)
	    {
#if defined(POINTER_SORT)
	      case 4: usw(ovcp, val);
		      goto item_done;
#endif
	      case 3: ush(ovcp, val >> 16);	/* top two bytes */
		      ovcp[2] = val >> 8;	/* next byte after the above */
#if defined(RECORD_SORT)
		      goto item_done;
#else
		      break;
#endif

	      case 2: ush(ovcp, val >> 16);
		      break;

	      case 1: ovcp[0] = val >> 24;
		      break;
	    }
	    ovcp += oplen;
	    init++;
	  } while (init->op != opEof);
item_done:

	  new_ovc = new->ovc;
	}

#if defined(POINTER_SORT)
	new_eov = new->extra.eov;
#endif

	insert = item_math(begin, -(int) item_size);
	for (;;)
	{
	    in_ovc = item_math(insert, item_size)->ovc;	/* optimizes well for */
	    insert = item_math(insert, item_size);	/* both ptr & rec */

	    if (insert == end_insert)
		goto do_insert;
	    if (new_ovc > in_ovc)
		continue;
	    if (new_ovc != in_ovc)/* same but faster than (new_ovc < in_ovc) */
		break;

	    /* new_ovc is equal to in_ovc, call recode() to resolve comparison
	     * and assign a new ovc to the loser.
	     */
#if 0
	    args->win_id = ((byte *) new - (byte *) begin) / item_size;
#else
	    args->win_id = new_id;
	    ASSERT(((byte *) new - (byte *) begin) / item_size == new_id);
#endif

#if defined(RECORD_SORT)
	    args->win_ovc = new_ovc;
	    args->curr_ovc = in_ovc;

	    swap = RECODE(sort, args, new, insert);
	    new_ovc = args->win_ovc;
	    in_ovc = args->curr_ovc;
	    new->ovc = new_ovc;
	    insert->ovc = in_ovc;
#else
	    ASSERT(new->ovc == new_ovc);
	    ASSERT(insert->ovc == in_ovc);
	    swap = RECODE(sort, args, new, insert);
	    new_ovc = new->ovc;
	    in_ovc = insert->ovc;
	    new_eov = new->extra.eov;
#endif

	    /* If new item is lower than the insert item, the insert
	     * item has been assigned a new ovc.  Break out to the
	     * insertion code to insert new item in front of insert item.
	     */
	    if (swap == 0)
		break;
	    
	    /* If we need to delete the new rec, do so.
	     */
	    if ((new_ovc >> VALUE_BITS) == (DEL_DUP_OVC >> VALUE_BITS))
		goto delete_rec;
	    
	    /* The insert item is lower, and the new item has been
	     * assigned a new ovc.  Continue search for proper insertion
	     * point of new item.
	     */
	}

	/* if need to shift items to make room for new item.
	 */
	if (insert != end_insert)
	{
	    /* insert the "new" list item at "insert" by shifting up one place
	     * (away from begin) all items from insert to new-1.
	     */
#if defined(POINTER_SORT)
	    ASSERT(new_rec == new->rec);
	    ASSERT(new_eov == new->extra.eov);
	    shift = new;
	    for (;;)
	    {
		ovc = (shift - 1)->ovc;
		rec = (shift - 1)->rec;
		eov = (shift - 1)->extra.eov;
		shift--;
		(shift + 1)->ovc = ovc;
		(shift + 1)->rec = rec;
		(shift + 1)->extra.eov = eov;
		if (shift == insert)
		    break;
	    }
#else
	    MEMMOVE/*(*sort->copier)*/(rec_buf, new_rec, length);
	    new_rec = rec_buf;

	    MEMMOVE/*BACKMOVE*/(item_math(insert, item_size), insert, (char *) new - (char *) insert);

#endif
	}
	
	/* if the insert point isn't where the new record is already, move
	 * the record to the insert point.
	 */
do_insert:
	end_insert = item_math(end_insert, item_size);
	if (insert != new)
	{
#if defined(POINTER_SORT)
	    insert->extra.eov = new_eov;
	    insert->rec = new_rec;
#else
	    /* The source address may not be word aligned, so we can't use
	     * the optimized inline word-at-a-time copy macros.
	     */
	    (*sort->copier)(ITEM_REC(insert), new_rec, length);
#endif
	    insert->ovc = new_ovc;
	}

delete_rec:
	continue;
    }

    end_insert->ovc = EOL_OVC;
    ITEM_NEXTLINE(end_insert) = NULL;

#if defined(DEBUG1)
    {
	int found = check_ll(sort, begin);
	int expected = (int) ((byte *) end - (byte *) begin) / item_size;
	
	if (found != expected && !sort->delete_dups && !sort->summarize)
	    die("insert_sort:%d records missing", expected - found);

#if defined(TESTBACKWARDSORT)
	for (new = (item_t *) backbuf;
	     begin != end_insert;
	     begin = item_math(begin, item_size), new = item_math(new, item_size))
	{
	    if ((begin->ovc != new->ovc || begin->extra.eov != new->extra.eov)&&
		(begin->ovc >> VALUE_BITS) != (KEPT_DUP_OVC >> VALUE_BITS))
		die("insert_sort:backwards has different ovc/eov: %x:%x %x:%x",
			begin, begin->ovc, new, new->ovc);
#if defined(VARIABLE_SORT)
	    if (memcmp(begin->rec,
		       new->rec,
		       var_hdr_extra + begin->extra.var_extra.var_length))
#elif defined(POINTER_SORT)
	    if (memcmp(begin->rec, new->rec, RECORD.maxlength))
#else
	    if (memcmp(ITEM_REC(begin),
		       ITEM_REC(new),
		       item_size - sizeof(ovc_t)))
#endif
		die("insert_sort:backwards has different data: %x %x",
			begin, new);
	}
#endif
    }
#endif
}

/*
 * backwards_sort[_rs]	- similar to insert_sort() for radix items; this one:
 *			+ is optimized for already-ordered items
 *			+ does not initalize the ovcs (they've already been set)
 *			  but transforms the hash values in the ovc field into
 *			  a "hash" ovc (and eov, for pointer sorts)
 * Parameters:
 *	begin	- start of first radix item
 *	end	- start of eol item, also: just past the end of the last item
 *
 * Returns:
 *	the beginning of the resulting sorted ovc items.
 *	this usually is 'begin' unless dups were deleted
 *
 * Notes:
 *	Since the array is usually mostly radix-sorted by the time we get here,
 *	recodes are very common.  Pointer sorting 1 million random 8 byte keys
 *	typically calls recode here 500K times.
 *	
 */
#define MINITEMSIZE	((unsigned) (sizeof(ovc_t) + sizeof(u4)))

#if defined(POINTER_SORT)
item_t *backwards_sort(sort_t *sort, recode_args_t *args, item_t *begin, item_t *end)
#else
item_t *backwards_sort_rs(sort_t *sort, recode_args_t *args, item_t *begin, item_t *end)
#endif
{
    item_t		*new;		/* next item whose position is sought */
    item_t		*search;	/* scans from start_sorted down to end*/
    item_t		*start_sorted;	/* first sorted record, until end */
    unsigned		new_ovc, search_ovc;
    byte		*new_rec;
    unsigned		swap;
    const unsigned	item_size = ITEM_SIZE(sort);
    int			new_id;
#if defined(POINTER_SORT)
    item_t		*shift;
    byte		*rec;
    unsigned		ovc;
    unsigned		eov, new_eov;
#else
    byte		rec_buf[MAXRECORDSORT];
    const unsigned	length = (unsigned) ROUND_UP_NUM(sort->edited_size, sizeof(u4));
#endif /* else RECORD_SORT */

    args->curr_id = 0;

    new_id = -1 + ((byte *) end - (byte *) begin) / MINITEMSIZE;
    STATS.backwards_calls++;
#if defined(DEBUG1)
    hist_popgrow(STATS.backwards_histogram, new_id + 1);
#endif

    for (new = item_math(end, -(int) item_size), start_sorted = end;
	 new >= begin;
	 new = item_math(new, -(int) item_size)/*, new_id--*/)
    {
	new_rec = ITEM_REC(new);

	if (sort->compute_hash)
	{
	    /* convert radix items to ovc items: 'insert' the offset in front
	     * of the hash value which has been stored in the ovc field.
	     */
#if defined(POINTER_SORT)
	    new->extra.var_extra.var_eov = new->ovc & 0xffff;
	    new_ovc = BUILD_OVC(HASH_INITIAL_OFFSET, new->ovc >> 16);
#else
	    new_ovc = BUILD_OVC(HASH_INITIAL_OFFSET, new->ovc >> 1);
#endif
	}
	else
	{
#if defined(POINTER_SORT)
	    new->extra.var_extra.var_eov = new->ovc & 0xffff;
	    new_ovc = BUILD_OVC(1, new->ovc >> 16);
#else
	    new_ovc = BUILD_OVC(1, new->ovc >> 8);
#endif
	}

	new->ovc = new_ovc;
#if defined(POINTER_SORT)
	new_eov = new->extra.eov;
#endif
	for (search = item_math(start_sorted, -(int) item_size); ; )
	{
	    /* optimizes well for both ptr & rec */
	    search_ovc = item_math(search, item_size)->ovc;
	    search = item_math(search, item_size);

	    if (search == end)
		break;

	    if (new_ovc > search_ovc)
		continue;

	    /* have we found the item before which "new" belongs?
	     * if so break out to perhaps shift from search through start_sorted
	     * and move "new" into position, the bumped-up value of search
	     */
	    if (new_ovc != search_ovc)	/* "!=" is same but faster than "<" */
		break;

	    /* new_ovc is equal to search_ovc, call recode()
	     * to resolve comparison and assign a new ovc to the loser.
	     */
	    args->win_id = (u2) (((byte *) new - (byte *) begin) / MINITEMSIZE);
	    args->curr_id = (u2) ((((byte *) search - (byte *) begin) / MINITEMSIZE));

#if defined(RECORD_SORT)
	    args->win_ovc = new_ovc;
	    args->curr_ovc = search_ovc;

	    swap = RECODE(sort, args, new, search);
	    new_ovc = args->win_ovc;
	    search_ovc = args->curr_ovc;
	    new->ovc = new_ovc;
	    search->ovc = search_ovc;
#else
	    ASSERT(new->ovc == new_ovc);
	    ASSERT(search->ovc == search_ovc);
	    swap = RECODE(sort, args, new, search);
	    new_ovc = new->ovc;
	    search_ovc = search->ovc;
	    new_eov = new->extra.eov;
#endif
	    /* If we need to delete the existing, then replace it with new
	     * and leave start_sorted where it is
	     */
	    if ((search_ovc >> VALUE_BITS) == (DEL_DUP_OVC >> VALUE_BITS))
		goto replace_rec;
	    
	    /* If new item is lower than the search item, the search item has
	     * been assigned a new ovc.  Put the new item in front of search.
	     */
	    if (swap == 0)
		break;
	    
	    /* The search item is lower, and the new item has been
	     * assigned a new ovc. Continue searching.
	     */
	}

	/* if need to shift items to make room for new item.
	 */
	if (search != start_sorted)
	{
	    /* prepare to put the "new" list item at "search" by shifting
	     * towards begin all items from start_sorted to search
	     */
	    STATS.backwards_moves += (char *) search - (char *) start_sorted;
#if defined(POINTER_SORT)
	    ASSERT(new_rec == new->rec);
	    ASSERT(new_eov == new->extra.eov);
	    shift = start_sorted - 1;
	    for (;;)
	    {
		ovc = (shift + 1)->ovc;
		rec = (shift + 1)->rec;
		eov = (shift + 1)->extra.eov;
		shift++;
		(shift - 1)->ovc = ovc;
		(shift - 1)->rec = rec;
		(shift - 1)->extra.eov = eov;
		if (shift + 1 == search)
		    break;
	    }
#else
	    /* Save the "new" item in case it would be overwritten by the shift
	     * Most of the time (except when a duplicate is actually deleted)
	     * it will be overwritten, so this copy usually is needed.
	     */
	    MEMMOVE/*(*sort->copier)*/(rec_buf, new_rec, length);
	    new_rec = rec_buf;

	    memmove(item_math(start_sorted, -(int) item_size), start_sorted, (char *) search - (char *) start_sorted);

#endif	/* of RECORD_SORT */
	}
	
	start_sorted = item_math(start_sorted, -(int) item_size);
	/* if the insert point isn't where the new record is already, move
	 * the record to the insert point.
	 */
	search = item_math(search, -(int) item_size);
	ASSERT(search >= begin);
	if (search != new)
	{
replace_rec:
#if defined(POINTER_SORT)
	    search->extra.eov = new_eov;
	    search->rec = new_rec;
#else
	    /* The source address may not be word aligned, so we can't use
	     * the optimized inline word-at-a-time copy macros.
	     */
	    (*sort->copier)(ITEM_REC(search), new_rec, length);
#endif	/* of RECORD_SORT */
	    search->ovc = new_ovc;
	}

    }

    /* this list is NOT terminated with an EOL_OVC - the data just sorted
     * usually isn't in a line, and often is too big to fit in a line
     */

    return (start_sorted);
}

/*
 * radix_disperse	- scatter in_bucket's radix items into 16 out_buckets
 *			  using the next RADIXSHIFT bits of the hash value
 *			  (which are found by shifting the key right by 'shift')
 *			  New out_bucket lines are allocated from list->free
 */
#if defined(POINTER_SORT)
void radix_disperse(sort_t	*sort,
		    bucket_t	*in_bucket,
		    bucket_t	*out_buckets, 
		    unsigned	shift,
		    list_desc_t	*list)
#else
void radix_disperse_rs(sort_t	*sort,
		    bucket_t	*in_bucket,
		    bucket_t	*out_buckets, 
		    unsigned	shift,
		    list_desc_t	*list)
#endif
{
    byte	*src_end;
    byte	*src, *dst;
    unsigned	bucket_index;
    bucket_t	*bucket;
    radix_key_t	next_radix_key;
    rad_line_t	*new_out_line;
    rad_line_t	*out_line;
    rad_line_t	*in_line;
    rad_line_t	*next_in_line;
    rad_line_t	*tail = in_bucket->tail;
    byte	*next_dst;
    unsigned	line_count;
    const unsigned item_size = ITEM_SIZE(sort);
    const unsigned item_space = sort->ll_item_space;

    ASSERT(in_bucket->tail == RADIX_LINE_START(in_bucket->next_item) || FULL_RADIX_LINE(in_bucket->next_item, item_space));

    /* for each input line
     */
    for (in_line = in_bucket->head; ; in_line = next_in_line)
    {
#pragma prefetch_ref= in_line[0], kind=rd

	/* assume this is not the last input line
	 */
	src_end = ENDING_RADIX_ITEM(in_line, item_space);

	/* if this is the last input line, get end from bucket next_item
	 */
	if (in_line == tail)
	    src_end = in_bucket->next_item;

	/* for each input item in the line
	 */
	src = FIRST_RADIX_ITEM(in_line);
	for (next_radix_key = ((radix_key_t *) src)[0];
	     src < src_end;
	     next_radix_key = ((radix_key_t *) (src + item_size))[0],
	     src += item_size)
	{
	    /* each C statement in this loop is a single machine instruction for
	     * later optimization */
	
	    bucket_index = next_radix_key >> shift;

	    bucket_index &= RADIXMASK;
	    bucket_index *= sizeof(bucket_t);
	    bucket = (bucket_t *)((byte *)out_buckets + bucket_index);
	    dst = bucket->next_item;
#if 1
	    /* Delete/summarize a duplicate? (single fixlen key only)
	     * The assignment to delete_in_disperse in prepare.c should
	     * correlate with the kind of equality comparison supported here.
	     *
	     * XXX Opportunity for an item_keys_equal() function. in prehash too
	     */
	    if (sort->delete_in_disperse &&
		dst != (byte *) item_space &&
		*((radix_key_t *) (dst - item_size)) == next_radix_key)
	    {
#if defined(POINTER_SORT)
		if (memcmp(((item_t *) src)->rec + sort->keys->position,
			   ((item_t *) (dst - item_size))->rec + sort->keys->position,
			   sort->keys->length) == 0)
		{
		    if (sort->n_summarized)
			summarize_pointer(sort, (item_t *) (dst - item_size),
						(item_t *) src);
		    STATS.radix_deletes++;
		    continue;
		}
#else	/* now RECORD_SORT */
		if (memcmp(src + sizeof(radix_key_t) + sort->keys->position,
			   (dst - item_size) + sizeof(radix_key_t) + sort->keys->position,
			   sort->keys->length) == 0)
		{
		    if (sort->n_summarized)
			summarize_record(sort, (dst - item_size) + sizeof(radix_key_t),
					       src + sizeof(radix_key_t));
		    STATS.radix_deletes++;
		    continue;
		}
#endif 	/* of else RECORD_SORT */
	    }
#endif	/* of deleting duplicates during dispersion */

	    /* if we need a new line for this bucket, allocate one.
	     */
	    if (FULL_RADIX_LINE(dst, item_space))
	    {
		out_line = bucket->tail;
		new_out_line = (rad_line_t *) list->free;
		line_count = bucket->line_count;
		if (out_line == NULL)
		    bucket->head = new_out_line;
		else
#if defined(INITIAL_FREELIST)
		    *((rad_line_t **) dst) = new_out_line;
		list->free = *((item_line_t **) new_out_line);
#else
		    ITEM_NEXTLINE(((item_t *) dst)) = (item_line_t *) new_out_line;
		list->free = LINE_NEXTLINE(new_out_line, item_space);
#endif
		bucket->tail = new_out_line;
		dst = FIRST_RADIX_ITEM(new_out_line);
		line_count++;
		bucket->line_count = line_count;
	    }
	    
	    next_dst = dst + item_size;
	    bucket->next_item = next_dst;

#if defined(POINTER_SORT)
	    MEMMOVE(dst, src, item_size);	/* optimizes to l/l/l s/s/s */
#else
	    (*sort->itemcopier)((item_t *) dst, (item_t *) src, item_size);
#endif
	}

	/* this in_bucket line's items have been copied, push line onto freelist
	 */
#if defined(INITIAL_FREELIST)
	next_in_line = *((rad_line_t **) src);
	*((item_line_t **) in_line) = list->free;
#else
	next_in_line = (rad_line_t *) ITEM_NEXTLINE((item_t *) src);
	LINE_NEXTLINE(in_line, item_space) = list->free;
#endif
	list->free = (item_line_t *) in_line;

	if (in_line == tail)
	    break;
    }
    ASSERT(bucket->tail == RADIX_LINE_START(bucket->next_item) || FULL_RADIX_LINE(bucket->next_item, item_space));
}

/*
 * radix_append_fixup	- correct the ovc of the first item being appended to
 *			  an existing ovc list in a radix sort (its ovc must
 *			  be relative to the last item (ending at tail_item))
 *			  Also skips over the first item if it is a del-dup or
 *			  collapses from summarization.
 * Returns:
 *	the first item which the caller is to append
 */
#if defined(POINTER_SORT)
item_t *radix_append_fixup(sort_t *sort,
			   recode_args_t *args,
			   item_t *prev_tail,
			   item_t *start_sorted)
#else
item_t *radix_append_fixup_rs(sort_t *sort,
			      recode_args_t *args,
			      item_t *prev_tail,
			      item_t *start_sorted)
#endif
{
#if defined(POINTER_SORT)
    ovc_t		save_tail_ovc;
#if defined(DEBUG1)
    unsigned		save_tail_eov;
#endif
#endif

    args->win_id = 0;
    args->curr_id = 1;
    prev_tail = item_math(prev_tail, -(int) ITEM_SIZE(sort));

#if defined(POINTER_SORT)
    save_tail_ovc = prev_tail->ovc;
#if defined(DEBUG1)
    save_tail_eov = prev_tail->extra.eov;
#endif
    prev_tail->ovc = VIRGIN_OVC;
    start_sorted->ovc = VIRGIN_OVC;
    if ((sort->recode)(sort, args, prev_tail, start_sorted))
#else
    args->win_ovc = VIRGIN_OVC;
    args->curr_ovc = VIRGIN_OVC;
    if ((sort->recode)(sort, args, (item_t *) ITEM_REC(prev_tail),
				   (item_t *) ITEM_REC(start_sorted)))
#endif
	die("radix_append_fixup:new list misplaced");

#if defined(POINTER_SORT)
    ASSERT(prev_tail->ovc == VIRGIN_OVC);
    ASSERT(prev_tail->extra.eov == save_tail_eov);
    ASSERT(start_sorted->ovc != VIRGIN_OVC);
    prev_tail->ovc = save_tail_ovc;	/* can't leave it as VIRGIN_OVC */
#else
    ASSERT(args->win_ovc == VIRGIN_OVC);
    ASSERT(args->curr_ovc != VIRGIN_OVC);
    start_sorted->ovc = args->curr_ovc;
#endif

    if ((start_sorted->ovc >> VALUE_BITS) == (DEL_DUP_OVC >> VALUE_BITS))
    {
	die("radix_append_fixup:delete dup!");
	start_sorted = item_math(start_sorted, ITEM_SIZE(sort));
    }

    return (start_sorted);
}

/*
 * radix_count	- sort the radix items of a single bucket, append them to
 *		  the ovc-item line list in "out_list"
 */
#if defined(POINTER_SORT)
void radix_count(sort_t		*sort,
		 bucket_t	*in_bucket,
		 unsigned	shift,
		 list_desc_t	*out_list,
		 merge_args_t	*args)
#else
void radix_count_rs(sort_t	*sort,
		 bucket_t	*in_bucket,
		 unsigned	shift,
		 list_desc_t	*out_list,
		 merge_args_t	*args)
#endif
{
    rad_line_t	*line;
    rad_line_t	*next;
    rad_line_t	*tail;
    byte	*src, *end;
    byte	*curr_ptr;
    item_t	*start_sorted;
    unsigned	bucket_index;
    unsigned	next_bucket_index;
    ptrdiff_t	item_bytes;
    unsigned	mask;
    unsigned	count;
    unsigned	count_bits;
    unsigned	recs_est;
    unsigned	max_bucket_size;
    ptrdiff_t	space_left;
    unsigned	bucket_size;
    item_line_t	*tail_ll;
    byte	*tail_item;
    const unsigned item_size = ITEM_SIZE(sort);
    const unsigned item_space = sort->ll_item_space;
    union
    {
	unsigned	size;
	byte		*ptr;
    } *up, *umax, u[1024];
    ptrdiff_t 	temp[2048];

    tail = in_bucket->tail;

    /* set count (and shift, mask) to be the minimum of:
     * 1) second power of 2 greater than the estimated number of records.
     * 2) bits left in the radix key
     * 3) 1024 (size of local count array)
     */
    recs_est = in_bucket->line_count * 2 * sort->ll_items;
#if defined(DEBUG1)
    hist_popgrow(STATS.radixcount_histogram, RADIXBITS - shift);
#endif
    for (count_bits = 0; ; count_bits++)
    {
	count = 1 << count_bits;
	if (count_bits == shift)
	    break;
	if (count_bits == 10)
	    break;
	if (count >= recs_est)
	    break;
    }
    /* printf("%2d %2d %2d %4d\n",
	   in_bucket->line_count, shift, count_bits, count); */
    mask = count - 1;
    shift -= count_bits;
    
    /* clear all bucket counts
     */
    umax = &u[count];
    for (up = &u[0]; up < umax; up += 4)
    {
	up[0].ptr = 0;
	up[1].ptr = 0;
	up[2].ptr = 0;
	up[3].ptr = 0;
    }
    
    ASSERT(in_bucket->tail == RADIX_LINE_START(in_bucket->next_item) || FULL_RADIX_LINE(in_bucket->next_item, item_space));
    /* determine the size for each bucket
     */
    max_bucket_size = item_size;
    if (count > 1)
    {
	for (line = in_bucket->head; ; line = next)
	{
	    if (line == tail)
		end = in_bucket->next_item;
	    else
		end = ENDING_RADIX_ITEM(line, item_space);

	    /* This loop is unrolled once */
	    for (src = FIRST_RADIX_ITEM(line); src < end; src += item_size)
	    {
		bucket_index = ((radix_key_t *) src)[0] >> shift;
		src += item_size;
		next_bucket_index = ((radix_key_t *) src)[0] >> shift;
		bucket_index &= mask;
		if (max_bucket_size == u[bucket_index].size)
		    max_bucket_size = u[bucket_index].size + item_size;
		next_bucket_index &= mask;
		u[bucket_index].size += item_size;

		if (src == end)
		    break;
		if (max_bucket_size == u[next_bucket_index].size)
		    max_bucket_size = u[next_bucket_index].size + item_size;
		u[next_bucket_index].size += item_size;
	    }
	    if (line == tail)
		break;
#if defined(INITIAL_FREELIST)
	    next = *((rad_line_t **) src);
#else
	    next = (rad_line_t *) ITEM_NEXTLINE((item_t *) src);
#endif
	}
    }

    if (max_bucket_size > 20 * item_size && in_bucket->line_count > 1)
    {
	/* fall back to an ovc sort to clean up this bucket
	 */
	last_resort(sort, in_bucket, out_list);
    }
    else
    {
	/* calculate pointers for each bucket
	 * The loop in unrolled four times (three extra times)
	 * The size/ptr array is a multiple of four in size, so 
	 * while the loop below usually goes beyond umax, it'll be harmless
	 */
	curr_ptr = (byte *) temp;
	umax = &u[count];
	for (up = &u[0]; up < umax; up += 4)
	{
	    bucket_size = up[0].size;
	    up[0].ptr = curr_ptr;
	    curr_ptr += bucket_size;

	    bucket_size = up[1].size;
	    up[1].ptr = curr_ptr;
	    curr_ptr += bucket_size;

	    bucket_size = up[2].size;
	    up[2].ptr = curr_ptr;
	    curr_ptr += bucket_size;

	    bucket_size = up[3].size;
	    up[3].ptr = curr_ptr;
	    curr_ptr += bucket_size;
	}

	/* distribute records into local buckets
	 */
	for (line = in_bucket->head; ; line = next)
	{
	    if (line == tail)
		end = in_bucket->next_item;
	    else
		end = ENDING_RADIX_ITEM(line, item_space);

	    for (src = FIRST_RADIX_ITEM(line); src < end; src += item_size)
	    {
		byte	*dst;

		bucket_index = ((radix_key_t *) src)[0] >> shift;
		bucket_index &= mask;
		dst = u[bucket_index].ptr;
		u[bucket_index].ptr = dst + item_size;
#if defined(POINTER_SORT)
		MEMMOVE(dst, src, item_size);	/* optimizes to l/l/l s/s/s */
#else
		(*sort->itemcopier)((item_t *) dst, (item_t *) src, item_size);
#endif
	    }

	    /* now that this line's items have been copied, put it on free list
	     */
#if defined(INITIAL_FREELIST)
	    next = *((rad_line_t **) src);
	    *((item_line_t **) line) = out_list->free;
#else
	    next = (rad_line_t *) LINE_NEXTLINE(line, item_space);
	    LINE_NEXTLINE(line, item_space) = out_list->free;
#endif
#if defined(DEBUG2)
	    memset((byte *) line + 4, 0xcb, end - FIRST_RADIX_ITEM(line) - 4);
#endif
	    out_list->free = (item_line_t *) line;

	    if (line == tail)
		break;
	}
	
	/* Insert sort the now-probably-nearly-ordered records.
	 * This also changes them into ovc items: afterwards the ovc field
	 * contains a hash offset as well as the ovc; in ptr sorts the eov
	 * contains the second half of the hash value.
	 */
	start_sorted = 
#if defined(POINTER_SORT)
			backwards_sort
#else
			backwards_sort_rs
#endif
			    (sort, &args->recode, (item_t *) temp,
						  (item_t *) curr_ptr);
	
	/* move records to sort partition line-list
	 */
	tail_item = out_list->next_item;
	tail_ll = out_list->tail;
	ASSERT((byte *) start_sorted < curr_ptr);

	/* If there already are records in the output, then we need to
	 * recompute the ovc for the first item in the set which is
	 * about to be copied (otherwise it gets an initial, rather
	 * than a relative-to-the-previous-item, ovc)
	 */
	if (tail_item != (byte *) &tail_ll->item[0])
#if defined(POINTER_SORT)
	    radix_append_fixup
#else
	    radix_append_fixup_rs
#endif
		    (sort, &args->recode, (item_t *) tail_item, start_sorted);

	item_bytes = curr_ptr - (byte *) start_sorted;

	for (;;)
	{
	    space_left = item_space - (tail_item - (byte *) tail_ll);
	    /* Is the rest of this radix_count()'s data too big to fit
	     * in the tail line? If so we copy only so much as can fit
	     */
	    if (item_bytes > space_left)
	    {
		/* The data is too large to all fit in this line. After copying
		 * we ovc-terminate this line and append another to the outlist.
		 */
		memmove(tail_item, start_sorted, space_left);
		tail_item += space_left;
		start_sorted = item_math(start_sorted, space_left);
		item_bytes -= space_left;
#if defined(DEBUG2)
		if (Print_task)
		    fprintf(stderr, "radix_count: tail %x appending %x (next %x)\n",
				    tail_ll, out_list->free,
				    *((item_line_t **) out_list->free));
#endif
		tail_ll = out_list->free;

		/* do something while tail_ll is being loaded
		 */
		((item_t *) tail_item)->ovc = NULL_OVC;
		ITEM_NEXTLINE(((item_t *) tail_item)) = tail_ll;
#if defined(INITIAL_FREELIST)
		out_list->free = *((item_line_t **) tail_ll);
#else
		out_list->free = LINE_NEXTLINE(tail_ll, item_space);
#endif

		tail_item = (byte *) tail_ll->item;
	    }
	    else
	    {
		/* All (the rest of) the data fits, here, we are done */
		memmove(tail_item, start_sorted, item_bytes);
		tail_item += item_bytes;
		break;
	    }
	}
	out_list->next_item = tail_item;
	out_list->tail = tail_ll;
    }

    ASSERT(out_list->tail == LINE_START(out_list->next_item));
#if defined(DEBUG2)
    {
	int found;

	((item_t *) out_list->next_item)->ovc = EOL_OVC;
	ITEM_NEXTLINE((item_t *) out_list->next_item) = NULL;
	found = check_ll(sort, out_list->head->item);
    }
#endif
}


#endif	/* insert_sort and backwards_sort have only two versions each */
