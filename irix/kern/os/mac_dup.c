
/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1989, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.11 $"

#ifndef _KERNEL
#include <libc_synonyms.h>
#endif
#include <sys/types.h>
#include <sys/mac_label.h>
#ifdef	_KERNEL
#include <sys/debug.h>
#include <sys/systm.h>
#else
#include <stdlib.h>
#endif	/* _KERNEL */

/*
 * mac_dup(lp):
 *	Returns a copy of the passed label.
 *	Uses malloc to create the data space. The caller will have to
 *	free it when he's done with it.
 */

mac_label *
mac_dup(mac_label *lp)
{
	mac_label *result;	/* result goes here */
	ssize_t size;		/* size of the result */

#ifdef	_KERNEL
	size = mac_size(lp);
	ASSERT( size != 0 );
	/* in kernel space, only allocate enough room for the label */
	result = (mac_label *)kern_malloc(size);
#else	/* _KERNEL */
	if ((size = mac_size(lp)) == 0)
		return ((mac_label *)0);
	/* in user space, always return a full mac_label */
	result = (mac_label *)malloc(sizeof(mac_label));
#endif	/* _KERNEL */

	if (result == (mac_label *)0)
		return ((mac_label *)0);

	bcopy((char *)lp, (char *)result, size);

	return (result);
}

/*
 * mac_size(lp):
 *	Returns the size of the passed label in bytes.
 */
ssize_t
mac_size(mac_label *lp)
{
#ifdef	_KERNEL
	if (lp == (mac_label *)0)
		return 0;
#else
	/* if it is a invalid label, return size 0 */
	if (mac_invalid(lp)) 
		return(0);
#endif
	return ( (caddr_t)&lp->ml_list[ lp->ml_catcount + lp->ml_divcount ]
	       - (caddr_t)lp );
}

/*
 * mac_demld(lp):
 *	Returns a copy of the passed label, changing the moldy status
 *	if it's set.
 */

mac_label *
mac_demld(mac_label *lp)
{
	mac_label *result;	/* result goes here */

	if ((result = mac_dup(lp)) == (mac_label *)0)
		return ((mac_label *)0);

	switch (lp->ml_msen_type) {
	case MSEN_MLD_HIGH_LABEL:
		result->ml_msen_type = MSEN_HIGH_LABEL;
		break;
	case MSEN_MLD_LOW_LABEL:
		result->ml_msen_type = MSEN_LOW_LABEL;
		break;
	case MSEN_MLD_LABEL:
		result->ml_msen_type = MSEN_TCSEC_LABEL;
		break;
	default:
		break;
	}
	return (result);
}


/*
 * mac_set_moldy(lp):
 *	Returns a copy of the passed label, setting the moldy status.
 *	Only makes sense if mac label type is hig, tcsec, or low.
 */

mac_label *
mac_set_moldy(mac_label *lp)
{
	mac_label *result;	/* result goes here */

	if ((result = mac_dup(lp)) == (mac_label *)0)
		return ((mac_label *)0);

	switch (lp->ml_msen_type) {
	case MSEN_HIGH_LABEL:
		result->ml_msen_type = MSEN_MLD_HIGH_LABEL;
		break;
	case MSEN_LOW_LABEL:
		result->ml_msen_type = MSEN_MLD_LOW_LABEL;
		break;
	case MSEN_TCSEC_LABEL:
		result->ml_msen_type = MSEN_MLD_LABEL;
		break;
	default:
		break;
	}
	return (result);
}

