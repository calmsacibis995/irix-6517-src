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
 *	$Ordinal-Id: compare.c,v 1.13 1996/07/22 22:04:23 charles Exp $
 */
#ident	"$Revision: 1.1 $"

#include "otcsort.h"
#include <math.h>
#include <ieeefp.h>

byte	*DebugKey = NULL;
byte	*DebugPtr = NULL;

int compare_string(keydesc_t *key, CONST byte *col1, CONST byte *col2, int len1, int len2)
{
    int	i;

    i = (len1 < len2) ? len1 : len2;
    while (--i >= 0)
    {
	if (*col1 < *col2)
	    return (-1);
	else if (*col1 != *col2)
	    return (+1);
	col1++;
	col2++;
    }
    if (len1 < len2)
    {
	for (i = len2 - len1; i > 0; i--, col2++)
	    if (*col2 != key->pad)
	    {
		if (key->pad < *col2)
		    return (-1);
		else
		    return (+1);
	    }
    }
    else if (len1 != len2)
    {
	for (i = len1 - len2; i > 0; i--, col1++)
	    if (*col1 != key->pad)
	    {
		if (*col1 < key->pad)
		    return (-1);
		else
		    return (+1);
	    }
    }
    return (0);
}

/*
 * get_delim_length	- return the number of significant bytes in the field
 *
 *	A field stops at:
 *		the first occurence of the field delimiter, or
 *		the end of the record.
 *	Any trailing pad characters are not "significant", and are excluded
 *	from the returned length.
 */
int get_delim_length(const byte *field, const keydesc_t *key, int till_eorec)
{
    const byte *end;

    end = (const byte *) memchr(field, key->delimiter, till_eorec);
    if (end == NULL)
	end = field + till_eorec;

    /* reduce the field length by the number of trailing pad chars (if any)
     */
    while (end > field && end[-1] == key->pad)
	end--;
    return (end - field);
}

int compare_delim_string(keydesc_t *key, CONST byte *rec1, CONST byte *rec2, int len1, int len2)
{
    CONST byte	*col1 = rec1 + key->position;
    CONST byte	*col2 = rec2 + key->position;
    CONST byte	*eorec1 = rec1 + len1;
    CONST byte	*eorec2 = rec2 + len2;
    int		i, j;

    for (i = 0; col1 < eorec1 && *col1 != key->delimiter; i++)
	col1++;
    for (j = 0; col2 < eorec2 && *col2 != key->delimiter; j++)
	col2++;
    return (compare_string(key, col1 - i, col2 - j, i, j));
}

/*
 * compare_decimal	- return -1/0/1 according to col1 being </==/> col2
 */
