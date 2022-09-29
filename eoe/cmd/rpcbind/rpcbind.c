/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)rpcbind:rpcbind.c	1.14.9.2"

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*	PROPRIETARY NOTICE (Combined)
*
* This source code is unpublished proprietary information
* constituting, or derived under license from AT&T's UNIX(r) System V.
* In addition, portions of such source code were derived from Berkeley
* 4.3 BSD under license from the Regents of the University of
* California.
*
*
*
*	Copyright Notice 
*
* Notice of copyright on this source code product does not indicate 
*  publication.
*
*	(c) 1986,1987,1988,1989,1990  Sun Microsystems, Inc
*	(c) 1983,1984,1985,1986,1987,1988,1989,1990  AT&T.
*	(c) 1990,1991  UNIX System Laboratories, Inc.
*          All rights reserved.
*/
 
/*
 * rpcbind.c
 * Implements the program, version to address mapping for rpc.
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <rpc/rpc.h>
#include <rpc/rpcb_prot.h>
#include <netconfig.h>
#include <netdir.h>
#include <sys/wait.h>
#include <signal.h>
#include <fmtmsg.h>
#ifdef PORTMAP
#include <netinet/in.h>
#include <rpc/pmap_prot.h>
#endif
#include "rpcbind.h"
#include <sys/termios.h>
#ifdef SYSLOG
#include <sys/syslog.h>
#else
#define	LOG_DAEMON (3<<3)
#define	LOG_CONS 0x02
#define	LOG_ERR 3
#endif /* SYSLOG */
#ifdef sgi
#include <string.h>
/* #include <sys/stream.h> #include <sys/tihdr.h> #include <sys/stropts.h> */
#include <tiuser.h>
#endif

extern void *malloc();
extern char *strdup();

#ifdef PORTMAP
extern void pmap_service();
#endif
extern int rpcb_service();
#ifdef WAIT3
void reap();
#endif
static void terminate();
static void detachfromtty();
static void parseargs();
static int init_transport();
extern void read_warmstart();
extern void write_warmstart();

/* Global variables */
int debugging = 0;	/* Tell me what's going on */
#ifdef sgi
int verbose   = 0;      /* report errors */
int do_mcast  = 0;      /* register for UDP multicasts */
SVCXPRT *m_xprt;
int max_forks = 10;	/* maximum number of callits at once */
#endif
RPCBLIST *list_rbl;	/* A list of version 3 rpcbind services */
char *loopback_dg;	/* Datagram loopback transport, for set and unset */
char *loopback_vc;	/* COTS loopback transport, for set and unset */
char *loopback_vc_ord;	/* COTS_ORD loopback transport, for set and unset */
int Secure = 1;

/* Local Variable */
static int warmstart = 0;	/* Grab a old copy of registrations */

#ifdef PORTMAP
PMAPLIST *list_pml;	/* A list of version 2 rpcbind services */
char *udptrans = "";	/* Name of UDP transport */
char *tcptrans = "";	/* Name of TCP transport */
char *udp_uaddr;	/* Universal UDP address */
char *tcp_uaddr;	/* Universal TCP address */
SVCXPRT *ludp_xprt = NULL;  /* loopback udp transport */
SVCXPRT *ltcp_xprt = NULL;  /* loopback tcp transport */
#endif

static char servname[] = "rpcbind";
static char superuser[] = "superuser";


extern int t_errno;
extern char *t_errlist[];

#ifdef sgi
/*
 * Simple access control to restrict any request for useful information
 * to a limited set of addresses.  The first match in the array of
 * mask-match pairs allows the request to be processed. Any address
 * for this host is always allowed.
 */
#define MAX_OKNETS 50
struct oknet oknets[MAX_OKNETS];
int num_oknets;
int Aflag;
int bflag;


#endif

