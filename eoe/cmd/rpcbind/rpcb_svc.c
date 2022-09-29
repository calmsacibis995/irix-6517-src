/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ident	"@(#)rpcbind:rpcb_svc.c	1.13.8.2"

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
 * rpcb_svc.c
 * The server procedure for the version 3 rpcbind (TLI).
 * It maintains a separate list of all the registered services with the
 * version 3 of rpcbind.
 */
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <rpc/rpc.h>
#include <rpc/rpcb_prot.h>
#include <netconfig.h>
#include <sys/param.h>
#include <sys/errno.h>
#ifdef PORTMAP
#include <netinet/in.h>
#include <rpc/pmap_prot.h>
#endif /* PORTMAP */
#ifdef SYSLOG
#include <sys/syslog.h>
#else
#define	LOG_ERR 3
#endif /* SYSLOG */
#include <netdir.h>
#include "rpcbind.h"

extern char *strdup(), *strcpy();
static bool_t xdr_opaque_parms();
static RPCBLIST *find_service();
static bool_t *rpcbproc_set_3();
static bool_t *rpcbproc_unset_3();
static char **rpcbproc_getaddr_3();
static RPCBLIST **rpcbproc_dump_3();
static u_long *rpcbproc_gettime_3();
static struct netbuf *rpcbproc_uaddr2taddr_3();
static char **rpcbproc_taddr2uaddr_3();
static char *getowner();
static void rpcbproc_callit();
bool_t map_set(), map_unset();
#ifdef PORTMAP
static int del_pmaplist();
static int add_pmaplist();
#endif

static int	forward_find();
static u_long	forward_register();
static struct netbuf	*netbufdup();
static void	handle_reply( int );
static int	svc_myreplyto();
static int	netbufcmp();
static int	free_slot_by_xid();
static int	free_slot_by_fd();
static int	free_slot_by_index();
static int	check_rmtcalls();

static int rpcb_rmtcalls;

char *nullstring = "";

/*
 * Called by svc_getreqset. There is a separate server handle for
 * every transport that it waits on.
 */
void
rpcb_service(rqstp, xprt)
	register struct svc_req *rqstp;
	register SVCXPRT *xprt;
{
	union {
		RPCB rpcbproc_set_3_arg;
		RPCB rpcbproc_unset_3_arg;
		RPCB rpcbproc_getaddr_3_arg;
		struct rpcb_rmtcallargs rpcbproc_callit_3_arg;
		char *rpcbproc_uaddr2taddr_3_arg;
		struct netbuf rpcbproc_taddr2uaddr_3_arg;
	} argument;
	char *result;
	bool_t (*xdr_argument)(), (*xdr_result)();
	char *(*local_func)();

#ifdef sgi
	if (!strcmp(tcptrans, xprt->xp_netid) ||
	    !strcmp(udptrans, xprt->xp_netid))  {
		struct sockaddr_in *who;
		struct netbuf      *nbuf;
		int	local;

		nbuf = svc_getrpccaller(xprt);
		who  = (struct sockaddr_in *)nbuf->buf;

		if (verbose)
			fprintf(stderr, "rpcbind: %s request for proc %u\n", 
				inet_ntoa(who->sin_addr), rqstp->rq_proc);

		local = chklocal(who->sin_addr);

		/*
		 * Allow "null" procedure requests from anybody since
		 * it returns no port information.
		 */
		if (num_oknets > 0 && !local && 
		    rqstp->rq_proc != PMAPPROC_NULL &&
		    !chknet(who->sin_addr)) {
			if (verbose)
				syslog(LOG_INFO|LOG_AUTH,
				    "rejected proc %u call from %s",
				    rqstp->rq_proc, inet_ntoa(who->sin_addr));
			svcerr_auth(xprt, AUTH_FAILED);
			return;
		}
	} else 
#endif
	if (verbose)
		(void) fprintf(stderr, "rpcbind: request for proc %u\n",
				rqstp->rq_proc);

	switch (rqstp->rq_proc) {
	case RPCBPROC_NULL:
		/*
		 * Null proc call
		 */
#ifdef DEBUG
		fprintf(stderr, "RPCBPROC_NULL\n");
#endif
		(void)svc_sendreply(xprt, xdr_void, (char *)NULL);
		return;

	case RPCBPROC_SET:
#ifdef DEBUG
		fprintf(stderr, "RPCBPROC_SET\n");
#endif
		/*
		 * Check to see whether the message came from
		 * loopback transports (for security reasons)
		 */
		if (strcasecmp(xprt->xp_netid, loopback_dg) &&
			strcasecmp(xprt->xp_netid, loopback_vc) &&
			strcasecmp(xprt->xp_netid, loopback_vc_ord)) {
			syslog(LOG_ERR, "non-local attempt to set");
			svcerr_weakauth(xprt);
			return;
		}
		xdr_argument = xdr_rpcb;
		xdr_result = xdr_bool;
		local_func = (char *(*)()) rpcbproc_set_3;
		break;

	case RPCBPROC_UNSET:
#ifdef DEBUG
		fprintf(stderr, "RPCBPROC_UNSET\n");
#endif
		/*
		 * Check to see whether the message came from
		 * loopback transports (for security reasons)
		 */
		if (strcasecmp(xprt->xp_netid, loopback_dg) &&
			strcasecmp(xprt->xp_netid, loopback_vc) &&
			strcasecmp(xprt->xp_netid, loopback_vc_ord)) {
			syslog(LOG_ERR, "non-local attempt to unset");
			svcerr_weakauth(xprt);
			return;
		}
		xdr_argument = xdr_rpcb;
		xdr_result = xdr_bool;
		local_func = (char *(*)()) rpcbproc_unset_3;
		break;

	case RPCBPROC_GETADDR:
#ifdef DEBUG
		fprintf(stderr, "RPCBPROC_GETADDR\n");
#endif
		xdr_argument = xdr_rpcb;
		xdr_result = xdr_wrapstring;
		local_func = (char *(*)()) rpcbproc_getaddr_3;
		break;

	case RPCBPROC_DUMP:
#ifdef DEBUG
		fprintf(stderr, "RPCBPROC_DUMP\n");
#endif
		xdr_argument = xdr_void;
		xdr_result = xdr_rpcblist;
		local_func = (char *(*)()) rpcbproc_dump_3;
		break;

	case RPCBPROC_CALLIT:
#ifdef DEBUG
		fprintf(stderr, "RPCBPROC_CALLIT\n");
#endif
		rpcbproc_callit(rqstp, xprt);
		return;

	case RPCBPROC_GETTIME:
#ifdef DEBUG
		fprintf(stderr, "RPCBPROC_GETTIME\n");
#endif
		xdr_argument = xdr_void;
		xdr_result = xdr_u_long;
		local_func = (char *(*)()) rpcbproc_gettime_3;
		break;

	case RPCBPROC_UADDR2TADDR:
#ifdef DEBUG
		fprintf(stderr, "RPCBPROC_UADDR2TADDR\n");
#endif
		xdr_argument = xdr_wrapstring;
		xdr_result = xdr_netbuf;
		local_func = (char *(*)()) rpcbproc_uaddr2taddr_3;
		break;

	case RPCBPROC_TADDR2UADDR:
#ifdef DEBUG
		fprintf(stderr, "RPCBPROC_TADDR2UADDR\n");
#endif
		xdr_argument = xdr_netbuf;
		xdr_result = xdr_wrapstring;
		local_func = (char *(*)()) rpcbproc_taddr2uaddr_3;
		break;

	default:
		svcerr_noproc(xprt);
		return;
	}
	memset((char *)&argument, 0, sizeof (argument));
	if (!svc_getargs(xprt, xdr_argument, &argument)) {
		svcerr_decode(xprt);
		if (verbose)
			(void) fprintf(stderr, "rpcbind: could not decode\n");
		return;
	}
	result = (*local_func)(&argument, rqstp, xprt);
	if (result != NULL && !svc_sendreply(xprt, xdr_result, result)) {
		svcerr_systemerr(xprt);
		if (debugging) {
			(void) fprintf(stderr, "rpcbind: svc_sendreply\n");
			rpcbind_abort();
		}
	}
	if (!svc_freeargs(xprt, xdr_argument, &argument)) {
		if (debugging) {
			(void) fprintf(stderr, "unable to free arguments\n");
			rpcbind_abort();
		}
	}
	return;
}

