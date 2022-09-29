/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 *  1.23 88/02/08
 */


/*
 * pmap_rmt.c
 * Client interface to pmap rpc service.
 * remote call and broadcast service
 */

#ifdef _KERNEL
#include "rpc.h"
#include "pmap_rmt.h"

#else
#ifdef __STDC__
	#pragma weak pmap_setrmtcalltimeout = _pmap_setrmtcalltimeout
	#pragma weak pmap_rmtcall = _pmap_rmtcall
	#pragma weak xdr_rmtcall_args = _xdr_rmtcall_args
	#pragma weak xdr_rmtcallres = _xdr_rmtcallres
	#pragma weak clnt_setbroadcastbackoff = _clnt_setbroadcastbackoff
	#pragma weak clnt_broadcast = _clnt_broadcast
	#pragma weak clnt_multicast = _clnt_multicast
	#pragma weak clnt_broadmulti = _clnt_broadmulti
	#pragma weak clnt_broadcast_exp = _clnt_broadcast_exp
	#pragma weak clnt_multicast_exp = _clnt_multicast_exp
	#pragma weak clnt_broadmulti_exp = _clnt_broadmulti_exp
#endif
#include "synonyms.h"
#include <bstring.h>
#include <ctype.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_rmt.h>
#include <rpc/errorhandler.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <net/if.h>
#include <net/route.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>		/* prototype for select() */
#include <string.h>		/* prototype for strerror() */

#define MAX_BROADCAST_SIZE 1400

static struct timeval timeout = { 3, 0 };
void
pmap_setrmtcalltimeout(struct timeval intertry)
{
	timeout = intertry;
}


/*
 * pmapper remote-call-service interface.
 * This routine is used to call the pmapper remote call service
 * which will look up a service program in the port maps, and then
 * remotely call that routine with the given parameters.  This allows
 * programs to do a lookup and call in one step.
*/
enum clnt_stat
pmap_rmtcall(struct sockaddr_in *addr,
	u_long prog,
	u_long vers,
	u_long proc,
	xdrproc_t xdrargs,
	void *argsp,
	xdrproc_t xdrres,
	void *resp,
	struct timeval tout,
	u_long *port_ptr)
{
	int socket = -1;
	register CLIENT *client;
	struct rmtcallargs a;
	struct rmtcallres r;
	enum clnt_stat stat;

	addr->sin_port = htons(PMAPPORT);
	client = clntudp_create(addr, PMAPPROG, PMAPVERS, timeout, &socket);
	if (client != (CLIENT *)NULL) {
		a.prog = prog;
		a.vers = vers;
		a.proc = proc;
		a.args_ptr = argsp;
		a.xdr_args = xdrargs;
		r.port_ptr = port_ptr;
		r.results_ptr = resp;
		r.xdr_results = xdrres;
		stat = CLNT_CALL(client, PMAPPROC_CALLIT, (xdrproc_t) xdr_rmtcall_args, &a,
		    (xdrproc_t) xdr_rmtcallres, &r, tout);
		CLNT_DESTROY(client);
	} else {
		stat = RPC_FAILED;
	}
	(void)close(socket);
	addr->sin_port = 0;
	return (stat);
}

#endif	/* !KERNEL */

/*
 * XDR remote call arguments
 * written for XDR_ENCODE direction only
 */
bool_t
xdr_rmtcall_args(register XDR *xdrs, register struct rmtcallargs *cap)
{
	u_int lenposition, argposition, position;

	if (xdr_u_long(xdrs, &(cap->prog)) &&
	    xdr_u_long(xdrs, &(cap->vers)) &&
	    xdr_u_long(xdrs, &(cap->proc))) {
		lenposition = XDR_GETPOS(xdrs);
		if (! xdr_u_long(xdrs, &(cap->arglen)))
		    return (FALSE);
		argposition = XDR_GETPOS(xdrs);
		if (! (*(p_xdrproc_t)(cap->xdr_args))(xdrs, cap->args_ptr))
		    return (FALSE);
		position = XDR_GETPOS(xdrs);
		cap->arglen = (u_long)position - (u_long)argposition;
		XDR_SETPOS(xdrs, lenposition);
		if (! xdr_u_long(xdrs, &(cap->arglen)))
		    return (FALSE);
		XDR_SETPOS(xdrs, position);
		return (TRUE);
	}
	return (FALSE);
}

