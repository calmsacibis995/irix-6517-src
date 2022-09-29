/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ident	"@(#)rpcbind:pmap_svc.c	1.4.5.2"

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
 * pmap_svc.c
 * The server procedure for the version 2 portmaper.
 * All the portmapper related interface from the portmap side.
 */

#ifdef PORTMAP
#include <stdio.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/rpcb_prot.h>

#ifdef CHECK_LOCAL
#include <syslog.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <net/if.h>		/* to find local addresses */
#include <net/route.h>
#include <sys/sysctl.h>
#define	BSD_COMP		/* XXX - so that it includes <sys/sockio.h> */
#include <sys/ioctl.h>
#include <sys/socket.h>

#include "rpcbind.h"

#ifndef INADDR_LOOPBACK		/* Some <netinet/in.h> files do not have this */
#define	INADDR_LOOPBACK		(u_long)0x7F000001
#endif
#endif  /* CHECK_LOCAL */

static PMAPLIST *find_service();
static bool_t pmapproc_change();
static bool_t pmapproc_getport();
static bool_t pmapproc_dump();
static void pmapproc_callit();

/*
 * Called for all the version 2 inquiries.
 */
void
pmap_service(rqstp, xprt)
	struct svc_req *rqstp;
	SVCXPRT *xprt;
{
#ifndef sgi
	if (verbose)
		fprintf(stderr, "portmap: request for proc %d\n", 
			rqstp->rq_proc);
#else
	struct sockaddr_in *who;
	struct netbuf      *nbuf;
	int	local;

	nbuf = svc_getrpccaller(xprt);
	who  = (struct sockaddr_in *)nbuf->buf;

	if (verbose)
		fprintf(stderr, "portmap: %s request for proc %u\n", 
			inet_ntoa(who->sin_addr), rqstp->rq_proc);

	local = chklocal(who->sin_addr);

	if (xprt == m_xprt) {
		/*
		 * Ignore (supposedly) local requests that arrived via
		 * multicast in case they come from a bad guy on the Internet
		 * sending poison packets to the universe.
		 */
		if (local) {
			if (verbose)
				syslog(LOG_INFO|LOG_AUTH,
				       "ignore multicast proc %u call"
				       " from local %s",
				       rqstp->rq_proc,
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
				       "ignore multicast proc %u call from %s",
				       rqstp->rq_proc,
				       inet_ntoa(who->sin_addr));
			return;
		}

	} else {
		/*
		 * Allow "null" procedure requests from anybody since
		 * it returns no port information.
		 */
		if ((num_oknets > 0 || Aflag) && !bflag &&
		    !local && rqstp->rq_proc != PMAPPROC_NULL &&
		    !chknet(who->sin_addr)) {
			if (verbose)
				syslog(LOG_INFO|LOG_AUTH,
				       "rejected proc %u call from %s",
				       rqstp->rq_proc,
				       inet_ntoa(who->sin_addr));
			svcerr_auth(xprt, AUTH_FAILED);
			return;
		}
	}
#endif
	switch (rqstp->rq_proc) {
	case PMAPPROC_NULL:
		/*
		 * Null proc call
		 */
#ifdef DEBUG
		fprintf(stderr, "PMAPPROC_NULL\n");
#endif
		if ((!svc_sendreply(xprt, xdr_void, NULL)) && debugging) {
			rpcbind_abort();
		}
		break;

	case PMAPPROC_SET:
		/*
		 * Set a program, version to port mapping
		 */
#ifdef DEBUG
		fprintf(stderr, "PMAPPROC_SET\n");
#endif
		if (Secure && xprt != ludp_xprt && xprt != ltcp_xprt) {
			syslog(LOG_ERR, "non-local attempt to set");
			svcerr_weakauth(xprt);
			return;
		}
		    
		pmapproc_change(rqstp, xprt, rqstp->rq_proc);
		break;

	case PMAPPROC_UNSET:
		/*
		 * Remove a program, version to port mapping.
		 */
#ifdef DEBUG
		fprintf(stderr, "PMAPPROC_UNSET \n");
#endif
		if (Secure && xprt != ludp_xprt && xprt != ltcp_xprt) {
			syslog(LOG_ERR, "non-local attempt to unset");
			svcerr_weakauth(xprt);
			return;
		}

		pmapproc_change(rqstp, xprt, rqstp->rq_proc);
		break;

	case PMAPPROC_GETPORT:
		/*
		 * Lookup the mapping for a program, version and return its
		 * port number.
		 */
#ifdef DEBUG
		fprintf(stderr, "PMAPPROC_GETPORT\n");
#endif
		pmapproc_getport(rqstp, xprt);
		break;

	case PMAPPROC_DUMP:
		/*
		 * Return the current set of mapped program, version
		 */
#ifdef DEBUG
		fprintf(stderr, "PMAPPROC_DUMP\n");
#endif
		pmapproc_dump(rqstp, xprt);
		break;

	case PMAPPROC_CALLIT:
		/*
		 * Calls a procedure on the local machine. If the requested
		 * procedure is not registered this procedure does not return
		 * error information!!
		 * This procedure is only supported on rpc/udp and calls via
		 * rpc/udp. It passes null authentication parameters.
		 */
#ifdef DEBUG
		fprintf(stderr, "PMAPPROC_CALLIT\n");
#endif
		pmapproc_callit(rqstp, xprt);
		break;

	default:
		svcerr_noproc(xprt);
		break;
	}
}

