/* PPP Link Control Protocol processing
 */

#ident "$Revision: 1.31 $"

#include <math.h>
#include <ctype.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <net/if.h>

#include "ppp.h"

extern unsigned sysid(unsigned char id[16]);

extern struct timeval rcv_msg_ts;


static void lcp_scr(struct ppp*);
static void lcp_str(struct ppp*, struct fsm*);
static void lcp_ser(struct ppp*, struct fsm*);
static void lcp_act(struct ppp*, enum fsm_action);

static struct fsm_ops lcp_ops = {
	lcp_scr,
	fsm_sca,
	fsm_scn,
	lcp_str,
	fsm_sta,
	fsm_scj,
	lcp_ser,
	lcp_act
};


#define L ppp->lcp
#define A ppp->auth
#define LNAME L.fsm.name
#define STRUCT_PKT struct lcp_pkt
#define STRUCT_CF struct lcp_cf
#define BASE_P(bufp) (&(bufp)->lcp_un.cf[0])

#define EARLY_NAK_SENT() (L.fsm.nak_sent < 2)
#define EARLY_NAK_RECV() (L.fsm.nak_recv < 3)

#define MIGHT_RX_PRED1 (ppp0.ccp.conf.rx & SIOC_PPP_CCP_PRED1)
#define MIGHT_TX_PRED1 (ppp0.ccp.conf.tx & SIOC_PPP_CCP_PRED1)

/* LCP Configuration Options */
#define LCP_CO_MRU_T	    1		/* Maximum-Receive-Unit */
#define LCP_CO_MRU_L	    4
#define LCP_CO_ACCM_T	    2		/* Async-Control-Character-Map */
#define LCP_CO_ACCM_L	    6
#define LCP_CO_AUTH_T	    3
#define LCP_CO_PAP_L	    4		/* Password Auth. Protocol */
#define LCP_CO_PAP_P	    PPP_PAP
#define LCP_CO_CHAP_L	    5		/* Challenge-Handshake Auth. */
#define LCP_CO_CHAP_MD5	    5
#define LCP_CO_CHAP_P	    PPP_CHAP
#define LCP_CO_LQ_T	    4
#define LCP_CO_MAGIC_T	    5		/* Magic-Number */
#define LCP_CO_MAGIC_L	    6
#define LCP_CO_PCOMP_T	    7		/* Protocol-Field-Compression */
#define LCP_CO_PCOMP_L	    2
#define LCP_CO_ACOMP_T	    8		/* Address-and-Control-Field comp. */
#define LCP_CO_ACOMP_L	    2
#define LCP_CO_FCS_ALT_T    9
#define LCP_CO_S_D_PAD_T    10		/* self-describing padding */
#define LCP_CO_N_MODE_T	    11		/* numbered mode */
#define LCP_CO_ML_T	    12
#define LCP_CO_CALLBACK_T   13
#define LCP_CONNECT_TIME_T  14
#define LCP_COMPOUND_T	    15
#define LCP_NOMICAL_T	    16
#define LCP_CO_MRRU_T	    17		/* Multilik MRRU */
#define LCP_CO_MRRU_L	    4
#define LCP_CO_SSNHF_T	    18		/* Multilink short sequence # */
#define	LCP_CO_SSNHF_L	    2
#define LCP_CO_ENDPOINT_T   19		/* Multilink Endpoint Discriminator */
#define LCP_CO_ENDPOINT_MAX ((SYSNAME_LEN-10)/2)
#define LCP_CO_BACP_LINK_T  23

static char *
lcp_opt_name(u_int type)
{
	static char buf[32];

	switch (type) {
	case LCP_CO_MRU_T:
		return "MRU";
	case LCP_CO_ACCM_T:
		return "ACCM";
	case LCP_CO_AUTH_T:
		return "authentication";
	case LCP_CO_LQ_T:
		return "link quality monitoring";
	case LCP_CO_MAGIC_T:
		return "magic-numbers";
	case LCP_CO_PCOMP_T:
		return "protocol field compression";
	case LCP_CO_ACOMP_T:
		return "address and control field compression";
	case LCP_CO_FCS_ALT_T:
		return "FCS alternatives";
	case LCP_CO_S_D_PAD_T:
		return "self-describing pad";
	case LCP_CO_N_MODE_T:
		return "numbered mode";
	case LCP_CO_ML_T:
		return "multi-link procedure";
	case LCP_CO_CALLBACK_T:
		return "callback";
	case LCP_CONNECT_TIME_T:
		return "connect time";
	case LCP_COMPOUND_T:
		return "compound frames";
	case LCP_NOMICAL_T:
		return "nominal data encapsulation";
	case LCP_CO_MRRU_T:
		return "MRRU";
	case LCP_CO_SSNHF_T:
		return "MP short sequence #s";
	case LCP_CO_ENDPOINT_T:
		return "Endpoint Discriminator";
	case LCP_CO_BACP_LINK_T:
		return "BACP Link Discriminator";
	default:
		(void)sprintf(buf,"unknown option #%#x",type);
		return buf;
	}
}


/* shape of an LCP packet */
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
		    u_char  accm[4];
		    u_char  mru[2];
		    u_char  auth[2];
		    struct {
			u_char  proto[2];
		    } pap;
		    struct {
			u_char  proto[2];
			u_char	algorithm;
		    } chap;
		    u_char  magic[4];
		    u_char  s_d_pad;
		    u_char  mrru[2];
		    struct ep {
			u_char	class;
			u_char	addr[LCP_CO_ENDPOINT_MAX];
		    } ep;
		    struct {
			u_char	class;
			u_char	addr[4];
		    } ep_ip;
		    struct {
			u_char	class;
			u_char	addr[6];
		    } ep_mac;
		    struct {
			u_char	class;
			char	irix[4];
			u_char	sn[4];
		    } ep_sgi;
		} cf_un;
	    } cf[1];
	    struct {			/* Protocol-Reject */
		u_char	proto[2];
		u_char	info[1];
	    } pj;
	    struct {			/* Echo-Request and Echo-Reply */
		__uint32_t  magic;
		struct timeval ts;	/* 0 or more bytes of data */
	    } echo;
	    struct {			/* Code-Reject */
		u_char  cj_code;
		u_char  cj_id;
		u_short cj_len;
		u_char  cj_info[1];
	    } cj;
	    struct ppp_ident ident;	/* Identification */
	} lcp_un;
};


/* make a string to display 256-bit TX ACCM
 */
static char*
tx_accm_str(PPP_FM accm[PPP_ACCM_LEN])
{
	static char str[2+10*8+1];
	PPP_FM laccm[PPP_ACCM_LEN];
	int i;

	/* do not mention the escaping of the frame and escape bytes
	 * unless debugging is very high.
	 */
	bzero(laccm,sizeof(laccm));
	if (debug < 6) {
		PPP_SET_ACCM(laccm,PPP_FLAG);
		PPP_SET_ACCM(laccm,PPP_ESC);
	}
	for (i = 0; i < PPP_ACCM_LEN; i++)
		laccm[i] = accm[i] & ~laccm[i];
	(void)sprintf(&str[2], "%08x%08x%08x%08x%08x%08x%08x%08x",
		      laccm[7], laccm[6],
		      laccm[5], laccm[4],
		      laccm[3], laccm[2],
		      laccm[1], laccm[0]);
	i = strspn(&str[2], "0")+2;
	if (str[i] == '\0') {
		i--;
	} else {
		str[--i] = 'x';
		str[--i] = '0';
	}
	return &str[i];
}


char *
phase_name(enum ppp_phase phase)
{
	static char *names[] = {
		"Dead",
		"Establish",
		"Authenticate",
		"Terminate",
		"Network"
	};
	static char bad[] = "xxxxxxxxxx";

	if (phase <= NET_PHASE)
		return names[phase];
	sprintf(bad, "%d", phase);
	return bad;
}


void
log_phase(struct ppp *ppp,
	  enum ppp_phase new_phase)
{
	log_debug(2,ppp->link_name,"entering %s Phase",
		  phase_name(new_phase), 0,0);
	ppp->phase = new_phase;
}


/* Set LCP transmit parameters and anything else that should be requested
 * each time LCP is reset.
 * LCP is reset when a link is first started and upon the receipt of
 * a Configure-Request.
 */
