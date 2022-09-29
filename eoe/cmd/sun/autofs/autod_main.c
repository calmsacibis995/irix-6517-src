/*
 *	autod_main.c
 *
 *	Copyright (c) 1988-1993 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#ident	"@(#)autod_main.c	1.6	93/11/18 SMI"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h> /* getenv, exit */
#include <stdarg.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <memory.h>
#include <netdb.h>
#include <stropts.h>
#include <syslog.h>
#include <getopt.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/sched.h>
/*
#include <sys/sema.h>
*/
#include <sys/sockio.h>
#include <rpc/rpc.h>
#include <net/if.h>
#include <netdir.h>
#include <sys/fs/autofs.h>
#include <cap_net.h>
#include "autofsd.h"
#include "autofs_prot.h"
#include <sys/resource.h>
#include <rpc/svc.h>
#include <mplib.h>

#define MAXTHREADS 16
#define RESOURCE_FACTOR 8

struct autodir *dir_head;
struct autodir *dir_tail;
char str_arch[32];
char str_cpu[32];
int mac_enabled;
/* User to execute, executable maps, as. */
char *user_to_execute_maps_as = NULL;
int ping_cache_time = 5;	/* seconds to believe ping results */

static time_t myaddrs_time;
static int hup_recv = 0;
static sigset_t sigset_hup;

void usage(void);
void handle_hup(int);
void getmyaddrs(void);

extern char self[64];
extern int __svc_label_agile;
extern void init_names(void);

static void autofs_prog(struct svc_req *, SVCXPRT *);

/* 
 * local version of inet_ntoa, the libc version uses a static character buffer
 * which is not good for the pthreaded version of autofsd. This version
 * assumes a buffer b on the stack.
 */
#define UC(b)   (((int)b)&0xff)
#define local_inet_ntoa(in) {                                               \
	register char *p;                                                   \
                                                                            \
	p = (char *)&in;                                                    \
        sprintf(b, "%d.%d.%d.%d", UC(p[0]), UC(p[1]), UC(p[2]), UC(p[3]));  \
}

main(argc, argv)
	int argc;
	char *argv[];

