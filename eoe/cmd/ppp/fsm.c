/* PPP finite state machine skeleton.
 *	This is used by the many PPP state machines
 */


#ident "$Revision: 1.18 $"

#include <values.h>
#include <sys/types.h>

#include "ppp.h"


#define STRUCT_PKT struct ncp_pkt
#define STRUCT_CF struct ncp_cf

/* generic shape of an LCP or NCP packet */
STRUCT_PKT {
	u_char	code;
	u_char	id;
	u_short	len;
	union {
	    STRUCT_CF {			/* Configure-Req, Ack, Nak, Reject */
		u_char  type;
		u_char  len;
		u_char  info[1];
	    } un_cf[1];
	    struct {			/* Code-Reject */
		u_char  cj_code;
		u_char  cj_id;
		u_short cj_len;
		u_char  cj_info[1];
	    } cj;
	} ncp_un;
};
#define cf ncp_un.un_cf


static void
fsm_reinit(struct fsm *fsm)
{
	fsm->state = FSM_INITIAL_0;
	fsm->timer.tv_sec = TIME_NEVER;
	fsm->nak_sent = 0;
	fsm->nak_recv = 0;
}


/* initialize a finite state machine.
 */
void
fsm_init(struct fsm *fsm)
{
	u_long nid;

	fsm_reinit(fsm);
	nid = newmagic();
	fsm->id = (u_char)(nid ^ (nid >> 8) ^ (nid >> 16) ^ (nid >> 24));
}


void
fsm_settimer(struct timeval *timer,
	     time_t ms)
{
	get_clk();
	timer->tv_usec = clk.tv_usec + ms*1000;
	timer->tv_sec = clk.tv_sec + timer->tv_usec / 1000000;
	timer->tv_usec %= 1000000;
}


/* get time until expiration
 */
long					/* return ms until expired */
cktime(struct timeval *tp)
{
	struct timeval delta;

	if (tp->tv_sec == TIME_NEVER)
		return MAXLONG/10;

	timevalsub(&delta,tp,&clk);

	/* defined against time changes and being late */
	if (delta.tv_sec < 0)
		return TIME_NOW*1000;

	return (delta.tv_sec*1000 + delta.tv_usec/1000);
}



char*
fsm_action_name(enum fsm_action act)
{
	static char *names[] = {
		"Null",
		"TLU",
		"TLD",
		"TLS",
		"TLF"
	};
	static char bad[] = "xxxxxxxxxx";

	if (act <= FSM_TLF)
		return names[act];
	sprintf(bad, "%d", act);
	return bad;
}


char*
fsm_state_name(enum fsm_state state)
{
	static char *names[] = {
		"Initial(0)",
		"Starting(1)",
		"Closed(2)",
		"Stopped(3)",
		"Closing(4)",
		"Stopping(5)",
		"Req-Sent(6)",
		"Ack-Rcvd(7)",
		"Ack-Sent(8)",
		"Opened(9)"
	};
	static char bad[] = "xxxxxxxxxx";

	if (state <= FSM_OPENED_9)
		return names[state];
	sprintf(bad, "%d", state);
	return bad;
}


char*
fsm_code_name(u_char code)
{
	static char *names[] = {
		0,			/* invalid	    0 */
		"Configure-Request",	/* PPP_CODE_CR	    1 */
		"Configure-Ack",	/* PPP_CODE_CA	    2 */
		"Configure-Nak",	/* PPP_CODE_CN	    3 */
		"Configure-Reject",	/* PPP_CODE_CONFJ   4 */
		"Terminate-Request",	/* PPP_CODE_TR	    5 */
		"Terminate-Ack",	/* PPP_CODE_TA	    6 */
		"Code-Reject",		/* PPP_CODE_CJ	    7 */
		"Protocol-Reject",	/* PPP_CODE_PJ	    8 */
		"Echo-Request",		/* PPP_CODE_EREQ    9 */
		"Echo-Reply",		/* PPP_CODE_EREP    10 */
		"Discard-Request",	/* PPP_CODE_DIS	    11 */
		"Identification",	/* PPP_CODE_IDENT   12 */
		"Time-Remaining",	/* PPP_CODE_TIME    13 */
		"Reset-Request",	/* PPP_CODE_RREQ    14 */
		"Reset-Ack",		/* PPP_CODE_RACK    15 */
	};
	static char bad[] = "unknown packet code 0xXX";

	if (code != 0
	    && code < sizeof(names)/sizeof(names[0])
	    && names[code] != 0)
		return names[code];
	sprintf(bad, "unknown packet code %#x", code);
	return bad;
}


