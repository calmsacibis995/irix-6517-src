#ifndef __RPC_CLNT_H__
#define __RPC_CLNT_H__
#ident "$Revision: 2.27 $"
/*
 *
 * Copyright 1992, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*	@(#)clnt.h	1.3 90/07/19 4.1NFSSRC SMI	*/

/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 *  1.31 88/02/08 SMI
 */


/*
 * clnt.h - Client side remote procedure call interface.
 */

#ifdef _KERNEL
#include "net/if.h"
#include "netinet/in.h"
#include "sys/socketvar.h"
#include "sys/vsocket.h"
#else
#include <netinet/in.h>
#include <sys/socketvar.h>
#endif

#ifdef _SVR4_TIRPC
#include <rpc/rpc_com.h>
#endif

/*
 * Rpc calls return an enum clnt_stat.  This should be looked at more,
 * since each implementation is required to live with this (implementation
 * independent) list of errors.
 */
enum clnt_stat {
	RPC_SUCCESS=0,			/* call succeeded */
	/*
	 * local errors
	 */
	RPC_CANTENCODEARGS=1,		/* can't encode arguments */
	RPC_CANTDECODERES=2,		/* can't decode results */
	RPC_CANTSEND=3,			/* failure in sending call */
	RPC_CANTRECV=4,			/* failure in receiving result */
	RPC_TIMEDOUT=5,			/* call timed out */
	RPC_INTR=18,			/* call interrupted */
	RPC_UDERROR=23,			/* recv got uderr indication */
	/*
	 * remote errors
	 */
	RPC_VERSMISMATCH=6,		/* rpc versions not compatible */
	RPC_AUTHERROR=7,		/* authentication error */
	RPC_PROGUNAVAIL=8,		/* program not available */
	RPC_PROGVERSMISMATCH=9,		/* program version mismatched */
	RPC_PROCUNAVAIL=10,		/* procedure unavailable */
	RPC_CANTDECODEARGS=11,		/* decode arguments error */
	RPC_SYSTEMERROR=12,		/* generic "other problem" */

	/*
	 * callrpc & clnt_create errors
	 */
	RPC_UNKNOWNHOST=13,		/* unknown host name */
	RPC_UNKNOWNPROTO=17,		/* unknown protocol */
	RPC_UNKNOWNADDR=19,		/* Remote address unknown */
	RPC_NOBROADCAST=21,		/* Broadcasting not supported */

	/*
	 * clnt*_create errors
	 */
	RPC_RPCBFAILURE=14,		/* the pmapper failed in its call */
#define RPC_PMAPFAILURE RPC_RPCBFAILURE /* ABI uses both symbols */
	RPC_PROGNOTREGISTERED=15,	/* remote program is not registered */
	RPC_N2AXLATEFAILURE=22,		/* Name to address translation failed */
	/*
	 * Misc error in the TLI library
	 */
	RPC_TLIERROR=20,

	/*
	 * unspecified error
	 */
	RPC_FAILED=16
};

#ifndef _SVR4_TIRPC
extern enum clnt_stat callrpc(const char *, u_long, u_long, u_long, 
	xdrproc_t, void *, xdrproc_t, void *);
#endif

/*
 * Error info.
 */
struct rpc_err {
	enum clnt_stat re_status;
	union {
		int RE_errno;		/* related system error */
#ifdef _SVR4_TIRPC
		struct {
			int errno;	/* related system error */
			int t_errno;	/* related tli error number */
		} RE_err;
#endif
		enum auth_stat RE_why;	/* why the auth error occurred */
		struct {
			u_long low;	/* lowest verion supported */
			u_long high;	/* highest verion supported */
		} RE_vers;
		struct {		/* maybe meaningful if RPC_FAILED */
			long s1;
			long s2;
		} RE_lb;		/* life boot & debugging only */
	} ru;
#define	re_errno	ru.RE_errno
#define	re_why		ru.RE_why
#define	re_vers		ru.RE_vers
#define	re_lb		ru.RE_lb
#ifdef _SVR4_TIRPC
#define re_terrno       ru.RE_err.t_errno
#else
#define re_terrno       ru.RE_lb.s2
#endif
};

struct __client_s;
struct clnt_ops {
	/* call remote procedure */
	enum clnt_stat	(*cl_call)(struct __client_s *, u_long,
			    xdrproc_t, void *, xdrproc_t,
			    void *, struct timeval);
	/* abort a call */
	void		(*cl_abort)(struct __client_s *);

	/* get specific error code */
	void		(*cl_geterr)(struct __client_s *, struct rpc_err *);

	/* frees results */
	bool_t		(*cl_freeres)(struct __client_s *, xdrproc_t, void *);

	/* destroy this structure */
	void		(*cl_destroy)(struct __client_s *);

