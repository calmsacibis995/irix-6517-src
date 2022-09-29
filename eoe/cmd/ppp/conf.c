/* Silicon Graphics PPP
 *	Parse control file.
 */

#ident "$Revision: 1.26 $"


#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <protocols/routed.h>

#include "ppp.h"

#include "keyword.h"

static char *get_host(char*, struct ok_host*, int*);
static void init_afilter(void);

/* cannot escape this byte */
#define NOT_ESC (PPP_FLAG ^ PPP_ESC_BIT)
#define CK_ESC(c) ((c) == NOT_ESC || ((c) ^ PPP_ESC_BIT) <  0x20)

static FILE *cfile;

#define WSPC	(EOF-1)			/* whitespace */
#define EOE	(WSPC-1)		/* end of entry */
#define EQU	(EOE-1)			/* equal sign */
#define ERR	(EQU-1)

#define MAX_CFILE_LEN 9999

#define KWBUF_SIZE 64
static char kwbuf[KWBUF_SIZE+1];	/* keyword buffer */
#define VBUF_SIZE 1024
static int vlen;
static char vbuf[VBUF_SIZE+1];		/* values for keywords */

static int version, lnum, lnum_delayed, max_lnum, looping;

static struct {
	int null;
	int stray_equ;
	int too_long;
	int initial;
} complained;


/* Allow more than one character to be un-read, unlike ungetc(3).
 *	Only 2 slots are needed, but be generous since more is free.
 *	This only buffers raw characters, and not WSPC etc.
 */
#define GBUF_LEN 8
static int gbuf[8], gbuf_in, gbuf_out;

#define GETB()	    ((gbuf_in == gbuf_out) ? getc(cfile)	    \
		     : gbuf[gbuf_out = (gbuf_out+1)%GBUF_LEN])
#define UNGETB(c)   (gbuf[gbuf_in = (gbuf_in+1) % GBUF_LEN] = c)


static char mval[] = "missing value for %s %s";
static char bval[] = "bad \"%s=%s\" %s";
static char tmany[] = "too many \"%s\" values %s";
static char tmany_str[] = "too many \"%s\" values %s%s";
static char tlong[] = "\"%s=%s\" is too long %s";

static char*
nearfile(int ln)
{
	static char buf[256];

	(void)sprintf(buf, "near line %d in %.200s", ln, cfilename);
	return buf;
}

static char*
prev_str(char *str)
{
	static char buf[256];

	if (!str)
		buf[0] = '\0';
	else
		(void)sprintf(buf, "; previous value=\"%.200s\"", str);
	return buf;
}


/* Skip a sequence of whitespace characters, incuding '='.
 *	Be certain '=' does not cross newlines
 */
static int				/* WSPC, EQU, EOE, or EOF */
skip_whitespace(int type,		/* WSPC or EQU */
		int c)			/* next byte */
{
	for (;;) {
		switch (c) {
		case ' ':
		case '\t':
			c = GETB();
			break;

		case '#':
			do {
				c = GETB();
			} while (c != '\n' && c != EOF);
			if (c == EOF)
				break;
			/* fall into newline case */
		case '\n':
			if (type == EQU) {
				UNGETB(c);
				return EQU;
			}
			++lnum_delayed;
			/* A newline that is followed by a newline, blank,
			 * tab, or a comment is whitespace but a newline
			 * followed by anything else ends an entry.
			 */
			c = GETB();
			if (c != ' ' && c != '\t' && c != '#' && c != '\n') {
				UNGETB(c);
				return EOE;
			}
			type = WSPC;
			break;

		case '=':
			/* ignore tabs and blanks before and after '=',
			 * but not newlines
			 */
			if (type != WSPC || lnum_delayed != 0) {
				UNGETB(c);
				return type;
			}
			type = EQU;
			c = GETB();
			break;;

		case EOF:
			return (type == EQU) ? EQU : EOF;

		default:
			UNGETB(c);
			return type;
		}
	}
}



/* Get the next character from the file, including dealing with escapes
 * and compressing whitespace.
 */
static int
next_char(int esc,			/* 1=honor backslash */
	  int quoting)			/* 1=blanks and '#' are literal */
{
	int c, c1;


	lnum += lnum_delayed;
	lnum_delayed = 0;

	c = GETB();
	switch (c) {
	case '\t':
	case ' ':
		return quoting ? c : skip_whitespace(WSPC, c);
	case '=':
		if (quoting)
			return c;
		return skip_whitespace(EQU, GETB());
	case '#':
		if (quoting)
			return c;
		return skip_whitespace(WSPC, c);
	case '\n':
		return skip_whitespace(WSPC, c);

	case '\\':
		/* Escapes only in the new version, and only
		 * in values for keywords.
		 */
		if (version < 2 || !esc)
			return c;
		c = GETB();
		if (c == EOF || c == '\n') {
			UNGETB(c);	/* no escaping newline or EOF */
			return '\\';
		}
		if (c == 'n')
			return '\n';
		if (c == 'r')
			return '\r';
		if (c == 't')
			return '\t';
		if (c == 'b')
			return '\b';
		if (c >= '0' && c <= '7') {
			c -= '0';
			c1 = GETB();
			if (c1 < '0' || c1 > '7') {
				UNGETB(c1);
				break;
			}
			c = (c<<3)+(c1 - '0');
			c1 = GETB();
			if (c1 < '0' || c1 > '7') {
				UNGETB(c1);
				break;
			}
			c = (c<<3)+(c1 - '0');
		}
		break;
	}

	if (c == '\0') {
		if (!complained.null++)
			log_complain("","illegal NUL (\\0) %s",
				     nearfile(lnum));
		return '?';
	}
	return c;
}


/* skip to the start of the next entry in the control file by skipping
 * to the end of the current entry.
 */
static void
next_entry(void)
{
	int c;

	do {
		c = next_char(0,0);
	} while (c != EOE && c != EOF);
}


/* Get the next word in the current entry.
 *	A word is a label or keyword (including =xxx), a newline ending
 *	the current entry, or EOF.
 *
 *	Unless searching for a label, EOF is reported as EOE
 */
static int				/* WSPC, EOE, or EOF */
next_word(int label)			/* 1=looking for label */
{
	int c, i, quoting;

	vlen = -1;
	vbuf[0] = '\0';
	i = 0;
	do {
		c = next_char(0,0);
		switch (c) {
		case EQU:
			kwbuf[i] = '\0';
			if (label) {
				if (!complained.stray_equ++)
					log_complain("","syntax error near"
						     " \"%s\" %s",
						     &kwbuf[MAX(i-8,0)],
						     nearfile(lnum));
				return ERR;
			}
			i = 0;
			quoting = 0;
			for (;;) {
				if (i >= VBUF_SIZE) {
					log_complain("",
						     "value for \"%s\" too big"
						     " %s",
						     kwbuf, nearfile(lnum));
					return ERR;
				}
				c = next_char(1,quoting);
				if (c == EOE || c == WSPC || c == ERR)
					break;
				if (c == EQU) {
					log_complain("","syntax error near"
						     " \"%s\" %s",
						     &kwbuf[MAX(i-8,0)],
						     nearfile(lnum));
					return ERR;
				}
				if (c == '"') {
					/* simplistic quoted string */
					if (i == 0 && !quoting) {
					    quoting = 1;
					    continue;
					} else if (quoting) {
					    quoting = 0;
					    continue;
					}
				}
				if (c == EOF) {
					c = EOE;
					break;
				}
				vbuf[i++] = c;
			}
			vbuf[vlen = i] = '\0';
			if (quoting && c != ERR)
				log_complain("", "missing \" %s",
					     nearfile(lnum));
			return c;

		case WSPC:
			if (i == 0)
				continue;
			kwbuf[i] = '\0';
			return c;

		case ERR:
			kwbuf[i] = '\0';
			return ERR;

		case EOF:
		case EOE:
			kwbuf[i] = '\0';
			return label ? c : EOE;

		default:
			kwbuf[i++] = c;
			break;
		}
	} while (i < KWBUF_SIZE);

	if (!label || !complained.too_long++)
		log_complain("","word too long near \"%.8s\" %s",
			     kwbuf, nearfile(lnum));
	return ERR;
}


static int				/* 0=bad */
need_v(void)
{
	if (vlen <= 0) {
		log_complain("",mval, kwbuf,nearfile(lnum));
		return 0;
	}
	return 1;
}


