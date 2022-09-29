/*
 * @(#) $Id: rapi_lib.c,v 1.13 1998/11/25 08:43:36 eddiem Exp $
 */

/****************************************************************************

            RSVPD -- ReSerVation Protocol Daemon

                USC Information Sciences Institute
                Marina del Rey, California

		Original Version: Don Hoffman, Sun Microsystems
		Current Version:  Bob Braden, May 1996.

  Copyright (c) 1996 by the University of Southern California
  All rights reserved.

  Permission to use, copy, modify, and distribute this software and its
  documentation in source and binary forms for any purpose and without
  fee is hereby granted, provided that both the above copyright notice
  and this permission notice appear in all copies. and that any
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

/*  History:
 *   Oct 94  Vers 1: Original code written by Don Hoffman @ Sun Microsystems
 *   ??? 95  Vers 2: Updated by Bob Braden @ ISI
 *   Aug 95  Vers 3: Update client calls to match ID07 protocol spec.
 *   Sep 95  Vers 3.10: Add new calls rapi_session(), rapi_sender() as
 *			alternative to rapi_register().  Also, included
			improvements developed by Joshua Gahm (JBG) at BBN.
 *   Sep 95  Vers 3.11: Include adspecs in upcalls.
 *   Apr 96  Vers 4.0: Remove non-reentrant rapi_errno variable.
 *			Remove obs parameters and add some new ones.
 *			Allow null rapi_sender call to imply path_tear.
 */

/* get around multiply defined rsvp_socket in rsvp_global.h - mwang */
#define __STATIC_RSVP_SOCKET__

char *librsvp_info = "SGI librsvp for IRIX 6.5 based on ISI rel4.1a6";

#include "rsvp_daemon.h"
#include "rsvp_api.h"
#include "rsvp_specs.h"
#include <sys/uio.h>


/* Serial numbers to use in calls to connect_to_daemon to 
   distinguish UNIX filenames for the pipes.  NB: can only 
   have one outstanding DEBUG connection to the server at a 
   time, so not safe for multi-threaded use. */ 
 
#define MAIN_RSVP_CONNECTION_SERIAL_NUM 0 
#define DEBUG_RSVP_CONNECTION_SERIAL_NUM 1 

/*
 *	Define an entry in client session table.
 */
typedef struct Sid_info {
	struct sockaddr_in	dest;	/* Destination addr, port  */
	u_char			protid; /* Protocol Id		*/
	u_char			in_use;	/* in-use flag 		*/
	u_char			flags;  /* Session flags	*/
	u_char			rflags; /* Reserve flags	*/
	rapi_event_rtn_t	event_rtn;
	void			*event_rtn_arg;
}               sid_rec;

/*
 *	Client session table;   index into this table is
 *	the session handle given to application.
 */
static sid_rec	sid_vec[MAX_RAPI_SESS];

/*
 *	Global variables
 */
static int	rapi_errno = RAPI_ERR_OK;
static int	pid, init_flag = 0;
static int	max_sid = 0;	/* (To shorten linear search time) */
char		client_namestr[70]; /* Name of client send of socket */

/*	Define empty RAPI and API objects: just header (framing)
 */
int		Empty_RObj[2] = {sizeof(rapi_hdr_t), 0};
#define Empty_APIObj	Empty_RObj		/* empty API object */
#define Empty_Flowspec (rapi_flowspec_t *) &Empty_RObj
#define Empty_Filter (rapi_filter_t *) &Empty_RObj
	
struct sockaddr_in	Wild_socket;

/*	JBG: the single socket that will be connected to the 
 *	RSVP server.  All individual sids will have their socket  
 *	fields set to this value when they are in use. 
 */ 
static int              rsvp_socket = -1; 
static int              rsvp_socket_refs = 0; 
 
#define mark_sid_inuse(sid_to_mark) \
if (! sid_vec[sid_to_mark].in_use) \
     rsvp_socket_refs++; \
sid_vec[sid_to_mark].in_use = 1; 
 
#define mark_sid_free(sid_to_mark) \
if (sid_vec[sid_to_mark].in_use) \
     rsvp_socket_refs--; \
sid_vec[sid_to_mark].in_use = 0; 

/*
 *	Forward declarations
 */
static rapi_sid_t
		common_register(rapi_sid_t, struct sockaddr *, int,
		   rapi_filter_t *, rapi_tspec_t *,
		   rapi_adspec_t *, rapi_policy_t *, int);
static int	rapi_dispatch_fd(int);
static int	Get_free_slot();
static void	sig_pipe(int sig);
static int	init_rapi();
static int	connect_to_daemon(int);
static int	send_req(rsvp_req *, int, long, int);
static char	*copy_policy_i2d(rapi_policy_t *, rapi_policy_t *, char *, int);
static char	*copy_flowspec_i2d(rapi_flowspec_t *, API_Flowspec *, char *);
static char	*copy_tspec_i2d(rapi_tspec_t *, API_TSpec *, char *);
static char	*copy_filterspec_i2d(rapi_filter_t *, API_FilterSpec *, char *);
static int	copy_flowspec_d2i(API_Flowspec *, rapi_flowspec_t *, char *, 
							int);
static int	copy_tspec_d2i(API_TSpec *, rapi_tspec_t *, char *, int);
static int	copy_filterspec_d2i(API_FilterSpec *, rapi_filter_t *, char *);
static int	copy_adspec_d2i(API_Adspec *, rapi_adspec_t *, char *, int);
static char	*copy_sender_desc(rapi_filter_t *, rapi_tspec_t *, 
							API_FlowDesc *, char *);
void		sockaddr2filterbase(struct sockaddr *, rapi_filter_t *);
#ifdef OBSOLETE_API
int		CSZtoCL_spec(qos_flowspec_t *, IS_specbody_t *);
int		CSZtoG_spec(qos_flowspec_t *, IS_specbody_t *);
int		CSZtoCL_tspec(qos_flowspec_t *, IS_tspbody_t *);
#endif
int		CSZXtoG_spec(qos_flowspecx_t *, IS_specbody_t *);
int		CSZXtoGen_tspec(qos_tspecx_t *, IS_tspbody_t *);
int		CSZXtoCL_spec(qos_flowspecx_t *, IS_specbody_t *);
int		CLtoCSZX_spec(IS_specbody_t *, qos_flowspecx_t *);
int		GtoCSZX_spec(IS_specbody_t *, qos_flowspecx_t *);
int		GentoCSZX_tspec(IS_tspbody_t *, qos_tspecx_t *);
int		IStoCSZX_adspec(IS_adsbody_t *, qos_adspecx_t *);
/* this is already defined in uio.h - mwang */
/* int		writev(); */
int		List_Length(char *, int);

#define is_valid(sid) (init_flag && sid <= max_sid && (sid_vec[sid].in_use))
#define IS2RAPI_len(x) (4*(x) + sizeof(rapi_hdr_t) + sizeof(IS_main_hdr_t))
#define RAPI2IS_len(y) wordsof((y)- sizeof(rapi_hdr_t) - sizeof(IS_main_hdr_t))
#define API_IsPath(x)  ((x)->resp_type == RAPI_PATH_EVENT || \
		        (x)->resp_type == RAPI_PATH_ERROR || \
		        (x)->resp_type == RAPI_PATH_STATUS)


/**********************************************************
 *       	Visible RAPI Routines
 *
 **********************************************************/

char       *rapi_rstyle_names[] = {"?", "WF", "FF", "SE"};

