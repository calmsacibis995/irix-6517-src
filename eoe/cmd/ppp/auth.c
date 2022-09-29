/* PPP authorization protocols
 */

#ident "$Revision: 1.30 $"

#include <syslog.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <arpa/inet.h>
#include <crypt.h>

#include "ppp.h"


struct pap_pkt {
	u_char	code;
	u_char	id;
	u_short	len;
	u_char	slen;
	char	d[1];
};

struct chap_pkt {
	u_char	code;
	u_char	id;
	u_short	len;
	union {
	    struct {
		u_char	vsize;
		struct cval v;		/* what we use */
		char	name[1];
	    } chal;
	    struct {
		u_char	vsize;
#		 define CHAP_RESP_MD5_LEN   MD5_DIGEST_LEN
		u_char	val[CHAP_RESP_MD5_LEN];
		char	name[1];
	    } resp;
	    char    msg[1];
	} u;
};


#define A ppp->auth
#define ANAME A.name
#define IBUF_PAP ((struct pap_pkt*)&(ibuf.un.info[0]))
#define OBUF_PAP ((struct pap_pkt*)&(obuf.un.info[0]))
#define IBUF_CHAP ((struct chap_pkt*)&(ibuf.un.info[0]))
#define OBUF_CHAP ((struct chap_pkt*)&(obuf.un.info[0]))

/* PAP packet codes */
#define PAP_CODE_REQ 1			/* Authenticate-Request */
#define PAP_CODE_ACK 2			/* Authenticate-Ack */
#define PAP_CODE_NAK 3			/* Authenticate-Nak */

/* CHAP packet codes */
#define CHAP_CODE_CHALLENGE	1
#define CHAP_CODE_RESPONSE	2
#define CHAP_CODE_SUCCESS	3
#define CHAP_CODE_FAILURE	4


/* check username and (if PAP) password against /etc/passwd
 */
static int				/* 1=ok */
ck_name(struct ppp *ppp,
	u_short proto,
	char *name,
	char *passwd)
{
	if (strlen(name) >= sizeof(A.recvd_name)) {
		log_complain(ANAME, "received name \"%s\" too long",
			     name);
		return 0;
	}

	/* Do not let the peer try another name, to prevent a bad
	 * peer from probing or using us as an oracle.
	 */
	if (A.recvd_name[0] != '\0') {
		if (strcmp(name, A.recvd_name)) {
			log_complain(ANAME, "received wrong (changed) name"
				     " \"%s\" instead of \"%s\"",
				     name, A.recvd_name);
			return 0;
		}
	} else {
		(void)strcpy(A.recvd_name, name);

		/* If no names configured, than accept any name
		 * If any names were specified in the control file, then
		 * require that the peer give us one of the names on the
		 * specified list.
		 */
		if (A.recv_names.nm[0] != '\0') {
			struct recv_name *np = &A.recv_names;
			while (strcmp(np->nm, name)) {
				np = np->nxt;
				if (!np) {
					log_complain(ANAME,
						     "received wrong name"
						     " \"%s\"",
						     name);
					return 0;
				}
			}
		}
	}

	if (proto == PPP_PAP) {
		char *cp;
		struct passwd *pw = getpwnam(name);

		if (!pw) {
			log_complain(ANAME, "no such username as"
				     " received PAP name \"%s\"",
				     name);
			return 0;
		}

		cp = pw->pw_passwd;
		if (cp != 0 && *cp != '\0'
		    && strcmp(crypt(passwd, cp), cp)) {
			log_complain(ANAME,
				     "received bad PAP password for \"%s\"",
				     name);
			return 0;
		}
	}

	return 1;
}


/* send PAP ACK or NAK
 */
static void
pap_acknak(struct ppp *ppp,
	   u_char code,
	   u_char id,
	   char *msg,
	   char *debug_msg)
{
	if (code == PAP_CODE_ACK) {
		log_debug(2,ANAME, "send PAP Ack with \"%s\" %s",
			  msg, debug_msg);
	} else {
		log_complain(ANAME, "send PAP Nak with \"%s\" %s",
			     msg, debug_msg);
	}

	OBUF_PAP->code = code;
	OBUF_PAP->id = id;
	OBUF_PAP->slen = strlen(msg);
	(void)strcpy(OBUF_PAP->d, msg);
	obuf.bits = 0;
	obuf.proto = PPP_PAP;
	OBUF_PAP->len = OBUF_PAP->slen + sizeof(struct pap_pkt)-1;
	ppp_send(ppp,&obuf,OBUF_PAP->len);
}