static int				/* 0=bad */
get_num(uint_t *pi,			/* put the number here */
	uint_t min,			/* limits on the number */
	uint_t max)
{
	char *p;
	uint_t i;

	if (!need_v())
		return 0;
	i = strtoul(vbuf,&p,0);
	if (*p != '\0') {
		log_complain("",bval, kwbuf,vbuf,nearfile(lnum));
		return 0;
	}
	if (i < min) {
		log_complain("","%s too small for %s %s",
			     vbuf,kwbuf,nearfile(lnum));
		return 0;
	}
	if (i > max) {
		log_complain("","%s too large for %s %s",
			     vbuf,kwbuf,nearfile(lnum));
		return 0;
	}

	*pi = i;
	return 1;
}


#define	COMPLAIN_TMANY() log_complain("",tmany, kwbuf, nearfile(lnum))
#define	COMPLAIN_TMANY_STR(old) log_complain("",tmany_str, kwbuf,	\
					     nearfile(lnum), prev_str(old))
#define	COMPLAIN_TLONG() log_complain("",tlong, kwbuf,vbuf, nearfile(lnum))

#define NEED_V() {if (!need_v()) break;}

#define GET_NUM(min,max) {						\
	if (!get_num((uint_t*)&i,(uint_t)min,(uint_t)max)) break;}

#define CK_STR(nam,len) {						\
	NEED_V();							\
	if (nam[0] != '\0' && strcmp(nam,vbuf)) COMPLAIN_TMANY_STR(nam);\
	if ((int)strlen(vbuf) > len) {COMPLAIN_TLONG(); break;}}

#define CK_NUM_MIN(nam,val,min) {					\
	if ((nam) >= (min) && (nam) != (val)) COMPLAIN_TMANY();		\
	(nam) = (val);}

#define CK_NUM(nam,val) CK_NUM_MIN(nam,val,0)

#define CK_NUM_BAIL(nam,val) {						\
	if (nam >= 0 && nam != val) {COMPLAIN_TMANY(); break;}		\
	nam = val;}

static char tmodes[] = "\"in\",\"out\",\"quiet\"";
static char bhost[] = "%s in \"%s=%s\"";