/*
 * Data is passed across the API interface in the format used
 * internally by the RSVP daemon. Currently this means that
 * IP addresses and ports are passed in network byte order (including
 * those embedded in filter specs and sender templates). All other
 * data is passed in the host's natural format.
 */

/*  rapi_session():
 *	Register with the RSVP daemon, creating a new API session.
 *	If successful, return local session id, else return -1 and
 *	set error code in variable.
 */
rapi_sid_t
rapi_session(
	struct sockaddr		*dest,	/* Destination host, port */
	int			protid,
	int			flags,
	rapi_event_rtn_t	event_rtn,
	void *			event_rtn_arg,
	int			*errnop)
	{
	int			rc;
	rapi_sid_t		sid = NULL_SID;

	rapi_errno = RAPI_ERR_OK;
	if (dest == NULL) {
		rapi_errno = RAPI_ERR_INVAL;
		goto exit;
	}
	if (!init_flag) {
		if (init_rapi() < 0) {
			rapi_errno = RAPI_ERR_NORSVP;
			goto exit;
		}
	}
	/* Locate empty session slot */
	if ((int)(sid = Get_free_slot()) < 0) {
		rapi_errno = RAPI_ERR_MAXSESS;
		goto exit;
	}
	mark_sid_inuse(sid);		/* Mark it in use */

	sid_vec[sid].protid = protid;
	sid_vec[sid].flags = (flags &= ~(API_DIR_SEND|API_DIR_RECV));
	sid_vec[sid].dest = * (struct sockaddr_in *) dest;		
	sid_vec[sid].event_rtn = event_rtn;
	sid_vec[sid].event_rtn_arg = event_rtn_arg;
	
	rc = common_register(sid, NULL, flags, NULL, NULL, NULL, NULL, 0);
	if (rc != RAPI_ERR_OK) {
		mark_sid_free(sid);
		sid = NULL_SID;
	}
exit:
	if (errnop)
		*errnop = rapi_errno;
	return sid;
}


/*  rapi_sender():
 *	Set/modify sender parameters.
 */
int
rapi_sender(
	rapi_sid_t		sid,
	int			flags,	/* Currently, none used */
	struct sockaddr 	*LHost,	/* Sender host, port (optional) */
	rapi_filter_t *		sender_template,
	rapi_tspec_t  *		sender_tspec,
	rapi_adspec_t *		sender_adspec,
	rapi_policy_t *		sender_policy,
	int			ttl)
	{

	rapi_errno = RAPI_ERR_OK;
	if (!is_valid(sid))
		return RAPI_ERR_BADSID;
	if (!sender_tspec && LHost)    /* Tspec not required when LHost=NULL */
		return RAPI_ERR_NOTSPEC;

	sid_vec[sid].flags |= flags & ~(API_DIR_SEND|API_DIR_RECV);
	/* Assumes session and sender flags are distinct. */

	if (LHost)
		sid_vec[sid].flags |= API_DIR_SEND;
	else
		sid_vec[sid].flags &= ~API_DIR_SEND;
		/* Sender = NULL => undefine sender
		 */
	common_register(sid, LHost, sid_vec[sid].flags,
			sender_template, sender_tspec,
			sender_adspec, sender_policy, ttl);
	return (rapi_errno);
}


/*  rapi_reserve():
 *	Make/modify/delete reservation for session.
 *
 */
int
rapi_reserve(
	rapi_sid_t		sid,
	int			rflags,
	struct sockaddr *	rhost,	/* Receiver host (optional) */
	rapi_styleid_t		styleid,
	rapi_stylex_t	*	style_ext,   /* style extension */
        rapi_policy_t	*	rcvr_policy,
	int			n_filter,
	rapi_filter_t	*	filter_spec_list,
	int			n_flow,
	rapi_flowspec_t	*	flowspec_list)
	{
	char			*req_buf = NULL, *cp, *End_buf;
	rsvp_req		*req = (rsvp_req *) req_buf;
	int			i, len, n1, n2, nflwd;
	rapi_filter_t		*rfiltp;
	rapi_flowspec_t		*rflowp;

	rapi_errno = RAPI_ERR_OK;
	if (!is_valid(sid))
		return RAPI_ERR_BADSID;

	/*	Check parameters according to style
	 */
	rfiltp = filter_spec_list;
	rflowp = flowspec_list;
	nflwd = 0;

	switch (styleid) {

	    case RAPI_RSTYLE_WILDCARD:
		if (n_flow == 0)
			break;	/* Teardown case */
		if (n_filter < 0 || n_filter > 1 || n_flow != 1)
			return RAPI_ERR_N_FFS;
		if (rflowp == NULL)
			return RAPI_ERR_INVAL;
		nflwd = 1;
		break;

	    case RAPI_RSTYLE_FIXED:
		if (n_flow == 0)
			break;	/* Teardown case */
		if (n_filter != n_flow || n_filter <= 0)
			return RAPI_ERR_N_FFS;
		if (rfiltp == NULL || rflowp == NULL)
			return RAPI_ERR_INVAL;
		nflwd = n_filter;
		break;

	    case RAPI_RSTYLE_SE:
		if (n_flow == 0)
			break;	/* Teardown case */
		if (n_flow != 1 || n_filter <= 0) {
			return RAPI_ERR_N_FFS;
		}
		if (rfiltp == NULL || rflowp == NULL)
			return RAPI_ERR_INVAL;
		nflwd = n_filter;
		break;

	    default:
		return RAPI_ERR_BADSTYLE;
	}
	/*	Must scan parm lists to get total length in order to
	 *	dynamically alloc buffer.
	 */
	if ((n1 = List_Length( (char *)rflowp, n_flow)) < 0 ||
	    (n2 = List_Length( (char *)rfiltp, n_filter)) < 0) {
		return RAPI_ERR_INVAL;
	}
	else len = sizeof(rsvp_req) + n1 + n2 + 
				abs(n_flow - n_filter)*sizeof( Empty_RObj );
 
	if (!(req_buf = malloc(len)))
		return RAPI_ERR_MEMFULL;

	req = (rsvp_req *) req_buf;
	End_buf = req_buf + len;
	memset((char *)req, 0, len);

	req->rq_style = (u_char) styleid;
	req->rq_type = API2_RESV;
	req->rq_dest = sid_vec[sid].dest;
	req->rq_protid = sid_vec[sid].protid;
	if (rhost)
		req->rq_host = *(struct sockaddr_in *)rhost;
	else
		req->rq_host = Wild_socket;
	req->rq_nflwd = nflwd;
	req->rq_flags = rflags;

	cp = copy_policy_i2d(rcvr_policy, req->rq_policy, End_buf, 1);
	if (!cp)
		goto exit;

	/*  If n_flow == 0, simply remove existing reservation (without
	 *  releasing API session).
	 */
	sid_vec[sid].flags |= API_DIR_RECV;
	if (n_flow == 0) {
		req->rq_nflwd = 0;
		sid_vec[sid].flags &= ~API_DIR_RECV;
		(void)send_req(req, cp - req_buf, sid, rsvp_socket);
		goto exit;
	}

	/*
	 *	Copy ({flowspec|EMPTY}, {filter_spec|EMPTY}) pairs
	 *	into request.  Note that copy_xxxx_i2d() routines
	 *	check that there is room in buffer and if not, set
	 *	rapi_errno and return NULL.
	 *
	 *	copy_flowspec_i2d translates contents into Intserv format.
	 *	copy_filterspec_i2d (currently) simply copies without change.
	 */
	for (i = 0; i < req->rq_nflwd; i++) {
		if (i < n_filter && sid_vec[sid].flags&RAPI_GPI_SESSION) {
			if (rfiltp->form != RAPI_FILTERFORM_GPI 
			 && rfiltp->form != RAPI_FILTERFORM_GPI6) {
				rapi_errno = RAPI_ERR_GPISESS;
				goto exit;
			}
		}
		cp = (char *)copy_filterspec_i2d(
				(i < n_filter)? rfiltp : Empty_Filter,
				(API_FilterSpec *)cp, End_buf);
		if (!cp)
			goto exit;
		cp = copy_flowspec_i2d((i < n_flow)? rflowp : Empty_Flowspec,
				(API_Flowspec *)cp, End_buf);
		if (!cp)
			goto exit;
		if (i < n_filter)
			rfiltp = (rapi_filter_t *) After_RAPIObj(rfiltp);
		if (i < n_flow)
			rflowp = (rapi_flowspec_t *) After_RAPIObj(rflowp);
	}
	(void)send_req(req, cp - req_buf, sid, rsvp_socket);

exit:
	if (req_buf)
		free(req_buf);
	return rapi_errno;
}