/*
 * XDR remote call results
 * written for XDR_DECODE direction only
 */
bool_t
xdr_rmtcallres(register XDR *xdrs, register struct rmtcallres *crp)
{
	caddr_t port_ptr;

	port_ptr = (caddr_t)crp->port_ptr;
	if (xdr_reference(xdrs, &port_ptr, sizeof (u_long), (xdrproc_t) xdr_u_long)
	    && xdr_u_long(xdrs, &crp->resultslen)) {
		crp->port_ptr = (u_long *)port_ptr;
		return ((*(p_xdrproc_t)(crp->xdr_results))(xdrs, crp->results_ptr));
	}
	return (FALSE);
}

#ifndef _KERNEL

/*
 * The following is kludged-up support for simple rpc broadcasts.
 * Someday a large, complicated system will replace these trivial
 * routines which only support udp/ip .
 */

union brd_addrs {
	char	buf[1];
	struct ifa_msghdr ifam;
	struct in_addr a[1];
};

static int
getbroadcastnets(union brd_addrs **addrs_p)
{
	int i, nets;
	size_t needed;
	union brd_addrs *addrs;
	int mib[6];
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam, *ifam_lim, *ifam2;
	struct sockaddr *sa;

	/*
	 * Fetch the interface list.
	 */
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;
	for (;;) {
		needed = 0;
		if (sysctl(mib, 6, 0, &needed, 0, 0) < 0) {
			_rpc_errorhandler(LOG_ERR,
					  "broadcast: sysctl estimate: %s",
					  strerror(errno));
			return (0);
		}
		addrs = (union brd_addrs *)malloc(needed);
		if (addrs == 0) {
			_rpc_errorhandler(LOG_ERR, "broadcast: malloc failed");
			return (0);
		}

		if (sysctl(mib, 6, addrs,&needed, 0, 0) >= 0)
			break;

		if (errno != ENOMEM && errno != EFAULT) {
			_rpc_errorhandler(LOG_ERR, "broadcast: sysctl: %s",
					  strerror(errno));
			free(addrs);
			return (0);
		}
		/*
		 * If a bunch of interfaces appeared between
		 * the two sysctl() calls, try again.
		 */
		free(addrs);
	}

	nets = 0;
	ifam_lim = (struct ifa_msghdr *)&addrs->buf[needed];
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

		if ((ifm->ifm_flags & (IFF_UP | IFF_BROADCAST))
		    != (IFF_UP | IFF_BROADCAST))
			continue;

		/*
		 * Find the broadcast address among the other addresses.
		 */
		sa = (struct sockaddr *)(ifam+1);
		for (i = 0;
		     i <= RTAX_BRD && sa < (struct sockaddr *)ifam2;
		     i++) {
			if ((ifam->ifam_addrs & (1 << i)) == 0)
				continue;
			if (i == RTAX_BRD)
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
		if (i > RTAX_BRD
#ifdef _HAVE_SA_LEN
		    || sa->sa_len == 0
#endif
		    || sa >= (struct sockaddr *)ifam2
		    || sa->sa_family != AF_INET)
			continue;

		addrs->a[nets++] = ((struct sockaddr_in *)sa)->sin_addr;
	}

	*addrs_p = addrs;
	return nets;
}


/*
 * Parameterized broadcast timeout backoff.
 */

typedef void (*firstproc_t)(struct timeval *, int);
typedef int  (*nextproc_t)(struct timeval *, int);

static int default_total;

static void
default_firsttimeout(struct timeval *tv, int init)
{
	tv->tv_sec = init;
	tv->tv_usec = 0;
	default_total = 0;
}

static int
default_nexttimeout(struct timeval *tv, int wait)
{
	default_total += tv->tv_sec;
	tv->tv_sec += tv->tv_sec;
	return default_total <= wait;
}

static firstproc_t	firsttimeout = default_firsttimeout;
static nextproc_t	nexttimeout = default_nexttimeout;

void
clnt_setbroadcastbackoff(firstproc_t first, nextproc_t  next)
{
	firsttimeout = first;
	nexttimeout = next;
}

/*
 * Try to preserve Sun's error messages, including character case.
 */
static char *
typename(u_short type, bool_t capitalize)
{
	char *name = (type == IFF_BROADCAST) ? "broadcast" : "multicast";
	if (capitalize)
		*name = (char)toupper(*name);
	return name;
}

typedef bool_t (*resultproc_t)(void *, struct sockaddr_in *);

