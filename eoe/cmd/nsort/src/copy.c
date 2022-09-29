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
 ****************************************************************************
 *
 *	$Ordinal-Id: copy.c,v 1.4 1996/07/22 22:15:20 charles Exp $
 */

#ident	"$Revision: 1.1 $"

#include "ordinal.h"

/*
 * Each of these copy function must be safe for overlapping moves
 * in which the destinatation address is insie [src, src+size]
 * Most moves in fact do not overlap, but build_tbuf_items_rs()
 * will occasionally have the record and the item in overlapping
 * regions of memory when it has nearly finished. building items.
 */
void	copy_4(void *dest, const void *src, size_t size)
{
	((u4 *) dest)[0] = ((u4 *) src)[0];
}

void	copy_8(void *dest, const void *src, size_t size)
{
	u4	t0;
	u4	t1;

	t0 = ((u4 *) src)[0];
	t1 = ((u4 *) src)[1];
	((u4 *) dest)[0] = t0;
	((u4 *) dest)[1] = t1;
}
void	copy_12(void *dest, const void *src, size_t size)
{
	u4	t0;
	u4	t1;
	u4	t2;

	t0 = ((u4 *) src)[0];
	t1 = ((u4 *) src)[1];
	t2 = ((u4 *) src)[2];
	((u4 *) dest)[0] = t0;
	((u4 *) dest)[1] = t1;
	((u4 *) dest)[2] = t2;
}
void	copy_16(void *dest, const void *src, size_t size)
{
	u4	t0;
	u4	t1;
	u4	t2;
	u4	t3;

	t0 = ((u4 *) src)[0];
	t1 = ((u4 *) src)[1];
	t2 = ((u4 *) src)[2];
	t3 = ((u4 *) src)[3];
	((u4 *) dest)[0] = t0;
	((u4 *) dest)[1] = t1;
	((u4 *) dest)[2] = t2;
	((u4 *) dest)[3] = t3;
}

void	copy_20(void *dest, const void *src, size_t size)
{
	u4	t0;
	u4	t1;
	u4	t2;
	u4	t3;
	u4	t4;

	t0 = ((u4 *) src)[0];
	t1 = ((u4 *) src)[1];
	t2 = ((u4 *) src)[2];
	t3 = ((u4 *) src)[3];
	t4 = ((u4 *) src)[4];
	((u4 *) dest)[0] = t0;
	((u4 *) dest)[1] = t1;
	((u4 *) dest)[2] = t2;
	((u4 *) dest)[3] = t3;
	((u4 *) dest)[4] = t4;
}

void	copy_24(void *dest, const void *src, size_t size)
{
	u4	t0;
	u4	t1;
	u4	t2;
	u4	t3;
	u4	t4;
	u4	t5;

	t0 = ((u4 *) src)[0];
	t1 = ((u4 *) src)[1];
	t2 = ((u4 *) src)[2];
	t3 = ((u4 *) src)[3];
	t4 = ((u4 *) src)[4];
	t5 = ((u4 *) src)[5];
	((u4 *) dest)[0] = t0;
	((u4 *) dest)[1] = t1;
	((u4 *) dest)[2] = t2;
	((u4 *) dest)[3] = t3;
	((u4 *) dest)[4] = t4;
	((u4 *) dest)[5] = t5;
}

void	copy_28(void *dest, const void *src, size_t size)
{
	u4	t0;
	u4	t1;
	u4	t2;
	u4	t3;
	u4	t4;
	u4	t5;
	u4	t6;

	t0 = ((u4 *) src)[0];
	t1 = ((u4 *) src)[1];
	t2 = ((u4 *) src)[2];
	t3 = ((u4 *) src)[3];
	t4 = ((u4 *) src)[4];
	t5 = ((u4 *) src)[5];
	t6 = ((u4 *) src)[6];
	((u4 *) dest)[0] = t0;
	((u4 *) dest)[1] = t1;
	((u4 *) dest)[2] = t2;
	((u4 *) dest)[3] = t3;
	((u4 *) dest)[4] = t4;
	((u4 *) dest)[5] = t5;
	((u4 *) dest)[6] = t6;
}