/*
 * Set a mapping of program, version, netid
 */
/* ARGSUSED */
static bool_t *
rpcbproc_set_3(regp, rqstp, xprt)
	RPCB *regp;
	struct svc_req *rqstp;	/* Not used here */
	SVCXPRT *xprt;
{
	static bool_t ans;
	char owner[64];

	ans = map_set(regp, getowner(xprt, owner));
	return (&ans);
}

bool_t
map_set(regp, owner)
	RPCB *regp;
	char *owner;
{
	RPCB reg, *a;
	RPCBLIST *rbl, *fnd;

	reg = *regp;
	/*
	 * check to see if already used
	 * find_service returns a hit even if
	 * the versions don't match, so check for it
	 */
	fnd = find_service(reg.r_prog, reg.r_vers, reg.r_netid);
	if (fnd && (fnd->rpcb_map.r_vers == reg.r_vers)) {
		if (!strcmp(fnd->rpcb_map.r_addr, reg.r_addr))
			/*
			 * if these match then it is already
			 * registered so just say "OK".
			 */
			return (TRUE);
		else
			return (FALSE);
	}
	/*
	 * add to the end of the list
	 */
	rbl = (RPCBLIST *) malloc((u_int)sizeof (RPCBLIST));
	if (rbl == (RPCBLIST *)NULL) {
		return (FALSE);
	}
	a = &(rbl->rpcb_map);
	a->r_prog = reg.r_prog;
	a->r_vers = reg.r_vers;
	a->r_netid = strdup(reg.r_netid);
	a->r_addr = strdup(reg.r_addr);
	a->r_owner = strdup(owner);
	if (!a->r_addr || !a->r_netid || !a->r_owner) {
		free((char *)rbl);
		return (FALSE);
	}
	rbl->rpcb_next = (RPCBLIST *)NULL;
	if (list_rbl == NULL) {
		list_rbl = rbl;
	} else {
		for (fnd= list_rbl; fnd->rpcb_next;
			fnd = fnd->rpcb_next);
		fnd->rpcb_next = rbl;
	}
#ifdef PORTMAP
	(void) add_pmaplist(regp);
#endif
	return (TRUE);
}

/*
 * Unset a mapping of program, version, netid
 */
/* ARGSUSED */
static bool_t *
rpcbproc_unset_3(regp, rqstp, transp)
	RPCB *regp;
	struct svc_req *rqstp;	/* Not used here */
	SVCXPRT *transp;
{
	static bool_t ans;
	char owner[64];

	ans = map_unset(regp, getowner(transp, owner));
	return (&ans);
}

bool_t
map_unset(regp, owner)
	RPCB *regp;
	char *owner;
{
	int ans = 0;
	RPCBLIST *rbl, *prev, *tmp;

	if (owner == NULL)
		return (0);

	for (prev = NULL, rbl = list_rbl; rbl; ) {
		if ((rbl->rpcb_map.r_prog != regp->r_prog) ||
			(rbl->rpcb_map.r_vers != regp->r_vers) ||
			(regp->r_netid[0] && strcasecmp(regp->r_netid,
				rbl->rpcb_map.r_netid))) {
			/* both rbl & prev move forwards */
			prev = rbl;
			rbl = rbl->rpcb_next;
			continue;
		}
		/*
		 * Check whether appropriate uid. Unset only
		 * if superuser or the owner itself.
		 */
		if (strcmp(owner, "superuser") &&
			strcmp(rbl->rpcb_map.r_owner, owner))
			return (0);
		/* found it; rbl moves forward, prev stays */
		ans = 1;
		tmp = rbl;
		rbl = rbl->rpcb_next;
		if (prev == NULL)
			list_rbl = rbl;
		else
			prev->rpcb_next = rbl;
		free(tmp->rpcb_map.r_addr);
		free(tmp->rpcb_map.r_netid);
		free(tmp->rpcb_map.r_owner);
		free((char *)tmp);
	}
#ifdef PORTMAP
	if (ans)
		(void) del_pmaplist(regp);
#endif
	return (ans);
}

