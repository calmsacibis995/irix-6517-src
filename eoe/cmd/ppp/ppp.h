/* common definitions for user-level PPP code
 */

#ifndef __PPP_H__
#define __PPP_H__

#ident "$Revision: 1.29 $"


#include <utmp.h>
#include <sys/param.h>
#include <sys/stream.h>
#include "fsm.h"
#include "pputil.h"
#include <sys/if_ppp.h>

#define MD5_DIGEST_LEN 16
typedef struct {
	u_int32_t state[4];		/* state (ABCD) */
	u_int32_t count[2];		/* # of bits, modulo 2^64 (LSB 1st) */
	unsigned char buffer[64];	/* input buffer */
} MD5_CTX;
extern void MD5Init(MD5_CTX*);
extern void MD5Update(MD5_CTX*, u_char*, u_int);
extern void MD5Final(u_char[MD5_DIGEST_LEN], MD5_CTX*);


#ifdef DEFINE
#define PPPEXT
#define PPPDATA(x) = x
#else
#define PPPEXT extern
#define PPPDATA(x)
#endif

extern char version_str[];

#define ADV(p) (p = (STRUCT_CF*)(p->len+(char*)p))
#define GET_S(p,s) (((p)->cf_un.s[0]<<8) + (p)->cf_un.s[1])
#define GET_L(p,s) (((p)->cf_un.s[0]<<24) + ((p)->cf_un.s[1]<<16) \
		    + ((p)->cf_un.s[2]<<8) + (p)->cf_un.s[3])


/* add to a configure ACK, NAK, or Reject output buffer */
#define GEN_CO_BOOL(p, t)   {(p)->type = (t);			\
	(p)->len = 2;						\
	(p) = (STRUCT_CF*)&(p)->cf_un.info[0];}

#define	GEN_CO(p,t,s) ((p)->type = (t),				\
		       (p)->len = sizeof((p)->cf_un.s)+2,	\
		       (p) = (STRUCT_CF*)((char*)(p)+sizeof((p)->cf_un.s)+2))

#define ADD_CO_SHORT(p, v,s) ((p)->cf_un.s[0] = (v)>>8,		\
			      (p)->cf_un.s[1] = (u_char)(v))

#define ADD_CO_LONG(p, v,s)  ((p)->cf_un.s[0] = (v)>>24,	\
			      (p)->cf_un.s[1] = (v)>>16,	\
			      (p)->cf_un.s[2] = (v)>>8,		\
			      (p)->cf_un.s[3] = (u_char)(v))


/* All of these must fit in a default MTU, 1500 byte packet. */
#define AUTH_NAME_LEN	256
#define PASSWD_LEN	256

#define MAX_PAP_NAME_LEN sizeof(((struct utmp*)0)->ut_user)

struct ppp {
	struct dev dv;

	u_int   version;		/* version of this structure */

	u_int	conf_lnum;		/* initial control file line number */

	pid_t	mypid;

	/* per-link configuration values.  Others shared with SLIP
	 * are simple globals.
	 */
	struct {
	    int	    max_fail;		/* config-request failure limit */
	    int	    max_conf;		/* conf-req without ack,nak, or rej */
	    int	    max_term;		/* max attempts to terminate */

	    time_t  restart_ms;		/* FSM restart timer period in sec */
	    time_t  restart_ms_lim;	/* limit on exponential backoff */
	    time_t  stop_ms;		/* delay turning off link this long */

	    flg	    mp;			/* 1=MP ok */
	    flg	    mp_callee;		/* let callee add a link */
	    flg	    send_ssn;		/* 1=ask for short sequence #s */
	    flg	    recv_ssn;		/* 1=short sequence #s ok */
	    flg	    mp_headers;		/* 1=always MP encapsulate */
	    int	    frag_size;		/* target MP fragment size */
	    int	    ip_mtu;		/* output IP packet size */
	    int	    reasm_window;	/* limit MP fragments */
	    flg	    mp_nulls;		/* 1=use MP null fragments */

	    flg	    send_epdis;		/* Endpoint Discriminator type */
	    flg	    recv_epdis;		/* 1=use received Endpoint Discrim. */
	} conf;

	int	age;			/* how recently created */

