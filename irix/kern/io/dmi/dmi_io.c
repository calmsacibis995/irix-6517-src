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
#ident "$Revision: 1.34 $"


#include "dmi_private.h"


int
dm_read_invis_rvp(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_off_t	off,
	dm_size_t	len,
	void		*bufp,
	rval_t		*rvp)
{
	dm_fsys_vector_t *fsys_vector;
	dm_tokdata_t	*tdp;
	int 		error;

	error = dm_app_get_tdp(sid, hanp, hlen, token, DM_TDT_REG,
		DM_RIGHT_SHARED, &tdp);
	if (error != 0)
		return(error);

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->read_invis_rvp(tdp->td_bdp, tdp->td_right,
		off, len, bufp, rvp);

	dm_app_put_tdp(tdp);
	return(error);
}


int
dm_write_invis_rvp(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	int		flags,
	dm_off_t	off,
	dm_size_t	len,
	void		*bufp,
	rval_t		*rvp)
{
	dm_fsys_vector_t *fsys_vector;
	dm_tokdata_t	*tdp;
	int 		error;

	error = dm_app_get_tdp(sid, hanp, hlen, token, DM_TDT_REG,
		DM_RIGHT_EXCL, &tdp);
	if (error != 0)
		return(error);

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->write_invis_rvp(tdp->td_bdp, tdp->td_right,
		flags, off, len, bufp, rvp);

	dm_app_put_tdp(tdp);
	return(error);
}


int
dm_sync_by_handle (
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token)
{
	dm_fsys_vector_t *fsys_vector;
	dm_tokdata_t	*tdp;
	int 		error;

	error = dm_app_get_tdp(sid, hanp, hlen, token, DM_TDT_REG,
		DM_RIGHT_EXCL, &tdp);
	if (error != 0)
		return(error);

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->sync_by_handle(tdp->td_bdp, tdp->td_right);

	dm_app_put_tdp(tdp);
	return(error);
}


int
dm_get_dioinfo (
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_dioinfo_t	*diop)
{
	dm_fsys_vector_t *fsys_vector;
	dm_tokdata_t	*tdp;
	int 		error;

	error = dm_app_get_tdp(sid, hanp, hlen, token, DM_TDT_REG,
		DM_RIGHT_SHARED, &tdp);
	if (error != 0)
		return(error);

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->get_dioinfo(tdp->td_bdp, tdp->td_right, diop);

	dm_app_put_tdp(tdp);
	return(error);
}
