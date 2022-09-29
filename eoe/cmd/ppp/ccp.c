/* PPP Compression Control Protocol
 */

#ident "$Revision: 1.17 $"

#include <stropts.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "ppp.h"


static void ccp_scr(struct ppp*);
static void ccp_sca(struct ppp*, struct fsm*);
static void ccp_act(struct ppp*, enum fsm_action);

static struct fsm_ops ccp_ops = {
	ccp_scr,
	ccp_sca,
	fsm_scn,
	fsm_str,
	fsm_sta,
	fsm_scj,
	fsm_ser,
	ccp_act
};


#define C ppp->ccp
#define CNAME ppp->ccp.fsm.name
#define STRUCT_PKT struct ccp_pkt
#define STRUCT_CF struct ccp_cf
#define BASE_P(bufp) (&(bufp)->ccp_un.cf[0])

/* shape of a CCP packet */
STRUCT_PKT {
	u_char	code;
	u_char	id;
	u_short	len;
	union {
	    STRUCT_CF {			/* Configure-Req, Ack, Nak, Reject */
		u_char  type;
		u_char  len;
		union {
		    u_char  info[1];
		    struct {		/* common compression type */
			u_char  clen;
			u_char  copt;
		    } com;
		    struct {
			u_char  vers:3;
			u_char  bits:5;
		    } bsd;
		} cf_un;
	    } cf[1];
	    struct {			/* code reject */
		u_char  cj_code;
		u_char  cj_id;
		u_short cj_len;
		u_char  cj_info[1];
	    } cj;
	} ccp_un;
};


/* CCP Configuration Options */
#define CCP_CO_PRED1	1		/* Predictor type 1 */
#define CCP_CO_PRED1_L	2
#define CCP_CO_PRED2	2		/* Predictor type 2 */
#define CCP_CO_PJ	3		/* Puddle Jumper */
#define CCP_CO_HP	16		/* Hewlett-Packard PPC */
#define CCP_CO_STAC	17		/* Stac Electronics LZS */
#define CCP_CO_MSOFT	18		/* Microsoft PPC */
#define CCP_CO_GANDALF	19		/* Gandalf FZA */
#define CCP_CO_V42BIS	20		/* V.42bis compression */
#define CCP_CO_BSD	21		/* UNIX LZW Compress */
#define CCP_CO_BSD_L	3
#define CCP_CO_DEFLATE	26

static char *
ccp_name(u_char type)
{
	static char res[32];

	switch (type) {
	case CCP_CO_PRED1:
		return "Predictor Type 1";
	case CCP_CO_PRED2:
		return "Predictor Type 2";
	case CCP_CO_PJ:
		return "Puddle Jumper";
	case CCP_CO_HP:
		return "Hewlett-Packard PPC";
	case CCP_CO_STAC:
		return "Stac Electronics LZS";
	case CCP_CO_MSOFT:
		return "Microsoft PPC";
	case CCP_CO_GANDALF:
		return "Gandalf FZA";
	case CCP_CO_V42BIS:
		return "V.42bis";
	case CCP_CO_BSD:
		return "BSD Compress";
	case CCP_CO_DEFLATE:
		return "Deflate";
	default:
		sprintf(res,"CCP option #%#x", type);
		return res;
	}
}


/* get compression going
 */
void
ccp_init(struct ppp *ppp)
{
	C.fsm.protocol = PPP_CCP;
	C.fsm.ops = &ccp_ops;

	ccp_clear(ppp);

	fsm_init(&C.fsm);
}


/* Really tell the kernel
 */
static int				/* 0=failure, 1=ok */
ccp_tx_change(struct ppp *ppp, u_char new)
{
	struct ppp_arg arg;

	log_debug(6,CNAME, "SIOC_PPP_CCP_TX=%#x", new);

	bzero(&arg,sizeof(arg));
	arg.v.ccp.proto = new;
	arg.v.ccp.debug = (debug > 2);
	arg.v.ccp.bits = C.neg_tx_bsd_bits;
	arg.v.ccp.unit = ppp->dv.unit;
	return (0 <= do_strioctl(ppp, SIOC_PPP_CCP_TX,
				 &arg, "ioctl(CP TX change)"));
}


static int				/* 0=failure, 1=ok */
ccp_rx_change(struct ppp *ppp, u_char new)
{
	struct ppp_arg arg;

	log_debug(6,CNAME, "SIOC_PPP_CCP_RX=%#x", new);

	bzero(&arg,sizeof(arg));
	arg.v.ccp.proto = new;
	arg.v.ccp.debug = (debug > 2);
	arg.v.ccp.bits = C.neg_rx_bsd_bits;
	/* adjust the MRU for the protocol field */
	arg.v.ccp.mru = MAX(PPP_DEF_MRU,ppp->lcp.neg_mrru) + 2;
	arg.v.ccp.unit = ppp->dv.unit;
	return (0 <= do_strioctl(ppp, SIOC_PPP_CCP_RX,
				 &arg, "ioctl(_CCP_RX change)"));
}

