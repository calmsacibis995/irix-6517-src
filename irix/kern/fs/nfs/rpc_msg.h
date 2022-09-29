#ifndef __RPC_RPC_MSG_H__
#define __RPC_RPC_MSG_H__
#ident "$Revision: 2.9 $"
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

/*	@(#)rpc_msg.h	1.2 90/07/17 4.1NFSSRC SMI	*/

/* 
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 *      @(#)rpc_msg.h 1.8 88/02/08 SMI      
 */


/*
 * rpc_msg.h
 * rpc message definition
 */


#include "xdr.h"

#define RPC_MSG_VERSION		((u_long) 2)
#define RPC_SERVICE_PORT	((u_short) 2048)

/*
 * Bottom up definition of an rpc message.
 * NOTE: call and reply use the same overall stuct but
 * different parts of unions within it.
 */

enum msg_type {
	CALL=0,
	REPLY=1
};

enum reply_stat {
	MSG_ACCEPTED=0,
	MSG_DENIED=1
};

enum accept_stat {
	SUCCESS=0,
	PROG_UNAVAIL=1,
	PROG_MISMATCH=2,
	PROC_UNAVAIL=3,
	GARBAGE_ARGS=4,
	SYSTEM_ERR=5
};

enum reject_stat {
	RPC_MISMATCH=0,
	AUTH_ERROR=1
};

/*
 * Reply part of an rpc exchange
 */

/*
 * Reply to an rpc request that was accepted by the server.
 * Note: there could be an error even though the request was
 * accepted.
 */
struct accepted_reply {
	struct opaque_auth	ar_verf;
	enum accept_stat	ar_stat;
	union {
		struct {
			u_long	low;
			u_long	high;
		} AR_versions;
		struct {
#ifdef _SVR4_TIRPC
			char	*where;
#else
			void	*where;
#endif
			xdrproc_t proc;
		} AR_results;
		/* and many other null cases */
	} ru;
#define	ar_results	ru.AR_results
#define	ar_vers		ru.AR_versions
};

/*
 * Reply to an rpc request that was rejected by the server.
 */
struct rejected_reply {
	enum reject_stat rj_stat;
	union {
		struct {
			u_long low;
			u_long high;
		} RJ_versions;
		enum auth_stat RJ_why;  /* why authentication did not work */
	} ru;
#define	rj_vers	ru.RJ_versions
#define	rj_why	ru.RJ_why
};

/*
 * Body of a reply to an rpc request.
 */
struct reply_body {
	enum reply_stat rp_stat;
	union {
		struct accepted_reply RP_ar;
		struct rejected_reply RP_dr;
	} ru;
#define	rp_acpt	ru.RP_ar
#define	rp_rjct	ru.RP_dr
};

/*
 * Body of an rpc request call.
 */
struct call_body {
	u_long cb_rpcvers;	/* must be equal to two */
	u_long cb_prog;
	u_long cb_vers;
	u_long cb_proc;
	struct opaque_auth cb_cred;
	struct opaque_auth cb_verf; /* protocol specific - provided by client */
};

/*
 * The rpc message
 */
struct rpc_msg {
	u_long			rm_xid;
	enum msg_type		rm_direction;
	union {
		struct call_body RM_cmb;
		struct reply_body RM_rmb;
	} ru;
#define	rm_call		ru.RM_cmb
#define	rm_reply	ru.RM_rmb
};
#define	acpted_rply	ru.RM_rmb.ru.RP_ar
#define	rjcted_rply	ru.RM_rmb.ru.RP_dr


/*
 * XDR routine to handle a rpc message.
 */
extern bool_t	xdr_callmsg(XDR *, struct rpc_msg *);

/*
 * XDR routine to pre-serialize the static part of a rpc message.
 */
extern bool_t	xdr_callhdr(XDR *, struct rpc_msg *);

/*
 * XDR routine to handle a rpc reply.
 */
extern bool_t	xdr_replymsg(XDR *, struct rpc_msg *);

/* XDR an opaque authentication struct */
extern bool_t	xdr_opaque_auth(XDR *, struct opaque_auth *);

/* XDR the MSG_ACCEPTED part of a reply message union */
extern bool_t	xdr_accepted_reply(XDR *, struct accepted_reply *);

/* XDR the MSG_DENIED part of a reply message union */
extern bool_t	xdr_rejected_reply(XDR *, struct rejected_reply *);

/*
 * Fills in the error part of a reply message.
 */
extern void	_seterr_reply(struct rpc_msg *, struct rpc_err *e);

#ifdef __cplusplus
}
#endif

#endif /* !__RPC_RPC_MSG_H__ */
