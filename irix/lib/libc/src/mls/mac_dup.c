
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

#ident "$Revision: 1.3 $"

#ifdef __STDC__
	#pragma weak mac_dup = _mac_dup
	#pragma weak mac_demld = _mac_demld
	#pragma weak mac_set_moldy = _mac_set_moldy
	#pragma weak mac_is_moldy = _mac_is_moldy
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <sys/mac.h>
#include <sys/mac_label.h>
#include <stdlib.h>

/*
 * mac_dup(lp):
 *	Returns a copy of the passed label.
 *	Uses malloc to create the data space. The caller will have to
 *	free it when he's done with it.
 */

mac_t
mac_dup(mac_t lp)
{
	mac_t result;		/* result goes here */
	ssize_t size;		/* size of the result */

	if ((size = mac_size(lp)) == (ssize_t) -1)
		return ((mac_t) NULL);

	/* in user space, always return a full mac_label */
	result = (mac_t) malloc (sizeof (mac_label));
	if (result == (mac_t) NULL)
		return (result);

	memcpy((void *) result, (void *) lp, (size_t) size);
	return (result);
}

/*
 * mac_demld(lp):
 *	Returns a copy of the passed label, changing the moldy status
 *	if it's set.
 */

mac_t
mac_demld(mac_t lp)
{
	mac_t result;		/* result goes here */

	if ((result = mac_dup(lp)) == (mac_t) NULL)
		return (result);

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

mac_t
mac_set_moldy(mac_t lp)
{
	mac_t result;		/* result goes here */

	if ((result = mac_dup(lp)) == (mac_t) NULL)
		return (result);

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

/*
 * mac_is_moldy
 *	Returns 1 iff the label is moldy.
 *	Returns 0 otherwise, including errors.
 */
int
mac_is_moldy(mac_t lp)
{
	return (lp->ml_msen_type == MSEN_MLD_HIGH_LABEL || 
		lp->ml_msen_type == MSEN_MLD_LOW_LABEL || 
		lp->ml_msen_type == MSEN_MLD_LABEL);
}