/* send CHAP Success or Failure
 */
static void
chap_acknak(struct ppp *ppp,
	    u_char code,
	    u_char id)
{
	char *peer_msg;


	if (code == CHAP_CODE_SUCCESS) {
		peer_msg = "howdy";
		log_debug(2,ANAME, "send CHAP Success with \"%s\" %s",
			  peer_msg, "");
	} else {
		peer_msg = "wrong";
		log_complain(ANAME,"send CHAP Failure with \"%s\" %s",
			     peer_msg, "");
	}

	OBUF_CHAP->code = code;
	OBUF_CHAP->id = id;
	(void)strcpy(OBUF_CHAP->u.msg, peer_msg);
	obuf.bits = 0;
	obuf.proto = PPP_CHAP;
	OBUF_CHAP->len = strlen(peer_msg) + 4;
	ppp_send(ppp,&obuf,OBUF_CHAP->len);

	if (code != CHAP_CODE_SUCCESS) {
		/* no second chance */
		set_tr_msg(&ppp->lcp.fsm, "authentication failure");
		lcp_event(ppp,FSM_CLOSE);
	}
}


/* Use the right control file entry if we were using a generic model.
 *  Use the name the peer gave us as the tag for the control file entry.
 *  The generic control file entry must have either include no names
 * (and so any name is ok) or one of the specified names must have been
 * what the peer sent us.
 */
static int				/* 1=need to renegotiate */
reparse(struct ppp *ppp)
{
	int result = 0;

	if (reconfigure) {
		if (remote)
			free(remote);
		remote = strdup(A.recvd_name);
		log_debug(1,ANAME,"re-parse control file using label \"%s\"",
			  remote);

		result = parse_conf(1);

		/* ensure new MP parameters are used */
		(void)set_mp(ppp);
	}

	return result;
}

/* Try to join a resident daemon
 *	If we do rendezvous, this function does not return, but will
 *	indirectly exit().
 */
static void
try_rend(struct ppp *ppp)
{
	/* add remote host name to rendezvous node */
	if (remhost.sin_addr.s_addr != 0
	    && !def_remhost)
		(void)add_rend_name("",remhost_str);

	/* if the resident daemon is alive, join it */
	if (!reconfigure
	    && make_rend(ppp->rem_epdis[0] != '\0'))
		do_rend(ppp);
}


/* renegotiate LCP parameters after reconfiguring
 */
static void
renegotiate(struct ppp *ppp)
{
	log_debug(2,ANAME,"renegotiate LCP parameters");

	lcp_event(ppp, FSM_DOWN);
	lcp_event(ppp, FSM_UP);

	try_rend(ppp);
}


void
auth_init(struct ppp *ppp)
{
	A.chap_id = (u_char)newmagic();
	A.pap_id = A.chap_id;
	A.recvd_name[0] = '\0';
}


/* get ready to authenticate, or just skip it
 */
void
auth_start(struct ppp *ppp)
{
	char *recv, *send;


	++A.pap_id;

	if ((A.we_happy = (A.our_proto == 0)))
		A.we_happy_time = clk.tv_sec;
	A.peer_happy = (A.peer_proto == 0);

	if (!A.we_happy || !A.peer_happy) {
		switch (A.our_proto) {
		case PPP_PAP:
			recv = "PAP requests";
			break;
		case PPP_CHAP:
			recv = "CHAP responses";
			break;
		default:
			recv = "no authentication";
			break;
		}
		switch (A.peer_proto) {
		case PPP_PAP:
			send = "PAP requests";
			break;
		case PPP_CHAP:
			send = "CHAP responses";
			break;
		default:
			send = "no authentication";
			break;
		}
		log_debug(2,ANAME,
			  "will send %s %s receive %s",
			  send,
			  A.our_proto == A.peer_proto ? "and" : "but",
			  recv);
	}

	A.stime = clk.tv_sec;
	auth_time(ppp);			/* send 1st request */

	auth_done(ppp);
}


