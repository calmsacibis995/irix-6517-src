
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
#ident "$Revision: 1.4 $"


#include "dmi_private.h"


int
dm_clear_inherit(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrname_t	*attrnamep)
{
	dm_fsys_vector_t *fsys_vector;
	dm_tokdata_t	*tdp;
	int 		error;

	error = dm_app_get_tdp(sid, hanp, hlen, token, DM_TDT_VFS,
		DM_RIGHT_EXCL, &tdp);
	if (error != 0)
		return(error);

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->clear_inherit(tdp->td_bdp, tdp->td_right,
		attrnamep);

	dm_app_put_tdp(tdp);
	return(error);
}


int
dm_get_dmattr(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrname_t	*attrnamep,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp)
{
	dm_fsys_vector_t *fsys_vector;
	dm_tokdata_t	*tdp;
	int 		error;

	error = dm_app_get_tdp(sid, hanp, hlen, token, DM_TDT_VNO,
		DM_RIGHT_SHARED, &tdp);
	if (error != 0)
		return(error);

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->get_dmattr(tdp->td_bdp, tdp->td_right,
		attrnamep, buflen, bufp, rlenp);

	dm_app_put_tdp(tdp);
	return(error);
}


int
dm_getall_dmattr(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp)
{
	dm_fsys_vector_t *fsys_vector;
	dm_tokdata_t	*tdp;
	int 		error;

	error = dm_app_get_tdp(sid, hanp, hlen, token, DM_TDT_VNO,
		DM_RIGHT_SHARED, &tdp);
	if (error != 0)
		return(error);

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->getall_dmattr(tdp->td_bdp, tdp->td_right,
		buflen, bufp, rlenp);

	dm_app_put_tdp(tdp);
	return(error);
}


int
dm_getall_inherit(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		nelem,
	dm_inherit_t	*inheritbufp,
	u_int		*nelemp)
{
	dm_fsys_vector_t *fsys_vector;
	dm_tokdata_t	*tdp;
	int 		error;

	error = dm_app_get_tdp(sid, hanp, hlen, token, DM_TDT_VFS,
		DM_RIGHT_SHARED, &tdp);
	if (error != 0)
		return(error);

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->getall_inherit(tdp->td_bdp, tdp->td_right,
		nelem, inheritbufp, nelemp);

	dm_app_put_tdp(tdp);
	return(error);
}


int
dm_remove_dmattr(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	int		setdtime,
	dm_attrname_t	*attrnamep)
{
	dm_fsys_vector_t *fsys_vector;
	dm_tokdata_t	*tdp;
	int 		error;

	error = dm_app_get_tdp(sid, hanp, hlen, token, DM_TDT_VNO,
		DM_RIGHT_EXCL, &tdp);
	if (error != 0)
		return(error);

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->remove_dmattr(tdp->td_bdp, tdp->td_right,
		setdtime, attrnamep);

	dm_app_put_tdp(tdp);
	return(error);
}


int
dm_set_dmattr(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrname_t	*attrnamep,
	int		setdtime,
	size_t		buflen,
	void		*bufp)
{
	dm_fsys_vector_t *fsys_vector;
	dm_tokdata_t	*tdp;
	int 		error;

	error = dm_app_get_tdp(sid, hanp, hlen, token, DM_TDT_VNO,
		DM_RIGHT_EXCL, &tdp);
	if (error != 0)
		return(error);

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->set_dmattr(tdp->td_bdp, tdp->td_right,
		attrnamep, setdtime, buflen, bufp);

	dm_app_put_tdp(tdp);
	return(error);
}


int
dm_set_inherit(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrname_t	*attrnamep,
	mode_t		mode)
{
	dm_fsys_vector_t *fsys_vector;
	dm_tokdata_t	*tdp;
	int 		error;

	error = dm_app_get_tdp(sid, hanp, hlen, token, DM_TDT_VFS,
		DM_RIGHT_EXCL, &tdp);
	if (error != 0)
		return(error);

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->set_inherit(tdp->td_bdp, tdp->td_right,
		attrnamep, mode);

	dm_app_put_tdp(tdp);
	return(error);
}