static struct {
	int	lvl;
	char	*pat;
	char	*name;
} ev_names[] = {
	{3,	"event %s",	"Up"},
	{3,	"event %s",	"Down"},
	{3,	"event %s",	"Open"},
	{3,	"event %s",	"Close"},
	{2,	"event %s #%d",	"TO+: Timeout"},
	{2,	"event %s",	"TO-: Final Timeout"},
	{3,	"event %s",	"RCR+: Receive Configure-Request"},
	{3,	"event %s",	"RCR-: Receive bad Configure-Request"},
	{3,	"event %s",	"RCA: Receive Configure-Ack"},
	{3,	"event %s",	"RCN: Receive Configure-Nak/Reject"},
	{3,	"event %s",	"RTR: Receive Terminate-Request"},
	{3,	"event %s",	"RTA: Receive Terminat-Ack"},
	{3,	"event %s",	"RUC: Receive Unknown-Code"},
	{3,	"event %s",	"RXJ+: Receive ok Code- or Protocol-Reject"},
	{3,	"event %s",	"RXJ-: Receive bad Code- or Protocol-Reject"},
	{3,	"event %s",	"RXR: Receive Echo- or Discard-"}
};

static void
fsm_bad_event(struct fsm *fsm,
	      enum fsm_event event)
{
	if (event <= FSM_RXR)
		log_complain(fsm->name, "bad event %s in state %s",
			     ev_names[event].name,
			     fsm_state_name(fsm->state));
	else
		log_complain(fsm->name, "impossible event %d in state %s",
			     event, fsm_state_name(fsm->state));
}


void
fsm_run(struct ppp *ppp,
	struct fsm *fsm,
	enum fsm_event event)
{
#	define SET_RTIMER() (fsm_settimer(&fsm->timer,fsm->ms),	\
			     fsm->ms = MIN(fsm->ms*2,		\
					   fsm->restart_ms_lim))

#	define CLR_TIMER() (fsm->timer.tv_sec = TIME_NEVER)

	/* This Layer Up */
#	define ACT_TLU (set_tr_msg(fsm,0),		\
			fsm->ops->act(ppp,FSM_TLU))

	/* This Layer Down */
#	define ACT_TLD fsm->ops->act(ppp,FSM_TLD)

	/* This Layer Start */
#	define ACT_TLS (set_tr_msg(fsm,0),		\
			fsm->ops->act(ppp,FSM_TLS))

	/* This Layer Finished */
#	define ACT_TLF(st) (CLR_TIMER(),		\
			    fsm->state = (st),		\
			    set_tr_msg(fsm,0),		\
			    fsm->ops->act(ppp,FSM_TLF))

	/* Initialize Restart Counter */
#	define ACT_IRC(c,t) (fsm->restart = fsm->restart0 = (c), \
			     fsm->ms = (t))

	/* Initialize Restart Counter for Configure-Requests */
#	define ACT_IRC_CONF ACT_IRC(ppp->conf.max_conf,fsm->restart_ms)

	/* Initialize Restart Counter for Terminate-Requests */
#	define ACT_IRC_TERM ACT_IRC(ppp->conf.max_term,fsm->restart_ms)

	/* Zero Restart Counter */
#	define ACT_ZRC (fsm->restart = 0,		\
			fsm->ms = ppp->conf.stop_ms, SET_RTIMER())

	/* Send Configure Request */