/* Try to advance to the network phase if authentication went well.
 */
void
auth_done(struct ppp *ppp)
{
	if (ppp->phase != AUTH_PHASE)
		return;

	/* We might now know the name or the address of the peer
	 * and be able to rendezvous.
	 */
	try_rend(ppp);

	/* go to the next phase only if not waiting for a password */
	if (A.we_happy && A.peer_happy) {
		log_phase(ppp,NET_PHASE);

		if (link_mux(ppp) < 0) {
			set_tr_msg(&ppp->lcp.fsm,"MP system failure");
			lcp_event(ppp,FSM_CLOSE);
			return;
		}

		/* Start IP if it was not already started for another
		 * link in an MP bundle.
		 * We should not start CCP, because the peer might not
		 * have been able to start CCP, because it does not know
		 * what IP addresses to use yet, and so has not been
		 * able to build its STREAMS mux.  If we start CCP now,
		 * it might have to discard our Configure-Request,
		 * causing us to time out.
		 */
		if (!mp_on || numdevs <= 1)
			ipcp_go(ppp);
	}
}


void
pap_ipkt(struct ppp *ppp)
{
	int ulen, plen;
	char *name, *passwd;


	if (ibuf_info_len < (int)IBUF_PAP->len) {
		log_complain(ANAME,"discard PAP packet with %d bytes "
			     "but claiming %d",
			     ibuf_info_len, IBUF_PAP->len);
		return;
	}

	if (ppp->phase != AUTH_PHASE
	    && ppp->phase != NET_PHASE) {
		log_debug(2,ANAME,"discard PAP packet received in %s phase",
			  phase_name(ppp->phase));
		return;
	}

	switch (IBUF_PAP->code) {
	case PAP_CODE_REQ:		/* Authenticate-Request */
		if (ibuf.bits != 0) {	/* MP encapsulation is forbidden */
			log_complain(ANAME,"discard MP PAP Request ID=%#x",
				     IBUF_PAP->id);
			break;
		}

		log_debug(2,ANAME,"receive PAP request ID=%#x", IBUF_PAP->id);
		ulen = IBUF_PAP->slen;
		name = &IBUF_PAP->d[0];
		if (ulen > (int)IBUF_PAP->len-4-2) {
			log_complain(ANAME,
				     "received bad PAP name length of %d",
				     ulen);
			break;
		}
		if (ulen > MAX_PAP_NAME_LEN) {
			log_complain(ANAME,"Warning: PAP names must be"
				     " UNIX usernames,"
				     " %d extra bytes will be ignored",
				     ulen - MAX_PAP_NAME_LEN);
		}
		passwd = &name[ulen];
		plen = *passwd;
		*passwd++ = '\0';
		if (ulen+plen > (int)IBUF_PAP->len-4-2) {
			log_complain(ANAME,
				     "received bad PAP password length of %d",
				     plen);
			break;
		}
		passwd[plen] = '\0';

		if (A.our_proto != PPP_PAP) {
			/* Ack it if we did not ask for it */
			pap_acknak(ppp,PAP_CODE_ACK,IBUF_PAP->id,
				   "why is this?",
				   "for unsolicited PAP request");
			log_complain(ANAME,
				     "unsolicited request for PAP username"
				     " \"%s\"",
				     &IBUF_PAP->d[0]);
			break;
		}

		/* check it */
		if (!ck_name(ppp, PPP_PAP, name, passwd)) {
			pap_acknak(ppp,PAP_CODE_NAK,IBUF_PAP->id, "wrong","");
			/* no second chance */
			set_tr_msg(&ppp->lcp.fsm, "authentication failure");
			lcp_event(ppp,FSM_CLOSE);
			break;
		}

		if (ppp->phase != AUTH_PHASE) {
			pap_acknak(ppp,PAP_CODE_ACK,IBUF_PAP->id,
				   "why is this?", "for tardy PAP request");
			break;
		}

		/* Reconfigure and start LCP over if we were using a
		 * generic control file entry.
		 * Do not send the PAP Ack, because that might fool
		 * the peer into advancing to the Network Phase and
		 * sending us IPCP packets before we are willing accept
		 * them.
		 */
		if (reparse(ppp)) {
			renegotiate(ppp);
			break;
		}

		A.we_happy = 1;
		A.we_happy_time = clk.tv_sec;
		pap_acknak(ppp, PAP_CODE_ACK, IBUF_PAP->id, "howdy", "");

		/* send PAP request or CHAP challenge if we now have
		 * the the info.
		 */
		if (A.chap_chal_delayed || A.pap_req_delayed)
			auth_time(ppp);
		else
			auth_done(ppp);
		break;


	case PAP_CODE_ACK:		/* Authenticate-Ack */
		if (ibuf.bits != 0) {	/* MP encapsulation is forbidden */
			log_complain(ANAME,"discard MP PAP Request ID=%#x",
				     IBUF_PAP->id);
			break;
		}
		ulen = IBUF_PAP->slen;
		name = &IBUF_PAP->d[0];
		if (ulen > (int)IBUF_PAP->len-4) {
			log_complain(ANAME, "received bad PAP Ack message"
				     " length of %d, %d too large",
				     ulen, ulen - IBUF_PAP->len-4);
			ulen = IBUF_PAP->len-4;
		}
		name[ulen] = '\0';

		if (A.peer_proto != PPP_PAP) {
			log_complain(ANAME, "discard PAP Ack"
				    " without LCP negotiation");
			break;
		}

		if (IBUF_PAP->id != A.pap_id) {
			log_debug(0,ANAME,
				  "wrong PAP Ack ID=%#x != expected %#x,"
				  " containing \"%s\"",
				  IBUF_PAP->id, A.pap_id, name);
			break;
		}
		A.peer_happy = 1;

		log_debug(2,ANAME,"receive PAP Ack ID=%#x containing \"%s\"",
			  A.pap_id, name);

		auth_done(ppp);
		break;


	case PAP_CODE_NAK:		/* Authenticate-Nak */
		if (ibuf.bits != 0) {	/* MP encapsulation is forbidden */
			log_complain(ANAME, "discard MP PAP Request ID=%#x",
				     IBUF_PAP->id);
			break;
		}
		ulen = IBUF_PAP->slen;
		name = &IBUF_PAP->d[0];
		if (ulen > (int)IBUF_PAP->len-4) {
			log_complain(ANAME,"received bad PAP Nak message"
				     " length of %d", ulen);
			break;
		}
		name[ulen] = '\0';

		if (A.peer_proto != PPP_PAP) {
			log_complain(ANAME, "discard PAP Ack"
				     " without LCP negotiation");
			break;
		}

		if (IBUF_PAP->id != A.pap_id) {
			log_debug(0, ANAME,
				  "bad PAP Nak ID %#x != expected %#x,"
				  " containing \"%s\"",
				  IBUF_PAP->id, A.pap_id, name);
				  break;
		}
		log_complain(ANAME,
			     "our PAP send_name and send_passwd were"
			     " rejected with \"%s\"",
			     name,0,0);
		break;

	default:
		log_ipkt(0, ANAME,"discard bogus PAP packet");
		break;
	}
}