void
lcp_param(struct ppp *ppp)
{
	int min_frag_size;


	L.fsm.restart_ms = ppp->conf.restart_ms;
	L.fsm.restart_ms_lim = ppp->conf.restart_ms_lim;

	/* cannot start MP on a second link or if IPCP or CCP are alive */
	L.refuse_mp = (!mp_on && mp_known);
	L.neg_mp = (!L.refuse_mp && !L.seen_mp_rej);
	L.refuse_recv_ssn = (L.refuse_mp
			     || !ppp->conf.recv_ssn
			     || (!mp_recv_ssn && mp_known));
	L.neg_recv_ssn = !L.refuse_recv_ssn;

	L.neg_send_ssn = (ppp->conf.send_ssn
			  && L.neg_mp
			  && (mp_send_ssn || !mp_known));

	/* Compute the various hoped-for and required MTUs and MRUs.
	 *
	 * ppp->conf.ip_mtu is used to reduce the IP MTU below 1500.
	 *
	 * ppp->conf.frag_size sets the minimum fragment size,
	 * and so bounds below the per-link MTU.  You cannot
	 * force a length of more than 1500.  If the minimum fragment
	 * size is not set, then try for 1500 but go along with the
	 * peer if it wants something smaller.
	 *
	 * Notice if the kernel is already committed to an IP MTU.
	 * If MP is possible, the MTU is unrestrained, but the MTRU
	 * is controlled by the kernel's value.
	 */
	min_frag_size = (ppp->conf.frag_size < PPP_MIN_MTU
			 ? PPP_MIN_MTU
			 : MIN(ppp->conf.frag_size, PPP_DEF_MRU));
	if (kern_ip_mtu > 0) {
		L.min_mtru = kern_ip_mtu;
		L.tgt_mtru = kern_ip_mtu;
	} else if (ppp->conf.ip_mtu < PPP_MIN_MTU) {
		L.min_mtru = min_frag_size;
		L.tgt_mtru = MAX(L.min_mtru, PPP_DEF_MRU);
	} else {
		L.min_mtru = MIN(ppp->conf.ip_mtu, PPP_DEF_MRU);
		L.tgt_mtru = ppp->conf.ip_mtu;
	}
	L.min_mtu = L.neg_mp ? min_frag_size : L.min_mtru;
	L.tgt_mtu = MAX(L.min_mtu, L.tgt_mtru);
	L.tgt_mru = L.tgt_mtu;
	L.tgt_mrru = L.tgt_mtru;

	if (L.neg_mtru == 0)
		L.neg_mtru = MIN(L.tgt_mtru, PPP_DEF_MRU);
	if (L.neg_mtu == 0)
		L.neg_mtu = MIN(L.tgt_mtu, PPP_DEF_MRU);

	/* adjust for optional protocols */
	if (L.neg_mp) {
		L.tgt_mtu += PPP_MP_MAX_OVHD;
		L.tgt_mru += PPP_MP_MAX_OVHD;
	}
	if (MIGHT_TX_PRED1) {
		L.tgt_mtu += PRED_OVHD;
		L.tgt_mtru += PRED_OVHD;
	}
	if (MIGHT_RX_PRED1) {
		L.tgt_mru += PRED_OVHD;
		L.tgt_mrru += PRED_OVHD;
	}

	/* The actual values of the MRU and MRRU do not matter to us,
	 * because we can always accept any size up to the maximum.
	 * The object of all of this is to pick values that the peer
	 * will accept with arguing.  The values of the MTU and MTRU do
	 * matter, because we cannot send packets larger than their limits.
	 *
	 * If our MRU is larger than the default, then inflate it to reduce
	 * Naks from peers that like bridging.  Do not inflate unless
	 * it is already not the default to avoid sending an unneeded
	 * configure-request.
	 */
	if (L.tgt_mru != PPP_DEF_MRU && L.tgt_mru < PPP_NONDEF_MRU)
		L.tgt_mru = PPP_NONDEF_MRU;
	/* Make the target MRU at least the default to speed negotiations,
	 * since the default is always possible.
	 */
	if (L.tgt_mtu < PPP_DEF_MRU)
		L.tgt_mtu = PPP_DEF_MRU;
	/* Always inflate the MRRU because it is always sent if
	 * it is relevant--i.e. when multilink is possible.
	 */
	if (L.tgt_mrru < PPP_NONDEF_MRU)
		L.tgt_mrru = PPP_NONDEF_MRU;

	/* Configure-Nak values from the peer or what we have sent
	 * in Configure-Requests should stick.
	 */
	if (L.neg_mru != 0)
		L.tgt_mru = L.neg_mru;
	if (L.neg_mrru != 0)
		L.tgt_mrru = L.neg_mrru;

	if (L.tgt_mtu > PPP_MAX_MTU)
		L.tgt_mtu = PPP_MAX_MTU;
	if (L.tgt_mtru > PPP_MAX_MTU)
		L.tgt_mtru = PPP_MAX_MTU;
	if (L.tgt_mru > PPP_MAX_MTU)
		L.tgt_mru = PPP_MAX_MTU;
	if (L.tgt_mrru > PPP_MAX_MTU)
		L.tgt_mrru = PPP_MAX_MTU;

	if (L.conf.echo_int < 0)
		L.conf.echo_int = DEF_ECHO_INT;
	if (L.conf.echo_fail < 0 && L.conf.echo_int != 0)
		L.conf.echo_fail = MAX(3, DEF_ECHO_RTT/L.conf.echo_int);
}


/* send link parameters to the kernel, for all input and later IP output
 */
int					/* <0 on failure */
lcp_set(struct ppp *ppp,
	int force)			/* 1=print results even if unchanged */
{
	char dbg[80];
	struct ppp_arg arg;
	int res;

	if (ppp->dv.sync == SYNC_DEFAULT)
		log_complain(LNAME, "sync unknown before SIOC_PPP_LCP");

	bzero(&arg,sizeof(arg));

	if (ppp->dv.sync == SYNC_ON) {
		arg.v.lcp.bits |= SIOC_PPP_LCP_SYNC;
	} else {
		if (ppp->dv.sync == SYNC_OFF
		    && (!PPP_ASYNC_MAP(PPP_ESC, L.tx_accm)
			|| !PPP_ASYNC_MAP(PPP_FLAG, L.tx_accm)))
			log_complain(LNAME, "bad ACCM");

		bcopy(L.tx_accm, arg.v.lcp.tx_accm,
		      sizeof(arg.v.lcp.tx_accm));
		arg.v.lcp.rx_accm = L.rx_accm;
	}

	if (L.neg_acomp)
		arg.v.lcp.bits |= SIOC_PPP_LCP_ACOMP;
	if (L.neg_pcomp)
		arg.v.lcp.bits |= SIOC_PPP_LCP_PCOMP;

	res = 0;
	if (force
	    || bcmp(&arg.v.lcp, &L.arg_lcp_set, sizeof(L.arg_lcp_set))) {
		(void)sprintf(dbg, "%s acomp=%d pcomp=%d rx_ACCM=%#x",
			      ppp->dv.sync ? "sync" : "async",
			      L.neg_acomp, L.neg_pcomp,
			      arg.v.lcp.rx_accm);
		log_debug(2,LNAME, "set %s tx=%s", dbg,
			  tx_accm_str(arg.v.lcp.tx_accm));

		if (0 <= (res = do_strioctl(ppp, SIOC_PPP_LCP, &arg,
					    "ioctl(SIOC_PPP_LCP)")))
			bcopy(&arg.v.lcp, &L.arg_lcp_set,
			      sizeof(L.arg_lcp_set));
	}

	return res;
}


/* reset ACCM to the most conservative value
 */
int					/* <0 on failure */
reset_accm(struct ppp *ppp,
	   int force)			/* 1=print results even if unchanged */
{
	lcp_param(ppp);

	if (ppp->dv.sync == SYNC_DEFAULT)
		log_complain(LNAME, "sync unknown before ACCM set");

	L.neg_pcomp = 0;
	L.neg_acomp = 0;

	if (ppp->dv.sync == SYNC_ON) {
		bzero(L.tx_accm, sizeof(L.tx_accm));
		L.rx_accm = 0;
		L.cr_accm = 0;
	} else {
		bcopy(L.conf.accm, L.tx_accm, sizeof(L.tx_accm));
		L.tx_accm[0] = PPP_ACCM_DEF;
		PPP_SET_ACCM(L.tx_accm,PPP_FLAG);
		PPP_SET_ACCM(L.tx_accm,PPP_ESC);

		if (L.parity_accm)
			L.tx_accm[PPP_ACCM_LEN/2] = PPP_ACCM_DEF;

		if (L.use_rx_accm == RX_ACCM_ON)
			L.rx_accm = PPP_ACCM_DEF;
		if (L.use_rx_accm != RX_ACCM_SET)
			L.cr_accm = 0;
	}

	return lcp_set(ppp, force);
}


static int				/* 1=got an address */
get_loc_mac(void)
{
	if (have_loc_mac == 0) {
		if (if_mac_addr(0,0,&loc_mac) != 0) {
			have_loc_mac = 1;
		} else {
			have_loc_mac = -1;
			bzero(&loc_mac,sizeof(loc_mac));
		}
	}

	return have_loc_mac;
}


/* get LCP going
 */
void
lcp_init(struct ppp *ppp)
{
	L.fsm.protocol = PPP_LCP;
	L.fsm.ops = &lcp_ops;

	/* At first the RX ACCM should be very conservative. */
	if (L.use_rx_accm == RX_ACCM_SET)
		L.use_rx_accm = RX_ACCM_ON;
	L.cr_accm = 0;
	L.my_magic = sysid(0);
	L.bad_magic = 0;
	L.auth_suggest = 0;
	L.auth_refuse = 0;
	A.peer_proto = 0;
	L.seen_epdis_rej = 0;
	L.seen_mru_nak_rej = 0;
	L.seen_mp_rej = 0;
	L.seen_pcomp_rej = 0;
	L.seen_acomp_rej = 0;
	L.ident_off = L.conf.ident_off;
	L.ident_id = 0;
	L.echo_seen = 0;
	L.echo_next_id += L.conf.echo_fail+1;
	bzero(&L.ident_rcvd, sizeof(L.ident_rcvd));
	bzero(&L.arg_lcp_set,sizeof(L.arg_lcp_set));
	bzero(&L.arg_mp_set,sizeof(L.arg_mp_set));
	L.arg_mp_set.max_frag = PPP_MAX_MTU;

	ppp->loc_epdis[0] = '\0';
	ppp->rem_epdis[0] = '\0';

	L.neg_mtu = PPP_DEF_MRU;
	L.neg_mru = 0;
	L.neg_mtru = PPP_DEF_MRU;
	L.neg_mrru = 0;
	lcp_param(ppp);

	fsm_init(&L.fsm);

	A.timer.tv_sec = TIME_NEVER;
}


/* send protocol reject if LCP is in OPENED state
 */