/* Tell the kernel which protocol to use, to reset the dictionary,
 *	and to release input.
 */
static void
ccp_set_rx(struct ppp *ppp, u_char new, int lvl)
{
	if (new != C.neg_rx
	    || debug > lvl) {
		C.neg_rx = new;
		if (new & SIOC_PPP_CCP_PRED1) {
			log_debug(1,CNAME,"turn on/reset RX %s",
				  ccp_name(CCP_CO_PRED1));
		} else if (new & SIOC_PPP_CCP_BSD) {
			log_debug(1,CNAME,"turn on/reset RX %d bit %s",
				  C.neg_rx_bsd_bits, ccp_name(CCP_CO_BSD));
		} else {
			log_debug(1,CNAME,"turn off RX compression");
		}
	}
	(void)ccp_rx_change(ppp,new);
}


/* Tell the kernel which protocol to use and to reset the dictionary.
 *	Output is released with the Configure-ACK in ccp_sca().
 */
static void
ccp_set_tx(struct ppp *ppp, u_char new, int lvl)
{
	if (new != C.neg_tx
	    || debug > lvl) {
		C.neg_tx = new;
		if (new & SIOC_PPP_CCP_PRED1) {
			log_debug(1,CNAME,"turn on/reset TX %s",
				  ccp_name(CCP_CO_PRED1));
		} else if (new & SIOC_PPP_CCP_BSD) {
			log_debug(1,CNAME,"turn on/reset TX %d bit %s",
				  C.neg_tx_bsd_bits, ccp_name(CCP_CO_BSD));
		} else {
			log_debug(1,CNAME,"turn off TX compression");
		}
	}
	(void)ccp_tx_change(ppp,new);
}


void
ccp_clear(struct ppp *ppp)
{
	C.fsm.restart_ms = C.conf.restart_ms;
	C.fsm.restart_ms_lim = C.fsm.restart_ms;

	if (C.conf.max_rx_errors < 0)
		C.conf.max_rx_errors = 3;
	else if (C.conf.max_rx_errors == 0)
		C.conf.max_rx_errors = 1000;
	if (C.conf.max_tx_errors < 0)
		C.conf.max_tx_errors = 10;
	else if (C.conf.max_tx_errors == 0)
		C.conf.max_tx_errors = 1000;

	/* stay off if no compression is permitted */
	C.bad_peer = (C.conf.rx != 0 || C.conf.tx != 0) ? 0 : 1;

	C.funny_ack = 0;

	C.neg_tx = 0;

	C.neg_rx = 0;
	C.prev_rx = SIOC_PPP_CCP_PROTOS;
	C.neg_rx_bsd_bits = C.conf.rx_bsd_bits;
	C.rej_rx = 0;
	C.nak_rx = 0;
	C.skip_rx = 0;

	C.reset_ack_pending = 0;

	/* do not tell kernel about compression until the device is
	 * linked to the STREAMS mux
	 */
	if (ppp->dv.dev_index != -1) {
		(void)ccp_rx_change(ppp,0);
		(void)ccp_tx_change(ppp,0);
	}
}


/* give up because the peer cannot play
 */
static void
ccp_quit(struct ppp *ppp)
{
	if (!C.bad_peer) {
		C.bad_peer = 1;
		C.rej_rx = SIOC_PPP_CCP_PROTOS;
		ccp_event(ppp, FSM_CLOSE);
	}
}


/* Start compression machine now that the STREAMS modules are ready,
 *	including linking this stream to the mux.
 */
int					/* 0=failure, 1=ok */
ccp_go(struct ppp *l_ppp)
{
	struct ppp *ppp = mp_ppp ? mp_ppp : l_ppp;  /* MP state machine */
	int res = 1;


	/* All of this must wait until connected to the STREAMS mux.
	 * We cannot send a Configure-Ack until we are prepared to
	 * clear the compression dictionary.  We do not have a dictionary
	 * until linked to the mux.
	 */
	if (l_ppp->dv.dev_index == -1)
		return 1;

	if (!C.bad_peer) {
		if (C.fsm.state <= FSM_INITIAL_0)
			ccp_event(ppp, FSM_OPEN);
		if (C.fsm.state <= FSM_STARTING_1)
			ccp_event(ppp, FSM_UP);
	}

	/* release output */
	log_debug(6,CNAME, "SIOC_PPP_TX_OK=1");
	if (0 > do_strioctl_ok(l_ppp, SIOC_PPP_TX_OK,
			       1, "ioctl(ccp_go() TX_OK on)"))
		res = 0;

	if (ppp->ipcp.fsm.state >= FSM_ACK_SENT_8) {
		log_debug(6,CNAME, "SIOC_PPP_IP_TX_OK=1");
		if (0 > do_strioctl_ok(ppp, SIOC_PPP_IP_TX_OK,
				       1, "ioctl(ccp_go() IP_TX_OK on)"))
			res = 0;
	}

	return res;
}