#	define ACT_SCR (fsm->restart--, SET_RTIMER(), fsm->ops->scr(ppp))

	/* Send Configure Ack */
#	define ACT_SCA (fsm->ops->sca(ppp,fsm))

	/* Send Configure-Nak or Configure-Reject */
#	define ACT_SCN (++fsm->nak_sent, fsm->ops->scn(ppp,fsm))

	/* Send Terminate Request */
#	define ACT_STR (fsm->restart--, SET_RTIMER(), fsm->ops->str(ppp,fsm))

	/* Send Terminate Ack */
#	define ACT_STA (fsm->ops->sta(ppp,fsm))

	/* Send Code Reject */
#	define ACT_SCJ (fsm->ops->scj(ppp,fsm))

	/* Send Echo Reply */
#	define ACT_SER (fsm->ops->ser(ppp,fsm))


	enum fsm_state old_state = fsm->state;

	if (event > FSM_RXR) {
		fsm_bad_event(fsm,event);
		return;
	}
	log_debug(ev_names[event].lvl, fsm->name,
		  ev_names[event].pat,
		  ev_names[event].name,
		  fsm->restart0 - fsm->restart);

	switch (old_state) {
	case FSM_INITIAL_0:		/* Initial */
		switch (event) {
		case FSM_UP:
			fsm->state = FSM_CLOSED_2;
			break;
		case FSM_DOWN:		/* not in the RFC state machine */
			fsm_reinit(fsm);
			break;
		case FSM_OPEN:
			fsm->state = FSM_STARTING_1;
			ACT_TLS;
			break;
		case FSM_CLOSE:
			fsm_reinit(fsm);
			break;
		default:
			fsm_bad_event(fsm,event);
		}
		break;

	case FSM_STARTING_1:		/* Starting */
		switch (event) {
		case FSM_UP:
			ACT_IRC_CONF;
			ACT_SCR;
			fsm->state = FSM_REQ_SENT_6;
			break;
		case FSM_DOWN:		/* not in the RFC state machine */
			fsm->state = FSM_STARTING_1;
			break;
		case FSM_OPEN:
			fsm->state = FSM_STARTING_1;
			break;
		case FSM_CLOSE:
			fsm_reinit(fsm);
			break;
		default:
			fsm_bad_event(fsm,event);
		}
		break;

	case FSM_CLOSED_2:		/* Closed */
		switch (event) {
		case FSM_DOWN:
			fsm_reinit(fsm);
			break;
		case FSM_OPEN:
			ACT_IRC_CONF;
			ACT_SCR;
			fsm->state = FSM_REQ_SENT_6;
			break;
		case FSM_CLOSE:
			break;
		case FSM_RCR_P:
		case FSM_RCR_M:
		case FSM_RCA:
		case FSM_RCN:
		case FSM_RTR:
			ACT_STA;
			break;
		case FSM_RTA:
			set_tr_msg(fsm,0);
			break;
		case FSM_RUC:
			ACT_SCJ;
			break;
		case FSM_RXJ_P:
			break;
		case FSM_RXJ_M:
			ACT_TLF(FSM_CLOSED_2);
			break;
		case FSM_RXR:
			break;
		default:
			fsm_bad_event(fsm,event);
		}
		break;

	case FSM_STOPPED_3:		/* Stopped */
		switch (event) {
		case FSM_DOWN:
			fsm->state = FSM_STARTING_1;
			ACT_TLS;
			break;
		case FSM_OPEN:
			break;
		case FSM_CLOSE:
			fsm->state = FSM_CLOSED_2;
			break;
		case FSM_RCR_P:
			ACT_IRC_CONF;
			ACT_SCR;
			ACT_SCA;
			fsm->state = FSM_ACK_SENT_8;
			break;
		case FSM_RCR_M:
			ACT_IRC_CONF;
			ACT_SCR;
			ACT_SCN;
			fsm->state = FSM_REQ_SENT_6;
			break;
		case FSM_RCA:
		case FSM_RCN:
		case FSM_RTR:
			ACT_STA;
			break;
		case FSM_RTA:
			set_tr_msg(fsm,0);
			break;
		case FSM_RUC:
			ACT_SCJ;
			break;
		case FSM_RXJ_P:
			break;
		case FSM_RXJ_M:
			ACT_TLF(FSM_STOPPED_3);
			break;
		case FSM_RXR:
			break;
		default:
			fsm_bad_event(fsm,event);
		}
		break;

	case FSM_CLOSING_4:		/* Closing */
		switch (event) {
		case FSM_DOWN:
			fsm_reinit(fsm);
			CLR_TIMER();
			break;
		case FSM_OPEN:
			fsm->state = FSM_STOPPING_5;
			break;
		case FSM_CLOSE:
			break;
		case FSM_TO_P:
			ACT_STR;
			break;
		case FSM_TO_M:
			ACT_TLF(FSM_CLOSED_2);
			break;
		case FSM_RCR_P:
		case FSM_RCR_M:
		case FSM_RCA:
		case FSM_RCN:
			break;
		case FSM_RTR:
			ACT_STA;
			break;
		case FSM_RTA:
			ACT_TLF(FSM_CLOSED_2);
			break;
		case FSM_RUC:
			ACT_SCJ;
			break;
		case FSM_RXJ_P:
			break;
		case FSM_RXJ_M:
			ACT_TLF(FSM_CLOSED_2);
			break;
		case FSM_RXR:
			break;
		default:
			fsm_bad_event(fsm,event);
		}
		break;

	case FSM_STOPPING_5:		/* Stopping */
		switch (event) {
		case FSM_DOWN:
			fsm->state = FSM_STARTING_1;
			CLR_TIMER();
			break;
		case FSM_OPEN:
			break;
		case FSM_CLOSE:
			fsm->state = FSM_CLOSING_4;
			break;
		case FSM_TO_P:
			ACT_STR;
			break;
		case FSM_TO_M:
			ACT_TLF(FSM_STOPPED_3);
			break;
		case FSM_RCR_P:
		case FSM_RCR_M:
		case FSM_RCA:
		case FSM_RCN:
			break;
		case FSM_RTR:
			ACT_STA;
			break;
		case FSM_RTA:
			ACT_TLF(FSM_STOPPED_3);
			break;
		case FSM_RUC:
			ACT_SCJ;
			break;
		case FSM_RXJ_P:
			break;
		case FSM_RXJ_M:
			ACT_TLF(FSM_STOPPED_3);
			break;
		case FSM_RXR:
			break;
		default:
			fsm_bad_event(fsm,event);
		}
		break;

	case FSM_REQ_SENT_6:		/* Req-Sent */
		switch (event) {
		case FSM_DOWN:
			fsm->state = FSM_STARTING_1;
			CLR_TIMER();
			break;
		case FSM_OPEN:
			break;
		case FSM_CLOSE:
			ACT_IRC_TERM;
			ACT_STR;
			fsm->state = FSM_CLOSING_4;
			break;
		case FSM_TO_P:
			ACT_SCR;
			break;
		case FSM_TO_M:
			ACT_TLF(FSM_STOPPED_3);
			break;
		case FSM_RCR_P:
			ACT_SCA;
			fsm->state = FSM_ACK_SENT_8;
			break;
		case FSM_RCR_M:
			ACT_SCN;
			break;
		case FSM_RCA:
			ACT_IRC_CONF;
			fsm->state = FSM_ACK_RCVD_7;
			break;
		case FSM_RCN:
			ACT_IRC_CONF;
			ACT_SCR;
			break;
		case FSM_RTR:
			ACT_STA;
			fsm->state = FSM_REQ_SENT_6;
			break;
		case FSM_RTA:
			set_tr_msg(fsm,0);
			break;
		case FSM_RUC:
			ACT_SCJ;
			break;
		case FSM_RXJ_P:
			break;
		case FSM_RXJ_M:
			ACT_TLF(FSM_STOPPED_3);
			break;
		case FSM_RXR:
			break;
		default:
			fsm_bad_event(fsm,event);
		}
		break;

	case FSM_ACK_RCVD_7:		/* Ack-Rcvd */
		switch (event) {
		case FSM_DOWN:
			fsm->state = FSM_STARTING_1;
			CLR_TIMER();
			break;
		case FSM_OPEN:
			break;
		case FSM_CLOSE:
			ACT_IRC_TERM;
			ACT_STR;
			fsm->state = FSM_CLOSING_4;
			break;
		case FSM_TO_P:
			ACT_SCR;
			fsm->state = FSM_REQ_SENT_6;
			break;
		case FSM_TO_M:
			ACT_TLF(FSM_STOPPED_3);
			break;
		case FSM_RCR_P:
			ACT_SCA;
			fsm->state = FSM_OPENED_9;
			CLR_TIMER();
			ACT_TLU;
			break;
		case FSM_RCR_M:
			ACT_SCN;
			break;
		case FSM_RCA:
			ACT_SCR;
			break;
		case FSM_RCN:
			ACT_SCR;
			break;
		case FSM_RTR:
			ACT_STA;
			fsm->state = FSM_REQ_SENT_6;
			break;
		case FSM_RTA:
			set_tr_msg(fsm,0);
			break;
		case FSM_RUC:
			ACT_SCJ;
			fsm->state = FSM_ACK_RCVD_7;
			break;
		case FSM_RXJ_P:
			break;
		case FSM_RXJ_M:
			ACT_TLF(FSM_STOPPED_3);
			break;
		case FSM_RXR:
			break;
		default:
			fsm_bad_event(fsm,event);
		}
		break;

	case FSM_ACK_SENT_8:		/* Ack-Sent */
		switch (event) {
		case FSM_DOWN:
			fsm->state = FSM_STARTING_1;
			CLR_TIMER();
			break;
		case FSM_OPEN:
			break;
		case FSM_CLOSE:
			ACT_IRC_TERM;
			ACT_STR;
			fsm->state = FSM_CLOSING_4;
			break;
		case FSM_TO_P:
			ACT_SCR;
			break;
		case FSM_TO_M:
			ACT_TLF(FSM_STOPPED_3);
			break;
		case FSM_RCR_P:
			ACT_SCA;
			break;
		case FSM_RCR_M:
			ACT_SCN;
			fsm->state = FSM_REQ_SENT_6;
			break;
		case FSM_RCA:
			ACT_IRC_CONF;
			fsm->state = FSM_OPENED_9;
			CLR_TIMER();
			ACT_TLU;
			break;
		case FSM_RCN:
			ACT_IRC_CONF;
			ACT_SCR;
			break;
		case FSM_RTR:
			ACT_STA;
			fsm->state = FSM_REQ_SENT_6;
			break;
		case FSM_RTA:
			set_tr_msg(fsm,0);
			break;
		case FSM_RUC:
			ACT_SCJ;
			break;
		case FSM_RXJ_P:
			break;
		case FSM_RXJ_M:
			ACT_TLF(FSM_STOPPED_3);
			break;
		case FSM_RXR:
			break;
		default:
			fsm_bad_event(fsm,event);
		}
		break;

	case FSM_OPENED_9:		/* Opened */
		switch (event) {
		case FSM_DOWN:
			fsm->state = FSM_STARTING_1;
			CLR_TIMER();
			ACT_TLD;
			break;
		case FSM_OPEN:
			break;
		case FSM_CLOSE:
			ACT_IRC_TERM;
			ACT_STR;
			fsm->state = FSM_CLOSING_4;
			ACT_TLD;
			break;
		case FSM_RCR_P:
			ACT_TLD;
			ACT_SCR;
			ACT_SCA;
			fsm->state = FSM_ACK_SENT_8;
			break;
		case FSM_RCR_M:
			ACT_TLD;
			ACT_SCR;
			ACT_SCN;
			fsm->state = FSM_REQ_SENT_6;
			break;
		case FSM_RCA:
		case FSM_RCN:
			ACT_TLD;
			ACT_SCR;
			fsm->state = FSM_REQ_SENT_6;
			break;
		case FSM_RTR:
			ACT_ZRC;
			ACT_TLD;
			ACT_STA;
			fsm->state = FSM_STOPPING_5;
			break;
		case FSM_RTA:
			set_tr_msg(fsm,0);
			ACT_TLD;
			ACT_SCR;
			fsm->state = FSM_REQ_SENT_6;
			break;
		case FSM_RUC:
			ACT_SCJ;	/* change from new PPP draft */
			break;
		case FSM_RXJ_P:
			break;
		case FSM_RXJ_M:
			ACT_TLD;
			ACT_IRC_TERM;
			ACT_STR;
			fsm->state = FSM_STOPPING_5;
			break;
		case FSM_RXR:
			ACT_SER;
			break;
		default:
			fsm_bad_event(fsm,event);
		}
		break;
	}

	log_debug(3, fsm->name, "%s->%s",
		  fsm_state_name(old_state), fsm_state_name(fsm->state));