	float	bps;			/* apparent link speed */
	float	conf_bps;

	flg	nowait;			/* 0=wait forever after rendezvous
					 * 1=wait for device to be taken
					 * 2=do not wait at all
					 */

	int	link_num;		/* MP or BF&I multilink number */
	char	link_name[8];

	flg	utmp_type;
#	define NO_UTMP	    0
#	define PPP_UTMP	    1
	flg	utmp_has_host;
	char	utmp_id[4+1];
	char	utmp_name[SYSNAME_LEN];

	char	rem_sysname[SYSNAME_LEN+1]; /* name of remote system */

	/* these are only for passing through the rendezvous pipe */
	char	loc_epdis[SYSNAME_LEN+1];
	char	rem_epdis[SYSNAME_LEN+1];
	struct sockaddr_in lochost;
	struct sockaddr_in remhost;

	flg	in_stop;		/* 0=input enabled,
					 * 1=SIOC_PPP_1_RX used,
					 * 2=input stopped */

	enum ppp_phase {		/* PPP "phases" */
	    DEAD_PHASE,
	    ESTAB_PHASE,
	    AUTH_PHASE,
	    TERM_PHASE,
	    NET_PHASE
	} phase;

	/* PPP link state machine */
	struct {
	    struct fsm fsm;

	    struct lcp_conf {		/* per-link configuration values */
		flg	pcomp;		/* 1=try Protocol-Field-Compression */
		flg	acomp;		/* 1=try Address-and-Control-Fields */
		int	echo_int;	/* probe the peer this often */
#		    define DEF_ECHO_INT 10  /* delay between Echo-Requests */
#		    define MIN_ECHO_INT 1
#		    define MAX_ECHO_INT	120
		int	echo_fail;
#		    define DEF_ECHO_RTT (RESTART_MS_MAX/1000)
		flg	ident_off;	/* 1=turn off Identification packets */
		PPP_FM  accm[PPP_ACCM_LEN];
	    } conf;

	    int	    min_mtu, min_mtru;
	    int	    tgt_mru, tgt_mrru;	/* target (multilink) MRRU, etc */
	    int	    tgt_mtu, tgt_mtru;
	    int	    neg_mru, neg_mrru;	/* negotiated (multilink) MRRU, etc */
	    int	    neg_mtu, neg_mtru;
	    flg	    seen_mru_nak_rej;	/* seen Nak or Reject for MRU */
	    flg	    seen_mp_rej;	/* 1=peer rejected MRRU */

	    flg	    seen_epdis_rej;	/* peer rejected E.P.Discriminator */

	    /* Async-Control-Character-Map */
	    PPP_FM  cr_accm;		/* in Configure-Request from peer */
	    PPP_FM  tx_accm[PPP_ACCM_LEN];
	    PPP_FM  rx_accm;		/* discard received XON and XOFF */
	    flg	    use_rx_accm;
#	     define RX_ACCM_OFF	0	/* do not discard unescaped bytes */
#	     define RX_ACCM_ON  1	/* discard unescaped bytes */
#	     define RX_ACCM_SET 2
#	     define RX_ACCM_DEF 3
	    flg	    parity_accm;	/* ignore parity bit for escaping */

	    flg	    prefer_recvpap;	/* 1=prefer PAP instead of CHAP */
	    int	    auth_suggest;	/* our protocol peer sent in Nak */
	    int	    auth_refuse;	/* our protocol peer did not like */

	    flg	    neg_pcomp;		/* 1=compress Protocol field */
	    flg	    seen_pcomp_rej;
	    flg	    neg_acomp;		/* 1=Address and Control Fields */
	    flg	    seen_acomp_rej;

	    flg	    refuse_mp;		/* 1=refuse to do MP */
	    flg	    neg_mp;		/* 1=have or might negotiate MP */
	    flg	    neg_send_ssn;	/* send short MP sequence #s */
	    flg	    neg_recv_ssn;	/* receive short MP seqeuenc #s */
	    flg	    refuse_recv_ssn;

	    __uint32_t  my_magic;
	    __uint32_t  his_magic;
	    flg	    bad_magic;		/* 1=magic #'s have implied loopback */