{
	SVCXPRT *transp;
	int c;
	int sock;
	int fd;
	struct sockaddr_in addr;
	int asize = sizeof(struct sockaddr_in);
	char *myname = "autofsd";
	char *non_num;
	int	policy = SCHED_TS;	/* timeshare scheduling policy */
	struct	sched_param sparam;

	pid_t pid;
	struct rlimit rlset;
	int rpc_svc_mode = RPC_SVC_MT_AUTO;
	int maxthreads = MAXTHREADS;
	int prevthreads = 0;
	struct sigaction sigact;


	multithread = 1;

	if (geteuid() != 0) {
		(void) fprintf(stderr, "%s must be run as root\n", argv[0]);
		exit(1);
	}

	/* see if MAC is enabled on this system */
	mac_enabled = (sysconf(_SC_MAC) > 0);

	/* see if IP Security is enabled on this system */
	__svc_label_agile = (sysconf(_SC_IP_SECOPTS) > 0);

	/*
	 * Error messages go to both stderr and syslog until the fork().
	 */
	openlog(myname, LOG_PID | LOG_NOWAIT | LOG_PERROR, LOG_DAEMON);

	sparam.sched_priority = 30;		/* Default priority */

	/* NOTE: Both autofsd and autofs can be started from the
	 *	 /etc/init.d/network script which, will pass the contents
	 *	 of /etc/config/autofs.options to BOTH autofsd and autofs.
	 *	 This means that autofsd must gracefully handle (and ignore)
	 *	 any autofs options (e.g., -t).
	 */

	while ((c = getopt(argc, argv, "vp:m:M:t:TD:E:")) != EOF) {
		switch (c) {
		case 'v':
			verbose++;
			break;
		case 'p':
			if (isdigit(*optarg) == 0)
				usage();
			sparam.sched_priority = atoi(optarg);
			break;
		case 't':
			if (argv[optind-1][2] != 'p') /* autofs -t option */
				break;
			/*
			 *	getopt() assumes that all options are single
			 *	character. Because of this, 'optarg' will always
			 *	be pointing at 'p'. So, to correctly process
			 *	the 'tp' option we need to do some manual
			 *	manipulation of 'optarg' and possibly 'optind'.
			 */
			if (argv[optind-1][3] != '\0') {
				/*
				 *	This is the '-tpn' case, where no space
				 *	is between the option and argument. We
				 *	need to point 'optarg' to the argument.
				 */
				optarg = &argv[optind-1][3];
			} else if (optind < argc) {
				/*
				 *	In this case (-tp n), a space has been
				 *	used, and the argument is in the next
				 *	argv entry, which 'optind' is pointing
				 *	to. We then need to increment 'optind'
				 *	so that it points to the next option.
				 */
				optarg = argv[optind];
				optind++;
			} else {
				fprintf(stderr,
					"Option 'tp' requires an argument\n");
				usage();
			}
			if ((strtol(optarg, &non_num, 10) == 0) ||
			    (*non_num != '\0')) {
				fprintf(stderr,
				     "Option 'tp' requires numeric argument\n");
				usage();
			}
			ping_cache_time = atoi(optarg);
			break;
		case 'T':
			trace++;
			break;
		case 'D':
			(void) putenv(optarg);
			break;
		case 'E':
			user_to_execute_maps_as = argv[optind-1];
			break;
		case 'm':
			if (isdigit(*optarg) == 0)
				usage();
			maxthreads = atoi(optarg);
			if (maxthreads < 0)
				usage();
			maxthreads = maxthreads > MAXTHREADS ? MAXTHREADS : maxthreads;
			multithread = maxthreads > 1;
			break;
		case 'M':			/* Ignore automount option */
			syslog(LOG_ERR, "WARNING: -M option is not "
				"supported, and will be ignored.");
			break;
		case '?':
		default:
			usage();
		}
	}

	(void) init_names();

	/* initialize all the semaphores */
	AUTOFS_LOCKINIT(mysubnet_hosts);
	AUTOFS_LOCKINIT(pingnfs_lock);
	AUTOFS_LOCKINIT(xc_lock);
	AUTOFS_LOCKINIT(mtab_lock);
	AUTOFS_LOCKINIT(map_lock);
	AUTOFS_LOCKINIT(misc_lock);

#ifndef NOFORK
	pid = fork();
	if (pid < 0) {
		syslog(LOG_ERR, "Cannot fork: %m");
		exit(1);
	}
	if (pid)
		return(0);
	/* child */
	fd = open("/dev/tty", O_RDWR);
	if (fd > 0)
		(void) ioctl(fd, TIOCNOTTY, (char *)0);
	for (fd = getdtablehi(); --fd >= 0;) {
		if (trace && fd == 2)
			continue;
		(void) close(fd);
	}
#endif /* NOFORK */

	openlog("autofsd", LOG_PID | LOG_NOWAIT | LOG_CONS, LOG_DAEMON);

	if ((maxthreads > 1) && 
	   ((rpc_control(RPC_SVC_THRMAX_GET, &prevthreads)) == TRUE) &&
	   ((rpc_control(RPC_SVC_THRMAX_SET, &maxthreads)) == TRUE)) {
		rlset.rlim_max = RESOURCE_FACTOR * maxthreads;
		rlset.rlim_cur = RESOURCE_FACTOR * maxthreads;
		if ((setrlimit(RLIMIT_NOFILE, &rlset)) != 0) {
			syslog(LOG_ERR, "unable to increase system resource limit");
			if ((rpc_control(RPC_SVC_THRMAX_SET, 
				&prevthreads)) == FALSE) {
				syslog(LOG_ERR, "unable to match threads to resources");
				exit(1);
			}
			syslog(LOG_ERR, "decreased threads to match low resources");
		} else {
			if (!rpc_control(RPC_SVC_MTMODE_SET, &rpc_svc_mode)) {
				syslog(LOG_ERR, "unable to set automatic MT mode");
				exit(1);
			}
		}
	}

	/*
	 * If we coredump it'll be /core.
	 */
	if (chdir("/") < 0)
		syslog(LOG_ERR, "chdir /: %m");
	
	/*
	 * Get all IP addresses for this host
	 */
	getmyaddrs();

	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("autofsd cannot create socket");
		exit(1);
	}
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(AUTOFS_PORT);
	if (bind(sock, (struct sockaddr *)&addr, asize) != 0) {
		perror("autofsd cannot bind");
		exit(1);
	}
	
	transp = svcudp_create(sock);
	if (transp == NULL) {
		syslog(LOG_ERR, "Cannot create UDP service");
		exit(1);
	}
	if (!svc_register(transp, AUTOFS_PROG, AUTOFS_VERS, 
			  autofs_prog, IPPROTO_UDP)) {
		syslog(LOG_ERR, "svc_register failed");
		exit(1);
	}
	
	
	sigemptyset(&sigset_hup);
	sigaddset(&sigset_hup, SIGHUP);

	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags    = SA_RESTART;
	sigact.sa_handler  = handle_hup;
	sigaction(SIGHUP, &sigact, NULL);

	if (cap_sched_setscheduler(0, policy, &sparam) < 0) {
		syslog(LOG_ERR,
		       "Cannot set scheduler priority: %m");
		perror("autofsd cannot set scheduler priority");
		exit(1);
	}
	svc_run();
	syslog(LOG_ERR, "svc_run returned");
	exit(1);

	return(0);
}