#undef SET_RTIMER
#undef ACT_TLU
#undef ACT_TLD
#undef ACT_TLS
#undef ACT_TLF
#undef ACT_IRC
#undef ACT_IRC_CONF
#undef ACT_IRC_TERM
#undef ACT_ZRC
#undef ACT_SCR
#undef ACT_SCA
#undef ACT_SCN
#undef ACT_STR
#undef ACT_STA
#undef ACT_SCJ
#undef ACT_SER
}


/* common code for checking PPP packet IDs
 */
int					/* 0=ok */
fsm_ck_id(struct fsm *fsm)
{
	if (IBUF_CP->id == fsm->id)
		return 0;

	log_debug(2,fsm->name,"ignore %s with ID %#x instead of %#x",
		  fsm_code_name(IBUF_CP->code),
		  IBUF_CP->id, fsm->id);
	return 1;
}


/* generic send a Configure-ACK
 */
void
fsm_sca(struct ppp *ppp,
	struct fsm *fsm)
{
	IBUF_CP->code = PPP_CODE_CA;
	log_debug(2,fsm->name,"send Configure-ACK ID=%#x", IBUF_CP->id);
	ppp_send(ppp,&ibuf,IBUF_CP->len);
	fsm->nak_sent = 0;
}


/* generic send a Configure-Nak or Configure-Reject
 */
