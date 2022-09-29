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


#include <sys/dmi_kern.h>


extern int
dm_downgrade_right(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token)
{
	return dmi(DM_DOWNGRADE_RIGHT, sid, hanp, hlen, token);
}


extern int
dm_obj_ref_hold(
	dm_sessid_t	sid,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen)
{
	return dmi(DM_OBJ_REF_HOLD, sid, token, hanp, hlen);
}


extern int
dm_obj_ref_query(
	dm_sessid_t	sid,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen)
{
	return dmi(DM_OBJ_REF_QUERY, sid, token, hanp, hlen);
}


extern int
dm_obj_ref_rele(
	dm_sessid_t	sid,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen)
{
	return dmi(DM_OBJ_REF_RELE, sid, token, hanp, hlen);
}


extern int
dm_query_right(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_right_t	*rightp)
{
	return dmi(DM_QUERY_RIGHT, sid, hanp, hlen, token, rightp);
}


extern int
dm_release_right(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token)
{
	return dmi(DM_RELEASE_RIGHT, sid, hanp, hlen, token);
}


extern int
dm_request_right(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		flags,
	dm_right_t	right)
{
	return dmi(DM_REQUEST_RIGHT, sid, hanp, hlen, token, flags, right);
}


extern int
dm_upgrade_right(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token)
{
	return dmi(DM_UPGRADE_RIGHT, sid, hanp, hlen, token);
}
