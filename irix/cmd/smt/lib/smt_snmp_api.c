/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.16 $
 */

/***********************************************************
	Copyright 1989 by Carnegie Mellon University

		      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of CMU not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>

#include <sm_types.h>
#include <sm_log.h>
#include <smt_asn1.h>
#include <smt_mib.h>
#include <smt_snmp_auth.h>
#include <smt_snmp.h>
#include <smt_snmp_impl.h>
#include <smt_snmp_api.h>
#include <smtd_parm.h>
#include <smt_parse.h>
#include <smtd.h>


oid default_enterprise[] = {1, 3, 6, 1, 4, 1, 3, 1, 1}; /* enterprises.cmu.systems.cmuSNMP */

#define DEFAULT_COMMUNITY   "public"
#define DEFAULT_RETRIES	    3
#define DEFAULT_TIMEOUT	    2000000L
#define DEFAULT_REMPORT	    SNMP_PORT
#define DEFAULT_ENTERPRISE  default_enterprise
#define DEFAULT_TIME	    0

/*
 * Internal information about the state of the snmp session.
 */
struct snmp_internal_session {
	int	sd;		/* socket descriptor for this connection */
	ipaddr	addr;		/* address of connected peer */
	struct	request_list *requests;/* Info about outstanding requests */
};

/*
 * A list of all the outstanding requests for a particular session.
 */
struct request_list {
	struct request_list *next_request;
	u_long  request_id;	/* request id */
	int	    retries;	/* Number of retries */
	u_long timeout;		/* length to wait for timeout */
	struct timeval time;	/* Time this request was made */
	struct timeval expire;  /* time this request is due to expire */
	struct snmp_pdu *pdu;   /* pdu for this req (saved for retransmit) */
};

/*
 * The list of active/open sessions.
 */
struct session_list {
	struct session_list *next;
	struct snmp_session *session;
	struct snmp_internal_session *internal;
};

int snmp_errno = 0;

static	struct session_list *Sessions = NULL;
static	u_long Reqid = 0;
static	char *api_errors[4] = {
	"Unknown session",
	"Unknown host",
	"Invalid local port",
	"Unknown Error"
};

static char * api_errstring(int);
static void init_snmp();
static void free_request_list(struct request_list *);
static int snmp_build(struct snmp_session*, struct snmp_pdu*, char*, int*);
static int snmp_parse(struct snmp_session*, struct snmp_pdu*, char*, int);

static char *
api_errstring(err)
	int err;
{
	if (err <= SNMPERR_BAD_SESSION && err >= SNMPERR_GENERR){
		return(api_errors[err + 4]);
	}
	return("Unknown Error");
}


/*
 * Gets initial request ID for all transactions.
 */
static void
init_snmp()
{
	struct timeval tv;

	smt_gts(&tv);
	srandom(tv.tv_sec ^ tv.tv_usec);
	Reqid = random();
}

/*
 * Sets up the session with the snmp_session information provided
 * by the user.  Then opens and binds the necessary UDP port.
 * A handle to the created session is returned (this is different than
 * the pointer passed to snmp_open()).  On any error, NULL is returned
 * and snmp_errno is set to the appropriate error code.
 */