void
fsm_scn(struct ppp *ppp,
	struct fsm *fsm)
{
	struct ppp_buf *buf;
	char *type;
	int len;


	if (JBUF_CP->len != 4) {
		buf = &rejbuf;
		len = JBUF_CP->len;
		buf->un.pkt.id = IBUF_CP->id;
		buf->un.pkt.code = PPP_CODE_CONFJ;
		type = "Configure-Reject";

	} else if (NBUF_CP->len != 4) {
		buf = &nakbuf;
		len = NBUF_CP->len;
		buf->un.pkt.id = IBUF_CP->id;
		if (fsm->nak_sent < ppp->conf.max_fail) {
			buf->un.pkt.code = PPP_CODE_CN;
			type = "Configure-NAK";
		} else {
			buf->un.pkt.code = PPP_CODE_CONFJ;
			type = "Configure-Reject instead of NAK";
		}
	} else {
		log_complain(fsm->name, "impossible fsm_scn() lengths of"
			     " %d and %d",
			     JBUF_CP->len, NBUF_CP->len);
		(void)fflush(stderr);
		abort();
	}

	buf->bits = ((buf->proto == PPP_IPCP || buf->proto == PPP_CCP)
		     ? mp_ncp_bits : 0);
	if (fsm->nak_sent <= ppp->conf.max_fail+2) {
		log_debug(2,fsm->name,"send %s ID=%#x",
			  type, buf->un.pkt.id);
		ppp_send(ppp,buf,len);
	} else {
		log_complain(fsm->name,"%d failures, Code-Reject instead",
			     fsm->nak_sent);
		fsm_scj(ppp,fsm);
	}