void
lcp_pj(struct ppp *ppp, char* name)
{
	if (L.fsm.state != FSM_OPENED_9)
		return;

	obuf.proto = PPP_LCP;
	obuf.bits = (mp_ncp_bits != 0) ? ibuf.bits : 0;
	OBUF_CP->code = PPP_CODE_PJ;
	OBUF_CP->id = ++L.fsm.id;
	OBUF_CP->len = MIN(L.neg_mtu-2, 6+ibuf_info_len);
	OBUF_CP->lcp_un.pj.proto[0] = ibuf.proto>>8;
	OBUF_CP->lcp_un.pj.proto[1] = ibuf.proto;
	bcopy(&ibuf.un.info[0], &OBUF_CP->lcp_un.pj.info[0],
	      OBUF_CP->len-6);
	log_debug(2,name,"send Protocol-Reject ID=%#x", L.fsm.id);
	ppp_send(ppp, &obuf, OBUF_CP->len);
}


/* Make something happen to the LCP machine.
 */
void
lcp_event(struct ppp *ppp,
	  enum fsm_event ev)
{
	switch (ev) {
	case FSM_UP:			/* the modem is alive */
	case FSM_DOWN:			/* the modem died */
	case FSM_OPEN:			/* start work */
		fsm_run(ppp,&L.fsm,ev);
		break;

	case FSM_CLOSE:			/* start to shut down */
		fsm_run(ppp,&L.fsm,ev);
		log_phase(ppp,TERM_PHASE);
		break;

	case FSM_TO_P:			/* the restart timer has expired */
		if (L.fsm.restart <= 0)
			ev = FSM_TO_M;
		fsm_run(ppp,&L.fsm,ev);
		break;

	default:
		log_complain(LNAME,"unknown event #%d", ev);
	}
}


static struct {
    int	    min;			/* minimum length */
    int	    max;			/* maximum length */
    char    *name;			/* name of type */
} ep_info[] = {
	{0, 0,	"Null"},		/* class 0 */
	{1, 20,	"Locally Assigned"},	/* class 1 */
	{4, 4,	"IP"},			/* class 2 */
	{6, 6,	"MAC"},			/* class 3 */
	{4, 20,	"Magic"},		/* class 4 */
	{1, 15,	"Phone #"}		/* class 5 */
};

/* convert Endpoint Discriminator to ASCII
 */
static void
ep2ascii(char *buf,
	 struct ep *ep,
	 int len)
{
	int t;
	int i;
	u_int n;
	struct in_addr its_addr;

	switch (ep->class) {
	case LCP_CO_ENDPOINT_NULL:
		*buf = '\0';
		return;

	case LCP_CO_ENDPOINT_IP:
		its_addr.s_addr = ((ep->addr[0]<<24)
				   + (ep->addr[1]<<16)
				   + (ep->addr[2]<<8)
				   + ep->addr[3]);
		(void)strcpy(buf, inet_ntoa(its_addr));
		return;

	case LCP_CO_ENDPOINT_MAC:
		(void)sprintf(buf,"%x:%x:%x:%x:%x:%x",
			      ep->addr[0],ep->addr[1],ep->addr[2],
			      ep->addr[3],ep->addr[4],ep->addr[5]);
		return;

	case LCP_CO_ENDPOINT_MAGIC:
		(void)sprintf(buf, "%d-%d", ep->class, len);
		i = 0;
		while (len > 1) {
			buf += strlen(buf);
			n = ep->addr[i++];
			while (i%4 != 0 && len > 1) {
				n = (n<<8) + ep->addr[i++];
				len--;
			}
			(void)sprintf(buf,"-%#x", n);
		}
		return;

	case LCP_CO_ENDPOINT_LOC:
	case LCP_CO_ENDPOINT_PHONE:
	default:
		break;
	}

	(void)sprintf(buf, "%d-%d-", ep->class, len);
	buf += strlen(buf);

	/* trim trailing zeros */
	while (len > 1 && ep->addr[len-2] == 0)
		len--;
	/* translate the rest to ASCII, recognizing safe ASCII bytes
	 */
	t = 0;
	for (i = 1; i < len; i++) {
		if (!t && (isalnum(ep->addr[i-1])
			   || ep->addr[i-1] == '+'
			   || ep->addr[i-1] == '-'
			   || ep->addr[i-1] == '_'
			   || ep->addr[i-1] == '@'
			   || ep->addr[i-1] == '='
			   || ep->addr[i-1] == '#'
			   || ep->addr[i-1] == '%')) {
			*buf++ = ep->addr[i-1];
		} else {
			t = 1;
			(void)sprintf(buf, "%02x", ep->addr[i-1]);
			buf += 2;
		}
	}
	*buf = '\0';
}


/* process result from FSM
 */
static void
lcp_act(struct ppp *ppp,
	enum fsm_action act)
{
	static char pat[] = "MTU=%d MRU=%d %s%s%s%s%s%s%s%s";
	char str[sizeof(pat)+8*12+10*8+6*8];
	char bigxmit_str[8+8+1+1];
	char mtru_str[4+8+1+1];
	char mrru_str[4+8+1+1];
	int i;
	struct ppp *ppp1;


	log_debug(2, LNAME,"action %s", fsm_action_name(act));

	switch (act) {
	case FSM_TLU:
		/* dump configuration */
		(void)sprintf(bigxmit_str, "bigxmit=%d ", bigxmit);
		if (L.neg_mp) {
			(void)sprintf(mtru_str, "MTRU=%d ", L.neg_mtru);
			(void)sprintf(mrru_str, "MRRU=%d ", L.neg_mrru);
		} else {
			mtru_str[0] = '\0';
			mrru_str[0] = '\0';
		}
		(void)sprintf(str, pat, L.neg_mtu, L.neg_mru,
			      mtru_str, mrru_str,
			      (bigxmit > 0) ? bigxmit_str : "",
			      telnettos ? "TOS " : "",
			      L.neg_pcomp ? "PCOMP " : "",
			      L.neg_acomp ? "ACOMP " : "",
			      (L.neg_mp && L.neg_send_ssn) ? "send-SSN " : "",
			      (L.neg_mp && L.neg_recv_ssn) ? "recv-SSN" : "");
		log_debug(1, LNAME,"%s", str);

		if (ppp->dv.sync == SYNC_ON) {
			(void)strcpy(str,"sync ");
		} else {
			/* stop using LCP default ACCM */
			bcopy(L.conf.accm, L.tx_accm, sizeof(L.tx_accm));
			L.tx_accm[0] |= L.cr_accm;
			if (L.parity_accm)
				L.tx_accm[PPP_ACCM_LEN/2] |= L.cr_accm;
			if (L.use_rx_accm == RX_ACCM_ON
			    || L.use_rx_accm == RX_ACCM_SET) {
				L.use_rx_accm = RX_ACCM_SET;
				L.rx_accm = L.cr_accm | L.conf.accm[0];
			}

			(void)sprintf(str, "rx_ACCM=%#x tx=%s",
				      (int)L.rx_accm,
				      tx_accm_str(L.tx_accm));

			PPP_SET_ACCM(L.tx_accm,PPP_FLAG);
			PPP_SET_ACCM(L.tx_accm,PPP_ESC);
		}
		log_debug(1, LNAME, "my magic #=%#x,peer's=%#x %s",
			  L.my_magic, L.his_magic, str);

		if (lcp_set(ppp,0) < 0) {
			ipcp_event(ppp, FSM_CLOSE);
			break;
		}

		lcp_send_ident(ppp, L.my_magic);

		L.echo_old_id = (L.echo_next_id += L.conf.echo_fail);
		L.echo_timer.tv_sec = TIME_NOW;
		(void)strcpy(ppp->loc_epdis, loc_epdis);
		(void)strcpy(ppp->rem_epdis, rem_epdis);

		log_phase(ppp,AUTH_PHASE);
		auth_start(ppp);	/* start authenticating */
		break;

	case FSM_TLD:
		/* clear ACCM, stop authentication, and block IP on this link
		 */
		(void)reset_accm(ppp,0);
		A.timer.tv_sec = TIME_NEVER;
		if (ppp->dv.dev_index != -1)
			(void)do_strioctl_ok(ppp, SIOC_PPP_TX_OK,
					     0, "ioctl(LCP TLD TX_OK off)");
		/* Count active links in the MP bundle.
		 */
		i = 0;
		if (mp_on) {
			for (ppp1 = &ppp0;
			     ppp1 != 0;
			     ppp1 = (struct ppp*)ppp1->dv.next) {
				if (ppp1 != ppp
				    && ppp1->lcp.fsm.state == FSM_OPENED_9)
					i++;
			}
		}
		/* If this is the only active link in the bundle
		 * (or using BF&I multilink), tell the kernel to kill IP
		 * and restart all NCPs
		 */
		if (i == 0) {
			ipcp_event(ppp,FSM_DOWN);
			if (ppp->ccp.fsm.state >= FSM_STARTING_1)
				ccp_event(ppp,FSM_DOWN);
		}
		break;

	case FSM_TLS:
		if (0 > reset_accm(ppp,0)) {
			ipcp_event(ppp, FSM_CLOSE);
			break;
		}
		log_phase(ppp,ESTAB_PHASE);
		break;

	case FSM_TLF:
		log_phase(ppp,DEAD_PHASE);
		hangup(ppp);
		break;
	}
}


/* parse a received Configure-Request
 */
static enum fsm_event
lcp_parse_cr(struct ppp *ppp)
{
	struct lcp_cf *icfp;
	struct lcp_cf *jcfp = BASE_P(JBUF_CP);
	struct lcp_cf *ncfp = BASE_P(NBUF_CP);
	register u_int l;
	register int i;
	int complained, seen_mtu, seen_mtru;
	char offered[50], buf[SYSNAME_LEN+1];
#   define BAD_LEN(cond, L) {if (icfp->len cond (L)) {			    \
	log_complain(LNAME, "   reject %s with strange length=%d",	    \
		     lcp_opt_name(icfp->type), icfp->len);		    \
	complained = 1;							    \
	break;}}
	/* skip the NAK since we are sending a REJECT */