/*  rapi_release():
 *	Delete API session and free session handle.
 */
int
rapi_release(rapi_sid_t sid)
	{
	char		req_buf[MAX_MSG];	/* TBD: dyn alloc */
	rsvp_req       *req = (rsvp_req *) req_buf;

	if (!is_valid(sid))
		return RAPI_ERR_BADSID;

	memset(req_buf, 0, sizeof req_buf);	/* Redundant, but cautious */
	req->rq_nflwd = 0;

	req->rq_type = API_CLOSE;
	(void)send_req(req, sizeof(rsvp_req), sid, rsvp_socket);
	mark_sid_free(sid);
	return (RAPI_ERR_OK);
}

/*  rapi_getfd():
 *	Return file descriptor for Unix socket.
 */
int
rapi_getfd(rapi_sid_t sid)
	{
	if (!is_valid(sid))
		return -1;
	return rsvp_socket;
}

/*  rapi_dispatch():
 *	Dispatch upcalls.
 */
int
rapi_dispatch()
	{
	int     rc = 0;
    	fd_set  tmp_fds;
	int     n, fd_wid;
	struct  timeval tout;

	rapi_errno = RAPI_ERR_OK;

	FD_ZERO(&tmp_fds);
	memset(&tout, 0, sizeof(tout));

	FD_SET(rsvp_socket, &tmp_fds);
	fd_wid = rsvp_socket+1;

	/* Poll for input ready on Unix socket
	 */
	n = select(fd_wid, &tmp_fds, NULL, NULL, &tout);

	if (FD_ISSET(rsvp_socket, &tmp_fds))  {
		rc = rapi_dispatch_fd(rsvp_socket);
		return rc;
	}

	return RAPI_ERR_OK;     /* do-nothing successful return */
}

int
rapi_version()
	{
	return RAPI_VERSION;
}

#ifdef ISI_TEST
/*  rapi_status():
 *	Trigger Path Event and/or Resv Event upcalls for specified sid,
 *	or, if sid = 0, all sid's in this process.
 */
int
rapi_status(
	rapi_sid_t sid,
	int	flags)		/* Direction flags */
	{
	char		req_buf[MAX_MSG];
	rsvp_req	*req = (rsvp_req *) req_buf;

	rapi_errno = RAPI_ERR_OK;

	if (sid && !is_valid(sid)) {
		return RAPI_ERR_BADSID;
	}
	memset((char *)req, 0, sizeof(req_buf));
	req->rq_type = API2_STAT;
	if (sid)
		req->rq_dest = sid_vec[sid].dest; /* already in host BO */
	req->rq_flags = flags;

	(void)send_req(req, sizeof(rsvp_req), sid, rsvp_socket);
	return (rapi_errno);	
}
#endif /* ISI_TEST */


/*  The following routine is not part of the official RAPI interface.
 *	It is a management interface to rsvpd.
 */
rapi_sid_t
rapi_rsvp_cmd(rapi_cmd_t *cmd, int *errnop)
	{
	char		*req_buf;
	rsvp_req	*req;
	int             lsid = NULL_SID;
	int             old = 0;
	char		*cp;
	int		tmp_socket = -1;  
				/* temporary socket to */ 
				/* be used in sending debug cmd */ 
 	int		n;

	rapi_errno = RAPI_ERR_OK;

	if (init_flag == 0) {
		if (init_rapi()<0) {
			rapi_errno = RAPI_ERR_NORSVP;
			goto exit;
		}
	}
	n = sizeof(int)*cmd->rapi_cmd_len;

	if (!(req_buf = malloc(n+sizeof(rsvp_req)))) {
		rapi_errno = RAPI_ERR_MEMFULL;
		goto exit;
	}

	/* Use any free sid entry
	 */ 
	if ((lsid = Get_free_slot()) < 0) {
		rapi_errno = RAPI_ERR_MAXSESS;
		goto exit;
	}
	mark_sid_inuse(lsid);

	req  = (rsvp_req *) req_buf;
	req->rq_type = API_DEBUG;
	cp = req_buf + sizeof(rsvp_req);
	memcpy(cp, cmd, n);
	cp += n;
	
	/* Grab a temporary socket to connect to the daemon so that 
	 * we can safely expect a reply to our request as the next 
	 * thing sent along on the socket. 
	 */
	tmp_socket = connect_to_daemon(DEBUG_RSVP_CONNECTION_SERIAL_NUM); 

        if (tmp_socket < 0) {
		rapi_errno = RAPI_ERR_NORSVP;
	}
	else {
		send_req(req, cp-req_buf, lsid, tmp_socket);
		close(tmp_socket);  /* get rid of the tmp socket */
	}
	mark_sid_free(lsid);

exit:
	if (errnop)
		*errnop = rapi_errno;	
	if (req_buf)
		free(req_buf);
	return old;
}


/**********************************************************
 *       Initialization and I/O Routines
 *
 **********************************************************/


/* Common routine to format a REGISTER request and send it to
 *	the daemon.  Returns rapi_errno.
 */
rapi_sid_t
common_register(
	rapi_sid_t		sid,
	struct sockaddr		*LHost,	/* Source host, port (optional) */
	int			flags,  /* Session and sender flags */
	rapi_filter_t *		sender_template,
	rapi_tspec_t  *		sender_tspec,
	rapi_adspec_t *		sender_adspec,
	rapi_policy_t *		sender_policy,
	int			ttl)
	{
	char			req_buf[API_REGIS_BUF_LEN];
	char			*End_buf = &req_buf[API_REGIS_BUF_LEN];
	rsvp_req		*req = (rsvp_req *) req_buf;
	char 			*cp;
	rapi_filter_t		fl, *flp;

	/*
	 *  Build REGISTER request and send to daemon
	 */
	memset((char *)req, 0, API_REGIS_BUF_LEN);
	req->rq_type = API2_REGISTER;
	req->rq_dest = sid_vec[sid].dest;
	req->rq_protid = sid_vec[sid].protid;
	req->rq_nflwd = 0;
	req->rq_flags = flags;
	req->rq_ttl = ttl;

	if (flags & API_DIR_SEND) {
		/*	rapi_sender call.
		 */
		cp = copy_policy_i2d(sender_policy, req->rq_policy, End_buf, 0);
		if (!cp)
			return rapi_errno;
		/*
		 *	If sender_template is NULL, use LHost.
		 */
		if (sender_template == NULL) {
			if (flags&RAPI_GPI_SESSION)
				return RAPI_ERR_GPISESS;
			sockaddr2filterbase(LHost, flp = &fl);
		} else {
			if (flags&RAPI_GPI_SESSION) {
			    if (sender_template->form != RAPI_FILTERFORM_GPI 
			     && sender_template->form != RAPI_FILTERFORM_GPI6)
				return RAPI_ERR_GPISESS;
			flp = sender_template;
			}
		}		

		cp = (char *) copy_sender_desc(flp,
				 sender_tspec, (API_FlowDesc *) cp, End_buf);
		if (!cp)
			return rapi_errno;

		req->rq_nflwd = 1;
	} else {
		cp = copy_policy_i2d(NULL, req->rq_policy, End_buf, 0);
		if (!cp)
			return rapi_errno;
	}

	/* XXX No code here yet to pass adspec */

	send_req(req, cp-req_buf, sid, rsvp_socket);
	return(rapi_errno);
}