	if (buf->un.pkt.code != PPP_CODE_CN && buf->proto == PPP_LCP)
		lcp_send_ident(ppp, 0);
}


/* generic send a Terminate-Request
 */
void
fsm_str(struct ppp *ppp,
	struct fsm *fsm)
{
	obuf.proto = fsm->protocol;
	obuf.bits = ((obuf.proto == PPP_IPCP || obuf.proto == PPP_CCP)
		     ? mp_ncp_bits : 0);
	OBUF_CP->code = PPP_CODE_TR;
	OBUF_CP->id = ++fsm->id;
	OBUF_CP->len = 4+fsm->tr_msg_len;
	if (fsm->tr_msg_len != 0) {
		bcopy(fsm->tr_msg, 1+&OBUF_CP->len, fsm->tr_msg_len);
		obuf.un.info[4+fsm->tr_msg_len] = '\0';
		log_debug(2,fsm->name,"send Terminate-Request ID=%#x \"%s\"",
			  fsm->id, 1+&OBUF_CP->len);
	} else {
		log_debug(2,fsm->name,"send Terminate-Request ID=%#x",
			  fsm->id);
	}
	ppp_send(ppp,&obuf,OBUF_CP->len);
}


/* set Terminate-Request message
 *	The total length of the message must be less than TR_MSG_MAX
 */
