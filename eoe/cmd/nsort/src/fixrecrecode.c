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
 * fixrecrecode.c	- special purpose offset value code computation
 *
 *	Computes the first (or next) offset value code of a pair of records,
 *	relative to the key ordering specified in the sort descriptor
 *
 *	$Ordinal-Id: fixrecrecode.c,v 1.7 1996/10/02 23:32:51 charles Exp $
 */
#ident	"$Revision: 1.1 $"

#include "otcsort.h"
#include "ovc.h"

/*
** fixrecode	- compute the next (or first) offset value code of
**		  *win_node relative to *curr_node. Compare successive ovc's
**		  until a difference is found. Update the ovc of the loser
**		  and return whether the previous winner remains the champ.
**
**	*** This function works only for single-key sorts ***
**
** Parameters (pointer sort versions):
**	sort	 - the sort description, mostly for the key type info
**	args	 - additional parameters to recode which aren't in the items:
**		   In pointer sorts only win_id and curr_id are valid.
**		   Their relative ordering is the tie-breaking criterion
**		   the lower id appears earlier in the input data set and will
**		   be the first one output - 'stable' sorting is nearly free.
**	win_item - the winning item up to now
**	curr_item- the next item in the merge to battle with the champion
**
** Parameters (record sort version):
**	sort	 - same as ptr
**	args	 - additional parameters to recode which aren't in the items:
**		   In addition to win_id, curr_id, and summarize 
**		   the offset value codes are in win_ovc and curr_ovc.
**	win_rec	 - the winning record up to now
**	curr_rec - the next record in the merge to battle with the champion
**
** Returns:
**	0 - win_item remains the winner.
**	1 - curr_item is the winner. We have a new champeen!
**
** Notes:
**	Dealing with 'ovc.offset' can be confusing, since it is in 'descending'
**	order. 	The 1st offset (VIRGIN_OVC) is 0xffff(0000); 2nd is 0xfffe(0000)
**
*/
#if defined(VARIABLE_SORT)
unsigned fixrecode_variable(sort_t *sort,
			recode_args_t *args,
			item_t *win_item,
			item_t *curr_item)
#elif defined(POINTER_SORT)
unsigned fixrecode_pointer(sort_t *sort,
			recode_args_t *args,
			item_t *win_item,
			item_t *curr_item)
#else /* is RECORD_SORT */
unsigned fixrecode_record(sort_t *sort,
		       recode_args_t *args,
		       byte *win_rec,
		       byte *curr_rec)
