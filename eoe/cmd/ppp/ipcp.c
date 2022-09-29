/* PPP Internet Protocol Control Protocol
 */

#ident "$Revision: 1.20 $"

#include <stropts.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "ppp.h"


static void ipcp_scr(struct ppp*);
static void ipcp_sca(struct ppp*, struct fsm*);
static void ipcp_act(struct ppp*, enum fsm_action);

static struct fsm_ops ipcp_ops = {
	ipcp_scr,
	ipcp_sca,
	fsm_scn,
	fsm_str,
	fsm_sta,
	fsm_scj,
	fsm_ser,
	ipcp_act
};


#define I ppp->ipcp
#define PNAME ppp->ipcp.fsm.name
#define STRUCT_PKT struct ipcp_pkt
#define STRUCT_CF struct ipcp_cf
#define BASE_P(bufp) (&(bufp)->cf[0])

/* shape of an IPCP packet */
STRUCT_PKT {
	u_char	code;
	u_char	id;
	u_short	len;
	STRUCT_CF {			/* Configure-Req, Ack, Nak, Reject */
	    u_char  type;
	    u_char  len;
	    union {
		u_char  info[1];
		struct {
		    u_char  proto[2];
		    u_char  vj_slots;
		    u_char  vj_compslot;
		} comp;
		u_char  addr[4];
		struct {
		    u_char  src[4];
		    u_char  dst[4];
		} addrs;
	    } cf_un;
	} cf[1];
};


/* IPCP Configuration Options */
#define IPCP_CO_ADDRS_T	1		/* Addresses */
#define	IPCP_CO_ADDRS_L	10
#define IPCP_CO_COMP_T	2		/* Compression-Protocol */
#define IPCP_CO_COMP_L	6
#define IPCP_CO_COMP_VJ	PPP_VJC_COMP
#define IPCP_CO_ADDR_T	3		/* Address */
#define	IPCP_CO_ADDR_L	6

static char *
ipcp_opt_name(u_int type)
{
	static char buf[32];

	switch (type) {
	case IPCP_CO_ADDRS_T:
		return "ADDRS";
	case IPCP_CO_COMP_T:
		return "COMP";
	case IPCP_CO_ADDR_T:
		return "ADDR";
	default:
		(void)sprintf(buf,"unknown #%#x",type);
		return buf;
	}
}


/* Convert an IP address to a string.
 *	One feature of this routine is that it has several static
 *	buffers so that it can be called more than once for a single
 *	printf().
 */
char*
ip2str(u_long ip_addr)
{
	static int i;
	static struct {
		char addr[20];
	} str[6];
	static struct in_addr addr;

	if (ip_addr == 0)
		return "0";

	i = (i+1) % (sizeof(str)/sizeof(str[0]));

	addr.s_addr = ip_addr;
	(void)strcpy(str[i].addr, inet_ntoa(addr));
	return str[i].addr;
}


/* check and accept an address negotiation from a Configure-Request or
 * a Configure-Nak.
 */