/*	Find and return index of free slot in table, or return -1 if
 *	table is full.  If succeeds, clears slot to zero.
 */
int
Get_free_slot()
	{
	int sid;

	/* To avoid ambiguity, we avoid using sid = zero.
	 */
	for (sid = 1; sid <= max_sid ; sid++)
		if (!sid_vec[sid].in_use)
			return(sid);
	if (sid < MAX_RAPI_SESS) {
		max_sid = sid;
		memset((char *)&sid_vec[sid], 0, sizeof(sid_rec));
		return(sid);
	}
	return(-1);	/* Table full */
}


static void
sig_pipe(int sig)
	{
	/*
	 * This has to be fixed.  A SIGPIPE typically means the server
	 * dropped the connection.  The client either needs to die, or to try
	 * to reestablish the connection.
	 */
	/* 
	 * TBD:  call client handler with appropriate error 
	 */
	fprintf(stderr, "Got a SIGPIPE\n");
	signal(SIGPIPE, sig_pipe);
}

static int
init_rapi()
	{

	pid = getpid();
	signal(SIGPIPE, sig_pipe);
	(void)memset((char *) sid_vec, 0, sizeof(sid_vec));
	init_flag = 1;
	rapi_errno = RAPI_ERR_OK;
	rsvp_socket_refs = 0;
	/*	Create wildcard socket INADDR_ANY:0
	 */
	memset(&Wild_socket, 0, sizeof(struct sockaddr_in));
	Wild_socket.sin_family = AF_INET;
#ifdef SOCKADDR_LEN
	Wild_socket.sin_len = sizeof(struct sockaddr_in);
#endif
	Wild_socket.sin_addr.s_addr = INADDR_ANY;

	/*	Open a single Unix pipe that this process will use 
	 *	to talk to the rsvp daemon.
	 */ 
	rsvp_socket = connect_to_daemon(MAIN_RSVP_CONNECTION_SERIAL_NUM); 
	if (rsvp_socket < 0) {
		fprintf(stderr, "Could not connect to rsvp server\n"); 
		return -1;
	}
	return 0;
}

static int
connect_to_daemon(int serial_no)
	{
	int             sock;
	struct sockaddr_un name, server;
	int             addr_len;

	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror("opening socket");
		return (-1);
	}
	sprintf(client_namestr, "%s.%u.%u", LNAME, pid, serial_no);
	unlink(client_namestr);
	name.sun_family = AF_UNIX;
	strcpy(name.sun_path, client_namestr);
#ifdef STANDARD_C_LIBRARY
	addr_len = (offsetof(struct sockaddr_un, sun_path)
		    + strlen(name.sun_path));
#else
	addr_len = sizeof(name.sun_family) + strlen(name.sun_path);
#endif
#ifdef SOCKADDR_LEN
	name.sun_len = addr_len;
#endif
	if (bind(sock, (struct sockaddr *) & name, addr_len) < 0) {
		perror("bind");
		goto BadOpen;
	}
	server.sun_family = AF_UNIX;
	strcpy(server.sun_path, SNAME);
#ifdef STANDARD_C_LIBRARY
	addr_len = (offsetof(struct sockaddr_un, sun_path)
		    + strlen(server.sun_path));
#else
	addr_len = sizeof(server.sun_family) + strlen(server.sun_path);
#endif
#if BSD >= 199103
	server.sun_len = addr_len;
#endif
	if (connect(sock, (struct sockaddr *) &server, sizeof(struct sockaddr_un)) >= 0) {
		unlink(client_namestr);
		return (sock);
	}
	perror("connect");
BadOpen:
	close(sock);
	unlink(client_namestr);
	return (-1);
}


/*	Send request to daemon.  Send 4 bytes containing length of
 *	req structure, followed by req structure itself.
 *
 *	JBG: added sock as an argument to the following so that the 
 *	debug interface can open its own temporary socket and use it. 
 */ 
static int
send_req(rsvp_req * req, int len, long sid, int sock)
	{
	struct iovec iov[2];

	req->rq_pid = pid;
	req->rq_a_sid = sid;
	req->rq_version = VERSION_API;

	if (sock < 0) {
		rapi_errno = RAPI_ERR_NORSVP;
		return (-1);
	}
	iov[0].iov_base = (char *) (&len); 
	iov[0].iov_len  = sizeof(int); 
	iov[1].iov_base = (char *) req; 
	iov[1].iov_len  = len;   
      
	if (writev(sock, iov, 2) == -1)  {
		rapi_errno = RAPI_ERR_SYSCALL;
		return(-1);
	}
	return(0);
}