	    flg	    ident_off;		/* 1=turn off Identification packets */
	    u_char  ident_id;
	    int	    ident_rcvd_len;
	    struct ppp_ident ident_rcvd;

	    struct timeval echo_timer;
	    u_char  echo_next_id;
	    u_char  echo_old_id;	/* echo requests sent without answer */
	    flg	    echo_seen;		/* peer has answered at least once */
	    float   echo_rtt;		/* measured Round Trip Time */

	    struct ppp_arg_lcp arg_lcp_set; /* what the kernel has been told */
	    struct ppp_arg_mp arg_mp_set;

	    struct {
		int	bad_len;	/* bad LCP frame length */
		int	bad_id;		/* bad LCP frame identifier */
		int	rcvd_code_rej;	/* received Code-Reject frames */
		int	rcvd_prot_rej;	/* received Protocol-Reject */
		int	rcvd_dis;	/* received Discard-Requests */
		int	rcvd_bad;	/* unknown packet */
	    } cnts;
	} lcp;


	/* security state */
	struct {
	    char    name[8];

	    time_t  max_secs;		/* authentication limit */
	    time_t  reauth_secs;	/* periodic new CHAP authentication */
	    time_t  ms;			/* timer period in milliseconds */

	    u_char  pap_id;		/* ID on PAP requests we send */
	    u_char  chap_id;		/* ID on CHAP challenges we send */

	    flg	    want_sendpap;	/* 1=willing to send PAP password */
	    flg	    want_recvpap;	/* need to hear PAP password */

	    flg	    want_sendchap_response; /* willing to answer CHAP */
	    flg	    want_recvchap_response; /* want to challenge with CHAP */

	    flg	    have_send_passwd;	/* we have a password to send */

	    u_short our_proto;		/* authentication we require */
	    u_short peer_proto;		/* authentication demanded by peer */

	    flg	    peer_happy;		/* peer liked our password */
	    flg	    we_happy;		/* we like its credentials */

	    flg	    chap_chal_delayed;
	    flg	    pap_req_delayed;

	    struct recv_name {		/* choices for PAP or CHAP name */
		struct recv_name *nxt;
		char	nm[AUTH_NAME_LEN+1];
	    } recv_names;
	    char    recvd_name[AUTH_NAME_LEN+1];    /* actually received */
	    char    send_name[AUTH_NAME_LEN+1];	/* either PAP or CHAP */
	    char    send_passwd[PASSWD_LEN+1];	/* password or challenge */
	    char    recv_passwd[PASSWD_LEN+1];	/* response secret */

	    struct  cval {		/* last challenge value */
#		 define	CVAL_RND_L  8
		u_char	cval_rnd[CVAL_RND_L];	/* crypto random number */
#		 define	CVAL_NAM_L 4
		u_char	cval_nam[CVAL_NAM_L];	/* digested hostname */
#		 define	CVAL_DIR_L 4
		u_char	cval_dir[CVAL_DIR_L];	/* direction xor preceding */
#		 define CVAL_DIR_UNKN "Oops"
#		 define CVAL_DIR_CALLEE "rECV"
#		 define CVAL_DIR_CALLER "MaDe"
	    } chap_cval;

	    time_t  stime;		/* started authentication */
	    time_t  we_happy_time;	/* were happy about peer's password */
	    struct timeval timer;
	} auth;

	/* IP state machine */
	struct {
	    struct fsm fsm;

	    flg	    went_up;

	    struct {
		flg	no_addr;	/* avoid using ADDR and ADDRS */
		flg	rx_vj_comp;	/* 1=try VJ-compression */
		flg	rx_vj_compslot;	/* 1=compress the slot ID */
		int	rx_vj_slots;	/* maximum number of slots */
		flg	tx_vj_comp;
		flg	tx_vj_compslot;
		int	tx_vj_slots;
	    } conf;

	    flg	    rx_comp;
	    flg	    rx_compslot;
	    int	    rx_slots;
	    flg	    tx_comp;
	    flg	    tx_compslot;
	    int	    tx_slots;

	    flg	    seen_addr_rej;
#ifdef DO_ADDRS
	    flg	    seen_addrs_rej;
#endif

	    struct {
		int	bad_len;
		int	bad_id;
		int	rcvd_code_rej;
		int	rcvd_bad;
	    } cnts;
	} ipcp;