static int				/* 0=bad */
ck_host(struct ppp *ppp,
	char x,
	__uint32_t addr,
	char *ptype)
{
	int i;
	char *owner;
	struct ok_host *okp;
	struct sockaddr_in *h;
	int *defp;
	char *strp;


	if (x == 'r') {
		owner = "its";
		okp = remhost_nams;
		i = num_remhost_nams;
		h = &remhost;
		defp = &def_remhost;
		strp = remhost_str;
	} else {
		owner = "our";
		okp = lochost_nams;
		i = num_lochost_nams;
		h = &lochost;
		defp = &def_lochost;
		strp = lochost_str;
	}

	/* A zero address is never good.  It always means the peer
	 * does not know its address.  We always send an initial
	 * Configure-Request for an address, even if is 0, meaning
	 * "please tell me what address to use on my end."  Receiving
	 * 0 means that the peer did not hear our request for 0 or
	 * does not have any idea either.
	 */
	if (addr == 0) {
		if (I.fsm.nak_recv > 2 || I.fsm.nak_sent > 2)
			log_complain(PNAME,"peer does not know %s IP address",
				     owner);
		return 0;
	}

	if (*defp) {
		/* If we defaulted the address,
		 * then accept what the peer says.
		 */
		h->sin_addr.s_addr = addr;
#ifdef _HAVE_SIN_LEN
		h->sin_len = _SIN_ADDR_SIZE;
#endif
		h->sin_family = AF_INET;
		*defp = 0;
		(void)strcpy(strp, ip2str(addr));
		log_debug(2,PNAME, "   set %s address to %s from %s",
			  owner, strp, ptype);
		return 1;

	} else if (h->sin_addr.s_addr == 0) {
		/* Validate the address from peer against list of possibles.
		 */
		while (i > 0) {
			if (((okp->addr ^ addr) & okp->mask) == 0) {
				h->sin_addr.s_addr = addr;
#ifdef _HAVE_SIN_LEN
				h->sin_len = _SIN_ADDR_SIZE
#endif
				h->sin_family = AF_INET;
				*defp = 0;
				(void)strcpy(strp,  ip2str(addr));
				log_debug(2,PNAME,
					  "   set %s address to %s from %s",
					  owner, strp, ptype);
				return 1;
			}
			okp++;
			i--;
		}
	}

	if (h->sin_addr.s_addr == addr) {
		(void)strcpy(strp,  ip2str(addr));
		log_debug(2,PNAME,"   accept %s address %s from %s",
			  owner, strp, ptype);
		return 1;

	} else {
		if (h->sin_addr.s_addr == 0) {
			log_complain(PNAME,"   peer offering unacceptible %s"
				     " for %s address",
				     ip2str(addr), owner);
		} else {
			log_cd(I.fsm.nak_sent > 1,2,PNAME,
			       "   peer says %s instead of %s for %s address",
			       ip2str(addr), strp, owner);
		}
		return 0;
	}
}


/* set IP parameters
 */
void
ipcp_param(struct ppp *ppp)
{
	I.fsm.restart_ms = ppp->conf.restart_ms;
	I.fsm.restart_ms_lim = ppp->conf.restart_ms_lim;

	I.rx_comp = I.conf.rx_vj_comp;
	I.rx_slots = I.conf.rx_vj_slots;
	I.rx_compslot = I.conf.rx_vj_compslot;

	I.tx_slots = I.conf.tx_vj_slots;
	I.tx_compslot = I.conf.tx_vj_compslot;

	I.seen_addr_rej = 0;
#ifdef DO_ADDRS
	I.seen_addrs_rej = 0;
#endif
}


/* get IP going
 */
void
ipcp_init(struct ppp *ppp)
{
	I.fsm.protocol = PPP_IPCP;
	I.fsm.ops = &ipcp_ops;

	I.went_up = 0;

	ipcp_param(ppp);

	fsm_init(&I.fsm);
}


void
ipcp_go(struct ppp *l_ppp)
{
	struct ppp *ppp = mp_ppp ? mp_ppp : l_ppp;  /* MP state machine */

	(void)set_mp(l_ppp);

	if (ppp->ipcp.fsm.state <= FSM_INITIAL_0)
		ipcp_event(ppp,FSM_OPEN);
	if (ppp->ipcp.fsm.state <= FSM_STARTING_1)
		ipcp_event(ppp,FSM_UP);
}


/* See if IP should be turned on in the kernel.
 *	This kludge lets things be done after the IPCP FSM has
 *	entirely finished doing what it needs to do.
 */