int
rapi_dispatch_fd(int fd_val)
	{
	char		*resp_buf = NULL;
	rsvp_resp	*resp;
	int		i, len, n, nr, size;
	int		n_filter, n_flowspec, n_adspec;
	rapi_filter_t	*api_filter = NULL, *tapi_filt;
	rapi_flowspec_t *api_flowspec = NULL, *tapi_spec;
	rapi_adspec_t	*api_adspec = NULL, *tapi_adspp;
	char		*eapi_filt, *eapi_spec, *eapi_adspp;
	API_FilterSpec	*fltr;
	API_Flowspec	*flow;
	API_Adspec	*adspp;
	sid_rec		*sidp;

	rapi_errno = RAPI_ERR_OK;
	
	/*	Response is sent as a 4-byte length field followed by
	 *	that many bytes of response data.  Read length and
	 *	then malloc buffer.
	 */
	n = read(fd_val, &len, sizeof(int));
	if (n == 0) {	/* EOF => RSVP daemon is gone... ! */
		rapi_errno = RAPI_ERR_NORSVP;
		goto err_exit;
	}
	else if (n < sizeof(int)) {
		rapi_errno = RAPI_ERR_SYSCALL;
		goto err_exit;	/* some error in the mechanism */
	}
	if (!(resp_buf = malloc(len))) {
		rapi_errno = RAPI_ERR_MEMFULL;
		goto err_exit;
	}
	resp = (rsvp_resp *)resp_buf;
	nr = len;
	while (nr) {
		n = read(fd_val, resp_buf, nr);
		if (n <= 0) {
			rapi_errno = RAPI_ERR_NORSVP;
			goto err_exit;
		}
		nr -= n;
	}
	sidp = &sid_vec[resp->resp_a_sid];
			/* XXX should validate a_sid */

	if (resp->resp_nflwd == 0) {
		/* Special case: nflwd == 0; means teardown of state.
		 * Invoke the client's upcall routine
		 */
		if (sidp->event_rtn)
	  	  (*(sidp->event_rtn)) (
			(rapi_sid_t) resp->resp_a_sid,
			(rapi_eventinfo_t) resp->resp_type,
			resp->resp_style,
 			resp->resp_errcode, resp->resp_errval,
			(struct sockaddr *) &resp->resp_errnode, 
			resp->resp_errflags,
			0, NULL, 0, NULL, 0, NULL,
			sidp->event_rtn_arg
	   	 );
		goto err_exit;
	}
	/*	Malloc space for filtspec, flowspec, adspec lists for app
	 *
	 *	NB:  allocating space based on worst case sizes.
	 *      XXX Could 'parse' vbl-length info to determine actual sizes
	 */

	size = sizeof(rapi_filter_t) * resp->resp_nflwd;
	if ((api_filter = malloc(size)) == NULL) {
		rapi_errno = RAPI_ERR_MEMFULL;
		goto err_exit;
	}
	eapi_filt = (char *)api_filter + size;
	size = sizeof(rapi_flowspec_t) * resp->resp_nflwd;
	if ((api_flowspec = malloc(size)) == NULL) {
		rapi_errno = RAPI_ERR_MEMFULL;
		goto err_exit;
	}
	eapi_spec = (char *)api_flowspec + size;

	/*  Copy list of (filterspec, flowspec) pairs into 2 areas
	 */
	tapi_filt = api_filter;
	tapi_spec = api_flowspec;
	n_filter = n_flowspec = n_adspec = 0;

	fltr = (API_FilterSpec *) resp->resp_flows;
	for (i = 0; i < resp->resp_nflwd; i++) {
		copy_filterspec_d2i(fltr, tapi_filt, eapi_filt);
		tapi_filt = (rapi_filter_t *) After_RAPIObj(tapi_filt);
		n_filter++;
		flow = (API_Flowspec *) After_APIObj(fltr);

		if (API_IsPath(resp)) {
			copy_tspec_d2i((API_TSpec*)flow,
					(rapi_tspec_t *)tapi_spec, eapi_spec,
					sidp->flags&RAPI_USE_INTSERV);
			tapi_spec= (rapi_flowspec_t *) After_RAPIObj(tapi_spec);
			n_flowspec++;
		}
		else if (n_flowspec < 1) {
			copy_flowspec_d2i(flow, tapi_spec, eapi_spec,
					sidp->flags&RAPI_USE_INTSERV);
			tapi_spec = (rapi_flowspec_t *) After_RAPIObj(tapi_spec);
			n_flowspec++;
		}			
		fltr = (API_FilterSpec *) After_APIObj(flow);
	}
	if (API_IsPath(resp) && (char *)fltr - resp_buf < len){
		/*
		 *	For path event, list of adspecs follows sender
		 *	descriptor list.  (We make sure there really is
		 *	an adspec list, for backwards compatibility).
		 */
		size = (MAX_ADSPEC_LEN+sizeof(rapi_hdr_t)) * resp->resp_nflwd;

		api_adspec = malloc(size);
		if (api_adspec == NULL) {
			rapi_errno = RAPI_ERR_MEMFULL;
			goto err_exit;
		}
		eapi_adspp = (char *)api_adspec + size;
		tapi_adspp = api_adspec;
		adspp = (rapi_adspec_t *) fltr;
		for (i = 0; i < resp->resp_nflwd; i++) {
			copy_adspec_d2i(adspp, tapi_adspp, eapi_adspp,
					sidp->flags&RAPI_USE_INTSERV);
			if (adspp->form != RAPI_EMPTY_OTYPE)
				n_adspec++; /* Count non-empties */
			adspp = (API_Adspec *)After_APIObj(adspp);
			tapi_adspp = (rapi_adspec_t *) 
						After_RAPIObj(tapi_adspp);
		}
		/* If list contained only empty objects, pass 0 for n_adspec;
		 * otherwise, pass resp_nflwd.
		 */
		if (n_adspec)
			n_adspec = resp->resp_nflwd;
	}
			
	if (n_filter == 0) {
		free(api_filter);
		api_filter = NULL;
	}
	if (n_flowspec == 0) {
		free(api_flowspec);
		api_flowspec = NULL;
	}
	if (n_adspec == 0) {
		if (api_adspec)
			free(api_adspec);
		api_adspec = NULL;
	}

	/* Invoke the client's upcall routine
	 */
	if (sidp->event_rtn)
	    (*(sidp->event_rtn)) (
		(rapi_sid_t) resp->resp_a_sid,
		(rapi_eventinfo_t) resp->resp_type,
		resp->resp_style,
 		resp->resp_errcode, resp->resp_errval,
		(struct sockaddr *)&resp->resp_errnode,
		resp->resp_errflags,
		n_filter, api_filter,
		n_flowspec, api_flowspec,
		n_adspec, api_adspec,
		sidp->event_rtn_arg
	    );
	/*
	 *	Upon return, free malloc'd areas
	 */
err_exit:
	if (api_filter)
		free(api_filter);
	if (api_flowspec)
		free(api_flowspec);
	if (api_adspec)
		free(api_adspec);
	if (resp_buf)
		free(resp_buf);
	return (rapi_errno);
}


/*	Scan a given list of N RAPI objects, and compute upper bound on
 *	length after translation to API objects, for buffer allocation.
 */
int
List_Length(char *lcp, int N)
	{
	int		i;
	int		len = 0;

	for (i= 0; i < N; i++) {
		switch (((rapi_hdr_t *)lcp)->form) {

		    case RAPI_EMPTY_OTYPE:
			len += sizeof(Empty_RObj);
			break;

#ifdef OBSOLETE_API
		    case RAPI_FILTERFORM_1:
			len += sizeof(rapi_filter_t);
			break;
#endif
		    case RAPI_FILTERFORM_BASE:
			len += sizeof(rapi_hdr_t) + sizeof(struct sockaddr_in);
			break;

#ifdef OBSOLETE_API
		    case RAPI_FLOWSTYPE_CSZ:	/* qos: Legacy format */
#endif
		    case RAPI_FLOWSTYPE_Simplified:
			/* Use max Intserv flowspec size */
			len += sizeof(rapi_hdr_t) + sizeof(IS_specbody_t);
			break;

		    case RAPI_FLOWSTYPE_Intserv: /* IS: The Real Thing */
			/* Use actual Intserv flowspec size */
			len += IS2RAPI_len(
			    ((rapi_flowspec_t *)lcp)->specbody_IS.ISmh_len32b);
			break;

		    default:
			/* SHOULD NOT HAPPEN */
			return -1;

		}
		lcp = After_RAPIObj(lcp);
	}
	return len;
}


char *
copy_sender_desc(
	rapi_filter_t	*	i_filter,
	rapi_tspec_t 	*	i_tspec,
	API_FlowDesc	* 	flwdp,
	char		*	endp)
	{
	char		*cp;

	if (!i_tspec) i_tspec = (rapi_tspec_t *) &Empty_APIObj;
	cp = copy_filterspec_i2d(i_filter, (API_FilterSpec *) flwdp, endp);
	if (!cp)
		return NULL;
	cp = copy_tspec_i2d(i_tspec, (API_TSpec *)cp, endp);
	return cp;
}