/*
 * Lookup the mapping for a program, version and return its
 * address. Assuming that the caller wants the address of the
 * server running on the transport on which the request came.
 *
 * We also try to resolve the universal address in terms of
 * address of the caller.
 */
/* ARGSUSED */
static char **
rpcbproc_getaddr_3(regp, rqstp, transp)
	RPCB *regp;
	struct svc_req *rqstp;	/* Not used here */
	SVCXPRT *transp;
{
	static char *uaddr;
	RPCBLIST *fnd;

	if (uaddr && uaddr != nullstring)
		(void) free(uaddr);
	fnd = find_service(regp->r_prog, regp->r_vers, transp->xp_netid);
	if (fnd) {
		if (!(uaddr = mergeaddr(transp, fnd->rpcb_map.r_addr))) {
			/* Try whatever we have */
			uaddr = strdup(fnd->rpcb_map.r_addr);
		} else if (!uaddr[0]) {
			/* the server died. Unset this combination */
			uaddr = nullstring;
			(void) map_unset(regp, "superuser");
		}
	} else {
		uaddr = nullstring;
	}
#ifdef DEBUG
	fprintf(stderr, "getaddr: %s\n", uaddr);
#endif
	return (&uaddr);
}

/* VARARGS */
static RPCBLIST **
rpcbproc_dump_3()
{
	return (&list_rbl);
}

/* VARARGS */
static u_long *
rpcbproc_gettime_3()
{
	static time_t curtime;

	time(&curtime);
	return ((u_long *)&curtime);
}

/*
 * Convert uaddr to taddr. Should be used only by
 * local servers/clients. (kernel level stuff only)
 */
/* ARGSUSED */
static struct netbuf *
rpcbproc_uaddr2taddr_3(uaddrp, rqstp, transp)
	char **uaddrp;
	struct svc_req *rqstp;	/* Not used here */
	SVCXPRT *transp;
{
	struct netconfig *nconf;
	static struct netbuf nbuf;
	static struct netbuf *taddr;

	if (!(nconf = rpcbind_get_conf(transp->xp_netid))) {
		memset((char *)&nbuf, 0, sizeof (struct netbuf));
		return (&nbuf);
	}
	if (taddr) {
		free(taddr->buf);
		free((char *)taddr);
	}
	taddr = uaddr2taddr(nconf, *uaddrp);
	if (taddr == NULL) {
		memset((char *)&nbuf, 0, sizeof (struct netbuf));
		return (&nbuf);
	}
	return (taddr);
}

/*
 * Convert taddr to uaddr. Should be used only by
 * local servers/clients. (kernel level stuff only)
 */
/* ARGSUSED */
static char **
rpcbproc_taddr2uaddr_3(taddr, rqstp, transp)
	struct netbuf *taddr;
	struct svc_req *rqstp;	/* Not used here */
	SVCXPRT *transp;
{
	static char *uaddr;
	struct netconfig *nconf;

	if (uaddr && uaddr != nullstring)
		(void) free(uaddr);
	if (!(nconf = rpcbind_get_conf(transp->xp_netid))) {
		uaddr = nullstring;
		return (&uaddr);
	}
	if (!(uaddr = taddr2uaddr(nconf, taddr)))
		uaddr = nullstring;
	return (&uaddr);
}

/*
 * Stuff for the rmtcall service
 */
struct encap_parms {
	u_int arglen;
	char *args;
};

static bool_t
xdr_encap_parms(xdrs, epp)
	XDR *xdrs;
	struct encap_parms *epp;
{
	return (xdr_bytes(xdrs, &(epp->args), &(epp->arglen), ~0));
}


struct rmtcall_args {
	u_long 	rmt_prog;
	u_long 	rmt_vers;
	u_long 	rmt_proc;
#ifdef PORTMAP
	int	rmt_reply_type;	/* whether to send port # or uaddr */
#endif
	char 	*rmt_uaddr;
	struct encap_parms rmt_args;
};

/*
 * XDR remote call arguments.  It ignores the address part.
 * written for XDR_DECODE direction only
 */
static bool_t
xdr_rmtcall_args(xdrs, cap)
	register XDR *xdrs;
	register struct rmtcall_args *cap;
{
	/* does not get the address or the arguments */
	if (xdr_u_long(xdrs, &(cap->rmt_prog)) &&
	    xdr_u_long(xdrs, &(cap->rmt_vers)) &&
	    xdr_u_long(xdrs, &(cap->rmt_proc))) {
		return (xdr_encap_parms(xdrs, &(cap->rmt_args)));
	}
	return (FALSE);
}

/*
 * XDR remote call results along with the address.  Ignore
 * program number, version  number and proc number.
 * Written for XDR_ENCODE direction only.
 */
static bool_t
xdr_rmtcall_result(xdrs, cap)
	register XDR *xdrs;
	register struct rmtcall_args *cap;
{
	bool_t result;

#ifdef PORTMAP
	if (cap->rmt_reply_type == PMAP_TYPE) {
		int h1, h2, h3, h4, p1, p2;
		u_long port;

		/* interpret the universal address for TCP/IP */
		sscanf(cap->rmt_uaddr, "%d.%d.%d.%d.%d.%d",
			&h1, &h2, &h3, &h4, &p1, &p2);
		port = ((p1 & 0xff) << 8) + (p2 & 0xff);
		result = xdr_u_long(xdrs, &port);
	} else if (cap->rmt_reply_type == RPCB_TYPE)
#endif
		result = xdr_wrapstring(xdrs, &(cap->rmt_uaddr));
#ifdef PORTMAP
	else
		return (FALSE);
#endif
	if (result == TRUE)
		return (xdr_encap_parms(xdrs, &(cap->rmt_args)));
	return (FALSE);
}

/*
 * only worries about the struct encap_parms part of struct rmtcall_args.
 * The arglen must already be set!!
 */