	/* Compression state machine */
	struct {
	    struct fsm fsm;

	    struct {
		time_t  restart_ms;

		u_char  tx;		/* SIOC_PPP_CCP_{PRED,LZW} */
		u_char  tx_bsd_bits;
		u_char  rx;
		u_char  rx_bsd_bits;

		int	max_rx_errors;
		int	max_tx_errors;
	    } conf;

	    u_int   neg_tx;
	    u_int   new_tx;

	    u_int   save_rx;		/* need to pick scheme when the
					 *	kernel is ready */

	    flg	    funny_ack;		/* 1=funny Ack sent, 2=give up */

	    flg	    bad_peer;

	    u_int   prev_rx;		/* previously agreed protocol */
	    u_int   nak_rx;		/* received Nak protocols */
	    u_int   skip_rx;		/* give up on these after many Naks */

	    int	    neg_tx_bsd_bits;

	    u_int   neg_rx;
	    u_int   rej_rx;

	    int	    neg_rx_bsd_bits;

	    flg	    seen_reset_rej;	/* got code-reject for reset */
	    flg	    reset_ack_pending;	/* waiting for Reset-Ack */
	    time_t  sent_reset_request;

	    u_char  rcvd_rreq_id;	/* last reset request ID seen */

	    struct {
		int	bad_id;
		int	bad_len;
		int	rcvd_code_rej;
		int	rcvd_bad;	/* unrecognized control packets */
		int	bad_ccp;	/* bad data packets received */
		int	rreq_sent;
		int	rreq_rcvd;
		int	rack_rcvd;
		float	rreq_rcvd_aged;
		float	rreq_sent_aged;
	    } cnts;
	} ccp;
};


PPPEXT enum connmodes connmode PPPDATA(UNKNOWN_CALL);

PPPEXT struct ppp ppp0;
PPPEXT struct dev *dp0 PPPDATA(&ppp0.dv);

PPPEXT int  ibuf_info_len;
PPPEXT struct ppp_buf ibuf;		/* recently received packet */
PPPEXT struct ppp_buf obuf;		/* common output buffer */
PPPEXT struct ppp_buf rejbuf;
PPPEXT struct ppp_buf nakbuf;

/* STRUCT_PKT must be #defined in file including this */
#define IBUF_CP ((STRUCT_PKT*)&ibuf.un)
#define OBUF_CP ((STRUCT_PKT*)&obuf.un)
#define JBUF_CP ((STRUCT_PKT*)&rejbuf.un)
#define NBUF_CP ((STRUCT_PKT*)&nakbuf.un)


PPPEXT char *cfilename PPPDATA("/etc/ppp.conf");

PPPEXT int assume_callee;

PPPEXT int arg_debug;

#define NUM_UUCP_NAMES MAX_PPP_LINKS
PPPEXT int num_uucp_nams;
PPPEXT char uucp_names[NUM_UUCP_NAMES][sizeof(ppp0.dv.uucp_nam)];

#define MAX_HOSTS 20
struct ok_host {
    __uint32_t  addr;
    __uint32_t  mask;
};

PPPEXT char rem_sysname[SYSNAME_LEN+1];	/* name of remote system */

PPPEXT char loc_epdis[SYSNAME_LEN+1];	/* Endpoint Discriminators */
PPPEXT char rem_epdis[SYSNAME_LEN+1];

struct ether_addr {u_char ether_addr_octet[6];};
PPPEXT struct ether_addr loc_mac;	/* MAC address for Discriminator */
PPPEXT flg have_loc_mac;		/* -1=impossible, 1=have one */

PPPEXT char ourhost_nam[MAXHOSTNAMELEN+1];  /* our hostname */

PPPEXT int def_remhost;			/* 1=defaulted remote IP address */
PPPEXT char remhost_str[sizeof("111.222.333.444")+1] PPPDATA("0");
PPPEXT int num_remhost_nams;
PPPEXT struct ok_host remhost_nams[MAX_HOSTS];

PPPEXT int def_lochost;			/* 1=defaulted local IP address */
PPPEXT char lochost_str[sizeof("111.222.333.444")+1] PPPDATA("0");
PPPEXT int num_lochost_nams;
PPPEXT struct ok_host lochost_nams[MAX_HOSTS];

