/*
 * FDDI Station Management (SMT) daemon.
 * This module s the basis for the smt daemon.  It deals with processing
 * SMT Frames and executing Connection Management functionality.
 * Using the raw drain socket, smt processes station management
 * packets from the network and requests information about the
 * stations it serves.
 * A System Management Application (SMAP-TBD) will have access to
 * SMT data and functionality through the smtd rpc service.
 * Note, this implementation supports the following station types:
 * Single Attach, Dual Attach/Single Mac, Dual Attach/Dual Mac.
 */

#ident "$Revision: 1.31 $"

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <bstring.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <rpc/rpc.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <assert.h>

#include <sm_types.h>
#include <sm_log.h>
#include <sm_mib.h>
#include <sm_addr.h>
#include <smt_snmp.h>
#include <smt_snmp_api.h>

#include <smtd.h>
#include <ma.h>

#include <smt_parse.h>
#include <smtd_fs.h>
#include <smtd_svc.h>
#include <smtd_snmp.h>
#include <smtd_conf.h>
#include <smtd_parm.h>
#include <smtd_event.h>
#include <sys/mac_label.h>

mac_label dblow     = {MSEN_LOW_LABEL,      MINT_HIGH_LABEL,  0, 0, 0, 0};
mac_label highlabel = {MSEN_HIGH_LABEL,     MINT_EQUAL_LABEL, 0, 0, 0, 0};
mac_label origlabel;

extern char *optarg;

static	SMTD smtd[NSMTD];
SMTD	*smtp = &smtd[0];
int	nsmtd = 0;
int	valid_nsmtd = 0;
int	smtd_debug = LOG_ERR;
char	*smtd_cfile = {SMT_CFILE};	/* config file name */
char	*smtd_mfile = {SMT_MFILE};	/* MIB file name */
char	*smtd_traphost = 0;		/* traphost name */
struct timeval curtime;			/* now, more or less */
LFDDI_ADDR smtd_broad_mid;		/* broadcast addr */
LFDDI_ADDR smtd_null_mid;		/* all-zero addr */

/* SNMP-daemon GLOBALS */
struct tree	*smtd_mib;		/* mib tree */
struct tree	*smtd_fdditree;		/* fddi subtree */
fd_set		smtd_fdset;		/* fdset */
int		smtd_cmd;		/* Cmd that is being work on */
int		smtd_rvalue;		/* Return val for INTEGER */
SMT_MAC		*smtd_config;		/* configuration */
struct sockaddr_in supersin;
long		sr_timer = 0;		/* SRF timer */

static int	smtd_udpsock = 0;	/* main fd--must be 0 */
static int	smtd_trapsock;		/* trap fd from snmp_open */
static int	smtd_remotesock;	/* remote fd from snmp_open */
static struct snmp_session *smtd_session = 0;		/* local session */
static struct snmp_session *smtd_trapSession = 0;	/* trap session */
static struct snmp_session *smtd_remotesession = 0;	/* remote session */
static struct hostent *smtd_he;				/* local host ent */

static int issock(int);
static void smtd_init(void);
static void smtd_openss(void);

static char usage[] = "Usage: smtd";

void
smtd_setcx(int iid)
{
	smtp = &smtd[iid];
}

int
smtd_setsid(SMT_MAC *mac)
{
#ifdef STASH
	if (smtp != &smtd[0]) {
		int i;

		for (i = 1; i < valid_nsmtd; i++) {
			if (smtp == &smtd[i]) {
				smtp->sid = smtd[0].sid;
				smtp->sid.b[1] = i;
				return(0);
			}
		}
		return(-1);
	}
#endif
	if (!bcmp((char*)(&smtp->sid.ieee), (char*)(&smtd_null_mid),
		sizeof(smtd_null_mid))) {
		struct smt_st st;

		if (sm_stat(mac->name, 0, &st) != SNMP_ERR_NOERROR)
			return(-1);
		mac->addr = st.addr;

		smtp->sid.b[0] = 0;
		smtp->sid.b[1] = 0;
		bcopy((char*)&mac->addr, (char*)&smtp->sid.ieee,
				sizeof(mac->addr));
	}
	return(0);
}

