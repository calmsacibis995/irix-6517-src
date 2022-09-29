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
#ident "$Revision: 1.8 $"


#include <sys/dmi_kern.h>


extern int
dm_get_events(
	dm_sessid_t	sid,
	u_int		maxmsgs,
	u_int		flags,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp)
{
	return dmi(DM_GET_EVENTS, sid, maxmsgs, flags, buflen, bufp, rlenp);
}

extern int
dm_respond_event(
	dm_sessid_t	sid,
	dm_token_t	token,
	dm_response_t	response,
	int		reterror,
	size_t		buflen,
	void		*respbufp)
{
	return dmi(DM_RESPOND_EVENT, sid, token, response, reterror,
			buflen, respbufp);
}


extern int
dm_get_eventlist(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		nelem,
	dm_eventset_t	*eventsetp,
	u_int		*nelemp)
{
	return dmi(DM_GET_EVENTLIST, sid, hanp, hlen, token, nelem,
			eventsetp, nelemp);
}

extern int
dm_set_eventlist(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_eventset_t	*eventsetp,
	u_int		maxevent)
{
	return dmi(DM_SET_EVENTLIST, sid, hanp, hlen, token,
			eventsetp, maxevent);
}

extern int
dm_set_disp(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_eventset_t	*eventsetp,
	u_int		maxevent)
{
	return dmi(DM_SET_DISP, sid, hanp, hlen, token, eventsetp, maxevent);
}

extern int
dm_create_userevent(
	dm_sessid_t	sid,
	size_t		msglen,
	void		*msgdatap,
	dm_token_t	*tokenp)
{
	return dmi(DM_CREATE_USEREVENT, sid, msglen, msgdatap, tokenp);
}

extern int
dm_send_msg(
	dm_sessid_t	targetsid,
	dm_msgtype_t	msgtype,
	size_t		buflen,
	void		*bufp)
{
	return dmi(DM_SEND_MSG, targetsid, msgtype, buflen, bufp);
}

extern int
dm_move_event(
	dm_sessid_t	srcsid,
	dm_token_t	token,
	dm_sessid_t	targetsid,
	dm_token_t	*rtokenp)
{
	return dmi(DM_MOVE_EVENT, srcsid, token, targetsid, rtokenp);
}

extern int
dm_pending(
	dm_sessid_t	sid,
	dm_token_t	token,
	dm_timestruct_t	*delay)
{
	return dmi(DM_PENDING, sid, token, delay);
}

extern int
dm_getall_disp(
	dm_sessid_t	sid,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp)
{
	return dmi(DM_GETALL_DISP, sid, buflen, bufp, rlenp);
}