int					/* !=0 when need to reconfigure */
parse_conf(int reconfig_mode)		/* 0=normal, 1=reconfigure */
{
	int i, c;
	u_long l;
	char *p;
	int lo, hi, tgt;
	struct servent *sp;
	char* tline;
	char cont_buf[SYSNAME_LEN+2];
	int cont_lnum;
	static char conf_pat1[] = "debug=%d mode=%s%s"
				  " mindevs=%d,outdevs=%d,maxdevs=%d"
				  " active_timeout=%d,inactive=%d";
	char conf_str[sizeof(conf_pat1)+7*8+8];
	static char conf_pat2[]="debug=%d mode=%s%s mindevs=%d,out=%d,max=%d";
	int conf_debug;
	struct recv_name *nmp;
	struct lcp_conf lcp_conf;
	u_char set_rx_ccp, set_tx_ccp;
	int set_del_route;
	int max_term_ms;
	struct stat sbuf;

	tline = remote;

	bzero(&sbuf,sizeof(sbuf));

	ppp0.version = CONF_VERSION;

	conf_debug = 0;
	camping = 0;
	reconfigure = 0;

	/* detect changes in some parameters from a reconfigure
	 */
	bcopy(&ppp0.lcp.conf, &lcp_conf, sizeof(lcp_conf));

	/* Define some parameters from scratch both at first and
	 * after a reconfiguration.
	 * Some of these, such as the ACCM, are just too much mess
	 * to default to the pre-reconfigure values.
	 */
	ppp0.conf.max_fail = DEF_MAX_FAIL;  /* failure limit */
	ppp0.conf.max_conf = DEF_MAX_CONF;  /* conf-req without response */
	ppp0.conf.max_term = -1;	/* max attempts to terminate */
	max_term_ms = -1;
	ppp0.conf.restart_ms_lim = 0;
	ppp0.conf.restart_ms = 0;
	bzero(ppp0.lcp.conf.accm, sizeof(ppp0.lcp.conf.accm));
	ppp0.lcp.parity_accm = 0;
	ppp0.lcp.use_rx_accm = RX_ACCM_DEF;

	/* use pre-reconfigure values as defaults for many other parameters
	 */

	set_del_route = 0;

	set_rx_ccp = 0;
	set_tx_ccp = 0;
	if (!reconfig_mode) {
		bzero(&complained, sizeof(complained));

		def_remhost = 1;
		def_lochost = 1;
		num_remhost_nams = 0;
		num_lochost_nams = 0;
		num_uucp_nams = 0;
		remhost.sin_addr.s_addr = 0;
		lochost.sin_addr.s_addr = 0;
		netmask.sin_addr.s_addr = 0;

		metric = 0;

		busy_delay = BEEP/HZ*2;	/* default 10 seconds */
		idle_delay = BEEP/HZ*6;	/* default 30 seconds */
		sactime = 0;
		lactime = 0;
		ppp0.dv.acct.toll_bound = -1;

		noicmp = 0;

		conf_del_route = 1;
		conf_proxy_arp = -1;

		ppp0.dv.modwait = 0;
		ppp0.dv.modtries = DEFAULT_MODTRIES;
		ppp0.dv.modpause = -1;

		conf_mindevs = -1;
		conf_in_mindevs = -1;
		conf_maxdevs = -1;
		conf_outdevs = -1;
		conf_in_outdevs = -1;

		bigxmit = 0;		/* packet size TOS hack--off */
		telnettos = 1;		/* port # TOS hack */
		qmax = -1;

		ppp0.dv.conf_sync = ppp0.dv.sync = SYNC_DEFAULT;

		if (0 > gethostname(&ourhost_nam[0], sizeof(ourhost_nam))) {
			log_errno("gethostnam","");
			exit(1);
		}

		ppp0.lcp.conf.pcomp = -1;
		ppp0.lcp.conf.acomp = -1;
		ppp0.lcp.conf.echo_int = -1;
		ppp0.lcp.conf.echo_fail = -1;

		ppp0.auth.max_secs = 30;
		ppp0.auth.reauth_secs = 0;
		ppp0.auth.ms = 3*1000;

		ppp0.lcp.prefer_recvpap = 0;
		ppp0.auth.want_recvpap = -1;
		ppp0.auth.want_sendpap = -1;
		ppp0.auth.want_recvchap_response = -1;
		ppp0.auth.want_sendchap_response = -1;

		ppp0.conf.mp = -1;
		ppp0.conf.send_ssn = 0;
		ppp0.conf.recv_ssn = 0;
		ppp0.conf.mp_headers = 0;
		ppp0.conf.frag_size = 0;
		ppp0.conf.ip_mtu = -1;
		ppp0.conf.reasm_window = -1;
		ppp0.conf.mp_nulls = -1;
		ppp0.conf.send_epdis = -1;
		ppp0.conf.recv_epdis = -1;

		ppp0.ipcp.conf.no_addr = -1;
		ppp0.ipcp.conf.rx_vj_comp = 1;
		ppp0.ipcp.conf.rx_vj_slots = DEF_VJ_SLOT;
		ppp0.ipcp.conf.rx_vj_compslot = 0;

		ppp0.ipcp.conf.tx_vj_comp = 1;
		ppp0.ipcp.conf.tx_vj_slots = DEF_VJ_SLOT;
		ppp0.ipcp.conf.tx_vj_compslot = 0;

		ppp0.ccp.conf.rx |= (SIOC_PPP_CCP_BSD | SIOC_PPP_CCP_PRED1);
		ppp0.ccp.conf.rx_bsd_bits = DEF_BSD_BITS;
		ppp0.ccp.conf.tx |= (SIOC_PPP_CCP_BSD | SIOC_PPP_CCP_PRED1);
		ppp0.ccp.conf.tx_bsd_bits = DEF_BSD_BITS;
		ppp0.ccp.conf.max_rx_errors = -1;
		ppp0.ccp.conf.max_tx_errors = -1;
	}


	looping = 0;			/* machinery to detect infinite */
	max_lnum = 0;			/*	continue=xxx loops */

	ppp0.conf_lnum = 0;		/* # of the main line of the entry */
cont_line:;
	cfile = fopen(cfilename, "r");
	if (!cfile) {
		log_complain("","cannot open \"%s\"--assume defaults",
			     cfilename);
		c = EOF;

	} else {
		if (sbuf.st_ino == 0) {
			if (0 > fstat(fileno(cfile), &sbuf)) {
				log_errno("fstat",cfilename);
				exit(1);
			}
			if (sbuf.st_uid != 0
			    || (sbuf.st_mode & (S_IRWXG|S_IRWXO)) != 0)
				log_complain("","security problem: %s"
					     " is readable by more than root",
					     cfilename);
		}

		/* Ensure the first entry is labeled,
		 * to catch an easy to make error.
		 */
		lnum_delayed = 0;
		lnum = 1;
		version = 0;
		gbuf_in = 0;
		gbuf_out = 0;
		c = next_char(0,0);
		switch (c) {
		case WSPC:
			c = next_word(0);
			if (!strcasecmp(kwbuf,"version")) {
				if (c == ERR) {
					;
				} else if (c != EOE) {
					log_complain("",
						     "bogus \"version=%s\" %s",
						     vbuf, nearfile(lnum));
					next_entry();
				} else {
					(void)get_num((uint_t*)&version, 2,2);
				}
				break;
			}
			/* fall into EQU case */
		case EQU:
			if (!complained.initial++)
				log_complain("",
					     "missing label on initial entry"
					     " %s", nearfile(lnum));
			next_entry();
			break;

		case ERR:
			next_entry();
			break;

		case EOF:
		case EOE:
			break;

		default:
			UNGETB(c);
			break;
		}

		/* search for the desired entry */
		for (;;) {
			c = next_word(1);
			if (c == ERR) {
				next_entry();
				continue;
			}
			if (!strcasecmp(kwbuf, tline)
			    && tline[0] != '\0') {
				/* if this is the desired entry,
				 * stop looking */
				break;
			}

			if (c == EOF) {
				/* hit EOF without finding the right label */
				if (tline != cont_buf) {
					log_complain("","failed to find label"
						     " \"%s\" in %s"
						     "--assume defaults",
						     tline, cfilename);
				} else {
					log_complain("","failed to find"
						     " \"continue %s\" %s",
						     tline,
						     nearfile(cont_lnum));
				}
				c = EOE;
				break;
			}

			if (c != EOE)
				next_entry();
		}
	}

	cont_buf[0] = '\0';

	while (c != EOE) {
		c = next_word(0);
		if (c == ERR)
			break;

		/* binary search the list of keywords */
		lo = 0;
		hi = KEYTBL_LEN-1;
		for (;;) {
			tgt = lo + (hi-lo)/2;
			i = strcasecmp(keytbl[tgt].str, kwbuf);
			if (i < 0) {
				lo = tgt+1;
			} else if (i == 0) {
				/* found it */
				if (keytbl[tgt].flag && vlen > 0)
					tgt = KEYTBL_LEN-1; /* syntax error */
				break;
			} else {
				hi = tgt-1;
			}
			if (hi < lo) {
				tgt = KEYTBL_LEN-1; /* force syntax error */
				break;
			}
		}

		switch (keytbl[tgt].key) {
		case KEYW_CONTINUE:
			CK_STR(cont_buf, SYSNAME_LEN);
			if (++looping > (max_lnum+1)*2) {
				log_complain("","continuation loop near "
					     "line %d and \"%s\" in %s",
					     lnum,vbuf,cfilename);
				break;
			}
			(void)strncpy(cont_buf,vbuf,sizeof(cont_buf)-1);
			cont_buf[sizeof(cont_buf)-1] = '\0';
			cont_lnum = lnum;
			break;

		case KEYW_DEBUG:
			if (vlen < 0) {
				i = 1;
			} else {
				GET_NUM(0,100);
			}
			if (conf_debug < i)
				conf_debug = i;
			break;

		case KEYW_MODWAIT:
			GET_NUM(1,HEARTBEAT*5);
			CK_NUM_MIN(ppp0.dv.modwait, i, 1);
			break;

		case KEYW_MODTRIES:
			GET_NUM(1,30);
			ppp0.dv.modtries = i;
			break;

		case KEYW_MODPAUSE:
			GET_NUM(1,86400);
			CK_NUM(ppp0.dv.modpause, i);
			break;

		case KEYW_RESTART_MS:
			i = (ppp0.conf.restart_ms_lim == 0
			     ? RESTART_MS_MAX
			     : ppp0.conf.restart_ms_lim);
			GET_NUM(RESTART_MS_MIN,i);
			ppp0.conf.restart_ms = i;
			break;

		case KEYW_RESTART_MS_LIM:
			i = (ppp0.conf.restart_ms == 0
			     ? RESTART_MS_MIN
			     : ppp0.conf.restart_ms);
			GET_NUM(i,RESTART_MS_MAX);
			ppp0.conf.restart_ms_lim = i;
			break;

		case KEYW_CCP_RESTART_MS:
			GET_NUM(RESTART_MS_MIN,RESTART_MS_MAX);
			ppp0.ccp.conf.restart_ms = i;
			break;

		case KEYW_STOP_MS:
			GET_NUM(RESTART_MS_MIN,RESTART_MS_MAX);
			ppp0.conf.stop_ms = i;
			break;

		case KEYW_MAX_FSM_FAIL:
			GET_NUM(0,100);
			ppp0.conf.max_fail = i;
			break;

		case KEYW_MAX_FSM_CONF:
			GET_NUM(0,100);
			ppp0.conf.max_conf = i;
			break;

		case KEYW_MAX_FSM_TERM_MS:
			GET_NUM(RESTART_MS_MIN,RESTART_MS_MAX);
			if (ppp0.conf.max_term >= 0
			    || max_term_ms != i) {
				COMPLAIN_TMANY();
			}
			max_term_ms = i;
			break;

		case KEYW_MAX_FSM_TERM:
			GET_NUM(1,100);
			if (ppp0.conf.max_term != i
			    || max_term_ms >= 0) {
				COMPLAIN_TMANY();
			}
			ppp0.conf.max_term = i;
			break;

		case KEYW_MAX_AUTH_SECS:
			GET_NUM(1,30);
			ppp0.auth.max_secs = i;
			break;

		case KEYW_AUTH_SECS:
			GET_NUM(1,30);
			ppp0.auth.ms = i*1000;
			break;

		case KEYW_CHAP_REAUTH_SECS:
			GET_NUM(10,3600*2);
			ppp0.auth.reauth_secs = i;
			break;

/* For the authentication machinery, we need
 *	- list of names that are acceptible, for either PAP or CHAP
 *	    responses from the peer.
 *	- one name and one password to send to the peer for PAP.
 *	- one name and one secret to respond to the peer for CHAP.
 *	- whether PAP, CHAP, or both are acceptible in each direction.
 *
 * CHAP could use 4 names and 2 secrets, one triple of challenge name,
 * response name, and secret for each direction of the authentication.
 * However, to minimize user confusion, only two names (one for each
 * computer) and two secrets are permitted.
 *
 * Whether a name is required for CHAP or PAP is determined by whether
 * CHAP or PAP is specified.
 *
 * "Reconfigure" requires that the peer send us a name by either PAP or
 * CHAP.  Reconfigure is only used with in-coming calls, and so it can
 * and does give the peer a chance to send a PAP request first, and
 * then uses the name in that request to decide which (if any) control
 * file entry to use.  The PAP name and password to be sent to the
 * peer can be in the incoming entry (the one with "reconfigure"), but
 * that forces all peers to accept the same name and password from this
 * machine.  It is better to put the PAP "send_name" and "send_passwd"
 * in the control entry selected via "reconfigure" named by the PAP
 * username received from the peer.
 *
 * When CHAP is selected, 'reconfigure' uses the name from a CHAP response
 * or challenge to pick the true control file entry.  To get a CHAP
 * response, you must first send a CHAP challenge.  Thus, the CHAP
 * "send_name" specification must be known (specified explicitly or
 * defaulted from the hostname) in the incoming entry.   However, the
 * CHAP "secret" specifications should be in the control entry selected
 * via reconfigure.
 */
		case KEYW_RECV_NAME:
		case KEYW_RECV_USERNAME:    /* obsolete */
			/* can have no effect after reconfigure */
			if (reconfig_mode)
				break;
			if (vlen <= 0) {
				ppp0.auth.want_recvpap = 1;
			} else {
				if ((int)strlen(vbuf) > AUTH_NAME_LEN) {
					COMPLAIN_TLONG();
					break;
				}
				if (ppp0.auth.recv_names.nm[0] == '\0') {
					nmp = &ppp0.auth.recv_names;
				} else {
					nmp = (struct recv_name*
					       )malloc(sizeof(*nmp));
					nmp->nxt = ppp0.auth.recv_names.nxt;
					ppp0.auth.recv_names.nxt = nmp;
				}
				(void)strcpy(nmp->nm, vbuf);
			}
			break;

		case KEYW_SEND_NAME:
		case KEYW_SEND_USERNAME:    /* obsolete */
			CK_STR(ppp0.auth.send_name, AUTH_NAME_LEN);
			if (!vlen)
				log_complain("","CHAP names cannot and PAP"
					     " usernames should not be null"
					     " %s", nearfile(lnum));
			(void)strcpy(ppp0.auth.send_name, vbuf);
			break;

		case KEYW_SEND_PASSWD:
			CK_STR(ppp0.auth.send_passwd, PASSWD_LEN);
			if (!vlen)
				log_complain("","CHAP secrets cannot and PAP"
					     " passwords should not be null"
					     " %s", nearfile(lnum));
			(void)strcpy(ppp0.auth.send_passwd, vbuf);
			ppp0.auth.have_send_passwd = 1;
			break;

		case KEYW_RECV_PASSWD:
			CK_STR(ppp0.auth.recv_passwd, PASSWD_LEN);
			if (!vlen)
				log_complain("","CHAP secrets cannot be null"
					     " %s", nearfile(lnum));
			(void)strcpy(ppp0.auth.recv_passwd, vbuf);
			break;

		case KEYF_SEND_PAP:
			CK_NUM(ppp0.auth.want_sendpap, 1);
			break;

		case KEYF__SEND_PAP:
			CK_NUM(ppp0.auth.want_sendpap, 0);
			break;

		case KEYF_SEND_CHAP:
			CK_NUM(ppp0.auth.want_sendchap_response, 1);
			break;

		case KEYF__SEND_CHAP:
			CK_NUM(ppp0.auth.want_sendchap_response, 0);
			break;

		case KEYF_RECV_PAP:
			CK_NUM(ppp0.auth.want_recvpap, 1);
			break;

		case KEYF__RECV_PAP:
			CK_NUM(ppp0.auth.want_recvpap, 0);
			break;

		case KEYF_RECV_CHAP:
			CK_NUM(ppp0.auth.want_recvchap_response, 1);
			break;

		case KEYF__RECV_CHAP:
			CK_NUM(ppp0.auth.want_recvchap_response, 0);
			break;

		case KEYF__UTMP:
			no_utmp = 1;
			break;

		case KEYF_RECONFIGURE:
			if (!reconfig_mode)
				reconfigure = 1;
			break;

		case KEYF_CAMP:
			if (!reconfig_mode && !assume_callee)
				camping = 1;
			break;

		case KEYW_NETMASK:
			NEED_V();
			if (inet_aton(vbuf, &netmask.sin_addr) <= 0) {
				netmask.sin_addr.s_addr = 0;
				log_complain("",bval,
					     kwbuf,vbuf, nearfile(lnum));
			}
			break;

		case KEYW_METRIC:
			GET_NUM(0, HOPCNT_INFINITY);
			metric = i;
			break;

		case KEYF__LCP_IDENT:	/* turn off identification packets */
			ppp0.lcp.conf.ident_off = 1;
			break;

		case KEYF_NOICMP:	/* do not send ICMP packets */
			noicmp = 1;
			break;

		case KEYW_QMAX:
			GET_NUM(3,DEF_WINDOW/PPP_MIN_MTU);
			CK_NUM(qmax, i);
			break;

		case KEYW_BIGXMIT:
			GET_NUM(PPP_MIN_MTU+1,PPP_MAX_MTU);
			bigxmit = i;
			break;

		case KEYF__TELNETTOS:
			telnettos = 0;
			break;

		case KEYW_ACTIVE_TIMEOUT:
			if (sactime != 0) {
				GET_NUM(sactime, 60*60*24*30);
				lactime = i;
			} else {
				GET_NUM(BEEP/HZ, 60*60*24*30);
				sactime = lactime = i;
			}
			break;

		case KEYW_INACTIVE_TIMEOUT:
			if (lactime != 0) {
				GET_NUM(BEEP/HZ, lactime);
				sactime = i;
			} else {
				GET_NUM(BEEP/HZ, 60*60*24*30);
				sactime = lactime = i;
			}
			break;

		case KEYW_TOLL_BOUNDARY:
			GET_NUM(1, 86400);
			CK_NUM(ppp0.dv.acct.toll_bound, i);
			break;

		case KEYW_BUSY_DELAY:
			GET_NUM(BEEP/HZ,60*60*24*30);
			busy_delay = i;
			break;

		case KEYW_IDLE_DELAY:
			GET_NUM(BEEP/HZ,60*60*24*30);
			idle_delay = i;
			break;

		case KEYW_BPS:
			GET_NUM(300,155000000);
			ppp0.conf_bps = ppp0.bps = i;
			break;

		case KEYF__INACT_PORT:
			if (!afilter)
				init_afilter();
			bzero(afilter->port,sizeof(afilter->port));
			break;

		case KEYW_INACT_PORT:
			NEED_V();
			if (!afilter)
				init_afilter();
			if (0 != (sp = getservbyname(vbuf, "tcp"))) {
				SET_PORT(afilter, sp->s_port);
			} else if (0 != (sp = getservbyname(vbuf, "udp"))) {
				SET_PORT(afilter, sp->s_port);
			} else {
				GET_NUM(1,65535)
				SET_PORT(afilter, i);
			}
			break;

		case KEYF__INACT_ICMP:
			if (!afilter)
				init_afilter();
			bzero(afilter->icmp,sizeof(afilter->icmp));
			break;

		case KEYW_INACT_ICMP:
			if (!afilter)
				init_afilter();
			GET_NUM(1,ICMP_MAXTYPE);
			SET_ICMP(afilter, i);
			break;

		case KEYW_MAP_CHAR_NUM:
			GET_NUM(0,255);
			if (CK_ESC(i)) {
				log_complain("","cannot escape %s %s",
					     vbuf,nearfile(lnum));
			} else {
				PPP_SET_ACCM(ppp0.lcp.conf.accm,i);
				if (ppp0.lcp.use_rx_accm == RX_ACCM_DEF)
					ppp0.lcp.use_rx_accm = RX_ACCM_ON;
			}
			break;

		case KEYW_MAP_CHAR:
			NEED_V();
			for (p = vbuf; *p != 0; p++) {
				i = *p ^ '@';
				if (CK_ESC(i)) {
				    log_complain("", "cannot escape"
						 "control-%c %s",
						 i, nearfile(lnum));
				} else {
					PPP_SET_ACCM(ppp0.lcp.conf.accm, i);
				    if (ppp0.lcp.use_rx_accm == RX_ACCM_DEF)
					ppp0.lcp.use_rx_accm = RX_ACCM_ON;
				}
				++p;
			}
			break;

		case KEYW_ACCM:
			GET_NUM(0,0xffffffff);
			ppp0.lcp.conf.accm[0] |= i;
			if (ppp0.lcp.use_rx_accm == RX_ACCM_DEF)
				ppp0.lcp.use_rx_accm = RX_ACCM_ON;
			break;

		case KEYF_ACCM_PARITY:
			ppp0.lcp.parity_accm = 1;
			if (ppp0.lcp.use_rx_accm == RX_ACCM_DEF)
				ppp0.lcp.use_rx_accm = RX_ACCM_ON;
			break;

		case KEYF_RX_ACCM:
			ppp0.lcp.use_rx_accm = RX_ACCM_ON;
			break;

		case KEYF__RX_ACCM:
			ppp0.lcp.use_rx_accm = RX_ACCM_OFF;
			break;

		case KEYF_XON_XOFF:
			PPP_SET_ACCM(ppp0.lcp.conf.accm, CSTART);
			PPP_SET_ACCM(ppp0.lcp.conf.accm, CSTOP);
			if (ppp0.lcp.use_rx_accm == RX_ACCM_DEF)
				ppp0.lcp.use_rx_accm = RX_ACCM_ON;
			ppp0.dv.xon_xoff = 1;
			break;

		case KEYF_SYNC:
			if (!reconfig_mode) {
				if (ppp0.dv.conf_sync != SYNC_DEFAULT
				    && ppp0.dv.conf_sync != SYNC_ON)
					COMPLAIN_TMANY();
				ppp0.dv.conf_sync = ppp0.dv.sync = SYNC_ON;
			}
			break;

		case KEYF__SYNC:
			if (!reconfig_mode) {
				if (ppp0.dv.conf_sync != SYNC_DEFAULT
				    && ppp0.dv.conf_sync!= SYNC_OFF)
					COMPLAIN_TMANY();
				ppp0.dv.conf_sync = ppp0.dv.sync = SYNC_OFF;
			}
			break;

		case KEYF_PCOMP:
			CK_NUM(ppp0.lcp.conf.pcomp, 1);
			break;

		case KEYF__PCOMP:
			CK_NUM(ppp0.lcp.conf.pcomp, 0);
			break;

		case KEYF_ACOMP:
			CK_NUM(ppp0.lcp.conf.acomp, 1);
			break;

		case KEYF__ACOMP:
			CK_NUM(ppp0.lcp.conf.acomp, 0);
			break;

		case KEYF__LCP_ECHOS:
			CK_NUM(ppp0.lcp.conf.echo_int, 0);
			break;

		case KEYW_LCP_ECHO_INTERVAL:
			GET_NUM(MIN_ECHO_INT, MAX_ECHO_INT);
			CK_NUM(ppp0.lcp.conf.echo_int,i);
			break;

		case KEYW_LCP_ECHO_FAILURES:
			GET_NUM(1,999999);
			CK_NUM(ppp0.lcp.conf.echo_fail,i);
			break;

		case KEYW_MAXDEVS:
			if (connmode == CALLEE) {
				GET_NUM(MAX(1, MAX(conf_mindevs,
						    conf_outdevs)),
					 MAXMAXDEVS);
			} else {
				GET_NUM(MAX(1, MAX(conf_in_mindevs,
						    conf_in_outdevs)),
					 MAXMAXDEVS);
			}
			CK_NUM(conf_maxdevs,i);
			break;

		case KEYW_OUTDEVS:
			GET_NUM(MAX(1, conf_mindevs),
				 conf_maxdevs>0 ? conf_maxdevs : MAXMAXDEVS);
			CK_NUM(conf_outdevs,i);
			break;

		case KEYW_IN_OUTDEVS:
			GET_NUM(MAX(1, conf_in_mindevs),
				 conf_maxdevs>0 ? conf_maxdevs : MAXMAXDEVS);
			CK_NUM(conf_in_outdevs,i);
			break;

		case KEYW_MINDEVS:
			GET_NUM(1, conf_maxdevs>0 ? conf_maxdevs : MAXMAXDEVS);
			CK_NUM(conf_mindevs, i);
			break;

		case KEYW_IN_MINDEVS:
			GET_NUM(1, conf_maxdevs>0 ? conf_maxdevs : MAXMAXDEVS);
			CK_NUM(conf_in_mindevs, i);
			break;

		case KEYF__VJ_COMP:
			ppp0.ipcp.conf.rx_vj_comp = 0;
			ppp0.ipcp.conf.tx_vj_comp = 0;
			break;

		case KEYF__RX_VJ_COMP:
			ppp0.ipcp.conf.rx_vj_comp = 0;
			break;

		case KEYF__TX_VJ_COMP:
			ppp0.ipcp.conf.tx_vj_comp = 0;
			break;

		case KEYF_VJ_COMPSLOT:
			ppp0.ipcp.conf.rx_vj_compslot = 1;
			ppp0.ipcp.conf.tx_vj_compslot = 1;
			break;

		case KEYF_RX_VJ_COMPSLOT:
			ppp0.ipcp.conf.rx_vj_compslot = 1;
			break;

		case KEYF_TX_VJ_COMPSLOT:
			ppp0.ipcp.conf.tx_vj_compslot = 1;
			break;

		case KEYF__VJ_COMPSLOT:
			ppp0.ipcp.conf.rx_vj_compslot = 0;
			ppp0.ipcp.conf.tx_vj_compslot = 0;
			break;

		case KEYF__RX_VJ_COMPSLOT:
			ppp0.ipcp.conf.rx_vj_compslot = 0;
			break;

		case KEYF__TX_VJ_COMPSLOT:
			ppp0.ipcp.conf.tx_vj_compslot = 0;
			break;

		case KEYW_VJ_SLOTS:
			GET_NUM(MIN_VJ_SLOT, MAX_VJ_SLOT);
			ppp0.ipcp.conf.rx_vj_slots = i;
			ppp0.ipcp.conf.tx_vj_slots = i;
			break;

		case KEYW_RX_VJ_SLOTS:
			GET_NUM(MIN_VJ_SLOT, MAX_VJ_SLOT);
			ppp0.ipcp.conf.rx_vj_slots = i;
			break;

		case KEYW_TX_VJ_SLOTS:
			GET_NUM(MIN_VJ_SLOT, MAX_VJ_SLOT);
			ppp0.ipcp.conf.tx_vj_slots = i;
			break;

		case KEYF__ADDR_NEGOTIATE:
			CK_NUM(ppp0.ipcp.conf.no_addr, 1);
			break;

		case KEYW_RX_BSD:
			if (vlen < 0) {
				i = 12;
			} else {
				GET_NUM(MIN_BSD_BITS,MAX_BSD_BITS)
			}
			if (0 != (set_rx_ccp
				  & ppp0.ccp.conf.rx
				  & SIOC_PPP_CCP_BSD))
				COMPLAIN_TMANY();
			set_rx_ccp |= SIOC_PPP_CCP_BSD;
			ppp0.ccp.conf.rx_bsd_bits = i;
			ppp0.ccp.conf.rx |= SIOC_PPP_CCP_BSD;
			break;

		case KEYF__RX_BSD:
			if (0 != (set_rx_ccp
				  & ppp0.ccp.conf.rx
				  & SIOC_PPP_CCP_BSD))
				COMPLAIN_TMANY();
			set_rx_ccp |= SIOC_PPP_CCP_BSD;
			ppp0.ccp.conf.rx &= ~SIOC_PPP_CCP_BSD;
			break;

		case KEYW_TX_BSD:
			if (vlen < 0) {
				i = 12;
			} else {
				GET_NUM(MIN_BSD_BITS,MAX_BSD_BITS)
			}
			if (0 != (set_tx_ccp
				  & ppp0.ccp.conf.tx
				  & SIOC_PPP_CCP_BSD))
				COMPLAIN_TMANY();
			set_tx_ccp |= SIOC_PPP_CCP_BSD;
			ppp0.ccp.conf.tx_bsd_bits = i;
			ppp0.ccp.conf.tx |= SIOC_PPP_CCP_BSD;
			break;

		case KEYF__TX_BSD:
			if (0 != (set_tx_ccp
				  & ppp0.ccp.conf.tx
				  & SIOC_PPP_CCP_BSD))
				COMPLAIN_TMANY();
			set_tx_ccp |= SIOC_PPP_CCP_BSD;
			ppp0.ccp.conf.tx &= ~SIOC_PPP_CCP_BSD;
			break;

		case KEYF_RX_PREDICTOR1:
			if (0 != (set_rx_ccp
				  & ~ppp0.ccp.conf.rx
				  & SIOC_PPP_CCP_PRED1))
				COMPLAIN_TMANY();
			ppp0.ccp.conf.rx |= SIOC_PPP_CCP_PRED1;
			break;

		case KEYF__RX_PREDICTOR1:
		case KEYF__RX_PREDICTOR:	/* deprecated */
			if (0 != (set_rx_ccp
				  & ppp0.ccp.conf.rx
				  & SIOC_PPP_CCP_PRED1))
				COMPLAIN_TMANY();
			set_rx_ccp |= SIOC_PPP_CCP_PRED1;
			ppp0.ccp.conf.rx &= ~SIOC_PPP_CCP_PRED1;
			break;

		case KEYF_TX_PREDICTOR1:
			if (0 != (set_tx_ccp
				  & ~ppp0.ccp.conf.tx
				  & SIOC_PPP_CCP_PRED1))
				COMPLAIN_TMANY();
			ppp0.ccp.conf.tx |= SIOC_PPP_CCP_PRED1;
			break;

		case KEYF__TX_PREDICTOR1:
		case KEYF__TX_PREDICTOR:	/* deprecated */
			if (0 != (set_tx_ccp
				  & ppp0.ccp.conf.tx
				  & SIOC_PPP_CCP_PRED1))
				COMPLAIN_TMANY();
			set_tx_ccp |= SIOC_PPP_CCP_PRED1;
			ppp0.ccp.conf.tx &= ~SIOC_PPP_CCP_PRED1;
			break;

		case KEYF__CCP:
			if ((set_rx_ccp & ppp0.ccp.conf.rx) != 0
			    || (set_tx_ccp & ppp0.ccp.conf.tx) != 0)
				COMPLAIN_TMANY();
			ppp0.ccp.conf.rx = 0;
			ppp0.ccp.conf.tx = 0;
			set_rx_ccp = SIOC_PPP_CCP_PROTOS;
			set_tx_ccp = SIOC_PPP_CCP_PROTOS;
			break;

		case KEYW_CCP_MAX_RX_ERRORS:
			GET_NUM(0,1000);
			CK_NUM(ppp0.ccp.conf.max_rx_errors, i);
			break;

		case KEYW_CCP_MAX_TX_ERRORS:
			GET_NUM(0,1000);
			CK_NUM(ppp0.ccp.conf.max_tx_errors, i);
			break;

		case KEYW_MTU:		/* MTU */
			GET_NUM(PPP_MIN_MTU,PPP_MAX_MTU);
			ppp0.conf.ip_mtu = i;
			break;

		case KEYF_UNSAFE_MP:	/* let callee add a link */
			ppp0.conf.mp_callee = 1;
			break;

		case KEYF_MP:		/* IETF MP */
			CK_NUM_BAIL(ppp0.conf.mp, 1);
			break;

		case KEYF__MP:
			CK_NUM_BAIL(ppp0.conf.mp, 0);
			break;

		case KEYF_MP_SEND_SSN:	/* send short MP sequence #s */
			if (!reconfig_mode)
				ppp0.conf.send_ssn = 1;
			break;

		case KEYF_MP_RECV_SSN:	/* receive short MP sequence #s */
			if (!reconfig_mode)
				ppp0.conf.recv_ssn = 1;
			break;

		case KEYF_MP_HEADERS:
			if (!reconfig_mode)
				ppp0.conf.mp_headers = 1;
			break;

		case KEYF__MP_FRAG:
			CK_NUM(ppp0.conf.frag_size, PPP_MAX_MTU);
			break;

		case KEYW_MP_FRAG_SIZE:
			GET_NUM(PPP_MIN_MTU,PPP_MAX_MTU);
			CK_NUM(ppp0.conf.frag_size, i);
			break;

		case KEYF__MP_NULLS:
			ppp0.conf.mp_nulls = 0;
			break;

		case KEYW_MP_REASM_WINDOW:
			GET_NUM(1,100);
			CK_NUM(ppp0.conf.reasm_window, i);
			break;

		case KEYF__ENDPOINT_DISCRIMINATOR:
			if (!reconfig_mode
			    || ppp0.conf.send_epdis < 0)
				CK_NUM(ppp0.conf.send_epdis, 0);
			CK_NUM(ppp0.conf.recv_epdis, 0);
			break;

		case KEYF__SEND_ENDPOINT_DISCRIMINATOR:
			if (!reconfig_mode
			    || ppp0.conf.send_epdis < 0)
				CK_NUM(ppp0.conf.send_epdis, 0);
			break;

		case KEYF_RECV_ENDPOINT_DISCRIMINATOR:
			CK_NUM(ppp0.conf.recv_epdis, 1);
			break;

		case KEYF__RECV_ENDPOINT_DISCRIMINATOR:
			CK_NUM(ppp0.conf.recv_epdis, 0);
			break;

		case KEYF_SEND_MAC_ENDPOINT_DISCRIMINATOR:
			CK_NUM(ppp0.conf.send_epdis, LCP_CO_ENDPOINT_MAC);
			break;

		case KEYF_SEND_LOC_ENDPOINT_DISCRIMINATOR:
			CK_NUM(ppp0.conf.send_epdis, LCP_CO_ENDPOINT_LOC);
			break;

		case KEYF_SEND_IP_ENDPOINT_DISCRIMINATOR:
			CK_NUM(ppp0.conf.send_epdis, LCP_CO_ENDPOINT_IP);
			break;

		case KEYW_ADD_ROUTE:
			if (add_route) {
				COMPLAIN_TMANY_STR(add_route);
				free(add_route);
			}
			if (vlen < 0) {
				strcpy(vbuf,"-");
			} else if (strncmp(vbuf,"add ",4)) {
				log_complain("","\"%s\" does not start with"
					     " \"add\" %s",
					     vbuf, nearfile(lnum));
			}
			add_route = strdup(vbuf);
			break;

		case KEYF__DEL_ROUTE:
			if (set_del_route > 0
			    && conf_del_route)
				COMPLAIN_TMANY_STR(del_route);
			if (del_route) {
				free(del_route);
				del_route = 0;
			}
			conf_del_route = 0;
			set_del_route = 1;
			break;

		case KEYW_DEL_ROUTE:
			if (set_del_route > 0
			    && (!conf_del_route
				|| (vlen >= 0 && del_route != 0
				    && strcmp(vbuf, del_route))))
				COMPLAIN_TMANY_STR(del_route);
			if (vlen >= 0) {
				if (del_route)
					free(del_route);
				del_route = strdup(vbuf);
			}
			conf_del_route = 1;
			set_del_route = 1;
			break;

		case KEYW_START_CMD:
			NEED_V();
			if (start_cmd != 0) {
				COMPLAIN_TMANY_STR(start_cmd);
				free(start_cmd);
			}
			start_cmd = strdup(vbuf);
			break;

		case KEYW_END_CMD:
			NEED_V();
			if (end_cmd != 0) {
				COMPLAIN_TMANY_STR(end_cmd);
				free(end_cmd);
			}
			end_cmd = strdup(vbuf);
			break;

		case KEYW_PROXY_ARP:
			if (conf_proxy_arp == 0
			    || (vlen >= 0 && proxy_arp_if != 0
				&& strcmp(vbuf, proxy_arp_if)))
				COMPLAIN_TMANY_STR(proxy_arp_if);
			if (vlen >= 0) {
				if (proxy_arp_if)
					free(proxy_arp_if);
				proxy_arp_if = strdup(vbuf);
			}
			conf_proxy_arp = 1;
			break;

		case KEYF__PROXY_ARP:
			if (conf_proxy_arp > 0)
				COMPLAIN_TMANY_STR(proxy_arp_if);
			if (proxy_arp_if) {
				free(proxy_arp_if);
				proxy_arp_if = 0;
			}
			conf_proxy_arp = 0;
			break;

		case KEYW_STREAM_MODULE:
			NEED_V();
			if (!reconfig_mode) {
				if (num_smods >= MAX_SMODS) {
					COMPLAIN_TMANY();
					break;
				}
				smods[num_smods++] = strdup(vbuf);
			}
			break;

		case KEYW_UUCP_NAME:
			NEED_V();
			if (num_uucp_nams >= NUM_UUCP_NAMES) {
				COMPLAIN_TMANY();
				break;
			}
			i = strlen(vbuf);
			if (i > sizeof(uucp_names[0])-1) {
				log_complain("","UUCP name %s is too long",
					     vbuf);
				break;
			}
			if (i > SYSNSIZE)
				log_complain("","only the first %d bytes of "
					     "%s are significant as a "
					     "UUCP name %s",
					     SYSNSIZE, vbuf, nearfile(lnum));
			(void)strncpy(uucp_names[num_uucp_nams++], vbuf,
				      sizeof(uucp_names[0])-1);
			break;

		case KEYW_LOCHOST:	/* obsolete */
		case KEYW_LOCALHOST:
			NEED_V();
			if (num_lochost_nams >= MAX_HOSTS) {
				COMPLAIN_TMANY();
				break;
			}
			p = get_host(vbuf, lochost_nams, &num_lochost_nams);
			if (p)
				log_complain("",bhost, p,kwbuf,vbuf);
			else
				def_lochost = 0;
			break;

		case KEYW_REMOTEHOST:
			NEED_V();
			if (num_remhost_nams >= MAX_HOSTS) {
				COMPLAIN_TMANY();
				break;
			}
			p = get_host(vbuf, remhost_nams, &num_remhost_nams);
			if (p)
				log_complain("",bhost, p,kwbuf,vbuf);
			else
				def_remhost = 0;
			break;

		case KEYW_REM_SYSNAME:
			CK_STR(ppp0.rem_sysname, SYSNAME_LEN);
			strncpy(ppp0.rem_sysname,vbuf,
				sizeof(ppp0.rem_sysname));
			break;

		case KEYF_IN:
			if (reconfig_mode || assume_callee)
				break;
			if (connmode != UNKNOWN_CALL
			    && connmode != CALLEE) {
				log_complain("",tmany,
					     tmodes, nearfile(lnum));
			} else {
				connmode = CALLEE;
				no_interact();
			}
			break;

		case KEYF_OUT:
			if (reconfig_mode || assume_callee)
				break;
			if (connmode != UNKNOWN_CALL
			    && connmode != CALLER) {
				log_complain("",tmany,
					     tmodes, nearfile(lnum));
			} else {
				connmode = CALLER;
			}
			break;

		case KEYF_QUIET:
			if (reconfig_mode || assume_callee)
				break;
			if (connmode != UNKNOWN_CALL
			    && connmode != Q_CALLER) {
				log_complain("",tmany,
					     tmodes, nearfile(lnum));
			} else {
				connmode = Q_CALLER;
			}
			break;

		default:
			log_complain("","syntax error at \"%s\" %s",
				     kwbuf,nearfile(lnum));
			break;
		}
	}

	if (cfile != 0) {
		/*  A continue=xxx line will result in reopening the file
		 * and rescanning it from the beginning.
		 */
		(void)fclose(cfile);
		cfile = 0;

		if (lnum > max_lnum)
			max_lnum = lnum;
		if (!ppp0.conf_lnum)
			ppp0.conf_lnum = lnum;
	}

	/* if there was a continuation, use it
	 */
	if (cont_buf[0] != '\0') {
		tline = cont_buf;
		goto cont_line;
	}

	if (debug < arg_debug + conf_debug)
		debug = arg_debug + conf_debug;
	SET_Debug();

	if (!reconfig_mode) {
		if (!afilter)
			init_afilter();

		smods[num_smods++] = "ppp_fram";

		if (camping && connmode == CALLEE) {
			log_complain("","cannot camp as callee");
			camping = 0;;
		}

		if (camping && connmode == Q_CALLER) {
			log_complain("","cannot camp in quiet mode");
			camping = 0;;
		}
	}

	if (connmode == Q_CALLER || sactime != 0 || lactime != 0) {
		if (sactime == 0)
			sactime = (lactime
				   ? MIN(DEFAULT_SACTIME, lactime)
				   : DEFAULT_SACTIME);
		if (lactime == 0)
			lactime = MAX(DEFAULT_LACTIME, sactime);
	}
	if (connmode == UNKNOWN_CALL)
		connmode = CALLER;


	/* default the remote host name */
	if (def_remhost
	    && !reconfigure) {
		tline = ((ppp0.rem_sysname[0] != '\0')
			 ? ppp0.rem_sysname
			 : remote);
		p = get_host(tline, remhost_nams, &num_remhost_nams);
		if (p != 0) {
			for (i = 0; i < num_uucp_nams; i++) {
				p = get_host(tline = uucp_names[i],
					     remhost_nams, &num_remhost_nams);
				if (p == 0)
					break;
			}
		}
		if (p == 0) {
#ifdef _HAVE_SIN_LEN
			remhost.sin_len = _SIN_ADDR_SIZE;
#endif
			remhost.sin_family = AF_INET;
			remhost.sin_addr.s_addr = remhost_nams[0].addr;
			(void)strcpy(remhost_str,
				     ip2str(remhost.sin_addr.s_addr));
			log_debug(1,"", "assume remote host %s, %s",
				  tline, remhost_str);
		} else {
			log_debug(1,"", "no remote IP address; "
				  "hope for negotiation");
			(void)get_host("0", remhost_nams, &num_remhost_nams);
		}
	}

	if (num_uucp_nams == 0
	    && !reconfigure) {
		(void)strncpy(uucp_names[0], remote,
			      sizeof(uucp_names[0])-1);
		num_uucp_nams = 1;
		if ((int)strlen(remote) > SYSNSIZE
		    && connmode != CALLEE)
			log_complain("","only the first %d bytes of %s are "
				     "significant as a UUCP name",
				     SYSNSIZE, remote);
	}

	/* If a single remote host name was spedified, resolve its address
	 * to prevent its negotiation.
	 */
	if (num_remhost_nams == 1
	    && remhost_nams[0].mask == (u_long)-1) {
#ifdef _HAVE_SIN_LEN
		remhost.sin_len = _SIN_ADDR_SIZE;
#endif
		remhost.sin_family = AF_INET;
		remhost.sin_addr.s_addr = remhost_nams[0].addr;
		(void)strcpy(remhost_str, ip2str(remhost.sin_addr.s_addr));
	}


	/* default the local hostname */
	if (def_lochost
	    && !reconfigure
	    && get_host(ourhost_nam, lochost_nams, &num_lochost_nams) != 0) {
		log_complain("","cannot get local IP address");
		exit(1);
	}

	/* If a single local host name was specified, resolve its address
	 * to prevent its negotiation.
	 */
	if (num_lochost_nams == 1
	    && lochost_nams[0].mask == (u_long)-1) {
#ifdef _HAVE_SIN_LEN
		lochost.sin_len = _SIN_ADDR_SIZE;
#endif
		lochost.sin_family = AF_INET;
		lochost.sin_addr.s_addr = lochost_nams[0].addr;
		(void)strcpy(lochost_str, ip2str(lochost.sin_addr.s_addr));
	}

	/* insist on IP addresses if we must start quietly */
	if (connmode == Q_CALLER) {
		if (lochost.sin_addr.s_addr == 0
		    || remhost.sin_addr.s_addr == 0) {
			log_complain("",
				     "QUIET calls require fixed addresses");
			connmode = CALLER;
		} else {
			def_lochost = 0;
			def_remhost = 0;
		}
	}

	if (ppp0.ipcp.conf.no_addr > 0
	    && !reconfigure) {
		if (lochost.sin_addr.s_addr == 0) {
			log_complain("","local IP adddress must be known if"
				     " negotiation disabled");
			ppp0.ipcp.conf.no_addr = 0;
		} else if (remhost.sin_addr.s_addr == 0) {
			log_complain("","remote IP adddress must be known if"
				     " negotiation disabled");
			ppp0.ipcp.conf.no_addr = 0;
		} else if (!reconfigure) {
			def_lochost = 0;
			def_remhost = 0;
		}
	}


	/* try to find a system name for rendezvous
	 */
	if (!reconfigure) {
		/* If the remote IP address is known, then we can use the
		 * name of the system to rendezvous.
		 */
		if (ppp0.rem_sysname[0] == '\0'
		    && remhost.sin_addr.s_addr != 0
		    && !def_remhost)
			strncpy(ppp0.rem_sysname,remote,
				sizeof(ppp0.rem_sysname));
		if (ppp0.rem_sysname[0] != '\0') {
			strcpy(rem_sysname,ppp0.rem_sysname);
			(void)add_rend_name("",ppp0.rem_sysname);
		}
	}


	/* If no escapes were requested, do not use a receive map.
	 * In other words, do not discard unescaped control characters.
	 */
	if (ppp0.lcp.use_rx_accm == RX_ACCM_DEF)
		ppp0.lcp.use_rx_accm = RX_ACCM_OFF;

	/* if ignoring the parity bit for mapping, then duplicate the map */
	if (ppp0.lcp.parity_accm) {
		for (i = 0; i < PPP_ACCM_LEN/2; i++) {
			l = ppp0.lcp.conf.accm[i];
			if (i == NOT_ESC/SFM)
				l &= ~(1 << (NOT_ESC%SFM));
			ppp0.lcp.conf.accm[i+PPP_ACCM_LEN/2] |= l;
		}
	}

	/* default FSM restart timer base and exponential backoff limit */
	if (ppp0.conf.restart_ms == 0) {
		if (ppp0.conf.restart_ms_lim == 0) {
			ppp0.conf.restart_ms = RESTART_MS_DEF;
		} else {
			ppp0.conf.restart_ms = MIN(ppp0.conf.restart_ms_lim,
						   RESTART_MS_DEF);
		}
	}
	if (ppp0.conf.restart_ms_lim == 0)
		ppp0.conf.restart_ms_lim = MAX(ppp0.conf.restart_ms,
					       RESTART_MS_LIM_DEF);
	if (ppp0.conf.stop_ms == 0)
		ppp0.conf.stop_ms = STOP_MS_DEF;

	/* Queue delays from uncompressed traffic slow down CCP.
	 * A short timeout causes retransmissions which cause lost
	 * packets.
	 */
	if (ppp0.ccp.conf.restart_ms == 0)
		ppp0.ccp.conf.restart_ms = MAX(6000,ppp0.conf.restart_ms);

	/* Default or compute the Terminate-Request restart counter.
	 */
	if (ppp0.conf.max_term < 0) {
		if (max_term_ms < 0)
			max_term_ms = DEF_MAX_TERM_MS;
		ppp0.conf.max_term = 0;
		i = 0;
		do {
			i += (ppp0.conf.restart_ms
			      << (ppp0.conf.max_term++));
		} while (i < max_term_ms);
	}


	/* If we want to receive a password, then since incoming
	 * PAP passwords are only /etc/passwd, we must want the peer
	 * to send us a CHAP response.
	 *
	 * If we want to receive a username, but neither PAP nor CHAP was
	 * chosen, then default to PAP.
	 *
	 * Prefer PAP instead of CHAP for upward compatibility.
	 */
	if ((ppp0.auth.recv_names.nm[0] != '\0'
	     || ppp0.auth.recv_passwd[0] != '\0')
	    && ppp0.auth.want_recvchap_response <= 0
	    && (ppp0.auth.want_recvpap <= 0
		|| ppp0.auth.recv_passwd[0] != '\0')) {
		/* do not let assumptions override explicit requests */
		if (ppp0.auth.want_recvpap > 0
		    && ppp0.auth.want_recvchap_response <= 0)
			ppp0.lcp.prefer_recvpap = 1;
		/* given a username but no password, then prefer PAP */
		if (ppp0.auth.recv_passwd[0] == '\0')
			ppp0.lcp.prefer_recvpap = 1;

		if (ppp0.auth.recv_passwd[0] != '\0'
		    && ppp0.auth.want_recvchap_response < 0) {
			log_debug(1,"","given RECV_PASSWD, assume RECV_CHAP");
			ppp0.auth.want_recvchap_response = 1;
		}
		if (ppp0.auth.recv_names.nm[0] != '\0'
		    && ppp0.auth.want_recvpap < 0) {
			log_debug(1,"","given RECV_NAME, assume RECV_PAP");
			ppp0.auth.want_recvpap = 1;
		}
	}

	/* To reconfigure, we must have enough information to cause enough
	 * authentication to happen so that we will receive a name from the
	 * peer.
	 *
	 * We can get a name by receiving a PAP request (want_recvpap),
	 * a CHAP challenge (want_sendchap_response), or a CHAP
	 * response (want_recvchap_response).
	 *
	 * If none of those are specified, then
	 *	turn on want_recvpap unless it was explicitly turned off.
	 *	turn on want_recvchap_response unless it was explicitly
	 *	    turned off in the hope that a password will be
	 *	    specified in the reconfiguring control file entry.
	 *	turn on want_sendchap_response if a password to send
	 *	    was specified (we can default the name to our hostname)
	 *	    and unless it was explicitly turned off.
	 * Prefer PAP unless it was explicitly turned off.
	 */
	if (reconfigure
	    && ppp0.auth.want_recvpap <= 0
	    && ppp0.auth.want_sendchap_response <= 0
	    && ppp0.auth.want_recvchap_response <= 0) {
		char str[3*20];
		str[0] = '\0';
		/* try CHAP even without a password in the hope that the
		 * reconfigured entry will specify one.
		 */
		if (ppp0.auth.want_recvchap_response < 0) {
			(void)strcat(str,"RECV_CHAP ");
			ppp0.auth.want_recvchap_response = 1;
			if (ppp0.auth.want_recvpap != 0)
				ppp0.lcp.prefer_recvpap = 1;
		}
		if (ppp0.auth.want_sendchap_response < 0
		    && ppp0.auth.have_send_passwd) {
			(void)strcat(str,"SEND_CHAP ");
			ppp0.auth.want_sendchap_response = 1;
			if (ppp0.auth.want_recvpap != 0)
				ppp0.lcp.prefer_recvpap = 1;
		}
		if (ppp0.auth.want_recvpap < 0) {
			(void)strcat(str,"RECV_PAP ");
			ppp0.auth.want_recvpap = 1;
		}
		if (str[0] != '\0') {
			str[strlen(str)-1] = '\0';
			log_debug(1,"", "given RECONFIGURE, assume %s", str);
		}
	}

	/* If we have a password or hostname to send, but neither
	 * PAP nor CHAP was chosen, then default to both PAP and CHAP.
	 */
	if ((ppp0.auth.have_send_passwd
	     || ppp0.auth.send_name[0] != '\0')
	    && ppp0.auth.want_sendchap_response <= 0
	    && ppp0.auth.want_sendpap <= 0) {
		if (ppp0.auth.want_sendchap_response == 0
		    && ppp0.auth.want_sendpap == 0) {
			log_complain("", "SEND_PASSWD or SEND_NAME"
				     " well as -SEND_CHAP and -SEND_PAP"
				     " specified");
		} else {
			char str[3*20];
			str[0] = '\0';
			if (ppp0.auth.want_sendpap < 0) {
				(void)strcat(str,"SEND_PAP ");
				ppp0.auth.want_sendpap = 1;
			}
			if (ppp0.auth.want_sendchap_response < 0) {
				(void)strcat(str,"SEND_CHAP ");
				ppp0.auth.want_sendchap_response = 1;
			}
			str[strlen(str)-1] = '\0';
			log_debug(1,"","given SEND_NAME or SEND_PASSWD,"
				  " assume %s", str);
		}
	}

	if (connmode == CALLEE) {
		mindevs = conf_in_mindevs;
		outdevs = conf_in_outdevs;
	} else {
		mindevs = conf_mindevs;
		outdevs = conf_outdevs;
	}

	demand_dial = (sactime != 0
		       && connmode != CALLEE && connmode != CALLER);

	maxdevs = conf_maxdevs;
	if (mindevs < 0)
		mindevs = 1;
	if (outdevs < 0)
		outdevs = MAX(1, mindevs);
	if (maxdevs < 0)
		maxdevs = MAX(MAX(2, mindevs), outdevs);

	if (!ppp0.conf.mp) {
		mp_on = 0;
		mp_known = 1;
	}

	lcp_param(&ppp0);
	ipcp_param(&ppp0);
	ccp_clear(&ppp0);

	(void)sprintf(conf_str,
		      (lactime != 0 || sactime != 0) ? conf_pat1 : conf_pat2,
		      debug,
		      (connmode == CALLEE ? "in"
		       : (connmode == CALLER ? "out"
			  : (connmode == Q_CALLER ? "quiet"
			     : "???"))),
		      demand_dial ? " demand-dial" : "",
		      mindevs,outdevs,maxdevs,
		      lactime, sactime);
	log_debug(1,"",conf_str);

	return (reconfig_mode
		&& bcmp(&ppp0.lcp.conf, &lcp_conf, sizeof(lcp_conf)));
}