static bool_t
xdr_opaque_parms(xdrs, cap)
	XDR *xdrs;
	struct rmtcall_args *cap;
{
	return (xdr_opaque(xdrs, cap->rmt_args.args, cap->rmt_args.arglen));
}

/*
 * Call a remote procedure service.  This procedure is very quiet when things
 * go wrong.  The proc is written to support broadcast rpc.  In the broadcast
 * case, a machine should shut-up instead of complain, lest the requestor be
 * overrun with complaints at the expense of not hearing a valid reply.
 * When receiving a request and verifying that the service exists, we
 *
 *	receive the request
 *
 *	open a new TLI endpoint on the same transport on which we received
 *	the original request
 *
 *	remember the original request's XID (which requires knowing the format
 *	of the svc_dg_data structure)
 *
 *	forward the request, with a new XID, to the requested service,
 *	remembering the XID used to send this request (for later use in
 *	reassociating the answer with the original request), the requestor's
 *	address, the file descriptor on which the forwarded request is
 *	made and the service's address.
 *
 *	mark the file descriptor on which we anticipate receiving a reply from
 *	the service and one to select for in our private svc_run procedure
 *
 * At some time in the future, a reply will be received from the service to
 * which we forwarded the request.  At that time, we detect that the socket
 * used was for forwarding (by looking through the finfo structures to see
 * whether the fd corresponds to one of those) and call handle_reply() to
 *
 *	receive the reply
 *
 *	bundle the reply, along with the service's universal address
 *
 *	create a SVCXPRT structure and use a version of svc_sendreply
 *	that allows us to specify the reply XID and destination, send the reply
 *	to the original requestor.
 */

/*	begin kludge XXX */
/*
 * This is from .../libnsl/rpc/svc_dg.c, and is the structure that xprt->xp_p2
 * points to (and shouldn't be here - we should know nothing of its structure).
 */
#define	MAX_OPT_WORDS	32
#define	RPC_BUF_MAX	32768	/* can be raised it required */

struct svc_dg_data {
	struct	netbuf optbuf;
	long	opts[MAX_OPT_WORDS];		/* options */
	u_int	su_iosz;			/* size of send.recv buffer */
	u_long	su_xid;				/* transaction id */
	XDR	su_xdrs;			/* XDR handle */
	char	su_verfbody[MAX_AUTH_BYTES];	/* verifier body */
	char	*su_cache;			/* cached data, NULL if none */
};
#define	getbogus_data(xprt) ((struct svc_dg_data *) (xprt->xp_p2))
/*	end kludge XXX	*/

static void
rpcbproc_callit(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	rpcbproc_callit_common(rqstp, transp, RPCB_TYPE);
}