void
sockaddr2filterbase(struct sockaddr *sockp, rapi_filter_t *filtp)
	{
	filtp->len = sizeof(rapi_hdr_t) + sizeof(rapi_filter_base_t);
	filtp->form = RAPI_FILTERFORM_BASE;
	filtp->filt_u.base.sender = *(struct sockaddr_in *)sockp;
}


/**********************************************************************
 *       COPY-AND-TRANSLATE ROUTINES
 *
 *	For flowspecs and adspecs, these routines may do serious translation,
 *	since the API protocol assumes Intserv data structure bodies.
 *	For filterspecs, these routines (currently) simply pass through the
 *	RAPI/API object.
 *
 **********************************************************************/

/*	Translate flowspec from user interface format to daemon format,
 *	checking type, length, and for overflow of req message.
 *	Return ptr to next d_spec locn, or NULL if error.
 */
static char *
copy_flowspec_i2d(rapi_flowspec_t * i_spec, API_Flowspec * d_spec, char *endp)
	{
	if ((char *)d_spec + RAPIObj_Size(i_spec) > endp) {
		rapi_errno = RAPI_ERR_OVERFLOW;
		return NULL;
	}
	switch (i_spec->form) {

#ifdef OBSOLETE_API
	    case RAPI_FLOWSTYPE_CSZ:
		/*
		 * Translate legacy flowspec into Intserv Controlled-Load
		 * or Guaranteed service, supplying worst-case values for 
		 * parameters that were not supplied: p= Inf, m= 0, M= 32K.
		 *   Old Guaranteed -> New Guaranteed with S= 0, p= Inf
		 *   Old Predictive -> Controlled Load
		 *   Old Controlled Delay -> Controlled Load
		 */
		if (RAPIObj_Size(i_spec) < 
				sizeof(qos_flowspec_t)+sizeof(rapi_hdr_t)) {
			rapi_errno = RAPI_ERR_OBJLEN;
			return NULL;
		}
		switch (i_spec->specbody_qos.spec_type) {

		case QOS_GUARANTEED:
		    CSZtoG_spec(&i_spec->specbody_qos, &d_spec->specbody_IS);
		    break;

		case QOS_CNTR_DELAY:
		case QOS_PREDICTIVE:
		    CSZtoCL_spec(&i_spec->specbody_qos, &d_spec->specbody_IS);
		    break;

		default:
	    	    rapi_errno = RAPI_ERR_INVAL;
		    return NULL;
		}
		d_spec->form = RAPI_FLOWSTYPE_Intserv;
		d_spec->len = IS2RAPI_len(d_spec->specbody_IS.ISmh_len32b);
		break;
#endif /* OBSOLETE_API */

	    case RAPI_FLOWSTYPE_Simplified:
		/*
		 * Simplifed: translate into proper Intserv
		 * format.  In this case, don't have to supply any values.
		 */
		if (RAPIObj_Size(i_spec) < 
				sizeof(qos_flowspecx_t)+sizeof(rapi_hdr_t)) {
			rapi_errno = RAPI_ERR_OBJLEN;
			return NULL;
		}
		switch (i_spec->specbody_qosx.spec_type) {

		case QOS_GUARANTEEDX:
		    CSZXtoG_spec(&i_spec->specbody_qosx, 
						&d_spec->specbody_IS);
		    break;

        	case QOS_CNTR_LOAD:
		    CSZXtoCL_spec(&i_spec->specbody_qosx, 
						&d_spec->specbody_IS);
		    break;

		default:
		    rapi_errno = RAPI_ERR_INVAL;
		    return NULL;
		}
		d_spec->form = RAPI_FLOWSTYPE_Intserv;
		d_spec->len = IS2RAPI_len(d_spec->specbody_IS.ISmh_len32b);
		break;

	    case RAPI_FLOWSTYPE_Intserv:
		/*	Set length in Intserv flowspec header
		 */
		i_spec->specbody_IS.ISmh_len32b = 
					RAPI2IS_len(RAPIObj_Size(i_spec));
		memcpy((char *) d_spec, (char *) i_spec, RAPIObj_Size(i_spec));
		break;

	    case RAPI_EMPTY_OTYPE:
		memcpy((char *)d_spec, (char *)&Empty_RObj, sizeof(Empty_RObj));
		break;

	    default:
		rapi_errno = RAPI_ERR_OBJTYPE;
		return NULL;
	}
	return ( After_RAPIObj(d_spec));
}

/*	Translate Tspec from user interface format to daemon format,
 *	checking type, length, and for overflow of req message.
 *	Return ptr to next d_spec locn, or NULL if error.
 */
static char *
copy_tspec_i2d(rapi_tspec_t * i_spec, API_TSpec * d_spec, char *endp)
	{
/* this is already defined in stdio.h - mwang */
/*	int printf(); */

	if ((char *)d_spec + RAPIObj_Size(i_spec) > endp) {
		rapi_errno = RAPI_ERR_OVERFLOW;
		return NULL;
	}
	switch (i_spec->form) {

#ifdef OBSOLETE_API
	    case RAPI_FLOWSTYPE_CSZ:
		/*
		 * Translate legacy Tspec into Intserv Controlled-Load
		 * Tspec, supplying worst-case values for parameters
		 * that were not supplied: m = 0, M = 32K.
		 */
		if (RAPIObj_Size(i_spec) < 
				sizeof(qos_flowspec_t)+sizeof(rapi_hdr_t)) {
			rapi_errno = RAPI_ERR_OBJLEN;
			return NULL;
		}
		switch (i_spec->tspecbody_qos.spec_type) {

		case QOS_TSPEC:
		case QOS_GUARANTEED:
		case QOS_CNTR_DELAY:
		case QOS_PREDICTIVE:
		    CSZtoCL_tspec(&i_spec->tspecbody_qos, &d_spec->tspecbody_IS);
		    break;

		default:
	    	    rapi_errno = RAPI_ERR_INVAL;
		    return NULL;
		}
		d_spec->form = RAPI_TSPECTYPE_Intserv;
		d_spec->len = IS2RAPI_len(d_spec->tspecbody_IS.IStmh_len32b);
		break;
#endif /* OBSOLETE_API */

	    case RAPI_FLOWSTYPE_Simplified:
		printf("Obsolete Tspec form: should be RAPI_TSPECTYPE_Simplified\n");

	    case RAPI_TSPECTYPE_Simplified:
		/*
		 * Simplified format: translate into proper Intserv
		 * format.  In this case, don't have to supply any values.
		 */
		if (RAPIObj_Size(i_spec) < 
				sizeof(qos_tspecx_t)+sizeof(rapi_hdr_t)) {
			rapi_errno = RAPI_ERR_OBJLEN;
			return NULL;
		}
		switch (i_spec->tspecbody_qosx.spec_type) {

		case QOS_GUARANTEEDX:
        	case QOS_CNTR_LOAD:
		case QOS_TSPECX:
			/* -> Generic Tspec */
		    CSZXtoGen_tspec(&i_spec->tspecbody_qosx, 
						&d_spec->tspecbody_IS);
		    break;

		default:
		    rapi_errno = RAPI_ERR_INVAL;
		    return NULL;
		}
		d_spec->form = RAPI_TSPECTYPE_Intserv;
		d_spec->len =IS2RAPI_len(d_spec->tspecbody_IS.IStmh_len32b);
		break;

	    case RAPI_TSPECTYPE_Intserv:
		/*	Set length in Intserv flowspec header
		 */
		i_spec->tspecbody_IS.IStmh_len32b = wordsof(RAPIObj_Size(i_spec)
			- sizeof(rapi_hdr_t) - sizeof(IS_main_hdr_t));
		memcpy((char *) d_spec, (char *) i_spec, RAPIObj_Size(i_spec));
		break;

	    case RAPI_EMPTY_OTYPE:
		memcpy((char *)d_spec, (char *)&Empty_RObj, sizeof(Empty_RObj));
		break;

	    default:
		rapi_errno = RAPI_ERR_OBJTYPE;
		return NULL;
	}
	return ( After_RAPIObj(d_spec));
}

