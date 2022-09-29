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
#ident "$Revision: 1.8 $"

#include "dmi_private.h"


int
dm_init_attrloc(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrloc_t	*locp)
{
	dm_fsys_vector_t *fsys_vector;
	dm_tokdata_t	*tdp;
	int 		error;

	error = dm_app_get_tdp(sid, hanp, hlen, token, DM_TDT_VFS|DM_TDT_DIR,
		DM_RIGHT_SHARED, &tdp);
	if (error != 0)
		return(error);

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->init_attrloc(tdp->td_bdp, tdp->td_right, locp);

	dm_app_put_tdp(tdp);
	return(error);
}


/*
 * Retrieves both standard and DM specific file attributes for the file
 * system indicated by the handle. (The FS has to be mounted).
 * Syscall returns 1 to indicate SUCCESS and more information is available.
 * -1 is returned on error, and errno will be set appropriately.
 * 0 is returned upon successful completion.
 */

int
dm_get_bulkattr_rvp(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		mask,
	dm_attrloc_t	*locp,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp,
	rval_t		*rvp)
{
	dm_fsys_vector_t *fsys_vector;
	dm_tokdata_t	*tdp;
	int 		error;

	error = dm_app_get_tdp(sid, hanp, hlen, token, DM_TDT_VFS,
		DM_RIGHT_SHARED, &tdp);
	if (error != 0)
		return(error);

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->get_bulkattr_rvp(tdp->td_bdp, tdp->td_right,
			mask, locp, buflen, bufp, rlenp, rvp);

	dm_app_put_tdp(tdp);
	return(error);
}


/*
 * Retrieves attributes of directory entries given a handle to that
 * directory. Iterative.
 * Syscall returns 1 to indicate SUCCESS and more information is available.
 * -1 is returned on error, and errno will be set appropriately.
 * 0 is returned upon successful completion.
 */

int
dm_get_dirattrs_rvp(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		mask,
	dm_attrloc_t	*locp,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp,
	rval_t		*rvp)
{
	dm_fsys_vector_t *fsys_vector;
	dm_tokdata_t	*tdp;
	int 		error;

	error = dm_app_get_tdp(sid, hanp, hlen, token, DM_TDT_DIR,
		DM_RIGHT_SHARED, &tdp);
	if (error != 0)
		return(error);

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->get_dirattrs_rvp(tdp->td_bdp, tdp->td_right,
		mask, locp, buflen, bufp, rlenp, rvp);

	dm_app_put_tdp(tdp);
	return(error);
}


int
dm_get_bulkall_rvp(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		mask,
	dm_attrname_t	*attrnamep,
	dm_attrloc_t	*locp,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp,
	rval_t		*rvp)
{
	dm_fsys_vector_t *fsys_vector;
	dm_tokdata_t	*tdp;
	int 		error;

	error = dm_app_get_tdp(sid, hanp, hlen, token, DM_TDT_VFS,
		DM_RIGHT_SHARED, &tdp);
	if (error != 0)
		return(error);

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->get_bulkall_rvp(tdp->td_bdp, tdp->td_right,
		mask, attrnamep, locp, buflen, bufp, rlenp, rvp);

	dm_app_put_tdp(tdp);
	return(error);
}