void
set_tr_msg(struct fsm *fsm,
	   char *fmt, ...)
{
	va_list args;

	if (!fmt) {
		fsm->tr_msg_len = 0;
	} else {
		va_start(args, fmt);
		fsm->tr_msg_len = vsprintf(fsm->tr_msg, fmt, args);
		va_end(args);
	}
}


/* generic send a Terminate-Ack
 */
void
fsm_sta(struct ppp *ppp,
	struct fsm *fsm)
{
	obuf.proto = fsm->protocol;
	obuf.bits = ((obuf.proto == PPP_IPCP || obuf.proto == PPP_CCP)
		     ? mp_ncp_bits : 0);
	OBUF_CP->code = PPP_CODE_TA;
	OBUF_CP->id = IBUF_CP->id;
	OBUF_CP->len = 4;
	log_debug(2,fsm->name,"send Terminate-Ack");
	ppp_send(ppp,&obuf,OBUF_CP->len);
}


/* generic send a Code-Reject
 */
void
fsm_scj(struct ppp *ppp,
	struct fsm *fsm)
{
	obuf.proto = fsm->protocol;
	obuf.bits = ((obuf.proto == PPP_IPCP || obuf.proto == PPP_CCP)
		     ? mp_ncp_bits : 0);
	OBUF_CP->code = PPP_CODE_CJ;
	OBUF_CP->id = 0;
	OBUF_CP->len = MIN(ppp->lcp.neg_mtu, 4+ibuf_info_len);
	bcopy(ibuf.un.info, &OBUF_CP->ncp_un.cj, OBUF_CP->len-4);
	log_debug(2,fsm->name,"send Code-Reject");
	ppp_send(ppp,&obuf,OBUF_CP->len);
}


/* deal with bogus Code-Rejects
 */
void
fsm_rcj(struct ppp *ppp,
	struct fsm *fsm)
{
	if (ibuf_info_len < 5) {
		log_ipkt(0,fsm->name,"receive bogus Code-Reject:");
		fsm_run(ppp, fsm, FSM_RXJ_M);
		return;
	}

	log_complain(fsm->name, "receive unexpected %s for %s",
		     fsm_code_name(PPP_CODE_CJ),
		     fsm_code_name(IBUF_CP->ncp_un.cj.cj_code));
	fsm_run(ppp, fsm, FSM_RXJ_M);
}


/* generic send-an-echo-reply for NCPs that consider it an error
 */
void
fsm_ser(struct ppp *ppp,
	struct fsm *fsm)
{
	log_complain(fsm->name,"bogus Echo-Request received");
	fsm_scj(ppp,fsm);
}