void
smtd_quit(void)
{
	(void)unlink(SMTD_UDS);
	exit(0);
}

/* Increase verbosity of debugging messages */
static void
incrdbg()
{
	smtd_debug++;
	sm_loglevel(smtd_debug);
}

/* Decrease verbosity of debugging messages */
static void
decrdbg()
{
	if (smtd_debug > LOG_ERR)
		smtd_debug--;
	sm_loglevel(smtd_debug);
}

main(int argc, char *argv[])
{
	int i, n;
	SMT_MAC *mac;
	struct sockaddr_un name;
	int daemonize_flags = 0;
	long osstm = 0;
	long smtd_timo;

	(void)sigset(SIGTERM, smtd_quit);
	(void)sigset(SIGUSR1, incrdbg);
	(void)sigset(SIGUSR2, decrdbg);

	while (EOF != (i = getopt(argc, argv, "dpt:c:m:B"))) {
		switch(i) {
		case 'd':
			smtd_debug++;
			break;
		case 't':
			smtd_traphost = optarg;
			break;
		case 'c':
			smtd_cfile = optarg;
			break;
		case 'm':
			smtd_mfile = optarg;
			break;
		case 'B':
			daemonize_flags |= _DF_NOFORK;
			break;
		default:
			sm_log(LOG_EMERG, 0, usage);
			exit(-1);
		}
	}

	sm_openlog(SM_LOGON_SYSLOG, smtd_debug,
			SMTD_SERVICE, LOG_PID, LOG_DAEMON);

	if (geteuid()) {
		sm_log(LOG_EMERG, SM_ISSYSERR, "You must be superuser.");
		smtd_quit();
	}


	(void)umask(055);

	smtd_udpsock = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (smtd_udpsock < 0) {
		perror("smtd: socket");
		exit(-1);
	}
	bzero(&name, sizeof(name));
	name.sun_family = AF_UNIX;
	strcpy(name.sun_path, SMTD_UDS);
	if (bind(smtd_udpsock, &name, sizeof(name)) < 0) {
		if (errno != EADDRINUSE) {
			perror("smtd: bind");
			exit(-1);
		}
		/* notice if smtd is already running */
		if (connect(smtd_udpsock, &name, sizeof(name)) >= 0) {
			sm_log(LOG_EMERG, SM_ISSYSERR,
			       "smtd already running");
			exit(-1);
		}
		(void)close(smtd_udpsock);
		(void)unlink(SMTD_UDS);
		smtd_udpsock = socket(AF_UNIX, SOCK_DGRAM, 0);
		if (smtd_udpsock < 0) {
			perror("smtd: socket");
			exit(-1);
		}
		if (bind(smtd_udpsock, &name, sizeof(name)) < 0) {
			perror("smtd: bind");
			exit(-1);
		}
	}
	if (smtd_udpsock != 0) {
		(void)dup2(smtd_udpsock, 0);
		(void)close(smtd_udpsock);
		smtd_udpsock = 0;
	}

	/*
	 * Initialize things before forking, to let smtconfig know
	 * how things went, so it does not have to wait a fixed,
	 * possibly wrong time.
	 */
	smtd_init();
	if (!FD_ISSET(smtd_udpsock, &smtd_fdset)) {
		sm_log(LOG_EMERG, 0, "SNMP PORT != %d", smtd_udpsock);
		smtd_quit();
	}

	if (0 > _daemonize(daemonize_flags, smtd_udpsock, -1, -1)) {
		sm_log(LOG_ERR,SM_ISSYSERR,"smtd: can't fork");
		exit(-1);
	}
	/* Reopen log after all FDs closed */
	sm_openlog(SM_LOGON_SYSLOG, smtd_debug,
		   SMTD_SERVICE, LOG_PID, LOG_DAEMON);
	cached_soc = -1;


	/* Do not try to talk to the portmapper before it is awake. */
	smt_gts(&curtime);
	osstm = curtime.tv_sec + 5;

	for (;;) {
		fd_set readfds;
		struct timeval time_out;

		smt_gts(&curtime);
		smtd_timo = curtime.tv_sec + T_NOTIFY;
		if (smtd_timo > osstm)
			smtd_timo = osstm;
		for (i = 0; i < nsmtd; i++) {
			for (mac = smtd[i].mac; mac != 0; mac = mac->next) {
			    /* defend against time changes */
			    if (mac->sampletime.tv_sec > curtime.tv_sec
				|| mac->sampletime.tv_sec < (curtime.tv_sec
							     -2*T_NOTIFY)) {
				if (mac->boottime > curtime.tv_sec)
				    mac->boottime =curtime.tv_sec;
				mac->tflip = curtime.tv_sec;
				mac->tflop = curtime.tv_sec;
				mac->tnn = curtime.tv_sec;
				mac->pcmtime = curtime.tv_sec;
				if (mac->trm_ck != SMT_NEVER) {
				    mac->trm_base = curtime.tv_sec;
				    mac->trm_ck = curtime.tv_sec;
				}
			    }
			    if (smtd_timo > mac->tnn)
				smtd_timo = mac->tnn;
			    if (smtd_timo > mac->trm_ck)
				smtd_timo = mac->trm_ck;
			}
		}
		if (smtd_timo > sr_timer)
			smtd_timo = sr_timer;

		time_out.tv_sec = smtd_timo - curtime.tv_sec;
		/* To keep the state machine timers accurate,
		 * make the timer expire near the next
		 * tenth second mark.
		 */
		time_out.tv_usec = (1000000/10) - curtime.tv_usec;
		if (time_out.tv_usec < 0) {
			if (time_out.tv_sec > 0)
				time_out.tv_sec--;
			time_out.tv_usec += 1000000;
		}
		/* defend against time changes */
		if (time_out.tv_sec < 0) {
			time_out.tv_sec = 0;
			time_out.tv_usec = 0;
		} else if (time_out.tv_sec > T_NOTIFY+1) {
			time_out.tv_sec = T_NOTIFY+1;
		}

		if (time_out.tv_sec == 0 && time_out.tv_usec == 0) {
			/* If we are running late, do not delay.
			 * Just act as if select() had instantly timed out.
			 */
			n = 0;
			FD_ZERO(&readfds);
		} else {
			readfds = smtd_fdset;
			n = select(FD_SETSIZE, &readfds,0,0, &time_out);
			if (n < 0) {
				if (errno == EINTR || errno == EAGAIN) {
					n = 0;
				} else {
					sm_log(LOG_EMERG,SM_ISSYSERR,"select");
					smtd_quit();
				}
			}
			smt_gts(&curtime);
		}

		for (i = 0; i < nsmtd; i++) {
			smtd_setcx(i);
			for (mac = smtp->mac;
			     mac != 0 && n > 0;
			     mac = mac->next) {
				if (FD_ISSET(mac->smtsock, &readfds)) {
					/* Handle Frame Service request. */
					smtd_fs(mac);
					FD_CLR(mac->smtsock, &readfds);
					n--;
				}
			}
			smtd_timeout();
		}

		/*
		 * Run status reporting frame events.
		 */
		sr_timo();

		/*
		 * Handle Request from smt-agents.
		 */
		if (n > 0) {
			/*
			 * Ensure the MIB is up to date.
			 * XXX This really should be done in each
			 * service function, and mac->changed set
			 * only when needed.
			 */
			for (i = 0; i < nsmtd; i++) {
				smtd_setcx(i);
				for (mac = smtp->mac;
				     mac != 0;
				     mac = mac->next) {
					mac->changed = 1;
					updatemac(mac);
				}
			}
			snmp_read(&readfds);
		}

		/* keep trying until the portmapper is alive */
		if (smtd_remotesession) {
			osstm = SMT_NEVER;
		} else {
			if (osstm <= curtime.tv_sec) {
				osstm = curtime.tv_sec+2;
				smtd_openss();
			}
		}
	}
}


