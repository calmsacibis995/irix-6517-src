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
#ident "$Revision: 1.5 $"


#include <sys/dmi_kern.h>


extern int
dm_init_attrloc(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrloc_t	*locp)
{
	return dmi(DM_INIT_ATTRLOC, sid, hanp, hlen, token, locp);
}

extern int
dm_get_bulkattr(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		mask,
	dm_attrloc_t	*locp,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp)
{
	uint64_t	args[3];

	args[0] = (uint64_t) buflen;
	args[1] = (uint64_t) bufp;
	args[2] = (uint64_t) rlenp;
	return dmi(DM_GET_BULKATTR, sid, hanp, hlen, token, mask, locp, args);
}

extern int
dm_get_dirattrs(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		mask,
	dm_attrloc_t	*locp,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp)
{
	uint64_t	args[3];

	args[0] = (uint64_t) buflen;
	args[1] = (uint64_t) bufp;
	args[2] = (uint64_t) rlenp;
	return dmi(DM_GET_DIRATTRS, sid, hanp, hlen, token, mask, locp, args);
}

extern int
dm_get_bulkall(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		mask,
	dm_attrname_t	*attrnamep,
	dm_attrloc_t	*locp,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp)
{
	uint64_t	args[4];

	args[0] = (uint64_t) locp;
	args[1] = (uint64_t) buflen;
	args[2] = (uint64_t) bufp;
	args[3] = (uint64_t) rlenp;
	return dmi(DM_GET_BULKALL, sid, hanp, hlen, token, mask,
			attrnamep, args);
}