PPPEXT int reconfigure;

PPPEXT flg no_utmp;			/* do not create a utmp entry */

PPPEXT int qmax;			/* maximum IF queue length */

PPPEXT int bigxmit;			/* TOS packet length hack--0=off,
					 * -1=default value, or value */
PPPEXT int telnettos;			/* turn on TOS port # hack */

PPPEXT time_t busy_delay;
PPPEXT time_t idle_delay;

#define MAXMAXDEVS MAX_PPP_LINKS	/* devices per multi-link bundle */
PPPEXT int numdevs;			/* currently active devices */

PPPEXT int mindevs, conf_mindevs;	/* try to keep this many going */
PPPEXT int conf_in_mindevs;
PPPEXT int outdevs, conf_outdevs;	/* ok to call if fewer than this */
PPPEXT int conf_in_outdevs;
PPPEXT int maxdevs, conf_maxdevs;	/* never more than this many */

PPPEXT int mp_on;			/* 1=are or will be using MP */
PPPEXT int mp_known;			/* 1=mp_on valid */
PPPEXT u_char mp_ncp_bits;		/* MP bits used with NCP packets */
PPPEXT int mp_recv_ssn;
PPPEXT int mp_send_ssn;
PPPEXT struct ppp *mp_ppp;		/* state machines for MP bundle */

PPPEXT int noicmp;			/* >0 if dropping ICMP */

PPPEXT struct afilter *afilter;

PPPEXT int kern_ip_mtu PPPDATA(-1);	/* MTU kernel should put in if_net */

#define DEF_BSD_BITS 12

#define DEF_WINDOW (60*1024)		/* common default TCP window */

PPPEXT flg conf_proxy_arp;		/* 1=add ARP table entry,
					 * 2=delete when finished */
PPPEXT char *proxy_arp_if;		/* try to use this interface */


extern int parse_conf(int);

extern void proxy_arp(void);
extern void proxy_unarp(void);
extern char *if_mac_addr(u_long, char *, struct ether_addr *);

extern int do_strioctl(struct ppp*, int, struct ppp_arg*, char*);
extern int do_strioctl_ok(struct ppp*, int, u_char, char*);

extern u_long newmagic(void);
extern void ppp_send(struct ppp*, struct ppp_buf*, int);
extern void dologin(struct ppp*);
extern void hangup(struct ppp*);

extern void lcp_param(struct ppp*);
extern int lcp_set(struct ppp*, int);
extern int reset_accm(struct ppp*, int);
extern void lcp_init(struct ppp*);
extern void lcp_pj(struct ppp*, char*);
extern void lcp_event(struct ppp*, enum fsm_event);
extern void lcp_ipkt(struct ppp*);
extern char *phase_name(enum ppp_phase);
extern void log_phase(struct ppp*, enum ppp_phase);
extern void log_ipkt(int, char*, char*);
extern void lcp_send_echo(struct ppp*);
extern void lcp_send_ident(struct ppp*, int);

extern void auth_init(struct ppp*);
extern void auth_start(struct ppp*);
extern void auth_time(struct ppp*);
extern void auth_done(struct ppp*);
extern void pap_ipkt(struct ppp*);
extern void chap_ipkt(struct ppp*);

extern int link_mux(struct ppp*);
extern int set_mp(struct ppp*);
extern void do_rend(struct ppp*);
extern char *epdis_msg(char*, char*, char*);

extern char *ip2str(u_long);
extern void ipcp_param(struct ppp*);
extern void ipcp_init(struct ppp*);
extern void ipcp_ipkt(struct ppp*);
extern void ipcp_go(struct ppp*);
extern void ipcp_event(struct ppp*, enum fsm_event);

extern void ccp_clear(struct ppp*);
extern void ccp_init(struct ppp*);
extern void ccp_ipkt(struct ppp*);
extern void ccp_event(struct ppp*, enum fsm_event);
extern void ccp_blip(struct ppp*, int);
extern void ccp_blip_age(struct ppp* ppp);
extern int ccp_go(struct ppp*);


#undef PPPEXT
#undef PPPDATA
#endif /* __PPP_H__ */