main(argc, argv)
	int argc;
	char *argv[];
{
	struct netconfig *nconf;
	void *nc_handle;	/* Net config handle */

	openlog("rpcbind", LOG_CONS, LOG_DAEMON);
	parseargs(argc, argv);

	if (geteuid()) { /* This command allowed only to root */
		(void) fprintf(stderr, "must be root to run %s\n", argv[0]);
		exit(1);
	}
#ifdef sgi
	if (!Secure)
		(void) fmtmsg(MM_CONSOLE, "rpcbind", MM_WARNING, "The -C option provides backward compatibility for broken applications.  It also exposes a widely known security problem.", NULL, NULL);
#endif

	nc_handle = setnetconfig(); 	/* open netconfig file */
	if (nc_handle == NULL) {
		syslog(LOG_ERR, "could not read /etc/netconfig");
		exit(1);
	}
	loopback_dg = "";
	loopback_vc = "";
	loopback_vc_ord = "";

	{ struct rlimit rl;
		rl.rlim_cur = 1024;
		rl.rlim_max = 1024;
		(void) setrlimit(RLIMIT_NOFILE, &rl);	/* SCA */
	}
	while (nconf = getnetconfig(nc_handle)) {
		init_transport(nconf);
	}
	endnetconfig(nc_handle);

	if ((loopback_dg[0] == NULL) && (loopback_vc[0] == NULL) &&
		(loopback_vc_ord[0] == NULL)) {
		syslog(LOG_ERR, "could not find loopback transports");
		exit(1);
	}

	(void) signal(SIGCHLD, SIG_IGN); /* XXX see reap below */
	(void) signal(SIGINT, terminate);
	if (warmstart) {
		read_warmstart();
	}
	if (debugging) {
		printf("rpcbind debugging enabled -- will abort on errors!\n");
	} else {
		detachfromtty();
	}
	my_svc_run();
	syslog(LOG_ERR, "svc_run returned unexpectedly");
	rpcbind_abort();
	/* NOTREACHED */
}

#ifdef sgi
int
rpcb_setsockopt(
	int		fd,
	int		level,
	int		optname,
	void		*optval,
	int		optlen)
{
	int retval;
	struct t_optmgmt reqt;
	struct {
		struct opthdr	hdr;
		char 		optbytes[1000];
	} buf;

	buf.hdr.level = level;
	buf.hdr.name  = optname;
	buf.hdr.len   = optlen;

	(void)memcpy(buf.optbytes, optval, optlen);

	reqt.opt.maxlen = reqt.opt.len = sizeof buf.hdr + optlen;
	reqt.opt.buf    = (char *)&buf;
	reqt.flags      = T_NEGOTIATE;

	retval = t_optmgmt(fd, &reqt, &reqt);

	return retval;
}

void
add_mcast( int fd )
{
	struct ip_mreq mreq;

	mreq.imr_multiaddr.s_addr = htonl(PMAP_MULTICAST_INADDR);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if (rpcb_setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
			    &mreq, sizeof mreq)) {
		fprintf(stderr, "cannot enable multicast reception: %s\n",
			t_errlist[t_errno]);
	}
}
#endif

/*
 * Adds the entry into the rpcbind database.
 * If PORTMAP, then for UDP and TCP, it adds the entries for version 2 also
 * Returns 0 if succeeds, else fails
 */
static int
init_transport(nconf)
	struct netconfig *nconf;	/* Transport provider info */
{
	int fd;
	struct t_bind *taddr, *baddr;
	RPCBLIST *rbl;
	SVCXPRT	*my_xprt, *l_xprt;
	struct nd_addrlist *nas;
	struct nd_hostserv hs;
	int status;	/* bound checking ? */

	if ((nconf->nc_semantics != NC_TPI_CLTS) &&
		(nconf->nc_semantics != NC_TPI_COTS) &&
		(nconf->nc_semantics != NC_TPI_COTS_ORD))
		return (1);	/* not my type */

	if (verbose) {
	int i;
	char **s;

	(void) fprintf(stderr, "%s: %d lookup routines :\n",
		nconf->nc_netid, nconf->nc_nlookups);
	for (i = 0, s = nconf->nc_lookups; i < nconf->nc_nlookups; i++, s++)
		fprintf(stderr, "[%d] - %s\n", i, *s);
	}