static enum clnt_stat
clnt_manycast(
	u_short		type,		/* IFF_BROADCAST or IFF_MULTICAST */
	u_long		prog,		/* program number */
	u_long		vers,		/* version number */
	u_long		proc,		/* procedure number */
	xdrproc_t	xargs,		/* xdr routine for args */
	void *		argsp,		/* pointer to args */
	xdrproc_t	xresults,	/* xdr routine for results */
	void *		resultsp,	/* pointer to results */
	resultproc_t	eachresult,	/* call with each result obtained */
	firstproc_t	firstproc,
	int		firsttime,
	nextproc_t	nextproc,
	int		nexttime,
	u_char		ttl,		/* initial TTL */
	u_char		incttl)		/* TTL increment */
{
	enum clnt_stat stat;
	AUTH *unix_auth = authunix_create_default();
	XDR xdr_stream;
	register XDR *xdrs = &xdr_stream;
	fd_set readfds;
	int outlen, inlen, fromlen, nets = 0;
	union brd_addrs *addrs = 0;
	register int sock, i;
	bool_t done = FALSE;
	register u_long xid;
	u_long port;
	struct sockaddr_in maddr, raddr; /* multicast and response addresses */
	struct rmtcallargs a;
	struct rmtcallres r;
	struct rpc_msg msg;
	struct timeval t;
	char outbuf[MAX_BROADCAST_SIZE], inbuf[UDPMSGSIZE];

	if (unix_auth == (AUTH *)NULL)
		return (RPC_SYSTEMERROR);
	/*
	 * initialization: create a socket, a multicast address, and
	 * preserialize the arguments into a send buffer.
	 */
	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		_rpc_errorhandler(LOG_ERR,
			"Cannot create socket: %s", strerror(errno));
		stat = RPC_CANTSEND;
		goto done_many;
	}
	if (type & IFF_BROADCAST) {
		int on = 1;
		if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof on)
		    < 0) {
			_rpc_errorhandler(LOG_ERR,
				"Cannot set socket option SO_BROADCAST: %s",
				strerror(errno));
			stat = RPC_CANTSEND;
			goto done_many;
		}
		nets = getbroadcastnets(&addrs);
	}

	FD_ZERO(&readfds);
	bzero(maddr.sin_zero, sizeof maddr.sin_zero);
	maddr.sin_family = AF_INET;
	maddr.sin_port = htons(PMAPPORT);
	(void)gettimeofday(&t, 0);
	msg.rm_xid = xid = (unsigned long)(getpid() ^ t.tv_sec ^ t.tv_usec);
	t.tv_usec = 0;
	msg.rm_direction = CALL;
	msg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
	msg.rm_call.cb_prog = PMAPPROG;
	msg.rm_call.cb_vers = PMAPVERS;
	msg.rm_call.cb_proc = PMAPPROC_CALLIT;
	msg.rm_call.cb_cred = unix_auth->ah_cred;
	msg.rm_call.cb_verf = unix_auth->ah_verf;
	a.prog = prog;
	a.vers = vers;
	a.proc = proc;
	a.xdr_args = xargs;
	a.args_ptr = argsp;
	r.port_ptr = &port;
	r.xdr_results = xresults;
	r.results_ptr = resultsp;
	xdrmem_create(xdrs, outbuf, sizeof outbuf, XDR_ENCODE);
	if ((! xdr_callmsg(xdrs, &msg)) || (! xdr_rmtcall_args(xdrs, &a))) {
		stat = RPC_CANTENCODEARGS;
		goto done_many;
	}
	outlen = (int)xdr_getpos(xdrs);
	xdr_destroy(xdrs);
	/*
	 * Basic loop: multicast a packet and wait a while for response(s).
	 * The response timeout grows larger per iteration.
	 */
	(*firstproc)(&t, firsttime);
	do {
		int sent_ok = 0;
		/*
		 * Send broadcast packets
		 */
		if (type & IFF_BROADCAST) {
			for (i = 0; i < nets; i++) {
				maddr.sin_addr = addrs->a[i];
				if (sendto(sock, outbuf, outlen, 0,
					   (struct sockaddr *)&maddr,
					   sizeof (struct sockaddr)) == outlen)
					sent_ok = 1;
			}
		}

		/*
		 * Send multicast packet
		 */
		if (type & IFF_MULTICAST) {
			if (ttl > 32)
				ttl = 32;
			if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL,
				       &ttl, sizeof ttl) < 0) {
				if (sent_ok)
					goto out_done;
				_rpc_errorhandler(LOG_ERR,
						  "Cannot set IP_MULTICAST_TTL"
						  " to %d: %s",
						  strerror(errno));
				stat = RPC_CANTSEND;
				goto done_many;
			}
			maddr.sin_addr.s_addr = htonl(PMAP_MULTICAST_INADDR);
			if (sendto(sock, outbuf, outlen, 0,
				   (struct sockaddr *)&maddr,
				   sizeof (struct sockaddr)) == outlen)
				sent_ok = 1;
			ttl += incttl;
		}

		if (!sent_ok) {
			/* Reports last error */
			_rpc_errorhandler(LOG_ERR,
					  "Cannot send %s packet: %s",
					  typename(type, 0),
					  ((nets != 0 || (type&IFF_MULTICAST))
					   ? strerror(errno)
					   : ""));
			stat = RPC_CANTSEND;
			goto done_many;
		}
	out_done:
		if (eachresult == NULL) {
			stat = RPC_SUCCESS;
			goto done_many;
		}
	recv_again:
		msg.acpted_rply.ar_verf = _null_auth;
		msg.acpted_rply.ar_results.where = (caddr_t)&r;
		msg.acpted_rply.ar_results.proc = (xdrproc_t) xdr_rmtcallres;
		FD_SET(sock, &readfds);
		switch (select(sock+1, &readfds, (fd_set *)NULL,
			       (fd_set *)NULL, &t)) {

		case 0:  /* timed out */
			stat = RPC_TIMEDOUT;
			continue;

		case -1:  /* some kind of error */
			if (errno == EINTR)
				goto recv_again;
			_rpc_errorhandler(LOG_ERR, "%s select problem: %s",
				typename(type, 1), strerror(errno));
			stat = RPC_CANTRECV;
			goto done_many;

		}  /* end of select results switch */
	try_again:
		fromlen = sizeof(struct sockaddr);
		inlen = recvfrom(sock, inbuf, sizeof inbuf, 0,
			(struct sockaddr *)&raddr, &fromlen);
		if (inlen < 0) {
			if (errno == EINTR)
				goto try_again;
			_rpc_errorhandler(LOG_ERR,
				"Cannot receive reply to %s: %s",
				typename(type, 0), strerror(errno));
			stat = RPC_CANTRECV;
			goto done_many;
		}
		if (inlen < (int)sizeof(u_long))
			goto recv_again;
		/*
		 * see if reply transaction id matches sent id.
		 * If so, decode the results.
		 */
		xdrmem_create(xdrs, inbuf, (u_int)inlen, XDR_DECODE);
		if (xdr_replymsg(xdrs, &msg)) {
			if ((msg.rm_xid == xid) &&
				(msg.rm_reply.rp_stat == MSG_ACCEPTED) &&
				(msg.acpted_rply.ar_stat == SUCCESS)) {
				raddr.sin_port = htons((u_short)port);
				done = (*eachresult)(resultsp, &raddr);
			}
			/* otherwise, we just ignore the errors ... */
		} else {
#ifdef notdef
			/* some kind of deserialization problem ... */
			if (msg.rm_xid == xid)
				_rpc_errorhandler(LOG_ERR,
				    "Broadcast deserialization problem");
			/* otherwise, just random garbage */
#endif
		}
		xdrs->x_op = XDR_FREE;
		msg.acpted_rply.ar_results.proc = (xdrproc_t) xdr_void;
		(void)xdr_replymsg(xdrs, &msg);
		(void)(*(p_xdrproc_t)xresults)(xdrs, resultsp);
		xdr_destroy(xdrs);
		if (done) {
			stat = RPC_SUCCESS;
			goto done_many;
		}
		goto recv_again;
		/* NOTREACHED */
	} while ((*nextproc)(&t, nexttime));