/* parse hostname specification
 */
static char *				/* return 0 or error message */
get_host(char *v,
	 struct ok_host *h0,
	 int *indexp)
{
	struct ok_host *h1, *h2;
	char hname[MAXHOSTNAMELEN+1];
	struct hostent *hp;
	struct timeval beg, end;
	int secs;
	char *p;
	u_long m;

	/* Separate host name or IP address from an optional mask
	 * following a comma.  Parse the mask.
	 */
	m = 0;
	p = hname;
	for (;;) {
		if (*v == '\0')
			break;
		if (*v == ',') {
			if (*++v != '\0') {
				m = inet_addr(v);
				if (m == (u_long)-1)
					return "invalid netmask";
			}
			break;
		}
		if (p - hname >= MAXHOSTNAMELEN)
			return "hostname too long";
		*p++ = *v++;
	}
	*p = '\0';

	(void)gettimeofday(&beg,0);
	hp = gethostbyname(hname);
	(void)gettimeofday(&end,0);
	secs = ((end.tv_sec*10+end.tv_usec/100000)
		- (beg.tv_sec*10+beg.tv_usec/100000));
	if (secs < 0 || secs > 5)
		log_debug(1,"",
			  "%d.%d seconds spent looking for hostname \"%s\"",
			  secs/10, secs % 10, hname);
	if (0 == hp)
		return hstrerror(h_errno);

	h1 = &h0[*indexp];
	h1->addr = ((struct in_addr*)(hp->h_addr))->s_addr;
	if (p)
		h1->mask = m;

	if (h1->addr == 0) {		/* infer a wild card for "0" */
		h1->mask = 0;
	} else {
		h1->mask = (u_long)-1;
	}

	/* ensure they are unique */
	for (h2 = h0; ; h2++) {
		if (h1->addr == h2->addr
		    && h1->mask == h2->mask) {
			if (h2 == h1)
				++*indexp;
			break;
		}
	}

	return 0;
}