void
rpcbproc_callit_common(rqstp, transp, reply_type)
	struct svc_req *rqstp;
	SVCXPRT *transp;
	int reply_type;	/* whether rpcbind or portmap style reply ? */
{
	register RPCBLIST *rbl;
	struct netconfig *nconf;
	struct rmtcall_args a;
	char *buf_alloc = NULL;
	char *outbuf_alloc = NULL;
	char buf[RPC_BUF_MAX], outbuf[RPC_BUF_MAX];
	struct netbuf *na = (struct netbuf *) NULL;
	struct t_info tinfo;
	struct t_unitdata tu_data;
	struct rpc_msg call_msg;
	struct svc_dg_data *bd;
	int outlen;
	u_int sendsz;
	XDR outxdr;
	AUTH *auth;
	int fd = -1;
	char *uaddr;
	extern int errno, t_errno;

	if (t_getinfo(transp->xp_fd, &tinfo) == -1)
		return;
	if (tinfo.servtype != T_CLTS)
		return;	/* Only datagram type accepted */
	sendsz = _rpc_get_t_size(0, tinfo.tsdu);
	if (sendsz == 0)	/* data transfer not supported */
		return;
	/*
	 * Should be multiple of 4 for XDR.
	 */
	sendsz = ((sendsz + 3) / 4) * 4;

	if (debugging)
		fprintf(stderr, "rpcbproc_callit:  sendsz %d\n", sendsz);

	if (sendsz > RPC_BUF_MAX) {
		buf_alloc = malloc(sendsz);
		if (buf_alloc == NULL) {
			if (verbose)
				fprintf(stderr,
					"rpcbproc_callit:  No Memory!\n");
			return;
		}
		a.rmt_args.args = buf_alloc;
	} else {
		a.rmt_args.args = buf;
	}

	call_msg.rm_xid = 0;	/* For error checking purposes */
	if (!svc_getargs(transp, xdr_rmtcall_args, &a)) {
		if (verbose)
			fprintf(stderr,
			"rpcbproc_callit:  svc_getargs failed\n");
		goto error;
	}

	/*
	 * Disallow calling rpcbind for certain procedures.
	 * Luckily Portmap set/unset/callit also have same procedure numbers.
	 * So, will not check for those.
	 */
	if (a.rmt_prog == RPCBPROG) {
		if ((a.rmt_proc == RPCBPROC_SET) ||
			(a.rmt_proc == RPCBPROC_UNSET) ||
			(a.rmt_proc == RPCBPROC_CALLIT)) {
			if (verbose)
				fprintf(stderr,
"rpcbproc_callit:  calling RPCBPROG procs SET, UNSET, or CALLIT not allowed\n");
			goto error;
		}
	/*
	 * Ideally, we should have called rpcb_service() or pmap_service()
	 * with appropriate parameters instead of going about in a
	 * roundabout manner.  Hopefully, this case should happen rarely.
	 */
	}
#ifdef DEBUG
	fprintf(stderr, "rmtcall for %d %d %d on %s\n", a.rmt_prog, a.rmt_vers,
				a.rmt_proc, transp->xp_netid);
#endif
	rbl = find_service(a.rmt_prog, a.rmt_vers, transp->xp_netid);

	if (rbl == (RPCBLIST *)NULL) {
		if (verbose)
			fprintf(stderr,
"rpcbproc_callit:  didn't find service <prog %u, vers %u, netid %s>\n",
				a.rmt_prog, a.rmt_vers, transp->xp_netid);
		goto error;
	}
	if (verbose) {
			fprintf(stderr,
"rpcbproc_callit:  found service <prog %u, vers %u, netid %s> at uaddr %s\n",
				a.rmt_prog, a.rmt_vers,
				transp->xp_netid, rbl->rpcb_map.r_addr);
	}

	/*
	 * Check whether this is a valid bound entry
	 * or not? [As done in rpcb_getaddr()].
	 */
	if (!(uaddr = mergeaddr(transp, rbl->rpcb_map.r_addr))) {
		/* Try whatever we have */
		uaddr = strdup(rbl->rpcb_map.r_addr);
		if (uaddr == NULL)
			goto error;
	}

	nconf = rpcbind_get_conf(transp->xp_netid);
	if (nconf == (struct netconfig *)NULL) {
		if (verbose)
			fprintf(stderr,
			"rpcbproc_callit: rpcbind_get_conf failed\n");
		goto error;
	}

	/*
	 * We go thru this hoopla of opening up clnt_call() because we want
	 * to be able to set the xid of our choice.
	 */
	if ((fd = t_open(nconf->nc_device, O_RDWR, NULL)) == -1) {
		if (verbose)
			fprintf(stderr,
	"rpcbproc_callit:  couldn't open \"%s\" (errno %d, t_errno %d)\n",
			nconf->nc_device, errno, t_errno);
		goto error;
	}
	if (t_bind(fd, (struct t_bind *) 0,
		(struct t_bind *) 0) == -1) {
		if (verbose)
			fprintf(stderr,
"rpcbproc_callit:  couldn't bind to fd for \"%s\" (errno %d, t_errno %d)\n",
				nconf->nc_device, errno, t_errno);
		goto error;
	}
	bd = getbogus_data(transp);
	call_msg.rm_xid = forward_register(bd->su_xid,
			svc_getrpccaller(transp), fd, uaddr, reply_type);
	if (call_msg.rm_xid == 0) {
		/*
		 * A duplicate request for the slow server.  Lets not
		 * beat on it any more.
		 */

		if (verbose)
			fprintf(stderr,
			"rpcbproc_callit:  duplicate request\n");

		if (uaddr != nullstring)	/* uaddr should not be == 0 */
			free(uaddr);
		goto error;
	}

	if (debugging)
		fprintf(stderr,
		"rpcbproc_callit:  original XID %x, new XID %x\n",
			bd->su_xid, call_msg.rm_xid);

	call_msg.rm_direction = CALL;
	call_msg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
	call_msg.rm_call.cb_prog = a.rmt_prog;
	call_msg.rm_call.cb_vers = a.rmt_vers;
	if (sendsz > RPC_BUF_MAX) {
		outbuf_alloc = malloc(sendsz);
		if (outbuf_alloc == NULL) {
			if (verbose)
				fprintf(stderr,
				"rpcbproc_callit: No memory!\n");
			goto error;
		}
		xdrmem_create(&outxdr, outbuf_alloc, sendsz, XDR_ENCODE);
	} else {
		xdrmem_create(&outxdr, outbuf, sendsz, XDR_ENCODE);
	}
	if (!xdr_callhdr(&outxdr, &call_msg)) {
		if (verbose)
			fprintf(stderr,
			"rpcbproc_callit:  xdr_callhdr failed\n");
		goto error;
	}
	if (!xdr_u_long(&outxdr, &(a.rmt_proc))) {
		if (verbose)
			fprintf(stderr,
			"rpcbproc_callit:  xdr_u_long failed\n");
		goto error;
	}

	if (rqstp->rq_cred.oa_flavor == AUTH_NULL) {
		auth = authnone_create();
	} else if (rqstp->rq_cred.oa_flavor == AUTH_SYS) {
		struct authsys_parms *au;

		au = (struct authsys_parms *)rqstp->rq_clntcred;
		auth = authsys_create(au->aup_machname,
				au->aup_uid, au->aup_gid,
				au->aup_len, au->aup_gids);
		if (auth == NULL) /* fall back */
			auth = authnone_create();
	} else {
		/* we do not support any other authentication scheme */
		if (verbose)
			fprintf(stderr,
"rpcbproc_callit:  oa_flavor != AUTH_NONE and oa_flavor != AUTH_SYS\n");
		goto error;
	}
	if (auth == NULL) {
		if (verbose)
			fprintf(stderr,
		"rpcbproc_callit:  authwhatever_create returned NULL\n");
		goto error;
	}
	if (!AUTH_MARSHALL(auth, &outxdr)) {
		AUTH_DESTROY(auth);
		if (verbose)
			fprintf(stderr,
		"rpcbproc_callit:  AUTH_MARSHALL failed\n");
		goto error;
	}
	AUTH_DESTROY(auth);
	if (!xdr_opaque_parms(&outxdr, &a)) {
		if (verbose)
			fprintf(stderr,
		"rpcbproc_callit:  xdr_opaque_parms failed\n");
		goto error;
	}
	outlen = (int) XDR_GETPOS(&outxdr);
	if (outbuf_alloc)
		tu_data.udata.buf = outbuf_alloc;
	else
		tu_data.udata.buf = outbuf;
	tu_data.udata.len = outlen;
	tu_data.opt.len = 0;

	na = uaddr2taddr(nconf, rbl->rpcb_map.r_addr);
#ifdef ND_DEBUG
	if (!na)
		fprintf(stderr, "\tCouldn't resolve remote address!\n");
#endif
	if (!na)
		goto error;
	tu_data.addr = *na;

	if (t_sndudata(fd, &tu_data) == -1) {
		if (verbose)
			fprintf(stderr,
	"rpcbproc_callit:  t_sndudata failed:  t_errno %d, errno %d\n",
				t_errno, errno);
		goto error;
	}
	goto out;

error:
	if (call_msg.rm_xid != 0)
		/* fd is closed here */
		free_slot_by_xid(call_msg.rm_xid);
	else if (fd != -1)
		t_close(fd);
out:
	if (buf_alloc)
		free(buf_alloc);
	if (outbuf_alloc)
		(void) free(outbuf_alloc);
	if (na)
		netdir_free((char *)na, ND_ADDR);
}