int compare_decimal(byte *val1, byte *val2, int len1, int len2)
{
    int		i;
    int		before;
    int		sign1 = +1;
    int		sign2 = +1;
    int		samesign;
    int		negative;
    int		allzero = TRUE;

    while (len1 != 0 && isspace(*val1))
	val1++, len1--;
    while (len2 != 0 && isspace(*val2))
	val2++, len2--;

    /* Process the signs of the strings, if any
     * This is somewhat complicated by +0 == -0, we have to remember the
     * signs of the strings until we see a non-zero digit.
     */
    if (len1 != 0 && *val1 == '+')
	val1++, len1--;
    else if (len1 != 0 && *val1 == '-')
	val1++, len1--, sign1 = -1;
    if (len2 != 0 && *val2 == '+')
	val2++, len2--;
    else if (len2 != 0 && *val2 == '-')
    {
	val2++, len2--;
	sign2 = -1;
    }

    /* skip over leading (and hence insignificant) zeroes
     */
    while (len1 != 0 && *val1 == '0')
	val1++, len1--;
    while (len2 != 0 && *val2 == '0')
	val2++, len2--;

    samesign = sign1 == sign2;
    negative = sign1 < 0;

    /* Find the first non-digit in each numeric string.
     * It is either the end of the digit string, or a decimal point.
     */
    for (i = 0; i != len1; i++)
	if (!isdigit(val1[i]))
	    break;
    before = i;
    for (i = 0; i != len2; i++)
	if (!isdigit(val2[i]))
	    break;

    /* If the numbers have a different number of significant digits before the
     * decimal point position, then the shorter one has a smaller magnitude.
     * To get here at least one number has a non-zero digit, so we
     * don't need to handle the +0 == -0 case.
     */
    if (before != i)
    {
	/* If the (now-known-non-zero) magnitudes have differing signs,
	 * then retval is 1 if val1 is positive (and hence val2 is negative),
	 * -1 if val1 is negative (and val2 is positive).
	 * If they have the same signs, then the retval is -1 if
	 *	the first one is shorter and the signs are positive, or
	 *	the first one is longer and the signs are negative.
	 * and +1 otherwise (first shorter && signs -, first long && signs +)
	 * if (!samesign) return (sign1)
	 * if (before1 < before2 && !negative ||
	 *     before1 > before2 && negative) return -1
	 * else return 1
	 * This collapses into the following
	 * 	(before1 > before2) == negative ? -1 : 1
	 */
	if (samesign)
	    return ((before > i) == negative ? -1 : 1);
	else
	    return (sign1);
    }
    
    if (before != 0)
    {
	/* There are some non-zero digits before the decimal point position.
	 * If the signs are different return -1 if first is neg, +1 if pos
	 */
	if (!samesign)
	    return (sign1);

	/* Signs are the same, compare this digit string till dec. pt. position
	 */
	for (i = 0; i != before; i++)
	{
	    if (val1[i] != val2[i])
		return ((val1[i] > val2[i]) == negative ? -1 : 1);
	}

	allzero = FALSE;
	val1 += before, len1 -= before;
	val2 += before, len2 -= before;
    }
    if (len1 != 0 && *val1 == '.')
	val1++, len1--;
    if (len2 != 0 && *val2 == '.')
	val2++, len2--;

    /* Now the parts of the numbers before the decimal point position are equal.
     * Compare any part after the decimal point
     */
    for (i = min(len1, len2); i != 0; i--, val1++, val2++)
    {
	if (!isdigit(*val1))	/* val1 ends with non-digit, perhaps first */
	{
	    if (isdigit(*val2) && *val2 != '0')
		allzero = FALSE;
	    goto val1_shorter;
	}
	if (*val1 != '0')
	    allzero = FALSE;
	if (!isdigit(*val2))	/* val2 ends first, with non-digit */
	    goto val2_shorter;
	if (*val1 != *val2)
	{
	    if (samesign)
		return ((*val1 > *val2) == negative ? -1 : 1);
	    else
		return (sign1);
	}
	if (*val2 != '0')
	    allzero = FALSE;
    }

    /* Here the number are equal up to the length of the shorter digit string.
     * See whether the longer one is just padded with insignificant zeroes,
     * or the longer digit string is really has a larger magnitude.
     */
    if (len1 < len2)
    {
val1_shorter:
	while (len2 != 0 && *val2 == '0')
	    val2++, len2--;
	/* either val2 has all trailing zero digits
	 * or it has a trailing non-zero digit
	 */
	if (len2 == 0 || !isdigit(*val2))
	{
	    if (samesign || allzero)
		return (0);
	    else
		return (sign1);
	}
	/* Now val2 is known to have a larger magnitude, it has a non-zero
	 * differing-from-val1 digit somewhere to the right of the dec. pt.
	 */
	if (samesign)				/* cmp(-2, -2.3) -> +1 */
	    return (negative ? +1 : -1);	/* cmp(2, 2.3)   -> -1 */
	else					/* cmp(2, -2.3)  -> +1 */
	    return (sign1);			/* cmp(-2, 2.3)  -> -1 */
    }
    else if (len1 > len2)
    {
val2_shorter:
	while (len1 != 0 && *val1 == '0')
	    val1++, len1--;
	/* either val1 has all trailing zero digits
	 * or it has a trailing non-zero digit
	 */
	if (len1 == 0 || !isdigit(*val1))
	{
	    if (samesign || allzero)
		return (0);
	    else
		return (sign1);
	}
	/* Now val1 is known to have a larger magnitude, it has a non-zero
	 * differing-from-val1 digit somewhere to the right of the dec. pt.
	 */
	if (samesign)
	    return (negative ? -1 : +1);
	else
	    return (sign1);
    }

    /* force copying of args from regs to stack so debuggers can print vars */
    ASSERT(len1 == len2);

    return (0);
}