static void
ck_up(struct ppp *l_ppp)
{
	struct ppp *ppp = mp_ppp ? mp_ppp : l_ppp;  /* MP state machine */
	char str[100];
	int lvl;

	if (I.went_up == 1) {
		I.went_up = 2;

		(void)sprintf(str, "rx_vj_comp=%c,tx=%c "
			      "rx_compslot=%c,tx=%c rx_slots=%d,tx=%d",
			      ppp->ipcp.rx_comp ? 'y' : 'n',
			      ppp->ipcp.tx_comp ? 'y' : 'n',
			      ppp->ipcp.rx_compslot ? 'y' : 'n',
			      ppp->ipcp.tx_compslot ? 'y' : 'n',
			      ppp->ipcp.rx_slots, ppp->ipcp.tx_slots);

		/* always display the message for the first link of a bundle
		 */
		if (interact && numdevs == 1)
			lvl = 0;
		else
			lvl = 1;
		log_debug(lvl, PNAME,
			  debug ? "ready %s to %s, %s" : "ready %s to %s",
			  lochost_str, remhost_str, str);
	}

	if (link_mux(l_ppp) < 0
	    || !ccp_go(l_ppp)) {	/* tell kernel to release IP */
		ipcp_event(ppp,FSM_CLOSE);  /* die if sick */
		set_tr_msg(&l_ppp->lcp.fsm, "IP system failure");
		lcp_event(l_ppp,FSM_CLOSE);

	} else {
		/* we now know the IP address, so update utmp.
		 */
		dologin(l_ppp);
	}
}


/* process result from FSM
 */
static void
ipcp_act(struct ppp *ppp,
	 enum fsm_action act)
{
	log_debug(2, PNAME,"action %s", fsm_action_name(act));

	switch (act) {
	case FSM_TLU:
		I.went_up = 1;
		break;

	case FSM_TLD:
		if (I.went_up == 1)
			I.went_up = 0;

		/* stop ordinary IP traffic for IPCP */
		(void)do_strioctl_ok(ppp, SIOC_PPP_IP_TX_OK,
				     0,"ioctl(IPCP TLD IP_TX_OK off)");
		break;

	case FSM_TLS:
		break;

	case FSM_TLF:
		ipcp_event(ppp,FSM_CLOSE);
		lcp_event(ppp,FSM_CLOSE);
		break;
	}
}


/* Make something happen to the IPCP machine.
 */
void
ipcp_event(struct ppp *l_ppp,
	   enum fsm_event ev)
{
	struct ppp *ppp = mp_ppp ? mp_ppp : l_ppp;  /* MP state machine */

	switch (ev) {
	case FSM_UP:			/* authentication finished */
	case FSM_DOWN:			/* LCP down */
	case FSM_OPEN:			/* start work */
	case FSM_CLOSE:			/* start to shut down */
		fsm_run(ppp,&I.fsm, ev);
		break;

	case FSM_TO_P:			/* the restart timer has expired */
		if (I.fsm.restart <= 0)
			ev = FSM_TO_M;
		fsm_run(ppp,&I.fsm,ev);
		break;

	default:
		log_complain(PNAME,"unknown event #%d", ev);
	}
}


/* parse a received Configure-Request
 */
