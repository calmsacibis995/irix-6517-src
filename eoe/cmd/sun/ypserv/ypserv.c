#ifndef lint
static char sccsid[] = 	"@(#)ypserv.c	1.2 88/06/02 4.0NFSSRC Copyr 1988 Sun Micro";
#endif

/*
 * This contains the mainline code for the NIS server.  Data
 * structures which are process-global are also in this module.  
 */

#include "ypsym.h"
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <arpa/inet.h>

#include "ypdefs.h"
USE_YPDBPATH
USE_YP_LAST_MODIFIED
USE_YP_MASTER_NAME

char *progname;
static char create_failed[] = "Unable to create server for ";
static char register_failed[] = "Unable to register service for ";
static char failed_format[] = "%s: %s%s.\n";

static char myhostname[MAX_MASTER_NAME + 1];
SVCXPRT *udphandle;
SVCXPRT *tcphandle;
bool silent = TRUE;
bool InterDomains = FALSE;		/* perform inter-domain resolution */
static bool doflush = FALSE;
int log_request = 0;
char logfile[] = "/var/yp/ypserv.log";

void ypexit();
void ypinit();
void ypdispatch();
void ypolddispatch();
void ypget_command_line_args();

/*
 * External refs to functions named only by the dispatchers.
 */
extern void ypdomain();
extern void ypmatch();
extern void ypfirst();
extern void ypnext();
extern void ypxfr();
extern void ypall();
extern void ypmaster();
extern void yporder();
extern void ypoldmatch();
extern void ypoldfirst();
extern void ypoldnext();
extern void ypoldpoll();
extern void yppush();
extern void yppull();
extern void ypget();
extern void ypmaplist();
extern void reapchild();
extern void retry(void);
extern void qc_flush();

/*
 * This is the main line code for the NIS server.
 */
main(argc, argv)
	int argc;
	char **argv;
{
	fd_set readfds;

	progname = argv[0];
	if (getuid() != 0) {
		fprintf(stderr, "%s: You are not superuser\n", progname);
		exit(1);
	}
 	ypinit(argc, argv); 			/* Set up shop */

	for (;;) {
		readfds = svc_fdset;
		errno = 0;

		switch (select(32, &readfds, (fd_set*)NULL, (fd_set*)NULL, 0)) {
		case -1:
			if (errno != EINTR) {
				logprintf("%s: %s\n",progname, strerror(errno));
			}
			break;

		case 0:
			logprintf("%s: indefinite select returned 0!\n",
			    progname);
			break;

		default:
			svc_getreqset (&readfds);
			break;
		
		}
		if (doflush) {
			doflush = FALSE;
			qc_flush();
		}
	}

}

static void
flushcache(void)
{
	doflush = TRUE;
	signal(SIGHUP, flushcache);
}

#define	YP_UDP_PORT	627
#define	YP_TCP_PORT	635

static char **retry_argv;
static time_t started;

void
retry(void)
{
	int	i;

	/* avoid doing this too often, but be quick if it happens
	 * infrequently
	 */
	if (time(0) <= started) {
		logprintf("%s: respawning rapidly.\n", progname);
		sleep(1);
	}

	for (i = getdtablehi(); i > 2; i--)
		(void)close(i);

	/* do not recurse */
	(void) signal(SIGILL, SIG_DFL);
	(void) signal(SIGTRAP, SIG_DFL);
	(void) signal(SIGEMT, SIG_DFL);
	(void) signal(SIGFPE, SIG_DFL);
	(void) signal(SIGBUS, SIG_DFL);
	(void) signal(SIGSEGV, SIG_DFL);

	execvp(retry_argv[0], retry_argv);
}


/*
 * Does startup processing for the NIS server.
 */