struct snmp_session *
snmp_open(struct snmp_session *session, int *sd, char *udsname)
{
	struct session_list *slp;
	struct snmp_internal_session *isp;
	char *cp;
	u_long addr;
	struct sockaddr_in me;
	struct hostent *hp;
	struct servent *servp;
	int estat, notbound;

	if (Reqid == 0)
		init_snmp();

	/*
	 * Copy session structure and link into list.
	 */
	isp = (struct snmp_internal_session *)Malloc(sizeof(*isp));
	slp = (struct session_list *)Malloc(sizeof(*slp));
	slp->internal = isp;
	slp->internal->sd = -1;		/* mark it not set */
	slp->session = (struct snmp_session *)Malloc(sizeof(*session));
	bcopy(session, slp->session, sizeof(*session));
	session = slp->session;

	/* now link it in. */
	slp->next = Sessions;
	Sessions = slp;

	/*
	 * session now points to the new structure that still contains
	 * pointers to data allocated elsewhere.
	 * Some of this data is copied to space malloc'd
	 * here, and the pointer replaced with the new one.
	 */
	if (session->peername != NULL) {
		cp = Malloc(strlen(session->peername)+1);
		strcpy(cp, session->peername);
		session->peername = cp;
		/* do not free peername. let the caller handle that */
	}

	/*
	 * Fill in defaults if necessary
	 * replace pointer with pointer to new data.
	 */
	if (session->community_len == SNMP_DEFAULT_COMMUNITY_LEN) {
		session->community_len = strlen(DEFAULT_COMMUNITY);
		session->community = DEFAULT_COMMUNITY;
	}
	cp = Malloc((unsigned)session->community_len);
	bcopy(session->community, cp, session->community_len);
	session->community = cp;

	if (session->retries == SNMP_DEFAULT_RETRIES)
		session->retries = DEFAULT_RETRIES;

	if (session->timeout == SNMP_DEFAULT_TIMEOUT)
		session->timeout = DEFAULT_TIMEOUT;
	isp->requests = NULL;

	/*
	 * Set up New connections iff the connection is not set up yet.
	 */
	if (*sd < 0) {
		int dom;
		if (udsname) {
			unlink(udsname);
			dom = AF_UNIX;
		} else
			dom = AF_INET;
		*sd = socket(dom, SOCK_DGRAM, 0);
		if (*sd < 0) {
			sm_log(LOG_ERR, SM_ISSYSERR, "snmp_open: socket");
			snmp_errno = SNMPERR_GENERR;
			estat = 1;
			goto sso_abort;
		}
		notbound = 1;
	} else
		notbound = 0;
	isp->sd = *sd;

	if (session->peername == SNMP_DEFAULT_PEERNAME) {
		isp->addr.sin_addr.s_addr = SNMP_DEFAULT_ADDRESS;
	} else if (udsname) {
		struct sockaddr_un *name;

		name = (struct sockaddr_un *)&isp->addr;
		bzero(name, sizeof(isp->addr));
		name->sun_family = AF_UNIX;
		strcpy(name->sun_path, SMTD_UDS);
	} else {
		if ((addr = inet_addr(session->peername)) != -1) {
			bcopy(&addr, &isp->addr.sin_addr,
					sizeof(isp->addr.sin_addr));
		} else {
			hp = gethostbyname(session->peername);
			if (hp == NULL){
				SNMPDEBUG((LOG_DBGSNMP, SM_ISSYSERR,
					"unknown host: %s",session->peername));
				snmp_errno = SNMPERR_BAD_ADDRESS;
				estat = 2;
				goto sso_abort;
			}
			bcopy(hp->h_addr, &isp->addr.sin_addr,
					hp->h_length);
		}
		isp->addr.sin_family = AF_INET;
		if (session->remote_port == SNMP_DEFAULT_REMPORT) {
			servp = getservbyname("smt", "udp");
			if (servp != NULL){
				isp->addr.sin_port = servp->s_port;
			} else {
#ifdef SMTPORTDEFINED
				/* fail until we get real SMT-PORT NUMBER.  */
				isp->addr.sin_port = htons(SNMP_PORT);
				sm_log(LOG_ERR,0,"SMTPORT not defined yet");
				exit(-1);
#endif
				isp->addr.sin_port =
					pmap_getport(&isp->addr, SMTPROG,
						SMTVERS, IPPROTO_UDP);
				if (isp->addr.sin_port == 0) {
					sm_log(LOG_ERR, SM_ISSYSERR,
						"couldn't get remote port");
					goto sso_abort;
				}
			}
		} else {
			isp->addr.sin_port = htons(session->remote_port);
		}
	}

	if (notbound) {
		int sts;
		if (udsname) {
			struct sockaddr_un name;

			bzero(&name, sizeof(name));
			name.sun_family = AF_UNIX;
			strcpy(name.sun_path, udsname);
			sts = bind(*sd, &name, sizeof(struct sockaddr_un));
		} else {
			bzero(&me, sizeof(me));
			me.sin_family = AF_INET;
			me.sin_addr.s_addr = INADDR_ANY;
			me.sin_port = htons(session->local_port);
			sts = bind(*sd, (struct sockaddr *)&me, sizeof(me));
		}
		if (sts) {
			sm_log(LOG_ERR, SM_ISSYSERR, "bind");
			snmp_errno = SNMPERR_BAD_LOCPORT;
			estat = 3;
			SNMPDEBUG((LOG_DBGSNMP, 0, "session bound failed"));
			goto sso_abort;
		}
		SNMPDEBUG((LOG_DBGSNMP, 0, "session bound to %d", *sd));
	}
	return(session);

sso_abort:
	if (!snmp_close(session)){
		sm_log(LOG_ERR, 0, "Couldn't abort session: %s. Exiting",
				api_errstring(snmp_errno));
		exit(estat);
	}
	return(0);
}