static enum fsm_event
ipcp_parse_cr(struct ppp *ppp)
{
	STRUCT_CF *icfp;
	STRUCT_CF *jcfp = BASE_P(JBUF_CP);
	STRUCT_CF *ncfp = BASE_P(NBUF_CP);
	u_int m, n;
#ifdef DO_ADDRS
	char abuf[sizeof("its=123.123.123.123,our=123.123.123.123")];
#endif
	int seen_vj = 0;
	int nak_addr = 0;
	int complained;
#   define BAD_LEN(cond, L) {if (icfp->len cond (L)) {			    \
	log_complain(PNAME, "   reject %s CR option with strange length=%d",\
		     ipcp_opt_name(icfp->type), icfp->len);		    \
	complained = 1;							    \
	break;}}


	log_debug(2,PNAME,"receive Configure-Request ID=%#x", IBUF_CP->id);

	/* "all options are always negotiated at once," so a missing
	 * option implies the default
	 */
	I.tx_comp = 0;

	for (icfp = BASE_P(IBUF_CP);
	     icfp < (STRUCT_CF*)&ibuf.un.info[ibuf_info_len];
	     ADV(icfp)) {
		/* forget entire packet if the length of one option is bad */
		if (icfp->len < 2) {
			log_complain(PNAME,"   bad Configure-Request option"
				     " length %#x", icfp->len);
			return FSM_RUC;
		}

		complained = 0;
		switch (icfp->type) {
		case IPCP_CO_COMP_T:
			seen_vj = 1;
			BAD_LEN(!=, IPCP_CO_COMP_L);
			m = GET_S(icfp,comp.proto);
			if (m != IPCP_CO_COMP_VJ) {
				log_cd(I.fsm.nak_sent > 1,2,PNAME,
				       "   reject header compression type %#x",
				       m);
				complained = 1;
				break;
			}
			/* Reject unknown compressions or VJ if off.
			 * Limit number of slots.
			 */
			if (!I.conf.tx_vj_comp) {
				log_cd(I.fsm.nak_sent > 1,2,PNAME,
				       "   reject VJ header compression"
				       " since it is off");
				complained = 1;
				break;
			}

			/* We are always willing to turn off slot #
			 * compression.  Since we do not have to send
			 * compressed slot IDs even if the peer wants
			 * us to, do not bother disagreeing if it wants
			 * them to receive them and we do not want to
			 * send them.
			 */
			if (icfp->cf_un.comp.vj_compslot != 1)
				I.tx_compslot = 0;

			/* Do not worry about increasing the number of
			 * slots, since we do not need to send as many
			 * as the peer wants.  Refuse to let them be reduced
			 * below the minimum the kernel code can handle.
			 */
			n = icfp->cf_un.comp.vj_slots+1;
			if (n < MIN_VJ_SLOT) {
				log_cd(I.fsm.nak_sent > 1,2,PNAME,
				       "   instead of %d,"
				       " Nak for %d slot VJ compression"
				       " %s compressed slot IDs",
				       n,
				       MIN_VJ_SLOT,
				       I.tx_compslot ? "with" : "without");
				n = MIN_VJ_SLOT;
				ADD_CO_SHORT(ncfp,IPCP_CO_COMP_VJ,comp.proto);
				ncfp->cf_un.comp.vj_slots = n-1;
				ncfp->cf_un.comp.vj_compslot = I.tx_compslot;
				GEN_CO(ncfp, IPCP_CO_COMP_T, comp);
			} else if (n == 17) {
				/* Previous versions of this code were off
				 * by one, and talked about 17 slots when
				 * they meant 16.  Actually sending them
				 * 17-slots causes a black hole for those
				 * TCP connections that win slot 16.
				 */
				n = 16;
				log_debug(2,PNAME, "   accept"
					  " 17 slot VJ header compression"
					  " (but use 16)"
					  " %s compressed slot IDs",
					  I.tx_compslot ? "with" : "without");
			} else {
				log_debug(2,PNAME, "   accept"
					  " %d slot VJ header compression"
					  " %s compressed slot IDs",
					  n,
					  I.tx_compslot ? "with" : "without");
			}
			I.tx_comp = 1;
			I.tx_slots = n;
			continue;
			
#ifdef DO_ADDRS
		case IPCP_CO_ADDRS_T:
			BAD_LEN(!=, IPCP_CO_ADDRS_L);
			m = GET_L(icfp,addrs.src);
			n = GET_L(icfp,addrs.dst);
			/* If he does not know his address, give him
			 * our best guess.  If we have no idea, then Nak(0)
			 * regardless of the standard, since the only
			 * alternative is to kill the link.  We cannot run
			 * without both IP addresses.
			 */
			if (!ck_host(ppp,'r',m,"ADDRS Request")
			    || !ck_host(ppp,'l',n,"ADDRS Request")
			    || remhost.sin_addr.s_addr != m
			    || lochost.sin_addr.s_addr != n) {
				ADD_CO_LONG(ncfp, remhost.sin_addr.s_addr,
					    addrs.src);
				ADD_CO_LONG(ncfp, lochost.sin_addr.s_addr,
					    addrs.dst);
				GEN_CO(ncfp, IPCP_CO_ADDRS_T, addrs);
				(void)sprintf(abuf,"its=%s,our=%s",
					      ip2str(m),ip2str(n));
				log_debug(2,PNAME,
					  "   Nak ADDRS %s with"
					  " its=%s,our=%s",
					  abuf, remhost_str, lochost_str);
				nak_addr = 1;
			}
			continue;
#endif

		case IPCP_CO_ADDR_T:
			BAD_LEN(!=, IPCP_CO_ADDR_L);
			m = GET_L(icfp,addr);
			/* If he does not know his address, give him
			 * our best guess.  If we have no idea, then Nak(0)
			 * regardless of the standard, since the only
			 * alternative is to kill the link.  We cannot run
			 * without both IP addresses.
			 */
			if (!ck_host(ppp,'r',m,"ADDR Request")
			    || remhost.sin_addr.s_addr != m) {
				ADD_CO_LONG(ncfp, remhost.sin_addr.s_addr,
					    addr);
				GEN_CO(ncfp, IPCP_CO_ADDR_T, addr);
				log_debug(2,PNAME,"   Nak %s with %s",
					  ip2str(m),
					  ip2str(remhost.sin_addr.s_addr));
				nak_addr = 1;
			}
			continue;
		}

		/* Reject an option we do not understand.
		 */
		bcopy(icfp, jcfp, icfp->len);
		ADV(jcfp);
		if (!complained)
			log_cd(I.fsm.nak_sent > 1, 2,PNAME,
			       "   reject %s CR option",
			       ipcp_opt_name(icfp->type));
	}

	/* If we do not know the remote IP address, and have not already
	 * added a query to our reply,
	 * or if we have defaulted the address and have peer has ,
	 * then add a query.
	 *
	 * Use the old protocol if the peer has rejected the new one.
	 * If both protocols have been rejected, then use the new one,
	 * since we cannot run without both IP addresses.
	 */
	if (!nak_addr) {
#ifdef DO_ADDRS
		if (I.seen_addr_rej && !I.seen_addrs_rej) {
			if (remhost.sin_addr.s_addr == 0
			    || lochost.sin_addr.s_addr == 0
			    || (I.fsm.nak_sent < ppp->conf.max_fail-2
				&& (def_remhost || def_lochost))) {
				ADD_CO_LONG(ncfp, remhost.sin_addr.s_addr,
					    addrs.src);
				ADD_CO_LONG(ncfp, lochost.sin_addr.s_addr,
					    addrs.dst);
				GEN_CO(ncfp, IPCP_CO_ADDRS_T, addrs);
				log_debug(2,PNAME,
					  "   Nak ADDRS its=%s,our=%s",
					  ip2str(remhost.sin_addr.s_addr),
					  ip2str(lochost.sin_addr.s_addr));
			}
		} else {
#endif
			if (remhost.sin_addr.s_addr == 0
			    || (I.fsm.nak_sent < ppp->conf.max_fail-2
				&& !I.seen_addr_rej
				&& def_remhost)) {
				ADD_CO_LONG(ncfp, 0, addr);
				GEN_CO(ncfp, IPCP_CO_ADDR_T, addr);
				log_debug(2,PNAME,"   Nak ADDR 0");
#ifdef DO_ADDRS
			}
#endif
		}
	}

	/* If sending a Nak for other reasons, then include a request
	 * for VJ header compression.
	 */
	if (I.conf.tx_vj_comp
	    && !seen_vj
	    && ncfp != BASE_P(NBUF_CP)) {
		ADD_CO_SHORT(ncfp,IPCP_CO_COMP_VJ,comp.proto);
		ncfp->cf_un.comp.vj_slots = I.tx_slots-1;
		ncfp->cf_un.comp.vj_compslot = I.tx_compslot;
		GEN_CO(ncfp, IPCP_CO_COMP_T, comp);
		log_debug(2,PNAME, "   Nak for %d slot VJ compression"
			  " %s compressed slot IDs",
			  I.tx_slots,
			  I.tx_compslot ? "with" : "without");
	}

	rejbuf.proto = PPP_IPCP;
	JBUF_CP->len = 4+((char*)jcfp - (char*)BASE_P(JBUF_CP));

	nakbuf.proto = PPP_IPCP;
	NBUF_CP->len = 4+((char*)ncfp - (char*)BASE_P(NBUF_CP));

	return ((JBUF_CP->len == 4 && NBUF_CP->len == 4)
		? FSM_RCR_P
		: FSM_RCR_M);

