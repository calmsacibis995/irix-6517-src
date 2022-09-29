#if 0
static	char sccsid[] = "@(#)ypbind.c	1.4 88/07/29 4.0NFSSRC"; /* from 1.31 88/02/07 SMI Copyr 1985 Sun Micro */
#endif

/*
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 */

/*
 * This constructs a list of servers by domains, and keeps more-or-less up to
 * date track of those server's reachability.
 */

#define _BSD_SIGNALS

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <bstring.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <syslog.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <rpc/rpc.h>
#include <rpc/svc.h>
#include <sys/dir.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>		/* for ntohx() */
#include <net/if.h>		/* to find local addresses */
#include <net/route.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/sysctl.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include <rpc/pmap_clnt.h>

/*
 * The domain struct is the data structure used by the NIS binder to remember
 * mappings of domain to a server.  The list of domains is pointed to by
 * known_domains.  Domains are added when the NIS binder gets binding requests
 * for domains which are not currently on the list.  Once on the list, an
 * entry stays on the list forever.  Bindings are initially made by means of
 * a broadcast method, using functions ypbind_broadcast_bind and
 * ypbind_broadcast_ack.  This means of binding is re-done any time the domain
 * becomes unbound, which happens when a server doesn't respond to a ping.
 * current_domain is used to communicate among the various functions in this
 * module; it is set by ypbind_get_binding.
 *
 */
struct domain {
	struct domain *dom_pnext;
	char dom_name[MAXNAMLEN + 1];
	bool_t dom_boundp;
	bool_t dom_new;			/* no broadcaster yet */
	bool_t dom_perm;		/* permanently bound? */
	CLIENT *ping_clnt;
	struct in_addr dom_serv_addr;
	unsigned short int dom_serv_port;
	int dom_report_success;		/* Controls msg to /dev/console*/
	int dom_broadcaster_pid;
	bool_t dom_set;			/* ypset */
	FILE *broadcaster_pipe;		/* to get answer from broadcaster */
	XDR broadcaster_xdr;
};
static int ping_sock = RPC_ANYSOCK;
struct domain init_domain = {
	0,				/* dom_pnext */
	{ 0 },				/* dom_name */
	1,				/* dom_boundp */
	1,				/* dom_new */
	0,				/* dom_perm */
	0,				/* ping_clnt */
	{ 0 }, 0,			/* dom_serv_addr, dom_serv_port */
	-1,				/* dom_report_success */
	0,				/* dom_broadcaster_pid */
	0,				/* dom_set */
	NULL,				/* broadcaster_pipe */
};
struct domain *known_domains = &init_domain;
struct domain *current_domain;		/* Used by ypbind_broadcast_ack, set
					 *   by all callers of clnt_broadcast */
SVCXPRT *tcphandle;
SVCXPRT *udphandle;

#define PINGTOTTIM 20			/* Total seconds for ping timeout */
#define PINGINTERTRY 10
#define SETDOMINTERTRY 20
#define SETDOMTOTTIM 60

static int debug = FALSE;
static int verbose = FALSE;
static int secure = FALSE;		/* running c2 secure for -s */
static u_long ttl = 2;			/* initial multicast TTL */

static enum {YPSETANY, YPSETLOCAL, YPSETNONE} setok = YPSETNONE;

void
cleanup()
{
	(void) pmap_unset(YPBINDPROG, YPBINDVERS);
	exit(0);
}


void dispatch();
void ypbind_dispatch();
void ypbind_get_binding();
void ypbind_set_binding();
void ypbind_pipe_setdom();
static struct domain *ypbind_point_to_domain(char *, unsigned short);
static bool_t ypbind_broadcast_ack(void *, struct sockaddr_in *);
static void ypbind_broadcast_bind(unsigned short);
static int ypbind_ping(struct domain *, bool_t);
void broadcast_proc_exit();
static bool_t chklocal(struct in_addr);
static void getlocal(void);

const char prog[] = "ypbind";