/*
** compare_data	- compare the values of two record's fields, noticing asc/desc
**
**	Paramters:
**		sort	- sort struct containing the record info and  keys
**		rec1	-
**		rec2	-
**	Returns:
**		< 0 if rec1 wins (its keys are typically lower than rec2's)
**		> 0 if rec1 loses (its keys are typically higher than rec2's)
**		= 0 if rec1's keys are the same as rec2's
*/
int compare_data(sort_t *sort, CONST byte *rec1, CONST byte *rec2)
{
    keydesc_t	*key;
    int		len1, len2;
    CONST byte	*col1, *col2;
    int		i;
    int		ordered;
    float	tempfloat1, tempfloat2;
    double	tempdouble1, tempdouble2;

    switch (RECORD.flags & RECORD_TYPE)
    {
      case RECORD_FIXED:
    	len1 = len2 = RECORD.length;
        break;

      case RECORD_VARIABLE:
    	len1 = ulh(rec1) + RECORD.var_hdr_extra;
    	len2 = ulh(rec2) + RECORD.var_hdr_extra;
	break;

      case RECORD_STREAM:
	len1 = (int) ((byte *) memchr(rec1 + RECORD.minlength - 1,
				      RECORD.delimiter,
				      MAXRECORDSIZE - 1)
		      - rec1);
	len2 = (int) ((byte *) memchr(rec2 + RECORD.minlength - 1,
				      RECORD.delimiter,
				      MAXRECORDSIZE - 1)
		      - rec2);
	break;

      default:
	die("compare_data:unsupported record type %x", RECORD.flags);
    }

    if (sort->compute_hash)
    {
	unsigned	hash1, hash2;

	hash1 = calc_hash(sort, rec1, len1);
	hash2 = calc_hash(sort, rec2, len2);
	/* Is this a record sort? if so use only 31 bits
	 */
	if (sort->method == SortMethRecord)
	{
	    hash1 >>= 1;
	    hash2 >>= 1;
	}
	if (hash1 < hash2)
	    return (-1);
	else if (hash1 > hash2)
	    return (+1);
    }

    for (key = sort->keys; key->type != typeEOF; key++)
    {
	col1 = rec1 + key->position;
	col2 = rec2 + key->position;
	if (key->flags & FIELD_DESCENDING)
	    ordered = +1;
	else
	    ordered = -1;

	switch (key->type)
	{
	  case typeI1:
	    if (*(i1 *) col1 < *(i1 *) col2)
		return (ordered);
	    if (*(i1 *) col1 > *(i1 *) col2)
		return (-ordered);
	    break;

	  case typeI2:
	    if ((i2) ulh(col1) < (i2) ulh(col2))
		return (ordered);
	    if ((i2) ulh(col1) > (i2) ulh(col2))
		return (-ordered);
	    break;

	  case typeI4:
	    if ((i4) ulw(col1) < (i4) ulw(col2))
		return (ordered);
	    if ((i4) ulw(col1) > (i4) ulw(col2))
		return (-ordered);
	    break;

	  case typeI8:
	    if ((i4) ulw(col1) < (i4) ulw(col2))
		return (ordered);
	    if ((i4) ulw(col1) > (i4) ulw(col2))
		return (-ordered);
	    col1 += 4;
	    col2 += 4;
	    if (ulw(col1) < ulw(col2))
		return (ordered);
	    if (ulw(col1) > ulw(col2))
		return (-ordered);
	    break;

	  case typeU1:
	  case typeU2:
	  case typeU4:
	  case typeU8:
	  case typeFixedString:
	    i = memcmp(col1, col2, key->length);
	    if (i < 0)
		return (ordered);
	    if (i > 0)
		return (-ordered);
	    break;

	  case typeFloat:
	    memmove(&tempfloat1, col1, sizeof(tempfloat1));
	    memmove(&tempfloat2, col2, sizeof(tempfloat2));
	    if (unordered(tempfloat1, tempfloat2))	/* NaN? say it's ordered */
		return (-1);
	    if (tempfloat1 < tempfloat2)
		return (ordered);
	    if (tempfloat1 > tempfloat2)
		return (-ordered);
	    break;

	  case typeDouble:
	    memmove(&tempdouble1, col1, sizeof(tempdouble1));
	    memmove(&tempdouble2, col2, sizeof(tempdouble2));
	    if (unordered(tempdouble1, tempdouble2))	/* NaN? say it's ordered */
		return (-1);
	    if (tempdouble1 < tempdouble2)
		return (ordered);
	    if (tempdouble1 > tempdouble2)
		return (-ordered);
	    break;

	  case typeFixedDecimalString:
	    i = compare_decimal(col1, col2, key->length, key->length);
	    if (i != 0)
		return (-i * ordered);
	    break;

	  case typeDelimString:
	    /* get string lengths first. this allows common string comparison
	    ** and trailing blank handling code. 
	    ** We can't use strchr() to get length, it behaves differently 
	    ** in the case of a '\0' as the delimiter.
	    */
	    i = compare_delim_string(key, rec1, rec2, len1, len2);
	    if (i != 0)
		return (-i * ordered);
	    break;
		
	  case typeLengthString:    /* whoops! this type doesn't exist yet */
	    len1 = *col1++;
	    len2 = *col2++;
	    /* compare_string(....); */
	  default:
	    die("compare_data:unknown type %d", key->type);
	    break;
	}
    }

    return (0);
}

