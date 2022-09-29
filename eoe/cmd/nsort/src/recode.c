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
 *	recode.c	- general purpose offset value code computation
 *
 *	Computes the first (or next) offset value code of a pair of records,
 *	relative to the key ordering specified in the sort descriptor
 *
 *	$Ordinal-Id: recode.c,v 1.18 1996/10/02 22:26:04 charles Exp $
 */
#ident	"$Revision: 1.1 $"

#include "otcsort.h"
#include "ovc.h"

/*
** recode	- compute the next (or first) offset value code of
**		  *win_node relative to *curr_node. Compare successive ovc's
**		  until a difference is found. Update the ovc of the loser
**		  and return whether the previous winner remains the champ.
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
unsigned recode_variable(sort_t *sort,
			recode_args_t *args,
			item_t *win_item,
			item_t *curr_item)
#elif defined(POINTER_SORT)
unsigned recode_pointer(sort_t *sort,
			recode_args_t *args,
			item_t *win_item,
			item_t *curr_item)
#else /* is RECORD_SORT */
unsigned recode_record(sort_t *sort,
		       recode_args_t *args,
		       byte *win_rec,
		       byte *curr_rec)
#endif
{
    keydesc_t	*key;
    keydesc_t	*startkey;
    ptrdiff_t	i;
    byte	*win_key, *curr_key;
    byte	*win_ovcp, *curr_ovcp;
    item_t	win_temp, curr_temp;
    unsigned	win_ovc, curr_ovc;
    unsigned	temp;	/* using a local in some of the byte moves helps the */
    			/* optimizer to overlap loads and stores */
    u4		win_float;
    u4		curr_float;
    u4		win_double[2];
    u4		curr_double[2];
#ifdef POINTER_SORT
    unsigned	win_eov, curr_eov;
#ifdef VARIABLE_SORT
    byte	*win_endrec, *curr_endrec;
    char	fdelim;
    unsigned	mask;	
#endif
#endif
    int		ovcleft;
    unsigned	offset;
    unsigned	bytes_so_far;

#ifdef DEBUG_RECODE
    node_t	win_save, curr_save;

    win_save = *win_item;
    curr_save = *curr_item;
#endif

    ARENACHECK;

    /*
    ** Translate the large, scaled-by-64K ovc.offset to
    ** a small 'offset' of ovc.offset. It is easier for
    ** humans to figure out, and probably the
    ** switch's "offset % (either 3 or 2)" is faster too.
    **  The first field's offset is 0.
    */
#ifdef POINTER_SORT
    offset = VIRGIN_OFFSET - GET_OVC_OFFSET(win_item->ovc);
#else
    offset = VIRGIN_OFFSET - GET_OVC_OFFSET(args[0].win_ovc);
#endif
    
    /* find the key containing the current offset. The keys are
    ** ordered most significant (lowest translated offset) first.
    */
    for (key = sort->keys; offset > key->ending_offset ; key++)
	    continue;

    if (key->type == typeEOF)
	goto keys_equal;

#ifdef POINTER_SORT
    switch (offset % (OVCEOVSIZE / sizeof(valuecode_t)))
    {
      case OVC_FETCHKEYS:
#endif
	/* get the next ovc[+eov] data bytes, from either fixed
	** or variable length key fields.  We have to fetch and
	** condition the keys into a local buffer, rather than the
	** ovc variables that were passed in.  Only the loser's ovc changes.
	*/
refill:
	/* the valuecode to be filled in starts inside the ovc
	** extends beyond it into the next field (eov) for pointer sorts
	*/
	ovcleft = OVCEOVSIZE;	/* how much of ovc[+eov] has to be filled in */
	win_ovcp = (byte *) &win_temp.ovc + sizeof(ovcoff_t);
	curr_ovcp = (byte *) &curr_temp.ovc + sizeof(ovcoff_t);
	startkey = key;
	/* Calculate how much of the field is left. Some of it may have been
	** put into a previous ovc+eov by a prior refilling recode.
	*/
	bytes_so_far = (int) sizeof(valuecode_t) * (offset - key->offset);

	/* one or two bytes (if in a record sort's with three byte ovcs)
	** may have been put in the previous ovc
	** bump over that amount
	*/
	if (key->flags & FIELD_ODDSTART)
	{
	    ASSERT(offset > key->offset);
	    bytes_so_far -= key->flags & FIELD_ODDSTART;
	}

	for (;;)	/* loop until ovc+eov is fill, ~~ ovcleft == 0 */
	{
	    /*
	    ** The following section is duplicated below in summarize(),
	    ** in init_hash_item*(), and (simplified) in compile_ovc_init()
	    */
	    i = key->position;
#ifdef MOVABLE_KEYS
	    if (key->flags & FIELD_SHIFTY)	/* position of this field isn't constant */
	    {
		unsigned	win_position, curr_position;

		win_position = win_node->id.fpt->fpt[i];
		curr_position = curr_node->id.fpt->fpt[i];
		if (win_position == FPT_UNSEEN)
		    win_key = fill_fpt_entry(sort, win_node->id.fpt, i);
		else
		    win_key = win_node->id.fpt->record + win_position;
		if (curr_position == FPT_UNSEEN)
		    curr_key = fill_fpt_entry(sort, curr_node->id.fpt, i);
		else
		    curr_key = curr_node->id.fpt->record + curr_position;
	    }
	    else			/* no variable fields before this key */
	    {
		if (sort->anyfpt)
		{
		    win_key = (byte *) win_node->id.fpt->record + i;
		    curr_key = (byte *) curr_node->id.fpt->record + i;
		}
		else
#endif /* MOVABLE_KEYS */
		{
#ifdef POINTER_SORT
		    win_key = win_item->rec + i;
		    curr_key = curr_item->rec + i;
#else
		    win_key = win_rec + i;
		    curr_key =  curr_rec + i;
#endif /* !POINTER_SORT */
		}
#ifdef MOVABLE_KEYS
	    }
#endif
	    /* end of duplicated section */

	    /*
	    ** First copy up to min(bytes-till-eov's-end, field length)
	    ** into ovc.vc+eov. There are three cases for variable fields:
	    **	1)  the winner is shorter
	    **	2)	the current node is shorter
	    **	3)	they are the same length
	    ** We don't bother checking for variable length fields if this
	    ** is the record sort version of recode
	    */
#ifdef VARIABLE_SORT
	    if (key->flags & (FIELD_VARIABLE | FIELD_DELIMSTRING))
	    {
		int	win_delim_byte;
		int	curr_delim_byte;

		/*
		 * A non-fixed-length field with 'ignore-me' trailing pad chars
		 * cannot share an ovc or eov with any other field. These types
		 * 'jump' in the strings to the first point of different, and
		 * so they must have their own offset.
		 */
		if (ovcleft != OVCEOVSIZE)
		{
		    do {
			*win_ovcp++ = 0;
			*curr_ovcp++ = 0;
			ovcleft--;
		    } while (ovcleft > 0);
		    break;
		}
		fdelim = key->delimiter;
		/*
		** Figure where each record ends, since fields are
		** terminated by the record delimiter or an overall
		** record length, in addition to the field delimiter.
		**	Stream (delimited) records are given a fake length
		** which extends beyond the ovc+eov, and so won't
		** ever matter.
		**	Fixed-length or length-prefix records are given the
		** same record delimiter as the field delimiter.
		*/
#ifdef MOVABLE_KEYS
		if (sort->anyfpt)
		{
		    win_endrec = win_item->id.fpt->record;
		    curr_endrec = curr_item->id.fpt->record;
		}
		else
#endif /* MOVABLE_KEYS */
		{
		    win_endrec = win_item->rec;
		    curr_endrec = curr_item->rec;
		}

		if (sort->record.flags & RECORD_STREAM)
		{
		    /*rdelim = sort->record.delimiter;*/
		    /* The -1 excludes the record delimiter from consideration
		     * as part of the key, if the key happens to be the last field
		     */
		    win_endrec += win_item->extra.var_extra.var_length - 1;
		    curr_endrec += curr_item->extra.var_extra.var_length - 1;
		}
		else
		{
		    /*rdelim = fdelim;*/

		    if (sort->record.flags & RECORD_VARIABLE)
		    {
			win_endrec += win_item->extra.var_extra.var_length;
			curr_endrec += curr_item->extra.var_extra.var_length;
		    }
		    else
		    {
			win_endrec += sort->edited_size;
			curr_endrec += sort->edited_size;
		    }
		}
		
		/*
		** Copy and condition a delimited string
		** - don't go beyond the end of the field: create virtual pad
		**	characters for the shorter string's value
		** - don't go beyond eov
		** - pad with an extra byte if the data ends at an odd position
		** - copy the delimiter too, after all the data and pads
		** - complement the value codes if this key's order_by is :desc
		*/
#if 1
		/*
		 * Find the interesting comparison point for these two strings:
		 * where they differ or at least one of them ends.
		 */
		for (i = 0;
		     win_key < win_endrec && curr_key < curr_endrec;
		     win_key += 4, curr_key += 4, i++)
		{
		    win_ovc = ulw(win_key);
		    curr_ovc = ulw(curr_key);
		    if (win_ovc != curr_ovc)
			break;
		    if (FIND_ZEROBYTE(win_ovc ^ key->delimword) ||
		        FIND_ZEROBYTE(curr_ovc ^ key->delimword))
			break;
		}
		offset = i * (sizeof(u4) / sizeof(valuecode_t)) + key->offset;

		/*
		 * If the value is beyond the end of the record then
		 * replace the irrelevent bytes with the field delimiter.
		 * This removes the `endrec' check from the later delim loop.
		 */
		i = win_key - win_endrec;
		if (i >= 0)
		    win_ovc = key->delimword;
		else
		{
		    i += sizeof(u4);
		    if (i > 0)
			win_ovc = (win_ovc & ~((1 << (i << 3)) - 1)) |
				  (key->delimword & ((1 << (i << 3)) - 1));
		}
		i = curr_key - curr_endrec;
		if (i >= 0)
		    curr_ovc = key->delimword;
		else
		{
		    i += sizeof(u4);
		    if (i > 0)
			curr_ovc = (curr_ovc & ~((1 << (i << 3)) - 1)) |
				   (key->delimword & ((1 << (i << 3)) - 1));
		}

		/*
		 *   We know that the current words (win_ovc and curr_ovc)
		 * contain either different bytes or at least one of the
		 * string ends somewhere inside them.  Now we'll put the
		 * field pad char into each byte of each ovc which is
		 * past the end of the field. This conditions the values
		 * by converting all three ways in which a delimited string
		 * may end (field delim, record delim, or record length)
		 * into a single uniform trailing format (the pad).
		 * While the conditioning is not needed for this recode, it
		 * is needed so that the loser's ovc will be correctly
		 * comparable with another key which happens to differ
		 * at the same offset: if the ovcs contain the same data and
		 * different delimiters or slightly different lengths then
		 * the comparison would not be 'equality'.
		 *   This loop does no memory stores, so it ought to optimize.
		 */
		win_delim_byte = curr_delim_byte = -1;
		for (i = 24, mask = ~0; i >= 0; i -=8, mask >>= 8)
		{
		    if (win_delim_byte < 0 &&
			((win_ovc >> i) & 0xff) == fdelim)
		    {
			/* win_key ends here, and was equal to curr_key
			 * in any earlier bytes: 
			 * put the normalized delim after 'real' data in ovc.
			 */
		    	win_ovc = (win_ovc & ~mask) | (key->padword & mask);
			win_delim_byte = (int) i;
			if (curr_delim_byte >= 0)
			    break;
		    }
		    if (curr_delim_byte < 0 &&
			((curr_ovc >> i) & 0xff) == fdelim)
		    {
		    	curr_ovc = (curr_ovc & ~mask) | (key->padword & mask);
			curr_delim_byte = (int) i;
			if (win_delim_byte >= 0)
			    break;
		    }
		}

		/*
		 * Now, after all delimiter have been conditioned, we
		 * can compare the ovc values. If they are equal and
		 * both strings end in the last word then the strings are equal.
		 * If the ovcs are different then we need to check if they
		 */

		if (win_delim_byte >= 0 && curr_delim_byte >= 0)
		{
		    /* Both strings end in these words. Nothing to do right now
		     */
		}
		else if (win_ovc == curr_ovc)
		{
		    /*
		     * The 'interesting' word is equal (after conditioning) and
		     * exactly one of the strings ends in this word.
		     * The other string extends into later bytes. Scan down
		     * the longer string until we find either its end or
		     * a non-pad character.  Build a ovc comparing the last
		     * word of the longer string to the pad.
		     */
		    if (win_delim_byte > curr_delim_byte)  /* win ends 1st */
		    {
			curr_ovc = win_ovc = key->padword;
			for (i = curr_endrec - curr_key; i > 0; i -= sizeof(u4))
			{
			    curr_key += sizeof(u4);
			    offset += sizeof(u4) / sizeof(valuecode_t);
			    if (curr_key == curr_endrec)
			    {
				ASSERT(curr_ovc == win_ovc);
				break;
			    }
			    
			    curr_ovc = ulw(curr_key);
			    if (curr_ovc == key->padword)
				continue;

			    /*
			     * The word is not all pad bytes, so we are
			     * definitely looking at the last word. We still
			     * have to condition it in order to determine
			     * the relative ordering of curr_key vs. win_key.
			     *
			     * There is at least one valid by the in the word:
			     * if it is the delimiter then the string are equal
			     */
			    if ((curr_ovc >> 24) == fdelim)
			    {
				curr_ovc = win_ovc;
			    }
			    else
			    {
				/*
				 * Have we reached the end of the key field,
				 * either because we've hit the end of
				 * the record without a field delimiter,
				 * or we're at a word containing a field delim?
				 */
				if (((curr_ovc >> 16) & 0xff) == fdelim)
				    i = 3;
				else if (((curr_ovc >> 8) & 0xff) == fdelim)
				    i = 2;
				else if ((curr_ovc & 0xff) == fdelim)
				    i = 1;
				if (i < sizeof(u4))
				{
				    curr_ovc = (curr_ovc & ~((1 << (i << 3)) - 1)) |
					       (key->padword & ((1 << (i << 3)) - 1));
#if 0
				    if ((curr_ovc >> 16) == (key->padword & 0xffff))
				    {
					curr_ovc = (curr_ovc << 16) |
						   (key->padword & 0xffff);
					offset++;
				    }
#endif
				}
			    }

			    break;	/* done searching the trailing pads */
			}
		    }
		    else	/* current item is shorter, scan win_item */
		    {
			win_ovc = curr_ovc = key->padword;
			for (i = win_endrec - win_key; i > 0; i -= sizeof(u4))
			{
			    win_key += sizeof(u4);
			    offset += sizeof(u4) / sizeof(valuecode_t);
			    if (win_key == win_endrec)
			    {
				ASSERT(win_ovc == curr_ovc);
				break;
			    }
			    
			    win_ovc = ulw(win_key);
			    if (win_ovc == key->padword)
				continue;

			    /*
			     * The word is not all pad bytes, so we are
			     * definitely looking at the last word. We still
			     * have to condition it in order to determine
			     * the relative ordering of curr_key vs. win_key.
			     *
			     * There is at least one valid by the in the word:
			     * if it is the delimiter then the string are equal
			     */
			    if ((win_ovc >> 24) == fdelim)
			    {
				win_ovc = curr_ovc;	/* all pad characters */
			    }
			    else
			    {
				/*
				 * Have we reached the end of the key field,
				 * either because we've hit the end of
				 * the record without a field delimiter,
				 * or we're at a word containing a field delim?
				 * Set 'i' to the number of bytes beyond the
				 * last data byte we've accessed.
				 */
				if (((win_ovc >> 16) & 0xff) == fdelim)
				    i = 3;
				else if (((win_ovc >> 8) & 0xff) == fdelim)
				    i = 2;
				else if ((win_ovc & 0xff) == fdelim)
				    i = 1;
				if (i < sizeof(u4))
				{
				    /*
				     * Set all bytes past (to the right of)
				     * the last data byte to the pad.
				     */
				    win_ovc = (win_ovc & ~((1 << (i << 3)) - 1)) |
					      (key->padword & ((1 << (i << 3)) - 1));
#if 0
				    if ((win_ovc >> 16) == (key->padword & 0xffff))
				    {
					win_ovc = (win_ovc << 16) |
						  (key->padword & 0xffff);
					offset++;
				    }
#endif
				}
			    }

			    break;	/* done searching the trailing pads */
			}
		    }
		}

		i = 4;

#if 0		/* can't do this - if an ovc+eov contains any part of
		 * a non-fixed length, padded string then the entire
		 *  ovc+eov must have data for that key only
		 * Jumping offsets in an OVC_EOV1 recode is too much work.
		 */
		/*
		 * Adjust the offset? The difference between the strings,
		 * if any, may be in the second half of the word
		 * and so would need the next higher offset.
		 */
		if ((win_ovc >> 16) == (curr_ovc >> 16) &&
		    offset < key->ending_offset - 1)
		{
		    win_key += 2;
		    curr_key += 2;
		    win_ovc  = (win_ovc  << 16) | (key->padword & 0xffff);
		    curr_ovc = (curr_ovc << 16) | (key->padword & 0xffff);
		    offset++;
		    i = 2;
		}
#endif
		/* If the ovcs are equal here then 
		 * 1- win_key and curr_key will both
		 *    be at the end of their respective strings
		 * 2- the entire strings are equal
		 *   Give them the offset of the end of the string, so that
		 * the next recode will use the next key, and not this one.
		 */
		if (win_ovc == curr_ovc)
		{
#if 0
		    fprintf(stderr, "delimstring %x (%.*s) == %x(%.*s)\n",
			win_item, win_item->extra.var_extra.var_length - 1,
			win_item->rec,
			curr_item, curr_item->extra.var_extra.var_length - 1,
			curr_item->rec);
#endif

		    offset = key[1].offset;
		    if (offset & 1)
			i = 2;
		    offset -= i / 2;
		}

		if (key->flags & FIELD_DESCENDING)
		{
		    win_ovc = ~win_ovc;
		    curr_ovc = ~curr_ovc;
		}

		/*ASSERT(ovcleft == i);*/
		if (i == 4)
		{
		    usw(win_ovcp, win_ovc);
		    usw(curr_ovcp, curr_ovc);
		}
		else
		{
#if 0
		    fprintf(stderr, "delimstring %x %x doesn't fill ovc+eov\n",
			win_item, curr_item);
#endif
		    ush(win_ovcp, win_ovc >> 16);
		    ush(curr_ovcp, curr_ovc >> 16);
		}
		win_ovcp += i;
		curr_ovcp += i;
		ovcleft -= i;

#else	/* use old (broken) delimstring code */

		for (i = 0; ovcleft > 0; i++)
		{
		    if (win_key >= win_endrec ||
			win_key[0] == fdelim ||
			win_key[0] == rdelim)
		    {
			/* winner ends here - if the current does too (case 3)
			** then we are done copying/conditioning this key
			*/
			if (curr_key >= curr_endrec ||
			    curr_key[0] == fdelim ||
			    curr_key[0] == rdelim)
			    break;
			/*
			** case 1: If this is the first byte of data that will
			** be in ovc+eov, remember its offset
			*/
			do
			{
			    ovcleft--;
			    *win_ovcp = key->pad;
			    *curr_ovcp = curr_key[0];
			    curr_key++;
			    win_ovcp++;
			    curr_ovcp++;
			    i++;
			} while (curr_key[0] != fdelim &&
				 curr_key[0] != rdelim &&
				 ovcleft > 0);
			break;
		    }
		    else if (curr_key >= curr_endrec ||
			     curr_key[0] == fdelim ||
			     curr_key[0] == rdelim)
		    {
			/* case 2: current ends here (and winner does not;
			** that would have been detected just above)
			** pad current key's ovc+eov with 'blanks'
			*/
			do
			{
			    ovcleft--;
			    *curr_ovcp = key->pad;
			    *win_ovcp = win_key[0];
			    win_key++;
			    win_ovcp++;
			    curr_ovcp++;
			    i++;
			} while (win_key[0] != fdelim &&
				 win_key[0] != rdelim &&
				 ovcleft > 0);
			break;
		    }
		    else
		    {
			ovcleft--;
			*win_ovcp = win_key[0];
			*curr_ovcp = curr_key[0];
			win_key++;
			curr_key++;
			win_ovcp++;
			curr_ovcp++;
		    }
		}

		/*
		** Negate the value codes of descending keys, so that the
		** ovc comparisons need not be aware of the key boundaries
		** or their ascending/descending-ness.
		*/
		if (key->flags & FIELD_DESCENDING)
		{
		    win_ovcp -= i;
		    curr_ovcp -= i;
		    while (--i >= 0)
		    {
			*win_ovcp = -*win_ovcp;
			*curr_ovcp = -*curr_ovcp;
			win_ovcp++;
			curr_ovcp++;
		    }
		}
#endif

#if defined(NOLONGER)
		/*
		**	We can combine two keys into the same value code only if
		** their offsets are adjacent.  This rarely happens for
		** variable length fields since they are given a large range
		** of value codes (typically several thousand) and
		** the offset would  be adjacent to the following key's
		** only when the field was full.
		** Currently the code does not optimize for this case.
		** 	To prevent incorrectly combining value codes append a
		** zero if the strings end in the middle of a v.c.  (ovcleft
		** is odd). This ensures that the delimiter will always be
		** in the high byte of a variable length string's value code.
		*/
		if (ovcleft > 0)
		{
		    /*
		    ** The strings ended before we filled up the eov, so
		    ** there is room to add the delimiters.
		    ** If the delimiter would be in the low byte, insert a null
		    ** to force the delimiter to the high byte of the next ovc. 
		    ** Then add the field delimiter if there is room.  If there
		    ** isn't room then the delimiter will be put into the first
		    ** byte of the ovc by the next 'refill' recode.
		    **	The conditioned delimiter is always the field delimiter,
		    ** even if this key ended with the record delimiter. It's
		    ** easier here, and down below in the delimiter check.
		    **	Neither the spacer nor the delimiter are complemented
		    ** in a descending sort.  Since they will always be the same
		    ** there's no need to bother with complementing them.
		    */
		    if ((ovcleft & 1))	/* ovcleft in {1, 3, 5} */
		    {
			ovcleft--;
			*win_ovcp++ = 0xea;	/* the value doesn't matter - */
			*curr_ovcp++ = 0xea;	/* tis the same in both nodes */
		    }
		    if (ovcleft > 0)
		    {
			ovcleft--;
			*win_ovcp++ = fdelim;
			*curr_ovcp++ = fdelim;
		    }
		}
#endif /* NOLONGER */
	    }
	    else	/* it is a fixed length type */
#endif /* VARIABLE_SORT */
	    {
		if (key->type == typeFloat)
		{
		    /*
		    ** Floating point compares have -0.0 == +0.0.
		    ** Set the sign bit for both kinds of zero.
		    ** If a number is negative then the excess-128 or
		    ** excess-1024 exponent needs to have its sign bit
		    ** toggled. -2.3e+9 < -2.3e-2, but +2.3e+9 > +2.3e-2
		    ** We also have to deal with the 'hidden' bit in the
		    ** mantissa of all normalized numbers.
		    ** What are the nan/inf/denorm inequality issues here?
		    */
		    win_float = ulw(win_key); 
		    COND_FLOAT(win_float);
		    win_key = (byte *) &win_float + bytes_so_far;
		    curr_float = ulw(curr_key);
		    COND_FLOAT(curr_float);
		    curr_key = (byte *) &curr_float + bytes_so_far;
		}
		else if (key->type == typeDouble)
		{
		    win_double[0] = ulw(win_key); 
		    win_double[1] = ulw(win_key + 4); 
		    COND_DOUBLE(win_double[0], win_double[1]);
		    win_key = (byte *) &win_double[0] + bytes_so_far;
		    curr_double[0] = ulw(curr_key); 
		    curr_double[1] = ulw(curr_key + 4); 
		    COND_DOUBLE(curr_double[0], curr_double[1]);
		    curr_key = (byte *) &curr_double[0] + bytes_so_far;
		}
		else
		{
		    /*
		    ** We may have already process some of the preceding bytes
		    ** in this key and found them to be equal. Skip over however
		    ** many were done by a prior call to recode().
		    */
		    win_key += bytes_so_far;
		    curr_key += bytes_so_far;
		}
		
		i = key->length - bytes_so_far;
		if (i > ovcleft)
		    i = ovcleft;

		switch (i)
		{
		  default:
		    die("recode:excessive fixed length:%d", i);
		    break;

#if defined(POINTER_SORT)
#if 0	/* !defined(VARIABLE_SORT) */
		  case OVCEOVSIZE:
			usw(win_ovcp + 1, ulw(win_key + 1));
			usw(curr_ovcp + 1, ulw(curr_key + 1));
		  	temp = win_key[5];
			curr_ovcp[5] = curr_key[5]; win_ovcp[5] = temp;
			goto condition_check;
			break;

		  case 5:
			usw(win_ovcp + 1, ulw(win_key + 1));
			usw(curr_ovcp + 1, ulw(curr_key + 1));
			goto condition_check;
#endif
		  case 4:
			ush(win_ovcp + 1, ulh(win_key + 1));
			ush(curr_ovcp + 1, ulh(curr_key + 1));
		  	temp = win_key[3];
			curr_ovcp[3] = curr_key[3]; win_ovcp[3] = temp;
			goto condition_check;
#endif /* POINTER_SORT */

		  case 3:
			temp = ulh(win_key + 1);
			ush(curr_ovcp + 1, ulh(curr_key + 1));
			ush(win_ovcp + 1, temp);
			goto condition_check;
		  case 2: 
		  	temp = win_key[1];
			curr_ovcp[1] = curr_key[1]; win_ovcp[1] = temp;
		  case 1:
condition_check:
		    if (bytes_so_far == 0 && (key->flags & FIELD_TOGGLESIGN))
		    {
			temp = 0x80 ^ win_key[0];
			curr_ovcp[0] = 0x80 ^ curr_key[0];
			win_ovcp[0] = temp;
		    }
		    else
		    {
			temp = win_key[0];
			curr_ovcp[0] = curr_key[0];
			win_ovcp[0] = temp;
		    }
		    break;

		  case 0:
		    break;
		}
		
		/*
		** Complement the value code for order_by:d[escending]
		*/
		if (key->flags & FIELD_DESCENDING)
		{
		    int j;

#if defined(POINTER_SORT)
		    if (i == 4)
		    {
			temp = ulw(win_ovcp);
			usw(curr_ovcp, ~ulw(curr_ovcp));
			usw(win_ovcp, ~temp);
		    }
		    else
#endif
		    {
			if (i >= 2)
			{
			    temp = ulh(win_ovcp);
			    ush(curr_ovcp, ~ulh(curr_ovcp));
			    ush(win_ovcp, ~temp);
			    j = 2;
			}
			else
			    j = 0;
			while (j < i)
			{
			    temp = win_ovcp[j];
			    curr_ovcp[j] = ~curr_ovcp[j];
			    win_ovcp[j] = ~temp;
			    j++;
			}
		    }
		}
		
		ovcleft -= i;
		win_ovcp += i;
		curr_ovcp += i;
	    }

	    if (ovcleft == 0)
		break;

	    /*
	    ** Advance to the next key.  If that was the last key
	    ** then fill the rest of ovc+eov with zeroes.
	    ** If there are more keys then reset the count of the
	    ** number of bytes of this key which were 'handled' by
	    ** prior offset value codes.
	    */
	    key++;
	    if (key->type == typeEOF)
	    {
		if (ovcleft == 0)	/* what was this for, long time ago? */
		{
		    die("eof key fill but ovcleft == 0");
#if 0
		    curr_item->ovc = BUILD_OVC(offset, 0);
		    return (0);
#endif
		}
		if (ovcleft >= 2)
		{
		    ush(win_ovcp, 0);
		    ush(curr_ovcp, 0);
		    ovcleft -= 2;
		    win_ovcp += 2;
		    curr_ovcp += 2;
		}
		while (--ovcleft >= 0)
		{
		    *win_ovcp = 0;
		    *curr_ovcp = 0;
		    win_ovcp++;
		    curr_ovcp++;
		}
		break;
	    }
	    else
	    {
		bytes_so_far = 0;
	    }
	}

#if defined(POINTER_SORT)
	win_eov = win_temp.extra.var_extra.var_eov;
	curr_eov = curr_temp.extra.var_extra.var_eov;
#endif
	win_ovc = win_temp.ovc & VALUE_MASK;
	curr_ovc = curr_temp.ovc & VALUE_MASK;

	/* Bump up the offset to go past the value code already known to be eq
	 */
	offset++;

	/*
	** Finally we have ovc+eov for both nodes.
	** Figure out who is the winner, and put the
	** ovc+eov of the loser back into its node.
	** The winner's ovc must not be changed.
	*/
	key = startkey;
	if ((key->flags & FIELD_DELIMSTRING) &&
	    ((win_ovc >> 8) == key->delimiter))
	{
	    die("recode_(variable):delimiter in refill ovc");
	    /*
	    ** The high byte of the ovc has the delimiter for the
	    ** variable length field which just ended.  The low byte
	    ** of the ovc has the beginning of the next key, so we
	    ** have to use the next offset. Variable length keys
	    ** almost always have offset 'gaps' between the end of
	    ** their data and the start of the next key. Fixed length
	    ** keys are always packed densely; they have no such gap
	    ** after their data.
	    */
	    key++;
	    offset = 49152;
	    if (key->type == typeEOF)
	    {
keys_equal:
		if (args->win_id <= args->curr_id)    /* if in stable order already */
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
	    offset = key->offset;
	}

	/*
	 * See if the value code at this new offset distinguishes the keys.
	 */
	if (win_ovc < curr_ovc)
	{
	    ASSIGN_OVC(CURR, offset, curr_ovc);
	    ASSIGN_EOV(CURR, curr_eov);		/* only assign eov of loser */
	    return (0);		/* indicate winner still winner - no swap */
	}
	else if (win_ovc > curr_ovc)
	{
	    ASSIGN_OVC(WIN, offset, win_ovc);
	    ASSIGN_EOV(WIN, win_eov);		/* only assign eov of loser */
	    return (1);		/* indicate prior winner lost the comparison */
	}
	
	/*
	** Find the key which contains the recently-incremented offset.
	** Sometimes two adjacent one-byte keys have the same offset;
	** the second key is must not be 'seen' here, so it is
	** possible for key to be advanced twice: once to get to the
	** second small field, and again to get to the field
	** corresponding to the new offset.
	**    Change the second 'if' to a 'while' if offsets
	** may hold more than two bytes.
	*/ 
	if (offset > key->ending_offset /*offset >= key[1].offset*/)
	{
	    key++;
	    if (key->type == typeEOF)
		goto keys_equal;
	    if (offset > key->ending_offset /*offset == key[1].offset*/)	/* two fields w/ same offset */
	    {
		key++;
		if (key->type == typeEOF)
		    goto keys_equal;
#if defined(RECORD_SORT)
		if (offset > key->ending_offset /*offset == key[1].offset*/)	/* 3 fields w/ same offset */
		{
		    key++;
		    if (key->type == typeEOF)
			goto keys_equal;
		}
#endif
	    }
	}
	ASSERT(key < &sort->keys[sort->keycount]);
#if defined(RECORD_SORT)
	goto refill;		/* record sort has no eov, get next ovc */
#else
	/*
	** the remainder of this function is only for both kinds
	** of pointer sorts
	*/
	goto eov_part1;


  case OVC_EOV1:
	/*
	** ovc matches, compare first part of eov.
	** This case is reached in both variable and fixed length pointer sorts.
	*/
	win_eov = win_item->extra.var_extra.var_eov;
	curr_eov = curr_item->extra.var_extra.var_eov;

eov_part1:

	win_ovc = win_eov;
	curr_ovc = curr_eov;
	offset++;
	if ((key->flags & FIELD_DELIMSTRING) &&
	    ((win_ovc >> 8) == key->delimiter))
	{
	    die("recode_(variable):delimiter in eov1 ovc");
	    /*
	    ** The high byte of the ovc has the delimiter for the
	    ** variable length field which just ended.  The low byte
	    ** of the ovc has the beginning of the next key, so we
	    ** have to use the next offset. Variable length keys
	    ** almost always have offset 'gaps' between the end of
	    ** their data and the start of the next key. Fixed length
	    ** keys are always packed densely; they have no such gap
	    ** after their data.
	    */
	    key++;
	    if (key->type == typeEOF)
		goto keys_equal;
	    offset = key->offset;
	}
	
	if (win_ovc < curr_ovc)
	{
	    curr_item->ovc = BUILD_OVC(offset, curr_ovc);
	    ASSIGN_EOV(CURR, curr_eov);	/* only assign eov of loser */
	    return (0);			/* indicate winner still winner */
	}
	else if (win_ovc > curr_ovc)
	{
	    win_item->ovc = BUILD_OVC(offset, win_ovc);
	    ASSIGN_EOV(WIN, win_eov);	/* only assign eov of loser */
	    return (1);			/* indicate winner lost comparison */
	}
	/* else comparison couldn't be resolved, fall through to next offset */

	if (offset > key->ending_offset /*offset >= key[1].offset*/)
	{
	    key++;
	    if (key->type == typeEOF)
		goto keys_equal;
	    if (offset > key->ending_offset /*offset == key[1].offset*/)	/* two fields w/ same offset */
	    {
		key++;
		if (key->type == typeEOF)
		    goto keys_equal;
	    }
	}

	/* for helping skip merges both fixed pointer and variable sorts
	 * have only one extra valuecode
	 */
	goto refill;	

#if defined(THREE_PART_POINTER_EOV)
  case OVC_EOV2:
	/*
	** ovc & first part of eov matches, try last part of eov
	** This case is reached ONLY in fixed length pointer sorts.
	*/
	win_eov = win_item->extra.eov;
	curr_eov = curr_item->extra.eov;
eov_part2:
	win_ovc = win_eov & VALUE_MASK;
	curr_ovc = curr_eov & VALUE_MASK;
	if ((key->flags & FIELD_VARIABLE) && ((win_ovc >> 8) == key->delimiter))
	{
	    /*
	    ** The high byte of the ovc has the delimiter for the
	    ** variable length field which just ended.  The low byte
	    ** of the ovc has the beginning of the next key, so we
	    ** have to use the next offset. Variable length keys
	    ** almost always have offset 'gaps' between the end of
	    ** their data and the start of the next key. Fixed length
	    ** keys are always packed densely; they have no such gap
	    ** after their data.
	    */
	    key++;
	    if (key->type == typeEOF)
		goto keys_equal;
	    offset = key->offset;
	}
	
	offset++;
	if (win_ovc < curr_ovc)
	{
	    curr_item->ovc = BUILD_OVC(offset, curr_ovc);
	    curr_item->extra.eov = curr_eov;	/* only assign eov of loser */
	    return (0);			/* indicate winner still winner */
	}
	else if (win_ovc > curr_ovc)
	{
	    win_item->ovc = BUILD_OVC(offset, win_ovc);
	    win_item->extra.eov = win_eov;	/* only assign eov of loser */
	    return (1);		/* indicate winner lost comparison */
	}
	/* else comparison couldn't be resolved, refill ovc+eov */

	if (offset >= key[1].offset)
	{
	    key++;
	    if (offset == key[1].offset)	/* two fields w/ same offset */
		key++;
	}
	if (key->type == typeEOF)
	    goto keys_equal;
	goto refill;	/* back up to the top to get the next ovc+eov */
#endif /* THREE_PART_POINTER_EOV */
    }
    /* NOTREACHED */
#endif /* POINTER_SORT */
}

#if !defined(VARIABLE_SORT)	/* a variable sort uses summarize_pointer */
/*
** summarize_{record,pointer}	- accumulate summary fields when equal records found
**
** 	Called by recode when the keys in both records are equal and
**	there are duplicates to delete and/or summary fields to update
**
**	Returns
**		0 - overflow narrowly averted; keep the losing item
**		1 - delete the losing item
**	Side Effects:
**		updates win_item iff curr_item is deleted (i.e. it returns 1)
*/
#ifdef POINTER_SORT
int summarize_pointer(sort_t	*sort,
		      item_t	*win_item,
		      CONST item_t	*curr_item)
#else /* is RECORD_SORT */
int summarize_record (sort_t *sort,
		      byte *win_rec,
		      CONST byte *curr_rec)
#endif
{
    int		i;
    i4		win_i4, curr_i4;
    u4		win_u4, curr_u4;
    i8		win_i8, curr_i8;
    u8		win_u8, curr_u8;
    short	*fp;
    field_t	*field;
    byte	*win_field, *curr_field;
    struct summation
    {
	byte		*address;
	anyval_t	value;
    } summation[10], *sum;

    for (fp = sort->summarize, sum = summation; *fp >= 0; fp++, sum++)
    {
	field = &sort->record.fields[*fp];
	/*
	** The following section is duplicated from recode
	*/
	i = field->position;
#ifdef MOVABLE_FIELDS
	if (field->flags & FIELD_SHIFTY)	/* position of this field isn't constant */
	{
	    unsigned	win_position, curr_position;

	    win_position = win_node->id.fpt->fpt[i];
	    curr_position = curr_node->id.fpt->fpt[i];
	    if (win_position == FPT_UNSEEN)
		win_field = fill_fpt_entry(sort, win_node->id.fpt, i);
	    else
		win_field = win_node->id.fpt->record + win_position;
	    if (curr_position == FPT_UNSEEN)
		curr_field = fill_fpt_entry(sort, curr_node->id.fpt, i);
	    else
		curr_field = curr_node->id.fpt->record + curr_position;
	}
	else			/* no variable fields before this field */
	{
	    if (sort->anyfpt)
	    {
		win_field = (byte *) win_node->id.fpt->record + i;
		curr_field = (byte *) curr_node->id.fpt->record + i;
	    }
	    else
#endif /* MOVABLE_FIELDS */
	    {
#ifdef POINTER_SORT
		win_field = win_item->rec + i;
		curr_field = curr_item->rec + i;
#else
		win_field = win_rec + i;
		curr_field =  curr_rec + i;
#endif /* !POINTER_SORT */
	    }
#ifdef MOVABLE_FIELDS
	}
#endif
	/* end of duplicated section from recode */

	switch (field->type)
	{
	  case typeI4:
	    win_i4 = ulw(win_field);
	    curr_i4 = ulw(curr_field);
	    sum->value.i4val = win_i4 + curr_i4;

	    /* overflow in signed addition occurs when the operands have
	    ** the same sign and the sum has a different sign
	    */
	    if (((win_i4 ^ curr_i4) & SIGN_BIT) == 0 &&
		((win_i4 ^ sum->value.i4val) & SIGN_BIT) != 0)
		break;
	    /* no overflow, save this field for later possible use
	    */
	    sum->address = win_field;
	    continue;

	  case typeU4:
	    win_u4 = ulw(win_field);
	    curr_u4 = ulw(curr_field);
	    sum->value.u4val = win_u4 + curr_u4;
	    if (sum->value.u4val < win_u4 || sum->value.u4val < curr_u4)
		break;			/* overflow occured */
	    sum->address = win_field;
	    continue;

	  case typeI8:
	    win_i8 = ((i8) ulw(win_field) << 32) + ulw(win_field + 4);
	    curr_i8 = ((i8) ulw(curr_field) << 32) + ulw(curr_field + 4);
	    sum->value.i8val = win_i8 + curr_i8;
	    if (((win_i8 ^ curr_i8) & (1LL << 63)) == 0 &&
		((win_i8 ^ sum->value.i8val) & (1LL << 63)) != 0)
		break;
	    sum->address = win_field;
	    continue;

	  case typeU8:
	    win_u8 = ((i8) ulw(win_field) << 32) + ulw(win_field + 4);
	    curr_u8 = ((i8) ulw(curr_field) << 32) + ulw(curr_field + 4);
	    sum->value.u8val = win_u8 + curr_u8;
	    if (sum->value.u8val < win_u8 || sum->value.u8val < curr_u8)
		break;			/* overflow occured */
	    sum->address = win_field;
	    continue;

	  default:
	    die("summarize: unsupported type %d", field->type);
	    break;
	}

	/*
	** If we get here then the summation would overflow, so
	** don't do it.  Tell the callers (recode() and merge_???_out)
	** to keep both items.
	*/
	return (0);
    }

    for (fp = sort->summarize, sum = summation; *fp >= 0; fp++, sum++)
    {
	memmove(sum->address, &sum->value, sort->record.fields[*fp].length);
    }

    return (1);		/* summarization okay, delete the loser */
}
#endif /* !VARIABLE_SORT, uses the same summarize as POINTER_sort */