main(argc, argv)
	int argc;
	char **argv;
{
	fd_set readfds;
	int oldmask;
	int i;
	char *p;

	openlog(prog, LOG_PID|LOG_NOWAIT|LOG_CONS, LOG_DAEMON);

	if (0 > getdomainname(init_domain.dom_name,
			      sizeof(init_domain.dom_name))) {
		syslog(LOG_ERR, "getdomainname: %m");
		exit(1);
	}
	/* User started us by hand, so make error visible */
	if (init_domain.dom_name[0] == '\0') {
		fprintf(stderr, "ypbind: error: domain name not set\n");
		exit(1);
	}
	if (getuid() != 0) {
		fprintf(stderr, "ypbind: You are not superuser\n");
		exit(1);
	}

	(void) signal(SIGHUP, SIG_IGN);
	(void) signal(SIGTERM, cleanup);

	(void) pmap_unset(YPBINDPROG, YPBINDVERS);

	opterr = 0;
	while ((i = getopt(argc,argv,"svy:DT:")) != -1) {
		switch (i) {
		case 's':		/* -s[ecure] */
			secure = TRUE;
			(void) syslog(LOG_INFO, "running secure");
			break;
		case 'v':
			verbose = TRUE;
			(void) syslog(LOG_INFO, "running verbose");
			break;
		case 'y':
			/*
			 * kludge to handle "-ypset" and "-ypsetme"
			 * and still allow "-svt0"
			 */
			if (!strcmp(optarg,"pset"))
				setok = YPSETANY;
			else if (!strcmp(optarg, "psetme"))
				setok = YPSETLOCAL;
			else
				goto usage;
			break;
		case 'D':
			debug = TRUE;
			(void) syslog(LOG_INFO, "debugging");
			break;
		case 'T':
			ttl = strtoul(optarg, &p, 0);
			if (*p != '\0' || ttl > 32) {
				ttl = 2;
				(void) syslog(LOG_ERR,"bad ttl: %s",optarg);
			}
			break;
		default:
usage:
			(void) syslog(LOG_ERR, "usage: %s [-sv] [-T ttl]"
				      " [-ypset | -ypsetme] [server]",
				      prog);
			exit(1);
		}
	}

	/* Try ourself, unless a permanent server was specified */
	init_domain.dom_serv_addr.s_addr = INADDR_LOOPBACK;
	if (argc > optind) {
		struct hostent *hp;

		if ((hp = gethostbyname(argv[optind])) != NULL) {
			bcopy(hp->h_addr, &init_domain.dom_serv_addr,
				hp->h_length);
			init_domain.dom_perm = TRUE;
		} else
			syslog(LOG_ERR, "can't find address for %s",
				argv[optind]);
	}

	if (!debug) {
		int t, pid;

		pid = fork();

		if (pid == -1) {
			(void) syslog(LOG_ERR, "fork failed: %m");
			exit(1);
		}

		if (pid != 0) {
			exit(0);
		}

		closelog();
		for (t = getdtablehi(); --t > 0; )
			(void) close(t);

		t = open("/dev/tty", 2);
		if (t >= 0) {
			(void) ioctl(t, TIOCNOTTY, (char *)0);
			(void) close(t);
		}

		openlog(prog, LOG_PID|LOG_NOWAIT|LOG_CONS, LOG_DAEMON);
	}

	if (signal(SIGCHLD, broadcast_proc_exit) == SIG_ERR) {
		(void) syslog(LOG_ERR,"Can't establish reaper signal handler.");
		exit(1);
	}
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		(void) syslog(LOG_ERR,"Can't establish pipe signal handler.");
		exit(1);
	}
	/* Open a socket for pinging everyone can use */
	ping_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (ping_sock < 0) {
		(void) syslog(LOG_ERR, "Cannot create socket for pinging: %m");
		exit(1);
	}

	/*
	 * if not running c2 secure, don't use priviledged ports.
	 * Accomplished by side effect of not being root when creating
	 * rpc based sockets.
	 */
	if (! secure) {
		(void) setreuid(-1, 3);
	}

	if ((tcphandle = svctcp_create(RPC_ANYSOCK,
	    RPCSMALLMSGSIZE, RPCSMALLMSGSIZE)) == NULL) {
		(void) syslog(LOG_ERR, "Can't create tcp service.");
		exit(1);
	}

	if (!svc_register(tcphandle, YPBINDPROG, YPBINDVERS,
	    ypbind_dispatch, IPPROTO_TCP) ) {
		(void) syslog(LOG_ERR, "Can't register tcp service.");
		exit(1);
	}

	if ((udphandle = svcudp_bufcreate(RPC_ANYSOCK,
	    RPCSMALLMSGSIZE, RPCSMALLMSGSIZE)) == (SVCXPRT *) NULL) {
		(void) syslog(LOG_ERR, "Can't create udp service.");
		exit(1);
	}

	if (!svc_register(udphandle, YPBINDPROG, YPBINDVERS,
	    ypbind_dispatch, IPPROTO_UDP) ) {
		(void) syslog(LOG_ERR, "Can't register udp service.");
		exit(1);
	}

	/* undo the gross hack associated with c2 security */
	if (! secure)
		(void) setreuid(-1, 0);

	/* See if we guessed right about our initial server */
	current_domain = &init_domain;
	if (ypbind_ping(current_domain, FALSE) == FALSE) {
		init_domain.dom_boundp = FALSE;
		oldmask = sigblock(sigmask(SIGCHLD));
		ypbind_broadcast_bind(YPVERS);
		(void) sigsetmask(oldmask);
	}

	for (;;) {
		readfds = svc_fdset;
		errno = 0;

		switch (select(FD_SETSIZE, &readfds, NULL, NULL, NULL)) {

		case -1:  {

			if (errno != EINTR) {
			    (void) syslog(LOG_ERR, "main loop select: %m");
			}

			break;
		}

		case 0:  {
			(void) syslog(LOG_ERR,
				"Invalid timeout in main loop select.");
			break;
		}

		default:  {
			oldmask = sigblock(sigmask(SIGCHLD));
			svc_getreqset (&readfds);
			(void) sigsetmask(oldmask);
			break;
		}

		}
	}
}