/*
 * svc_myreplyto is like svc_sendreply but also allows the caller to specify
 * the reply xid and destination
 */
static int
svc_myreplyto(xprt, who, xid, xdr_results, a)
	SVCXPRT		*xprt;
	struct netbuf	*who;
	u_long		xid;
	xdrproc_t	xdr_results;
	char		*a;
{
	struct svc_dg_data *bd;

	*(svc_getrpccaller(xprt)) = *who;
	bd = getbogus_data(xprt);
	bd->su_xid = xid;		/* set xid on reply */
	return (svc_sendreply(xprt, xdr_results, a));
}

#define	NFORWARD	128
#define	MAXTIME_OFF	300	/* 5 minutes */

struct finfo {
	int		flag;
#define	FINFO_ACTIVE	0x1
	u_long		caller_xid;
	struct netbuf	*caller_addr;
	u_long		forward_xid;
	int		forward_fd;
	char		*uaddr;
#ifdef PORTMAP
	int		reply_type;
#endif
	time_t		time;
};
static struct finfo	FINFO[NFORWARD];
/*
 * Makes an entry into the FIFO for the given request.
 * If duplicate request, returns a 0, else returns the xid of its call.
 */
static u_long
forward_register(caller_xid, caller_addr, forward_fd, uaddr, reply_type)
	u_long		caller_xid;
	struct netbuf	*caller_addr;
	int		forward_fd;
	char		*uaddr;
	int		reply_type;
{
	int		i;
	int		j = 0;
	time_t		min_time, time_now;
	static u_long	lastxid;
	int		entry = -1;

	min_time = FINFO[0].time;
	time_now = time((time_t *)0);
	/* initialization */
	if (lastxid == 0)
		lastxid = time_now * NFORWARD;

	/*
	 * Check if it is an duplicate entry. Then,
	 * try to find an empty slot.  If not available, then
	 * use the slot with the earliest time.
	 */
	for (i = 0; i < NFORWARD; i++) {
		if (FINFO[i].flag & FINFO_ACTIVE) {
			if ((FINFO[i].caller_xid == caller_xid) &&
#ifdef PORTMAP
				(FINFO[i].reply_type == reply_type) &&
#endif
				(!netbufcmp(FINFO[i].caller_addr, caller_addr))) {
				FINFO[i].time = time((time_t *)0);
				return (0);	/* Duplicate entry */
			} else {
				/* Should we wait any longer */
				if ((time_now - FINFO[i].time) > MAXTIME_OFF)
					free_slot_by_index(i);
			}
		}
		if (entry == -1) {
			if ((FINFO[i].flag & FINFO_ACTIVE) == 0) {
				entry = i;
			} else if (FINFO[i].time < min_time) {
				j = i;
				min_time = FINFO[i].time;
			}
		}
	}
	if (entry != -1) {
		/* use this empty slot */
		j = entry;
	} else {
		(void) free_slot_by_index(j);
	}
	rpcb_rmtcalls++;	/* no of pending calls */
	FINFO[j].flag = FINFO_ACTIVE;
#ifdef PORTMAP
	FINFO[j].reply_type = reply_type;
#endif
	FINFO[j].time = time_now;
	FINFO[j].caller_xid = caller_xid;
	FINFO[j].caller_addr = netbufdup(caller_addr);
	FINFO[j].forward_fd = forward_fd;
	FD_SET(forward_fd, &svc_fdset);
	/*
	 * Though uaddr is not allocated here, it will still be freed
	 * from free_slot_*().
	 */
	FINFO[j].uaddr = uaddr;
	lastxid = lastxid + NFORWARD;
	FINFO[j].forward_xid = lastxid + j;	/* encode slot */
	return (FINFO[j].forward_xid);		/* forward on this xid */
}

static int
forward_find(reply_xid, outxid, uaddrp, caller_addr, flagp)
	u_long		reply_xid;
	u_long		*outxid;
	char		**uaddrp;
	struct netbuf	*caller_addr;
	int		*flagp;
{
	int		i;

	i = reply_xid % NFORWARD;
	if (i < 0)
		i += NFORWARD;
	if ((FINFO[i].flag & FINFO_ACTIVE) &&
	    (FINFO[i].forward_xid == reply_xid)) {
		*caller_addr = *FINFO[i].caller_addr;
		*outxid = FINFO[i].caller_xid;
		*uaddrp = FINFO[i].uaddr;
#ifdef PORTMAP
		*flagp = FINFO[i].reply_type;
#endif
		return (1);
	}
	return (0);
}

static int
free_slot_by_xid(xid)
	u_long xid;
{
	int entry;

	entry = xid % NFORWARD;
	if (entry < 0)
		entry += NFORWARD;
	return (free_slot_by_index(entry));
}

static int
free_slot_by_fd(fd)
	int fd;
{
	int i;

	for (i = 0; i < NFORWARD; i++)
		if ((FINFO[i].forward_fd == fd) &&
			(FINFO[i].flag & FINFO_ACTIVE))
			break;
	if (i == NFORWARD)
		return (0);
	return (free_slot_by_index(i));
}


static int
free_slot_by_index(index)
	int index;
{
	struct finfo	*fi;

	fi = &FINFO[index];
	if (fi->flag & FINFO_ACTIVE) {
		(void) free((char *) fi->caller_addr);
		if (fi->uaddr && fi->uaddr != nullstring) {
			(void) free((char *) fi->uaddr);
		}
		if (fi->forward_fd != -1) {
			t_close(fi->forward_fd);
			FD_CLR(fi->forward_fd, &svc_fdset);
		}
		fi->flag &= ~FINFO_ACTIVE;
		rpcb_rmtcalls--;
		return (1);
	}
	return (0);
}

static int
netbufcmp(n1, n2)
	struct netbuf	*n1, *n2;
{
	return ((n1->len != n2->len) || memcmp(n1->buf, n2->buf, n1->len));
}