/*
 * Initialize SNMP access.
 */
static void
smtd_init(void)
{
	int i;
	oid oids[30];
	struct hostent *h;
	struct sockaddr name;
	struct sockaddr_in sin;
	int namelen = sizeof(name);
	struct snmp_session session, trapsession;

	/* init main smtd structure */
	atomid("ff-ff-ff-ff-ff-ff", &smtd_broad_mid);
	atomid("00-00-00-00-00-00", &smtd_null_mid);

	/* get smtd configuration. */
	config_smtd();

	/* set up mibtree */
	smtd_mib = init_fddimib(smtd_mfile);
	i = get_fddimib((char *)oids);
	if ((smtd_fdditree = find_oid(smtd_mib, 0, &oids[1], i-1)) == 0) {
		sm_log(LOG_EMERG, 0, "couldn't get fdditree.");
		smtd_quit();
	}

	/* Get host name and addr */
	smtd_he = gethostbyname("localhost");
	if (!smtd_he) {
		sm_log(LOG_ERR, SM_ISSYSERR, "could not find 'localhost'");
		smtd_he = gethostbyname("127.0.0.1");
		if (!smtd_he) {
			sm_log(LOG_EMERG, SM_ISSYSERR,
			       "could not find '127.0.0.1'");
			smtd_quit();
		}
	}
	bcopy(smtd_he->h_addr, (char*)&supersin.sin_addr, smtd_he->h_length);

	/* Open Session */
	bzero((char *)&session, sizeof(struct snmp_session));
	session.peername = NULL;
	session.community = NULL;
	session.community_len = 0;
	session.retries = SNMP_DEFAULT_RETRIES;
	session.timeout = SNMP_DEFAULT_TIMEOUT;
	session.authenticator = snmp_auth;
	session.callback = snmp_input;
	session.callback_magic = NULL;

	/* XXX - stdin has to be sock */
	if (getsockname(smtd_udpsock, &name, &namelen)) {
		sm_log(LOG_EMERG, SM_ISSYSERR, "local port");
		smtd_quit();
	}

	if (name.sa_family == AF_INET) {
		h = smtd_he;
		sin.sin_family = h->h_addrtype;
		sin.sin_port = INADDR_ANY;
		bzero(sin.sin_zero, sizeof(sin.sin_zero));
		bcopy(h->h_addr, (char*)&sin.sin_addr, h->h_length);
		session.local_port =
			pmap_getport(&sin, SMTPROG, SMTVERS, IPPROTO_UDP);
		if (session.local_port == 0) {
			sm_log(LOG_EMERG, 0, "couldn't get localport");
			smtd_quit();
		}
	}
	SNMPDEBUG((LOG_DBGSNMP,0,"My local port is %d",session.local_port));
	if ((smtd_session=snmp_open(&session,&smtd_udpsock,SMTD_UDS))==NULL) {
		sm_log(LOG_EMERG, 0, "couldn't open SNMP session");
		smtd_quit();
	}
	SNMPDEBUG((LOG_DBGSNMP, 0, "Open session done"));

	FD_SET(smtd_udpsock, &smtd_fdset);
	SNMPDEBUG((LOG_DBGSNMP, 0, "udp fd set = %d", smtd_udpsock));

#ifdef DO_TRAP
	/* Open Trap Session iff applicable */
	if (smtd_traphost != NULL) {
		bzero((char *)&trapsession, sizeof(struct snmp_session));
		trapsession.peername = smtd_traphost;
		trapsession.community = NULL;
		trapsession.community_len = 0;
		trapsession.retries = SNMP_DEFAULT_RETRIES;
		trapsession.timeout = SNMP_DEFAULT_TIMEOUT;
		trapsession.authenticator = NULL;
		trapsession.callback = snmp_trapinput;
		trapsession.callback_magic = NULL;
		if ((se = getservbyname("smt-trap","udp")) == NULL) {
#ifdef TBD
faile until we get REAL smt-trap port number
			trapsession.remote_port = SNMP_TRAP_PORT;
#endif
			sm_log(LOG_EMERG, SM_ISSYSERR,
				"SMT_TRAPPORT NUMBER not defined yet");
			smtd_quit();
		} else
			trapsession.remote_port = se->s_port;
		smtd_trapsock = -1;	/* mark it not opened yet */
		smtd_trapSession = snmp_open(&trapsession, &smtd_trapsock, 0);
		if (smtd_trapSession == NULL) {
			sm_log(LOG_EMERG, 0, "couldn't open SNMP Trap session");
			smtd_quit();
		}
		FD_SET(smtd_trapsock, &smtd_fdset);
		sm_log(LOG_INFO, 0, "trap fd set = %d", smtd_udpsock);
	}
	(void)sigset(SIGINT, quit);
#endif

	for (i = 0; i < valid_nsmtd; i++) {
		smtd_setcx(i);
		if (smtp->srf_on) {
			smtp->topology |= SMT_DO_SRF;
			sr_timer = 1;
		}
	}
}