/*
 * Free each element in the input request list.
 */
static void
free_request_list(rp)
	struct request_list *rp;
{
	struct request_list *orp;

	while(rp){
		orp = rp;
		rp = rp->next_request;
		if (orp->pdu != NULL)
			snmp_free_pdu(orp->pdu);
		free(orp);
	}
}

/*
 * Close the input session.  Frees all data allocated for the session,
 * dequeues any pending requests, and closes any sockets allocated for
 * the session.  Returns 0 on error, 1 otherwise.
 */
int
snmp_close(session)
	struct snmp_session *session;
{
	struct session_list *slp = NULL, *oslp = NULL;

	for (slp = Sessions; slp; slp = slp->next) {
		if (slp->session == session) {
			if (oslp)
				oslp->next = slp->next;
			else
				Sessions = slp->next;
			break;
		}
		oslp = slp;
	}

	if (slp == 0) {
		snmp_errno = SNMPERR_BAD_SESSION;
		return(0);
	}

	/* If we found the session, free all data associated with it */
	if (slp->session->community != NULL)
		free(slp->session->community);
	if(slp->session->peername != NULL)
		free(slp->session->peername);
	free(slp->session);
	if (slp->internal->sd != -1)
		close(slp->internal->sd);
	free_request_list(slp->internal->requests);
	free(slp->internal);
	free(slp);
	SNMPDEBUG((LOG_DBGSNMP, 0,
			"session closed for fd=%d\n", slp->internal->sd));
	return(1);
}

/*
 * Takes a session and a pdu and serializes the ASN PDU into the area
 * pointed to by packet.  out_length is the size of the data area available.
 * Returns the length of the completed packet in out_length.  If any errors
 * occur, -1 is returned.  If all goes well, 0 is returned.
 */
static int
snmp_build(struct snmp_session	*session,
	   struct snmp_pdu	*pdu,
	   register char	*packet,
	   int			*out_length)
{
	char	buf[PACKET_LENGTH];
	register char *cp;
	struct	variable_list *vp;
	int	length;
	int	totallength;
	long	zero = 0;

	length = *out_length;
	cp = packet;
	for (vp = pdu->variables; vp; vp = vp->next_variable) {
		cp = snmp_build_var_op(cp, vp->name, &vp->name_length, vp->type,
				vp->val_len, vp->val.string, &length);
		if (cp == NULL)
			return(-1);
	}
	totallength = cp - packet;

	length = PACKET_LENGTH;
	cp = asn_build_header(buf, &length,
			      ASN_SEQUENCE | ASN_CONSTRUCTOR, totallength);
	if (cp == NULL)
		return(-1);
	bcopy(packet, cp, totallength);
	totallength += cp - buf;

	length = *out_length;
	if (pdu->command != TRP_REQ_MSG) {
		/* request id */
		cp = asn_build_int(packet, &length,
			ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_INTEGER,
			(long *)&pdu->reqid, sizeof(pdu->reqid));
		if (cp == NULL)
			return(-1);
		/* error status */
		cp = asn_build_int(cp, &length,
			ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_INTEGER,
			(long *)&pdu->errstat, sizeof(pdu->errstat));
		if (cp == NULL)
			return(-1);
		/* error index */
		cp = asn_build_int(cp, &length,
			ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_INTEGER,
			(long *)&pdu->errindex, sizeof(pdu->errindex));
		if (cp == NULL)
			return(-1);
	} else {	/* this is a trap message */
		/* enterprise */
		cp = asn_build_objid(packet, &length,
			ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_OBJECT_ID,
			(oid *)pdu->enterprise, pdu->enterprise_length);
		if (cp == NULL)
			return(-1);
		/* agent-addr */
		cp = asn_build_string(cp, &length,
			ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_OCTET_STR,
			(char*)&pdu->agent_addr.sin_addr.s_addr,
			sizeof(pdu->agent_addr.sin_addr.s_addr));
		if (cp == NULL)
			return(-1);
		/* generic trap */
		cp = asn_build_int(cp, &length,
			ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_INTEGER,
			(long *)&pdu->trap_type, sizeof(pdu->trap_type));
		if (cp == NULL)
			return(-1);
		/* specific trap */
		cp = asn_build_int(cp, &length,
			ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_INTEGER,
			(long *)&pdu->specific_type,sizeof(pdu->specific_type));
		if (cp == NULL)
			return(-1);
		/* timestamp  */
		cp = asn_build_int(cp, &length,
			ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_INTEGER,
			(long *)&pdu->time, sizeof(pdu->time));
		if (cp == NULL)
			return(-1);
	}
	if (length < totallength)
		return(-1);
	bcopy(buf, cp, totallength);
	totallength += cp - packet;

	length = PACKET_LENGTH;
	cp = asn_build_header(buf, &length, pdu->command, totallength);
	if (cp == NULL)
		return(-1);
	if (length < totallength)
		return(-1);
	bcopy(packet, cp, totallength);
	totallength += cp - buf;

	length = *out_length;
	cp = snmp_auth_build(packet, &length, session->community,
			&session->community_len, &zero, totallength);
	if (cp == NULL)
		return(-1);
	if ((*out_length - (cp - packet)) < totallength)
		return(-1);
	bcopy(buf, cp, totallength);
	totallength += cp - packet;
	*out_length = totallength;
	return(0);
}

