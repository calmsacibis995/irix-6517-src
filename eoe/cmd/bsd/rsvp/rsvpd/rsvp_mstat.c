
/*
 * @(#) $Id: rsvp_mstat.c,v 1.5 1998/12/09 19:22:33 michaelk Exp $
 */
/****************************************************************************

            RSVPD -- ReSerVation Protocol Daemon

                USC Information Sciences Institute
                Marina del Rey, California

		Original Version: Shai Herzog, Derek Yeung, Nov. 1993.
		Current Version:  Steven Berson & Bob Braden, May 1996

  Copyright (c) 1996 by the University of Southern California
  All rights reserved.

  Permission to use, copy, modify, and distribute this software and its
  documentation in source and binary forms for any purpose and without
  fee is hereby granted, provided that both the above copyright notice
  and this permission notice appear in all copies, and that any
  documentation, advertising materials, and other materials related to
  such distribution and use acknowledge that the software was developed
  in part by the University of Southern California, Information
  Sciences Institute.  The name of the University may not be used to
  endorse or promote products derived from this software without
  specific prior written permission.

  THE UNIVERSITY OF SOUTHERN CALIFORNIA makes no representations about
  the suitability of this software for any purpose.  THIS SOFTWARE IS
  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

  Other copyrights might apply to parts of this software and are so
  noted when applicable.

********************************************************************/

/*
 *    Routines to multicast state information to status display programs.
 *    (Actually, could configure to send to a unicast address).
 *
 *	Version 2 -- Handles variable-length objects, and tests for
 *		sessions disappearing to give GONE indications.
 */

#include <cap_net.h>
#include "rsvp_daemon.h"
#include "rsvp_api.h"
#include "rsvp_mstat.h"
#include "rapi_lib.h"

/* Global rsvpd control variables */

extern int	vif_num;
extern if_rec	*if_vec;
extern Session *session_hash[];
extern int	probe_socket;
extern int	mstat_ttl;

static int	statsock = -1;
static struct	sockaddr_in mcast_sin;
static char	RSVPNodeName[NAME_LEN];	/* name of the current router */
static char 	buff[2*STAT_MSG_SIZE];
static int	max_msg_size = STAT_MSG_SIZE;
static int	nSession, nStyle_Sess;
static int	IsDebugSess = 0;
static int	Send_Empty = 0;

Object_header   NULL_OBJECT = {4, 0, 0}; /* min length NULL object */

/* external declarations */
char	*strtcpy(char *, char *, int);

/* forward declarations */

static void	status_RSVP_Path(struct sockaddr_in *);
static void	status_RSVP_Resv(struct sockaddr_in *);
static char	*mstat_objcpy(Object_header *, Object_header *);
static void	status_send(char *, struct sockaddr_in *);
static PathInfo_t *status_Path_Session(Session *, PathInfo_t *);
static ResvInfo_t *fmt_WF_Resv(Session *, ResvInfo_t *);
static ResvInfo_t *fmt_FF_Resv(Session *, ResvInfo_t *);
static ResvInfo_t *fmt_SE_Resv(Session *, ResvInfo_t *);
static void	New_Resv_Session(Session *, ResvInfo_t *, int);
static void	Mark_Session(Session_IPv4 *, int);
static void	Gone_Session(int);

/*
 *  List of sessions.  Use this list to detect deletion of session state,
 *    so can explicitly notify display programs of session deletion.
 *    For each session entry, make simple little FSM, separately for
 *    Path and Resv state.
 */
#define MAX_SL	4
struct session_list {
	Session_IPv4	sl_dest;
	u_char		sl_fsms[2];	/* FSM states */
#define SL_FSM_PATH	0		/* Path state FSM */
#define SL_FSM_RESV	1		/* Resv state FSM */

#define SL_IDLE		0		/* Session_list entry unused */
#define SL_OLD		1		/* Session existed */
#define SL_ACTV		2		/* Session exists now */
	} Session_List[MAX_SL],
	 *Gone_Sessp, *Next_Sessp;


/*
 *	Initialize for sending status messages:
 *		* Socket address mcast_sin
 *		* UDP socket statsock
 *		* RSVPNodeName
 *		* Session list pointer Next_Sessp;
 */