/*
 * ypbind_dispatch is a wrapper for dispatch which
 * remember which protocol the requestor is looking for.  The theory is,
 * that since YPVERS and YPBINDVERS are defined in the same header file, if
 * a request comes in on the old binder protocol, the requestor is looking
 * for the old NIS server.
 */
void
ypbind_dispatch(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	dispatch(rqstp, transp, (unsigned short) YPVERS);
}

/*
 * This dispatches to binder action routines.
 */
void
dispatch(rqstp, transp, vers)
	struct svc_req *rqstp;
	SVCXPRT *transp;
	unsigned short vers;
{

	switch (rqstp->rq_proc) {

	case YPBINDPROC_NULL:

		if (!svc_sendreply(transp, xdr_void, 0) ) {
			(void) syslog(LOG_ERR, "Can't reply to rpc call.");
		}

		break;

	case YPBINDPROC_DOMAIN:
		ypbind_get_binding(rqstp, transp, vers);
		break;

	case YPBINDPROC_SETDOM:
		ypbind_set_binding(rqstp, transp, vers);
		break;

	default:
		svcerr_noproc(transp);
		break;

	}
}


static void
close_pipe(struct domain *pdom)
{
	if (pdom->broadcaster_pipe) {
		xdr_destroy(&pdom->broadcaster_xdr);
		(void) fclose(pdom->broadcaster_pipe);
		pdom->broadcaster_pipe = NULL;
	}
}

/*
 * This is a Unix SIGCHILD handler which notices when a broadcaster child
 * process has exited, and retrieves the exit status.  The broadcaster pid
 * is set to 0.  If the broadcaster succeeded, dom_report_success will be
 * be set to -1.
 */