/*
 * The old automounter supported a SIGHUP
 * to allow it to resynchronize internal
 * state with the /etc/mtab.
 * This is no longer relevant, but we
 * need to catch the signal and warn
 * the user.
 */
/*ARGSUSED*/
void
handle_hup(int sig)
{
	hup_recv = 1;
}

void
usage(void)
{
	(void) fprintf(stderr, "Usage: autofsd\n"
		"\t[-T]\t\t(trace requests)\n"
		"\t[-v]\t\t(verbose error msgs)\n"
		"\t[-p n]\t\t(process priority)\n"
		"\t[-m n]\t\t(set max threads)\n"
		"\t[-tp n]\t\t(ping status timeout)\n"
		"\t[-D n=s]\t(define env variable)\n");
	exit(1);
}

static void
service_requests(struct svc_req *rqstp, register SVCXPRT *transp)
{
	union {
		mntrequest autofs_mount_1_arg;
		umntrequest autofs_umount_1_arg;
	} argument;

	union {
		mntres mount_res;
		umntres umount_res;
	} res;

	bool_t (*xdr_argument)(), (*xdr_result)();
	char *(*local)();
	char *result;

	struct sockaddr_in *who;
	
        who = svc_getcaller(transp);

	/* 
	 * Relying that 
	 * 1. clntkudp_init in the kernel binds to a reserved port. 
	 * 2. autofs_init in the kernel initializes IP to INADDR_LOOPBACK.
	 * If not this code has to be fixed.
	 */
	if (rqstp->rq_proc != NULLPROC && 
	    (who->sin_addr.s_addr != INADDR_LOOPBACK ||
	     ntohs(who->sin_port) >= IPPORT_RESERVED)) {
		char b[256];	
		
		local_inet_ntoa(who->sin_addr.s_addr);
		syslog(LOG_ERR,
		       "ALERT: %s(%d) attempt made from port=%d addr=%s "
		       "by uid=%d.",
		       rqstp->rq_proc == AUTOFS_MOUNT ? "Mount" : 
		       rqstp->rq_proc == AUTOFS_UNMOUNT ? "Unmount" :
		       "Unknown procedure", 
		       rqstp->rq_proc, who->sin_port, b, 
		       ((struct authunix_parms *)rqstp->rq_clntcred)->aup_uid);
		svcerr_noproc(transp);
		return;
	}

	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void) svc_sendreply(transp, xdr_void, (char *)NULL);
		return;

	case AUTOFS_MOUNT:
		xdr_argument = xdr_mntrequest;
		xdr_result = xdr_mntres;
		local = (char *(*)()) autofs_mount_1;
		break;

	case AUTOFS_UNMOUNT:
		xdr_argument = xdr_umntrequest;
		xdr_result = xdr_umntres;
		local = (char *(*)()) autofs_unmount_1;
		break;

	default:
		svcerr_noproc(transp);
		return;
	}

	memset((void *) &argument, '\0', sizeof (argument));
	if (!svc_getargs(transp, xdr_argument, (caddr_t) &argument)) {
		svcerr_decode(transp);
		return;
	}
	result = (*local)(&argument, &res, rqstp->rq_clntcred);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}

	if (!svc_freeargs(transp, xdr_argument, (caddr_t) &argument)) {
		syslog(LOG_ERR, "unable to free arguments");
	}
}