static struct netbuf *
netbufdup(ap)
	register struct netbuf  *ap;
{
	register struct netbuf  *np;

	np = (struct netbuf *) malloc(sizeof (struct netbuf) + ap->len);
	if (np) {
		np->maxlen = np->len = ap->len;
		np->buf = ((char *) np) + sizeof (struct netbuf);
		(void) memcpy(np->buf, ap->buf, ap->len);
	}
	return (np);
}

void
my_svc_run()
{
	fd_set readfds;
	fd_set exfds;
	int dtbsize = _rpc_dtbsize();
	int select_ret;
	extern int errno;
	int i;

	for (;;) {
		readfds = svc_fdset;
		exfds   = svc_fdset;
#ifdef DEBUG
		if (debugging) {
			fprintf(stderr, "selecting for read on fd < ");
			for (i = 0; i < dtbsize; i++)
				if (FD_ISSET(i, &readfds))
					fprintf(stderr, "%d ", i);
			fprintf(stderr, ">\n");
		}
#endif
		switch (select_ret = select(dtbsize, &readfds, (fd_set *)0,
			&exfds, (struct timeval *)0)) {
		case -1:
			/*
			 * We ignore all other errors except EBADF.  For all
			 * other errors, we just continue with the assumption
			 * that it was set by the signal handlers (or any
			 * other outside event) and not caused by select().
			 */
			if (errno != EBADF) {
				continue;
			}
			(void) syslog(LOG_ERR, "svc_run: - select failed: %m");
			return;
		case 0:
			continue;
		default:
#ifdef DEBUG
			if (debugging) {
				fprintf(stderr, "select returned read fds < ");
				for (i = 0; i < dtbsize; i++)
					if (FD_ISSET(i, &readfds))
						fprintf(stderr, "%d ", i);
				fprintf(stderr, ">\n");
			}
#endif
			for(i=0; i<dtbsize; i++)
				if (FD_ISSET(i, &exfds)) FD_SET(i, &readfds);

			/*
			 * If we found as many replies on callback fds
			 * as the number of descriptors selectable which
			 * select() returned, there can be no more so we
			 * don't call svc_getreqset.  Otherwise, there
			 * must be another so we must call svc_getreqset.
			 */
			if (select_ret == check_rmtcalls(&readfds))
				continue;
			svc_getreqset(&readfds);
		}
	}
}

static int
check_rmtcalls(readfdsp)
	fd_set *readfdsp;
{
	int i, ncallbacks_found, rmtcalls_pending;

	if (rpcb_rmtcalls == 0)
		return (0);
	for (i = 0, ncallbacks_found = 0, rmtcalls_pending = rpcb_rmtcalls;
		(ncallbacks_found < rmtcalls_pending) && (i < NFORWARD); i++) {
		register struct finfo	*fi = &FINFO[i];

		if ((fi->flag & FINFO_ACTIVE) &&
		    (FD_ISSET(fi->forward_fd, readfdsp))) {
			ncallbacks_found++;
#ifdef DEBUG
			if (debugging)
				fprintf(stderr,
"svc_run:  select on forwarding fd %d - calling handle_reply\n",
					fi->forward_fd);
#endif
			handle_reply(fi->forward_fd);
			FD_CLR(fi->forward_fd, readfdsp);
		}
	}
	return (ncallbacks_found);
}


static void
handle_reply( int fd)
{
	int		inlen;
	struct netbuf	to;
	XDR		reply_xdrs;
	struct rpc_msg	reply_msg;
	struct rpc_err	reply_error;
	int		ok;
	u_long		xto;
	char		*buffer;
	struct t_unitdata	*tr_data;
	int		res;
	char		*uaddr;
	extern int	errno, t_errno;
	int		reply_type;

	tr_data = (struct t_unitdata *) t_alloc(fd, T_UNITDATA,
						T_ADDR | T_UDATA);
	if (tr_data == (struct t_unitdata *) NULL) {
		if (verbose)
			fprintf(stderr,
			"handle_reply:  t_alloc T_UNITDATA failed\n");
		free_slot_by_fd(fd);
		return;
	}
	reply_msg.rm_xid = 0;  /* for easier error handling */
	do {
		int	moreflag;

		moreflag = 0;
		if (errno == EINTR)
			errno = 0;
		res = t_rcvudata(fd, tr_data, &moreflag);
		if (moreflag & T_MORE) {
			/*
			 *	Drop this packet - we have no more space.
			 */
			if (verbose)
				fprintf(stderr,
			"handle_reply:  recvd packet with T_MORE flag set\n");
			goto done;
		}
	} while (res < 0 && errno == EINTR);
	if (res < 0) {
		if (t_errno == TLOOK) {
			if (verbose)
				fprintf(stderr,
	"handle_reply:  t_rcvudata returned %d, t_errno TLOOK\n", res);
			(void) t_rcvuderr(fd, (struct t_uderr *) NULL);
		}
		if (debugging)
			fprintf(stderr,
	"handle_reply:  t_rcvudata returned %d, t_errno %d, errno %d\n",
				res, t_errno, errno);
		goto done;
	}
	inlen = tr_data->udata.len;
	if (debugging)
		fprintf(stderr,
		"handle_reply:  t_rcvudata received %d-byte packet\n", inlen);
	buffer = tr_data->udata.buf;
	if (buffer == (char *) NULL) {
		goto done;
	}

	reply_msg.acpted_rply.ar_verf = _null_auth;
	reply_msg.acpted_rply.ar_results.where = 0;
	reply_msg.acpted_rply.ar_results.proc = xdr_void;

	xdrmem_create(&reply_xdrs, buffer, (u_int)inlen, XDR_DECODE);
	ok = xdr_replymsg(&reply_xdrs, &reply_msg);
	if (!ok) {
		if (verbose)
			(void) fprintf(stderr,
				"handle_reply:  xdr_replymsg failed\n");
		goto done;
	}
	_seterr_reply(&reply_msg, &reply_error);
	if (reply_error.re_status != RPC_SUCCESS) {
		/* We keep quiet in cases of RPC failure */
		if (verbose)
			(void) fprintf(stderr, "handle_reply:  %s\n",
				clnt_sperrno(reply_error.re_status));
		goto done;
	}
	if (forward_find(reply_msg.rm_xid, &xto, &uaddr, &to, &reply_type)) {
		SVCXPRT		*xprt;
		int		pos, len;
		struct rmtcall_args a;

		xprt = svc_tli_create(fd, 0, (struct t_bind *) 0, 0, 0);
		if (xprt == NULL) {
			if (verbose)
				fprintf(stderr,
				"handle_reply:  svc_tli_create failed\n");
			goto done;
		}
		pos = XDR_GETPOS(&reply_xdrs);
		len = inlen - pos;
		a.rmt_args.args = &buffer[pos];
		a.rmt_args.arglen = len;
		a.rmt_uaddr = uaddr;
#ifdef PORTMAP
		a.rmt_reply_type = reply_type;
#endif
		if (verbose)
			fprintf(stderr,
			"handle_reply:  forwarding reply to %s\n", uaddr);
		svc_myreplyto(xprt, &to, xto, xdr_rmtcall_result, (char *) &a);
		xprt->xp_rtaddr.buf = NULL;
		SVC_DESTROY(xprt); /* fd gets destroyed here also */
	} else if (verbose) {
		fprintf(stderr,
		"handle_reply:  forward_find failed for xid %x\n",
			reply_msg.rm_xid);
	}
done:
	t_free((char *) tr_data, T_UNITDATA);
	if (reply_msg.rm_xid == 0)
		free_slot_by_fd(fd);
	else
		free_slot_by_xid(reply_msg.rm_xid);
	return;
}