void
broadcast_proc_exit()
{
	int pid;
	int wait_status;
	register struct domain *pdom;
	struct ypbind_setdom req;

	while ((pid = wait3(&wait_status,WNOHANG,NULL)) > 0) {
	    for (pdom = known_domains; pdom != (struct domain *)NULL;
		pdom = pdom->dom_pnext) {

		if (pdom->dom_broadcaster_pid == pid) {
		    pdom->dom_broadcaster_pid = 0;
		    pdom->dom_new = FALSE;

		    if ((WTERMSIG(wait_status) == 0) &&
			(WEXITSTATUS(wait_status) == 0)) {

			if (pdom->broadcaster_pipe) {
			    pdom->dom_report_success = -1;

			    if (xdr_ypbind_setdom(&pdom->broadcaster_xdr,&req)){
				    pdom->dom_serv_addr = req.ypsetdom_addr;
				    pdom->dom_serv_port = req.ypsetdom_port;
				    pdom->dom_boundp = TRUE;
				    if (pdom->ping_clnt != (CLIENT *)NULL) {
					    clnt_destroy(pdom->ping_clnt);
					    pdom->ping_clnt = (CLIENT *)NULL;
				    }
				    if (verbose)
					syslog(LOG_INFO, "bound to %s for %s",
						inet_ntoa(req.ypsetdom_addr),
						pdom->dom_name);
			    } else {
				syslog(LOG_ERR, "xdr_ypbind_setdom failed");
			    }
			} else {
			    syslog(LOG_ERR,
				"internal error: no broadcaster pipe");
			}
		    } else if (!pdom->dom_set)
			pdom->dom_boundp = FALSE;

		    pdom->dom_set = FALSE;

		    /* Always free the pipe */
		    close_pipe(pdom);
		}
	    }
	}
}

/*
 * This returns the current binding for a passed domain.
 */
void
ypbind_get_binding(rqstp, transp, vers)
	struct svc_req *rqstp;
	register SVCXPRT *transp;
	unsigned short vers;
{
	char *pdomain_name = NULL;
	struct ypbind_resp response;

	if (!svc_getargs(transp, xdr_ypdomain_wrap_string, &pdomain_name) ) {
		svcerr_decode(transp);
		return;
	}

	if ( (current_domain = ypbind_point_to_domain(pdomain_name, vers) ) !=
	    (struct domain *) NULL) {

		/*
		 * Bound or not, return the current state of the binding.
		 */

		if (current_domain->dom_boundp || current_domain->dom_perm) {
			response.ypbind_status = YPBIND_SUCC_VAL;
			response.ypbind_respbody.ypbind_bindinfo.ypbind_binding_addr =
			    current_domain->dom_serv_addr;
			response.ypbind_respbody.ypbind_bindinfo.ypbind_binding_port =
			    current_domain->dom_serv_port;
		} else if (current_domain->dom_new) {
			response.ypbind_status = YPBIND_FAIL_VAL;
			response.ypbind_respbody.ypbind_error =
			    YPBIND_ERR_ERR;
		} else {
			response.ypbind_status = YPBIND_FAIL_VAL;
			response.ypbind_respbody.ypbind_error =
			    YPBIND_ERR_NOSERV;
		}

	} else {
		response.ypbind_status = YPBIND_FAIL_VAL;
		response.ypbind_respbody.ypbind_error = YPBIND_ERR_RESC;
	}

	if (!svc_sendreply(transp, xdr_ypbind_resp, &response) ) {
		(void) syslog(LOG_ERR, "Can't respond to rpc request.");
	}

	if (!svc_freeargs(transp, xdr_ypdomain_wrap_string, &pdomain_name) ) {
		(void) syslog(LOG_ERR, "ypbind_get_binding can't free args.");
	}

	if (FALSE == ypbind_ping(current_domain, TRUE))
		ypbind_broadcast_bind(vers);
}

static void
conslog(int pri, const char *fmt, ...)
{
	FILE *f;
	va_list ap;

	va_start(ap, fmt);
	vsyslog(pri, fmt, ap);
	if ((f = fopen("/dev/console", "w")) != NULL) {
		vfprintf(f, fmt, ap);
		putc('\n', f);
		(void) fclose(f);
	}
	va_end(ap);
}