/*
 * Duplicate request checking.
 * Use a small fifo xid cache
 * to check for retransmitted requests.
 * Since the daemon is stateless, it will
 * attempt to mount over existing mounts
 * if it acts on a retransmited request
 * and will complain loudly about "busy"
 * mounts.
 *
 * XXX Note: this code should be removed
 * when the autofsd supports a
 * connectionless transport.
 */
#define	XID_CACHE_SIZE 256
u_long xid_cache[XID_CACHE_SIZE];
u_long *xcpfirst = &xid_cache[0];
u_long *xcp	 = &xid_cache[0];
u_long *xcplast  = &xid_cache[XID_CACHE_SIZE - 1];

#define	MAX_OPT_WORDS	32

struct svc_dg_data {
	u_char	flags;
	signed char su_iosz;
	u_long	su_xid;
	long	opts[MAX_OPT_WORDS];
	XDR	su_xdrs;
	char	su_verfbody[MAX_AUTH_BYTES];
	char 	*su_cache;
};

static int
dup_request(struct svc_req *req)
{
	u_long *x;
	int xid;

	if (req->rq_xprt->xp_p2 == NULL) {
		if (verbose)
			syslog(LOG_ERR, "dup_request: no xid");
		return (0);
	}

	xid = ((struct svc_dg_data *)(req->rq_xprt->xp_p2))->su_xid;
	/*
	 * Search the cache for the xid
	 */
	AUTOFS_LOCK(xc_lock);
	for (x = xcp; x >= xcpfirst; x--)
		if (xid == *x) {
			AUTOFS_UNLOCK(xc_lock);
			return (1);
		}
	for (x = xcplast; x > xcp; x--)
		if (xid == *x) {
			AUTOFS_UNLOCK(xc_lock);
			return (1);
		}

	/*
	 * Not there. Enter it.
	 */
	*xcp++ = xid;
	if (xcp > xcplast)
		xcp = xcpfirst;

	AUTOFS_UNLOCK(xc_lock);
	return (0);
}

#define REFRESH_MYADDRS_TIME (60*10)

static void
autofs_prog(rqstp, transp)
	struct svc_req *rqstp;
	register SVCXPRT *transp;
{

	time_now = time((long *) 0);
	if (dup_request(rqstp)) {
		return;
	}
	
	AUTOFS_LOCK(misc_lock);
	if (hup_recv || 
	    ((time_now - myaddrs_time) > REFRESH_MYADDRS_TIME)) {
		getmyaddrs();
		if (hup_recv)
			hup_recv = 0;
	}
	AUTOFS_UNLOCK(misc_lock);

	sigprocmask(SIG_BLOCK, &sigset_hup,NULL);
	service_requests(rqstp,transp);
	sigprocmask(SIG_UNBLOCK, &sigset_hup,NULL);
}

/*ARGSUSED*/
struct umntres *
autofs_unmount_1(m, res, cred)
	struct umntrequest *m;
	struct umntres *res;
	struct authunix_parms *cred;
{
	struct umntrequest *ul;
	unsigned self;

	self = _mplib_get_thread_id();

	if (trace > 0) {
		(void) syslog(LOG_ERR,
			"UNMOUNT REQUEST:\n");
		for (ul = m; ul; ul = ul->next)
			(void) syslog(LOG_ERR,
				"  dev=%x %s id=%d\n",
				ul->devid,
				ul->isdirect ? "direct":"indirect", self);
	}

	res->status = do_unmount1(m);

	if (trace > 0)
		(void) syslog(LOG_ERR,
			"UNMOUNT REPLY  : status=%d id=%d\n",
			res->status, self);

	return (res);
}


struct mntres *
autofs_mount_1(m, res, cred)
	struct mntrequest *m;
	struct mntres *res;
	struct authunix_parms *cred;
{
	unsigned self;

	self = _mplib_get_thread_id();

	if (trace > 0) {
		(void) syslog(LOG_ERR,
			"MOUNT REQUEST  : "
			"name=%s map=%s opts=%s path=%s id=%d\n",
			m->name, m->map, m->opts, m->path, self);
	}

	res->status = do_mount1(m->map, m->name, m->opts, m->path, cred);

	if (trace > 0) {
		(void) syslog(LOG_ERR,
			"MOUNT REPLY	: status=%d id=%d\n",
			res->status, self);
	}