void
ypinit(argc, argv)
	int argc;
	char **argv;
{
	int pid;
	int t, i;
	struct	sigaction sa;
	char	*strdup();

	/*
	 * This is a gross hack that attempts to make sure that ypserv
	 * never goes away.  If it dies for any reason that would cause a core
	 * dump, respawn.
	 */
	started = time(0);
	retry_argv = (char**)malloc(sizeof(char*)*(argc+2));
	retry_argv[i = 0] = strdup(argv[0]);
	if ((argc > 1) && (strcmp("-R", argv[1]) == 0)) {
		logprintf("%s: respawned.\n", progname);
	} else {
		retry_argv[++i] = "-R";
	}
	for (t = 1; t < argc; ++t) {
		retry_argv[++i] = strdup(argv[t]);
	}
	retry_argv[++i] = 0;

	pmap_unset(YPPROG, YPVERS);
	pmap_unset(YPPROG, YPOLDVERS);
	ypget_command_line_args(argc, argv);
	(void) signal(SIGHUP, flushcache);

	if (silent) {
		pid = fork();
		
		if (pid < 0) {
			logprintf("%s: %s\n", progname, strerror(errno));
			ypexit();
		}
	
		if (pid > 0) {
			exit(0);
		}
	
		if (access(logfile, W_OK) < 0) {
			(void) freopen("/dev/null", "w", stderr);
			(void) freopen("/dev/null", "w", stdout);
		} else {
			(void) freopen(logfile, "a", stderr);
			(void) freopen(logfile, "a", stdout);
		}

		(void) fclose(stdin);
		for (t = getdtablesize(); t > 2; --t) {
			(void) close(t);
		}
	

 		t = open("/dev/tty", 2);
	
 		if (t >= 0) {
 			(void) ioctl(t, (int) TIOCNOTTY, (char *) 0);
 			(void) close(t);
 		}

		(void) signal(SIGILL, retry);
		(void) signal(SIGTRAP, retry);
		(void) signal(SIGEMT, retry);
		(void) signal(SIGFPE, retry);
		(void) signal(SIGBUS, retry);
		(void) signal(SIGSEGV, retry);
	}

	(void) gethostname(myhostname, 256);

#define	SIG_CHILD_HANDLER
#ifdef	SIG_CHILD_HANDLER
	sa.sa_flags = 0;
	sa.sa_handler = reapchild;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGCHLD, &sa, 0) == -1) {
		logprintf("%s: %s\n", progname, strerror(errno));
		ypexit();
	}
#else
	signal(SIGCHLD, SIG_IGN);
#endif

	if ((udphandle = svcudp_bufcreate(getsock(YP_UDP_PORT, 0), YPMSGSZ, YPMSGSZ))
	    == (SVCXPRT *) NULL) {
		logprintf(failed_format, progname, create_failed, "udp");
		ypexit();
	}

	if ((tcphandle = svctcp_create(getsock(YP_TCP_PORT, 1), YPMSGSZ, YPMSGSZ))
	    == (SVCXPRT *) NULL) {
		logprintf(failed_format, progname, create_failed, "tcp");
		ypexit();
	}

	if (!svc_register(udphandle, YPPROG, YPVERS, ypdispatch, IPPROTO_UDP)) {
		logprintf(failed_format, progname, register_failed, "udp");
		ypexit();
	}

	if (!svc_register(tcphandle, YPPROG, YPVERS, ypdispatch, IPPROTO_TCP)) {
		logprintf(failed_format, progname, register_failed, "tcp");
		ypexit();
	}
	
	if (!svc_register(udphandle, YPPROG, YPOLDVERS, ypolddispatch,
			  IPPROTO_UDP)) {
		logprintf(failed_format, progname, register_failed, "udp");
		ypexit();
	}

	if (!svc_register(tcphandle, YPPROG, YPOLDVERS, ypolddispatch,
			  IPPROTO_TCP)) {
		logprintf(failed_format, progname, register_failed, "tcp");
		ypexit();
	}
}

/*
 * Try and get the socket on the port requested so that restarts work.
 */