/* Age the counts of compression problems
 */
void
ccp_blip_age(struct ppp* ppp)
{
	if (mp_ppp)
		ppp = mp_ppp;
	if (ppp) {
		C.cnts.rreq_rcvd_aged = C.cnts.rreq_rcvd_aged * 0.75;
		C.cnts.rreq_sent_aged = C.cnts.rreq_sent_aged * 0.75;
	}
}


/* Do something after a de-compression error.
 */
void
ccp_blip(struct ppp *ppp,
	 int force)			/* do it even if already doing it */
{
	if (!force) {
		if (C.fsm.state <= FSM_REQ_SENT_6)
			return;
		log_debug(1,CNAME, "blip for bad compressed packet");
	}

	/* If we recently complained to the peer about bad decompression,
	 * then do not worry.
	 */
	if (C.reset_ack_pending
	    && clk.tv_sec >= C.sent_reset_request
	    && clk.tv_sec < C.sent_reset_request+2) {
		log_debug(2,CNAME,"omitting redundant Reset-Request");
		return;
	}

	/* If the link has a bad error rate, then give up on CCP */
	if (++C.cnts.rreq_sent_aged >= C.conf.max_rx_errors) {
		log_complain(CNAME, "input error rate %.1f>%d,"
			     " total=%d; give up on CCP",
			     C.cnts.rreq_sent_aged,
			     C.conf.max_rx_errors,
			     C.cnts.rreq_sent);
		ccp_quit(ppp);
		return;
	}

	/* If the peer has rejected our Reset-Requests, then use the
	 * original blip-out-of-OPENED-state hack to ask that the
	 * compression dictionary be reset.
	 */
	if (C.seen_reset_rej) {
		/* Stop sending compressed packets,
		 * send a Configure-Request,
		 * and expect the same in response.
		 */
		ccp_set_rx(ppp,0,1);	/* release input */
		ccp_set_tx(ppp,0,1);	/* stop compressed output */

		log_debug(2, CNAME,"force %s->%s",
			  fsm_state_name(C.fsm.state),
			  fsm_state_name(FSM_CLOSED_2));
		C.fsm.state = FSM_CLOSED_2;
		ccp_event(ppp, FSM_OPEN);
		return;
	}

	/* send reset-request */
	OBUF_CP->code = PPP_CODE_RREQ;
	if (++C.fsm.id == PPP_CCP_DISARM_ID)
		++C.fsm.id;
	OBUF_CP->id = C.fsm.id;
	OBUF_CP->len = 4;
	obuf.proto = PPP_CCP;
	obuf.bits = mp_ncp_bits;
	log_debug(1,CNAME,"send Reset-Request ID=%#x", C.fsm.id);
	ppp_send(ppp,&obuf,OBUF_CP->len);

	C.sent_reset_request = clk.tv_sec;
	C.reset_ack_pending = 1;
	C.cnts.rreq_sent++;
}


/* process result from FSM
 */
static void
ccp_act(struct ppp *ppp,
	enum fsm_action act)
{
	log_debug(2, CNAME,"action %s", fsm_action_name(act));

	switch (act) {
	case FSM_TLU:
		break;

	case FSM_TLD:
		break;

	case FSM_TLS:
	case FSM_TLF:
		ccp_clear(ppp);
		break;
	}
}


/* Make something happen to the CCP machine.
 */
void
ccp_event(struct ppp *ppp,
	  enum fsm_event ev)
{
	switch (ev) {
	case FSM_UP:
		/* stay dead if we do not want to play */
		if (!C.bad_peer) {
			fsm_run(ppp,&C.fsm, ev);
		} else {
			log_debug(2, CNAME,
				  "all compression disabled so stay off");
		}
		break;

	case FSM_OPEN:
	case FSM_DOWN:
	case FSM_CLOSE:
		fsm_run(ppp,&C.fsm, ev);
		break;

	case FSM_TO_P:			/* the restart timer has expired */
		if (C.fsm.restart <= 0
		    || C.bad_peer)
			ev = FSM_TO_M;
		fsm_run(ppp,&C.fsm,ev);
		break;

	default:
		log_complain(CNAME,"unknown event #%d", ev);
	}
}


/* Parse a received Configure-Request.
 *	This concerns compressed packets we will transmit.
 */