/*
 * Parses the packet recieved on the input session, and places the data into
 * the input pdu.  length is the length of the input packet.  If any errors
 * are encountered, -1 is returned.  Otherwise, a 0 is returned.
 */
static int
snmp_parse(struct snmp_session *session,
	   struct snmp_pdu *pdu,
	   char  *data,
	   int	length)
{
	char	msg_type;
	char	type;
	char	*var_val;
	long    version;
	int	len, four;
	char community[128];
	int community_length = 128;
	struct variable_list *vp;
	oid    objid[MAX_NAME_LEN], *op;

	/* authenticates message and returns length if valid */
	data = snmp_auth_parse(data, &length, community,
				&community_length, &version);
	if (data == NULL)
		return(-1);

	if (version != SNMP_VERSION_1) {
		sm_log(LOG_ERR, 0, "Wrong version: %d, continuing anyway",
			version);
	}
	if (session->authenticator){
		data = session->authenticator(data, &length,
					community, community_length);
		if (data == NULL) {
			sm_log(LOG_INFO, 0, "auth only packet");
			return(0);
		}
	}
	data = asn_parse_header(data, &length, &msg_type);
	if (data == NULL) {
		sm_log(LOG_ERR, 0, "snmp_parse: bad msg_type");
		return(-1);
	}

	pdu->command = msg_type;
	if (pdu->command != TRP_REQ_MSG) {
		data = asn_parse_int(data, &length, &type,
				(long *)&pdu->reqid, sizeof(pdu->reqid));
		if (data == NULL) {
			sm_log(LOG_ERR, 0, "snmp_parse: bad reqid");
			return(-1);
		}
		data = asn_parse_int(data, &length, &type,
				(long *)&pdu->errstat, sizeof(pdu->errstat));
		if (data == NULL) {
			sm_log(LOG_ERR, 0, "snmp_parse: bad errstat");
			return(-1);
		}
		data = asn_parse_int(data, &length, &type,
				(long *)&pdu->errindex, sizeof(pdu->errindex));
		if (data == NULL) {
			sm_log(LOG_ERR, 0, "snmp_parse: bad errindex");
			return(-1);
		}
	} else {
		pdu->enterprise_length = MAX_NAME_LEN;
		data = asn_parse_objid(data, &length, &type,
					objid, &pdu->enterprise_length);
		if (data == NULL)
			return(-1);
		pdu->enterprise = (oid *)Malloc(pdu->enterprise_length
						* sizeof(oid));
		bcopy(objid, pdu->enterprise,
				pdu->enterprise_length * sizeof(oid));

		four = 4;
		data = asn_parse_string(data, &length, &type,
			(char*)&pdu->agent_addr.sin_addr.s_addr, &four);
		if (data == NULL)
			return(-1);
		data = asn_parse_int(data, &length, &type,
			(long *)&pdu->trap_type, sizeof(pdu->trap_type));
		if (data == NULL)
			return(-1);
		data = asn_parse_int(data, &length, &type,
			(long*)&pdu->specific_type, sizeof(pdu->specific_type));
		if (data == NULL)
			return(-1);
		data = asn_parse_int(data, &length, &type,
				(long *)&pdu->time, sizeof(pdu->time));
		if (data == NULL)
			return(-1);
	}
	data = asn_parse_header(data, &length, &type);
	if (data == NULL) {
		sm_log(LOG_ERR, 0, "snmp_parse: bad type length");
		return(-1);
	}

	if (type != (ASN_SEQUENCE | ASN_CONSTRUCTOR)) {
		sm_log(LOG_ERR, 0, "snmp_parse: bad type=%x", type);
		return(-1);
	}

	while (length > 0) {
		if (pdu->variables == NULL){
			vp = (struct variable_list *)Malloc(sizeof(*vp));
			pdu->variables = vp;
		} else {
			vp->next_variable =
				(struct variable_list *)Malloc(sizeof(*vp));
			vp = vp->next_variable;
		}
		vp->next_variable = NULL;
		vp->val.string = NULL;
		vp->name = NULL;
		vp->name_length = MAX_NAME_LEN;
		data = snmp_parse_var_op(data, objid, &vp->name_length,
			&vp->type, &vp->val_len, &var_val, (int *)&length);
		if (data == NULL) {
			sm_log(LOG_ERR, 0, "snmp_parse: bad var_op");
			return(-1);
		}
		op = (oid *)Malloc((unsigned)vp->name_length * sizeof(oid));
		bcopy(objid, op, vp->name_length * sizeof(oid));
		vp->name = op;

		len = PACKET_LENGTH;
		SNMPDEBUG((LOG_DBGSNMP, 0,
			"snmp_parse: type=%d", (int)vp->type));
		switch((short)vp->type){
		case ASN_INTEGER:
		case COUNTER:
		case GAUGE:
		case TIMETICKS:
			vp->val.integer = (long *)Malloc(sizeof(long));
			vp->val_len = sizeof(long);
			asn_parse_int(var_val, &len, &vp->type,
				(long*)vp->val.integer,sizeof(vp->val.integer));
			break;
		case ASN_OCTET_STR:
		case IPADDRESS:
		case OPAQUE:
			vp->val.string = Malloc(vp->val_len);
			asn_parse_string(var_val, &len, &vp->type,
					vp->val.string, &vp->val_len);
			break;
		case ASN_OBJECT_ID:
			vp->val_len = MAX_NAME_LEN;
			asn_parse_objid(var_val, &len, &vp->type,
						objid, &vp->val_len);
			vp->val_len *= sizeof(oid);
			vp->val.objid = (oid *)Malloc((unsigned)vp->val_len);
			bcopy(objid, vp->val.objid, vp->val_len);
			break;
		case ASN_NULL:
			break;
		default:
			sm_log(LOG_ERR, 0, "bad type returned(%x)", vp->type);
			return(-1);
		}
	}
	return(SNMP_ERR_NOERROR);
}