#undef BAD_LEN
}


/* parse a received Configure-Nak
 */
static void
ipcp_parse_cn(struct ppp *ppp)
{
	STRUCT_CF *icfp;
	int i;
	u_long m;
#   define BAD_LEN(cond,L) {if (icfp->len cond (L)) {			\
	log_complain(PNAME, "   ignore %s Nak with strange length=%d",	\
		     ipcp_opt_name(icfp->type), icfp->len);		\
	break;}}


	log_debug(2,PNAME,"receive Configure-NAK ID=%#x", IBUF_CP->id);
	I.fsm.nak_recv++;

	for (icfp = BASE_P(IBUF_CP);
	     icfp < (STRUCT_CF*)&ibuf.un.info[ibuf_info_len];
	     ADV(icfp)) {
		/* forget entire packet if the length of one option is bad */
		if (icfp->len < 2) {
			log_complain(PNAME,"   bad Configure-Nak option"
				     " length %#x", icfp->len);
			break;
		}

		switch (icfp->type) {
#ifdef DO_ADDRS
		case IPCP_CO_ADDRS_T:
			/* If we do not know his address,
			 * then take his answer.
			 */
			BAD_LEN(!=,IPCP_CO_ADDRS_L);
			(void)ck_host(ppp,'l',GET_L(icfp,addrs.src),
				      "ADDRS Nak");
			(void)ck_host(ppp,'r',GET_L(icfp,addrs.dst),
				      "ADDRS Nak");
			break;
#endif

		case IPCP_CO_ADDR_T:
			BAD_LEN(!=,IPCP_CO_ADDR_L);
			(void)ck_host(ppp,'l',GET_L(icfp,addr), "ADDR Nak");
			break;

		case IPCP_CO_COMP_T:
			BAD_LEN(!=,IPCP_CO_COMP_L);
			m = GET_S(icfp,comp.proto);
			if (m != IPCP_CO_COMP_VJ) {
				log_complain(PNAME,"   peer wants to send"
					     " unknown header compression"
					     " type %#x", m);
				I.rx_comp = 0;
				continue;
			}
			if (!I.conf.rx_vj_comp) {
				log_debug(2,PNAME,
					  "   ignore VJ compression;"
					  " turned off");
				break;
			}
			m = 0;
			i = icfp->cf_un.comp.vj_compslot;
			if (i != 0 && !I.rx_compslot) {
				log_cd(I.fsm.nak_sent > 1,2,PNAME,
				       "   peer demands"
				       " compressed VJ slot IDs");
				m++;
			} else if (i == 0 && I.rx_compslot) {
				log_debug(2,PNAME,
					  "   peer refuses"
					  " compressed VJ slot IDs");
				I.rx_compslot = 0;
				m++;
			}
			i = icfp->cf_un.comp.vj_slots+1;
			if (i < MIN_VJ_SLOT
			    || i > I.conf.rx_vj_slots) {
				I.rx_slots = ((i < MIN_VJ_SLOT)
					      ? MIN_VJ_SLOT
					      : I.conf.rx_vj_slots);
				log_complain(PNAME,
					     "   peer wants to send"
					     " %d VJ slots; try %d",
					     i, I.rx_slots);
				m++;
			} else if (i != I.rx_slots) {
				log_debug(2,PNAME, "   peer wants"
					  " %d VJ compression slots",
					  i);
				I.rx_slots = i;
				m++;
			}

			if (!m)
				log_complain(PNAME,"   vacuous COMP"
					     "configure Nak");
			if (I.fsm.nak_recv > 2) {
				log_debug(2,PNAME,"   too many COMP"
					  "Naks--give up");
				I.rx_comp = 0;
			}
			break;

		default:
			log_complain(PNAME, "   ignore %s Nak",
				     ipcp_opt_name(icfp->type));
			break;
		}
	}