static enum fsm_event
ccp_parse_cr(struct ppp *ppp)
{
	STRUCT_CF *icfp;
	STRUCT_CF *jcfp = BASE_P(JBUF_CP);
	STRUCT_CF *ncfp = BASE_P(NBUF_CP);
	int i;
#   define BAD_LEN(cond, L) {if (icfp->len cond (L)) {		\
	log_complain(CNAME, "   bad TX %s CR length=%d",	\
		     ccp_name(icfp->type), icfp->len);		\
	ccp_quit(ppp);						\
	return FSM_RUC;}}

	rejbuf.proto = PPP_CCP;
	JBUF_CP->len = 4;
	nakbuf.proto = PPP_CCP;
	NBUF_CP->len = 4;

	log_debug(2,CNAME,"receive Configure-Request ID=%#x", IBUF_CP->id);

	/* "all options are always negotiated at once" */
	C.new_tx = 0;

	/* Try the odd Ack of the first draft only once.
	 * After one attempt, Nak until one compression choice is offered.
	 */
	if (C.funny_ack != 0)
		C.funny_ack = 2;

	/* stop compressing output & clear dictionary */
	ccp_set_tx(ppp,0,1);

	for (icfp = BASE_P(IBUF_CP);
	     icfp < (STRUCT_CF*)&ibuf.un.info[ibuf_info_len];
	     ADV(icfp)) {
		/* forget entire packet if the length of one option is bad */
		if (icfp->len < 2) {
			log_complain(CNAME, "   bad Configure-Request"
				     " option length %#x",
				     icfp->len);
			ccp_quit(ppp);
			return FSM_RUC;
		}

		switch (icfp->type) {
		case CCP_CO_BSD:
			BAD_LEN(!=, CCP_CO_BSD_L);

			/* Reject old version or impossible code size
			 */
			i = icfp->cf_un.bsd.bits;
			if (icfp->cf_un.bsd.vers != BSD_VERS
			    || i < MIN_BSD_BITS) {
				log_complain(CNAME, "   bad %d bit, vers %d"
					     " TX BSD Compress",
					     i, icfp->cf_un.bsd.vers);
				C.rej_rx |= SIOC_PPP_CCP_BSD;
				break;
			}

			/* Reject BSD Compress if turned off.
			 * If we are doing a funny Ack, we will
			 * later decide to not send the Reject.
			 */
			if (!(C.conf.tx & SIOC_PPP_CCP_BSD))
				break;

			/* Reject BSD Compress if we have chosen otherwise.
			 */
			if (C.new_tx != 0)
				break;

			/* Because during incompressible data we
			 * cannot send CLEAR symbols, we cannot let
			 * the peer use a larger dictionary than
			 * we use.
			 */
			if (i > (int)C.conf.tx_bsd_bits) {
				ncfp->cf_un.bsd.bits = C.conf.tx_bsd_bits;
				ncfp->cf_un.bsd.vers = BSD_VERS;
				GEN_CO(ncfp, CCP_CO_BSD, bsd);
				log_debug(2,CNAME, "   ask for %d instead of"
					  " %d bit BSD compression",
					  C.conf.tx_bsd_bits, i);
				C.funny_ack = 2;
				continue;
			}

			/* We know this is the first usable choice.
			 * Accept the code length proposed by the peer
			 * if it is smaller than our preference.
			 */
			C.new_tx = SIOC_PPP_CCP_BSD;
			C.neg_tx_bsd_bits = C.conf.tx_bsd_bits;
			log_debug(2,CNAME,"   accept %d bit BSD compression",
				  icfp->cf_un.bsd.bits);

			/* Generate ordinary Ack when only 1 choice offered.
			 */
			if (ibuf_info_len == 3+4)
				continue;

			/* If we have previously sent a funny Ack,
			 * then do not do it again.
			 * If this turns out to be the only choice, we will
			 * Ack it.  Otherwise we must reject the alternatives.
			 */
			if (C.funny_ack != 0)
				continue;

			/* generate strange CCP Ack (from the 1st draft)
			 */
			C.funny_ack = 1;
			ncfp = BASE_P(NBUF_CP);
			ncfp->cf_un.bsd.bits = icfp->cf_un.bsd.bits;
			ncfp->cf_un.bsd.vers = BSD_VERS;
			GEN_CO(ncfp, CCP_CO_BSD, bsd);
			NBUF_CP->len = 4 + ((char*)ncfp
					    - (char*)BASE_P(NBUF_CP));
			continue;


		case CCP_CO_PRED1:
			BAD_LEN(!=,CCP_CO_PRED1_L);
			/* Reject Predictor if turned off.
			 * If we are doing a funny Ack, we will
			 * later decide to not send the Reject.
			 */
			if (!(C.conf.tx & SIOC_PPP_CCP_PRED1))
				break;

			/* Reject if we have chosen otherwise.
			 */
			if (C.new_tx != 0)
				break;

			/* We know this is the first usable choice.
			 */
			C.new_tx = SIOC_PPP_CCP_PRED1;
			log_debug(2, CNAME,"   accept Predictor 1");

			/* Generate ordinary Ack when only 1 choice offered.
			 */
			if (ibuf_info_len == 2+4)
				continue;

			/* If we have previously sent a funny Ack,
			 * then do not do it again.
			 * If this turns out to be the only choice, we will
			 * Ack it.  Otherwise we must reject the alternatives.
			 */
			if (C.funny_ack != 0)
				continue;

			/* Generate the peculiar CCP Ack (from the 1st draft)
			 */
			C.funny_ack = 1;
			ncfp = BASE_P(NBUF_CP);
			GEN_CO_BOOL(ncfp, CCP_CO_PRED1);
			NBUF_CP->len = 4 + ((char*)ncfp
					    - (char*)BASE_P(NBUF_CP));
			continue;
		}

		/* reject anything we do not like
		 */
		if (C.funny_ack == 1) {
			log_debug(2,CNAME,"   would have rejected TX %s",
				  ccp_name(icfp->type));
		} else {
			log_debug(2,CNAME,"   reject TX %s",
				  ccp_name(icfp->type));
			bcopy(icfp, jcfp, icfp->len);
			ADV(jcfp);
		}
	}

	if (C.funny_ack == 1)
		return FSM_RCR_P;

	NBUF_CP->len = 4+((char*)ncfp - (char*)BASE_P(NBUF_CP));
	JBUF_CP->len = 4+((char*)jcfp - (char*)BASE_P(JBUF_CP));

	return ((JBUF_CP->len == 4 && NBUF_CP->len == 4)
		? FSM_RCR_P
		: FSM_RCR_M);