	/* the ioctl() of rpc */
	bool_t		(*cl_control)(struct __client_s *, int, void *);
};

/*
 * Client rpc handle.
 * Created by individual implementations, see e.g. rpc_udp.c.
 * Client is responsible for initializing auth, see e.g. auth_none.c.
 */
typedef struct __client_s {
    AUTH		*cl_auth;	/* authenticator */
    struct clnt_ops  	*cl_ops;
#ifndef _SVR4_TIRPC
    void 		*cl_private;	/* private stuff */
#else
    char 		*cl_private;	/* private stuff */
    char		*cl_netid;	/* network token */
    char		*cl_tp;		/* device name */
#endif
} CLIENT;


/*
 * Client-side rpc interface ops
 *
 * Parameter types are:
 *
 */

/*
 * enum clnt_stat
 * CLNT_CALL(rh, proc, xargs, argsp, xres, resp, timeout)
 *	CLIENT *rh;
 *	u_long proc;
 *	xdrproc_t xargs;
 *	void *argsp;
 *	xdrproc_t xres;
 *	void *resp;
 *	struct timeval timeout;
 */
#define	CLNT_CALL(rh, proc, xargs, argsp, xres, resp, timeout)	\
	((*(rh)->cl_ops->cl_call)(rh, proc, xargs, argsp, xres, resp, timeout))
#define	clnt_call(rh, proc, xargs, argsp, xres, resp, timeout)	\
	((*(rh)->cl_ops->cl_call)(rh, proc, xargs, argsp, xres, resp, timeout))

/*
 * void
 * CLNT_ABORT(rh);
 *	CLIENT *rh;
 */
#define	CLNT_ABORT(rh)	((*(rh)->cl_ops->cl_abort)(rh))
#define	clnt_abort(rh)	((*(rh)->cl_ops->cl_abort)(rh))

/*
 * struct rpc_err
 * CLNT_GETERR(rh);
 *	CLIENT *rh;
 */
#define	CLNT_GETERR(rh,errp)	((*(rh)->cl_ops->cl_geterr)(rh, errp))
#define	clnt_geterr(rh,errp)	((*(rh)->cl_ops->cl_geterr)(rh, errp))


/*
 * bool_t
 * CLNT_FREERES(rh, xres, resp);
 *	CLIENT *rh;
 *	xdrproc_t xres;
 *	void *resp;
 */
#define	CLNT_FREERES(rh,xres,resp) ((*(rh)->cl_ops->cl_freeres)(rh,xres,resp))
#define	clnt_freeres(rh,xres,resp) ((*(rh)->cl_ops->cl_freeres)(rh,xres,resp))

/*
 * bool_t
 * CLNT_CONTROL(cl, request, info)
 *      CLIENT *cl;
 *      u_int request;
 *      void *info;
 */
#define	CLNT_CONTROL(cl,rq,in) ((*(cl)->cl_ops->cl_control)(cl,rq,in))
#define	clnt_control(cl,rq,in) ((*(cl)->cl_ops->cl_control)(cl,rq,in))

/*
 * control operations that apply to both UDP and TCP transports
 */
#define CLSET_TIMEOUT		1   /* set timeout (timeval) */
#define CLGET_TIMEOUT		2   /* get timeout (timeval) */
#define CLGET_SERVER_ADDR	3   /* get server's address (sockaddr) */
#define	CLGET_FD		6   /* get connections file descriptor */
#define CLGET_SVC_ADDR		7   /* get server's address (netbuf) */
#define	CLSET_FD_CLOSE		8   /* close fd while clnt_destroy */
#define	CLSET_FD_NCLOSE		9   /* Do not close fd while clnt_destroy */
/*
 * UDP only control operations
 */
#define CLSET_RETRY_TIMEOUT 4   /* set retry timeout (timeval) */
#define CLGET_RETRY_TIMEOUT 5   /* get retry timeout (timeval) */
/*
 * TCP only control operations
 */
#define CLSET_EINTR_RETURN  106	/* set != 0 to return on EINTR */
#define CLGET_EINTR_RETURN  107	/* get return on EINTR flag */

/*
 * void
 * CLNT_DESTROY(rh);
 *	CLIENT *rh;
 */
#define	CLNT_DESTROY(rh)	((*(rh)->cl_ops->cl_destroy)(rh))
#define	clnt_destroy(rh)	((*(rh)->cl_ops->cl_destroy)(rh))


/*
 * RPCTEST is a test program which is accessable on every rpc
 * transport/port.  It is used for testing, performance evaluation,
 * and network administration.
 */

#define RPCTEST_PROGRAM		((u_long)1)
#define RPCTEST_VERSION		((u_long)1)
#define RPCTEST_NULL_PROC	((u_long)2)
#define RPCTEST_NULL_BATCH_PROC	((u_long)3)