#undef BAD_LEN
}


/* parse a received Configure-Reject
 */
static void
ipcp_parse_confj(struct ppp *ppp)
{
	STRUCT_CF *icfp;


	log_debug(2,PNAME,"receive Configure-Reject ID=%#x", IBUF_CP->id);
	I.fsm.nak_recv++;

	for (icfp = BASE_P(IBUF_CP);
	     icfp < (STRUCT_CF*)&ibuf.un.info[ibuf_info_len];
	     ADV(icfp)) {
		/* forget entire packet if the length of one option is bad */
		if (icfp->len < 2) {
			log_complain(PNAME,
				     "   bad Configure-Reject option length"
				     " %#x", icfp->len);
			break;
		}
		switch (icfp->type) {
#ifdef DO_ADDRS
		case IPCP_CO_ADDRS_T:
			log_debug(2,PNAME,"   peer is rejecting ADDRS");
			I.seen_addrs_rej = 1;
			break;
#endif

		case IPCP_CO_COMP_T:
			log_debug(2,PNAME,"   peer is rejecting header "
				  "compression");
			I.rx_comp = 0;
			break;

		case IPCP_CO_ADDR_T:
			log_debug(2,PNAME,"   peer is rejecting ADDR option");
			I.seen_addr_rej = 1;
			break;

		default:
			log_complain(PNAME,
				     "   bogus Configure-Reject type %#x",
				     icfp->type);
			break;
		}
	}
}