#undef BAD_LEN
}


/* Parse a received Configure-Ack.
 *	This concerns received compressed packets.
 */
static void
ccp_parse_ca(struct ppp *ppp)
{
	STRUCT_CF *icfp;
	int i;
	u_char new_rx;
	int num_rx;
#   define BAD_LEN(cond, L) {if (icfp->len cond (L)) {		\
	log_complain(CNAME, "   bad RX %s CA length=%d",	\
		     ccp_name(icfp->type), icfp->len);		\
	ccp_quit(ppp);						\
	continue;}}


	log_debug(2,CNAME,"receive Configure-ACK ID=%#x", IBUF_CP->id);

	/* The other machine is supposed to send us a Configure-Ack
	 * containing exactly one of our unchanged Configure-Request
	 * options.  So insist on exactly one.
	 *
	 * Instead of not checking the contents of the option or
	 * insisting that it be identical, accept any changes
	 * if possible.
	 */

	/* "all options are always negotiated at once" */
	new_rx = 0;
	num_rx = 0;

	for (icfp = BASE_P(IBUF_CP);
	     icfp < (STRUCT_CF*)&ibuf.un.info[ibuf_info_len];
	     ADV(icfp)) {
		num_rx++;

		switch (icfp->type) {
		case CCP_CO_BSD:
			BAD_LEN(!=,CCP_CO_BSD_L);
			i = icfp->cf_un.bsd.bits;
			if (i > (int)C.conf.rx_bsd_bits
			    || i < MIN_BSD_BITS
			    || icfp->cf_un.bsd.vers != BSD_VERS) {
				/* It was not what we configure-requested.
				 * So give up.
				 */
				log_complain(CNAME,"   bad RX BSD compress"
					     " Configure-Ack code size=%d"
					     " vers=%d",
					     i, icfp->cf_un.bsd.vers);
				ccp_quit(ppp);
				continue;
			}
			C.neg_rx_bsd_bits = i;
			new_rx = SIOC_PPP_CCP_BSD;
			break;

		case CCP_CO_PRED1:
			BAD_LEN(!=,CCP_CO_PRED1_L);
			new_rx = SIOC_PPP_CCP_PRED1;
			break;

		default:
			log_complain(CNAME,"   bogus Configure-ACK for %s",
				     ccp_name(icfp->type));
			ccp_quit(ppp);
			break;
		}
	}

	/* there should be exactly one option in the packet */
	if (num_rx > 1 && !C.bad_peer) {
		log_complain(CNAME,"   wrong number of Configure-ACK options");
		ccp_quit(ppp);
	}

	if (C.bad_peer) {
		/* Give up and say no more about compression if the
		 * peer is an idiot.
		 */
		new_rx = 0;
	}
	C.prev_rx = new_rx;

	/* clear dictionary and release IP input */
	ccp_set_rx(ppp,new_rx,1);

#undef BAD_LEN
}


/* Parse a received Configure-Nak.
 *	This concerns compressed packets we might receive.
 */
