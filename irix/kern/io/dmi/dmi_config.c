/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.1 $"


#include "dmi_private.h"

int
dm_get_config(
	void		*hanp,
	size_t		hlen,
	dm_config_t	flagname,
	dm_size_t	*retvalp)
{
	dm_fsys_vector_t *fsys_vector;
	dm_tokdata_t	*tdp;
	dm_size_t	retval;
	int		system = 1;
	int		error;

	/* Trap and process configuration parameters which are system-wide. */

	switch (flagname) {
	case DM_CONFIG_LEGACY:
	case DM_CONFIG_PENDING:
	case DM_CONFIG_OBJ_REF:
		retval = DM_TRUE;
		break;
	case DM_CONFIG_MAX_MESSAGE_DATA:
		retval = DM_MAX_MSG_DATA;
		break;
	default:
		system = 0;
		break;
	}
	if (system) {
		if (copyout(&retval, retvalp, sizeof(retval)))
			return(EFAULT);
		return(0);
	}

	/* Must be filesystem-specific.  Convert the handle into a vnode. */

	if ((error = dm_get_config_tdp(hanp, hlen, &tdp)) != 0)
		return(error);

	/* Now call the filesystem-specific routine to determine the
	   value of the configuration option for that filesystem.
	*/

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->get_config(tdp->td_bdp, tdp->td_right,
		flagname, retvalp);

	dm_app_put_tdp(tdp);
	return(error);
}


int
dm_get_config_events(
	void		*hanp,
	size_t		hlen,
	u_int		nelem,
	dm_eventset_t	*eventsetp,
	u_int		*nelemp)
{
	dm_fsys_vector_t *fsys_vector;
	dm_tokdata_t	*tdp;
	int		error;

	/* Convert the handle into a vnode. */

	if ((error = dm_get_config_tdp(hanp, hlen, &tdp)) != 0)
		return(error);

	/* Now call the filesystem-specific routine to determine the
	   events supported by that filesystem.
	*/

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->get_config_events(tdp->td_bdp, tdp->td_right,
		nelem, eventsetp, nelemp);

	dm_app_put_tdp(tdp);
	return(error);
}