#   define SKIP_NAK() (jcfp != BASE_P(JBUF_CP))

	log_debug(2,LNAME,"receive Configure-Request ID=%#x", IBUF_CP->id);

	/* "all options are always negotiated at once," so a missing
	 * option implies the default.
	 */
	seen_mtu = 0;
	L.neg_mtu = MIN(L.tgt_mtu, PPP_DEF_MRU);
	seen_mtru = 0;
	L.neg_mtru = MIN(L.tgt_mtru, PPP_DEF_MRU);
	if (!mp_on || !mp_known)
		L.neg_mp = 0;
	L.neg_send_ssn = 0;
	L.cr_accm = (ppp->dv.sync == SYNC_ON) ? 0 : PPP_ACCM_DEF;
	A.peer_proto = 0;
	L.neg_pcomp = 0;
	L.neg_acomp = 0;

	for (icfp = BASE_P(IBUF_CP);
	     icfp < (struct lcp_cf*)&ibuf.un.info[ibuf_info_len];
	     ADV(icfp)) {
		/* forget entire packet if the length of one option is bad */
		if (icfp->len < 2) {
			log_complain(LNAME, "   bad option length %d",
				     icfp->len);
			return FSM_RUC;
		}

		complained = 0;
		switch (icfp->type) {
		case LCP_CO_MRU_T:
			/* This is the MRU of the peer or our MTU.
			 *
			 * Immediately agree to any MTU if it is bigger than
			 * what we want (or later, need).
			 *
			 * If the value of the peer is too small, Nak it in
			 * the early rounds.  If the value we need is no
			 * larger than the default, then eventually reject the
			 * option to force the peer to use the default.
			 * If we need more than the default, Nak forever.
			 */
			BAD_LEN(!=,LCP_CO_MRU_L);
			l = GET_S(icfp,mru);
			seen_mtu = 1;
			/* Give the peer a chance to give our absolute minimum.
			 */
			i = L.tgt_mtu;
			if (l < i && !EARLY_NAK_SENT())
				i = MAX(L.min_mtu, l);
			if (l < i) {
				if (i > PPP_DEF_MRU) {
					if (SKIP_NAK())
					    continue;
					log_cd(!EARLY_NAK_SENT(),2,LNAME,
					       "   Nak MTU=%d with %d",
					       l, i);
					ADD_CO_SHORT(ncfp, i, mru);
					GEN_CO(ncfp, LCP_CO_MRU_T, mru);
					continue;
				}
				log_complain(LNAME, "   reject peer's MTU=%d"
					     " instead of our minimum %d"
					     " to force %d, but use %d",
					     l, L.min_mtu,
					     PPP_DEF_MRU, L.neg_mtu);
				complained = 1;
				break;
			}
			/* Accept it, but do not use it
			 * if it is bigger than we need.
			 */
			if (l > L.tgt_mtu) {
				L.neg_mtu = L.tgt_mtu;
				log_debug(2,LNAME,
					  "   accept MTU=%d but use %d",
					  l, L.neg_mtu);
			} else {
				L.neg_mtu = l;
				log_debug(2,LNAME,
					  "   accept MTU=%d", L.neg_mtu);
			}
			continue;

		case LCP_CO_MRRU_T:
			BAD_LEN(!=,LCP_CO_MRRU_L);
			l = GET_S(icfp,mrru);
			if (L.refuse_mp) {
				/* Reject multilink if not ok */
				log_cd(!EARLY_NAK_SENT(),2,LNAME,
				       "   reject MP MRRU of %d%s",
				       l,
				       (L.seen_mp_rej
					? " because peer rejected MP" : ""));
				complained = 1;
				break;
			}
			L.neg_mp = 1;	/* note multilink */
			seen_mtru = 1;
			/* Give the peer a chance to give our absolute minimum.
			 */
			i = L.tgt_mtru;
			if (l < i && !EARLY_NAK_SENT())
				i = MAX(L.min_mtru, l);
			if (l < i) {
				/* Keep trying for a useful MP MTRU if we
				 * have not tried hard or if we have
				 * MP on other links.
				 */
				if (EARLY_NAK_SENT() || (mp_on && mp_known)) {
					if (SKIP_NAK())
					    continue;
					log_cd(!EARLY_NAK_SENT(),2,LNAME,
					       "   Nak MP MTRU=%d with %d",
					       l, i);
					ADD_CO_SHORT(ncfp, i, mrru);
					GEN_CO(ncfp, LCP_CO_MRRU_T, mrru);
					continue;
				}
				/* Otherwise, give up on MP */
				log_complain(LNAME, "   Reject MP MTRU=%d"
					     " instead of %d; give up on MP",
					     l, i);
				L.neg_mp = 0;
				L.neg_send_ssn = 0;
				complained = 1;
				break;
			}
			/* Accept it, but do not use it
			 * if it is bigger than we want.
			 */
			if (l > L.tgt_mtru) {
				L.neg_mtru = L.tgt_mtru;
				log_debug(2,LNAME,
					  "   accept MP MTRU=%d but use %d",
					  l, L.neg_mtru);
			} else {
				L.neg_mtru = l;
				log_debug(2,LNAME, "   accept MP MTRU=%d",
					  L.neg_mtru);
			}
			continue;

		case LCP_CO_ACCM_T:
			BAD_LEN(!=,LCP_CO_ACCM_L);
			L.cr_accm = GET_L(icfp,accm);
			/* Turn off ACCM quickly to ensure we do not
			 * discard un-escaped bytes from the other guy.
			 * After hearing his TX ACCM, make a note to not
			 * ignore bytes that he might not be escaping.
			 * This is to deal with the race of our switching
			 * to a conservative RX ACCM before he switches
			 * to a conservative TX ACCM.
			 */
			if (L.use_rx_accm == RX_ACCM_ON
			    || L.use_rx_accm == RX_ACCM_SET) {
				L.use_rx_accm = RX_ACCM_SET;
				L.rx_accm = L.cr_accm | L.conf.accm[0];
			}
			/* Nak if our peer does not want enough bits.
			 * Generate the Nak now to ensure we do it
			 * in the right order.
			 */
			if (0 != (L.conf.accm[0] & ~L.cr_accm)
			    && ppp->dv.sync == SYNC_OFF) {
				if (SKIP_NAK())
					continue;
				ADD_CO_LONG(ncfp, L.conf.accm[0] | L.cr_accm,
					    accm);
				GEN_CO(ncfp, LCP_CO_ACCM_T, accm);
				log_debug(2,LNAME,"   accept tx ACCM=0x%08x"
					  " but suggest 0x%08x",
					  L.cr_accm,
					  (L.conf.accm[0] | L.cr_accm));
			} else {
				log_debug(2,LNAME,"   accept tx ACCM=0x%08x",
					  L.cr_accm);
			}
			continue;

		case LCP_CO_AUTH_T:
			BAD_LEN(<,4);
			/* identify the offered authentication */
			i = GET_S(icfp,auth);
			if (i == LCP_CO_CHAP_P) {
				if (icfp->len != LCP_CO_CHAP_L) {
					i = 0;
					sprintf(offered, "CHAP (length %d)",
						icfp->len);
				} else if (icfp->cf_un.chap.algorithm
					   != LCP_CO_CHAP_MD5) {
					i = 0;
					sprintf(offered,"CHAP (algorithm %d)",
						icfp->cf_un.chap.algorithm);
				} else {
					strcpy(offered, "CHAP");
				}
			} else if (i == LCP_CO_PAP_P) {
				if (icfp->len != LCP_CO_PAP_L) {
					sprintf(offered,"PAP (length %d)",
						icfp->len);
					i = 0;
				} else {
					strcpy(offered,"PAP");
				}
			} else {
				strcpy(offered,"unknown");
			}

			/* If we have some authentication information,
			 * or if we might reconfigure and get it, and
			 * we recognize the protocol,
			 * then remember to send authenticate.
			 * If we have some authentication info but
			 * of the wrong kind, then Nak the request.
			 */
			if (i == LCP_CO_CHAP_P
			    && A.want_sendchap_response == 1) {
				A.peer_proto = PPP_CHAP;
				log_debug(2,LNAME,
					  "   note CHAP (MD5)");
				continue;
			}
			if (i == LCP_CO_PAP_P
			    && A.want_sendpap == 1) {
				A.peer_proto = PPP_PAP;
				log_debug(2,LNAME,
					  "   note PAP");
				continue;
			}
			if (A.want_sendchap_response == 1) {
				if (SKIP_NAK())
					continue;
				ADD_CO_SHORT(ncfp, LCP_CO_CHAP_P, chap.proto);
				ncfp->cf_un.chap.algorithm = LCP_CO_CHAP_MD5;
				GEN_CO(ncfp, LCP_CO_AUTH_T, chap);
				log_debug(2,LNAME,
					  "   Nak %s by suggesting CHAP (MD5)",
					  offered);
				continue;
			}
			if (A.want_sendpap == 1) {
				if (SKIP_NAK())
					continue;
				ADD_CO_SHORT(ncfp, LCP_CO_PAP_P, pap.proto);
				GEN_CO(ncfp, LCP_CO_AUTH_T, pap);
				log_debug(2,LNAME,
					  "   Nak %s by suggesting PAP",
					  offered);
				continue;
			}
			/* Otherwise, reject it and do not proceed
			 * past the Establish Phase.
			 */
			log_cd(!EARLY_NAK_SENT(),2,LNAME,
			       "   reject request of %s from us",
			       offered);
			complained = 1;
			break;

		case LCP_CO_LQ_T:
			/* reject it */
			log_cd(!EARLY_NAK_SENT(),2,LNAME,
			       "   Reject link quality");
			complained = 1;
			break;

		case LCP_CO_MAGIC_T:
			BAD_LEN(!=,LCP_CO_MAGIC_L);
			L.his_magic = GET_L(icfp,magic);
			if (L.his_magic == 0) {
				log_complain(LNAME, "   peer requesting"
					     " bogus magic #=0");
				continue;
			}
			/* reject magic if it has been turned off */
			if (L.my_magic == 0) {
				log_complain(LNAME, "   reject magic #s since"
					     " peer rejected them");
				complained = 1;
				break;
			}
			if (L.his_magic != L.my_magic) {
				log_debug(2,LNAME, "   peer's magic #=%#x",
					  L.his_magic);
			} else {
				if ((L.fsm.nak_sent >= ppp->conf.max_fail
				     || !EARLY_NAK_SENT())
				    && !L.bad_magic) {
					log_complain(LNAME,
						     "   Probable loopback;"
						     " both magic #s=%#x",
						     L.his_magic);
					L.bad_magic = 1;
				} else {
					log_debug(2,LNAME,
						  "   both magic #s=%#x",
						  L.his_magic);
				}
				if (L.fsm.nak_sent >= ppp->conf.max_fail-1
				    && L.fsm.nak_sent >= 4) {
					log_complain(LNAME,"   Stop worrying"
						     " about magic #s"
						     " and hope");
					L.my_magic = 0;
				} else {
					if (SKIP_NAK())
					    continue;
					log_debug(2,LNAME,
						  "   Nak for new magic #");
					L.my_magic = newmagic();
					ADD_CO_LONG(ncfp, L.my_magic, magic);
					GEN_CO(ncfp, LCP_CO_MAGIC_T, magic);
				}
			}
			continue;

		case LCP_CO_PCOMP_T:
			BAD_LEN(!=,LCP_CO_PCOMP_L);
			if (L.conf.pcomp) {
				L.neg_pcomp = 1;
				log_debug(2,LNAME,"   send compressed"
					  " protocol fields");
			} else {
				/* we do not have to send it */
				log_debug(2,LNAME,"   ignore request to send"
					  " compressed protocol fields");
			}
			continue;

		case LCP_CO_ACOMP_T:
			BAD_LEN(!=,LCP_CO_ACOMP_L);
			if (L.conf.acomp != 0) {
				L.neg_acomp = 1;
				log_debug(2,LNAME, "   send compressed"
					  " address fields");
			} else {
				/* we do not have to send it */
				log_debug(2,LNAME, "   ignore request to send"
					  " compressed address fields");
			}
			continue;

		case LCP_CO_SSNHF_T:
			/* Go along with the peer if it wants to receive
			 * short MP sequence numbers, unless we already
			 * have an active MP bundle.
			 */
			BAD_LEN(!=,LCP_CO_SSNHF_L);
			if (!L.refuse_mp)
				L.neg_mp = 1;	/* note multilink */
			if (ppp->conf.send_ssn
				   && !L.refuse_mp
				   && (mp_send_ssn || !mp_known)) {
				L.neg_send_ssn = 1;
				log_debug(2,LNAME,"   will send short"
					  "  MP sequence #s");
				continue;
			}
			/* Reject if not ok */
			log_cd(!EARLY_NAK_SENT(),2,LNAME,
			       "   reject short MP sequence #s%s",
			       L.seen_mp_rej ? " because peer rejected MP":"");
			complained = 1;
			break;

		case LCP_CO_ENDPOINT_T:
			l = icfp->len;
			i = icfp->cf_un.ep.class;
			if (i > LCP_MAX_CO_ENDPOINT) {
				log_complain(LNAME,
					     "   reject bogus Endpoint"
					     " Discriminator type %d", i);
				break;
			}
			if (l-3 < ep_info[i].min
			    || l-3 >ep_info[i].max
			    || (i == LCP_CO_ENDPOINT_MAGIC
				&& (l-3)%4 != 0)) {
				log_complain(LNAME,
					     "   reject bogus %s"
					     " Endpoint Discriminator length"
					     " of %d",
					     ep_info[i].name, l-3);
				complained = 1;
				break;
			}
			if (i == LCP_CO_ENDPOINT_NULL) {
				log_debug(2,LNAME,
					  "   ignore %s Endpoint"
					  " Discriminator",
					  ep_info[LCP_CO_ENDPOINT_NULL].name);
				continue;
			}
			ep2ascii(buf, &icfp->cf_un.ep, l-2);
			if (!ppp->conf.recv_epdis) {
				log_debug(2,LNAME,
					  "   reject %s Endpoint Discriminator"
					  " %s; turned off",
					  ep_info[i].name, buf);
				complained = 1;
				break;
			}
			if (rem_epdis[0] != '\0'
			    && strcmp(rem_epdis, buf)) {
				char *msg = epdis_msg(ep_info[i].name,
						      buf, rem_epdis);

				/* Allow the peer to change its discriminator
				 * only when starting the first link in a
				 * bundle.
				 * That means this is the only link, no other
				 * processes are dialing, and this LCP
				 * state machine must not have received
				 * reached OPENED.
				 */
				if (add_pid > 0 || numdevs > 1
				    || (ppp->rem_epdis[0] != '\0'
					&& strcmp(ppp->rem_epdis, buf))) {
					set_tr_msg(&L.fsm, "inconsistent %.*s",
						   TR_MSG_MAX-14, msg);
					return FSM_CLOSE;
				}
				log_debug(2,LNAME,
					  "   inconsistent %s", msg);
				rm_rend("ep-", rem_epdis);
			} else {
				log_debug(2,LNAME,
					  "   %s Endpoint Discriminator %s",
					  ep_info[i].name, buf);
			}
			strncpy(rem_epdis, buf, sizeof(rem_epdis));
			(void)add_rend_name("ep-", rem_epdis);
			continue;
		}

		/* Reject an option we do not understand.
		 */
		bcopy(icfp, jcfp, icfp->len);
		ADV(jcfp);
		if (!complained)
			log_cd(!EARLY_NAK_SENT(),2,LNAME,
			       "   reject %s",
			       lcp_opt_name(icfp->type));
	}

	if (!SKIP_NAK()) {
		/* If we did not receive an MRU option, have not been trying
		 * hard, and the default is smaller than we would like,
		 * then ask for a larger value
		 */
		if (!seen_mtu) {
			if (EARLY_NAK_SENT())
				i = L.tgt_mtu;
			else
				i = MAX(L.min_mtu, PPP_DEF_MRU);
			if (i > MAX(L.neg_mtu, PPP_DEF_MRU)) {
				log_debug(2, LNAME, "   Nak for MTU=%d", i);
				ADD_CO_SHORT(ncfp, i, mru);
				GEN_CO(ncfp, LCP_CO_MRU_T, mru);
				L.neg_mtu = i;
			}
		}

		/* Deal with MP if we have not already.
		 * If we are sending a Configure-Reject for anything
		 * (including MP), SKIP_NAK() will have kept us from coming
		 * here.
		 */
		if (!seen_mtru && !L.refuse_mp && !L.seen_mp_rej) {
			if (EARLY_NAK_SENT())
				i = L.tgt_mtru;
			else
				i = MAX(L.min_mtru, PPP_DEF_MRU);
			log_debug(2,LNAME,"   Nak for MP MTRU=%d", i);
			ADD_CO_SHORT(ncfp, i, mrru);
			GEN_CO(ncfp, LCP_CO_MRRU_T, mrru);
			L.neg_mtru = i;
		}

		/* Ask to send short sequence numbers if we want them and there
		 * is a chance the peer might allow them.
		 * If other links in the bundle have them, ask in any case.
		 */
		if (!L.neg_send_ssn
		    && ppp->conf.send_ssn
		    && L.neg_mp
		    && (mp_send_ssn || !mp_known)
		    && (ncfp != BASE_P(NBUF_CP)
			|| EARLY_NAK_SENT()
			|| (mp_send_ssn && mp_known))) {
			log_debug(2,LNAME,
				  "   Nak to send short MP sequence #s");
			GEN_CO_BOOL(ncfp, LCP_CO_SSNHF_T);
		}
	}

	/* install the new ACCM and so forth */
	(void)lcp_set(ppp,0);

	rejbuf.proto = PPP_LCP;
	JBUF_CP->len = 4+((char*)jcfp - (char*)BASE_P(JBUF_CP));

	nakbuf.proto = PPP_LCP;
	NBUF_CP->len = 4+((char*)ncfp - (char*)BASE_P(NBUF_CP));

	return ((JBUF_CP->len == 4 && NBUF_CP->len == 4)
		? FSM_RCR_P
		: FSM_RCR_M);