	if (res->status && verbose) {
		if (strcmp(m->path, m->name) == 0) {
			/* direct mount */
			syslog(LOG_ERR, "mount of %s failed", m->name);
		} else {
			/* indirect mount */
			syslog(LOG_ERR,
				"mount of %s/%s failed", m->path, m->name);
		}
	}
	return (res);
}

/* Stolen from portmap.c. Change when SIOCGIFNUM or equivalent comes. */

#define	MAX_LOCAL 82
#define	MAX_LOCAL_ALIASES 120
static int num_local = -1;
struct in_addr myaddrs[MAX_LOCAL];

void
getmyaddrs(void)
{
	struct ifconf	ifc;
	struct ifreq	*ifr;
	int		n, j, sock;
	char		buf[UDPMSGSIZE];
	struct ifaliasreq ifalias;
	struct sockaddr_in *sin;

	ifc.ifc_len = UDPMSGSIZE;
	ifc.ifc_buf = buf;
	sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
		return;
	if (ioctl(sock, SIOCGIFCONF, (char *) &ifc) < 0) {
		perror("ioctl SIOCGIFCONF");
		(void) close(sock);
		return;
	}
	ifr = ifc.ifc_req;
	j = 0;
	/* get all interfaces and aliases */
	for (n = ifc.ifc_len / sizeof (struct ifreq); n > 0; n--, ifr++) {
		/*
		 * Use SIOCLIFADDR to get interface aliases.
		 * Start cookie at 0 to include primary address.
		 * Cookie is incremented on success, -1 on end of list.
		 */
		bzero (&ifalias, sizeof (ifalias));
		strncpy(ifalias.ifra_name, ifr->ifr_name,
			sizeof (ifalias.ifra_name));
		ifalias.ifra_addr.sa_family = AF_INET;
		ifalias.cookie = 0;
		while (j < MAX_LOCAL) {
			if (ioctl(sock, SIOCLIFADDR, (char *) &ifalias) < 0) {
				perror("ioctl SIOCLIFADDR");
				(void) close(sock);
				return;
			}
			if (ifalias.cookie == -1)
				break;
			sin = (struct sockaddr_in *) &ifalias.ifra_addr;
			myaddrs[j++].s_addr = sin->sin_addr.s_addr;
		}
		if (j >= MAX_LOCAL)
			break;
	}
	num_local = j;
	myaddrs_time = time((long *) 0);
	(void) close(sock);
}

int
self_check(hostname)
	char *hostname;
{
	struct hostent server;
	int j;
	int  herr;
	char buf[4096];

	/*
	 * First do a quick comparison of hostname
	 */
	if (strcmp(hostname, self) == 0)
		return (1);

	if (! gethostbyname_r(hostname, &server, buf, sizeof(buf), &herr)) {
		syslog(LOG_ERR, "Can't find hostname information for %s", 
			hostname);
		return(0);
	}
	/*
	 * Now compare it against the list of
	 * addresses for the interfaces on this
	 * host.
	 */
	for (j=0;j < num_local;j++) {
		if (myaddrs[j].s_addr == *((u_long *)server.h_addr)) {
			return(1);	  /* it's me */
		}
	}
	return (0);			/* it's not me */
}

/*
 * Used for reporting messages from code
 * shared with autofs command.
 * Formats message into a buffer and
 * calls syslog.
 *
 * Print an error.
 * Works like printf (fmt string and variable args)
 * except that it will subsititute an error message
 * for a "%m" string (like syslog).
 */
void
pr_msg(char *fmt, ...)
{
	va_list ap;
	char fmtbuff[BUFSIZ], buff[BUFSIZ], *p2;
	char *p1, *errstr;

	p2 = fmtbuff;
	fmt = gettext(fmt);

	for (p1 = fmt; *p1; p1++) {
		if (*p1 == '%' && *(p1+1) == 'm') {
			errstr = strerror(errno);
			if (errstr != NULL) {
				(void) strcpy(p2, errstr);
				p2 += strlen(p2);
			}
			p1++;
		} else {
			*p2++ = *p1;
		}
	}
	if (p2 > fmtbuff && *(p2-1) != '\n')
		*p2++ = '\n';
	*p2 = '\0';

	va_start(ap, fmt);
	(void) vsprintf(buff, fmtbuff, ap);
	va_end(ap);
	syslog(LOG_ERR, buff);
}