/*
 * By convention, procedure 0 takes null arguments and returns them
 */

#define NULLPROC ((u_long)0)

/*
 * Below are the client handle creation routines for the various
 * implementations of client side rpc.  They can return NULL if a
 * creation failure occurs.
 */

#ifndef _KERNEL
/*
 * Generic client creation routine. Supported protocols are "udp" and "tcp"
 *	char *host;	-- hostname
 *	u_long prog;	-- program number
 *	u_long vers;	-- version number
 *	char *prot;	-- protocol
 */
extern CLIENT * clnt_create(const char *, u_long, u_long, const char *);

/*
 * Generic client creation routine. Supported protocols are "udp" and "tcp"
 *
 *	char *host; 	-- hostname
 *	u_long prog;	-- program number
 *	u_long *vers_out;	-- servers best  version number
 *	u_long vers_low;	-- low version number
 *	u_long vers_high;	-- high version number
 *	char *prot;	-- protocol
 */
extern CLIENT *clnt_create_vers(
	const char *, u_long, u_long *, u_long, u_long, const char *);

#ifndef _SVR4_TIRPC
/*
 * Memory based rpc (for speed check and testing)
 * CLIENT *
 * clntraw_create(prog, vers)
 *	u_long prog;
 *	u_long vers;
 */
extern CLIENT *clntraw_create(u_long, u_long);

/*
 * TCP-based rpc
 * CLIENT *
 * clnttcp_create(raddr, prog, vers, sockp, sendsz, recvsz)
 *	struct sockaddr_in *raddr;
 *	u_long prog;
 *	u_long version;
 *	register int *sockp;
 *	u_int sendsz;
 *	u_int recvsz;
 */

struct sockaddr_in;
extern CLIENT *clnttcp_create(
	struct sockaddr_in *, u_long, u_long, int *, u_int, u_int);

/*
 * UDP based rpc.
 * CLIENT *
 * clntudp_create(raddr, program, version, wait, sockp)
 *	struct sockaddr_in *raddr;
 *	u_long program;
 *	u_long version;
 *	struct timeval wait;
 *	int *sockp;
 *
 * Same as above, but you specify max packet sizes.
 * CLIENT *
 * clntudp_bufcreate(raddr, program, version, wait, sockp, sendsz, recvsz)
 *	struct sockaddr_in *raddr;
 *	u_long program;
 *	u_long version;
 *	struct timeval wait;
 *	int *sockp;
 *	u_int sendsz;
 *	u_int recvsz;
 */
extern CLIENT *clntudp_create(
	struct sockaddr_in *, u_long, u_long, struct timeval, int *);
extern CLIENT *clntudp_bufcreate(
	struct sockaddr_in *, u_long, u_long, struct timeval, int *,
	u_int, u_int);
#else /* _SVR4_TIRPC */

struct netconfig;
struct netbuf;
/*
 * Generic client creation routine. It takes a netconfig structure
 * instead of nettype
 */
extern CLIENT *
clnt_tp_create(
	const char *hostname,		/* hostname		*/
	u_long prog,			/* program number	*/
	u_long vers,			/* version number	*/
	struct netconfig *netconf); 	/* network config structure	*/

/*
 * Generic TLI create routine
 */
extern CLIENT *
clnt_tli_create(
	register int fd,		/* fd			*/
	struct netconfig *nconf,	/* netconfig structure	*/
	struct netbuf *svcaddr,		/* servers address	*/
	u_long prog,			/* program number	*/
	u_long vers,			/* version number	*/
	u_int sendsz,			/* send size		*/
	u_int recvsz);			/* recv size		*/

/*
 * Low level clnt create routine for connectionful transports, e.g. tcp.
 */
extern CLIENT *
clnt_vc_create(
	int fd,				/* open file descriptor	*/
	struct netbuf *svcaddr,		/* servers address	*/
	u_long prog,			/* program number	*/
	u_long vers,			/* version number	*/
	u_int sendsz,			/* buffer recv size	*/
	u_int recvsz);			/* buffer send size	*/

/*
 * Low level clnt create routine for connectionless transports, e.g. udp.
 */
extern CLIENT *
clnt_dg_create(
	int fd,				/* open file descriptor	*/
	struct netbuf *svcaddr,		/* servers address	*/
	u_long program,			/* program number	*/
	u_long version,			/* version number	*/
	u_int sendsz,			/* buffer recv size	*/
	u_int recvsz);			/* buffer send size	*/

/*
 * Memory based rpc (for speed check and testing)
 * CLIENT *
 * clnt_raw_create(prog, vers)
 *	u_long prog;			-- program number
 *	u_long vers;			-- version number
 */
extern CLIENT *clnt_raw_create(u_long, u_long);