/*
 * Sends the input pdu on the session after calling snmp_build to create
 * a serialized packet.  If necessary, set some of the pdu data from the
 * session defaults.  Add a request corresponding to this pdu to the list
 * of outstanding requests on this session, then send the pdu.
 * Returns the request id of the generated packet if applicable, otherwise 1.
 * On any error, 0 is returned.
 * The pdu is freed by snmp_send() unless a failure occured.
 */
int
snmp_send(session, pdu)
	struct snmp_session *session;
	struct snmp_pdu	*pdu;
{
	struct session_list *slp;
	struct snmp_internal_session *isp = NULL;
	char  packet[PACKET_LENGTH];
	int length = PACKET_LENGTH;
	struct request_list *rp;
	struct timeval tv;

	for (slp = Sessions; slp; slp = slp->next) {
		if (slp->session == session) {
			isp = slp->internal;
			break;
		}
	}
	if (isp == NULL) {
		snmp_errno = SNMPERR_BAD_SESSION;
		return(0);
	}
	SNMPDEBUG((LOG_DBGSNMP, 0, "isp found"));

	if (pdu->command == GET_REQ_MSG || pdu->command == GETNEXT_REQ_MSG ||
	    pdu->command == GET_RSP_MSG || pdu->command == SET_REQ_MSG) {
		if (pdu->reqid == SNMP_DEFAULT_REQID)
			pdu->reqid = ++Reqid;
		if (pdu->errstat == SNMP_DEFAULT_ERRSTAT)
			pdu->errstat = 0;
		if (pdu->errindex == SNMP_DEFAULT_ERRINDEX)
			pdu->errindex = 0;
	} else {
		/*
		 * fill in trap defaults.
		 * give a bogus non-error reqid for traps.
		 */
		pdu->reqid = 1;
		if (pdu->enterprise_length == SNMP_DEFAULT_ENTERPRISE_LENGTH) {
			pdu->enterprise = (oid *)Malloc(sizeof(DEFAULT_ENTERPRISE));
			bcopy(DEFAULT_ENTERPRISE, pdu->enterprise,
			      sizeof(DEFAULT_ENTERPRISE));
			pdu->enterprise_length =
				sizeof(DEFAULT_ENTERPRISE)/sizeof(oid);
		}
		if (pdu->time == SNMP_DEFAULT_TIME)
			pdu->time = DEFAULT_TIME;
	}