/*
 * returns the item with the given program, version number. If that version
 * number is not found, it returns the item with that program number, so that
 * the port number is now returned to the caller. The caller when makes a
 * call to this program, version number, the call will fail and it will
 * return with PROGVERS_MISMATCH. The user can then determine the highest
 * and the lowest version number for this program using clnt_geterr() and
 * use those program version numbers.
 */
static PMAPLIST *
find_service(prog, vers, prot)
	u_long prog;
	u_long vers;
	u_long prot;
{
	register PMAPLIST *hit = NULL;
	register PMAPLIST *pml;

	for (pml = list_pml; pml != NULL; pml = pml->pml_next) {
		if ((pml->pml_map.pm_prog != prog) ||
			(pml->pml_map.pm_prot != prot))
			continue;
		hit = pml;
		if (pml->pml_map.pm_vers == vers)
			break;
	}
	return (hit);
}

static bool_t
pmapproc_change(rqstp, xprt, op)
	struct svc_req *rqstp;
	register SVCXPRT *xprt;
	unsigned long op;
{
	PMAP reg;
	RPCB rpcbreg;
	int ans;
	struct sockaddr_in *who;
	extern bool_t map_set(), map_unset();

	if (!svc_getargs(xprt, xdr_pmap, &reg)) {
		svcerr_decode(xprt);
		return (FALSE);
	}
	who = svc_getcaller(xprt);

#ifdef CHECK_LOCAL
	/*
	 * To check whether the request came from a local server.  If this
	 * cannot be tested, we assign that call as "unknown".
	 */
	if (chklocal(ntohl(who->sin_addr)) == FALSE) {
		ans = FALSE;
		goto done_change;
	}
	if (ntohs(who->sin_port) >= IPPORT_RESERVED)
		rpcbreg.r_owner = "unknown";
	else
		rpcbreg.r_owner = "superuser";
#else
	rpcbreg.r_owner = "unknown";
#endif

	if ((op == PMAPPROC_SET) && (reg.pm_port < IPPORT_RESERVED) &&
	    (ntohs(who->sin_port) >= IPPORT_RESERVED)) {
		ans = FALSE;
		goto done_change;
	}
	rpcbreg.r_prog = reg.pm_prog;
	rpcbreg.r_vers = reg.pm_vers;

	if (op == PMAPPROC_SET) {
		char buf[32];

		sprintf(buf, "0.0.0.0.%d.%d", reg.pm_port >> 8 & 0xff,
			reg.pm_port & 0xff);
		rpcbreg.r_addr = buf;
		if (reg.pm_prot == IPPROTO_UDP) {
			rpcbreg.r_netid = udptrans;
		} else if (reg.pm_prot == IPPROTO_TCP) {
			rpcbreg.r_netid = tcptrans;
		} else {
			ans = FALSE;
			goto done_change;
		}
		ans = map_set(&rpcbreg, rpcbreg.r_owner);
	} else if (op == PMAPPROC_UNSET) {
		bool_t ans1, ans2;

		rpcbreg.r_addr = NULL;
		rpcbreg.r_netid = tcptrans;
		ans1 = map_unset(&rpcbreg, rpcbreg.r_owner);
		rpcbreg.r_netid = udptrans;
		ans2 = map_unset(&rpcbreg, rpcbreg.r_owner);
		ans = ans1 || ans2;
	} else {
		ans = FALSE;
	}
done_change:
	if ((!svc_sendreply(xprt, xdr_long, (caddr_t) &ans)) &&
	    debugging) {
		fprintf(stderr, "portmap: svc_sendreply\n");
		rpcbind_abort();
	}
	return (TRUE);
}