static void
ccp_parse_cn(struct ppp *ppp)
{
	STRUCT_CF *icfp;
	int i;


	log_debug(2,CNAME,"receive Configure-NAK ID=%#x",
		  IBUF_CP->id);
	C.fsm.nak_recv++;

	for (icfp = BASE_P(IBUF_CP);
	     icfp < (STRUCT_CF*)&ibuf.un.info[ibuf_info_len];
	     ADV(icfp)) {
		/* forget entire packet if the length of one option is bad */
		if (icfp->len < 2) {
			log_complain(CNAME,"   bad Configure-Nak option "
				     "length %#x",
				     icfp->len);
			break;
		}

		switch (icfp->type) {
		case CCP_CO_BSD:
			i = icfp->cf_un.bsd.bits;
			if (i <= (int)C.conf.rx_bsd_bits
			    && i >= MIN_BSD_BITS
			    && icfp->cf_un.bsd.vers == BSD_VERS) {
				C.neg_rx_bsd_bits = i;
				log_debug(2,CNAME,"   peer wants"
					  " %d-bit RX BSD Compress",
					  i);
			} else {
				log_complain(CNAME,"   bad %d bit,"
					     " vers %d RX BSD compress",
					     i, icfp->cf_un.bsd.vers);
			}
			C.nak_rx |= SIOC_PPP_CCP_BSD;
			C.skip_rx &= ~SIOC_PPP_CCP_BSD;
			break;

		case CCP_CO_PRED1:
			/* There are no Predictor parameters to NAK.
			 * Maybe the peer wants us to pick one.
			 */
			log_debug(2,CNAME,"   peer refuses Predictor 1");
			C.nak_rx |= SIOC_PPP_CCP_PRED1;
			C.skip_rx &= ~SIOC_PPP_CCP_PRED1;
			break;

		default:
			log_complain(CNAME,
				     "   ignore Configure-Nak for RX %s",
				     ccp_name(icfp->type));
			ccp_quit(ppp);
			break;
		}
	}
}


/* Parse a received Configure-Reject
 *	This concerns compressed packets we won't be receiving after all.
 */
static void
ccp_parse_confj(struct ppp *ppp)
{
	STRUCT_CF *icfp;

	log_debug(2,CNAME,"receive Configure-Reject ID=%#x", IBUF_CP->id);
	C.fsm.nak_recv++;

	for (icfp = BASE_P(IBUF_CP);
	     icfp < (STRUCT_CF*)&ibuf.un.info[ibuf_info_len];
	     ADV(icfp)) {
		/* forget entire packet if the length of one option is bad */
		if (icfp->len < 2) {
			log_complain(CNAME, "   bad Configure-Reject"
				     " option length %#x", icfp->len);
			ccp_quit(ppp);
			break;
		}
		switch (icfp->type) {
		case CCP_CO_BSD:
			log_debug(2,CNAME,
				  "   peer is rejecting BSD Compress");
			C.rej_rx |= SIOC_PPP_CCP_BSD;
			continue;
		case CCP_CO_PRED1:
			log_debug(2,CNAME,"   peer is rejecting Predictor 1");
			C.rej_rx |= SIOC_PPP_CCP_PRED1;
			continue;
		default:
			log_complain(CNAME,
				     "   bogus Configure-Reject type %#x",
				     icfp->type);
			ccp_quit(ppp);
		}
	}

	/* If the peer is rejecting an empty request, then give up.
	 * If our request was not empty, then complain.
	 */
	if (!C.bad_peer
	    && icfp == BASE_P(IBUF_CP)) {
		if ((C.conf.rx & ~C.rej_rx & ~C.skip_rx & C.prev_rx) != 0)
			log_complain(CNAME, "   bogus peer Configure-Reject"
				     " is empty");
		ccp_quit(ppp);
	}
}


/* FSM action to send a Configure-Request
 */
static void
ccp_scr(struct ppp *ppp)
{
	struct ppp_arg arg;
	STRUCT_CF *p = BASE_P(OBUF_CP);
	u_char rx, rx_req;
	static char *reqs[] = {
		"none",
		"BSD Compress",
		"Predictor 1",
		"BSD Compress and Predictor 1"};


	/* If there are no compression protocols that we can receive,
	 * then send an empty Configure-Request.  This gets us to the
	 * Opened state so we can transmit compressed packets.
	 */

	if (++C.fsm.id == PPP_CCP_DISARM_ID)
		++C.fsm.id;

	rx_req = 0;
	rx = (C.conf.rx & ~C.rej_rx & ~C.skip_rx & C.prev_rx);
	if (0 != rx) {
		/* Tell the kernel to stop accepting data packets as soon as
		 * it sees the Configure-ACK.  This is necessary to ensure
		 * that we start running uncompressed data packets through
		 * the dictionary starting with the very first packet
		 * after the Configure-ACK.
		 */
		log_debug(6,CNAME,"SIOC_PPP_CCP_ARM_CA=%#x", C.fsm.id);
		bzero(&arg,sizeof(arg));
		arg.v.ccp_arm.id = C.fsm.id;
		(void)do_strioctl(ppp, SIOC_PPP_CCP_ARM_CA,
				  &arg, "ioctl(_CCP_ARM_CA)");

		/* Request the compression protocols we like.
		 */
		if (rx & SIOC_PPP_CCP_BSD) {
			p->cf_un.bsd.vers = BSD_VERS;
			p->cf_un.bsd.bits = C.neg_rx_bsd_bits;
			GEN_CO(p, CCP_CO_BSD, bsd);
			rx_req = 1;
			/* If we have heard a Nak for BSD Compress,
			 * then ask only for it.
			 */
			if (C.nak_rx & SIOC_PPP_CCP_BSD) {
				rx &= ~SIOC_PPP_CCP_PRED1;
				C.skip_rx |= SIOC_PPP_CCP_PRED1;
			}
		}

		if (rx & SIOC_PPP_CCP_PRED1) {
			GEN_CO_BOOL(p, CCP_CO_PRED1);
			rx_req |= 2;
		}
	}

	obuf.proto = PPP_CCP;
	obuf.bits = mp_ncp_bits;
	OBUF_CP->code = PPP_CODE_CR;
	OBUF_CP->id = C.fsm.id;
	OBUF_CP->len = 4+((char*)p - (char*)BASE_P(OBUF_CP));
	if (0 != (rx_req & 1))
		log_debug(2,CNAME,
			  "send Configure-Request ID=%#x for %d bit %s",
			  C.fsm.id,
			  C.neg_rx_bsd_bits,
			  reqs[rx_req]);
	else
		log_debug(2,CNAME,"send Configure-Request ID=%#x for %s",
			  C.fsm.id, reqs[rx_req]);
	ppp_send(ppp,&obuf,OBUF_CP->len);
}


