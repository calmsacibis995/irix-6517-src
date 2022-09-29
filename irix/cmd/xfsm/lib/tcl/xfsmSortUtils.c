/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident  "xfsmSortUtils.c: $Revision: 1.2 $"

/*
 * Filename:	xfsmSortUtils.c
 *
 * Synopsis:	Functions that are callable from qsort(3C).
 *
 * Description:	These functions are called from qsort(3C) in order to sort
 *		a data set. 
 *
 *		This file contains the following functions:
 *			xfsmSortPartNames	Sort partition names.
 *			xfsmSortDiskObjs	Sort disk object signatures.
 */
#include	<stdio.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	<tcl.h>

extern Tcl_Interp	*sortInterp;

/*
 * Usage:	int xfsmSortPartNames(const void *item1, const void *item2)
 *
 * Synopsis:	Sort partition names in numerically ascending order.
 *
 * Arguments:	item1	First item to be compared.
 *		item2	Second item to be compared.
 *
 * Returns:	-1	item1 <  item2
 *		0	item1 == item2
 *		1	item1 >  item2
 *
 * Description:	This function is called from qsort(3C).  It is used to sort
 *		structured strings that contain both alpha and numeric 
 *		characters where consecutive integers are considered to
 *		comprise a number rather than just part of a character string.
 *
 *		This applies to, among other things, partition names.  Take,
 *		for example the partitions dks0d1s7 and dks0d1s11.  If we
 *		use strcmp(3C) on these, dks0d1s7 would compare as greater
 *		than dks0d1s11.  This function will compare dks0d1s7 as
 *		less than dks0d1s11, since 7 is numerically less than 11.
 */
int
xfsmSortPartNames(const void *item1, const void *item2)
{
	const char	*p1 = *(const char **)item1;
	const char	*p2 = *(const char **)item2;

	char	num1[64], num2[64];
	char	*n1 = num1,
		*n2 = num2;

	/*
	 *	Find first non-matching character
	 */
	for( ; *p1 == *p2 && *p1 != NULL; ++p1, ++p2)
		;

	if(*p1 == NULL) {
		if(*p2 == NULL) {
			return(0);
		}
		return(-1);
	} else if(*p2 == NULL) {
		return(1);
	}

	if(isdigit(*p1)) {
		if(isdigit(*p2)) {
			while(isdigit(*p1)) {
				*n1++ = *p1++;
			}
			*n1 = NULL;
			while(isdigit(*p2)) {
				*n2++ = *p2++;
			}
			*n2 = NULL;
			if(atoi(num1) < atoi(num2)) {
				return(-1);
			} else {
				return(1);
			}
		}
		else {
			return(1);
		}
	} else if(isdigit(*p2)) {
		return(-1);
	} else {
		return(*p1 < *p2 ? -1 : 1);
	}
}

/*
 * Usage:	int xfsmSortDiskObjs(const void *item1, const void *item2)
 *
 * Synopsis:	Sort disk object signatures in numerically ascending order.
 *
 * Arguments:	item1	First item to be compared.
 *		item2	Second item to be compared.
 *
 * Returns:	-1	item1 <  item2
 *		0	item1 == item2
 *		1	item1 >  item2
 *
 * Description:	This function is called from qsort(3C).  It is used to sort
 *		structured strings that represent XFSM disk object signatures.
 *
 *		Each item is a disk object signature.  They are first split
 *		using Tcl_SplitList() so that we can access the name contained
 *		in the signature.  The function xfsmSortPartNames() is called
 *		to do the actual comparison, since disk names and partition
 *		names are of the same structure (alpha/numeric).
 */
int
xfsmSortDiskObjs(const void *item1, const void *item2)
{
	const char	*p1 = *(const char **)item1;
	const char	*p2 = *(const char **)item2;
	int		p1_itemc,
			p2_itemc;
	char		**p1_itemv,
			**p2_itemv;
	int		rv = 0;

	if (Tcl_SplitList(sortInterp, p1, &p1_itemc, &p1_itemv) != TCL_OK) {
		return 0;
	}
	if (Tcl_SplitList(sortInterp, p2, &p2_itemc, &p2_itemv) != TCL_OK) {
		free(p1_itemv);
		return 0;
	}
	if(p1_itemc == p2_itemc && p1_itemc == 4) {
		rv = xfsmSortPartNames((const void *)&p1_itemv[2],
				       (const void *)&p2_itemv[2]);
	}

	free(p1_itemv);
	free(p2_itemv);
	return(rv);
}