#endif
{
    byte	*win_key, *curr_key;	/* 'temporaries' */
    unsigned	win_ovc, curr_ovc;
#if defined(POINTER_SORT)
    unsigned	win_eov, curr_eov;
#endif
    keydesc_t	*key;
    unsigned	offset;
    unsigned	sofar;
    int		tillend;
    unsigned	xormask;	

    /*
    ** Translate the large, scaled-by-64K ovc.offset to
    ** a small 'offset' of ovc.offset. It is easier for
    ** humans to figure out, and probably the
    ** switch's "offset % (either 3 or 2)" is faster too.
    **  The first field's offset is 0.
    */
#ifdef POINTER_SORT
    offset = GET_OVC_OFFSET(win_item->ovc);
#else
    offset = GET_OVC_OFFSET(args[0].win_ovc);
#endif

    key = sort->keys;
    if (offset < HASH_LOWEST_OFFSET)    /* a normal ovc offset? */
    {
        /* find the key containing the current offset. The keys are
        ** ordered most significant (lowest translated offset) first.
        */
        offset = HASH_LOWEST_OFFSET - offset;

        if (offset > key->ending_offset)
	{
            key++;
	    ASSERT(key->type == typeEOF);	/* this func handle only 1 key*/
	    goto keys_equal;
	}
    }
#ifdef POINTER_SORT
    else if (offset != VIRGIN_OFFSET)	/* second half of a hash ovc offset? */
    {
	/* when getting the second half of a dynamically calculated hashvalue
	 * don't skip through the key array to find the key for this offset;
	 * there is no key for the hash value. stay at the first key so that if
	 * the hash values are equal the loop will naturally start to do
	 * normal ovc comparisons on the 'real' keys.
	 */
	ASSERT(sort->compute_hash);
	win_eov = win_item->extra.var_extra.var_eov;
	curr_eov = curr_item->extra.var_extra.var_eov;
	if (win_eov < curr_eov)
	{
	    /* only assign ovc of loser, eov not updated, it is unchanged */
	    /* indicate winner still winner - no swap */
	    ASSIGN_OVC(CURR, 0, curr_eov);	/* off 0 -> low half of hash */
	    return (0);
	}
	else if (win_eov > curr_eov)
	{
	    /* only assign ovc of loser, eov not updated, it is unchanged*/
	    /* indicate prior winner lost the comparison */
	    ASSIGN_OVC(WIN, 0, win_eov);	/* off 0 -> low half of hash */
	    return (1);
	}
	offset = 0;
    }
#endif
    else if (!sort->compute_hash)	/* offset == VIRGIN_OFFSET non-hash */
    {
	offset = 0;	/* skip over the ovc space reserved for hash values */
    }
    else /* (offset == VIRGIN_OFFSET && sort->compute_hash) */
    {
# if defined(VARIABLE_SORT)
	win_ovc = (sort->calc_hash)(sort,
				    win_item->rec,
				    win_item->extra.var_extra.var_length + 2);
	curr_ovc = (sort->calc_hash)(sort,
				     curr_item->rec,
				     curr_item->extra.var_extra.var_length + 2);
# elif defined(POINTER_SORT)
	win_ovc = ((calc_hash_fixed_func_t) sort->calc_hash)(sort, win_item->rec);
	curr_ovc = ((calc_hash_fixed_func_t) sort->calc_hash)(sort, curr_item->rec);
# else
	win_ovc = ((calc_hash_fixed_func_t) sort->calc_hash)(sort, win_rec);
	curr_ovc = ((calc_hash_fixed_func_t) sort->calc_hash)(sort, curr_rec);
# endif

	/* in case the hash values are equal, prepare to fall though
	 */
	if (win_ovc != curr_ovc)
	{
#if defined(POINTER_SORT)
	    /* put one half of the hash value in the ovc, the lower in the eov*/
	    win_eov = win_ovc & 0xffff;
	    curr_eov = curr_ovc & 0xffff;
	    win_ovc >>= 16;
	    curr_ovc >>= 16;
#else
	    /* Put the low 31 bits of the hash value in the ovc in a record sort
	     * This leaves 128 - 1 (virgin) - 2 (dups) = 125 offsets for data
	     */
	    win_ovc >>= 1;
	    curr_ovc >>= 1;
#endif
	    if (win_ovc < curr_ovc)
	    {
		/* only assign eov of loser */
		/* indicate winner still winner - no swap */
		ASSIGN_OVC(CURR, HASH_INITIAL_OFFSET, curr_ovc);
		ASSIGN_EOV(CURR, curr_eov);
		return (0);
	    }
	    else if (win_ovc > curr_ovc)
	    {
		/* only assign eov of loser */
		/* indicate prior winner lost the comparison */
		ASSIGN_OVC(WIN, HASH_INITIAL_OFFSET, win_ovc);
		ASSIGN_EOV(WIN, win_eov);
		return (1);
	    }

#if defined(POINTER_SORT)	/* handle eov for pointer sorts */
	    if (win_eov < curr_eov)
	    {
		/* only assign eov of loser */
		/* indicate winner still winner - no swap */
		ASSIGN_OVC(CURR, HASH_INITIAL_OFFSET + 1, curr_eov);
		ASSIGN_EOV(CURR, curr_eov);	
		return (0);
	    }
	    else /* if (win_eov > curr_eov) */
	    {
		/* only assign eov of loser */
		/* indicate prior winner lost the comparison */
		ASSIGN_OVC(WIN, HASH_INITIAL_OFFSET + 1, win_eov);
		ASSIGN_EOV(WIN, win_eov);
		return (1);
	    }
#endif
	}
	key = sort->keys;
	offset = 0;
    }


#ifdef POINTER_SORT
    ASSERT(offset <= key->ending_offset);
    /*if (offset > key->ending_offset)
	goto keys_equal;*/
    if (offset & 1)
    {
	win_ovc = win_item->extra.var_extra.var_eov;
	curr_ovc = curr_item->extra.var_extra.var_eov;
	args->n_soft++;
	offset++;
	if (win_ovc != curr_ovc)
	{
	    if (win_ovc < curr_ovc)
	    {
		ASSIGN_OVC(CURR, offset, curr_ovc);
		return (0);		/* indicate winner still winner */
	    }
	    else
	    {
		ASSIGN_OVC(WIN, offset, win_ovc);
		return (1);		/* indicate winner lost comparison */
	    }
	}
	if (offset > key->ending_offset)
	    goto keys_equal;
    }
    sofar = offset * (int) sizeof(valuecode_t);
    xormask = key->xormask;
    win_key =  win_item->rec  + key->position + sofar;
    curr_key = curr_item->rec + key->position + sofar;
#else
    sofar = offset * (int) sizeof(valuecode_t);
    xormask = key->xormask & 0xffffff;
    win_key =  win_rec  + key->position + sofar;
    curr_key = curr_rec + key->position + sofar;
#endif
    args->n_hard++;

    for (tillend = key->length - sofar;
	 tillend > 0;
	 tillend -= sizeof(valuecode_t))
    {
#if defined(RECORD_SORT)
	if (tillend == 1)		/* one data byte, << by 16 for rec */
	{
	    win_ovc = (*win_key << 16) ^ (xormask & 0xff0000);
	    curr_ovc = (*curr_key << 16) ^ (xormask & 0xff0000);
	}
	else if (tillend == 2)		/* two data bytes, << by 8 for rec */
	{
	    win_ovc = (ulh(win_key) << 8) ^ (xormask & 0xffff00);
	    curr_ovc = (ulh(curr_key) << 8) ^ (xormask & 0xffff00);
	}
	else
	{
	    win_ovc = (ulw(win_key) >> (sizeof(ovcoff_t) << 3)) ^ xormask;
	    curr_ovc = (ulw(curr_key) >> (sizeof(ovcoff_t) << 3)) ^ xormask;
	}
#else
	if (offset & 1)			/* odd offset? get data from eov */
	{
	    win_ovc = win_eov;
	    curr_ovc = curr_eov;
	}
	else if (tillend > 2) 	
	{
	    win_eov = ulw(win_key) ^ xormask;
	    curr_eov = ulw(curr_key) ^ xormask;
	    win_ovc = win_eov >> 16;
	    curr_ovc = curr_eov >> 16;
	    if (tillend == 3)
	    {
		win_eov &= 0xff00;
		curr_eov &= 0xff00;
	    }
	    else
	    {
		win_eov &= 0xffff;
		curr_eov &= 0xffff;
	    }
	}
	else if (tillend == 2)		/* two data bytes */
	{
	    win_ovc = ulh(win_key) ^ (xormask & 0xffff);
	    curr_ovc = ulh(curr_key) ^ (xormask & 0xffff);
	    win_eov = curr_eov = 0;
	}
	else		/* one data byte, << by 8 for ptr */
	{
	    win_ovc = (*win_key << 8) ^ (xormask & 0xff00);
	    curr_ovc = (*curr_key << 8) ^ (xormask & 0xff00);
	    win_eov = 0;
	    curr_eov = 0;
	}
#endif

	offset++;

	if (win_ovc < curr_ovc)
	{
	    ASSIGN_OVC(CURR, offset, curr_ovc);
	    ASSIGN_EOV(CURR, curr_eov);	/* only assign eov of loser */
	    return (0);			/* indicate winner still winner */
	}
	else if (win_ovc > curr_ovc)
	{
	    ASSIGN_OVC(WIN, offset, win_ovc);
	    ASSIGN_EOV(WIN, win_eov);	/* only assign eov of loser */
	    return (1);			/* indicate winner lost comparison */
	}

	/* else comparison couldn't be resolved, advance to next offset */
	win_key += sizeof(valuecode_t);
	curr_key += sizeof(valuecode_t);
    }

#if defined(DEBUG1)
    if (tillend <= -(int) (sizeof(valuecode_t)))
	die("fixrecode: illegal offset - %08x", offset);
#endif

keys_equal:
    /* the keys in both records are equal, choose current winner as winner
     *
     * if in stable order already...
     *
     * Note that recodes for advance block reading use the case
     * where args->win_id == args->curr_id to assign an ovc to
     * a record relative to the previous record in the SAME run.
     */
    if (args->win_id <= args->curr_id)
    {
	if (sort->delete_dups || 
	    (args->summarize &&
#ifdef POINTER_SORT
	     summarize_pointer(sort, win_item, curr_item)
#else
	     summarize_record(sort, win_rec, curr_rec)
#endif
	     ))
	    ASSIGN_OVC(CURR, DEL_DUP_OVC_OFF, args->curr_id);
	else
	    ASSIGN_OVC(CURR, KEPT_DUP_OVC_OFF, args->curr_id);
	return (0);
    }
    else
    {
	if (sort->delete_dups ||
	    (args->summarize &&
#ifdef POINTER_SORT
	     summarize_pointer(sort, curr_item, win_item)
#else
	     summarize_record(sort, curr_rec, win_rec)
#endif
	   ))
	    ASSIGN_OVC(WIN, DEL_DUP_OVC_OFF, args->win_id);
	else
	    ASSIGN_OVC(WIN, KEPT_DUP_OVC_OFF, args->win_id);
	return (1);
    }
}