/*
 *	Copy and convert RAPI-format filter spec into daemon format.
 */
static char *
copy_filterspec_i2d(rapi_filter_t * i_filter, API_FilterSpec * d_filter,
								char *endp)
	{
	switch (i_filter->form) {

#ifdef OBSOLETE_API
	    case RAPI_FILTERFORM_1:
		/* We don't support general (mask, value) strings, so their
		 * length must be zero.
		 */
		if (RAPIObj_Size(i_filter) != (sizeof(struct sockaddr_in) +
				sizeof(int) + sizeof(rapi_hdr_t))){
			return 0;
		}
	 	break;
#endif

	    case RAPI_FILTERFORM_BASE:
		if (RAPIObj_Size(i_filter) !=
			   sizeof(rapi_filter_base_t)+sizeof(rapi_hdr_t)) {
			rapi_errno = RAPI_ERR_OBJLEN;
			return 0;
		}
		break;

	    case RAPI_FILTERFORM_GPI:
		if (RAPIObj_Size(i_filter) !=
			   sizeof(rapi_filter_gpi_t)+sizeof(rapi_hdr_t)) {
			rapi_errno = RAPI_ERR_OBJLEN;
			return 0;
		}
		break;

	    case RAPI_EMPTY_OTYPE:
		if (RAPIObj_Size(i_filter) != sizeof(rapi_hdr_t)) {
			rapi_errno = RAPI_ERR_OBJLEN;
			return 0;
		}
		break;

	    default:
	    case RAPI_FILTERFORM_BASE6:
	    case RAPI_FILTERFORM_GPI6:
		rapi_errno = RAPI_ERR_OBJTYPE;
		return 0;
	}		
	if ((char *)d_filter + RAPIObj_Size(i_filter) > endp) {
		rapi_errno = RAPI_ERR_OVERFLOW;
		return 0;
	}
	memcpy((char *) d_filter, (char *) i_filter, RAPIObj_Size(i_filter));
	return ( After_RAPIObj(d_filter));
}

/*	Translate from daemon format to user interface format, checking
 *	for overflow of upcall area.
 *		XXX For present, use Simplified-format format only
 *		
 */
static int
copy_flowspec_d2i(API_Flowspec * d_spec, rapi_flowspec_t * i_spec, char *endp,
							int Use_Intserv)
	{
	IS_serv_hdr_t	*isshp = (IS_serv_hdr_t *) &d_spec->specbody_IS.spec_u;
	int		 rapilen;

	rapilen = (Use_Intserv)? RAPIObj_Size(i_spec) :
			sizeof(qos_flowspecx_t) + sizeof(rapi_hdr_t);
	if ((char *)i_spec + rapilen > endp) {
		rapi_errno = RAPI_ERR_OVERFLOW;
		return 0;
	}
	i_spec->len = rapilen;

	if (Use_Intserv) {
		/*	RAPI format is int-serv body with RAPI framing,
		 *	that is, we can just copy RAPI object without change.
		 */
		memcpy((char *) d_spec, (char *) i_spec, rapilen);
		return 1;
	}
	i_spec->form = RAPI_FLOWSTYPE_Simplified;
	switch (isshp->issh_service) {

	    case CONTROLLED_LOAD_SERV:
		CLtoCSZX_spec(&d_spec->specbody_IS, &i_spec->specbody_qosx);
		return 1;

	    case GUARANTEED_SERV:
		GtoCSZX_spec(&d_spec->specbody_IS, &i_spec->specbody_qosx);
		return 1;

	    default:
		rapi_errno = RAPI_ERR_INTSERV;
		return 0;
	}
}

static int
copy_tspec_d2i(API_TSpec * d_spec, rapi_tspec_t * i_spec, char *endp,
							int Use_Intserv)
	{
	IS_serv_hdr_t	*isshp = (IS_serv_hdr_t *)&d_spec->tspecbody_IS.tspec_u;
	int	rapilen;

	rapilen = (Use_Intserv)? RAPIObj_Size(i_spec) :
			sizeof(qos_tspecx_t) + sizeof(rapi_hdr_t);
	if ((char *)i_spec + rapilen > endp) {
		rapi_errno = RAPI_ERR_OVERFLOW;
		return 0;
	}
	i_spec->len = rapilen;
	if (Use_Intserv) {
		memcpy((char *) d_spec, (char *) i_spec, rapilen);
		return 1;
	}
	i_spec->form = RAPI_TSPECTYPE_Simplified;
	switch (isshp->issh_service) {

	    case GENERAL_INFO:
		GentoCSZX_tspec(&d_spec->tspecbody_IS, &i_spec->tspecbody_qosx);
		return 1;

	    default:
		rapi_errno = RAPI_ERR_INTSERV;
		return 0;
	}
}

static int
copy_adspec_d2i(API_Adspec * d_adspp, rapi_adspec_t * i_adspp, char *endp,
							int Use_Intserv)
	{
	int	rapilen;

	rapilen = (Use_Intserv)? RAPIObj_Size(i_adspp) :
			sizeof(qos_adspecx_t) + sizeof(rapi_hdr_t);
	if ((char *)i_adspp + rapilen > endp) {
		rapi_errno = RAPI_ERR_OVERFLOW;
		return 0;
	}
	i_adspp->len = rapilen;
	if (Use_Intserv) {
		memcpy((char *) i_adspp, (char *) d_adspp, rapilen);
		return 1;
	}
	i_adspp->form = RAPI_ADSTYPE_Simplified;
	IStoCSZX_adspec(&d_adspp->adspecbody_IS, &i_adspp->adspecbody_qosx);
	return 1;
}


static int
copy_filterspec_d2i(API_FilterSpec * d_filter, rapi_filter_t * i_filter,
							char *endp)
	{
	if ((char *)i_filter + RAPIObj_Size(d_filter) > endp) {
		rapi_errno = RAPI_ERR_OVERFLOW;
		return 0;
	}
	memcpy((char *) i_filter, (char *) d_filter, RAPIObj_Size(d_filter));
	return 1;
}

static char *
copy_policy_i2d(
        rapi_policy_t   *i_pol,
        rapi_policy_t   *o_pol,
	char		*endp,
	int 		 policy)
	{

	i_pol = (rapi_policy_t *) &Empty_RObj;  /*** ALWAYS EMPTY ! ***/
	if ((char *)o_pol + RAPIObj_Size(i_pol) > endp) {
		rapi_errno = RAPI_ERR_OVERFLOW;
		return NULL;
	}
	memcpy((char *) o_pol, (char *)i_pol, RAPIObj_Size(i_pol));
	return ( After_APIObj(o_pol));
}


#ifdef OBSOLETE_API
/*	Translate legacy flowspec into Controlled Load Intserv flowspec
 */
int
CSZtoCL_spec(qos_flowspec_t *cszp, IS_specbody_t *ISp)
	{
	qos_flowspecx_t		Xspec, *csxp = &Xspec;

	csxp->xspec_r = cszp->spec_r/8;	/* b/sec -> B/sec */
	csxp->xspec_b = cszp->spec_b/8;	/* b -> B */
	csxp->xspec_p = INFINITY32f;
	csxp->xspec_m = 0;
	csxp->xspec_M = 32000;
	CSZXtoCL_spec(csxp, ISp);
	return(0);
}