getsock(port, tcp)
{
	int	sock;
	struct	sockaddr_in s;
#if 0
	int	namelen;
#endif

	if (tcp) {
		sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	} else {
		sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	}
	if (sock < 0) {
		perror("socket");
		exit(1);
	}
	bzero(&s, sizeof(s));
	s.sin_family = AF_INET;
	s.sin_port = htons(port);
	if (bind(sock, (struct sockaddr*)&s, sizeof(s)) < 0) {
		perror("bind");
	}
#if 0
	namelen = sizeof(s);
	if (getsockname(sock, (struct sockaddr *)&s, &namelen) < 0) {
		perror("getsockname");
		exit(3);
	}
	fprintf(stderr, "Server port %d\n", ntohs(s.sin_port));
#endif
	return (sock);
}

static struct logflag {
	char	*name;
	int	value;
} logflags[] = {
	{ "dispatch",		LOG_DISPATCH },
	{ "interdomain",	LOG_INTERDOMAIN },
	{ "querycache",		LOG_QUERYCACHE },
	0,
};

static void
usage()
{
	struct logflag *lf;

	lf = logflags;
	fprintf(stderr, "usage: %s [-f forklimit] [-i] [-L %s",
		progname, lf->name);
	for (lf++; lf->name; lf++)
		fprintf(stderr, ",%s", lf->name);
	fprintf(stderr, "] [-t timeout] [-v]\n");
	exit(-1);
}

/*
 * This picks up any command line args passed from the process invocation.
 */
void
ypget_command_line_args(argc, argv)
	int argc;
	char **argv;
{
	int opt;
	char *flag;
	struct logflag *lf;
	extern int optind;
	extern char *optarg;

	while ((opt = getopt(argc, argv, "f:iL:Rvt:")) != EOF) {
		switch (opt) {
		  case 'R':
			break;
		  case 'v': 
			silent = FALSE;
			break;
		  case 'i':
			InterDomains = TRUE;
			break;
		  case 'f':
			setForkCnt(atoi(optarg));
			break;
		  case 'L':
			while ((flag = strtok(optarg, ",")) != 0) {
				optarg = 0;
				for (lf = logflags; ; lf++) {
					if (lf->name == 0)
						usage();
					if (!strcmp(lf->name, flag)) {
						log_request |= lf->value;
						break;
					}
				}
			}
			break;
		  case 't':  {
			extern long qc_timeout;
			long t = atol(optarg);
			if (t > 0)
				qc_timeout = t;
			break;
		  }
		  default:
			usage();
		}
	}
}

/* 
 * Mapping of ypserv procedure numbers to names.
 * N.B. keep in sync with <rpcsvc/yp_prot.h>
 */

#ifdef __STDC__
#define REQ(x)	{YPPROC_ ## x, "x"}
#else
#define REQ(x)	{YPPROC_/**/x, "x"}
#endif
static struct {
    u_long proc;
    char *name;
} reqnames[] = {
    REQ(NULL),
    REQ(DOMAIN),
    REQ(DOMAIN_NONACK),
    REQ(MATCH),
    REQ(FIRST),
    REQ(NEXT),
    REQ(XFR),
    REQ(CLEAR),
    REQ(ALL),
    REQ(MASTER),
    REQ(ORDER),
    REQ(MAPLIST),
    { -1, NULL },
};
#undef REQ

static char *
reqname(u_long proc)
{
    static char buf[10];
    register int i;
    for (i=0; reqnames[i].name; i++) {
	    if (reqnames[i].proc == proc) {
		    return reqnames[i].name;
	    }
    }
    (void) sprintf(buf, "%u", proc);
    return buf;
}

/*
 * This dispatches to server action routines based on the input procedure
 * number.  ypdispatch is called from the RPC function svc_getreq.
 */
