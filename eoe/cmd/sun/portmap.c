/*
 * Copyright 1991 by Silicon Graphics, Inc.
 */
/*
 * Copyright (c) 1984,1990 by Sun Microsystems, Inc.
 */

/*
 * portmap.c, Implements the program,version to port number mapping for
 * rpc.
 */

#define _BSD_SIGNALS

#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>		/* for ntohx() */
#include <net/if.h>		/* to find local addresses */
#include <net/route.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <sys/sysctl.h>
#include <fcntl.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <cap_net.h>
#include <fmtmsg.h>

#ifndef PORTMAP_DEBUG
/* an extremely ugly hack, but it works */
# define perror(string)	syslog(LOG_DEBUG, "%s: %m", string)
# define fprintf	syslog
# undef stderr
# define stderr		LOG_DEBUG
#endif /* PORTMAP_DEBUG */

static void reap();
static void reg_service(struct svc_req *, SVCXPRT *);
static void callit(struct svc_req *, SVCXPRT *);
static bool_t chklocal(struct in_addr addr);
static void getlocal(void);
struct pmaplist *pmaplist;
static int debugging;
static int verbose;			/* Catch all errors */

static struct call_table {
	pid_t	pid;
} *call_table, *reap_table;
static volatile int reap_count;
static int fork_cnt;
static int max_forks = 30;

static int do_mcast;
static int mcast_sock = -1;
static int udp_sock;
static int tcp_fd = -1;	/* For checking whether TCP service is up */
static int udp_fd = -1;	/* For checking whether UDP service is up */

static SVCXPRT *ludp_xprt, *ltcp_xprt;  /* To check for loopback connections */

/*
 * Simple access control to restrict any request for useful information
 * to a limited set of addresses.  The first match in the array of
 * mask-match pairs allows the request to be processed. Any address
 * for this host is always allowed.
 */
#define MAX_OKNETS 50
static struct oknet {
	struct in_addr	mask, match;
} oknets[MAX_OKNETS];
static int num_oknets;
static int Aflag;
static int bflag;
static int Secure = 1;

static struct timeval when_local;
static int num_local = 0;
static union addrs {
	char	buf[1];
	struct ifa_msghdr ifam;
	struct oknet a[1];
} *addrs;
static size_t addrs_size = 0;

extern int _using_syslog;
extern int __svc_label_agile;

static void moredebug();
static void lessdebug();

/*
 * Return 1 if the address is accepted, 0 otherwise.
 */
static int
chknet(struct in_addr addr)
{
	register struct oknet *n;
	register int i;

	for (n = oknets, i = 0; i < num_oknets; n++, i++) {
		if ((addr.s_addr & n->mask.s_addr) == n->match.s_addr)
			return 1;
	}

	/*
	 * If it was not in the explicit list and if allowed, check
	 * the implicit list.
	 */
	if (Aflag) {
		getlocal();
		for (i = num_local, n = &addrs->a[0]; i != 0; i--, n++) {
			if (0 == ((addr.s_addr ^ n->mask.s_addr)
				  & n->match.s_addr))
				return 1;
		}
	}

	return 0;
}