/* Compute a standard CHAP challenge value given the random number and
 * the hostname
 */
static struct cval *
chap_chal_val(struct ppp *ppp,
	      u_char rnd[CVAL_RND_L],
	      char *name)
{
	int i;
	__uint32_t xor;
	union {
	    u_char  md5[MD5_DIGEST_LEN];
	    u_char  digest[CVAL_NAM_L];
	} nm;
	MD5_CTX md5_ctx;
	union {				/* local copy to avoid alignment */
	    struct cval c;		/*	hassles */
	    __uint32_t	i[1];
	} v;


	bcopy(rnd, v.c.cval_rnd, sizeof(v.c.cval_rnd));

	/* digest hostname */
	MD5Init(&md5_ctx);
	MD5Update(&md5_ctx, (u_char *)name, strlen(name));
	MD5Final(nm.md5, &md5_ctx);
	bcopy(nm.digest, v.c.cval_nam, sizeof(v.c.cval_nam));

	/* pick direction */
	switch (ppp->dv.callmode) {
	case CALLEE:
	case RE_CALLEE:
		bcopy("rECV", v.c.cval_dir, sizeof(v.c.cval_dir));
		break;
	case CALLER:
	case RE_CALLER:
	case Q_CALLER:
		bcopy("MaDe", v.c.cval_dir, sizeof(v.c.cval_dir));
		break;
	default:
		bcopy("Oops", v.c.cval_dir, sizeof(v.c.cval_dir));
		break;
	}

	/* combine direction and digested hostname */
	for (i = 0, xor = 0; i < sizeof(v.c)/4; i++)
		xor ^= v.i[i];
	bcopy(&xor, &v.c.cval_dir, sizeof(v.c.cval_dir));

	return &v.c;
}