static void
ypbind_broadcast_bind(unsigned short vers)
{
	char *pname;
	int broadcaster_pid = -2;
	int fildes[2];

	if ((current_domain) && (!current_domain->dom_broadcaster_pid)) {
		/*
		 * The current domain is unbound, and there is no broadcaster
		 * process active now.  Fork off a child who will yell out on
		 * the net.  Because of the flavor of request we're making of
		 * the server, we only expect positive ("I do serve this
		 * domain") responses.
		 */
		current_domain->dom_report_success++;
		pname = current_domain->dom_name;


		if (pipe(fildes) >= 0 && (broadcaster_pid = fork()) == 0) {
			int	true = 1;

			if (verbose)
				syslog(LOG_INFO,
				       "broadcasting%s for %s (v.%d)",
				       ttl == 0 ? "" : " and multicasting",
				       pname, vers);

			/* Allow "set domain" proc to kill this process */
			(void) signal(SIGTERM, SIG_DFL);

			current_domain->broadcaster_pipe= fdopen(fildes[1],"w");
			if (current_domain->broadcaster_pipe) {
				xdrstdio_create(
					&current_domain->broadcaster_xdr,
					current_domain->broadcaster_pipe,
					XDR_ENCODE);
			} else {
				syslog(LOG_ERR, "fdopen pipe");
				exit(1);
			}

			if (ttl == 0)
				(void) clnt_broadcast(YPPROG, vers,
					    YPPROC_DOMAIN_NONACK,
					    xdr_ypdomain_wrap_string,
					    &pname, xdr_int, &true,
					    ypbind_broadcast_ack);
			else
				(void) clnt_broadmulti(YPPROG, vers,
					    YPPROC_DOMAIN_NONACK,
					    xdr_ypdomain_wrap_string,
					    &pname, xdr_int, &true,
					    ypbind_broadcast_ack,
					    ttl, 8);

			if (current_domain->dom_boundp) {

				/*
				 * Send out a set domain request to our parent
				 */
				ypbind_pipe_setdom(current_domain, pname,
				    current_domain->dom_serv_addr,
				    current_domain->dom_serv_port, vers);

				if (current_domain->dom_report_success > 0) {
					conslog(LOG_INFO,
					 "NIS server for domain \"%s\" OK",
					    pname);
				}

				exit(0);
			} else {

				conslog(LOG_WARNING,
	      "NIS v.%d server not responding for domain \"%s\"; still trying",
				    vers, pname);
				exit(1);
			}

		} else if (broadcaster_pid < 0) {
			if (broadcaster_pid == -2) {
				(void) syslog(LOG_ERR,
				    "broadcaster pipe failure: %m");
			} else {
				(void) syslog(LOG_ERR,
				    "broadcaster fork failure: %m");
				(void) close(fildes[0]);
				(void) close(fildes[1]);
			}
		} else {
			/* Parent */
			(void) close(fildes[1]);
			current_domain->broadcaster_pipe= fdopen(fildes[0],"r");
			if (current_domain->broadcaster_pipe)
				xdrstdio_create(
					&current_domain->broadcaster_xdr,
					current_domain->broadcaster_pipe,
					XDR_DECODE);
			current_domain->dom_broadcaster_pid = broadcaster_pid;
		}
	}
}

/*
 * This sends a (current version) ypbind "Set domain" message back to our
 * parent.  The version embedded in the protocol message is that which is passed
 * to us as a parameter.
 */
void
ypbind_pipe_setdom(dp, dom, addr, port, vers)
	struct domain *dp;
	char *dom;
	struct in_addr addr;
	unsigned short int port;
	unsigned short int vers;
{
	struct ypbind_setdom req;

	strcpy(req.ypsetdom_domain, dom);
	req.ypsetdom_addr = addr;
	req.ypsetdom_port = port;
	req.ypsetdom_vers = vers;
	xdr_ypbind_setdom(&(dp->broadcaster_xdr),&req);
	xdr_destroy(&(dp->broadcaster_xdr));
	(void) fclose(dp->broadcaster_pipe);
	dp->broadcaster_pipe = (FILE *)NULL;
}