main(argc,argv)
	int argc;
	char *argv[];
{
	SVCXPRT *xprt;
	int sock, lsock, pid, t;
	struct sockaddr_in addr, laddr;
	int len = sizeof(struct sockaddr_in);
	struct ip_mreq mreq;
	register struct pmaplist *pml;
	int	on = 1, argerr = 0;

	openlog(argv[0], LOG_PID|LOG_NOWAIT, LOG_DAEMON);
	_using_syslog = 1;
	__svc_label_agile = (sysconf(_SC_IP_SECOPTS) > 0);

	opterr = 0;
	while ((t = getopt(argc, argv, "a:AbCdf:mv")) != EOF) {
		switch (t) {
		case 'a': {
			char *cp;
			struct oknet n;

			/*
			 * Option formats: "mask,match" or "network", where
			 * mask, match and network are valid IP address/network
			 * numbers. "network" is a shorthand for specifying the
			 * default mask and match appropriate for the
			 * network's address class.
			 */

			if (cp = strchr(optarg, ',')) {
				*cp++ = '\0';
				if (!inet_isaddr(optarg, &n.mask.s_addr)) {
					fprintf(stderr,
					    "%s: illegal IP address for mask\n",
					    optarg);
					argerr = 1;
					break;
				}
				if (!inet_isaddr(cp, &n.match.s_addr)) {
					fprintf(stderr,
					   "%s: illegal IP address for match\n",
					    cp);
					argerr = 1;
					break;
				}
			} else {
				/*
				 * Treat arg as a network address,
				 * host part of address is ignored.
				 */
				if (!inet_isaddr(optarg, &n.match.s_addr)) {
					fprintf(stderr,
					    "%s: illegal network address\n",
					    optarg);
					argerr = 1;
					break;
				}
				if (IN_CLASSA(n.match.s_addr))
					n.mask.s_addr = IN_CLASSA_NET;
				else if (IN_CLASSB(n.match.s_addr))
					n.mask.s_addr = IN_CLASSB_NET;
				else if (IN_CLASSC(n.match.s_addr))
					n.mask.s_addr = IN_CLASSC_NET;
				else {
					fprintf(stderr,
					  "%s: illegal network address class\n",
					    optarg);
					argerr = 1;
					break;
				}
				n.match.s_addr &= n.mask.s_addr;
			}
			if (num_oknets < MAX_OKNETS)
				oknets[num_oknets++] = n;
			else
				fprintf(stderr,
					"too many nets, extra ignored\n");
			break;
		}

		case 'A':
			/*
			 * Trust all directly connected networks.
			 */
			Aflag = 1;
			break;

		case 'b':
			/*
			 * Trust non-multicast including broadcast sources
			 * because we are behind a firewall.
			 */
			bflag = 1;
			break;

		case 'f': {
			int max = atoi(optarg);
			if (max < 1 || max > 1024) {
				fprintf(stderr,
				"%s: illegal value for fork limit, ignored\n",
				    optarg);
				argerr = 1;
			} else
				max_forks = max;
			break;
		}

		case 'm':
			do_mcast = 1;
			break;

		case 'd':
			debugging = 1;
			/* fall thru */
		case 'v':
			verbose = 1;
			break;

		case 'C':
			Secure = 0;
			(void) fmtmsg(MM_CONSOLE, "portmap", MM_WARNING, "The -C option provides backward compatibility for broken applications.  It also exposes a widely known security problem.", NULL, NULL);
			break;

		default:
		case '?':
			fprintf(stderr,"unknown option: %s\n", argv[optind -1]);
			argerr = 1;
			break;
		}
	}
	if (argerr) {
		/* -d option just for testing, don't show it */
		fprintf(stderr,
"usage: portmap [-vmAbC] [-f forklimit] [-a mask,match | -a match]\n");
	}

#ifndef PORTMAP_DEBUG
	_daemonize(0, -1, -1, -1);
	openlog(argv[0], LOG_PID|LOG_NOWAIT, LOG_DAEMON);
#endif

	call_table = (struct call_table *)malloc(sizeof(*call_table)
						 *max_forks);
	bzero(call_table, sizeof(*call_table)*max_forks);
	reap_table = (struct call_table *)malloc(sizeof(*call_table)
						 *max_forks);
	bzero(reap_table, sizeof(*call_table)*max_forks);

	if ((udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("portmap cannot create UDP socket");
		exit(1);
	}
	/*
	 * We use SO_REUSEADDR because there is a possibility of kernel
	 * disallowing the use of the same bind address for some time even
	 * after that corresponding socket has been closed.  In the case
	 * of TCP, some data transfer may still be pending.
	 */
	(void) setsockopt(udp_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);

	bzero(&addr, sizeof(addr));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PMAPPORT);
	if (cap_bind(udp_sock, (struct sockaddr *)&addr, len) != 0) {
		perror("portmap cannot bind");
		exit(1);
	}

	if ((xprt = svcudp_create(udp_sock)) == (SVCXPRT *)NULL) {
		fprintf(stderr, "couldn't do udp_create\n");
		exit(1);
	}

	if (Secure) {
	/* create a socket bound to the loopback interface so we can
	   definitively tell connections that originated from this machine */

		if ((lsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
			perror("portmap cannot create UDP socket");
			exit(1);
		}
		(void) setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &on, 
				  sizeof on);

		bzero(&laddr, sizeof(laddr));
		laddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		laddr.sin_family = AF_INET;
		laddr.sin_port = htons(PMAPPORT);

		if (cap_bind(lsock, (struct sockaddr *)&laddr, len) != 0) {
			perror("portmap cannot bind");
			exit(1);
		}

		if ((ludp_xprt = svcudp_create(lsock)) == (SVCXPRT *)NULL) {
			fprintf(stderr, "couldn't do local udp_create\n");
			exit(1);
		}
	}


	/* make an entry for ourself */
	pml = (struct pmaplist *)malloc((u_int)sizeof(struct pmaplist));
	pml->pml_next = 0;
	pml->pml_map.pm_prog = PMAPPROG;
	pml->pml_map.pm_vers = PMAPVERS;
	pml->pml_map.pm_prot = IPPROTO_UDP;
	pml->pml_map.pm_port = PMAPPORT;
	pmaplist = pml;

	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		perror("portmap cannot create socket");
		exit(1);
	}
	(void) setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
				(char *)&on, sizeof on);
	if (cap_bind(sock, (struct sockaddr *)&addr, len) != 0) {
		perror("portmap cannot bind");
		exit(1);
	}
	if ((xprt = svctcp_create(sock, RPCSMALLMSGSIZE, RPCSMALLMSGSIZE))
	    == (SVCXPRT *)NULL) {
		fprintf(stderr, "couldn't do tcp_create\n");
		exit(1);
	}

	if (Secure) {
	/* create a socket bound to the loopback interface so we can
	   definitively tell connections that originated from this machine */

		if ((lsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
			perror("portmap cannot create socket");
			exit(1);
		}
		(void) setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR,
				  (char *)&on, sizeof on);
		if (cap_bind(lsock, (struct sockaddr *)&laddr, len) != 0) {
			perror("portmap cannot bind");
			exit(1);
		}
		if ((ltcp_xprt = svctcp_create(lsock, RPCSMALLMSGSIZE, 
					       RPCSMALLMSGSIZE))
		    == (SVCXPRT *)NULL) {
			fprintf(stderr, "couldn't do tcp_create\n");
			exit(1);
		}
	}

	/* make an entry for ourself */
	pml = (struct pmaplist *)malloc((u_int)sizeof(struct pmaplist));
	pml->pml_map.pm_prog = PMAPPROG;
	pml->pml_map.pm_vers = PMAPVERS;
	pml->pml_map.pm_prot = IPPROTO_TCP;
	pml->pml_map.pm_port = PMAPPORT;
	pml->pml_next = pmaplist;
	pmaplist = pml;

	/*
	 * Create multicast socket.
	 * Use a separate socket to detect bad guys sending with a
	 * forged local source address.
	 */
	if (do_mcast) {
		if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
			perror("portmap cannot create multicast socket");
			exit(1);
		}
		(void) setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
				  &on, sizeof on);

		addr.sin_addr.s_addr = htonl(PMAP_MULTICAST_INADDR);
		addr.sin_family = AF_INET;
		addr.sin_port = htons(PMAPPORT);
		mreq.imr_multiaddr.s_addr = htonl(PMAP_MULTICAST_INADDR);
		mreq.imr_interface.s_addr = htonl(INADDR_ANY);
		if (cap_bind(sock, (struct sockaddr *)&addr, len) != 0) {
			perror("portmap cannot bind multcast socket");
		} else if (setsockopt(sock, IPPROTO_IP,
				      IP_ADD_MEMBERSHIP, &mreq, sizeof mreq)) {
			perror("cannot enable multicast reception");
		} else if ((xprt = svcudp_create(sock)) == 0) {
			fprintf(stderr, "couldn't do multicast udp_create\n");
			(void)close(sock);
		} else {
			mcast_sock = sock;
		}
	}

	(void)svc_register(xprt, PMAPPROG, PMAPVERS, reg_service, FALSE);

	/*
	 * sockets send this on write after disconnect --
	 * live and learn from SVR4 work
	 */
	(void)signal(SIGPIPE, SIG_IGN);
	(void)signal(SIGHUP, SIG_IGN);
	(void)signal(SIGCHLD, reap);
	(void)signal(SIGUSR1, moredebug);
	(void)signal(SIGUSR2, lessdebug);

	svc_run();
	fprintf(stderr, "svc_run returned unexpectedly\n");
	abort();
	/*NOTREACHED*/
}