	SNMPDEBUG((LOG_DBGSNMP, 0, "setting target addr"));
	if (pdu->address.sin_addr.s_addr == SNMP_DEFAULT_ADDRESS) {
		if (isp->addr.sin_addr.s_addr == SNMP_DEFAULT_ADDRESS) {
			sm_log(LOG_ERR, 0, "No remote IP address specified");
			snmp_errno = SNMPERR_BAD_ADDRESS;
			return(0);
		}
		bcopy(&isp->addr, &pdu->address,
				sizeof(pdu->address));
	}


	SNMPDEBUG((LOG_DBGSNMP, 0, "building packet"));
	if (snmp_build(session, pdu, packet, &length) < 0) {
		sm_log(LOG_ERR, 0, "Error building packet");
		snmp_errno = SNMPERR_GENERR;
		return(0);
	}
	sm_hexdump(LOG_DEBUG, packet, length);

	smt_gts(&tv);
	if (((struct sockaddr *)&pdu->address)->sa_family == AF_INET) {
		SNMPDEBUG((LOG_DBGSNMP, 0, "sending %d bytes to %s:%d",
			length,
			inet_ntoa(pdu->address.sin_addr),
			(int)pdu->address.sin_port));
	} else {
		SNMPDEBUG((LOG_DBGSNMP, 0, "sending %d bytes to %s",
			length,
			((struct sockaddr_un *)&pdu->address)->sun_path));
	}
	if (sendto(isp->sd, packet, length, 0,
		(struct sockaddr *)&pdu->address, sizeof(pdu->address)) < 0) {
		sm_log(LOG_ERR, SM_ISSYSERR, "snmp_send()sendto");
		snmp_errno = SNMPERR_GENERR;
		return 0;
	}
	if (pdu->command == GET_REQ_MSG ||
	    pdu->command == GETNEXT_REQ_MSG ||
	    pdu->command == SET_REQ_MSG) {
		/* set up to expect a response */
		rp = (struct request_list *)Malloc(sizeof(struct request_list));
		rp->next_request = isp->requests;
		isp->requests = rp;
		rp->pdu = pdu;
		rp->request_id = pdu->reqid;

		rp->retries = 1;
		rp->timeout = session->timeout;
		rp->time = tv;
		tv.tv_usec += rp->timeout;
		tv.tv_sec += tv.tv_usec / 1000000L;
		tv.tv_usec %= 1000000L;
		rp->expire = tv;
	}
	return(pdu->reqid);
}

/*
 * Frees the pdu and any malloc'd data associated with it.
 */
void
snmp_free_pdu(pdu)
	struct snmp_pdu *pdu;
{
	struct variable_list *vp, *ovp;

	vp = pdu->variables;
	while(vp){
		if (vp->name)
			free(vp->name);
		if (vp->val.string)
			free(vp->val.string);
		ovp = vp;
		vp = vp->next_variable;
		free(ovp);
	}
	if (pdu->enterprise)
		free(pdu->enterprise);
	free(pdu);
}


/*
 * Checks to see if any of the fd's set in the fdset belong to
 * snmp.  Each socket with it's fd set has a packet read from it
 * and snmp_parse is called on the packet received.  The resulting pdu
 * is passed to the callback routine for that session.  If the callback
 * routine returns successfully, the pdu and it's request are deleted.
 */