#if 0
/*
** compare_keydata	- compare the values of two record's key, noticing asc/desc
**
**	Paramters:
**		sort	- sort struct containing the record info and  keys
**		rec1	-
**		rec2	-
**	Returns:
**		< 0 if rec1 wins (its keys are typically lower than rec2's)
**		> 0 if rec1 loses (its keys are typically higher than rec2's)
**		= 0 if rec1's keys are the same as rec2's
*/
int compare_keydata(sort_t *sort, CONST byte *rec1, CONST byte *rec2)
{
    keydesc_t	*key;
    char	*p;
    int		len1, len2;
    CONST byte	*col1, *col2;
    int		i;
    int		ordered;
    float	tempfloat1, tempfloat2;
    double	tempdouble1, tempdouble2;

    switch (RECORD.flags & RECORD_TYPE)
    {
      case RECORD_FIXED:
    	len1 = len2 = RECORD.length;
        break;

      case RECORD_VARIABLE:
    	len1 = ulh(rec1) + RECORD.var_hdr_extra;
    	len2 = ulh(rec2) + RECORD.var_hdr_extra;
	break;

      case RECORD_STREAM:
	len1 = (byte *) memchr(rec1+RECORD.minlength-1, RECORD.delimiter, 65535)
		- rec1;
	len2 = (byte *) memchr(rec2+RECORD.minlength-1, RECORD.delimiter, 65535)
		- rec2;
	break;

      default:
	die("compare_data:unsupported record type %x", RECORD.flags);
    }

    for (key = sort->keys; key->type != typeEOF; key++)
    {
	col1 = rec1 + key->position;
	col2 = rec2 + key->position;
	if (key->flags & FIELD_DESCENDING)
	    ordered = +1;
	else
	    ordered = -1;

	switch (key->type)
	{
	  case typeI1:
	    if (*(i1 *) col1 < *(i1 *) col2)
		return (ordered);
	    if (*(i1 *) col1 > *(i1 *) col2)
		return (-ordered);
	    break;

	  case typeI2:
	    if ((i2) ulh(col1) < (i2) ulh(col2))
		return (ordered);
	    if ((i2) ulh(col1) > (i2) ulh(col2))
		return (-ordered);
	    break;

	  case typeI4:
	    if ((i4) ulw(col1) < (i4) ulw(col2))
		return (ordered);
	    if ((i4) ulw(col1) > (i4) ulw(col2))
		return (-ordered);
	    break;

	  case typeI8:
	    if ((i4) ulw(col1) < (i4) ulw(col2))
		return (ordered);
	    if ((i4) ulw(col1) > (i4) ulw(col2))
		return (-ordered);
	    col1 += 4;
	    col2 += 4;
	    if (ulw(col1) < ulw(col2))
		return (ordered);
	    if (ulw(col1) > ulw(col2))
		return (-ordered);
	    break;

	  case typeU1:
	  case typeU2:
	  case typeU4:
	  case typeU8:
	  case typeFixedString:
	    i = memcmp(col1, col2, key->length);
	    if (i < 0)
		return (ordered);
	    if (i > 0)
		return (-ordered);
	    break;

	  case typeFloat:
	    memmove(&tempfloat1, col1, sizeof(tempfloat1));
	    memmove(&tempfloat2, col2, sizeof(tempfloat2));
	    if (unordered(tempfloat1, tempfloat2))	/* NaN? say it's ordered */
		return (-1);
	    if (tempfloat1 < tempfloat2)
		return (ordered);
	    if (tempfloat1 > tempfloat2)
		return (-ordered);
	    break;

	  case typeDouble:
	    memmove(&tempdouble1, col1, sizeof(tempdouble1));
	    memmove(&tempdouble2, col2, sizeof(tempdouble2));
	    if (unordered(tempdouble1, tempdouble2))	/* NaN? say it's ordered */
		return (-1);
	    if (tempdouble1 < tempdouble2)
		return (ordered);
	    if (tempdouble1 > tempdouble2)
		return (-ordered);
	    break;

	  case typeDelimString:
	    /* get string lengths first. this allows common string comparison
	    ** and trailing blank handling code. 
	    ** We can't use strchr() to get length, it behaves differently 
	    ** in the case of a '\0' as the delimiter.
	    */
	    i = compare_delim_string(key, rec1, rec2, len1, len2);
	    if (i == 0)
		break;
	    return (-i * ordered);
#if 0
	    for (len1 = 0; col1[len1] != key->delimiter; len1++)
		continue;
	    for (len2 = 0; col2[len2] != key->delimiter; len2++)
		continue;
	    i = (len1 < len2) ? len1 : len2;
	    while (--i >= 0)
	    {
		if (*col1 < *col2)
		    return (ordered);
		else if (*col1 != *col2)
		    return (-ordered);
		col1++;
		col2++;
	    }
	    if (len1 < len2)
	    {
		for (i = len2 - len1; i > 0; i--, col2++)
		    if (*col2 != key->pad)
		    {
			if (key->pad < *col2)
			    return (ordered);
			else
			    return (-ordered);
		    }
	    }
	    else if (len1 != len2)
	    {
		for (i = len1 - len2; i > 0; i--, col1++)
		    if (*col1 != key->pad)
		    {
			if (*col1 < key->pad)
			    return (ordered);
			else
			    return (-ordered);
		    }
	    }
	    break;
#endif
		
	  case typeLengthString:    /* whoops! this type doesn't exist yet */
	    len1 = *col1++;
	    len2 = *col2++;
	    /* compare_string(....); */
	  default:
	    die("compare_keydata:unknown type %d", key->type);
	    break;
	}
    }

    return (0);
}
#endif