/* create an initial stab at the activity filter
 */
static void
init_afilter(void)
{
	static struct plist {
		char	*name;
		int	num;
	} ports[] = {
		{"daytime", 13},
		{"time",    37},
		{"ntp",	    123},
		{"route",   520},
		{"timed",   525},
		{"",	    0}
	};
	struct plist *pp;
	struct servent *sp;


	if (!afilter)
		afilter = (struct afilter*)malloc(sizeof(*afilter));
	bzero(afilter, sizeof(*afilter));

	SET_ICMP(afilter, ICMP_UNREACH);
	SET_ICMP(afilter, ICMP_SOURCEQUENCH);
	SET_ICMP(afilter, ICMP_ROUTERADVERT);
	SET_ICMP(afilter, ICMP_ROUTERSOLICIT);
	SET_ICMP(afilter, ICMP_TSTAMP);
	SET_ICMP(afilter, ICMP_TSTAMPREPLY);

	for (pp = &ports[0]; pp->name[0] != '\0'; pp++) {
		/* getservbyname() is very slow with YP, but
		 * getservbyport() has a cache and is much faster for
		 * 2 or more probes.
		 */
		sp = getservbyport(pp->num, "udp");
		if (!sp
		    || strcmp(pp->name, sp->s_name))
			sp = getservbyname(pp->name, "udp");
		if (!sp)
			SET_PORT(afilter, pp->num);
		else
			SET_PORT(afilter, sp->s_port);
	}
}