/* process incoming CCP frame
 */
void
ccp_ipkt(struct ppp *ppp)
{
#define CK_ID() {if (fsm_ck_id(&C.fsm)) {	\
	C.cnts.bad_id++;			\
	return;					\
	}}

	/* forget it if everything is turned off or the peer is broken. */
	if (C.conf.rx == 0 && C.conf.tx == 0) {
		log_debug(2, CNAME, "Protocol-Reject %s"
			  " because all compression disabled",
			  fsm_code_name(IBUF_CP->code));
		lcp_pj(ppp,CNAME);
		return;
	}
	if (C.bad_peer) {
		lcp_pj(ppp,CNAME);
		return;
	}

	if (ibuf_info_len < (int)IBUF_CP->len) {
		log_complain(CNAME,
			     "dropping %s with %d bytes but claiming %d",
			     fsm_code_name(IBUF_CP->code),
			     ibuf_info_len,
			     IBUF_CP->len);
		C.cnts.bad_len++;	/* bad packet length */
		return;
	}

	if (ppp->phase != NET_PHASE) {
		log_debug(1,CNAME,"dropping %s because in %s, not %s phase",
			  fsm_code_name(IBUF_CP->code),
			  phase_name(ppp->phase), phase_name(NET_PHASE));
		return;
	}

	if (C.fsm.state <= FSM_STARTING_1) {
		log_debug(1,CNAME,"dropping %s packet because in %s",
			  fsm_code_name(IBUF_CP->code),
			  fsm_state_name(C.fsm.state));
		return;
	}

	switch (IBUF_CP->code) {
	case PPP_CODE_CR:		/* Configure-Request */
		/* When we receive a Configure-Request when things seemed
		 * to be fine, it can mean the peer does not understand
		 * Reset-Request, has lost compression synchronization,
		 * and wants us to reset our transmit-dictionary.
		 */
		fsm_run(ppp, &C.fsm, ccp_parse_cr(ppp));
		break;

	case PPP_CODE_CA:		/* Configure-Ack */
		CK_ID();
		C.fsm.nak_recv = 0;
		ccp_parse_ca(ppp);
		fsm_run(ppp, &C.fsm, FSM_RCA);
		break;

	case PPP_CODE_CN:		/* Configure-Nak */
		CK_ID();
		ccp_parse_cn(ppp);
		if (C.fsm.nak_recv > ppp->conf.max_fail) {
			log_complain(CNAME,"giving up after %d"
				     " Configure-NAKs or Rejects",
				     C.fsm.nak_recv);
			fsm_run(ppp, &C.fsm, FSM_RXJ_M);
		} else {
			fsm_run(ppp, &C.fsm, FSM_RCN);
		}
		break;

	case PPP_CODE_CONFJ:		/* Configure-Reject */
		CK_ID();
		ccp_parse_confj(ppp);
		fsm_run(ppp, &C.fsm, FSM_RCN);
		break;

	case PPP_CODE_TR:		/* Terminate-Request */
		log_ipkt(1,CNAME,"receive Terminate-Request:");
		/* If the peer has rejected all of our suggestions,
		 * then give up on CCP entirely.
		 */
		if ((C.conf.rx & ~C.rej_rx) == 0)
			ccp_quit(ppp);
		fsm_run(ppp, &C.fsm, FSM_RTR);
		break;

	case PPP_CODE_TA:		/* Terminate-Ack */
		log_ipkt(2,CNAME,"receive Terminate-Ack:");
		fsm_run(ppp, &C.fsm, FSM_RTA);
		break;

	case PPP_CODE_CJ:		/* Code-Reject */
		C.cnts.rcvd_code_rej++;
		/* If the peer is rejecting a reset request, simply
		 * remember not to send one again and then blip out of OPEN.
		 */
		if (ibuf_info_len > 4
		    && IBUF_CP->ccp_un.cj.cj_code == PPP_CODE_RREQ) {
			log_complain(CNAME,"receive %s for %s",
				     fsm_code_name(PPP_CODE_CJ),
				     fsm_code_name(PPP_CODE_RREQ));
			C.seen_reset_rej = 1;
			ccp_blip(ppp,1);
		} else {
			/* Otherwise turn CCP off */
			fsm_rcj(ppp, &C.fsm);
			log_complain(CNAME, "give up");
			ccp_quit(ppp);
		}
		break;

	case PPP_CODE_RREQ:
		C.cnts.rreq_rcvd++;
		if (C.fsm.state < FSM_REQ_SENT_6) {
			log_debug(2,CNAME,
				  "receive stray Reset-Request ID=%#x",
				  IBUF_CP->id);
			break;
		}
		log_debug(2,CNAME,"receive Reset-Request ID=%#x",
			  IBUF_CP->id);
		/* If the link has a bad error rate, then give up on CCP */
		if (IBUF_CP->id != C.rcvd_rreq_id
		    && ++C.cnts.rreq_rcvd_aged >= C.conf.max_tx_errors) {
			log_complain(CNAME, "output error rate %.1f>%d,"
				     " total=%d; give up on CCP",
				     C.cnts.rreq_rcvd_aged,
				     C.conf.max_tx_errors,
				     C.cnts.rreq_rcvd);
			ccp_quit(ppp);
			break;
		}
		/* If the link is ok, then stop IP output, reset dictionary,
		 * send Reset-Ack, and release IP output.
		 */
		(void)do_strioctl_ok(ppp, SIOC_PPP_IP_TX_OK, 0,
				     "ioctl(CCP RREQ IP_TX_OK off)");
		log_debug(2,CNAME,"send Reset-Ack ID=%#x", IBUF_CP->id);
		C.rcvd_rreq_id = IBUF_CP->id;
		IBUF_CP->code = PPP_CODE_RACK;
		ibuf.bits = mp_ncp_bits;
		ppp_send(ppp,&ibuf,IBUF_CP->len);
		ccp_set_tx(ppp,C.neg_tx,1);
		if (ppp->ipcp.fsm.state >= FSM_ACK_SENT_8)
			(void)do_strioctl_ok(ppp, SIOC_PPP_IP_TX_OK,1,
					     "CCP RREQ IP_TX_OK on)");
		break;

	case PPP_CODE_RACK:
		C.cnts.rack_rcvd++;
		if (C.fsm.state < FSM_REQ_SENT_6) {
			log_debug(1,CNAME,"receive stray Reset-Ack ID=%#x",
				  IBUF_CP->id);
			if (!ppp->in_stop)
				ccp_set_rx(ppp,C.neg_rx,1);
		} else {
			log_debug(2,CNAME,
				  (C.reset_ack_pending
				   ? "receive Reset-Ack ID=%#x"
				   : "receive redundant Reset-Ack ID=%#x"),
				  IBUF_CP->id);
			C.reset_ack_pending = 0;
			ccp_set_rx(ppp,C.neg_rx,1);
		}
		break;

	default:
		C.cnts.rcvd_bad++;
		log_complain(CNAME,"bad %s", fsm_code_name(IBUF_CP->code));
		fsm_run(ppp, &C.fsm, FSM_RUC);
		break;
	}