	if ((fd = t_open(nconf->nc_device, O_RDWR, NULL)) < 0) {
		syslog(LOG_ERR, "%s: cannot open connection: %s",
				nconf->nc_netid, t_errlist[t_errno]);
		return (1);
	}

	taddr = (struct t_bind *)t_alloc(fd, T_BIND, T_ADDR);
	baddr = (struct t_bind *)t_alloc(fd, T_BIND, T_ADDR);
	if ((baddr == NULL) || (taddr == NULL)) {
		syslog(LOG_ERR, "%s: cannot allocate netbuf: %s",
				nconf->nc_netid, t_errlist[t_errno]);
		exit(1);
	}

	/* Get rpcbind's address on this transport */
	hs.h_host = HOST_SELF;
	hs.h_serv = servname;
	if (0 != (status = netdir_getbyname(nconf, &hs, &nas))) {
		if (verbose)
		    (void)fprintf(stderr, 
			"rpcbind : netdir_getbyname returned %d\n", status);
		goto error;
	}

	/* Copy the address */
	taddr->addr.len = nas->n_addrs->len;
	memcpy(taddr->addr.buf, nas->n_addrs->buf, (int)nas->n_addrs->len);

	if (verbose) {
		/* for debugging print out our universal address */
		char *uaddr;

		uaddr = taddr2uaddr(nconf, nas->n_addrs);
		(void) fprintf(stderr, "rpcbind : my address is %s\n", uaddr);
		(void) free(uaddr);
	}

	netdir_free((char *)nas, ND_ADDRLIST);

	if (nconf->nc_semantics == NC_TPI_CLTS)
		taddr->qlen = 0;
	else
		taddr->qlen = 8;	/* should be enough */

	if (t_bind(fd, taddr, baddr) != 0) {
		syslog(LOG_ERR, "%s: cannot bind: %s",
			nconf->nc_netid, t_errlist[t_errno]);
		goto error;
	}

	if (memcmp(taddr->addr.buf, baddr->addr.buf, (int)baddr->addr.len)) {
		syslog(LOG_ERR, "%s: address in use", nconf->nc_netid);
		goto error;
	}