void	copy_32(void *dest, const void *src, size_t size)
{
	u4	t0;
	u4	t1;
	u4	t2;
	u4	t3;
	u4	t4;
	u4	t5;
	u4	t6;
	u4	t7;

	t0 = ((u4 *) src)[0];
	t1 = ((u4 *) src)[1];
	t2 = ((u4 *) src)[2];
	t3 = ((u4 *) src)[3];
	t4 = ((u4 *) src)[4];
	t5 = ((u4 *) src)[5];
	t6 = ((u4 *) src)[6];
	t7 = ((u4 *) src)[7];
	((u4 *) dest)[0] = t0;
	((u4 *) dest)[1] = t1;
	((u4 *) dest)[2] = t2;
	((u4 *) dest)[3] = t3;
	((u4 *) dest)[4] = t4;
	((u4 *) dest)[5] = t5;
	((u4 *) dest)[6] = t6;
	((u4 *) dest)[7] = t7;
}

void	copy_36(void *dest, const void *src, size_t size)
{
	u4	t0;
	u4	t1;
	u4	t2;
	u4	t3;
	u4	t4;
	u4	t5;
	u4	t6;
	u4	t7;
	u4	t8;

	t0 = ((u4 *) src)[0];
	t1 = ((u4 *) src)[1];
	t2 = ((u4 *) src)[2];
	t3 = ((u4 *) src)[3];
	t4 = ((u4 *) src)[4];
	t5 = ((u4 *) src)[5];
	t6 = ((u4 *) src)[6];
	t7 = ((u4 *) src)[7];
	t8 = ((u4 *) src)[8];
	((u4 *) dest)[0] = t0;
	((u4 *) dest)[1] = t1;
	((u4 *) dest)[2] = t2;
	((u4 *) dest)[3] = t3;
	((u4 *) dest)[4] = t4;
	((u4 *) dest)[5] = t5;
	((u4 *) dest)[6] = t6;
	((u4 *) dest)[7] = t7;
	((u4 *) dest)[8] = t8;
}

/*
 * fmemchr	- ~30% faster than libc's memchr on an Indy with IRIX 5.3
 *
 *	Time, in cycyles, from pixie:
 *		simple		 9,097k
 *		complex		11,310k
 *		libc		14,187k
 *
 *	Elapsed, in fm.c: (100,000 * each of 80 bytes, aligned)
 *				aligned		aligned+1
 *		libc		13.11		12.77	(faster than aligned?!)
 *		byteonly	11.27		11.03	(faster than aligned?!)
 *		wordsimple
 *		    one word	 9.77
 *		    two words	 9.84		11.05
 *		wordcomplex
 *		    one word	
 *		    two words	 9.45		10.41
 */
void *fmemchr(const void *s, int c, size_t n)
{
    unsigned	i;
    u4		word;
    u4		word2;
    unsigned	delim;
    
    int		lim;

    /* Check any bytes before the first word-aligned part of the string 
     */
    if ((i = ((ptrdiff_t) s & 3)) != 0)
    {
	lim = min(4 - i, n);
	for (i = 0; i != lim; i++)
	{
	    if (((i1 *) s)[i] == c)
		return ((void *) ((ptrdiff_t) s + i));
	}
	s = (const void *) ((ptrdiff_t) s + lim);
	n -= lim;
	if (n == 0)
	    return (NULL);
    }

    /* Make a 4-byte word duplicating the sought-after character in each byte
     */
    delim = c | (c << 8);
    delim |= delim << 16;

    /* Find the first word containing the delimiter 'c', or the end of 's'
     */
    word = *((u4 *) s);
    for (i = n/8; i != 0; i--)
    {
	word2 = *(((u4 *) s) + 1);
	if (FIND_ZEROBYTE(word ^ delim))
	    break;
	if (FIND_ZEROBYTE(word2 ^ delim))
	{
	    s = (const void *) ((ptrdiff_t) s + sizeof(u4));
	    word = word2;
	    break;
	}
	word = *(((u4 *) s) + 2);
	s = (const void *) ((ptrdiff_t) s + 2 * sizeof(u4));
    }
    if (i != 0)		/* found a word with at least 1 delim; find which */
    {
	if ((word >> 24) == c)
	    return ((void *) ((ptrdiff_t) s + 0));
	if (((word >> 16) & 0xff) == c)
	    return ((void *) ((ptrdiff_t) s + 1));
	if (((word >>  8) & 0xff) == c)
	    return ((void *) ((ptrdiff_t) s + 2));
	else
	    return ((void *) ((ptrdiff_t) s + 3));
    }
    else 		/* didn't find it, checking trailing bytes if any */
    {
	for (i = 0; i != (n & 7); i++)
	    if (((i1 *) s)[i] == c)
		return ((void *) ((ptrdiff_t) s + i));
	return (NULL);
    }
}