/*
 * This routine is called to make sure that the given port number is
 * still bound.  This is helpful in those cases where the server dies
 * and the client gets the wrong information.  It is better if the
 * client about this fact at create time itself.  If the given port
 * number is not bound, we return FALSE.
 */
static bool_t
is_bound(pml)
	struct pmaplist *pml;
{
	int fd;
	struct sockaddr_in myaddr;

	if (pml->pml_map.pm_prot == IPPROTO_TCP) {
		if (tcp_fd < 0)
			tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
		fd = tcp_fd;
	} else if (pml->pml_map.pm_prot == IPPROTO_UDP) {
		if (udp_fd < 0)
			udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
		fd = udp_fd;
	} else
		return (TRUE);
	if (fd < 0)
		return (TRUE);
	bzero((char *)&myaddr, sizeof (myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons(pml->pml_map.pm_port);
	if (cap_bind(fd, (struct sockaddr *)&myaddr, sizeof (myaddr)) < 0)
		return (TRUE);
	if (pml->pml_map.pm_prot == IPPROTO_TCP) {
		/* XXX: Too bad that there is no unbind(). */
		(void) close(tcp_fd);
		tcp_fd = -1;
	} else {
		(void) close(udp_fd);
		udp_fd = -1;
	}
	return (FALSE);
}


static void
moredebug()
{
	if (!debugging) {
		debugging = 1;
		syslog(LOG_DEBUG, "debugging=1");
	} else {
		verbose = 1;
		syslog(LOG_DEBUG, "verbose=1");
	}
}



static void
lessdebug()
{
	if (verbose) {
		verbose = 0;
		syslog(LOG_DEBUG, "verbose=0");
	} else {
		debugging = 0;
		syslog(LOG_DEBUG, "debugging=0");
	}
}


/*
 * Finds the service for prog, vers.  Even if the specified vers
 * is not registered, we still return the registeration for that
 * program number, with the hope that the client will figure out
 * the actual available version number on doing a clnt_call() via
 * the RPC_PROGVERSMISMTACH.
 */
static struct pmaplist *
find_service(prog, vers, prot)
	u_long		prog;
	u_long		vers;
	u_long		prot;
{
	register struct pmaplist *hit = NULL;
	register struct pmaplist *hitp = NULL;
	register struct pmaplist *pml;
	register struct pmaplist *prevpml;

	for (prevpml = NULL, pml = pmaplist; pml != NULL; pml = pml->pml_next) {
		if ((pml->pml_map.pm_prog != prog) ||
		    (pml->pml_map.pm_prot != prot)) {
			prevpml = pml;
			continue;
		}
		hit = pml;
		hitp = prevpml;
		if (pml->pml_map.pm_vers == vers)
			break;
		prevpml = pml;
	}
	if (hit == NULL)
		return (NULL);

	/* Make sure it is bound */
	if (is_bound(hit))
		return (hit);

	/* unhook it from the list */
	if (verbose)
		fprintf(stderr,
			"service failed for prog %u, vers %u, prot %u\n",
			hit->pml_map.pm_prog, hit->pml_map.pm_vers,
			hit->pml_map.pm_prot);

	pml = hit->pml_next;
	if (hitp == NULL)
		pmaplist = pml;
	else
		hitp->pml_next = pml;
	free((caddr_t) hit);

	/*
	 * Most probably other versions and transports for this
	 * program number were also registered by this same
	 * service, so we should check their bindings also.
	 * This may slow down the response time a bit.
	 */
	for (prevpml = NULL, pml = pmaplist; pml != NULL;) {
		if (pml->pml_map.pm_prog != prog) {
			prevpml = pml;
			pml = pml->pml_next;
			continue;
		}
		/* Make sure it is bound */
		if (is_bound(pml)) {
			prevpml = pml;
			pml = pml->pml_next;
			continue;
		} else {
			/* unhook it */
			hit = pml;
			pml = hit->pml_next;
			if (prevpml == NULL)
				pmaplist = pml;
			else
				prevpml->pml_next = pml;
			free((caddr_t) hit);
		}
	}
	return (NULL);
}


/*
 * 1 OK, 0 not
 */
static void
reg_service(struct svc_req *rqstp, SVCXPRT *xprt)
{
	struct pmap	reg;
	struct pmaplist *pml, *prevpml, *fnd;
	int		ans, local;
	u_long		port;
	caddr_t		t;
	struct sockaddr_in *who;

	who = svc_getcaller(xprt);

	if (debugging)
		printf("%s: proc %u\n",
			inet_ntoa(who->sin_addr), rqstp->rq_proc);

	local = chklocal(who->sin_addr);

	if (xprt->xp_sock == mcast_sock) {
		/*
		 * Ignore (supposedly) local requests that arrived via
		 * multicast in case they come from a bad guy on the Internet
		 * sending poison packets to the universe.
		 */
		if (local) {
			if (verbose)
				syslog(LOG_INFO|LOG_AUTH,
				       "ignore multicast prog %u proc %u call"
				       " from local %s",
				       rqstp->rq_prog, rqstp->rq_proc,
				       inet_ntoa(who->sin_addr));
			return;
		}
		/*
		 * Ignore unauthorized all multicasts to avoid melting
		 * multicast forwarders with rejections.
		 */
		if ((num_oknets > 0 || Aflag) &&
		    !chknet(who->sin_addr)) {
			if (verbose)
				syslog(LOG_INFO|LOG_AUTH,
				       "ignore multicast prog %u proc %u call"
				       " from %s",
				       rqstp->rq_prog, rqstp->rq_proc,
				       inet_ntoa(who->sin_addr));
			return;
		}

	} else {
		/*
		 * Allow unicast or broadcast "null" procedure requests from
		 * anybody since they return no port information.
		 */
		if ((num_oknets > 0 || Aflag) && !bflag &&
		    !local && rqstp->rq_proc != PMAPPROC_NULL &&
		    !chknet(who->sin_addr)) {
			if (verbose)
				syslog(LOG_INFO|LOG_AUTH,
				       "rejected prog %u proc %u call from %s",
				       rqstp->rq_prog, rqstp->rq_proc,
				       inet_ntoa(who->sin_addr));
			svcerr_auth(xprt, AUTH_FAILED);
			return;
		}
	}

	switch (rqstp->rq_proc) {

	case PMAPPROC_NULL:
		/*
		 * Null proc call
		 */
		if ((!svc_sendreply(xprt, xdr_void, (char *)NULL)) &&
			debugging) {
			fprintf(stderr, "svc_sendreply\n");
			abort();
		}
		break;

	case PMAPPROC_SET:
		/*
		 * Set a program, version to port mapping
		 */
		if (!svc_getargs(xprt, xdr_pmap, &reg)) {
			svcerr_decode(xprt);
			return;
		}
		/* Be a little more restrictive here and require
		   that users come in using the loopback interface so
		   they cannot set from a remote host using a packet
		   with a spoofed loopback addr */
		if ((Secure && xprt != ltcp_xprt && xprt != ludp_xprt) ||
		    (!Secure && !local)) {
			if (verbose)
				syslog(LOG_INFO|LOG_AUTH,
				       "attempt to set"
				       " (prog %u vers %u port %u)"
				       " from host %s port %u",
				       reg.pm_prog, reg.pm_vers, reg.pm_port,
				       inet_ntoa(who->sin_addr),
				       ntohs(who->sin_port));
			ans = 0;
			goto done_set;
		}
		/*
		 * check to see if already used find_service returns
		 * a hit even if the versions don't match, so check
		 * for it
		 */
		fnd = find_service(reg.pm_prog, reg.pm_vers, reg.pm_prot);
		if (fnd && fnd->pml_map.pm_vers == reg.pm_vers) {
			if (fnd->pml_map.pm_port == reg.pm_port) {
				ans = 1;
				goto done_set;
			} else {
				/* Caller should have done UNSET first */
				ans = 0;
				goto done_set;
			}
		} else {
			if ((reg.pm_port < IPPORT_RESERVED) &&
			    (ntohs(who->sin_port) >= IPPORT_RESERVED)) {
				if (verbose)
					syslog(LOG_INFO|LOG_AUTH,
					       "attempt to set"
					       " (prog %u vers %u reserved"
					       " port %u) from port %u",
					       reg.pm_prog,
					       reg.pm_vers,
					       reg.pm_port,
					       ntohs(who->sin_port));
				ans = 0;
				goto done_set;
			}
			/*
			 * add to END of list
			 */
			pml = (struct pmaplist *)
				malloc((u_int) sizeof (struct pmaplist));
			if (pml == 0) {
				perror("malloc failed in set");
				svcerr_systemerr(xprt);
				if (debugging)
					abort();
				return;
			}
			pml->pml_map = reg;
			pml->pml_next = 0;
			if (pmaplist == 0) {
				pmaplist = pml;
			} else {
				for (fnd = pmaplist; fnd->pml_next != 0;
					fnd = fnd->pml_next);
				fnd->pml_next = pml;
			}
			ans = 1;
		}
	done_set:
		if ((!svc_sendreply(xprt, xdr_long, (caddr_t) &ans)) &&
		    debugging) {
			fprintf(stderr, "svc_sendreply\n");
			abort();
		}
		break;

	case PMAPPROC_UNSET:
		/*
		 * Remove a program, version to port mapping.
		 */
		if (!svc_getargs(xprt, xdr_pmap, &reg)) {
			svcerr_decode(xprt);
			return;
		}
		ans = 0;
		/* Be a little more restrictive here and require
		   that users come in using the loopback interface so
		   they cannot unset from a remote host using a packet
		   with a spoofed loopback addr */
		if ((Secure && xprt != ltcp_xprt && xprt != ludp_xprt) ||
		    (!Secure && !local)) {
			if (verbose)
				syslog(LOG_INFO|LOG_AUTH,
				       "attempt to unset (prog %u vers %u)"
				       " from host %s port %u",
				       reg.pm_prog, reg.pm_vers,
				       inet_ntoa(who->sin_addr),
				       ntohs(who->sin_port));
			goto done_unset;
		}
		for (prevpml = NULL, pml = pmaplist; pml != NULL;) {
			if ((pml->pml_map.pm_prog != reg.pm_prog) ||
			    (pml->pml_map.pm_vers != reg.pm_vers)) {
				/* both pml & prevpml move forwards */
				prevpml = pml;
				pml = pml->pml_next;
				continue;
			}
			if ((pml->pml_map.pm_port < IPPORT_RESERVED) &&
			    (ntohs(who->sin_port) >= IPPORT_RESERVED)) {
				if (verbose)
				syslog(LOG_INFO|LOG_AUTH,
				       "attempt to unset (prog %u vers %u"
				       " reserved port %u) port from %u",
				       pml->pml_map.pm_prog,
				       pml->pml_map.pm_vers,
				       pml->pml_map.pm_port,
				       ntohs(who->sin_port));
				goto done_unset;
			}
			/* found it; pml moves forward, prevpml stays */
			ans = 1;
			t = (caddr_t) pml;
			pml = pml->pml_next;
			if (prevpml == NULL)
				pmaplist = pml;
			else
				prevpml->pml_next = pml;
			free(t);
		}
	done_unset:
		if ((!svc_sendreply(xprt, xdr_long, (caddr_t) &ans)) &&
			debugging) {
			fprintf(stderr, "svc_sendreply\n");
			abort();
		}
		break;

	case PMAPPROC_GETPORT:
		/*
		 * Lookup the mapping for a program, version and
		 * return its port
		 */
		if (!svc_getargs(xprt, xdr_pmap, &reg)) {
			svcerr_decode(xprt);
			return;
		}
		fnd = find_service(reg.pm_prog, reg.pm_vers, reg.pm_prot);
		if (fnd)
			port = fnd->pml_map.pm_port;
		else
			port = 0;
		if ((!svc_sendreply(xprt, xdr_long, (caddr_t) &port)) &&
			debugging) {
			fprintf(stderr, "svc_sendreply\n");
			abort();
		}
		break;

	case PMAPPROC_DUMP:
		/*
		 * Return the current set of mapped program, version
		 */
		if ((!svc_sendreply(xprt, xdr_pmaplist,
			(caddr_t) &pmaplist)) && debugging) {
			fprintf(stderr, "svc_sendreply\n");
			abort();
		}
		break;

	case PMAPPROC_CALLIT:
		/*
		 * Calls a procedure on the local machine.  If the requested
		 * procedure is not registered this procedure does not return
		 * error information!! This procedure is only supported on
		 * rpc/udp and calls via rpc/udp.  It passes null
		 * authentication parameters.
		 */
		callit(rqstp, xprt);
		break;

	default:
		svcerr_noproc(xprt);
		break;
	}
}


/*
 * Stuff for the rmtcall service
 */
#define ARGSIZE 9000

struct encap_parms {
	u_int arglen;
	char *args;
};

static bool_t
xdr_encap_parms(xdrs, epp)
	XDR *xdrs;
	struct encap_parms *epp;
{

	return (xdr_bytes(xdrs, &(epp->args), &(epp->arglen), ARGSIZE));
}

struct rmtcallargs {
	u_long	rmt_prog;
	u_long	rmt_vers;
	u_long	rmt_port;
	u_long	rmt_proc;
	struct encap_parms rmt_args;
};

static bool_t
xdr_rmtcall_args(xdrs, cap)
	register XDR *xdrs;
	register struct rmtcallargs *cap;
{

	/* does not get a port number */
	if (xdr_u_long(xdrs, &(cap->rmt_prog)) &&
	    xdr_u_long(xdrs, &(cap->rmt_vers)) &&
	    xdr_u_long(xdrs, &(cap->rmt_proc))) {
		return (xdr_encap_parms(xdrs, &(cap->rmt_args)));
	}
	return (FALSE);
}

static bool_t
xdr_rmtcall_result(xdrs, cap)
	register XDR *xdrs;
	register struct rmtcallargs *cap;
{
	if (xdr_u_long(xdrs, &(cap->rmt_port)))
		return (xdr_encap_parms(xdrs, &(cap->rmt_args)));
	return (FALSE);
}

/*
 * only worries about the struct encap_parms part of struct rmtcallargs.
 * The arglen must already be set!!
 */
static bool_t
xdr_opaque_parms(xdrs, cap)
	XDR *xdrs;
	struct rmtcallargs *cap;
{

	return (xdr_opaque(xdrs, cap->rmt_args.args, cap->rmt_args.arglen));
}

/*
 * This routine finds and sets the length of incoming opaque paraters
 * and then calls xdr_opaque_parms.
 */
static bool_t
xdr_len_opaque_parms(xdrs, cap)
	register XDR *xdrs;
	struct rmtcallargs *cap;
{
	register u_int beginpos, lowpos, highpos, currpos, pos;

	beginpos = lowpos = pos = xdr_getpos(xdrs);
	highpos = lowpos + ARGSIZE;
	while ((int)(highpos - lowpos) >= 0) {
		currpos = (lowpos + highpos) / 2;
		if (xdr_setpos(xdrs, currpos)) {
			pos = currpos;
			lowpos = currpos + 1;
		} else {
			highpos = currpos - 1;
		}
	}
	xdr_setpos(xdrs, beginpos);
	cap->rmt_args.arglen = pos - beginpos;
	return (xdr_opaque_parms(xdrs, cap));
}


/*
 * Call a remote procedure service
 * This procedure is very quiet when things go wrong.
 * The proc is written to support broadcast rpc.  In the broadcast case,
 * a machine should shut-up instead of complain, less the requestor be
 * overrun with complaints at the expense of not hearing a valid reply ...
 *
 * This now forks so that the program & process that it calls can call
 * back to the portmapper.
 */
static void
callit(struct svc_req *rqstp,
       SVCXPRT *xprt)
{
	struct rmtcallargs a;
	struct pmaplist *pml;
	u_short port;
	struct sockaddr_in me;
	pid_t pid;
	int free_slot;
	int sock = -1;
	CLIENT *client;
	struct authunix_parms *au = (struct authunix_parms *)rqstp->rq_clntcred;
	struct timeval timeout;
	char buf[ARGSIZE];
	int prev_mask;

	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	a.rmt_args.args = buf;
	if (!svc_getargs(xprt, xdr_rmtcall_args, &a))
	    return;
	if ((pml = find_service(a.rmt_prog, a.rmt_vers, IPPROTO_UDP)) == NULL)
	    return;
	if (debugging) {
		syslog(LOG_DEBUG,
		       "broadcast or multicast prog %u, proc %u from %s.%u",
		       a.rmt_prog, a.rmt_proc,
		       inet_ntoa(svc_getcaller(xprt)->sin_addr),
		       svc_getcaller(xprt)->sin_port);
	}

	/*
	 * Fork a child to do the work.  Parent immediately returns.
	 * Child exits upon completion.
	 *
	 * First deal with finished children.  Hold off SIGCHLD
	 * while we are updating call_table data structure and 
	 * potentially killing a process to avoid race with SIGCHLD
	 * handler.
	 */
	prev_mask = sigblock(sigmask(SIGCHLD));

	free_slot = max_forks;
	while (reap_count > 0) {
	        --fork_cnt;
	        pid = reap_table[--reap_count].pid;
                for (free_slot = 0; free_slot < max_forks; free_slot++) {
                        if (call_table[free_slot].pid == pid) {
			        call_table[free_slot].pid = 0;
			        break;
			}
		}
		if (free_slot >= max_forks)
			syslog(LOG_DEBUG, "PID %d not found in table", pid);
	}

	if (fork_cnt >= max_forks) {
		/*
		 * Too much going on already.
		 *
		 * It is too expensive for ordinary portmap calls to
		 * lock up for an indirect call.  It is better to drop
		 * the new request.  But it is better still to try to keep
		 * a bad system from locking everything up.
		 *
		 * It would be possible to zap the oldest child process,
		 * but that policy can result in making no progress.
		 * The oldest child is the one most likely to finally finish.
		 */
		free_slot = rand()%(max_forks+1);
		/*
		 * Include the new request among those subject to random-drop.
		 */
		if (free_slot != max_forks) {
			pid = call_table[free_slot].pid;
			if (pid == 0) {
				syslog(LOG_ERR,"PID table slot %d empty",
				       free_slot);
			} else {
				if (debugging)
					syslog(LOG_DEBUG,"abort PID %d",pid);
				(void)kill(pid, SIGKILL);
			}
		}
		(void)sigsetmask(prev_mask);
		return;
	}

	/* 
	 * We can now unblock SIGCHLD since the race condition with
	 * the kill processing above has passed.  Unblocking SIGCHLD
	 * earlier could result in a newly created process 
	 * (with the same pid as a process in the reap_table) mistakenly 
	 * being killed due to IRIX's reuse of pids.
	 */
	(void)sigsetmask(prev_mask);

	if (free_slot >= max_forks) {
		free_slot = 0;
		for (;;) {
			if (call_table[free_slot].pid == 0)
				break;
			if (++free_slot >= max_forks) {
				syslog(LOG_ERR,
				       "no free PID slots; fork_cnt=%d",
				       fork_cnt);
				return;
			}
		}
	}

	pid = fork();			/* background the request */
	if (pid > 0) {
		fork_cnt++;
		call_table[free_slot].pid = pid;
		return;
	} else if (pid < 0) {
		syslog(LOG_ERR, "callit() fork(): %m");
		return;
	}

	(void)signal(SIGUSR1, SIG_IGN);
	(void)signal(SIGUSR2, SIG_IGN);

	port = pml->pml_map.pm_port;
	get_myaddress(&me);
	me.sin_port = htons(port);
	client = clntudp_create(&me, a.rmt_prog, a.rmt_vers, timeout, &sock);
	if (client != (CLIENT *)NULL) {
		if (rqstp->rq_cred.oa_flavor == AUTH_UNIX) {
			client->cl_auth = authunix_create(au->aup_machname,
			   au->aup_uid, au->aup_gid, au->aup_len, au->aup_gids);
			if (client->cl_auth == (AUTH *)NULL)
			    goto bail;
		}
		a.rmt_port = (u_long)port;
		if (clnt_call(client, a.rmt_proc, xdr_opaque_parms, &a,
		    xdr_len_opaque_parms, &a, timeout) == RPC_SUCCESS) {
			/* cannot send from multicast socket */
			if (xprt->xp_sock == mcast_sock)
				xprt->xp_sock = udp_sock;
			svc_sendreply(xprt, xdr_rmtcall_result, &a);
		}
		AUTH_DESTROY(client->cl_auth);
bail:
		clnt_destroy(client);
	}
	exit(0);
}


static void
getlocal(void)
{
	int i;
	struct oknet *n;
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
				perror("sysctl");
				exit(1);
			}
			free(addrs);
			needed = 0;
		}
		if (sysctl(mib, 6, 0, &needed, 0, 0) < 0) {
			perror("sysctl-estimate");
			exit(1);
		}
		addrs = (union addrs *)malloc(addrs_size = needed);
	}

	n = &addrs->a[0];
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
		n->mask.s_addr = 0xffffffff;
		sa = (struct sockaddr *)(ifam+1);
		for (i = 0;
		     i <= RTAX_IFA && sa < (struct sockaddr *)ifam2;
		     i++) {
			if ((ifam->ifam_addrs & (1 << i)) == 0)
				continue;
			if (i == RTAX_NETMASK
			    && !(ifm->ifm_flags & IFF_POINTOPOINT))
				n->mask = ((struct sockaddr_in *)sa)->sin_addr;
			else if (i == RTAX_IFA)
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

		n->match = ((struct sockaddr_in *)sa)->sin_addr;
		n++;
		num_local++;
	}

	if (debugging)
		fprintf(stderr, "%d local addresses detected\n",
			num_local);
}


static bool_t
chklocal(struct in_addr addr)
{
	int i;
	struct oknet *n;

	if (addr.s_addr == INADDR_LOOPBACK)
		return (TRUE);

	getlocal();
	for (i = num_local, n = &addrs->a[0]; i != 0; i--, n++) {
		if (addr.s_addr == n->match.s_addr)
			return (TRUE);
	}
	return (FALSE);
}

/*
 * Call wait3() to clean up child processes until it returns an error.
 * Make sure to save and restore the value of errno so that we don't
 * disturb the code which was interrupted by this signal handler.
 */
static void
reap(void)
{
	int     save_errno;
	pid_t   pid;

	save_errno = errno;
	while ((pid=wait3(0, WNOHANG, 0)) > 0) {
		/* it shouldn't ever wrap, but just to be safe */
		if (reap_count < max_forks)	
		        reap_table[reap_count++].pid = pid;
	}
	errno = save_errno;
}