/*
 * returns the item with the given program, version number and netid.
 * If that version number is not found, it returns the item with that
 * program number, so that address is now returned to the caller. The
 * caller when makes a call to this program, version number, the call
 * will fail and it will return with PROGVERS_MISMATCH. The user can
 * then determine the highest and the lowest version number for this
 * program using clnt_geterr() and use those program version numbers.
 *
 * Returns the rpcblist for the given prog, vers and netid
 */
static RPCBLIST *
find_service(prog, vers, netid)
	u_long prog;	/* Program Number */
	u_long vers;	/* Version Number */
	char *netid;	/* Transport Provider token */
{
	register RPCBLIST *hit = NULL;
	register RPCBLIST *rbl;

	for (rbl = list_rbl; rbl != NULL; rbl = rbl->rpcb_next) {
		if ((rbl->rpcb_map.r_prog != prog) ||
		    ((rbl->rpcb_map.r_netid != NULL) &&
			(strcasecmp(rbl->rpcb_map.r_netid, netid) != 0)))
			continue;
		hit = rbl;
		if (rbl->rpcb_map.r_vers == vers)
			break;
	}
	return (hit);
}

/*
 * Copies the name associated with the uid of the caller and returns
 * a pointer to it.  Similar to getwd().
 */
static char *
getowner(transp, owner)
	SVCXPRT *transp;
	char *owner;
{
	uid_t uid;

	if (_rpc_get_local_uid(transp, &uid) < 0)
		return (strcpy(owner, "unknown"));
	if (uid == 0)
		return (strcpy(owner, "superuser"));
	sprintf(owner, "%u", uid);
	return (owner);
}

#ifdef PORTMAP
/*
 * Add this to the pmap list only if it is UDP or TCP.
 */
static int
add_pmaplist(arg)
	RPCB *arg;
{
	PMAP pmap;
	PMAPLIST *pml;
	int h1, h2, h3, h4, p1, p2;

	if (strcmp(arg->r_netid, udptrans) == 0) {
		/* It is UDP! */
		pmap.pm_prot = IPPROTO_UDP;
	} else if (strcmp(arg->r_netid, tcptrans) == 0) {
		/* It is TCP */
		pmap.pm_prot = IPPROTO_TCP;
	} else
		/* Not a IP protocol */
		return (0);

	/* interpret the universal address for TCP/IP */
	sscanf(arg->r_addr, "%d.%d.%d.%d.%d.%d", &h1, &h2, &h3, &h4, &p1, &p2);
	pmap.pm_port = ((p1 & 0xff) << 8) + (p2 & 0xff);
	pmap.pm_prog = arg->r_prog;
	pmap.pm_vers = arg->r_vers;
	/*
	 * add to END of list
	 */
	pml = (PMAPLIST *) malloc((u_int)sizeof (PMAPLIST));
	if (pml == NULL) {
		(void) syslog(LOG_ERR, "rpcbind: no memory!\n");
		return (1);
	}
	pml->pml_map = pmap;
	pml->pml_next = NULL;
	if (list_pml == NULL) {
		list_pml = pml;
	} else {
		PMAPLIST *fnd;

		/* Attach to the end of the list */
		for (fnd = list_pml; fnd->pml_next; fnd = fnd->pml_next);
		fnd->pml_next = pml;
	}
	return (0);
}

/*
 * Delete this from the pmap list only if it is UDP or TCP.
 */
static int
del_pmaplist(arg)
	RPCB *arg;
{
	register PMAPLIST *pml;
	PMAPLIST *prevpml, *fnd;
	long prot;

	if (strcmp(arg->r_netid, udptrans) == 0) {
		/* It is UDP! */
		prot = IPPROTO_UDP;
	} else if (strcmp(arg->r_netid, tcptrans) == 0) {
		/* It is TCP */
		prot = IPPROTO_TCP;
	} else if (arg->r_netid[0] == NULL) {
		prot = 0;	/* Remove all occurrences */
	} else
		/* Not a IP protocol */
		return (0);
	for (prevpml = NULL, pml = list_pml; pml; ) {
		if ((pml->pml_map.pm_prog != arg->r_prog) ||
			(pml->pml_map.pm_vers != arg->r_vers) ||
			(prot && (pml->pml_map.pm_prot != prot))) {
			/* both pml & prevpml move forwards */
			prevpml = pml;
			pml = pml->pml_next;
			continue;
		}
		/* found it; pml moves forward, prevpml stays */
		fnd = pml;
		pml = pml->pml_next;
		if (prevpml == NULL)
			list_pml = pml;
		else
			prevpml->pml_next = pml;
		free((char *)fnd);
	}
	return (0);
}
#endif /* PORTMAP */