/* FSM action to send a Configure-Ack
 */
static void
ipcp_sca(struct ppp *ppp,
	 struct fsm *fsm)
{
	/* do not worry about MP bits before first bringing the link up
	 */
	if (ppp->dv.devfd >= 0)
		ibuf.bits = 0;
	fsm_sca(ppp,fsm);
}


/* FSM action to send a Configure-Request
 */
static void
ipcp_scr(struct ppp *ppp)
{
	STRUCT_CF *p = BASE_P(OBUF_CP);

	I.fsm.id++;
	log_debug(2,PNAME,"send Configure-Request ID=%#x", I.fsm.id);

	if (I.rx_comp) {
		ADD_CO_SHORT(p, IPCP_CO_COMP_VJ, comp.proto);
		p->cf_un.comp.vj_compslot = I.rx_compslot;
		p->cf_un.comp.vj_slots = I.rx_slots-1;
		GEN_CO(p, IPCP_CO_COMP_T, comp);
		log_debug(2,PNAME,"   %d slot VJ compression %s"
			  " compressed slot IDs",
			  I.rx_slots,
			  I.rx_compslot ? "with" : "without");
	}

	/* If we do not know both IP numbers,
	 *	then always send one kind or the other request.
	 * If we have not seen a rejection of the new kind,
	 *	then send the new kind of request, to detect
	 *	misconfigurations.
	 * If we have seen a rejection of the old kind and do not have a
	 *	local address, then send a new kind of request.
	 * If we have seen a rejection of the new kind,
	 *	and we already know both addresses, then do not worry.
	 */
	if (I.conf.no_addr > 0
	    && remhost.sin_addr.s_addr != 0
	    && lochost.sin_addr.s_addr != 0) {
		;			/* use default if no negotiating */
	} else if (!I.seen_addr_rej
#ifdef DO_ADDRS
		   || (I.seen_addrs_rej && lochost.sin_addr.s_addr == 0)
#else
		   || lochost.sin_addr.s_addr == 0
#endif
		   ) {
		ADD_CO_LONG(p, lochost.sin_addr.s_addr, addr);
		GEN_CO(p, IPCP_CO_ADDR_T, addr);
		log_debug(2,PNAME,"   ADDR our address %s",
			  lochost_str);
#ifdef DO_ADDRS
	} else if (!I.seen_addrs_rej
		   && (remhost.sin_addr.s_addr == 0
		       || lochost.sin_addr.s_addr == 0)) {
		ADD_CO_LONG(p, lochost.sin_addr.s_addr,
			    addrs.src);
		ADD_CO_LONG(p, remhost.sin_addr.s_addr,
			    addrs.dst);
		GEN_CO(p, IPCP_CO_ADDRS_T, addrs);
		log_debug(2,PNAME,"   ADDRS: our=%s,its=%s",
			  lochost_str, remhost_str);
#endif
	}

	obuf.proto = PPP_IPCP;
	obuf.bits = mp_ncp_bits;
	OBUF_CP->code = PPP_CODE_CR;
	OBUF_CP->id = I.fsm.id;
	OBUF_CP->len = 4+((char*)p - (char*)BASE_P(OBUF_CP));
	ppp_send(ppp,&obuf,OBUF_CP->len);
}