#undef BAD_LEN
}


/* parse a received Configure-Nak
 */
static void
lcp_parse_cn(struct ppp *ppp)
{
	struct lcp_cf *icfp;
	int i;
#   define BAD_LEN(cond,L) {if (icfp->len cond (L)) {			\
	log_complain(LNAME, "   ignore %s Nak with strange length=%d",	\
		     lcp_opt_name(icfp->type), icfp->len);		\
	break;}}


	log_debug(2,LNAME,"receive Configure-NAK ID=%#x", IBUF_CP->id,0,0);
	L.fsm.nak_recv++;

	for (icfp = BASE_P(IBUF_CP);
	     icfp < (struct lcp_cf*)&ibuf.un.info[ibuf_info_len];
	     ADV(icfp)) {
		/* forget entire packet if the length of one option is bad */
		if (icfp->len < 2) {
			log_complain(LNAME,"   bad CN option length %#x",
				     icfp->len);
			break;
		}

		switch (icfp->type) {
		case LCP_CO_MRU_T:
			/* The packets that peer wants us to send us should
			 * be no larger than we want to receive.
			 *
			 * Refuse an impossibly large MRU.
			 *
			 * However, we can always receive big packets and
			 * there is no way to force the peer to send big
			 * packets, so go along with too large or small
			 * a value to get the link up.
			 */
			BAD_LEN(!=,LCP_CO_MRU_L);
			i = GET_S(icfp,mru);
			if (i < PPP_MIN_MTU) {
				/* a value of 0 would mess up lcp_param() */
				log_cd(!EARLY_NAK_RECV(),2,LNAME,
				       "   ignore bad MRU=%d smaller than %d",
				       i, PPP_MIN_MTU);
				break;
			}
			if (i <= PPP_MAX_MTU) {
				L.tgt_mru = L.neg_mru = i;
				log_debug(2,LNAME,"   accept MRU=%d", i);
				break;
			}
			log_cd(!EARLY_NAK_RECV(),2,LNAME,
			       "   ignore bad MRU=%d bigger than %d",
			       i, PPP_MAX_MTU);
			break;

		case LCP_CO_MRRU_T:
			/* The MRRU option implies MP.
			 */
			BAD_LEN(!=,LCP_CO_MRRU_L);
			i = GET_S(icfp,mrru);
			if (L.refuse_mp) {
				log_cd(!EARLY_NAK_RECV(),2,LNAME,
				       "   ignore Nak for MP MRRU=%d%s",
				       i,
				       (L.seen_mp_rej
					? " because peer rejected MP" : ""));
				break;
			}
			L.neg_mp = 1;
			/* The packets that peer wants us to send us should
			 * be no larger than we want to receive.
			 *
			 * Refuse an impossibly large MRRU.
			 *
			 * However, we can always receive big packets and
			 * there is no way to force the peer to send big
			 * packets, so go along with too a large or small
			 * but tolerable value to get the link up.
			 */
			if (i <= PPP_MAX_MTU) {
				L.tgt_mrru = L.neg_mrru = i;
				log_debug(2,LNAME,"   accept MP MRRU=%d", i);
				break;
			}
			log_cd(!EARLY_NAK_RECV(),2,LNAME,
			       "   ignore bad MP MRRU=%d but note MP", i);
			break;

		case LCP_CO_ACCM_T:
			BAD_LEN(!=,LCP_CO_ACCM_L);
			i = GET_L(icfp,accm);
			if (ppp->dv.sync == SYNC_ON) {
				log_debug(2,LNAME, "   ignore rx ACCM=0x%08x"
					  " on sync line", i);
				break;
			}
			L.cr_accm = i;
			log_debug(2,LNAME,"   accept rx ACCM=0x%08x", i);
			break;

		case LCP_CO_AUTH_T:
			BAD_LEN(<,4);
			L.auth_refuse = A.our_proto;
			i = GET_S(icfp,auth);
			if (i == LCP_CO_CHAP_P) {
				L.auth_suggest = PPP_CHAP;
				log_cd(!EARLY_NAK_RECV(), 2,LNAME,
				       "   peer wants to send CHAP responses");
			} else if (i == LCP_CO_PAP_P) {
				L.auth_suggest = PPP_PAP;
				log_cd(!EARLY_NAK_RECV(), 2,LNAME,
				       "   peer wants to send PAP requests");
			} else {
				log_complain(LNAME, "   peer wants to send"
					     " authentication type %#x", i);
			}
			break;

		case LCP_CO_MAGIC_T:
			BAD_LEN(!=,LCP_CO_MAGIC_L);
			if (L.my_magic == 0) {
				log_complain(LNAME, "   ignore magic #s"
					     " that peer rejected");
				break;
			}
			L.his_magic = GET_L(icfp,magic);
			if (L.his_magic == L.my_magic) {
				do {
					L.my_magic = newmagic();
				} while (L.his_magic == L.my_magic);
				log_debug(2,LNAME,
					  "   accept peer's magic #=%#x"
					  " and pick new %#x",
					  L.his_magic, L.my_magic);
			} else {
				log_debug(2,LNAME,
					  "   accept peer's magic #=%#x",
					  L.his_magic);
			}
			break;

		case LCP_CO_SSNHF_T:
			BAD_LEN(!=,LCP_CO_SSNHF_L);
			if (L.refuse_recv_ssn || L.refuse_mp) {
				if (!L.refuse_mp) {
					L.neg_mp = 1;
					log_debug(2,LNAME, "   ignore"
						  " short MP sequence #s"
						  " but note MP");
				} else {
					log_debug(2,LNAME, "   ignore"
						  " short MP sequence #s%s",
						  (L.seen_mp_rej ?
						   " because peer rejected MP"
						   : ""));
				}
			} else {
				L.neg_mp = 1;
				L.neg_recv_ssn = 1;
				log_debug(2,LNAME,
					  "   note MP short sequence #s");
			}
			break;

		case LCP_CO_ENDPOINT_T:
		default:
			log_cd(EARLY_NAK_RECV(),2,LNAME, "   ignore %s Nak",
			       lcp_opt_name(icfp->type));
			break;
		}
	}

