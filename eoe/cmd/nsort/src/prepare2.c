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
#include "merge.h"
#include "radix.h"

/*
 * prepare_keys - fiddle with the keys to reduce recode's work
 *
 *	Parameters:
 *
 *	Tasks:
 *		append a terminating typeEOF marker key
 *		Compute the starting ovc.offset for each key
 *
 */
void prepare_keys(sort_t *sort)
{
    keydesc_t	*key;
    keydesc_t	*key2;
    keydesc_t	savekey;
    unsigned	keywidth;
    int		varcount;
    int		i;
    int		length;
    int		delimsize = (RECORD.flags & RECORD_STREAM) ? 1 : 0;
    unsigned	varwidth;
    unsigned	offset;
    unsigned	max_number;

    /*
    ** Find the largest key precedence number that which was explicitly
    ** specified via "-key=...,number=N".  Then assign all other keys
    ** a precedence starting after the maximum user-specified key number
    */
    for (key = sort->keys, max_number = 0; key->type != typeEOF; key++)
	if (max_number < key->number)
	    max_number = key->number;

    for (key = sort->keys; key->type != typeEOF; key++)
	if (key->number == 0)
	    key->number = ++max_number;
    
    /* Now that the keys all have a ordering priority 'number', sort them by it
     */
    for (key = sort->keys + 1, i = 1; i < sort->keycount ; key++, i++)
    {
	for (key2 = sort->keys; key2 != key; key2++)
	{
	    if (key2->number > key->number)
	    {
		savekey = *key;
		memmove(key2 + 1, key2, (byte *) key - (byte *) key2);
		*key2 = savekey;
		break;
	    }
	}
    }

    for (key = sort->keys, keywidth = 0, varcount = 0; key->type != typeEOF; key++)
    {
	if ((RECORD.flags & RECORD_VARIABLE) &&
	    !(key->flags & FIELD_VARPOSITION))
	{
	    key->position += sizeof(u2);
	    key->flags |= FIELD_VARPOSITION;
	}
	if (IsVariableType(key->type))
	{
	    if (IsDelimType(key->type))
		keywidth++;	/* space for 'pad' to put delim in high ovc byte */
	    varcount++;
	}
	else
	{
	    keywidth += key->length;
	    if (RECORD.minlength < key->position + key->length + delimsize)
	    {
		if (RECORD.flags & RECORD_FIXED)
		    runtime_error(sort, NSORT_KEY_BEYOND_END, key->name);
		RECORD.minlength = key->position + key->length + delimsize;
		if (RECORD.length < RECORD.minlength)
		    RECORD.length = RECORD.minlength;
	    }
	}
    }

    /*
    ** If there are any variable length fields whose maximum length is
    ** unspecified, then figure out how much of the 'ovc offset space'
    ** they will use.  Each of them get an equal share of the offset space
    ** left over after subtracting the offset space used by the other keys.
    */
    if (varcount > 0)
    {
	/* A radix sort has a hash value computed on demand which is prepended
	 * to the user specific key list. This hash value consumes some of the
	 * ovc offset space which would otherwise be available for keys.
	 * XXX commented out ptr-sort check - as varlens may not be rec-sorted
	 */
	if (sort->compute_hash /* && sort->method == SortMethPointer */)
	    keywidth -= sizeof(u4);	/* sizeof the 32 bit hash value */

	varwidth = (int) (((1 << ((sizeof(ovc_t) - sort->ovcsize) << 3)) - 1) -
			  ((long) ROUND_UP_NUM(keywidth, sort->ovcsize)) /
			   sort->ovcsize) / varcount;
	if (varwidth & 1)
	    varwidth--;
    }

    /* A radix sort has a hash value computed on demand which is prepended to
     * the user specific key list. This hash value consumes some of the
     * ovc offset space which would otherwise be available for keys.
     *	2 16 bit ovcs for pointer sorts, allowing a 32 bit hash value
     *	128 24 bit ovcs for record sorts, allowing a 31 bit hash value
     */
    offset = 0;

    for (key = sort->keys; key->type != typeEOF; key++)
    {
	if (IsVariableType(key->type) && key->length == 0)
	    length = varwidth;
	else
	    length = key->length;

	key->padword = (key->pad << 24) | (key->pad << 16) |
		       (key->pad << 8)  | key->pad;
	key->delimword = (key->delimiter << 24) | (key->delimiter << 16) |
		         (key->delimiter << 8)  | key->delimiter;
	if (key->flags & FIELD_DESCENDING)
	    key->xormask = ~0;
	else
	    key->xormask = 0;

	switch (key->type)
	{
	  case typeI1:
	  case typeI2:
	  case typeI4:
	  case typeI8:
	    key->flags |= FIELD_TOGGLESIGN;
	    break;

	  case typeDelimString:
	  case typeDelimDecimalString:
	    /* The '4' below is the size of the value code + extra offset value
	     * for a pointer sort (which is a result of delim strings, since
	     * they are allowed only in non-fixed records, and hence ptr sorts)
	     */
	    if (offset % 4)		/* no ODDSTART var len strs! */
	    {
		offset += 4 - (offset % 4);
		key[-1].ending_offset = (offset - 1) / sort->ovcsize;
	    }
	    key->flags |= FIELD_DELIMSTRING;
	    break;

#if 0	/* a length-follows string type - not implemented yet */
	  case typeLengthString:
	    if (offset % sort->ovcsize)		/* no ODDSTART var len strs! */
		offset += sort->ovcsize - (offset % sort->ovcsize);
	    key->flags |= FIELD_VARIABLE;
	    break;
#endif
	}

	if (offset % sort->ovcsize)
	{
	    key->flags |= (offset % sort->ovcsize);
	    /*
	    ** Warning- subtleties of the proper offset for particular
	    ** alignments of field starts and ends are common.
	    **
	    ** This key starts in the middle of a value code.
	    ** Tell recode that this will happen/will have happened.
	    */

	    /*
	    ** If the field is entirely contained inside a single offset
	    ** value code, and (since we're here in this offset % ovcsize block)
	    ** the first starts after the beginning of the ovc, then
	    ** this field gets the same ovc as the previous key.
	    ** Otherwise the field starts in this middle of this ovc
	    ** and continues into the next ovc -- give it an offset
	    ** corresponding to the next ovc by rounding it up.
	    if ((offset + length) / sort->ovcsize == offset / sort->ovcsize &&??)
		key->offset = offset / sort->ovcsize;
	    else
		key->offset = ROUND_UP_DIV(offset, sort->ovcsize);
	    */
	}

	/* ovc containing the key's first byte */
	key->offset = offset / sort->ovcsize; /*ROUND_UP_DIV(offset, sort->ovcsize);*/

	offset += length;
	key->length = length;
	/* ovc containing its last byte */
	key->ending_offset = (offset - 1) / sort->ovcsize;
    }

    /*
    ** Give the typeEOF an offset just beyond that of the last real key
    ** Round up in case the final key ends in the middle of an ovc -
    ** the remaining byte(s) will be filled with zeroes.
    */
    key->offset = key->ending_offset = ROUND_UP_DIV(offset, sort->ovcsize);
}