/*
 * Open remote SNMP Session.
 *	Get the SNMP port working so that remote applications such as
 *	`fddivis` can talk to us.
 */
static struct timeval pmap_timo = { 1, 1 };
static void
smtd_openss(void)
{
	int sinlen;
	struct sockaddr_in sin;
	struct snmp_session session;

	/* Get host name and addr */
	if ((smtd_remotesock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		sm_log(LOG_EMERG, SM_ISSYSERR, "couldn't get remotesocket");
		return;
	}
	sin.sin_family = AF_INET;
	sin.sin_port = 0;
	sin.sin_addr.s_addr = INADDR_ANY;
	if (bind(smtd_remotesock, &sin, sizeof(sin)) < 0) {
		sm_log(LOG_EMERG, SM_ISSYSERR, "couldn't bind remotesocket");
		goto oss_ret;
	}

	sinlen = sizeof(sin);
	if (getsockname(smtd_remotesock, &sin, &sinlen) != 0) {
		sm_log(LOG_EMERG, SM_ISSYSERR, "couldn't get socket name");
		goto oss_ret;
	}

	pmap_settimeouts(pmap_timo, pmap_timo);
	(void)pmap_unset(SMTPROG, SMTVERS);
	if (!pmap_set(SMTPROG, SMTVERS, IPPROTO_UDP, sin.sin_port)) {
		sm_log(LOG_INFO, SM_ISSYSERR, "couldn't map remotesocket");
		goto oss_ret;
	}

	/* init a Session */
	bzero(&session, sizeof(session));
	session.peername = NULL;
	session.community = NULL;
	session.community_len = 0;
	session.retries = SNMP_DEFAULT_RETRIES;
	session.timeout = SNMP_DEFAULT_TIMEOUT;
	session.authenticator = snmp_auth;
	session.callback = snmp_input;
	session.callback_magic = NULL;
	session.local_port = sin.sin_port;

	/* open session */
	if ((smtd_remotesession=snmp_open(&session,&smtd_remotesock,0))==NULL) {
		sm_log(LOG_EMERG, 0, "couldn't open remote SNMP session");
		smtd_remotesession = 0;
		goto oss_ret;
	}

	FD_SET(smtd_remotesock, &smtd_fdset);
	SNMPDEBUG((LOG_DBGSNMP, 0, "Open remote session done"));
	return;

oss_ret:
	(void)close(smtd_remotesock);
	return;
}