#undef BAD_LEN
}


/* parse a received Configure-Reject
 */
static void
lcp_parse_confj(struct ppp *ppp)
{
	struct lcp_cf *icfp;
	int i;
	char *str;

	log_debug(2,LNAME,"receive Configure-Reject ID=%#x", IBUF_CP->id);
	L.fsm.nak_recv++;

	icfp = BASE_P(IBUF_CP);
	if (icfp >= (struct lcp_cf*)&ibuf.un.info[ibuf_info_len]) {
		log_complain(LNAME,"empty Configure-Reject ID=%#x",
			     IBUF_CP->id);
	}

	for (;
	     icfp < (struct lcp_cf*)&ibuf.un.info[ibuf_info_len];
	     ADV(icfp)) {
		/* forget entire packet if the length of one option is bad */
		if (icfp->len < 2) {
			log_complain(LNAME, "   bad Configure-Reject option"
				     " length %#x",
				     icfp->len);
			break;
		}
		str = 0;
		switch (i = icfp->type) {
		case LCP_CO_MRU_T:
			L.tgt_mru = L.neg_mru = PPP_DEF_MRU;
			L.seen_mru_nak_rej = 1;
			break;

		case LCP_CO_ACCM_T:
			L.cr_accm = ((ppp->dv.sync == SYNC_ON)
				     ? 0 : PPP_ACCM_DEF);
			break;

		case LCP_CO_AUTH_T:
			/* For other Configure-Request options,
			 * we would stop asking.  Authentication
			 * is different.
			 */
			L.auth_refuse = A.our_proto;
			if (A.our_proto == PPP_CHAP) {
				str = "peer rejected CHAP";
			} else {
				str = "peer rejected PAP";
			}
			break;

		case LCP_CO_MAGIC_T:
			L.my_magic = 0;	/* no more magic numbers */
			break;

		case LCP_CO_PCOMP_T:
			L.neg_pcomp = 0;
			L.seen_pcomp_rej = 1;
			break;

		case LCP_CO_ACOMP_T:
			L.neg_acomp = 0;
			L.seen_acomp_rej = 1;
			break;

		case LCP_CO_MRRU_T:
			/* accept rejection of MP only if possible */
			if (mp_on && mp_known) {
				str = "had MP; ignore rejection of MP and";
			} else {
				L.seen_mp_rej = 1;
				L.refuse_mp = 1;
				L.neg_mp = 0;
			}
			break;

		case LCP_CO_SSNHF_T:
			if (mp_recv_ssn && mp_known) {
				str = "2nd link; ignore rejection of";
			} else {
				L.neg_recv_ssn = 0;
			}
			break;

		case LCP_CO_ENDPOINT_T:
			L.seen_epdis_rej = 1;
			break;

		default:
			str = "ignore unsolicited rejection of";
			break;
		}

		if (str == 0) {
			log_debug(2,LNAME, "   peer rejected %s",
				  lcp_opt_name(i));
		} else {
			log_complain(LNAME, "   %s %s", str, lcp_opt_name(i));
		}
	}

	/* review target parameters, especially MTU and MTRU */
	lcp_param(ppp);
}


/* FSM action to send a Configure-Request
 */
