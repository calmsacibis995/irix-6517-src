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


extern int
dm_init_service(
	char	**versionstrpp)
{
	*versionstrpp = DM_VER_STR_CONTENTS;
	return(0);
}

extern int
dm_create_session(
	dm_sessid_t	oldsid,
	char		*sessinfop,
	dm_sessid_t	*newsidp)
{
	return dmi(DM_CREATE_SESSION, oldsid, sessinfop, newsidp);
}

extern int
dm_destroy_session(
	dm_sessid_t	sid)
{
	return dmi(DM_DESTROY_SESSION, sid);
}

extern int
dm_getall_sessions(
	u_int		nelem,
	dm_sessid_t	*sidbufp,
	u_int		*nelemp)
{
	return dmi(DM_GETALL_SESSIONS, nelem, sidbufp, nelemp);
}

extern int
dm_query_session(
	dm_sessid_t	sid,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp)
{
	return dmi(DM_QUERY_SESSION, sid, buflen, bufp, rlenp);
}

extern int
dm_getall_tokens(
	dm_sessid_t	sid,
	u_int		nelem,
	dm_token_t	*tokenbufp,
	u_int		*nelemp)
{
	return dmi(DM_GETALL_TOKENS, sid, nelem, tokenbufp, nelemp);
}

extern int
dm_find_eventmsg(
	dm_sessid_t	sid,
	dm_token_t	token,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp)
{
	return dmi(DM_FIND_EVENTMSG, sid, token, buflen, bufp, rlenp);
}