done_many:
	(void)close(sock);
	if (addrs != 0)
		free(addrs);
	AUTH_DESTROY(unix_auth);
	return (stat);
}

enum clnt_stat
clnt_broadcast(
	u_long		prog,		/* program number */
	u_long		vers,		/* version number */
	u_long		proc,		/* procedure number */
	xdrproc_t	xargs,		/* xdr routine for args */
	void *		argsp,		/* pointer to args */
	xdrproc_t	xresults,	/* xdr routine for results */
	void *		resultsp,	/* pointer to results */
	resultproc_t	eachresult)	/* call with each result obtained */
{
	return clnt_manycast(IFF_BROADCAST,
			     prog, vers, proc, xargs, argsp,
			     xresults, resultsp, eachresult,
			     firsttimeout, 4, nexttimeout, 45, 2, 8);
}

enum clnt_stat
clnt_multicast(
	u_long		prog,		/* program number */
	u_long		vers,		/* version number */
	u_long		proc,		/* procedure number */
	xdrproc_t	xargs,		/* xdr routine for args */
	void *		argsp,		/* pointer to args */
	xdrproc_t	xresults,	/* xdr routine for results */
	void *		resultsp,	/* pointer to results */
	resultproc_t	eachresult)	/* call with each result obtained */
{
	return clnt_manycast(IFF_MULTICAST,
			     prog, vers, proc, xargs, argsp,
			     xresults, resultsp, eachresult,
			     firsttimeout, 4, nexttimeout, 45, 2, 8);
}

