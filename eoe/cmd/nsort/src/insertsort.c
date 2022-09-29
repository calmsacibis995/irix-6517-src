/*
 ****************************************************************************
 *    Copyright (c) 1996 Ordinal Technology Corp.  All Rights Reserved      *
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
 ****************************************************************************/

#ident	"$Revision: 1.1 $"

#include "otcsort.h"
#include "ovc.h"

#define DATA_LENGTH ((unsigned) ROUND_UP_NUM(EDITED_SIZE(sort), 4))

void INSERT_SORT(sort_t *sort, recode_args_t *args, item_t *begin, item_t *end)
{
    item_t		*new, *insert, *end_insert;
    unsigned		new_ovc, in_ovc;
    byte		*new_rec;
    unsigned		swap;
    const unsigned	item_size = ITEM_SIZE(sort);
    ovc_init_t		*init;
    byte		*ovcp;
    int			oplen;
    unsigned		val;
    int			new_id;
#if defined(POINTER_SORT)
    item_t		*shift;
    byte		*rec;
    unsigned		ovc;
    unsigned		eov, new_eov;
    int			past_eor;
    int			var_hdr_extra = RECORD.var_hdr_extra;
#else
    byte		rec_buf[MAXRECORDSORT];
    const unsigned	length = DATA_LENGTH /* (unsigned) ROUND_UP(sort->edited_size, sizeof(u4))*/;
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
#if defined(RECORD_SORT)
	new_rec = (byte *) &new->extra.data;
#else
	new_rec = new->rec;
#endif

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
		    new->ovc = VIRGIN_OVC;	/* XXX too hard to do now */
		    goto item_done;

#if defined(POINTER_SORT)	/* record sorts can have only fixlen keys */
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
#if defined(POINTER_SORT)
	    /* insert the "new" list item at "insert" by shifting up one
	     * place all items from insert to new-1.
	     */
	    ASSERT(new_rec == new->rec);
	    new_rec = new->rec;
	    new_eov = new->extra.eov;
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
	    new_rec = rec_buf;
	    MEMMOVE/*(*sort->copier)*/(rec_buf, &new->extra.data, length);

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
	    MEMMOVE/*(*sort->copier)*/(&insert->extra.data, new_rec, length);
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
    }
#endif
}
