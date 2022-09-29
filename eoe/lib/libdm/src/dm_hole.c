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
#ident "$Revision: 1.9 $"


#include <sys/dmi_kern.h>


extern int
dm_get_allocinfo (
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_off_t	*offp,
	u_int		nelem,
	dm_extent_t	*extentp,
	u_int		*nelemp)
{
	uint64_t	args[2];

	args[0] = (uint64_t) extentp;
	args[1] = (uint64_t) nelemp;
	return dmi(DM_GET_ALLOCINFO, sid, hanp, hlen, token, offp, nelem,
		args);
}

extern int
dm_probe_hole(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_off_t	off,
	dm_size_t	len,
	dm_off_t	*roffp,
	dm_size_t	*rlenp)
{
	uint64_t	args[2];

	args[0] = (uint64_t)off;	/* don't pass 64-bit parms in A regs */
	args[1] = (uint64_t)len;
	return dmi(DM_PROBE_HOLE, sid, hanp, hlen, token, args, roffp, rlenp);
}

extern int
dm_punch_hole(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_off_t	off,
	dm_size_t	len)
{
	uint64_t	args[2];

	args[0] = (uint64_t)off;	/* don't pass 64-bit parms in A regs */
	args[1] = (uint64_t)len;
	return dmi(DM_PUNCH_HOLE, sid, hanp, hlen, token, args);
}