enum clnt_stat
clnt_broadmulti(
	u_long		prog,		/* program number */
	u_long		vers,		/* version number */
	u_long		proc,		/* procedure number */
	xdrproc_t	xargs,		/* xdr routine for args */
	void *		argsp,		/* pointer to args */
	xdrproc_t	xresults,	/* xdr routine for results */
	void *		resultsp,	/* pointer to results */
	resultproc_t	eachresult,	/* call with each result obtained */
	u_char		ttl,		/* initial TTL */
	u_char		incttl)		/* TTL increment */
{
	return clnt_manycast(IFF_BROADCAST|IFF_MULTICAST,
			     prog, vers, proc, xargs, argsp,
			     xresults, resultsp, eachresult,
			     firsttimeout, 4, nexttimeout, 45,
			     ttl, incttl);
}

enum clnt_stat
clnt_broadcast_exp(
	u_long		prog,		/* program number */
	u_long		vers,		/* version number */
	u_long		proc,		/* procedure number */
	xdrproc_t	xargs,		/* xdr routine for args */
	void *		argsp,		/* pointer to args */
	xdrproc_t	xresults,	/* xdr routine for results */
	void *		resultsp,	/* pointer to results */
	resultproc_t	eachresult,	/* call with each result obtained */
	int		inittime,	/* how long to wait*/
	int		waittime)	/* how long to wait*/
{
	return clnt_manycast(IFF_BROADCAST,
			     prog, vers, proc, xargs, argsp,
			     xresults, resultsp, eachresult,
			     firsttimeout, inittime, nexttimeout, waittime,
			     2, 8);
}

enum clnt_stat
clnt_multicast_exp(
	u_long		prog,		/* program number */
	u_long		vers,		/* version number */
	u_long		proc,		/* procedure number */
	xdrproc_t	xargs,		/* xdr routine for args */
	void *		argsp,		/* pointer to args */
	xdrproc_t	xresults,	/* xdr routine for results */
	void *		resultsp,	/* pointer to results */
	resultproc_t	eachresult,	/* call with each result obtained */
	int		inittime,	/* how long to wait*/
	int		waittime)	/* how long to wait*/
{
	return clnt_manycast(IFF_MULTICAST,
			     prog, vers, proc, xargs, argsp,
			     xresults, resultsp, eachresult,
			     firsttimeout, inittime, nexttimeout, waittime,
			     2, 8);
}

enum clnt_stat
clnt_broadmulti_exp(
	u_long		prog,		/* program number */
	u_long		vers,		/* version number */
	u_long		proc,		/* procedure number */
	xdrproc_t	xargs,		/* xdr routine for args */
	void *		argsp,		/* pointer to args */
	xdrproc_t	xresults,	/* xdr routine for results */
	void *		resultsp,	/* pointer to results */
	resultproc_t	eachresult,	/* call with each result obtained */
	int		inittime,	/* how long to wait*/
	int		waittime,	/* how long to wait*/
	u_char		ttl,		/* initial TTL */
	u_char		incttl)		/* TTL increment */
{
	return clnt_manycast(IFF_BROADCAST|IFF_MULTICAST,
			     prog, vers, proc, xargs, argsp,
			     xresults, resultsp, eachresult,
			     firsttimeout, inittime, nexttimeout, waittime,
			     ttl, incttl);
}
#endif	/* !KERNEL */
