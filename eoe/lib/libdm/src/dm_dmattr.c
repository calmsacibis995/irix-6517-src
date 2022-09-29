/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.6 $"


#include <sys/dmi_kern.h>


extern int
dm_clear_inherit(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrname_t	*attrnamep)
{
	return dmi(DM_CLEAR_INHERIT, sid, hanp, hlen, token, attrnamep);
}


extern int
dm_get_dmattr (
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrname_t	*attrnamep,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp)
{
	uint64_t	args[2];

	args[0] = (uint64_t)bufp;
	args[1] = (uint64_t)rlenp;
	return dmi(DM_GET_DMATTR, sid, hanp, hlen, token, attrnamep,
		buflen, args);
}


extern int
dm_getall_dmattr (
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp)
{
	return dmi(DM_GETALL_DMATTR, sid, hanp, hlen, token, buflen,
		bufp, rlenp);
}


extern int
dm_getall_inherit(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		nelem,
	dm_inherit_t	*inheritbufp,
	u_int		*nelemp)
{
	return dmi(DM_GETALL_INHERIT, sid, hanp, hlen, token, nelem,
		inheritbufp, nelemp);
}


extern int
dm_remove_dmattr (
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	int		setdtime,
	dm_attrname_t	*attrnamep)
{
	return dmi(DM_REMOVE_DMATTR, sid, hanp, hlen, token, setdtime,
		attrnamep);
}


extern int
dm_set_dmattr (
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrname_t	*attrnamep,
	int		setdtime,
	size_t		buflen,
	void		*bufp)
{
	uint64_t	args[2];

	args[0] = (uint64_t)buflen;
	args[1] = (uint64_t)bufp;
	return dmi(DM_SET_DMATTR, sid, hanp, hlen, token, attrnamep,
		setdtime, args);
}


extern int
dm_set_inherit(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrname_t	*attrnamep,
	mode_t		mode)
{
	return dmi(DM_SET_INHERIT, sid, hanp, hlen, token, attrnamep, mode);
}


extern int
dm_set_return_on_destroy (
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrname_t	*attrnamep,
	dm_boolean_t	enable)
{
	return dmi(DM_SET_RETURN_ON_DESTROY, sid, hanp, hlen, token,
		attrnamep, enable);
}
