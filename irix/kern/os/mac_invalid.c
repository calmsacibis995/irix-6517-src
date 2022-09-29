
/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.5 $"

/*LINTLIBRARY*/
#include <sys/types.h>
#include <sys/param.h>
#include <sys/mac_label.h>
#ifndef _KERNEL
#ifdef __STDC__
	#pragma weak mac_invalid = _mac_invalid
#endif

#include "synonyms.h"
#include <mls.h>
#else

/* for the kernel, this is a boolean */

#define	VALID_MAC_LABEL		0  /* valid label */
#define	NULL_MAC_LABEL		1  /* invalid label */
#define	INVALID_MSENTYPE	1  /* invalid label */
#define	INVALID_MINTTYPE	1  /* invalid label */
#define	INVALID_MSEN_ASSOC	1  /* invalid label */
#define	INVALID_MINT_ASSOC	1  /* invalid label */
#define	INVALID_SET_FORMAT	1  /* invalid label */
#define	INVALID_SET_COUNT	1  /* invalid label */

#endif /* _KERNEL */

/*
 * check the category set value or division set value see whether they
 * are in ascending order and the value appear only once
 */

static int
check_setvalue(unsigned short *list, unsigned short count)
{
	int i;

	for (i = 1; i < count ; i++)
		if (list[i] <= list[i-1])
			return -1;
	return 0;
}

/*
 * mac_invalid(lp)
 * check the validity of a mac label
 */
int
mac_invalid(mac_label *lp)
{
	if (lp == NULL)
		return(NULL_MAC_LABEL);

	/*
	 * if the total category set and division set is greater than 250
	 * report error
	 */
	if ((lp->ml_catcount + lp->ml_divcount) > MAC_MAX_SETS)
		return(INVALID_SET_COUNT);

	/*
	 * check whether the msentype value is valid, and do they have
  	 * appropriate level, category association.
         */
	switch (lp->ml_msen_type) {
	case MSEN_ADMIN_LABEL:
	case MSEN_EQUAL_LABEL:
	case MSEN_HIGH_LABEL:
	case MSEN_MLD_HIGH_LABEL:
	case MSEN_LOW_LABEL:
	case MSEN_MLD_LOW_LABEL:
		if (lp->ml_level != 0 || lp->ml_catcount > 0 )
			return(INVALID_MSEN_ASSOC);
		break;
	case MSEN_TCSEC_LABEL:
	case MSEN_MLD_LABEL:
		if (lp->ml_catcount > 0)
			if (check_setvalue(lp->ml_list, lp->ml_catcount) < 0)
				return(INVALID_SET_FORMAT);
		break;
	case MSEN_UNKNOWN_LABEL:
	default:
		return(INVALID_MSENTYPE);
	}

	/*
	 * check whether the minttype value is valid, and do they have
	 * appropriate grade, division association.
	 */
	switch (lp->ml_mint_type) {
	case MINT_BIBA_LABEL:
		if (lp->ml_divcount > 0)
			if (check_setvalue((lp->ml_list) + lp->ml_catcount,
			    lp->ml_divcount) < 0)
				return(INVALID_SET_FORMAT);
		break;
	case MINT_EQUAL_LABEL:
	case MINT_HIGH_LABEL:
	case MINT_LOW_LABEL:
		if (lp->ml_grade != 0 || lp->ml_divcount > 0 )
			return(INVALID_MINT_ASSOC);
		break;
	default:
		return(INVALID_MINTTYPE);
	}

	return(0);
}