/*
 * This sets the internet address and port for the passed domain to the
 * passed values, and marks the domain as supported.  This accepts both the
 * old style message (in which case the version is assumed to be that of the
 * requestor) or the new one, in which case the protocol version is included
 * in the protocol message.  This allows our child process (which speaks the
 * current protocol) to look for NIS servers on behalf old-style clients.
 */
void
ypbind_set_binding(rqstp, transp, vers)
	struct svc_req *rqstp;
	register SVCXPRT *transp;
	unsigned short vers;
{
	struct ypbind_setdom req;
	unsigned short version;
	struct in_addr addr;
	struct sockaddr_in *who;
	unsigned short int port;
	char *domain;
	char a[17];

	if (!svc_getargs(transp, xdr_ypbind_setdom, &req) ) {
		svcerr_decode(transp);
		return;
	}
	version = req.ypsetdom_vers;
	addr = req.ypsetdom_addr;
	port = req.ypsetdom_port;
	domain = req.ypsetdom_domain;

	/* find out who originated the request */
	who = svc_getcaller(transp);

	if (setok == YPSETNONE) {
		strcpy(a, inet_ntoa(addr));
		syslog(LOG_ERR,
"Set domain request to host %s, domain %s, from host %s, failed (ypset not allowed)!",
			a, domain, inet_ntoa(who->sin_addr));
		svcerr_systemerr(transp);
		return;
	}

	/* This code implements some restrictions on who can set the
	 * NIS server for this host.
	 *
	 * This policy is that root can set the NIS server to anything,
	 * everyone else can't. This should also check for a valid NIS
	 * server.
	 */

	if (ntohs(who->sin_port) >= IPPORT_RESERVED) {
		strcpy(a, inet_ntoa(addr));
		syslog(LOG_ERR,
"Set domain request to host %s, domain %s, from host %s, failed (bad port %d)",
		    a, domain, inet_ntoa(who->sin_addr), ntohs(who->sin_port));
		svcerr_systemerr(transp);
		return;
	}

	if (setok == YPSETLOCAL && !chklocal(who->sin_addr)) {
		strcpy(a, inet_ntoa(addr));
		syslog(LOG_ERR,
"Set domain request to host %s, domain %s, from host %s, failed (not local).",
			a, domain, inet_ntoa(who->sin_addr));
		svcerr_systemerr(transp);
		return;
	}

	/* Now check the credentials */
	if (rqstp->rq_cred.oa_flavor == AUTH_UNIX) {
		if (((struct authunix_parms *)rqstp->rq_clntcred)->aup_uid != 0) {
			strcpy(a, inet_ntoa(addr));

			(void) syslog(LOG_ERR,
"Set domain request to host %s, domain %s, from host %s, failed (not root).",
				a, domain, inet_ntoa(who->sin_addr));
			svcerr_systemerr(transp);
			return;
		}
	} else {
		strcpy(a, inet_ntoa(addr));
		syslog(LOG_ERR,
"Set domain request to host %s, domain %s, from host %s, failed (bad credentials).",
			a, domain, inet_ntoa(who->sin_addr));
		svcerr_weakauth(transp);
		return;
	}

	if (!svc_sendreply(transp, xdr_void, 0) ) {
		syslog(LOG_ERR, "Can't reply to rpc call.");
	}

	if (verbose) {
		strcpy(a, inet_ntoa(addr));
		syslog(LOG_INFO,
	    "Set domain request to host %s, domain %s, from host %s, succeeded",
		    a, domain, inet_ntoa(who->sin_addr));
	}

	if ( (current_domain = ypbind_point_to_domain(domain,
	    version) ) != (struct domain *) NULL) {
		/*
		 * Kill the broadcaster because we've got a server.
		 * Set a flag to prevent the reaper from unbinding the domain.
		 */
		if (current_domain->dom_broadcaster_pid) {
			current_domain->dom_set = TRUE;
			kill(current_domain->dom_broadcaster_pid, SIGTERM);
		}
		current_domain->dom_serv_addr = addr;
		current_domain->dom_serv_port = port;
		current_domain->dom_boundp = TRUE;
		/* get rid of old pinging client if one exists */
		if (current_domain->ping_clnt != (CLIENT *)NULL) {
			clnt_destroy(current_domain->ping_clnt);
			current_domain->ping_clnt = (CLIENT *)NULL;
		}
	}
}
/*
 * This returns a pointer to a domain entry.  If no such domain existed on
 * the list previously, an entry will be allocated, initialized, and linked
 * to the list.  Note:  If no memory can be malloc-ed for the domain structure,
 * the functional value will be (struct domain *) NULL.
 */