void
ypdispatch(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	if (log_request & LOG_DISPATCH) {
		logprintf("ypdispatch: %s request from %s\n",
			  reqname(rqstp->rq_proc),
			  inet_ntoa(svc_getcaller(transp)->sin_addr));
	}

	switch (rqstp->rq_proc) {

	case YPPROC_NULL:

		if (!svc_sendreply(transp, xdr_void, 0) ) {
			logprintf("%s: Can't reply to NULL rpc call.\n", progname);
		}

		break;

	case YPPROC_DOMAIN:
		ypdomain(rqstp, transp, TRUE);
		break;

	case YPPROC_DOMAIN_NONACK:
		ypdomain(rqstp, transp, FALSE);
		break;

	case YPPROC_MATCH:
		ypmatch(rqstp, transp);
		break;

	case YPPROC_FIRST:
		ypfirst(rqstp, transp);
		break;

	case YPPROC_NEXT:
		ypnext(rqstp, transp);
		break;

	case YPPROC_XFR:
		ypxfr(rqstp, transp);
		break;

	case YPPROC_CLEAR:
		ypclr_current_map();
		
		if (!svc_sendreply(transp, xdr_void, 0) ) {
			logprintf("%s: Can't reply to CLEAR rpc call.\n", progname);
		}

		break;

	case YPPROC_ALL:
		ypall(rqstp, transp);
		break;

	case YPPROC_MASTER:
		ypmaster(rqstp, transp);
		break;

	case YPPROC_ORDER:
		yporder(rqstp, transp);
		break;

	case YPPROC_MAPLIST:
		ypmaplist(rqstp, transp);
		break;

	default:
		svcerr_noproc(transp);
		break;

	}

#ifndef	SIG_CHILD_HANDLER
	reapchild();
#endif
	return;
}

/*
 * This is the dispatcher for the old NIS protocol.  The case symbols are
 * defined in ypv1_prot.h, and are copied (with an added "OLD") from version
 * 1 of yp_prot.h.
 */
void
ypolddispatch(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	if (log_request & LOG_DISPATCH) {
		logprintf("ypolddispatch: req %d from %s\n",
			  rqstp->rq_proc, 
			  inet_ntoa(svc_getcaller(transp)->sin_addr));
	}
	switch (rqstp->rq_proc) {

	case YPOLDPROC_NULL:

		if (!svc_sendreply(transp, xdr_void, 0) ) {
			logprintf("%s: Can't reply to OLD_NULL rpc call.\n", progname);
		}

		break;

	case YPOLDPROC_DOMAIN:
		ypdomain(rqstp, transp, TRUE);
		break;

	case YPOLDPROC_DOMAIN_NONACK:
		ypdomain(rqstp, transp, FALSE);
		break;

	case YPOLDPROC_MATCH:
		ypoldmatch(rqstp, transp);
		break;

	case YPOLDPROC_FIRST:
		ypoldfirst(rqstp, transp);
		break;

	case YPOLDPROC_NEXT:
		ypoldnext(rqstp, transp);
		break;

	case YPOLDPROC_POLL:
		ypoldpoll(rqstp, transp);
		break;

	case YPOLDPROC_PUSH:
		yppush(rqstp, transp);
		break;

	case YPOLDPROC_PULL:
		yppull(rqstp, transp);
		break;

	case YPOLDPROC_GET:
		ypget(rqstp, transp);
		break;

	default:
		svcerr_noproc(transp);
		break;

	}

	return;
}

/*
 * This flushes output to stderr, then aborts the server process to leave a
 * core dump.
 */
static void
ypexit()
{
	(void) fflush(stderr);
	(void) abort();
}


/*
 * This constructs a logging record.
 */
/*VARARGS1*/
void
logprintf(char *format, ...)
{
	struct timeval t;
	va_list ap;

	if (silent) {
		(void) gettimeofday(&t, NULL);
		fseek(stderr,0,2);
		(void) fprintf(stderr, "%19.19s: ", ctime(&t.tv_sec));
	}
	va_start(ap, format);
	(void) vfprintf(stderr, format, ap);
	va_end(ap);
	fflush(stderr);
}