/* handle incoming CHAP packets
 */
void
chap_ipkt(struct ppp *ppp)
{
	int len;
	u_char respond_val[CHAP_RESP_MD5_LEN];
	MD5_CTX md5_ctx;
	char *name;
	int need_renegotiate;


	if (ibuf_info_len < (int)IBUF_CHAP->len) {
		log_complain(ANAME,"discard CHAP packet with %d bytes "
			     "but claiming %d",
			     ibuf_info_len, IBUF_CHAP->len);
		return;
	}
	/* ensure any names are null terminated */
	IBUF_CHAP->u.msg[IBUF_CHAP->len] = '\0';

	if (ppp->phase != AUTH_PHASE
	    && ppp->phase != NET_PHASE) {
		log_debug(2,ANAME,"discard CHAP packet received in %s phase",
			  phase_name(ppp->phase));
		return;
	}

	switch (IBUF_CHAP->code) {
	case CHAP_CODE_CHALLENGE:
		if (ibuf.bits != 0) {	/* MP encapsulation is forbidden */
			log_complain(ANAME,"discard MP CHAP Request ID=%#x",
				     IBUF_CHAP->id);
			break;
		}
		len = IBUF_CHAP->u.chal.vsize;
		if (len > (int)IBUF_CHAP->len - (4+2)) {
			log_complain(ANAME,
				     "receive bad CHAP challenge value-size=%d"
				     "; overall len=%d",
				     len, IBUF_CHAP->len);
			break;
		}
		name = ((char *)&IBUF_CHAP->u.chal.v)+len;

		if (A.peer_proto != PPP_CHAP) {
			log_complain(ANAME, "discard CHAP challenge"
				     " without LCP negotiation");
			break;
		}

		log_debug(2,ANAME,"receive CHAP challenge ID=%#x,"
			  " name \"%s\"",
			  IBUF_CHAP->id, name);

		/* Check the name.  If it is bad, there is no way to
		 * tell the peer except by letting it time-out.
		 */
		if (!ck_name(ppp,PPP_CHAP,name,0))
			break;

		/* Reconfigure if we were using a generic control file entry.
		 * This may get us a password with which to respond.
		 */
		need_renegotiate = reparse(ppp);

		if (!A.have_send_passwd) {
			log_debug(1,ANAME, "receive CHAP challenge"
				  " for name \"%s\", but have no send_passwd"
				  "--assume null secret",
				  name);
		}

		/* If we must renegotiate LCP, do not send the CHAP
		 * response, because that might fool the peer into
		 * advancing to the Network Phase and sending
		 * us IPCP packets before we are willing accept them.
		 */
		if (need_renegotiate) {
			renegotiate(ppp);
			break;
		}

		/* Check to see if the peer is trying to us as an oracle,
		 * feeding our own challenges back to us.
		 * If so, ignore its challenges.
		 */
		if (len == sizeof(A.chap_cval)
		    && !bcmp(chap_chal_val(ppp,
					   IBUF_CHAP->u.chal.v.cval_rnd,
					   name),
			     &IBUF_CHAP->u.chal.v,
			     sizeof(IBUF_CHAP->u.chal.v))) {
			log_complain(ANAME, "received our own CHAP challenge"
				     "--do not respond");
			break;
		}

		log_debug(2,ANAME,
			  "answer CHAP challenge with our send_name \"%s\"",
			  A.send_name);

		OBUF_CHAP->code = CHAP_CODE_RESPONSE;
		OBUF_CHAP->id = IBUF_CHAP->id;
		OBUF_CHAP->len = (4+1+CHAP_RESP_MD5_LEN+strlen(A.send_name));
		obuf.bits = 0;
		obuf.proto = PPP_CHAP;
		OBUF_CHAP->u.resp.vsize = CHAP_RESP_MD5_LEN;
		MD5Init(&md5_ctx);
		MD5Update(&md5_ctx, &OBUF_CHAP->id,
			  sizeof(OBUF_CHAP->id));
		if (A.have_send_passwd)
			MD5Update(&md5_ctx, (u_char*)A.send_passwd,
				  strlen(A.send_passwd));
		MD5Update(&md5_ctx, (u_char*)&IBUF_CHAP->u.chal.v, len);
		MD5Final(OBUF_CHAP->u.resp.val,&md5_ctx);
		(void)strcpy(OBUF_CHAP->u.resp.name, A.send_name);
		ppp_send(ppp,&obuf,OBUF_CHAP->len);

		/* send PAP request or CHAP challenge if we now have the info.
		 */
		if (A.chap_chal_delayed || A.pap_req_delayed)
			auth_time(ppp);
		else
			auth_done(ppp);
		break;


	case CHAP_CODE_RESPONSE:
		if (ibuf.bits != 0) {	/* MP encapsulation is forbidden */
			log_complain(ANAME,"discard MP CHAP Request ID=%#x",
				     IBUF_CHAP->id);
			break;
		}
		len = IBUF_CHAP->u.resp.vsize;
		if (len != CHAP_RESP_MD5_LEN
		    || len > (int)IBUF_CHAP->len - (4+2)) {
			log_complain(ANAME,
				     "received bad CHAP response"
				     " value-size=%d; overall len=%d",
				     len, IBUF_CHAP->len);
			break;
		}
		if (A.our_proto != PPP_CHAP) {
			log_complain(ANAME, "discard CHAP response"
				     " without LCP negotiation");
			break;
		}

		if (IBUF_CHAP->id != A.chap_id) {
			/* This can happen if one machine or the other
			 * gets behind.
			 */
			log_debug(2,ANAME,
				  "wrong CHAP response ID=%#x != expected %#x",
				  IBUF_CHAP->id, A.chap_id);
			break;
		}

		name = (char*)&IBUF_CHAP->u.resp.val[len];

		log_debug(2,ANAME,"receive CHAP Response ID=%#x, name \"%s\"",
			  A.chap_id, name);

		/* check the name */
		if (!ck_name(ppp,PPP_CHAP,name,0)) {
			chap_acknak(ppp, CHAP_CODE_FAILURE, A.chap_id);
			break;
		}

		/* Reconfigure if we were using a generic control file entry.
		 * This might find a usable secret.
		 */
		need_renegotiate = reparse(ppp);

		/* check the secret */
		MD5Init(&md5_ctx);
		MD5Update(&md5_ctx, &A.chap_id, sizeof(A.chap_id));
		if (A.recv_passwd[0] != '\0') {
			MD5Update(&md5_ctx, (u_char*)A.recv_passwd,
				  strlen(A.recv_passwd));
		} else {
			log_complain(ANAME, "receive CHAP response for \"%s\","
				     " but recv_passwd not specified",
				     name);
			break;
		}
		MD5Update(&md5_ctx,
			  (u_char*)&A.chap_cval, sizeof(A.chap_cval));
		MD5Final(respond_val,&md5_ctx);
		if (bcmp(respond_val, IBUF_CHAP->u.resp.val,
			 sizeof(respond_val))) {
			chap_acknak(ppp, CHAP_CODE_FAILURE, A.chap_id);
			break;
		}

		/* If we must renegotiate LCP, do not send the CHAP
		 * response, because that might fool the peer into
		 * advancing to the Network Phase and sending
		 * us IPCP packets before we are willing accept them.
		 */
		if (need_renegotiate) {
			renegotiate(ppp);
			break;
		}

		A.we_happy = 1;
		A.we_happy_time = clk.tv_sec;
		chap_acknak(ppp, CHAP_CODE_SUCCESS, A.chap_id);

		/* send PAP request or CHAP challenge if we now have the info.
		 */
		if (A.chap_chal_delayed || A.pap_req_delayed)
			auth_time(ppp);
		else
			auth_done(ppp);
		break;


	case CHAP_CODE_SUCCESS:
		if (ibuf.bits != 0) {	/* MP encapsulation is forbidden */
			log_complain(ANAME,"discard MP CHAP Request ID=%#x",
				     IBUF_CHAP->id);
			break;
		}
		if (A.peer_proto != PPP_CHAP) {
			log_complain(ANAME, "discard CHAP success"
				     " without LCP negotiation");
			break;
		}

		A.peer_happy = 1;

		log_debug(2,ANAME,"receive CHAP Success ID=%#x"
			  " containing \"%s\"",
			  IBUF_CHAP->id, IBUF_CHAP->u.msg);

		auth_done(ppp);
		break;

	case CHAP_CODE_FAILURE:
		if (ibuf.bits != 0) {	/* MP encapsulation is forbidden */
			log_complain(ANAME,"discard MP CHAP Request ID=%#x",
				     IBUF_CHAP->id);
			break;
		}
		if (A.peer_proto != PPP_CHAP) {
			log_complain(ANAME, "discard CHAP success"
				     " without LCP negotiation");
			break;
		}

		log_complain(ANAME,
			     "our CHAP send_name and send_passwd were"
			     " rejected with \"%s\"",
			     IBUF_CHAP->u.msg);
		break;

	default:
		log_ipkt(0, ANAME,"discard bogus CHAP packet");
		break;
	}
}


