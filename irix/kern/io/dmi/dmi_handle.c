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
dm_create_by_handle(
	dm_sessid_t	sid,
	void		*dirhanp,
	size_t		dirhlen,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen,
	char		*cname)
{
	dm_fsys_vector_t *fsys_vector;
	dm_tokdata_t	*tdp;
	int 		error;

	error = dm_app_get_tdp(sid, dirhanp, dirhlen, token, DM_TDT_DIR,
		DM_RIGHT_EXCL, &tdp);
	if (error != 0)
		return(error);

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->create_by_handle(tdp->td_bdp, tdp->td_right,
		hanp, hlen, cname);

	dm_app_put_tdp(tdp);
	return(error);
}


int
dm_mkdir_by_handle(
	dm_sessid_t	sid,
	void		*dirhanp,
	size_t		dirhlen,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen,
	char		*cname)
{
	dm_fsys_vector_t *fsys_vector;
	dm_tokdata_t	*tdp;
	int 		error;

	error = dm_app_get_tdp(sid, dirhanp, dirhlen, token, DM_TDT_DIR,
		DM_RIGHT_EXCL, &tdp);
	if (error != 0)
		return(error);

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->mkdir_by_handle(tdp->td_bdp, tdp->td_right,
		hanp, hlen, cname);

	dm_app_put_tdp(tdp);
	return(error);
}


int
dm_symlink_by_handle(
	dm_sessid_t	sid,
	void		*dirhanp,
	size_t		dirhlen,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen,
	char		*cname,
	char		*path)
{
	dm_fsys_vector_t *fsys_vector;
	dm_tokdata_t	*tdp;
	int 		error;

	error = dm_app_get_tdp(sid, dirhanp, dirhlen, token, DM_TDT_DIR,
		DM_RIGHT_EXCL, &tdp);
	if (error != 0)
		return(error);

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->symlink_by_handle(tdp->td_bdp, tdp->td_right,
		hanp, hlen, cname, path);

	dm_app_put_tdp(tdp);
	return(error);
}