int 
Status_Init(char *destaddr)
	{
	char            buf[256];
	struct in_addr	host;
	char		*tp;
	char		*rmsuffix(char *);
	u_char          ttlval = mstat_ttl;
	int             port = 0, one = 1;

	(void) strcpy(buf, destaddr);
	if ((tp = rmsuffix(buf)))
		port = hton16(atoi(tp));
	if ((host.s_addr = inet_addr(buf)) == -1) {
		fprintf(stderr, "Status addr not dotted dec.: %s\n", buf);
		return (-1);
	}
	Set_Sockaddr_in( &mcast_sin, host.s_addr, port);

	if ((statsock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		log(LOG_ERR, errno, "Socket Error\n");
		return(-1);
	}
	/* Allow socket to be reused */
	if (setsockopt(statsock, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof(one))) {
		log(LOG_ERR, errno, "Status SO_REUSE err\n");
		close(statsock);
		return(-1);
	}
	/* Set multicast TTL
	 */
	if (IN_MULTICAST(ntoh32(host.s_addr))) {
		if (0 > cap_setsockopt(statsock, IPPROTO_IP, IP_MULTICAST_TTL,
				   (char *) &ttlval, sizeof(ttlval))) {
			close(statsock);
			log(LOG_ERR, errno, "Status mcast TTL err\n");
			return(-1);
		}
	}

	/* Get my node DNS name, and remove all trailing components
	 */
	gethostname(RSVPNodeName, NAME_LEN);
	while (rmsuffix(RSVPNodeName)) {
	};
	/* Initialize session list pointer
	 */
	Next_Sessp = &Session_List[0];
	return (0);
}

/* init_probe_socket(): Called from main() during RSVP initialization.
 *	Initializes socket for receiving status probes.
 */
void
init_probe_socket()
	{
	struct	sockaddr_in sin;
	int	one = 1;

	/*
	 *	Create probe_socket, a UDP socket for receiving a state
	 *	probe packet.
	 */
	if ((probe_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		log(LOG_ERR, errno, "socket", 0);
		exit(-1);
	}
	if (setsockopt(probe_socket, SOL_SOCKET, SO_REUSEADDR, (char *) &one, 
							sizeof(one))) {
		log(LOG_ERR, errno, "Uisock REUSEADDR", 0);
		exit(-1);
	}		
	/*
	 *	Bind to probe port.
	 */
	Set_Sockaddr_in(&sin, INADDR_ANY, htons(STAT_PROBE_PORT));
	if (bind(probe_socket, (struct sockaddr *) &sin, sizeof(sin))) {
		log(LOG_ERR, errno, "bind", 0);
		exit(-1);
	}
}

/*
 * mcast_rsvp_state():  send Path State and Resv state information
 *	 to remote monitor programs (e.g., dmap or rsvpeep).
 *	 Called from dump_ds() in rsvp_debug.c.
 */
void 
mcast_rsvp_state()
	{
	if (statsock < -1)
		return;		/* ignore call */
	if (statsock < 0) {
		/*
		 * First call: Convert target address: <host>.<port>
		 * 	and set up socket address.  If it fails, set
		 *	statsock = -2 and ignore future calls.  Currently
		 *      <host> must be dotted decimal string.
		 */
		if (Status_Init(STAT_DEST_ADDR) < 0) {
			statsock = -2;
			return;
		}
	}
	/* Now format and send path and reservation state */
	Send_Empty = 0;
	status_RSVP_Path(&mcast_sin);
	status_RSVP_Resv(&mcast_sin);
}

void
send_rsvp_state(char *probe_buf, int probe_len, struct sockaddr_in *fromp)
	{
	/* Probe packet may generally contain list of hosts to be
	 * probed and list of sessions to be probed.  For now, assume
	 * only present host and all sessions.
	 */
	struct sockaddr_in to = *fromp;

	if (probe_len < sizeof(u_int32_t) ||
	    ntohl(*(u_int32_t *)probe_buf) != TYPE_RSVP_PROBE)
		return;

	if (statsock < -1)
		return;		/* ignore call */
	if (statsock < 0) {
		if (Status_Init(STAT_DEST_ADDR) < 0) {
			statsock = -2;
			return;
		}
	}
	/* Now format and send path and reservation state */
	Send_Empty = 1;
	status_RSVP_Path(&to);
	status_RSVP_Resv(&to);

}


/*
 * This procedure extracts the Path State information from rsvpd
 * data structure, marshals it, and sends it out.
 */
void
status_RSVP_Path(struct sockaddr_in *sinp)
	{
	RSVPInfo_t	*Infop = (RSVPInfo_t *) buff;
	PathInfo_t	*Pathp, *New_Pathp;
	Session   	*dst;
	int		 i;

	nSession = 0;
	Infop->type = hton32(TYPE_RSVP_VPATH);
	strtcpy(Infop->name, RSVPNodeName, NAME_LEN);
	Pathp = &Infop->Session_un.pathInfo;  /* first session */

	/*
	 * For each session ... 
	 */
	for (i= 0; i < SESS_HASH_SIZE; i++)
	    for (dst = session_hash[i]; dst; dst = dst->d_next) {
		/*
		 *   Call subroutine to move session data into packet.  Then,
		 *   if packet has overflown, write out data up through the
		 *   previous session, and copy new session back into first 
		 *   place.  Finally, count new session and record it in
		 *   cache.
		 */
		New_Pathp = status_Path_Session(dst, Pathp);
		if ((char *) New_Pathp > &buff[max_msg_size]) {
			int newlen = (char *)New_Pathp - (char *)Pathp;

			status_send((char *)Pathp, sinp);
			New_Pathp = &Infop->Session_un.pathInfo;
			memcpy(New_Pathp, Pathp, newlen);
			New_Pathp = (PathInfo_t *)
					((char *)New_Pathp + newlen);
			nSession = 0;
		}
		nSession++;
		Pathp = New_Pathp;
	}

	/*
	 * Scan my private Session_List, looking for sessions that had
	 *   path state last time but have none this time, and send an
	 *   "empty" entry for any that are found.
	 */
	Gone_Sessp = NULL;
	do {
		Gone_Session(SL_FSM_PATH);
		if (Gone_Sessp) {
			Pathp->path_session = Gone_Sessp->sl_dest;
			Pathp->path_R = 0;
			Pathp->nSender = 0;
			New_Pathp = (PathInfo_t *) &Pathp->path_sender;
			if ((char *) New_Pathp > &buff[max_msg_size]) {
				status_send((char *)Pathp, sinp);
				return;  /* truncate... */
			}
			nSession++;
			Pathp = New_Pathp;
		}
	} while (Gone_Sessp);

	/*
	 * Write last data, if any
	 */
	if ((nSession) || Send_Empty)
		status_send((char *) Pathp, sinp);
}

/*	Move path status of specified session into output packet at
 *	location Pathp, return pointer to next.  We assume an output
 *	buffer large enough to hold max message plus the additional
 *	session we will add here.
 */
static PathInfo_t *
status_Path_Session(Session *dstp, PathInfo_t *Pathp)
	{
	PathSender_t	*PathSp;
	PSB       	*snd;
	char        	*if_name, *tp;
	int		nSenders = 0;

	Pathp->path_session.sess_destaddr.s_addr = dstp->d_addr.s_addr;
	Pathp->path_session.sess_destport = dstp->d_port;
	Pathp->path_R = dstp->d_Rtimop;
	HTON32(Pathp->path_R);

	Mark_Session(&Pathp->path_session, SL_FSM_PATH);

	PathSp = &Pathp->path_sender;
	for (snd = dstp->d_PSB_list; snd != NULL; snd = snd->ps_next) {

		if_name = if_vec[snd->ps_in_if].if_name;
		strtcpy(PathSp->pathS_in_if, if_name, IFNAMSIZ);
		PathSp->pathS_phop.s_addr = snd->ps_phop.s_addr;
		PathSp->pathS_routes = snd->ps_outif_list;
		PathSp->pathS_ttd = snd->ps_ttd;

		HTON32(PathSp->pathS_routes);
		HTON32(PathSp->pathS_ttd);

		/****** NOT IN USE: LPM has this info, but not RSVP.*/
		tp = mstat_objcpy(Object_of(&PathSp->pathS_policy),
					    &NULL_OBJECT);
		tp = mstat_objcpy(Object_of(tp), Object_of(snd->ps_templ));
		tp = mstat_objcpy(Object_of(tp), Object_of(snd->ps_tspec));
 		PathSp = (PathSender_t *) tp;
		nSenders++;
		/**** CHECK FOR OVERFLOW ***/
	}
	Pathp->nSender = nSenders;
	HTON32(Pathp->nSender);
	return((PathInfo_t *) PathSp);
}



/*
 *   This procedure extracts reservation state information from rsvpd
 *   data structures, marshals it, and sends it in one or more UDP packets.
 */
void
status_RSVP_Resv(struct sockaddr_in *sinp)
{
	RSVPInfo_t	*Infop = (RSVPInfo_t *) buff;
	ResvInfo_t	*Resvp = (ResvInfo_t *) buff;
	ResvInfo_t	*New_Resvp;
	Session		*dst;
	int		i;

	nSession = 0;
	Infop->type = hton32(TYPE_RSVP_VRESV);
	strtcpy(Infop->name, RSVPNodeName, NAME_LEN);
	Resvp = &Infop->Session_un.resvInfo;  /* first session */

	/*
	 * For each session, marshal data for each style, if any.
	 */
	for (i= 0; i < SESS_HASH_SIZE; i++)
	    for (dst = session_hash[i]; dst; dst = dst->d_next) {
		nStyle_Sess = 0;
		New_Resvp = fmt_WF_Resv(dst, Resvp);
		New_Resvp = fmt_FF_Resv(dst, New_Resvp);
		New_Resvp = fmt_SE_Resv(dst, New_Resvp);

		if ((char *) New_Resvp > &buff[max_msg_size]) {
			int newlen = (char *)New_Resvp - (char *)Resvp;

			status_send((char *)Resvp, sinp);
			New_Resvp = &Infop->Session_un.resvInfo;
			memcpy(New_Resvp, Resvp, newlen);
			New_Resvp = (ResvInfo_t *)
				((char *)New_Resvp + newlen);
			nSession = 0;
		}
		nSession += nStyle_Sess;
		Resvp = New_Resvp;
	}
	/*
	 * Scan my private Session_List, looking for sessions that had
	 *   resv state last time but have none this time, and send an
	 *   "empty" entry for any that are found (but truncate if fill
	 *   buffer).
	 */
	Gone_Sessp = NULL;
	do {
		Gone_Session(SL_FSM_RESV);
		if (Gone_Sessp) {
			Resvp->resv_session = Gone_Sessp->sl_dest;
			Resvp->resv_style = 0;  /* => all styles! */
			Resvp->resv_R = 0;
			Resvp->nStruct = 0;
			New_Resvp = (ResvInfo_t *) 			
				mstat_objcpy(Object_of(&Resvp->resv_policy),
					    &NULL_OBJECT);
			if ((char *) New_Resvp > &buff[max_msg_size]) {
				status_send((char *)Resvp, sinp);
				return;  /* truncate... */
			}
			nSession++;
			Resvp = New_Resvp;
		}
	} while (Gone_Sessp);
	/*
	 * Write last data, if any
	 */
	if ((nSession) || Send_Empty)
		status_send((char *)Resvp, sinp); 
}

static ResvInfo_t *
fmt_WF_Resv(Session *dst, ResvInfo_t *Resvp)
	{
	ResvWF_t	*Rp;
	char		*if_name, *tp;
	int		nResv = 0;
	RSB		*rf;

	/* Compute offset into buffer, considering variable-length part
	 * (policy data) in 2nd level header.  Actually fill it in later
	 * if we marshal one or more 3rd level headers.
	 */
	Rp = (ResvWF_t *) mstat_objcpy(Object_of(&Resvp->resv_policy),
					    &NULL_OBJECT);
	/*
	 *   Scan sender list for wildcard sender, and for that sender,
	 *   the vector of outgoing interfaces, and marshal any
	 *   reservation state that is found; nResv is count.
	 */
	for (rf = dst->d_RSB_list; rf; rf = rf->rs_next) {
		if (rf->rs_style != STYLE_WF)
			continue;
		if (rf->rs_OIf == vif_num)
			if_name = LOCAL_HOST;
		else
			if_name = if_vec[vif_toif[rf->rs_OIf]].if_name;
		strtcpy(Rp->WF_if, if_name, IFNAMSIZ);
		Rp->WF_ttd = rf->rs_ttd;
		Rp->WF_nexthop = rf->rs_nhop;
		HTON32(Rp->WF_ttd);

		tp = mstat_objcpy(Object_of(&Rp->WF_flowspec), 
					Object_of(rf->rs_spec));
		Rp = (ResvWF_t *) mstat_objcpy(Object_of(tp),
					&NULL_OBJECT);
		/*** XXX CHECK OVERFLOW?? ***/
		nResv++;
	}
	/*   If we found any WF reservation state, set up 2nd level header
	 *   and advance output pointer.  If buffer full, send it.
	 */
	if (nResv) {
		nStyle_Sess++;
		New_Resv_Session(dst, Resvp, RAPI_RSTYLE_WILDCARD);
		Resvp->nStruct = nResv;
		HTON32(Resvp->nStruct);
		Resvp = (ResvInfo_t *) Rp;
	}
	return(Resvp);
}

static ResvInfo_t *
fmt_FF_Resv(Session *dst, ResvInfo_t *Resvp)
	{
	ResvFF_t	*Rp;
	int		nResv = 0;
	char		*if_name, *tp;
	RSB		*rf;

	Rp = (ResvFF_t *) mstat_objcpy(Object_of(&Resvp->resv_policy),
					    &NULL_OBJECT);

	for (rf = dst->d_RSB_list; rf; rf = rf->rs_next) {
		if (rf->rs_style != STYLE_FF)
			continue;
		if (rf->rs_OIf == vif_num)
			if_name = LOCAL_HOST;
		else
			if_name = if_vec[vif_toif[rf->rs_OIf]].if_name;

		strtcpy(Rp->FF_if, if_name, IFNAMSIZ);
		Rp->FF_ttd = rf->rs_ttd;
		Rp->FF_nexthop = rf->rs_nhop;
		HTON32(Rp->FF_ttd);
		tp = mstat_objcpy(Object_of(&Rp->FF_flowspec),
				Object_of(rf->rs_spec));
		Rp = (ResvFF_t *) mstat_objcpy(Object_of(tp),
				Object_of(rf->rs_filter0));
		nResv++;
			/* XXX CHECK FOR OVERFLOW */
	}
	/*   If we found any FF reservation state, set up 2nd level header
	 *   and advance output pointer.  If buffer full, send it.
	 */
	if (nResv) {
		nStyle_Sess++;
		New_Resv_Session(dst, Resvp, RAPI_RSTYLE_FIXED);
		Resvp->nStruct = nResv;
		HTON32(Resvp->nStruct);
		Resvp = (ResvInfo_t *) Rp;
	}
	return(Resvp);
}

static ResvInfo_t *
fmt_SE_Resv(Session *dst, ResvInfo_t *Resvp)
	{
	ResvSE_t	*Rp;
	int		nResv = 0, i;
	char		*if_name, *tp;
	RSB		*rf;

	Rp = (ResvSE_t *) mstat_objcpy(Object_of(&Resvp->resv_policy),
					    &NULL_OBJECT);

	for (rf = dst->d_RSB_list; rf; rf = rf->rs_next) {
		if (rf->rs_style != STYLE_SE)
			continue;
		if (rf->rs_OIf == vif_num)
			if_name = LOCAL_HOST;
		else
			if_name = if_vec[vif_toif[rf->rs_OIf]].if_name;

		strtcpy(Rp->SE_if, if_name, IFNAMSIZ);
		Rp->SE_ttd = rf->rs_ttd;
		Rp->SE_nexthop = rf->rs_nhop;
		HTON32(Rp->SE_ttd);
		tp = mstat_objcpy(Object_of(&Rp->SE_flowspec),
						Object_of(rf->rs_spec));
		Rp->nSE_filts = rf->rs_fcount;
		HTON32(Rp->nSE_filts);
		for (i = 0; i < rf->rs_fcount; i++) {
			tp = mstat_objcpy(Object_of(tp),
						Object_of(rf->rs_Filtp(i)));
		}
		Rp = (ResvSE_t *)tp;
		nResv++;
			/* CHECK FOR OVERFLOW */
	}
	/*   If we found any SE reservation state, set up 2nd level header
	 *   and advance output pointer.  If buffer full, send it.
	 */
	if (nResv) {
		nStyle_Sess++;
		New_Resv_Session(dst, Resvp, RAPI_RSTYLE_SE);
		Resvp->nStruct = nResv;
		HTON32(Resvp->nStruct);
		Resvp = (ResvInfo_t *) Rp;
	}
	return(Resvp);
}

static void
New_Resv_Session(Session *dst, ResvInfo_t *Resvp, int styleid)
	{
	Resvp->resv_session.sess_destaddr.s_addr = dst->d_addr.s_addr;
	Resvp->resv_session.sess_destport = dst->d_port;
	Mark_Session(&Resvp->resv_session, SL_FSM_RESV);

	Resvp->resv_style = styleid;
	HTON32(Resvp->resv_style);
	Resvp->resv_R = dst->d_Rtimor;
	HTON32(Resvp->resv_R);

	mstat_objcpy(Object_of(&Resvp->resv_policy), &NULL_OBJECT);
}



static void
status_send(char *bp, struct sockaddr_in *sinp)
	{
	RSVPInfo_t *Infop = (RSVPInfo_t *) buff;
	int	   len = bp - buff;
	
	Infop->nSession = nSession;
	HTON32(Infop->nSession);
	if (sendto(statsock, buff, len, 0, (struct sockaddr *) sinp,
					 sizeof(struct sockaddr_in)) < 0)
			log(LOG_WARNING, errno, "sendto\n");
	nSession = 0;
}


/*
 *  Copy variable-length RSVP object -- flowspec, filterspec, ... or NULL      
 */
static char *
mstat_objcpy(Object_header *dstp, Object_header *srcp)
{
	char *tp;

	if (srcp)
		memcpy(dstp, srcp, Obj_Length(srcp));
	else
		memcpy(dstp, &NULL_OBJECT, 4);
	tp = (char *)Next_Object(dstp);
#if BYTE_ORDER == LITTLE_ENDIAN
	hton_object(dstp);
#endif
	return(tp);
}	

/*
 *  Look up destination (session) [in network byte order] in local session  
 *  table, and mark it active.
 */
static void
Mark_Session(Session_IPv4 *sesp, int FSMid)
	{
	struct session_list *slp, *avail_slp = NULL;

	for (slp = Session_List; slp < Next_Sessp; slp++) {
		if (slp->sl_fsms[FSMid] == SL_IDLE) {
			if (slp->sl_fsms[SL_FSM_PATH] == SL_IDLE &&
			    slp->sl_fsms[SL_FSM_RESV] == SL_IDLE) {
				/* Entry not in use... clean it up */
				slp->sl_dest.sess_destaddr.s_addr = 0;
				slp->sl_dest.sess_destport = 0;
				avail_slp = slp;
			}
			continue;
		}
		if (slp->sl_dest.sess_destaddr.s_addr == 
						sesp->sess_destaddr.s_addr &&
		    slp->sl_dest.sess_destport == sesp->sess_destport) {
			slp->sl_fsms[FSMid] = SL_ACTV;
			if (IsDebugSess)
				fprintf(stderr, "%d ACTV %8.8X\n", FSMid, 
						(u_int) slp);
			return;
		}
	}
	/*
	 *   Session is new... add it to table.
	 */
	if (avail_slp == NULL) {
		/* Did not find an unused slot.  Extend table (if poss.) */
		if (Next_Sessp >= &Session_List[MAX_SL])
			return;  /* Table is full, ignore this session */
		avail_slp = Next_Sessp++;
	}
	if (IsDebugSess)
		fprintf(stderr, "%d NEW %8.8X <- ACTV\n", FSMid, 
						(u_int) avail_slp);
	avail_slp->sl_dest = *sesp;
	avail_slp->sl_fsms[FSMid] = SL_ACTV;		
}


/*
 *  Calling this procedure sets Gone_Sessp to point to an entry
 *  in the session_list that has been deleted, or to NULL if there
 *  are no more such entries.
 */
static void
Gone_Session(int FSMid)
	{
	if (Gone_Sessp == NULL)
		Gone_Sessp = &Session_List[0];
	else
		Gone_Sessp++;

	while (Gone_Sessp < Next_Sessp) {
		switch (Gone_Sessp->sl_fsms[FSMid]) {

		case SL_OLD:
			/*
			 *  Was there previously, but isn't now.  Change to
			 *  IDLE state and return to process send GONE status.
			 */
			Gone_Sessp->sl_fsms[FSMid] = SL_IDLE;
			if (IsDebugSess)
				fprintf(stderr,
				"%d GONE %8.8X <- IDLE\n",
					 FSMid, (u_int) Gone_Sessp);
			return;

		case SL_ACTV:
			if (IsDebugSess)
				fprintf(stderr, "%d UNMARK %8.8X <- OLD\n", 
					FSMid, (u_int) Gone_Sessp);
			Gone_Sessp->sl_fsms[FSMid] = SL_OLD;
			break;

		default:
			break;
		}
		Gone_Sessp++;
	}
	Gone_Sessp = NULL;
}

/*
 * Remove any final suffix .<chars> (if any) from the end of a given string
 * by storing a NULL on top of the dot.  Return a pointer to the beginning of
 * <chars>, or NULL if there is no suffix.  **ShOULD ALLOW ':'**
 */

char           *
rmsuffix(cp)
	char	*cp;
{
	char	*tp;

	tp = cp + strlen(cp) - 1;
	while (*tp != '.' && tp >= cp)
		tp--;
	if (*tp != '.')
		return (NULL);
	*tp = '\0';
	return ((char *) tp + 1);
}

