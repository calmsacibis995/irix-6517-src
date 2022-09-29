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

#ident "$Revision: 1.18 $"

#ifndef _KERNEL
#ifdef __STDC__
	#pragma weak mac_dom = _mac_dom
	#pragma weak mac_equ = _mac_equ
	#pragma weak mac_is_moldy = _mac_is_moldy
	#pragma weak msen_is_moldy = _msen_is_moldy
	#pragma weak mac_inrange = _mac_inrange
#endif

#include "synonyms.h"
#endif
#include <sys/types.h>
#include <sys/mac_label.h>
#ifdef	_KERNEL
#include <sys/debug.h>
#include <sys/systm.h>
#endif

/*
 * Compare the equality relationship between two TCSEC labels
 */
static int
tcsec_equ(mac_label *lp1, mac_label *lp2)
{

	/*
	 * Check the heirarchical levels
	 */
	if (lp1->ml_level != lp2->ml_level)
		return (0);
	/*
	 * Check the category sets
	 */
	if (lp1->ml_catcount != lp2->ml_catcount)
		return (0);
	/*
 	 * Check that every element of l2 is in l1.
	 */
	if (lp1->ml_catcount == 0)
		return 1;
	return ! bcmp(lp1->ml_list, lp2->ml_list, 
		      lp1->ml_catcount * sizeof( lp1->ml_list[0] ));
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
subset(unsigned short *l1, int c1, unsigned short *l2, int c2)
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
 * Compare the dominance relationship between two TCSEC labels
 */
static int
tcsec_dom(mac_label *lp1, mac_label *lp2)
{

	/*
	 * Check the heirarchical levels
	 */
	if (lp1->ml_level < lp2->ml_level)
		return (0);
	/*
	 * Check the category sets
	 */
	if (lp1->ml_catcount < lp2->ml_catcount)
		return (0);
	/*
	 * return true (1) if lp2's categories are a subset of lp1's
	 */
	return subset( lp2->ml_list, lp2->ml_catcount,
	               lp1->ml_list, lp1->ml_catcount );
}

/*
 * Compare the equality relationship between two BIBA labels
 */
static int
biba_equ(mac_label *lp1, mac_label *lp2)
{

	/*
	 * The trivial check.
	 */
	if (lp1 == lp2)
		return (1);
	/*
	 * Check the heirarchical grade
	 */
	if (lp1->ml_grade != lp2->ml_grade)
		return (0);
	/*
	 * Check the division sets
	 */
	if (lp1->ml_divcount != lp2->ml_divcount)
		return (0);
	/*
 	 * Check that every element of l2 is in l1.
	 */
	if (lp1->ml_divcount == 0)
		return 1;
	return ! bcmp(lp1->ml_list + lp1->ml_catcount, 
		      lp2->ml_list + lp2->ml_catcount, 
		      lp1->ml_divcount * sizeof( lp1->ml_list[0] ));
}

/*
 * Compare the dominance relationship between two BIBA labels
 */
static int
biba_dom(mac_label *lp1, mac_label *lp2)
{

	/*
	 * The trivial check.
	 */
	if (lp2 == lp1)
		return (1);
	/*
	 * Check the heirarchical grade
	 */
	if (lp2->ml_grade < lp1->ml_grade)
		return (0);
	/*
	 * Check the division sets
	 */
	if (lp2->ml_divcount < lp1->ml_divcount)
		return (0);
	/*
 	 * Check that every element of lp1 is in lp2.
	 * It is not required that every element of lp2 be in lp1.
	 * return true (1) if lp1's divisions are a subset of lp2's
	 */
	return subset( lp1->ml_list + lp1->ml_catcount, lp1->ml_divcount,
	               lp2->ml_list + lp2->ml_catcount, lp2->ml_divcount );
}


/*
 * mac_dom
 * 	Returns 1 iff the first label dominates the second label
 *	Returns 0 otherwise, including errors.
 */
int
mac_dom(mac_label *lp1, mac_label *lp2)
{
#ifndef	_KERNEL
	if (mac_invalid(lp1) || mac_invalid(lp2))
		return (0);
#else
	ASSERT( !mac_invalid(lp1) );
	ASSERT( !mac_invalid(lp2) );
#endif
	if (lp1 == lp2)
		return (1);

	/*
	 * If either label is MSEN_EQUAL don't check sensitivity further.
	 */
	if (lp1->ml_msen_type != MSEN_EQUAL_LABEL &&
	    lp2->ml_msen_type != MSEN_EQUAL_LABEL) {

		switch (lp1->ml_msen_type) {
		case MSEN_ADMIN_LABEL:
			switch (lp2->ml_msen_type) {
			case MSEN_ADMIN_LABEL:
			case MSEN_LOW_LABEL:
			case MSEN_MLD_LOW_LABEL:
				break;
			default:
				return (0);
			}
			break;
		case MSEN_HIGH_LABEL:
		case MSEN_MLD_HIGH_LABEL:
			/*
			 * High always dominates.
			 */
			break;
		case MSEN_LOW_LABEL:
		case MSEN_MLD_LOW_LABEL:
			switch (lp2->ml_msen_type) {
			case MSEN_LOW_LABEL:
			case MSEN_MLD_LOW_LABEL:
				break;
			default:
				return (0);
			}
			break;
		case MSEN_MLD_LABEL:
		case MSEN_TCSEC_LABEL:
			switch (lp2->ml_msen_type) {
			case MSEN_LOW_LABEL:
			case MSEN_MLD_LOW_LABEL:
				break;
			case MSEN_TCSEC_LABEL:
			case MSEN_MLD_LABEL:
				if (!tcsec_dom(lp1, lp2))
					return (0);
				break;
			default:
				return (0);
			}
			break;
		default:
			return (0);
		}
	}
	/*
	 * If either label is MINT_EQUAL we're done.
	 */
	if (lp1->ml_mint_type == MINT_EQUAL_LABEL ||
	    lp2->ml_mint_type == MINT_EQUAL_LABEL)
		return (1);

	switch (lp1->ml_mint_type) {
		case MINT_LOW_LABEL:
			return (1);
		case MINT_BIBA_LABEL:
			switch (lp2->ml_mint_type) {
			case MINT_BIBA_LABEL:
				return (biba_dom(lp1, lp2));
			case MINT_LOW_LABEL:
				return (0);
			default:
				break;
			}
		case MINT_HIGH_LABEL:
			return (lp2->ml_mint_type == MINT_HIGH_LABEL);
	}
	return (0);
}

/*
 * mac_equ
 * 	Returns 1 iff the labels are equal
 *	Returns 0 otherwise, including errors.
 */
int
mac_equ(
	mac_label *lp1,
	mac_label *lp2)
{
#ifndef	_KERNEL
	if (mac_invalid(lp1) || mac_invalid(lp2))
		return (0);
#else
	ASSERT( !mac_invalid(lp1) );
	ASSERT( !mac_invalid(lp2) );
#endif
	if (lp1 == lp2)
		return (1);

	/*
	 * If either label is MSEN_EQUAL don't check sensitivity further.
	 */
	if (lp1->ml_msen_type != MSEN_EQUAL_LABEL &&
	    lp2->ml_msen_type != MSEN_EQUAL_LABEL) {

		switch (lp1->ml_msen_type) {
		case MSEN_ADMIN_LABEL:
			switch (lp2->ml_msen_type) {
			case MSEN_ADMIN_LABEL:
				break;
			default:
				return (0);
			}
			break;
		case MSEN_HIGH_LABEL:
		case MSEN_MLD_HIGH_LABEL:
			switch (lp2->ml_msen_type) {
			case MSEN_HIGH_LABEL:
			case MSEN_MLD_HIGH_LABEL:
				break;
			default:
				return (0);
			}
			break;
		case MSEN_LOW_LABEL:
		case MSEN_MLD_LOW_LABEL:
			switch (lp2->ml_msen_type) {
			case MSEN_LOW_LABEL:
			case MSEN_MLD_LOW_LABEL:
				break;
			default:
				return (0);
			}
			break;
		case MSEN_MLD_LABEL:
		case MSEN_TCSEC_LABEL:
			switch (lp2->ml_msen_type) {
			case MSEN_TCSEC_LABEL:
			case MSEN_MLD_LABEL:
				if (!tcsec_equ(lp1, lp2))
					return (0);
				break;
			default:
				return (0);
			}
			break;
		default:
			return (0);
		}
	}
	/*
	 * If either label is MINT_EQUAL we're done.
	 */
	if (lp1->ml_mint_type == MINT_EQUAL_LABEL ||
	    lp2->ml_mint_type == MINT_EQUAL_LABEL)
		return (1);

	if (lp1->ml_mint_type != lp2->ml_mint_type)
		return (0);

	switch (lp1->ml_mint_type) {
		case MINT_HIGH_LABEL:
		case MINT_LOW_LABEL:
			return (1);
		case MINT_BIBA_LABEL:
			return (biba_equ(lp1, lp2));
		default:
			break;
	}
	return (0);
}

/*
 * mac_is_moldy
 *	Returns 1 iff the label is moldy.
 *	Returns 0 otherwise, including errors.
 */
int
mac_is_moldy(mac_label *lp)
{
#ifdef _KERNEL
	ASSERT(lp);
#endif
	return (lp->ml_msen_type == MSEN_MLD_HIGH_LABEL || 
	    lp->ml_msen_type == MSEN_MLD_LOW_LABEL || 
	    lp->ml_msen_type == MSEN_MLD_LABEL);
}

/*
 * mac_is_moldy
 *	Returns 1 iff the label is moldy.
 *	Returns 0 otherwise, including errors.
 */
int
msen_is_moldy(mac_b_label *msen)
{
#ifdef _KERNEL
	ASSERT(msen);
#endif
	return (msen->b_type == MSEN_MLD_HIGH_LABEL ||
		msen->b_type == MSEN_MLD_LOW_LABEL ||
		msen->b_type == MSEN_MLD_LABEL);
}

/*
 * mac_inrange 
 * 	Returns true iff (mac_dom(max,lbl) && mac_dom(lbl,min)).
 *	Returns 0 otherwise, including errors.
 */
int
mac_inrange(mac_label *min, mac_label *lp1, mac_label *max)
{
	return (mac_dom(max, lp1) && mac_dom(lp1, min));
}

#ifdef	_KERNEL
/*
 * mac_mint_equ
 *	Returns 1 iff two labels are comparable 
 *	and are equal *with respect to integrity*.
 *	Returns 0 otherwise, including errors.
 */
int
mac_mint_equ(
	mac_label *lp1,
	mac_label *lp2)
{
	ASSERT( !mac_invalid(lp1) );
	ASSERT( !mac_invalid(lp2) );
	if (lp1 == lp2)
		return (1);

	/*
	 * If either label is MINT_EQUAL we're done.
	 */
	if (lp1->ml_mint_type == MINT_EQUAL_LABEL ||
	    lp2->ml_mint_type == MINT_EQUAL_LABEL)
		return (1);

	if (lp1->ml_mint_type != lp2->ml_mint_type)
		return (0);

	switch (lp1->ml_mint_type) {
		case MINT_HIGH_LABEL:
		case MINT_LOW_LABEL:
			return (1);
		case MINT_BIBA_LABEL:
			return (biba_equ(lp1, lp2));
		default:
			break;
	}
	return (0);
}

/*
 * mac_cat_equ
 *	Returns 1 iff two labels are comparable 
 *	and are equal *with respect to categories*.
 *	Ignores MINT, ignores level, only looks at categories.
 *	Returns 0 otherwise, including errors.
 */
int
mac_cat_equ(
	mac_label *lp1,
	mac_label *lp2)
{
	/*
	 * If either label is a type which lacks categories call mac_equ.
	 */
	if (!((lp1->ml_msen_type == MSEN_TCSEC_LABEL ||
	       lp1->ml_msen_type == MSEN_MLD_LABEL) &&
	      (lp2->ml_msen_type == MSEN_TCSEC_LABEL ||
	       lp2->ml_msen_type == MSEN_MLD_LABEL)))
		return (mac_equ(lp1, lp2));

	/*
	 * Check the category sets
	 */
	if (lp1->ml_catcount != lp2->ml_catcount)
		return 0;
	/*
 	 * Check that every element of l2 is in l1.
	 */
	if (lp1->ml_catcount == 0)
		return 1;
	return ! bcmp(lp1->ml_list, lp2->ml_list, 
		      lp1->ml_catcount * sizeof( lp1->ml_list[0] ));
}
#endif	/* _KERNEL */