static void
lcp_scr(struct ppp *ppp)
{
	struct lcp_cf *p = BASE_P(OBUF_CP);

	L.fsm.id++;
	log_debug(2,LNAME,"send Configure-Request ID=%#x", L.fsm.id);

	L.neg_mru = L.tgt_mru;
	if (L.tgt_mru != PPP_DEF_MRU) {
		ADD_CO_SHORT(p, L.tgt_mru, mru);
		GEN_CO(p, LCP_CO_MRU_T, mru);
		log_debug(2,LNAME,"   MRU=%d", L.tgt_mru);
	}

	L.neg_mrru = L.tgt_mrru;
	if (L.neg_mp) {
		ADD_CO_SHORT(p, L.tgt_mrru, mrru);
		GEN_CO(p, LCP_CO_MRRU_T, mrru);
		log_debug(2,LNAME,"   MP MRRU=%d", L.tgt_mrru);
		if (L.neg_recv_ssn) {
			GEN_CO_BOOL(p, LCP_CO_SSNHF_T);
			log_debug(2,LNAME, "   short MP sequence #s");
		}
	}

	/* Always ask for authentication if we want it.
	 *
	 * If the choice between PAP and CHAP was not explicit in the
	 * control file (because only the passwords and user names were
	 * mentioned but not the choice), then prefer PAP.
	 * If both CHAP and PAP were explicitly configured, then try
	 * CHAP first.
	 *
	 * Do not honor a Configure-Reject from the peer rejection by
	 * ceasing requests for authentication.  Instead wait for the
	 * link to go away as the peer continues to refuse authentication.
	 *
	 * If CHAP is refused by the peer, then switch to PAP.
	 */
	if ((A.want_recvchap_response == 1
	     && (A.want_recvpap == 0
		 || (!L.prefer_recvpap && L.auth_refuse != PPP_CHAP)
		 || L.auth_refuse == PPP_PAP
		 || L.auth_suggest == PPP_CHAP))
	    || (A.want_recvchap_response < 0
		&& A.want_recvpap <= 0
		&& L.auth_suggest == PPP_CHAP)) {
		A.our_proto = PPP_CHAP;
		ADD_CO_SHORT(p, LCP_CO_CHAP_P, chap.proto);
		p->cf_un.chap.algorithm = LCP_CO_CHAP_MD5;
		GEN_CO(p, LCP_CO_AUTH_T, chap);
		log_debug(2,LNAME,"   receive CHAP responses");
	} else if (A.want_recvpap == 1
		   || (A.want_recvpap < 0 && reconfigure)) {
		A.our_proto = PPP_PAP;
		ADD_CO_SHORT(p, LCP_CO_PAP_P, pap.proto);
		GEN_CO(p, LCP_CO_AUTH_T, pap);
		log_debug(2,LNAME,"   receive PAP requests");
	} else {
		A.our_proto = 0;
	}

	if ((ppp->dv.sync == SYNC_OFF && L.conf.accm[0] != PPP_ACCM_DEF)
	    || (ppp->dv.sync == SYNC_ON && L.conf.accm[0] != 0)) {
		ADD_CO_LONG(p, L.conf.accm[0] | L.cr_accm, accm);
		GEN_CO(p, LCP_CO_ACCM_T, accm);
		log_debug(2,LNAME,"   rx ACCM=0x%08x",
			  L.conf.accm[0] | L.cr_accm);
	}

	if (L.my_magic != 0) {
		ADD_CO_LONG(p, L.my_magic, magic);
		GEN_CO(p, LCP_CO_MAGIC_T, magic);
		log_debug(2,LNAME,"   my magic #=%#x", L.my_magic);
	}

	if (L.conf.pcomp != 0
	    && !L.seen_pcomp_rej) {
		GEN_CO_BOOL(p, LCP_CO_PCOMP_T);
		log_debug(2,LNAME, "   receive compressed protocol fields");
	}

	if ((L.conf.acomp > 0
	     || (L.conf.acomp < 0 && ppp->dv.sync == SYNC_OFF))
	    && !L.seen_acomp_rej) {
		GEN_CO_BOOL(p, LCP_CO_ACOMP_T);
		log_debug(2,LNAME,"   receive compressed address fields");
	}

	/* Send Endpoint Discriminator of our IP address, if known.
	 * By default, do not send it if we are not trying for MP,
	 * because peers that do not support MP are likely to waste time
	 * sending a Configure-Reject for it.
	 * If we send them once, then continue sending them.
	 */
	if (!L.seen_epdis_rej
	    && (loc_epdis[0] != '\0'
		|| (L.neg_mp && ppp->conf.send_epdis < 0)
		|| ppp->conf.send_epdis > 0)
	    && (ppp->conf.send_epdis != LCP_CO_ENDPOINT_IP
		|| (!def_lochost && lochost.sin_addr.s_addr != 0))) {
		if (ppp->conf.send_epdis == LCP_CO_ENDPOINT_IP) {
			p->cf_un.ep.class = LCP_CO_ENDPOINT_IP;
			bcopy(&lochost.sin_addr.s_addr, p->cf_un.ep_ip.addr,
			      sizeof(p->cf_un.ep_ip.addr));
			ep2ascii(loc_epdis, &p->cf_un.ep,
				 sizeof(p->cf_un.ep_ip));
			GEN_CO(p, LCP_CO_ENDPOINT_T, ep_ip);
		} else if (get_loc_mac() > 0
			   && (ppp->conf.send_epdis == LCP_CO_ENDPOINT_MAC
			       || ppp->conf.send_epdis < 0)) {
			ppp->conf.send_epdis = LCP_CO_ENDPOINT_MAC;
			p->cf_un.ep.class = LCP_CO_ENDPOINT_MAC;
			bcopy(&loc_mac, p->cf_un.ep_mac.addr,
			      sizeof(p->cf_un.ep_mac.addr));
			ep2ascii(loc_epdis, &p->cf_un.ep,
				  sizeof(p->cf_un.ep_mac));
			GEN_CO(p, LCP_CO_ENDPOINT_T, ep_mac);
		} else {
			ppp->conf.send_epdis = LCP_CO_ENDPOINT_LOC;
			p->cf_un.ep.class = LCP_CO_ENDPOINT_LOC;
			(void)strncpy(p->cf_un.ep_sgi.irix, "IRIX",
				      sizeof(p->cf_un.ep_sgi.irix));
			ADD_CO_LONG(p,sysid(0),ep_sgi.sn);
			ep2ascii(loc_epdis, &p->cf_un.ep,
				  sizeof(p->cf_un.ep_sgi));
			GEN_CO(p, LCP_CO_ENDPOINT_T, ep_sgi);
		}
		log_debug(2,LNAME, "   %s Endpoint Discriminator %s",
			  ep_info[ppp->conf.send_epdis].name, loc_epdis);
	}


	obuf.proto = PPP_LCP;
	obuf.bits = 0;
	OBUF_CP->code = PPP_CODE_CR;
	OBUF_CP->id = L.fsm.id;
	OBUF_CP->len = 4+((char*)p - (char*)BASE_P(OBUF_CP));

	ppp_send(ppp,&obuf,OBUF_CP->len);
}


/* FSM action to send a Terminate-Request,
 */
static void
lcp_str(struct ppp *ppp,
	struct fsm *fsm)
{
	(void)reset_accm(ppp,0);
	fsm_str(ppp,fsm);
}


/* Complain if the other guy rejects a protocol he asked for
 * or one we did not send
 */
static void
complain_rej(struct ppp *ppp,
	     int proto,
	     char *nm,
	     struct fsm *fsm)
{
	log_complain(LNAME,"receive mysterious Protocol-Reject "
		     "%sfor protocol %#x%s",
		     ibuf.bits == 0 ? "" : "(MP) ",
		     proto, nm);
	if (fsm != 0)
		fsm_run(ppp, fsm, FSM_RXJ_M);
}


/* process incoming LCP frame
 */
void
lcp_ipkt(struct ppp *ppp)
{
	/* change the ID after recognizing it to trash extras */
#define CK_ID() {if (fsm_ck_id(&L.fsm)) {	\
	L.cnts.bad_id++;			\
	return;					\
	}}
	int i;


	if (ibuf_info_len < (int)IBUF_CP->len) {
		log_complain(LNAME,"dropping %s with %d bytes but claiming %d",
			     fsm_code_name(IBUF_CP->code),
			     ibuf_info_len, IBUF_CP->len);
		L.cnts.bad_len++;	/* bad packet length */
		return;
	}

	switch (IBUF_CP->code) {
	case PPP_CODE_CR:		/* Configure-Request */
		if (ibuf.bits != 0) {	/* MP encapsulation is forbidden */
			log_ipkt(0,LNAME,"receive bad Configure-Request:");
			break;
		}
		fsm_run(ppp, &L.fsm, lcp_parse_cr(ppp));
		break;

	case PPP_CODE_CA:		/* Configure-Ack */
		if (ibuf.bits != 0) {	/* MP encapsulation is forbidden */
			log_ipkt(0,LNAME,"receive Configure-Ack:");
			break;
		}
		CK_ID();
		log_debug(2,LNAME,"receive Configure-ACK ID=%#x",IBUF_CP->id);
		L.bad_magic = 0;
		L.fsm.nak_recv = 0;
		fsm_run(ppp, &L.fsm, FSM_RCA);
		break;

	case PPP_CODE_CN:		/* Configure-Nak */
		if (ibuf.bits != 0) {	/* MP encapsulation is forbidden */
			log_ipkt(0,LNAME,"receive Configure-Nak:");
			break;
		}
		CK_ID();
		lcp_parse_cn(ppp);
		if (L.fsm.nak_recv > ppp->conf.max_fail) {
			log_complain(LNAME,"giving up after %d"
				     " Configure-NAKs or -Rejects",
				     L.fsm.nak_recv);
			fsm_run(ppp, &L.fsm, FSM_RXJ_M);
		} else {
			fsm_run(ppp, &L.fsm, FSM_RCN);
		}
		break;

	case PPP_CODE_CONFJ:		/* Configure-Reject */
		if (ibuf.bits != 0) {	/* MP encapsulation is forbidden */
			log_ipkt(0,LNAME,"receive Configure-Reject:");
			break;
		}
		CK_ID();
		lcp_parse_confj(ppp);
		if (L.fsm.nak_recv > ppp->conf.max_fail) {
			log_complain(LNAME,"giving up after %d"
				     " Configure-NAKs or Rejects",
				     L.fsm.nak_recv);
			fsm_run(ppp, &L.fsm, FSM_RXJ_M);
		} else {
			fsm_run(ppp, &L.fsm, FSM_RCN);
		}
		lcp_send_ident(ppp, 0);
		break;

	case PPP_CODE_TR:		/* Terminate-Request */
		log_ipkt(ibuf.bits == 0 ? 1 : 0,
			 LNAME,"receive Terminate-Request:");
		if (ibuf.bits != 0)	/* MP encapsulation is forbidden */
			break;
		fsm_run(ppp, &L.fsm, FSM_RTR);
		break;

	case PPP_CODE_TA:		/* Terminate-Ack */
		log_ipkt(ibuf.bits == 0 ? 2 : 0,
			 LNAME,"receive Terminate-Ack:");
		if (ibuf.bits != 0)	/* MP encapsulation is forbidden */
			break;
		fsm_run(ppp, &L.fsm, FSM_RTA);
		break;

	case PPP_CODE_CJ:		/* Code-Reject */
		/* If the peer is rejecting an identification packet,
		 * stop sending them.
		 */
		if (ibuf_info_len > 4+8
		    && IBUF_CP->lcp_un.cj.cj_code == PPP_CODE_IDENT) {
			log_debug(2,LNAME,"receive %s for %s",
				  fsm_code_name(PPP_CODE_CJ),
				  fsm_code_name(IBUF_CP->lcp_un.cj.cj_code));
			L.ident_off = 1;
			fsm_run(ppp, &L.fsm, FSM_RXJ_P);
			break;
		}
		L.cnts.rcvd_code_rej++;
		fsm_rcj(ppp, &L.fsm);
		break;

	case PPP_CODE_PJ:		/* Protocol-Reject */
		L.cnts.rcvd_prot_rej++;
		i = ((IBUF_CP->lcp_un.pj.proto[0]<<8)
		     + IBUF_CP->lcp_un.pj.proto[1]);
		switch (i) {
		case PPP_CCP:
			log_debug(1,LNAME,"receive Protocol-Reject for CCP");
			ccp_event(ppp, FSM_DOWN);
			ccp_event(ppp, FSM_CLOSE);
			fsm_run(ppp, &L.fsm, FSM_RXJ_P);
			break;

		case PPP_CP:
			log_debug(1,LNAME,"receive Protocol-Reject for CP");
			ccp_event(ppp, FSM_DOWN);
			ccp_event(ppp, FSM_CLOSE);
			fsm_run(ppp, &L.fsm, FSM_RXJ_P);
			break;

		case PPP_LCP:
			complain_rej(ppp,i,", LCP, ", &L.fsm);
			break;
		case PPP_IP:
			complain_rej(ppp,i,", IP, ", &L.fsm);
			break;
		case PPP_IPCP:
			complain_rej(ppp,i,", IPCP, ", &L.fsm);
			break;
		case PPP_VJC_COMP:
			complain_rej(ppp,i,", VJC_COMP, ", &L.fsm);
			break;
		case PPP_VJC_UNCOMP:
			complain_rej(ppp,i,", VJC_UNCOMP, ", &L.fsm);
			break;
		case PPP_PAP:
			complain_rej(ppp,i,", PAP, ", &L.fsm);
			break;
		case PPP_CHAP:
			complain_rej(ppp,i,", CHAP, ", &L.fsm);
			break;
		default:
			complain_rej(ppp,i,"", 0);
			break;
		}
		break;

	case PPP_CODE_EREQ:		/* Echo-Request */
	case PPP_CODE_EREP:		/* Echo-Reply */
		fsm_run(ppp, &L.fsm, FSM_RXR);
		break;

	case PPP_CODE_DIS:		/* Discard-Request */
		log_ipkt(2,LNAME,"receive Discard-Request:");
		L.cnts.rcvd_dis++;
		fsm_run(ppp, &L.fsm, FSM_RXR);
		break;

	case PPP_CODE_IDENT:
		i = MIN(IBUF_CP->len-4, PPP_IDENT_LEN);
		log_debug((L.ident_rcvd_len != i
			   || bcmp(&IBUF_CP->lcp_un.ident,&L.ident_rcvd,i)
			   ? 1 : 2),
			  LNAME, "receive Identification: ID=%#x %#x \"%s\"",
			  IBUF_CP->id,
			  IBUF_CP->lcp_un.ident.magic,
			  ascii_str(IBUF_CP->lcp_un.ident.msg,
				    i-sizeof(IBUF_CP->lcp_un.ident.magic)));
		bcopy(&IBUF_CP->lcp_un.ident, &L.ident_rcvd, i);
		L.ident_rcvd_len = i;
		fsm_run(ppp, &L.fsm, FSM_RXR);
		break;

	default:
		L.cnts.rcvd_bad++;
		log_complain(LNAME,"bad %s", fsm_code_name(IBUF_CP->code));
		fsm_run(ppp, &L.fsm, FSM_RUC);
		break;
	}