static struct domain *
ypbind_point_to_domain(register char *pname,
		       unsigned short vers)
{
	register struct domain *pdom;

	for (pdom = known_domains; pdom != (struct domain *)NULL;
	    pdom = pdom->dom_pnext) {
		if (!strcmp(pname, pdom->dom_name) && vers == YPVERS)
			return (pdom);
	}

	/* Not found.  Add it to the list */

	if (pdom = (struct domain *)malloc(sizeof (struct domain))) {
		pdom->dom_pnext = known_domains;
		known_domains = pdom;
		strcpy(pdom->dom_name, pname);
		pdom->dom_boundp = FALSE;
		pdom->dom_perm = FALSE;
		pdom->ping_clnt = (CLIENT *)NULL;
		pdom->dom_report_success = -1;
		pdom->dom_broadcaster_pid = 0;
		pdom->dom_new = TRUE;
		pdom->dom_set = FALSE;
		pdom->broadcaster_pipe = NULL;
	}

	return (pdom);
}

/*
 * This is called by the broadcast rpc routines to process the responses
 * coming back from the broadcast request. Since the form of the request
 * which is used in ypbind_broadcast_bind is "respond only in the positive
 * case", we know that we have a server.  If we should be running secure,
 * return FALSE if this server is not using a reserved port.  Otherwise,
 * the internet address of the responding server will be picked up from
 * the saddr parameter, and stuffed into the domain.  The domain's boundp
 * field will be set TRUE.  The first responding server (or the first one
 * which is on a reserved port) will be the bound server for the domain.
 */
/* ARGSUSED */
bool_t
ypbind_broadcast_ack(void *ptrue,
		     struct sockaddr_in *saddr)
{
	/* if we should be running secure and the server is not using
	 * a reserved port, return FALSE
	 */
	if (secure && (saddr->sin_family != AF_INET ||
	    saddr->sin_port >= IPPORT_RESERVED) ) {
		return (FALSE);
	}
	current_domain->dom_boundp = TRUE;
	current_domain->dom_serv_addr = saddr->sin_addr;
	current_domain->dom_serv_port = saddr->sin_port;
	return(TRUE);
}

/*
 * This checks to see if a server bound to a named domain is still alive and
 * well.
 */
static int				/* TRUE=server ok */
ypbind_ping(struct domain *pdom,
	    bool_t complain)
{
	struct sockaddr_in addr;
	struct timeval timeout;
	struct timeval intertry;

	if (!pdom)
		return FALSE;

	/* Assume broadcaster will figure it all out */
	if (pdom->dom_broadcaster_pid != 0 || pdom->dom_perm)
		return TRUE;

	/* Someone should start broadcasting */
	if (!pdom->dom_boundp)
		return FALSE;

	timeout.tv_sec = PINGTOTTIM;
	timeout.tv_usec = intertry.tv_usec = 0;
	if (pdom->ping_clnt == (CLIENT *)NULL) {
		intertry.tv_sec = PINGINTERTRY;
		bzero(&addr, sizeof(addr));
		addr.sin_addr = pdom->dom_serv_addr;
		addr.sin_family = AF_INET;
		addr.sin_port = pdom->dom_serv_port;
		if ((pdom->ping_clnt = clntudp_bufcreate(&addr, YPPROG,
		    YPVERS, intertry, &ping_sock,
		    RPCSMALLMSGSIZE, RPCSMALLMSGSIZE)) == (CLIENT *)NULL) {
			if (complain)
			     syslog(LOG_ERR, "ypbind_ping %s: %s",
				 inet_ntoa(addr.sin_addr),
				 clnt_spcreateerror("clntudp_create error"));
			return FALSE;
		} else {
			pdom->dom_serv_port = addr.sin_port;
		}
	}
	if (clnt_call(pdom->ping_clnt, YPPROC_NULL, xdr_void, 0,
			xdr_void, 0, timeout) != RPC_SUCCESS) {
		pdom->dom_boundp = FALSE;
		clnt_destroy(pdom->ping_clnt);
		pdom->ping_clnt = (CLIENT *)NULL;
	}

	return pdom->dom_boundp;
}