	my_xprt = (SVCXPRT *)svc_tli_create(fd, nconf, baddr, 0, 0);
	if (my_xprt == (SVCXPRT *)NULL) {
		syslog(LOG_ERR, "%s: could not create service",
				nconf->nc_netid);
		goto error;
	}

#ifdef PORTMAP
	/*
	 * Register both the versions for tcp/ip and udp/ip
	 */
	if ((strcmp(nconf->nc_protofmly, NC_INET) == 0) &&
		((strcmp(nconf->nc_proto, NC_TCP) == 0) ||
		(strcmp(nconf->nc_proto, NC_UDP) == 0))) {
		PMAPLIST *pml;

		int lfd, on = 1, tcp = (strcmp(nconf->nc_proto, NC_TCP) == 0);
		char laddr[32];
		struct t_bind *ltaddr, *lbaddr;
		struct netbuf *nb;
		struct in_addr la = { htonl(INADDR_LOOPBACK) };

		if (!svc_register(my_xprt, PMAPPROG, PMAPVERS,
			pmap_service, NULL)) {
			syslog(LOG_ERR, "could not register on %s",
					nconf->nc_netid);
			goto error;
		}
		pml = (PMAPLIST *)malloc((u_int)sizeof (PMAPLIST));
		if (pml == (PMAPLIST *)NULL) {
			syslog(LOG_ERR, "no memory!");
			exit(1);
		}
		pml->pml_map.pm_prog = PMAPPROG;
		pml->pml_map.pm_vers = PMAPVERS;
		pml->pml_map.pm_port = PMAPPORT;
		if (tcp) {
			if (tcptrans[0]) {
				syslog(LOG_ERR,
				"cannot have more than one TCP transport");
				goto error;
			}
			tcptrans = strdup(nconf->nc_netid);
			pml->pml_map.pm_prot = IPPROTO_TCP;

			/* Let's snarf the universal address */
			/* "h1.h2.h3.h4.p1.p2" */
			tcp_uaddr = taddr2uaddr(nconf, &baddr->addr);
		} else {
			if (udptrans[0]) {
				syslog(LOG_ERR,
				"cannot have more than one UDP transport");
				goto error;
			}
			udptrans = strdup(nconf->nc_netid);
			pml->pml_map.pm_prot = IPPROTO_UDP;

			/* Let's snarf the universal address */
			/* "h1.h2.h3.h4.p1.p2" */
			udp_uaddr = taddr2uaddr(nconf, &baddr->addr);
#ifdef sgi
			/*
			 * Listen to multicast.
			 */
			if (do_mcast) {
				int mfd;
				char maddr[15+8+1];
				struct t_bind *mtaddr, *mbaddr;
				struct in_addr ma = {PMAP_MULTICAST_INADDR};
				
				if ((mfd = t_open(nconf->nc_device,
						  O_RDWR, NULL)) < 0) {
					syslog(LOG_ERR,
					       "%s: cannot open multicast"
					       " connection: %s",
					       nconf->nc_netid,
					       t_errlist[t_errno]);
					goto mcast_out;
				}
				mtaddr = (struct t_bind *)t_alloc(mfd, T_BIND,
								  T_ADDR);
				mbaddr = (struct t_bind *)t_alloc(mfd, T_BIND,
								  T_ADDR);
				sprintf(maddr, "%s.%u.%u", inet_ntoa(ma),
					PMAPPORT>>8, PMAPPORT&0xff);
				nb = uaddr2taddr(nconf, maddr);
				memcpy(mtaddr->addr.buf, nb->buf,
				       mtaddr->addr.len = nb->len);
				free(nb);
				(void)rpcb_setsockopt(mfd,SOL_SOCKET,
						      SO_REUSEADDR,
						      &on, sizeof(on));
				add_mcast(mfd);
				if (t_bind(mfd, mtaddr, mbaddr) != 0) {
					syslog(LOG_ERR,
					       "%s: cannot multicast bind: %s",
					       nconf->nc_netid,
					       t_errlist[t_errno]);
					goto mcast_out;
				}
				m_xprt = (SVCXPRT *)svc_tli_create(mfd, nconf,
								   mbaddr,0,0);
				if (m_xprt == 0) {
					syslog(LOG_ERR, "%s: could not create"
					       " mulicast service",
					       nconf->nc_netid);
					goto mcast_out;
				}
				if (!svc_register(m_xprt,PMAPPROG,PMAPVERS,
						  pmap_service, NULL)) {
					syslog(LOG_ERR,
					       "could not register on %s",
					       nconf->nc_netid);
					goto mcast_out;
				}
				/* version 3 registration */
				if (!svc_reg(m_xprt, RPCBPROG, RPCBVERS,
					     rpcb_service, NULL)) {
					syslog(LOG_ERR, "could not multicast"
					       " register %s version 3",
					       nconf->nc_netid);
					goto mcast_out;
				}
				svc_versquiet(m_xprt);
mcast_out:
				(void) t_free((char *)mtaddr, T_BIND);
				(void) t_free((char *)mbaddr, T_BIND);
			}
#endif
		}
		pml->pml_next = list_pml;
		list_pml = pml;

		/* Add version 3 information */
		pml = (PMAPLIST *)malloc((u_int)sizeof (PMAPLIST));
		if (pml == (PMAPLIST *)NULL) {
			syslog(LOG_ERR, "no memory!");
			exit(1);
		}
		pml->pml_map = list_pml->pml_map;
		pml->pml_map.pm_vers = RPCBVERS;
		pml->pml_next = list_pml;
		list_pml = pml;

		/* Also add version 2 stuff to rpcbind list */
		rbl = (RPCBLIST *)malloc((u_int)sizeof (RPCBLIST));
		if (rbl == (RPCBLIST *)NULL) {
			syslog(LOG_ERR, "no memory!");
			exit(1);
		}

		rbl->rpcb_map.r_prog = PMAPPROG;
		rbl->rpcb_map.r_vers = PMAPVERS;	/* Version 2 */
		rbl->rpcb_map.r_netid = strdup(nconf->nc_netid);
		rbl->rpcb_map.r_addr = taddr2uaddr(nconf, &baddr->addr);
		rbl->rpcb_map.r_owner = superuser;
		rbl->rpcb_next = list_rbl;	/* Attach to global list */
		list_rbl = rbl;

		if ( Secure ) {
		/* Create the udp/tcp loopback service port so we
		   identify connections that actually used the loopback
		   interface to connect vs those spoofing the loopback
		   source address.  We exit on any failures because
		   we will not be able to set/unset portmap services
		   in pmap_service without these service ports defined */

			if ((lfd = t_open(nconf->nc_device,O_RDWR,NULL)) <0) {
				syslog(LOG_ERR, 
				       "%s: cannot open loopback connection: %s",
				       nconf->nc_netid, t_errlist[t_errno]);
				exit(1);
			}
			ltaddr = (struct t_bind *)t_alloc(lfd, T_BIND, T_ADDR);
			lbaddr = (struct t_bind *)t_alloc(lfd, T_BIND, T_ADDR);
			if (ltaddr == NULL || lbaddr == NULL) {
				syslog(LOG_ERR, 
				       "%s: cannot allocate local netbuf: %s",
				       nconf->nc_netid, t_errlist[t_errno]);
				exit(1);
			}

			sprintf(laddr, "%s.%u.%u", inet_ntoa(la),
				PMAPPORT>>8, PMAPPORT&0xff);
			nb = uaddr2taddr(nconf, laddr);
			memcpy(ltaddr->addr.buf, nb->buf, 
			       ltaddr->addr.len = nb->len);
			free(nb);
			(void)rpcb_setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR,
					      &on, sizeof(on));
			if (t_bind(lfd, ltaddr, lbaddr) != 0) {
				syslog(LOG_ERR, "%s: cannot loopback bind: %s",
				       nconf->nc_netid, t_errlist[t_errno]);
				exit(1);
			}
			l_xprt = (SVCXPRT *)svc_tli_create(lfd, nconf, lbaddr,
							   0,0);
			if (l_xprt == 0) {
				syslog(LOG_ERR, "%s: could not create"
				       " loopback service", nconf->nc_netid);
				exit(1);
			}
			if (!svc_register(l_xprt, PMAPPROG, PMAPVERS,
					  pmap_service, NULL)) {
				syslog(LOG_ERR, "could not register loopback"
				       " pmap_service %s", nconf->nc_netid);
				exit(1);
			}
			svc_versquiet(l_xprt);
			if (tcp) 
			    ltcp_xprt = l_xprt; 
			else 
			    ludp_xprt = l_xprt; 

			(void) t_free((char *)ltaddr, T_BIND);
			(void) t_free((char *)lbaddr, T_BIND);
		}
	}