/*
 * The simplified interface:
 * enum clnt_stat
 * rpc_call(host, prognum, versnum, procnum, inproc, in, outproc, out, nettype)
 *	char *host;
 *	u_long prognum, versnum, procnum;
 *	xdrproc_t inproc, outproc;
 *	char *in, *out;
 *	char *nettype;
 */
extern enum clnt_stat rpc_call(
	const char *host,               /* host name */
	u_long prognum,                 /* program number */
	u_long versnum,                 /* version number */
	u_long procnum,                 /* procedure number */
	xdrproc_t inproc,               /* int XDR procedures */
	char *in,                       /* recv/send data */
	xdrproc_t outproc,              /* out XDR procedures */
	char *out,                      /* recv/send data */
	char *nettype);                 /* nettype */

/*
 * RPC broadcast interface
 * extern enum clnt_stat
 * rpc_broadcast(prog, vers, proc, xargs, argsp, xresults, resultsp,
 *			eachresult, nettype)
 *	u_long		prog;		-- program number
 *	u_long		vers;		-- version number
 *	u_long		proc;		-- procedure number
 *	xdrproc_t	xargs;		-- xdr routine for args
 *	caddr_t		argsp;		-- pointer to args
 *	xdrproc_t	xresults;	-- xdr routine for results
 *	caddr_t		resultsp;	-- pointer to results
 *	resultproc_t	eachresult;	-- call with each result obtained
 *	char		*nettype;	-- Transport type
 */
typedef bool_t (*resultproc_t)(caddr_t, struct netbuf *, struct netconfig *);
extern enum clnt_stat rpc_broadcast(u_long, u_long, u_long,
				xdrproc_t, void *, xdrproc_t, void *,
				resultproc_t, const char *);

#endif /* _SVR4_TIRPC */

/*
 * Print why creation failed
 */
extern void clnt_pcreateerror(const char *);	/* outputs to stderr */
extern char *clnt_spcreateerror(const char *);

/*
 * Like clnt_perror(), but is more verbose in its output
 */
extern void clnt_perrno(enum clnt_stat);	/* outputs to stderr */

/*
 * Print an error message, given the client error code
 */
extern void clnt_perror(CLIENT *, const char *);	/* outputs to stderr */
extern char *clnt_sperror(CLIENT *, const char *);

#endif /* !_KERNEL */

/*
 * Copy error message to buffer.
 */
extern char *clnt_sperrno(enum clnt_stat);

/*
 * If a creation fails, the following allows the user to figure out why.
 */
struct rpc_createerr {
	enum clnt_stat cf_stat;
	struct rpc_err cf_error; /* useful when cf_stat == RPC_PMAPFAILURE */
};

extern struct rpc_createerr rpc_createerr;


#ifdef _KERNEL
/*
 * Kernel UDP-based rpc
 */
enum kudp_intr { KUDP_NOINTR, KUDP_INTR };
enum kudp_xid { KUDP_XID_SAME, KUDP_XID_CREATE, KUDP_XID_PERCALL };

struct sockaddr_in;
struct cred;
struct mac_label;
extern CLIENT	*clntkudp_create(struct sockaddr_in *, u_long, u_long, int,
				 enum kudp_intr, enum kudp_xid, struct cred *);
extern int 	clntkudp_init(CLIENT *, struct sockaddr_in *, int,
			      enum kudp_intr, enum kudp_xid, struct cred *);
extern void	clntkudp_soattr(CLIENT *, struct mac_label *);
extern int	getport_loop(struct sockaddr_in *, u_long, u_long, u_long, int);
extern int	ku_sendto_mbuf(struct socket *, struct mbuf *, struct sockaddr_in *);
extern struct mbuf *ku_recvfrom(struct socket *, struct sockaddr_in *,
				struct mac_label **);
extern int	bindresvport(struct vsocket *);
extern void	clfree(CLIENT *);
#endif

/*
 * Timers used for the pseudo-transport protocol when using datagrams
 */
struct rpc_timers {
	u_short		rt_srtt;	/* smoothed round-trip time */
	u_short		rt_deviate;	/* estimated deviation */
	u_long		rt_rtxcur;	/* current (backed-off) rto */
};

/*
 * Feedback values used for possible congestion and rate control
 */
#define	FEEDBACK_REXMIT1	1	/* first retransmit */
#define	FEEDBACK_OK		2	/* no retransmits */

#define UDPMSGSIZE	8800	/* legacy limit on UDP */
#define RPCSMALLMSGSIZE	400	/* a more reasonable packet size */

#ifdef PORTMAP
/* For backward compatibility */
#include <rpc/clnt_soc.h> /* COMPATIBILITY */
#endif /* PORTMAP */

#ifdef __cplusplus
}
#endif

#endif /* !__RPC_CLNT_H__ */