void
snmp_read(fdset)
	fd_set  *fdset;
{
	struct session_list *slp;
	struct snmp_session *sp;
	struct snmp_internal_session *isp;
	char packet[PACKET_LENGTH];
	struct sockaddr_in from;
	int length, fromlength;
	struct snmp_pdu *pdu;
	struct request_list *rp, *orp;

	for (slp = Sessions; slp; slp = slp->next) {
		if (!FD_ISSET(slp->internal->sd, fdset))
			continue;

		SNMPDEBUG((LOG_DBGSNMP, 0, "snmp_read: got session"));
		sp = slp->session;
		isp = slp->internal;
		fromlength = sizeof(from);
		length = recvfrom(isp->sd, packet, PACKET_LENGTH,
				0, (struct sockaddr *)&from, &fromlength);
		if (length == -1)
			sm_log(LOG_ERR, SM_ISSYSERR, "recvfrom");

		if (from.sin_family == AF_INET) {
			SNMPDEBUG((LOG_DBGSNMP, 0, "recvd %d bytes from %s:%d",
				length,
				inet_ntoa(from.sin_addr),
				(int)from.sin_port));
		} else {
			SNMPDEBUG((LOG_DBGSNMP, 0, "recvd %d bytes from %s",
				length,
				((struct sockaddr_un *)&from)->sun_path));
		}

		sm_hexdump(LOG_DEBUG, packet, length);

		pdu = (struct snmp_pdu *)Malloc(sizeof(struct snmp_pdu));
		pdu->address = from;
		pdu->reqid = 0;
		pdu->variables = NULL;
		pdu->enterprise = NULL;
		pdu->enterprise_length = 0;
		if (snmp_parse(sp, pdu, packet, length) != SNMP_ERR_NOERROR){
			sm_log(LOG_ERR, 0, "Mangled packet");
			snmp_free_pdu(pdu);
			return;
		}
		SNMPDEBUG((LOG_DBGSNMP, 0, "snmp_read: parsed msg = %d",
						(u_long)pdu->command));
		if (pdu->command == GET_REQ_MSG ||
		    pdu->command == GETNEXT_REQ_MSG ||
		    pdu->command == TRP_REQ_MSG ||
		    pdu->command == SET_REQ_MSG) {
			sp->callback(RECEIVED_MESSAGE, sp, pdu->reqid, pdu,
					sp->callback_magic);
			goto rloop;
		}

		if (pdu->command != GET_RSP_MSG)
			goto rloop;

		/* Now serve GET_RSP_MSG */

		for (rp = isp->requests; rp; rp = rp->next_request) {
			if (rp->request_id != pdu->reqid)
				continue;
			if (sp->callback(RECEIVED_MESSAGE, sp, pdu->reqid,
				pdu, sp->callback_magic) != 1)
				continue;

			/* successful, so delete request */
			orp = rp;
			for (rp = isp->requests; rp; rp = rp->next_request) {
				if (isp->requests == orp){
					/* first in list */
					isp->requests = orp->next_request;
					break;
				}
				if (rp->next_request == orp) {
					rp->next_request = orp->next_request;
					break;
				}
			}
			snmp_free_pdu(orp->pdu);
			free(orp);
			break;
			/*
			 * there shouldn't be any more request with the
			 * same reqid.
			 */
		}
rloop:
		snmp_free_pdu(pdu);
	}
}

/*
 * Returns info about what snmp requires from a select statement.
 * numfds is the number of fds in the list that are significant.
 * All file descriptors opened for SNMP are OR'd into the fdset.
 * If activity occurs on any of these file descriptors, snmp_read
 * should be called with that file descriptor set
 *
 * The timeout is the latest time that SNMP can wait for a timeout.  The
 * select should be done with the minimum time between timeout and any other
 * timeouts necessary.  This should be checked upon each invocation of select.
 * If a timeout is received, snmp_timeout should be called to check if the
 * timeout was for SNMP.  (snmp_timeout is idempotent)
 *
 * Block is 1 if the select is requested to block indefinitely,
 * rather than time out.
 * If block is input as 1, the timeout value will be treated as undefined,
 * but it must
 * be available for setting in snmp_select_info.  On return,
 * if block is true, the value
 * of timeout will be undefined.
 *
 * snmp_select_info returns the number of open sockets.
 * (i.e. The number of sessions open)
 */