/* Send a (or another) CHAP challenge to get the peer to authenticate
 * itself to us.
 * If we do not know our name, because we are hoping to figure
 * it out after the peer tells us its name and we reconfigure,
 * then delay until the peer sends us a name and we reconfigure.
 * But do not delay if the peer is not going to send us a CHAP
 * challenge or PAP request.
 */
static void
send_chap(struct ppp *ppp)
{
	struct {
	    struct timeval clk;
	    struct timeval cur_date;
	    __uint32_t	unit;
	    __uint32_t	locaddr;
	    __uint32_t	remaddr;
	    __uint32_t  rand;
	} val;
	union {
	    u_char  md5[MD5_DIGEST_LEN];
	    u_char  val[CVAL_RND_L];
	} rnd;
	MD5_CTX md5_ctx;


	if (A.our_proto != PPP_CHAP || A.we_happy)
		return;

	if (reconfigure
	    && A.send_name[0] == '\0'
	    && !A.peer_happy) {
		A.chap_chal_delayed = 1;
		return;
	}

	A.chap_chal_delayed = 0;
	if (A.send_name[0] == '\0') {
		log_debug(1,ANAME,"default our send_name to %s",
			  ourhost_nam);
		(void)strcpy(ppp0.auth.send_name,ourhost_nam);
	}

	/* build the challenge value */
	bzero(&A.chap_cval, sizeof(A.chap_cval));
	val.clk = clk;
	val.cur_date = cur_date;
	val.unit = ppp->dv.unit;
	val.remaddr = remhost.sin_addr.s_addr;
	val.locaddr = lochost.sin_addr.s_addr;
	val.rand = newmagic();
	MD5Init(&md5_ctx);
	MD5Update(&md5_ctx, (u_char*)&val, sizeof(val));
	MD5Final(rnd.md5, &md5_ctx);
	bcopy(rnd.val, A.chap_cval.cval_rnd, sizeof(A.chap_cval.cval_rnd));

	bcopy(chap_chal_val(ppp, rnd.val, A.send_name),
	      &A.chap_cval, sizeof(A.chap_cval));

	OBUF_CHAP->code = CHAP_CODE_CHALLENGE;
	OBUF_CHAP->id = ++A.chap_id;
	OBUF_CHAP->u.chal.vsize = sizeof(A.chap_cval);
	bcopy(&A.chap_cval, &OBUF_CHAP->u.chal.v, sizeof(A.chap_cval));
	(void)strcpy(OBUF_CHAP->u.chal.name, A.send_name);
	obuf.bits = 0;
	obuf.proto = PPP_CHAP;
	OBUF_CHAP->len = (4+1+sizeof(A.chap_cval)+strlen(A.send_name));
	log_debug(2,ANAME,"send CHAP challenge ID=%#x, name \"%s\"",
		  A.chap_id, A.send_name);

	ppp_send(ppp,&obuf,OBUF_CHAP->len);
}