#endif

	/* version 3 registration */
	if (!svc_reg(my_xprt, RPCBPROG, RPCBVERS, rpcb_service, NULL)) {
		syslog(LOG_ERR, "could not register %s version 3",
				nconf->nc_netid);
		goto error;
	}
	rbl = (RPCBLIST *)malloc((u_int)sizeof (RPCBLIST));
	if (rbl == (RPCBLIST *)NULL) {
		syslog(LOG_ERR, "no memory!");
		exit(1);
	}

	rbl->rpcb_map.r_prog = RPCBPROG;
	rbl->rpcb_map.r_vers = RPCBVERS; /* The new version number */
	rbl->rpcb_map.r_netid = strdup(nconf->nc_netid);
	rbl->rpcb_map.r_addr = taddr2uaddr(nconf, &baddr->addr);
	rbl->rpcb_map.r_owner = superuser;
	rbl->rpcb_next = list_rbl;	/* Attach to global list */
	list_rbl = rbl;

	/*
	 * Tell RPC library to shut up about version mismatches so that new
	 * revs of broadcast protocols don't cause all the old servers to
	 * say: "wrong version".
	 */
	svc_versquiet(my_xprt);

	/*
	 * In case of loopback transports, negotiate for
	 * returning of the uid of the caller.
	 */
	if (strcmp(nconf->nc_protofmly, NC_LOOPBACK) == 0) {
		if (nconf->nc_semantics == NC_TPI_CLTS)
			loopback_dg = strdup(nconf->nc_netid);
		else if (nconf->nc_semantics == NC_TPI_COTS)
			loopback_vc = strdup(nconf->nc_netid);
		else if (nconf->nc_semantics == NC_TPI_COTS_ORD)
			loopback_vc_ord = strdup(nconf->nc_netid);
		if (_rpc_negotiate_uid(fd)) {
			syslog(LOG_ERR,
			"could not negotiate with loopback tranport %s",
				nconf->nc_netid);
		}
	}

	/* decide if bound checking works for this transport */
	status = add_bndlist(nconf, taddr, baddr);
