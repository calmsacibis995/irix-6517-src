#ifndef __RPC_CLNT_H__
#define __RPC_CLNT_H__
#ident "$Revision: 2.16 $"
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
	RPC_UNKNOWNPROTO=17,		/* unkown protocol */

	/*
	 * clnt*_create errors
	 */
	RPC_PMAPFAILURE=14,		/* the pmapper failed in its call */
	RPC_PROGNOTREGISTERED=15,	/* remote program is not registered */
	/*
	 * unspecified error
	 */
	RPC_FAILED=16
};

extern enum clnt_stat callrpc(const char *, u_long, u_long, u_long, 
	xdrproc_t, void *, xdrproc_t, void *);

/*
 * Error info.
 */
struct rpc_err {
	enum clnt_stat re_status;
	union {
		int RE_errno;		/* realated system error */
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
};


/*
 * Client rpc handle.
 * Created by individual implementations, see e.g. rpc_udp.c.
 * Client is responsible for initializing auth, see e.g. auth_none.c.
 */
typedef struct __client_s {
    AUTH		*cl_auth;	/* authenticator */
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
	bool_t          (*cl_control)(struct __client_s *, int, void *);
    } *cl_ops;
    void 		*cl_private;	/* private stuff */
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
#define CLSET_TIMEOUT       1   /* set timeout (timeval) */
#define CLGET_TIMEOUT       2   /* get timeout (timeval) */
#define CLGET_SERVER_ADDR   3   /* get server's address (sockaddr) */
#define	CLGET_FD		6   /* get connections file descriptor */
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
 * Memory based rpc (for speed check and testing)
 * CLIENT *
 * clntraw_create(prog, vers)
 *	u_long prog;
 *	u_long vers;
 */
extern CLIENT *clntraw_create(u_long, u_long);


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
struct ucred;
extern CLIENT	*clntkudp_create(struct sockaddr_in *, u_long, u_long, int,
				 enum kudp_intr, enum kudp_xid, struct ucred *);
extern void	clntkudp_init(CLIENT *, struct sockaddr_in *, int,
			      enum kudp_intr, enum kudp_xid, struct ucred *);
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

#define UDPMSGSIZE	8800	/* rpc imposed limit on udp msg size */
#define RPCSMALLMSGSIZE	400	/* a more reasonable packet size */

#ifdef __cplusplus
}
#endif

#endif /* !__RPC_CLNT_H__ */