/* Send another PAP request asking the peer to recognize us,
 * but only if we know what authentication to use.
 * If we are waiting for the peer to go first so we can reconfigure
 * and figure out the username or password, then just delay.
 *
 * If necessary, use a null PAP password.
 */
static void
send_pap(struct ppp *ppp)
{
	register int ulen, plen;

	if (A.peer_proto != PPP_PAP || A.peer_happy)
		return;

	if (reconfigure
	    && (A.send_name[0] == '\0' || !A.have_send_passwd)) {
		A.pap_req_delayed = 1;
		return;
	}

	A.pap_req_delayed = 0;
	if (A.send_name[0] == '\0') {
		log_debug(1,ANAME,"default send_name to %s",
			  ourhost_nam);
		(void)strcpy(ppp0.auth.send_name,ourhost_nam);
	}

	OBUF_PAP->code = PAP_CODE_REQ;
	OBUF_PAP->id = A.pap_id;
	ulen = strlen(A.send_name);
	OBUF_PAP->slen = ulen;
	(void)strcpy(&OBUF_PAP->d[0], A.send_name);
	if (!A.have_send_passwd) {
		plen = 0;
		log_debug(1,ANAME,"'send_pap' specified but"
			  " 'send_passwd' not specified--"
			  "assume null password");
	} else {
		plen = strlen(A.send_passwd);
	}
	OBUF_PAP->d[ulen] = plen;
	(void)strcpy(&OBUF_PAP->d[ulen+1], A.send_passwd);
	obuf.bits = 0;
	obuf.proto = PPP_PAP;
	OBUF_PAP->len = ulen+plen+sizeof(struct pap_pkt);
	log_debug(2,ANAME,"send PAP request ID=%#x",A.pap_id);
	ppp_send(ppp,&obuf,OBUF_PAP->len);
}