int
snmp_select_info(numfds, fdset, timeout, block)
	int	    *numfds;
	fd_set  *fdset;
	struct timeval *timeout;
	int	    *block; /* should the select block until input arrives. */
{
	struct session_list *slp;
	struct snmp_internal_session *isp;
	struct request_list *rp;
	struct timeval now, earliest;
	int active = 0, requests = 0;

	timerclear(&earliest);

	/*
	 * For each request outstanding, add it's socket to the fdset,
	 * and if it is the earliest timeout to expire, mark it as lowest.
	 */
	for (slp = Sessions; slp; slp = slp->next) {
		active++;
		isp = slp->internal;
		if ((isp->sd + 1) > *numfds)
			*numfds = (isp->sd + 1);
		FD_SET(isp->sd, fdset);
		if (isp->requests) {
			/* found another session with outstanding requests */
			requests++;
			for (rp = isp->requests; rp; rp = rp->next_request) {
				if (!timerisset(&earliest) ||
				     timercmp(&rp->expire, &earliest, <))
					earliest = rp->expire;
			}
		}
	}

	/* if none are active, skip arithmetic */
	if (requests == 0) {
		sm_log(LOG_DEBUG,0,"no req pending\n");
		return(active);
	}

	/*
	 * Now find out how much time until the earliest timeout.  This
	 * transforms earliest from an absolute time into a delta time, the
	 * time left until the select should timeout.
	 */
	smt_gts(&now);
	earliest.tv_sec--;	/* adjust time to make arithmetic easier */
	earliest.tv_usec += 1000000L;
	earliest.tv_sec -= now.tv_sec;
	earliest.tv_usec -= now.tv_usec;
	while (earliest.tv_usec >= 1000000L) {
		earliest.tv_usec -= 1000000L;
		earliest.tv_sec += 1;
	}

	if (earliest.tv_sec < 0) {
		earliest.tv_sec = 0;
		earliest.tv_usec = 0;
	}

	/*
	 * if it was blocking before or our delta time is less,
	 * reset timeout
	 */
	if (*block == 1 || timercmp(&earliest, timeout, <)) {
		*timeout = earliest;
		*block = 0;
	}
	return(active);
}

/*
 * snmp_timeout should be called whenever the timeout
 * from snmp_select_info expires,
 * but it is idempotent, so snmp_timeout can be polled (probably a cpu
 * expensive proposition).  snmp_timeout checks to see if any of the sessions
 * have an outstanding request that has timed out.  If it finds one (or more),
 * and that pdu has more retries available, a new packet is formed from the
 * pdu and is resent.  If there are no more retries available, the callback
 * for the session is used to alert the user of the timeout.
 */
void
snmp_timeout()
{
	struct session_list *slp;
	struct snmp_session *sp;
	struct snmp_internal_session *isp;
	struct request_list *rp, *orp, *freeme = NULL;
	struct timeval now;

	smt_gts(&now);
	/*
	* For each request outstanding, check to see if it has expired.
	*/
	for (slp = Sessions; slp; slp = slp->next) {
		sp = slp->session;
		isp = slp->internal;
		orp = NULL;
		for (rp = isp->requests; rp; rp = rp->next_request) {
			char  packet[PACKET_LENGTH];
			int length = PACKET_LENGTH;
			struct timeval tv;

			/*
			 * frees rp's after the for loop goes on
			 * to the next_request
			 */
			if (freeme != NULL) {
				free(freeme);
				freeme = NULL;
			}

			if (!timercmp(&rp->expire, &now, <)) {
				orp = rp;
				continue;
			}

			/* this timer has expired */
			if (rp->retries >= sp->retries){
				/* No more chances, delete this entry */
				sp->callback(TIMED_OUT, sp, rp->pdu->reqid,
						rp->pdu, sp->callback_magic);
				if (orp == NULL){
					isp->requests = rp->next_request;
				} else {
					orp->next_request = rp->next_request;
				}
				snmp_free_pdu(rp->pdu);
				freeme = rp;
				continue;	/* don't update orp below */
			}

			/* retransmit this pdu */
			rp->retries++;
			rp->timeout <<= 1;
			if (snmp_build(sp, rp->pdu, packet, &length) < 0) {
				sm_log(LOG_ERR, 0, "Error building packet");
			}
			sm_hexdump(LOG_DEBUG, packet, length);

			smt_gts(&tv);
			if (sendto(isp->sd, packet, length, 0,
				(struct sockaddr *)&rp->pdu->address,
				sizeof(rp->pdu->address)) < 0) {
				sm_log(LOG_ERR, SM_ISSYSERR,
				       "snmp_timeout()sendto");
			}
			rp->time = tv;
			tv.tv_usec += rp->timeout;
			tv.tv_sec += tv.tv_usec / 1000000L;
			tv.tv_usec %= 1000000L;
			rp->expire = tv;

			orp = rp;
		}

		if (freeme != NULL){
			free(freeme);
			freeme = NULL;
		}
	}
}

