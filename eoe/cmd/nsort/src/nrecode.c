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
 * nrecode.c	- general purpose offset value code computation (better version)
 *
 *	Computes the first (or next) offset value code of a pair of records,
 *	relative to the key ordering specified in the sort descriptor
 *
 *	$Ordinal-Id: nrecode.c,v 1.13 1996/10/02 23:31:17 charles Exp $
 */
#ident	"$Revision: 1.1 $"

#include "otcsort.h"
#include "ovc.h"

/*
** recode	- compute the next (or first) offset value code of
**		  *win_item relative to *curr_item. Compare successive ovc's
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
unsigned nrecode_variable(sort_t *sort,
			recode_args_t *args,
			item_t *win_item,
			item_t *curr_item)
#elif defined(POINTER_SORT)
unsigned nrecode_pointer(sort_t *sort,
			recode_args_t *args,
			item_t *win_item,
			item_t *curr_item)
#else /* is RECORD_SORT */
unsigned nrecode_record(sort_t *sort,
		       recode_args_t *args,
		       byte *win_rec,
		       byte *curr_rec)
#endif
{
    keydesc_t	*key;
    keydesc_t	*startkey;
    ptrdiff_t	i;
    byte	*win_key, *curr_key;	/* 'temporaries' */
    unsigned 	win_temp, curr_temp;
    unsigned	win_ovc, curr_ovc;
    int		ovcleft;
    unsigned	offset;			/* stack (darn!) */
    unsigned	bytes_so_far;
    u4		win_float;
    u4		curr_float;
    u4		win_double[2];
    u4		curr_double[2];
#if defined(POINTER_SORT)
    unsigned	win_eov, curr_eov;
#endif
#if defined(VARIABLE_SORT)
    byte	*win_endrec, *curr_endrec;
    char	fdelim;
    unsigned	mask;	
#endif

#ifdef DEBUG_RECODE
    node_t	win_save, curr_save;

    win_save = *win_item;
    curr_save = *curr_item;
#endif

    /*
     * Translate the large, scaled-by-64K ovc.offset to
     * a small 'offset' of ovc.offset. It is easier for
     * humans to figure out.  The first field's offset is 0.
     */
#ifdef POINTER_SORT
    offset = GET_OVC_OFFSET(win_item->ovc);
#else
    offset = GET_OVC_OFFSET(args[0].win_ovc);
#endif
    key = sort->keys;

    if (offset < HASH_LOWEST_OFFSET)	/* a normal ovc offset? */
    {
	/* find the key containing the current offset. The keys are
	** ordered most significant (lowest translated offset) first.
	*/
	offset = HASH_LOWEST_OFFSET - offset;

	while (offset > key->ending_offset)
	    key++;

	if (key->type == typeEOF)
	    goto keys_equal;
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
	args->n_hard++;

	win_ovc = 0;
	curr_ovc = 0;
	startkey = key;
	/* Calculate how much of the field is left. Some of it may have been
	** put into a previous ovc+eov by a prior refilling recode.
	*/
#if defined(POINTER_SORT)
	bytes_so_far = (offset - key->offset) << 1 /* * sizeof(valuecode_t) */ ;
#else
	bytes_so_far = (offset - key->offset) + ((offset - key->offset) << 1) /* * sizeof(valuecode_t) */ ;
#endif
	/* one or two bytes (if in a record sort's with three byte ovcs)
	** may have been put in the previous ovc
	** bump over that amount
	*/
	if (key->flags & FIELD_ODDSTART)
	{
	    ASSERT(offset > key->offset);
	    bytes_so_far -= key->flags & FIELD_ODDSTART;
	}

	do	/* loop until ovc+eov is filled, or we get to the 'eof' key */
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

		win_position = win_item->id.fpt->fpt[i];
		curr_position = curr_item->id.fpt->fpt[i];
		if (win_position == FPT_UNSEEN)
		    win_key = fill_fpt_entry(sort, win_item->id.fpt, i);
		else
		    win_key = win_item->id.fpt->record + win_position;
		if (curr_position == FPT_UNSEEN)
		    curr_key = fill_fpt_entry(sort, curr_item->id.fpt, i);
		else
		    curr_key = curr_item->id.fpt->record + curr_position;
	    }
	    else			/* no variable fields before this key */
	    {
		if (sort->anyfpt)
		{
		    win_key = (byte *) win_item->id.fpt->record + i;
		    curr_key = (byte *) curr_item->id.fpt->record + i;
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
	    if (key->flags & FIELD_DELIMSTRING)
	    {
		int	win_delim_byte;
		int	curr_delim_byte;

		/*
		 * A non-fixed-length field with 'ignore-me' trailing pad chars
		 * cannot share an ovc or eov with any other field. These types
		 * 'jump' in the strings to the first point of difference, and
		 * so they must have their own offset.
		 */
		if (ovcleft != OVCEOVSIZE)
		{
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
		    /* The -1 excludes the record delimiter
		     * from being included as part of the key
		     * (if the key happens to be the last field and
		     *  the key is not terminated by a field delimiter)
		     */
		    win_endrec += win_item->extra.var_extra.var_length - 1;
		    curr_endrec += curr_item->extra.var_extra.var_length - 1;
		}
		else
		{
		    if (sort->record.flags & RECORD_VARIABLE)
		    {
			win_endrec += win_item->extra.var_extra.var_length + 2;
			curr_endrec += curr_item->extra.var_extra.var_length +2;
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
		/*
		 * Find the interesting comparison point for these two strings:
		 * where they differ or at least one of them ends.
		 */
		for (offset = key->offset;
		     win_key < win_endrec && curr_key < curr_endrec;
		     win_key += 4, curr_key += 4, offset += 2)
		{
		    win_temp = ulw(win_key);
		    curr_temp = ulw(curr_key);
		    if (win_temp != curr_temp)
			break;
		    if (FIND_ZEROBYTE(win_temp ^ key->delimword) ||
		        FIND_ZEROBYTE(curr_temp ^ key->delimword))
			break;
		}

		/* If the value is beyond the end of the record then
		 * replace the irrelevent bytes with the field delimiter.
		 * This removes the `endrec' check from the later delim loop.
		 */
		i = win_key - win_endrec;
		if (i >= 0)
		    win_temp = key->delimword;
		else
		{
		    if (curr_key >= curr_endrec)	/* for aborted w/o ld?*/
			win_temp = ulw(win_key);
		     
		     if ((i += sizeof(u4)) > 0)
			win_temp = (win_temp & ~((1 << (i << 3)) - 1)) |
				   (key->delimword & ((1 << (i << 3)) - 1));
		}
		i = curr_key - curr_endrec;
		if (i >= 0)
		    curr_temp = key->delimword;
		else
		{
		     if (win_key >= win_endrec)		/* for aborted w/o ld?*/
			curr_temp = ulw(curr_key);
		     if ((i += sizeof(u4)) > 0)
			curr_temp = (curr_temp & ~((1 << (i << 3)) - 1)) |
				    (key->delimword & ((1 << (i << 3)) - 1));
		}

		/* We know that the current words (win_temp and curr_temp)
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
		if ((win_temp >> 24) == fdelim)
		{
		    win_temp = key->padword;
		    win_delim_byte = 24;
		}
		else if (((win_temp >> 16) & 0xff) == fdelim)
		{
		    win_temp = (win_temp & 0xff000000) | (key->padword >> 8);
		    win_delim_byte = 16;
		}
		else if (((win_temp >> 8) & 0xff) == fdelim)
		{
		    win_temp = (win_temp & 0xffff0000) | (key->padword >> 16);
		    win_delim_byte = 8;
		}
		else if ((win_temp & 0xff) == fdelim)
		{
		    win_temp = (win_temp & 0xffffff00) | (key->padword & 0xff);
		    win_delim_byte = 0;
		}

		if ((curr_temp >> 24) == fdelim)
		{
		    curr_temp = key->padword;
		    curr_delim_byte = 24;
		}
		else if (((curr_temp >> 16) & 0xff) == fdelim)
		{
		    curr_temp = (curr_temp & 0xff000000) | (key->padword >> 8);
		    curr_delim_byte = 16;
		}
		else if (((curr_temp >> 8) & 0xff) == fdelim)
		{
		    curr_temp = (curr_temp & 0xffff0000) | (key->padword >> 16);
		    curr_delim_byte = 8;
		}
		else if ((curr_temp & 0xff) == fdelim)
		{
		    curr_temp = (curr_temp & 0xffffff00) | (key->padword & 0xff);
		    curr_delim_byte = 0;
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
		else if (win_temp == curr_temp)
		{
		    /*
		     * The 'interesting' word is equal (after conditioning) and
		     * exactly one of the strings ends in this word.
		     * The other string extends into later bytes. Scan down
		     * the longer string until we find either its end or
		     * a non-pad character.  Build a ovc comparing the last
		     * word of the longer string to the pad.
		     */
		    win_temp = curr_temp = key->padword;
		    if (win_delim_byte > curr_delim_byte)  /* win ends 1st */
		    {
			for (i = curr_endrec - curr_key; i > 0; i -= sizeof(u4))
			{
			    curr_key += sizeof(u4);
			    offset += sizeof(u4) / sizeof(valuecode_t);
			    if (curr_key == curr_endrec)
				break;
			    
			    curr_temp = ulw(curr_key);
			    if (curr_temp == win_temp)	/* all pad chars? */
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
			    if ((curr_temp >> 24) == fdelim)
			    {
				curr_temp = win_temp;
			    }
			    else
			    {
				/*
				 * Have we reached the end of the key field,
				 * either because we've hit the end of
				 * the record without a field delimiter,
				 * or we're at a word containing a field delim?
				 */
				if (((curr_temp >> 16) & 0xff) == fdelim)
				    i = (1 << 24) - 1;
				else if (((curr_temp >> 8) & 0xff) == fdelim)
				    i = (1 << 16) - 1;
				else if ((curr_temp & 0xff) == fdelim)
				    i = (1 << 8) - 1;
				else
				    i = 0;
				/*
				 * Set all bytes past (to the right of)
				 * the last data byte to the pad.
				 */
				curr_temp = (curr_temp & ~i) | (win_temp & i);
			    }

			    break;	/* done searching the trailing pads */
			}
		    }
		    else	/* current item is shorter, scan win_item */
		    {
			for (i = win_endrec - win_key; i > 0; i -= sizeof(u4))
			{
			    win_key += sizeof(u4);
			    offset += sizeof(u4) / sizeof(valuecode_t);
			    if (win_key == win_endrec)
				break;
			    
			    win_temp = ulw(win_key);
			    if (win_temp == curr_temp)	/* all pad chars? */
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
			    if ((win_temp >> 24) == fdelim)
			    {
				win_temp = curr_temp;	/* all pad characters */
			    }
			    else
			    {
				/*
				 * Have we reached the end of the key field,
				 * either because we've hit the end of
				 * the record without a field delimiter,
				 * or we're at a word containing a field delim?
				 * Set 'i' to the mask value appropriate to
				 * condition the bytes beyond the last data byte
				 */
				if (((win_temp >> 16) & 0xff) == fdelim)
				    i = (1 << 24) - 1;
				else if (((win_temp >> 8) & 0xff) == fdelim)
				    i = (1 << 16) - 1;
				else if ((win_temp & 0xff) == fdelim)
				    i = (1 << 8) - 1;
				else
				    i = 0;
				/*
				 * Set all bytes past (to the right of)
				 * the last data byte to the pad.
				 */
				win_temp = (win_temp & ~i) | (curr_temp & i);
			    }

			    break;	/* done searching the trailing pads */
			}
		    }
		}

		i = 4;

		/* If the ovcs are equal here then 
		 * 1- win_key and curr_key will both
		 *    be at the end of their respective strings
		 * 2- the entire strings are equal
		 *   Give them the offset of the end of the string, so that
		 * the next recode will use the next key, and not this one.
		 */
		if (win_temp == curr_temp)
		{
#if 0
		    fprintf(stderr, "delimstring %x (%.*s) == %x(%.*s)\n",
			win_item,
			win_item->extra.var_extra.var_length + RECORD.var_hdr.extra - 1,
			win_item->rec,
			curr_item,
			curr_item->extra.var_extra.var_length + RECORD.var_hdr.extra - 1,
			curr_item->rec);
#endif

		    offset = key[1].offset;
		    if (offset & 1)
			i = 2;
		    offset -= i / 2;
		}
	    }
	    else	/* it is a fixed length type */
#endif /* of VARIABLE_SORT */
	    {
		i = key->length - bytes_so_far;
		if (i > ovcleft)
		    i = ovcleft;
		if (key->type == typeFixedDecimalString)
		{
		    /* Decimal strings share a complexity with padded strings:
		     * each time we recode them, we have to search through the
		     * data to find the end of the string or the decimal point,
		     * whichever comes first
		     */
		    win_temp = next_decimal_value(win_key,
						  key->length,
						  key->length,
						  bytes_so_far,
						  OVC_DEC_DIGITS
						 );
		    curr_temp = next_decimal_value(curr_key,
						   key->length,
						   key->length,
						   bytes_so_far,
						   OVC_DEC_DIGITS
						  );
		}
		else
		{
		    if (key->type == typeFloat)
		    {
			/*
			 * Floating point compares have -0.0 == +0.0.
			 * Set the sign bit for both kinds of zero.
			 * If a number is negative then the excess-128 or
			 * excess-1024 exponent needs to have its sign bit
			 * toggled. -2.3e+9 < -2.3e-2, but +2.3e+9 > +2.3e-2
			 * We also have to deal with the 'hidden' bit in the
			 * mantissa of all normalized numbers.
			 * What are the nan/inf/denorm inequality issues here?
			 */
			win_float = ulw(win_key); 
			curr_float = ulw(curr_key);
			COND_FLOAT(win_float);
			COND_FLOAT(curr_float);
			win_key = (byte *) &win_float + bytes_so_far;
			curr_key = (byte *) &curr_float + bytes_so_far;
		    }
		    else if (key->type == typeDouble)
		    {
			win_double[0] = ulw(win_key); 
			win_double[1] = ulw(win_key + 4); 
			curr_double[0] = ulw(curr_key); 
			curr_double[1] = ulw(curr_key + 4); 
			COND_DOUBLE(win_double[0], win_double[1]);
			COND_DOUBLE(curr_double[0], curr_double[1]);
			win_key = (byte *) &win_double[0] + bytes_so_far;
			curr_key = (byte *) &curr_double[0] + bytes_so_far;
		    }
		    else
		    {
			/* We may have already processed some of the
			 * preceding bytes in this key and found them
			 * to be equal. Skip over however
			 * many were done by a prior call to recode().
			 */
			win_key += bytes_so_far;
			curr_key += bytes_so_far;
		    }

		    win_temp = ulw(win_key);
		    curr_temp = ulw(curr_key);
		}

		/*	New, recode without {win,curr}_ovcp
		 * Win_temp and curr_temp contain the four bytes which start
		 * at the next byte of key data.  Floating point and decimal?)
		 * conditioning has been done already. Toggle sign for signed
		 * integer, and everything for :descending orderings.
		 * Then take the up to four bytes of data and add it to the
		 * local ovc values.  The useful data will always be at the high
		 * part of the word; irrelevent bytes will be in the low part.
		 */
		if (bytes_so_far == 0 && (key->flags & FIELD_TOGGLESIGN))
		{
		    win_temp ^= SIGN_BIT;
		    curr_temp ^= SIGN_BIT;
		}
	    }

	    /* Complement the values if this is a descending sort
	     */
	    win_temp ^= key->xormask;
	    curr_temp ^= key->xormask;

	    /*
	     * Take the first i bytes of the next portion of the key and
	     * add (OR'ing it) it into the accumulating ovc. The "&" 
	     * clears out the bytes of xxx_temp which are not currently
	     * relevent (those bytes below the interesting key data).
	     *	i	mask		>>'d mask
	     *	1	0xff000000	ff000000, ff0000, ff00, ff
	     *	2	0xffff0000	ffff0000, ffff00, ffff
	     *	3	0xffffff00	ffffff00, ffffff
	     *	4	0xffffffff	ffffffff
	     * The >> based on ovcleft moves those bytes from the high part
	     * of {win,curr}_temp to the correct part of the ovc.
	     */
	    win_ovc |= (win_temp & ~((1 << (32 - (i << 3))) - 1)) >>
		       ((OVCEOVSIZE - ovcleft) << 3);
	    curr_ovc |= (curr_temp & ~((1 << (32 - (i << 3))) - 1)) >>
			((OVCEOVSIZE - ovcleft) << 3);

	    ovcleft -= i;
	    if (ovcleft == 0)
		break;

	    /*
	    ** Advance to the next key.  If we have finished the last key
	    ** then the rest of ovc+eov still has zero bytes in it, so
	    ** the ovcs will compare as equal.
	    ** Reset the count of the number of bytes of this key
	    ** which were 'handled' by prior offset value codes.
	    */
	    key++;
	    bytes_so_far = 0;
	    
	} while (key->type != typeEOF);


#if defined(POINTER_SORT)
	win_eov = win_ovc & 0xffff;
	curr_eov = curr_ovc & 0xffff;
#endif
	win_ovc >>= (8 * sizeof(ovcoff_t));
	curr_ovc >>= (8 * sizeof(ovcoff_t));

	/* Bump up the offset to go past the value code already known to be eq
	 */
	offset++;

	/* Finally we have the ovc (and eov, if ptr sort) for both nodes.
	 * Figure out if we have a winner, and if so put the
	 * ovc+eov of the loser back into its node.
	 * The winner's ovc must not be changed.
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
	** Sometimes two or three adjacent one-byte keys have the same offset;
	** the later keys must not be 'seen' here, so it is
	** possible for key to be advanced twice: once to get to the
	** later small field(s), and again to get to the field
	** corresponding to the new offset.
	*/ 
	key = startkey;
	if (offset > key->ending_offset)
	{
	    key++;
	    if (key->type == typeEOF)
	    {
keys_equal:
		/* if in stable order already
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
	    if (offset > key->ending_offset)	/* two fields w/ same offset */
	    {
		key++;
		if (key->type == typeEOF)
		    goto keys_equal;
#if defined(RECORD_SORT)
		if (offset > key->ending_offset) /* 3 fields w/ same offset */
		{
		    key++;
		    if (key->type == typeEOF)
			goto keys_equal;
		}
#endif
	    }
	    ASSERT(key < &sort->keys[sort->keycount]);
	}
#if defined(RECORD_SORT)
	goto refill;		/* record sort has no eov, get next ovc */
#else
	/*
	** the remainder of this function is only for both kinds
	** of pointer sorts
	*/
	goto eov_part1;


/******************************************************************************/
      case OVC_EOV1:
	/*
	 * ovc matches, compare first part of eov.
	 * This case is reached in both variable and fixed length pointer sorts.
	 */
	win_eov = win_item->extra.var_extra.var_eov;
	curr_eov = curr_item->extra.var_extra.var_eov;

eov_part1:
	args->n_soft++;
	win_ovc = win_eov;
	curr_ovc = curr_eov;
	offset++;

	/* XXX The ASSIGN_EOV calls ARE needed here, in case the recode loops
	 * through a couple of ovcs before finding a difference.
	 */
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

	/* Record ordering couldn't be resolved.
	 * Try the next offset, which might be in a different key
	 */
	if (offset > key->ending_offset)
	{
	    key++;
	    if (key->type == typeEOF)
		goto keys_equal;
	    if (offset > key->ending_offset)	/* two fields w/ same offset */
	    {
		key++;
		if (key->type == typeEOF)
		    goto keys_equal;
	    }
	}

	/* for helping skip merges both fixed pointer and variable sorts
	 * have only one extra valuecode in the eov.
	 */
	goto refill;	
    }
    /* NOTREACHED */
#endif /* POINTER_SORT */
}