/* do something when the authorization timer expires
 */
void
auth_time(struct ppp *ppp)
{
	int secs, secs2;


	/* Do reauthentication during the network phase if appropriate.
	 */
	if (ppp->phase != AUTH_PHASE) {
		if (A.reauth_secs == 0
		    || A.our_proto != PPP_CHAP
		    || ppp->phase != NET_PHASE) {
			A.timer.tv_sec = TIME_NEVER;
			return;
		}

		if (A.we_happy) {
			/* sleep if not time to reauthenticate */
			secs = A.we_happy_time+A.reauth_secs - clk.tv_sec;
			if (secs > 0) {
				fsm_settimer(&A.timer, secs);
				return;
			}

			/* start to reauthenticate if it is time */
			A.we_happy = 0;
			A.stime = clk.tv_sec;
		}
	}


	/* Quit if no authentication received in time.
	 */
	secs = clk.tv_sec - A.stime;
	secs2 = A.max_secs - secs;
	if (secs2 <= 0) {
		if (!A.we_happy) {
			log_complain(ANAME, "no %s received after %d seconds",
				     ((A.our_proto == PPP_PAP)
				      ? "PAP username/password"
				      : "CHAP response"),
				     secs);
			A.timer.tv_sec = TIME_NEVER;
			set_tr_msg(&ppp->lcp.fsm, "no authentication");
			lcp_event(ppp,FSM_CLOSE);
			return;
		}

		if (!A.peer_happy) {
			if (A.peer_proto == PPP_PAP) {
				log_complain(ANAME, "%s did not like our"
					     " PAP requests; forget about"
					     " authentication", remote);
			} else {
				log_complain(ANAME, "no usable CHAP challenge"
					     " received from %s; forget about"
					     " autentication", remote);
			}
			A.peer_happy = 1;
		}
	}

	if (!A.we_happy || !A.peer_happy) {
		if (secs > 0)
			log_debug(2,ANAME,
				  "TO+  %d seconds remaining after %d seconds",
				  secs2, secs);

		/* Send another CHAP challenge to get the peer to authenticate
		 * itself to us.
		 */
		send_chap(ppp);
		/* Send another PAP request asking the peer to recognize us
		 */
		send_pap(ppp);
	}

	/* See if that finished things, and if so, set the timer for
	 * re-authentication
	 */
	auth_done(ppp);
	fsm_settimer(&A.timer, A.ms);
}
