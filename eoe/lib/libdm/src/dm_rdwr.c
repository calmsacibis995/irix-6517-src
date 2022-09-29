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
#ident "$Revision: 1.6 $"


#include <sys/dmi_kern.h>


extern dm_ssize_t
dm_read_invis(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_off_t	off,
	dm_size_t	len,
	void		*bufp)
{
	uint64_t	args[2];

	args[0] = (uint64_t)off;	/* don't pass 64 bit params in A regs */
	args[1] = (uint64_t)len;
	return dmi(DM_READ_INVIS, sid, hanp, hlen, token, args, bufp);
}

extern dm_ssize_t
dm_write_invis(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	int		flags,
	dm_off_t	off,
	dm_size_t	len,
	void		*bufp)
{
	uint64_t	args[2];

	args[0] = (uint64_t) off;	/* don't pass 64 bit params in A regs */
	args[1] = (uint64_t) len;
	return dmi(DM_WRITE_INVIS, sid, hanp, hlen, token, flags, args, bufp);
}

extern int
dm_sync_by_handle(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token)
{
	return dmi(DM_SYNC_BY_HANDLE, sid, hanp, hlen, token);
}


extern int
dm_get_dioinfo(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_dioinfo_t	*diop)
{
	return dmi(DM_GET_DIOINFO, sid, hanp, hlen, token, diop);
}