/*	Translate legacy Tspec into Generic Intserv Tspec
 */
int
CSZtoCL_tspec(qos_flowspec_t *cszp, IS_tspbody_t *IStp)
	{
	qos_tspecx_t	Xspec, *ctxp = &Xspec;

	ctxp->xtspec_r = cszp->spec_r/8;	/* b/sec -> B/sec */
	ctxp->xtspec_b = cszp->spec_b/8;	/* b -> B */
	ctxp->xtspec_p = INFINITY32f;
	ctxp->xtspec_m = 0;
	ctxp->xtspec_M = 32000;
	CSZXtoGen_tspec(ctxp, IStp);
	return(0);
}


/*	Translate legacy GUARANTEED spec into Guaranteed Intserv flowspec
 */
int
CSZtoG_spec(qos_flowspec_t *cszp, IS_specbody_t *ISp)
	{
	qos_flowspecx_t		Xspec, *csxp = &Xspec;

	csxp->xspec_R = cszp->spec_r/8;
	csxp->xspec_S = 0;
	csxp->xspec_r = cszp->spec_r/8;	/* b/sec -> B/sec */
	csxp->xspec_b = cszp->spec_b/8;	/* b -> B */
	csxp->xspec_p = INFINITY32f;
	csxp->xspec_m = 0;
	csxp->xspec_M = 32000;
	CSZXtoG_spec(csxp, ISp);
	return(0);
}
#endif /* OBSOLETE_API */


/*	Build (Intserv) Guaranteed Flowspec from Simplified-format flowspec
 */
int
CSZXtoG_spec(qos_flowspecx_t *csxp, IS_specbody_t *ISp)
	{
	Guar_flowspec_t *gp = &ISp->spec_u.G_spec;

	Set_Main_Hdr(&ISp->spec_mh, sizeof(Guar_flowspec_t));
	Set_Serv_Hdr(&gp->Guar_serv_hdr, GUARANTEED_SERV, Gspec_len);
	Set_Parm_Hdr(&gp->Guar_Tspec_hdr, IS_WKP_TB_TSPEC,
							sizeof(TB_Tsp_parms_t));
	gp->Gspec_r = csxp->xspec_r;
	gp->Gspec_b = csxp->xspec_b;
	gp->Gspec_p = csxp->xspec_p;
	gp->Gspec_m = csxp->xspec_m;
	gp->Gspec_M = csxp->xspec_M;

	Set_Parm_Hdr(&gp->Guar_Rspec_hdr, IS_GUAR_RSPEC, sizeof(guar_Rspec_t));
	gp->Gspec_R = csxp->xspec_R;
	gp->Gspec_S = csxp->xspec_S;
	return(0);
}


/*	Build (Intserv) Generic Tspec from Simplified-format Tspec
 */
int
CSZXtoGen_tspec(qos_tspecx_t *csxtp, IS_tspbody_t *IStp)
	{
	gen_Tspec_t *gtp = &IStp->tspec_u.gen_stspec;

	Set_Main_Hdr(&IStp->st_mh, sizeof(gen_Tspec_t));
	Set_Serv_Hdr(&gtp->gen_Tspec_serv_hdr, GENERAL_INFO, gtspec_len);
	Set_Parm_Hdr(&gtp->gen_Tspec_parm_hdr, IS_WKP_TB_TSPEC,
						sizeof(TB_Tsp_parms_t));
	gtp->gtspec_r = csxtp->xtspec_r;
	gtp->gtspec_b = csxtp->xtspec_b;
	gtp->gtspec_p = csxtp->xtspec_p;
	gtp->gtspec_m = csxtp->xtspec_m;
	gtp->gtspec_M = csxtp->xtspec_M;
	return(0);
}

/*	Build (Intserv) Controlled-Load Flowspec from Simplified-format Flowspec
 */
int
CSZXtoCL_spec(qos_flowspecx_t *csxp, IS_specbody_t *ISp)
	{
	CL_flowspec_t *clsp = &ISp->spec_u.CL_spec;

	Set_Main_Hdr(&ISp->spec_mh, sizeof(CL_flowspec_t));
	Set_Serv_Hdr(&clsp->CL_spec_serv_hdr, CONTROLLED_LOAD_SERV, CLspec_len);
	Set_Parm_Hdr(&clsp->CL_spec_parm_hdr, IS_WKP_TB_TSPEC, 
							sizeof(TB_Tsp_parms_t));
	clsp->CLspec_r = csxp->xspec_r;
	clsp->CLspec_b = csxp->xspec_b;
	clsp->CLspec_p = csxp->xspec_p;
	clsp->CLspec_m = csxp->xspec_m;
	clsp->CLspec_M = csxp->xspec_M;
	return(0);
}


/*	Map Intserv Guaranteed Flowspec into Simplified format
 */
int
GtoCSZX_spec(IS_specbody_t *ISp, qos_flowspecx_t *csxp)
	{
	Guar_flowspec_t *gp = &ISp->spec_u.G_spec;

	csxp->xspec_r = gp->Gspec_r;
	csxp->xspec_b = gp->Gspec_b;
	csxp->xspec_m = gp->Gspec_m;
	csxp->xspec_M = gp->Gspec_M;
	csxp->xspec_p = gp->Gspec_p;
	csxp->xspec_R = gp->Gspec_R;
	csxp->xspec_S = gp->Gspec_S;
	csxp->spec_type = QOS_GUARANTEEDX;
	return(0);
}

/*	Map Intserv Controlled-Load Flowspec into Simplified format
 *	(We don't check format here... assume intserv format is OK)
 */
int
CLtoCSZX_spec(IS_specbody_t *ISp, qos_flowspecx_t *csxp)
	{
	CL_flowspec_t *clsp = &ISp->spec_u.CL_spec;

	csxp->xspec_r = clsp->CLspec_r; 
	csxp->xspec_b = clsp->CLspec_b; 
	csxp->xspec_p = clsp->CLspec_p;
	csxp->xspec_m = clsp->CLspec_m; 
	csxp->xspec_M = clsp->CLspec_M; 
	csxp->xspec_R = 0;
	csxp->xspec_S = 0;
	csxp->spec_type = QOS_CNTR_LOAD;
	return(0);
}

/*	Map Intserv Generic Tspec into Simplified format Tspec
 *	(We don't check format here... assume intserv format is OK)
 */
int
GentoCSZX_tspec(IS_tspbody_t *IStp, qos_tspecx_t *ctxp)
	{
	gen_Tspec_t *gtp = &IStp->tspec_u.gen_stspec;

	ctxp->xtspec_r = gtp->gtspec_r; 
	ctxp->xtspec_b = gtp->gtspec_b; 
	ctxp->xtspec_p = gtp->gtspec_p; 
	ctxp->xtspec_m = gtp->gtspec_m; 
	ctxp->xtspec_M = gtp->gtspec_M;
	ctxp->spec_type = QOS_TSPECX;
	return(0);
}

/*	Map Intserv Adspec into Simplified format Adspec
 */
int
IStoCSZX_adspec(IS_adsbody_t *ISap, qos_adspecx_t *siap)
	{
	memset((char *)siap, 0, sizeof(qos_adspecx_t));
	/* Now need to parse Adspec, set variables... */
	return (0);
}