#undef CK_ID
}


/* Send a Configure-ACK, but for a funny Ack, use the NAK buffer since
 *	it is not the same as the previously received Configure-Request.
 */
/* ARGSUSED */
static void
ccp_sca(struct ppp *ppp,
	struct fsm *fsm)
{
	/* stop IP output.
	 */
	(void)do_strioctl_ok(ppp, SIOC_PPP_IP_TX_OK,
			     0, "ioctl(ccp_sca() IP_TX_OK off)");

	if (C.funny_ack == 1) {
		NBUF_CP->code = PPP_CODE_CA;
		NBUF_CP->id = IBUF_CP->id;
		nakbuf.bits = mp_ncp_bits;
		log_debug(2,CNAME,"send special Configure-ACK ID=%#x",
			  IBUF_CP->id);
		ppp_send(ppp,&nakbuf,NBUF_CP->len);
		C.fsm.nak_sent = 0;
	} else {
		fsm_sca(ppp, &C.fsm);
	}

	/* Restore output after starting to compress.
	 */
	ccp_set_tx(ppp,C.new_tx,1);
	if (ppp->ipcp.fsm.state >= FSM_ACK_SENT_8)
		(void)do_strioctl_ok(ppp, SIOC_PPP_IP_TX_OK,
				     1, "ioctl(ccp_sca() IP_TX_OK on)");
}