#ifdef BIND_DEBUG
	if (status < 0) {
		fprintf(stderr, "Error in finding bind status for %s\n",
			nconf->nc_netid);
	} else if (status == 0) {
		fprintf(stderr, "check binding for %s\n",
			nconf->nc_netid);
	} else if (status > 0) {
		fprintf(stderr, "No check binding for %s\n",
			nconf->nc_netid);
	}
#endif

	(void) t_free((char *)taddr, T_BIND);
	(void) t_free((char *)baddr, T_BIND);
	return (0);
error:
	(void) t_free((char *)taddr, T_BIND);
	(void) t_free((char *)baddr, T_BIND);
	(void) t_close(fd);
	return (1);
}


/*
 * XXX this should be fixed to reap our children rather than ignoring the
 * signal like we do for now ...
 */
#ifdef WAIT3
static void
reap()
{
	while (wait3(NULL, WNOHANG, NULL) > 0);
}
#endif

/*
 * Catch the signal and die
 */
static void
terminate()
{
	syslog(LOG_ERR, "terminating on signal");
	write_warmstart();	/* Dump yourself */
	exit(2);
}

void
rpcbind_abort()
{
	write_warmstart();	/* Dump yourself */
	abort();
}

/*
 * detach from tty
 */
static void
detachfromtty()
{
	close(0);
	close(1);
	close(2);
	switch (fork()) {
	case (pid_t)-1:
		perror("fork");
		break;
	case 0:
		break;
	default:
		exit(0);
	}
	setsid();
	(void)open("/dev/null", O_RDWR, 0);
	dup(0);
	dup(0);
}

/* get command line options */
static void
parseargs(argc, argv)
	int argc;
	char *argv[];
#ifndef sgi
{
	int c;

	while ((c = getopt(argc, argv, "dwC")) != EOF) {
		switch (c) {
		case 'd':
			debugging = 1;
			break;
		case 'w':
			warmstart = 1;
			break;
		case 'C':
			Secure = 0;
			break;
		default:	/* error */
			fprintf(stderr,	"usage: rpcbind -[dwC]\n");
			exit (1);
		}
	}
}
#else
{
	int t;
	int argerr = 0;
	extern int optind, opterr;
	extern char *optarg;


	opterr = 0;
	while ((t = getopt(argc, argv, "a:Abdf:mvwC")) != EOF) {
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

		/*
		 * NB: rpcbind doesn't presently fork for PMAPPROC_CALLIT
		 * so this option is ignored.  It is parsed for command
		 * line compatibility with portmap.
		 */
		case 'f': {
			int max = atoi(optarg);
			if (max < 1) {
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

		case 'w':
			warmstart = 1;
			break;

		case 'C':
			Secure = 0;
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
		/* do not mention -f since it is not supported */
		fprintf(stderr,
"usage: rpcbind [-vmAbwC] [-a mask,match | -a match]\n");
		exit(1);
	}
}


/*
 * Return 1 if the address is accepted, 0 otherwise.
 */
int
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

#endif /* sgi */
