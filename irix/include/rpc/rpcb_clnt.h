/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)uts-comm:net/rpc/rpcb_clnt.h	1.3"

/*
 *  		PROPRIETARY NOTICE (Combined)
 *  
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *  In addition, portions of such source code were derived from Berkeley
 *  4.3 BSD under license from the Regents of the University of
 *  California.
 *  
 *  
 *  
 *  		Copyright Notice 
 *  
 *  Notice of copyright on this source code product does not indicate 
 *  publication.
 *  
 *  	(c) 1986,1987,1988,1989  Sun Microsystems, Inc.
 *  	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *  	          All rights reserved.
 */

/*
 * rpcb_clnt.h
 * Supplies C routines to get to rpcbind services.
 *
 */

/*
 * Usage:
 *	success = rpcb_set(program, version, nconf, address);
 *	success = rpcb_unset(program, version, nconf);
 *	success = rpcb_getaddr(program, version, nconf, host);
 *	head = rpcb_getmaps(nconf, host);
 *	clnt_stat = rpcb_rmtcall(nconf, host, program, version, procedure,
 *		xdrargs, argsp, xdrres, resp, tout, addr_ptr)
 * 	clnt_stat = rpc_broadcast(program, version, procedure,
 *		xdrargs, argsp,	xdrres, resp, eachresult, nettype)
 *		(like rpcb_rmtcall, except the call is broadcasted to all
 *		locally connected nets.  For each valid response received,
 *		the procedure eachresult is called.  Its form is:
 *		done = eachresult(resp, raddr, netconf)
 *			bool_t done;
 *			caddr_t resp;
 *			struct netbuf *raddr;
 *			struct netconfig *netconf;
 *		where resp points to the results of the call and raddr is the
 *		address if the responder to the broadcast. netconf is the
 *		on which the response came.
 *	success = rpcb_gettime(host, timep)
 *	uaddr = rpcb_taddr2uaddr(nconf, taddr);
 *	taddr = rpcb_uaddr2uaddr(nconf, uaddr);
 */

#ifndef _NET_RPC_RPCB_CLNT_H	/* wrapper symbol for kernel use */
#define _NET_RPC_RPCB_CLNT_H	/* subject to change without notice */

#include <rpc/types.h> /* COMPATIBILITY */
#include <rpc/rpcb_prot.h> /* COMPATIBILITY */

extern bool_t		rpcb_set(u_long, u_long, struct netconfig *,
				struct netbuf *);
extern bool_t		rpcb_unset(u_long, u_long, struct netconfig *);
extern RPCBLIST		*rpcb_getmaps(struct netconfig *, const char *);
extern enum clnt_stat	rpcb_rmtcall(struct netconfig *, const char *,
				u_long, u_long, u_long, 
				xdrproc_t, caddr_t, xdrproc_t, caddr_t,
				struct timeval, struct netbuf *);
extern enum clnt_stat	rpc_broadcast(u_long, u_long, u_long,
				xdrproc_t, void *, xdrproc_t, void *,
				resultproc_t, const char *);
extern bool_t		rpcb_getaddr(u_long, u_long, struct netconfig *, 
				struct netbuf *, const char *);
extern bool_t		rpcb_gettime(const char *, time_t *);
extern char		*rpcb_taddr2uaddr(struct netconfig *, struct netbuf *);
extern struct netbuf	*rpcb_uaddr2taddr(struct netconfig *, char *);

#endif /*!_NET_RPC_RPCB_CLNT_H*/
