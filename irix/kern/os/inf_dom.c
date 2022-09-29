/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.1 $"

#ifndef _KERNEL
#include <libc_synonyms.h>
#endif
#include <sys/types.h>
#include <sys/inf_label.h>
#ifdef	_KERNEL
#include <sys/debug.h>
#include <sys/systm.h>
#endif

static int subset(unsigned short *, int, unsigned short *, int);

/*
 * inf_invalid
 *	Returns 1 if the label is bogus.
 *	Returns 0 iff it's OKay.
 */
int
inf_invalid(inf_label *lp)
{
	int i;

	if (!lp)
		return (1);
	if (lp->il_catcount > INF_MAX_SETS)
		return (1);

	for (i = 1; i < lp->il_catcount; i++)
		if (lp->il_list[i-1] >= lp->il_list[i])
			return (1);

	return (0);
}

/*
 * inf_equ
 * 	Returns 1 iff the labels are equal
 *	Returns 0 otherwise, including errors.
 */
int
inf_equ(inf_label *lp1, inf_label *lp2)
{
#ifndef	_KERNEL
	if (inf_invalid(lp1) || inf_invalid(lp2))
		return (0);
#else
	ASSERT( !inf_invalid(lp1) );
	ASSERT( !inf_invalid(lp2) );
#endif
	if (lp1 == lp2)
		return (1);

	if (lp1->il_level != lp2->il_level)
		return (0);
	if (lp1->il_catcount != lp2->il_catcount)
		return (0);

	return (subset(lp1->il_list, lp1->il_catcount,
	    lp2->il_list, lp2->il_catcount));
}

/*
 * inf_dom
 * 	Returns 1 iff lp1 dominates lp2
 *	Returns 0 otherwise, including errors.
 */
int
inf_dom(inf_label *lp1, inf_label *lp2)
{
#ifndef	_KERNEL
	if (inf_invalid(lp1) || inf_invalid(lp2))
		return (0);
#else
	ASSERT( !inf_invalid(lp1) );
	ASSERT( !inf_invalid(lp2) );
#endif
	if (lp1 == lp2)
		return (1);

	if (lp1->il_level < lp2->il_level)
		return (0);
	if (lp1->il_catcount < lp2->il_catcount)
		return (0);

	return (subset(lp2->il_list, lp2->il_catcount,
	    lp1->il_list, lp1->il_catcount));
}

/*
 *  Given two arrays of unsigned short integers, l1 and l2, 
 *  of respective dimensions c1 and c2,
 *  each sorted in ascending sequence,
 *  return 1 iff l1 is a (proper or improper) subset of l2;
 *  That is, return 1 is every element of l1 is in l2.
 *  Return 0 otherwise.
 *  This routine requires, but does not check nor ASSERT, that both
 *  arrays be sorted.  
 *  It does not return correct results if lists are not sorted.
 */
static int
subset( unsigned short *l1,	/* pointer to first element of list 1 */
	int             c1,	/* number of elements of list 1 */
	unsigned short *l2,	/* pointer to first element of list 2 */
	int             c2)	/* number of elements of list 2 */
{
	unsigned short *l1e;
	unsigned short *l2e;
	unsigned int    i1;
	unsigned int    i2;

	if (c1 == 0)
		return 1;
	l1e = l1 + c1;
	l2e = l2 + c2;
	while (l2 < l2e) {
		if ((i2 = *l2++) == (i1 = *l1)) {
			if (++l1 < l1e)
				continue;
			return 1;
		}
		if (i2 > i1)		/* i1 is missing from l2 */
			return 0;
	}
	return 0;
}

/*
 * inf_dup(lp):
 *	Returns a copy of the passed label.
 *	Uses malloc to create the data space. The caller will have to
 *	free it when he's done with it.
 */

inf_label *
inf_dup(inf_label *lp)
{
	inf_label *result;	/* result goes here */
	int size;		/* size of the result */

#ifdef	_KERNEL
	size = inf_size(lp);
	ASSERT( size != 0 );
	/* in kernel space, only allocate enough room for the label */
	result = (inf_label *)kern_malloc(size);
#else	/* _KERNEL */
	if ((size = inf_size(lp)) == 0)
		return ((inf_label *)0);
	/* in user space, always return a full inf_label */
	result = (inf_label *)malloc(sizeof(inf_label));
#endif	/* _KERNEL */

	if (result == (inf_label *)0)
		return ((inf_label *)0);

	bcopy((char *)lp, (char *)result, size);

	return (result);
}

/*
 * inf_size(lp):
 *	Returns the size of the passed label in bytes.
 */
int
inf_size(inf_label *lp)
{
#ifdef	_KERNEL
	if (lp == (inf_label *)0)
		return 0;
#else
	/* if it is a invalid label, return size 0 */
	if (inf_invalid(lp)) 
		return(0);
#endif
	return ( (caddr_t)&lp->il_list[lp->il_catcount] - (caddr_t)lp );
}