/* ARGSUSED */
static bool_t
pmapproc_getport(rqstp, xprt)
	struct svc_req *rqstp;
	register SVCXPRT *xprt;
{
	PMAP reg;
	int port;
	PMAPLIST *fnd;

	if (!svc_getargs(xprt, xdr_pmap, &reg)) {
		svcerr_decode(xprt);
		return (FALSE);
	}
	fnd = find_service(reg.pm_prog, reg.pm_vers, reg.pm_prot);
	if (fnd) {
		char serveuaddr[32], *ua;
		int h1, h2, h3, h4, p1, p2;
		char *netid;

		if (reg.pm_prot == IPPROTO_UDP) {
			ua = udp_uaddr;
			netid = udptrans;
		} else {
			ua = tcp_uaddr; /* To get the len */
			netid = tcptrans;
		}
		sscanf(ua, "%d.%d.%d.%d.%d.%d", &h1, &h2, &h3,
				&h4, &p1, &p2);
		p1 = (fnd->pml_map.pm_port >> 8) & 0xff;
		p2 = (fnd->pml_map.pm_port) & 0xff;
		sprintf(serveuaddr, "%d.%d.%d.%d.%d.%d",
				h1, h2, h3, h4, p1, p2);
		if (is_bound(netid, serveuaddr))
			port = fnd->pml_map.pm_port;
		else {	/* this service is dead; delete it */
			RPCB rpcbreg;

			rpcbreg.r_prog = reg.pm_prog;
			rpcbreg.r_vers = reg.pm_vers;
			rpcbreg.r_addr = NULL;
			rpcbreg.r_netid = netid;
			map_unset(&rpcbreg, "superuser");
			port = 0;
		}
	} else {
		port = 0;
	}
	if ((!svc_sendreply(xprt, xdr_long, (caddr_t)&port)) &&
			debugging) {
		(void) fprintf(stderr, "portmap: svc_sendreply\n");
		rpcbind_abort();
	}
	return (TRUE);
}

/* ARGSUSED */
static bool_t
pmapproc_dump(rqstp, xprt)
	struct svc_req *rqstp;
	register SVCXPRT *xprt;
{
	if (!svc_getargs(xprt, xdr_void, NULL)) {
		svcerr_decode(xprt);
		return (FALSE);
	}
	if ((!svc_sendreply(xprt, xdr_pmaplist,
			(caddr_t)&list_pml)) && debugging) {
		(void) fprintf(stderr, "portmap: svc_sendreply\n");
		rpcbind_abort();
	}
	return (TRUE);
}

#ifdef CHECK_LOCAL
static struct timeval when_local;
int num_local = 0;
union addrs *addrs;
static size_t addrs_size = 0;

void
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
	for (ifam = (struct ifa_msghdr *)addrs->buf; ifam < ifam_lim; ifam = ifam2) {
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
#define SAROUNDUP(a) ((a) > 0 ? (1 + (((a) - 1) | (sizeof(__uint64_t) - 1))) \
		      : sizeof(__uint64_t))
#ifdef _HAVE_SA_LEN
			sa = (struct sockaddr *)((char*)sa
						 + SAROUNDUP(sa->sa_len));
#else
			sa = (struct sockaddr *
			      )((char*)sa + SAROUNDUP(_FAKE_SA_LEN_DST(sa)));
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


bool_t
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
#endif /* CHECK_LOCAL */

/*
 * Call a remote procedure service
 * This procedure is very quiet when things go wrong.
 * The proc is written to support broadcast rpc. In the broadcast case,
 * a machine should shut-up instead of complain, less the requestor be
 * overrun with complaints at the expense of not hearing a valid reply ...
 */
static void
pmapproc_callit(rqstp, xprt)
	struct svc_req *rqstp;
	SVCXPRT *xprt;
{
	rpcbproc_callit_common(rqstp, xprt, PMAP_TYPE);
}

#endif /* PORTMAP */
