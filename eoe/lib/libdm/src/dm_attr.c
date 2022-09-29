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
#ident "$Revision: 1.2 $"


#include <sys/dmi_kern.h>


extern int
dm_get_fileattr(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		mask,
	dm_stat_t	*statp)
{
	return dmi(DM_GET_FILEATTR, sid, hanp, hlen, token, mask, statp);
}


extern int
dm_set_fileattr(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		mask,
	dm_fileattr_t	*attrp)
{
	return dmi(DM_SET_FILEATTR, sid, hanp, hlen, token, mask, attrp);
}