#undef CK_ID
}


/* FSM action to deal with Echo-Replies and similar noise
 */
static void
lcp_ser(struct ppp *ppp,
	struct fsm *fsm)
{
	struct timeval delta;
	float rtt;
	int hi, id;


	/* If the magic number is not what the peer said (or 0 if
	 * the peer did not say), the link is looped back or cross-
	 * connected.
	 */
	if (!L.bad_magic
	    && IBUF_CP->lcp_un.echo.magic != 0
	    && (ibuf.bits != 0		/* no magic numbers on MP */
		|| IBUF_CP->lcp_un.echo.magic != L.his_magic)) {
		L.bad_magic = 1;
		log_complain(LNAME,
			     "bad Echo-Request magic #=%#x instead of %#x"
			     "--possible loopback",
			     IBUF_CP->lcp_un.echo.magic,
			     L.his_magic);
	}

	/* respond to an Echo Request */
	if (IBUF_CP->code == PPP_CODE_EREQ) {
		log_debug(3,LNAME,"respond to Echo-Request ID=%#x",
			  IBUF_CP->id);
		IBUF_CP->code = PPP_CODE_EREP;
		IBUF_CP->lcp_un.echo.magic = ((ibuf.bits == 0
					       && fsm->state == FSM_OPENED_9)
					      ? L.my_magic : 0),
		IBUF_CP->len = MIN(MIN(L.neg_mtru, L.neg_mtu), IBUF_CP->len);
		ppp_send(ppp, &ibuf, IBUF_CP->len);
		return;
	}

	/* deal with an echo reply */
	if (IBUF_CP->code == PPP_CODE_EREP) {
		if (L.conf.echo_int == 0) {
			log_complain(LNAME,
				     "receive unexpected Echo-Reply ID=%#x",
				     IBUF_CP->id);
			return;
		}
		if (IBUF_CP->len != 4+sizeof(IBUF_CP->lcp_un.echo)) {
			log_complain(LNAME, "receive bogus Echo-Reply len=%d",
				     IBUF_CP->len);
			return;
		}
		id = IBUF_CP->id;
		hi = L.echo_next_id;
		if (hi < L.echo_old_id) {
			if (id < hi)
				id += 256;
			hi += 256;
		}
		if (L.echo_old_id > id || hi < id) {
			log_complain(LNAME, "receive bogus Echo-Reply ID=%#x"
				     " not in range [%d-%d)",
				     IBUF_CP->id,
				     L.echo_old_id, L.echo_next_id);
			return;
		}

		timevalsub(&delta, &rcv_msg_ts, &IBUF_CP->lcp_un.echo.ts);
		rtt = delta.tv_sec*1.0+delta.tv_usec/1000000.0;
		if (rtt <= 0
		    || rtt > (L.conf.echo_fail+1)*L.conf.echo_int*1.0) {
			log_complain(LNAME, "receive bogus Echo-Reply"
				     " timestamp with delta=%.3f sec",
				     rtt);
			return;
		}
		log_debug(3,LNAME,"receive Echo-Reply ID=%#x RTT=%.3f sec",
			  IBUF_CP->id, rtt);
		L.echo_rtt = rtt;
		L.echo_old_id = IBUF_CP->id+1;
		L.echo_seen = 1;
	}
}


/* send an Echo-Request to see if the peer is alive
 */
void
lcp_send_echo(struct ppp *ppp)
{
	u_char nechos;
	int secs;


	if (L.conf.echo_int == 0
	    || ppp->phase != NET_PHASE) {
		L.echo_timer.tv_sec = TIME_NEVER;
		return;
	}

	/* If we have sent several echos, and if there has been enough
	 * time for the peer to respond, either kill the link or stop
	 * trying.
	 */
	nechos = L.echo_next_id-L.echo_old_id;
	if (nechos >= L.conf.echo_fail) {
		L.echo_timer.tv_sec = TIME_NEVER;

		secs = nechos*L.conf.echo_int;
		if (!L.echo_seen) {
			/* If the peer never responds, eventually stop trying.
			 */
			log_debug(1,LNAME, "stop Echo-Requests after"
				  " %d in %d seconds failed",
				  nechos, secs);
		} else {
			/* If the peer stops responding, hang up the phone.
			 */
			log_complain(LNAME, "assume line is dead after"
				     " %d Echo-Requests failed in %d seconds",
				     nechos, secs);
			set_tr_msg(&L.fsm, "Echo-Replies stopped");
			lcp_event(ppp, FSM_CLOSE);
		}
		return;
	}

	obuf.proto = PPP_LCP;
	obuf.bits = 0;
	OBUF_CP->code = PPP_CODE_EREQ;
	OBUF_CP->id = L.echo_next_id++;
	OBUF_CP->len = 4+sizeof(OBUF_CP->lcp_un.echo);
	OBUF_CP->lcp_un.echo.magic = L.my_magic;
	get_clk();
	OBUF_CP->lcp_un.echo.ts = clk;
	log_debug(3,LNAME, "send Echo-Request ID=%#x", OBUF_CP->id);
	ppp_send(ppp, &obuf, OBUF_CP->len);

	fsm_settimer(&L.echo_timer, L.conf.echo_int*1000);
}


void
lcp_send_ident(struct ppp *ppp,
	       int magic)
{
	struct utsname buf;


	if (L.ident_off)
		return;

	bzero(&buf,sizeof(buf));
	(void)uname(&buf);

	obuf.proto = PPP_LCP;
	obuf.bits = 0;
	OBUF_CP->code = PPP_CODE_IDENT;
	OBUF_CP->id = ++L.ident_id;
	OBUF_CP->lcp_un.ident.magic = magic;
	OBUF_CP->len = 8+sprintf((char *)OBUF_CP->lcp_un.ident.msg,
				 "%s link #%d "RSTR" %s %s %s %s",
				 ourhost_nam, ppp->link_num,
				 buf.sysname, buf.release, buf.version,
				 buf.machine);
	log_debug(2,LNAME,"send Identification: ID=%#x %#x \"%s\"",
		  OBUF_CP->id,
		  OBUF_CP->lcp_un.ident.magic,
		  OBUF_CP->lcp_un.ident.msg);
	ppp_send(ppp, &obuf, OBUF_CP->len);
}