/* process incoming IPCP frame
 */
void
ipcp_ipkt(struct ppp *l_ppp)
{
	/* change the ID after recognizing it, to trash extras */
#define CK_ID() {if (fsm_ck_id(&I.fsm)) {	\
	ppp->ipcp.cnts.bad_id++;		\
	return;					\
	}}

	struct ppp *ppp = mp_ppp ? mp_ppp : l_ppp;  /* MP state machine */


	if (ibuf_info_len < (int)IBUF_CP->len) {
		log_complain(PNAME,
			     "dropping %s with %d bytes but claiming %d",
			     fsm_code_name(IBUF_CP->code),
			     ibuf_info_len, IBUF_CP->len);
		ppp->ipcp.cnts.bad_len++;   /* bad packet length */
		return;
	}

	if (ppp->phase != NET_PHASE) {
		log_debug(2,PNAME, "discard %s because in %s, not %s phase",
			  fsm_code_name(IBUF_CP->code),
			  phase_name(ppp->phase), phase_name(NET_PHASE));
		return;
	}

	switch (IBUF_CP->code) {
	case PPP_CODE_CR:		/* Configure-Request */
		fsm_run(ppp, &I.fsm, ipcp_parse_cr(ppp));
		break;

	case PPP_CODE_CA:		/* Configure-Ack */
		CK_ID();
		log_debug(2,PNAME,"receive Configure-Ack ID=%#x",IBUF_CP->id);
		I.fsm.nak_recv = 0;
		if (lochost.sin_addr.s_addr == 0) {
			log_complain(PNAME,
				     "   peer did not tell us our IP address;"
				     " ignore the Configure-Ack");
			break;
		}
		def_lochost = 0;
		fsm_run(ppp, &I.fsm, FSM_RCA);
		break;

	case PPP_CODE_CN:		/* Configure-Nak */
		CK_ID();
		ipcp_parse_cn(ppp);
		if (I.fsm.nak_recv > ppp->conf.max_fail) {
			log_complain(PNAME,"giving after %d Configure-NAKs",
				     I.fsm.nak_recv);
			fsm_run(ppp, &I.fsm, FSM_RXJ_M);
		} else {
			fsm_run(ppp, &I.fsm, FSM_RCN);
		}
		break;

	case PPP_CODE_CONFJ:		/* Configure-Reject */
		CK_ID();
		ipcp_parse_confj(ppp);
		if (I.fsm.nak_recv > ppp->conf.max_fail) {
			log_complain(PNAME,"giving after %d Configure-NAKs",
				     I.fsm.nak_recv);
			fsm_run(ppp, &I.fsm, FSM_RXJ_M);
		} else {
			fsm_run(ppp, &I.fsm, FSM_RCN);
		}
		break;

	case PPP_CODE_TR:		/* Terminate-Request */
		log_ipkt(1,PNAME,"receive Terminate-Request:");
		fsm_run(ppp, &I.fsm, FSM_RTR);
		break;

	case PPP_CODE_TA:		/* Terminate-Ack */
		log_ipkt(2,PNAME,"receive Terminate-Ack:");
		fsm_run(ppp, &I.fsm, FSM_RTA);
		break;

	case PPP_CODE_CJ:		/* Code-Reject */
		/* fail, since this IPCP is minimal */
		ppp->ipcp.cnts.rcvd_code_rej++;
		fsm_rcj(ppp, &I.fsm);
		break;

	default:
		ppp->ipcp.cnts.rcvd_bad++;
		log_complain(PNAME,"bad %s",
			     fsm_code_name(IBUF_CP->code));
		fsm_run(ppp, &I.fsm, FSM_RUC);
		break;
	}
#undef CK_ID

	ck_up(l_ppp);
}