static struct timeval when_local;
static int num_local = 0;
static union addrs {
	char	buf[1];
	struct ifa_msghdr ifam;
	struct in_addr a[1];
} *addrs;
static size_t addrs_size = 0;

static void
getlocal(void)
{
	int i;
	size_t needed;
	struct timeval now;
	int mib[6];
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam, *ifam_lim, *ifam2;
	struct sockaddr *sa;

	/*
	 * Get current addresses periodically, in case interfaces change.
	 */
	gettimeofday(&now,0);
	if (addrs_size > 0 && now.tv_sec < when_local.tv_sec+60)
		return;
	when_local  = now;
	num_local = 0;

	/*
	 * Fetch the interface list, without too many system calls
	 * since we do it repeatedly.
	 */
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;
	for (;;) {
		if ((needed = addrs_size) != 0) {
			if (sysctl(mib, 6, addrs,&needed, 0, 0) >= 0)
				break;

			if (errno != ENOMEM && errno != EFAULT) {
				syslog(LOG_ERR, "sysctl: %m");
				exit(1);
			}
			free(addrs);
			needed = 0;
		}
		if (sysctl(mib, 6, 0, &needed, 0, 0) < 0) {
			syslog(LOG_ERR, "sysctl-estimate: %m");
			exit(1);
		}
		addrs = (union addrs *)malloc(addrs_size = needed);
	}

	ifam_lim = (struct ifa_msghdr *)(addrs->buf + needed);
	for (ifam = &addrs->ifam; ifam < ifam_lim; ifam = ifam2) {
		ifam2 = (struct ifa_msghdr*)((char*)ifam + ifam->ifam_msglen);

		if (ifam->ifam_type == RTM_IFINFO) {
			ifm = (struct if_msghdr *)ifam;
			continue;
		}
		if (ifam->ifam_type != RTM_NEWADDR) {
			syslog(LOG_ERR, "out of sync");
			abort();
		}

		if (!(ifm->ifm_flags & IFF_UP))
			continue;

		/*
		 * Find the interface address among the other addresses.
		 */
		sa = (struct sockaddr *)(ifam+1);
		for (i = 0;
		     i <= RTAX_IFA && sa < (struct sockaddr *)ifam2;
		     i++) {
			if ((ifam->ifam_addrs & (1 << i)) == 0)
				continue;
			if (i == RTAX_IFA)
				break;
#define ROUNDUP(a) ((a) > 0 ? (1 + (((a) - 1) | (sizeof(__uint64_t) - 1))) \
		    : sizeof(__uint64_t))
#ifdef _HAVE_SA_LEN
			sa = (struct sockaddr *)((char*)sa
						 + ROUNDUP(sa->sa_len));
#else
			sa = (struct sockaddr *
			      )((char*)sa + ROUNDUP(_FAKE_SA_LEN_DST(sa)));
#endif
		}
		if (i > RTAX_IFA
#ifdef _HAVE_SA_LEN
		    || sa->sa_len == 0
#endif
		    || sa >= (struct sockaddr *)ifam2
		    || sa->sa_family != AF_INET)
			continue;

		addrs->a[num_local++] = ((struct sockaddr_in *)sa)->sin_addr;
	}
}


static bool_t
chklocal(struct in_addr addr)
{
	int i;

	if (addr.s_addr == INADDR_LOOPBACK)
		return (TRUE);

	getlocal();
	for (i = 0; i < num_local; i++) {
		if (addr.s_addr == addrs->a[i].s_addr)
			return (TRUE);
	}
	return (FALSE);
}
