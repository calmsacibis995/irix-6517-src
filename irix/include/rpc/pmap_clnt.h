#ifndef __RPC_PMAP_CLNT_H__
#define __RPC_PMAP_CLNT_H__
#ident "$Revision: 2.12 $"
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

/*	@(#)pmap_clnt.h	1.2 90/07/17 4.1NFSSRC SMI	*/

/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 *	@(#)pmap_clnt.h 1.11 88/02/08 SMI
 */


/*
 * pmap_clnt.h
 * Supplies C routines to get to portmap services.
 */

/*
 * Usage:
 *	success = pmap_set(program, version, protocol, port);
 *	success = pmap_unset(program, version);
 *	port = pmap_getport(address, program, version, protocol);
 *	head = pmap_getmaps(address);
 *	clnt_stat = pmap_rmtcall(address, program, version, procedure,
 *			xdrargs, argsp, xdrres, resp, tout, port_ptr)
 *		works for UDP only.
 *	clnt_stat = clnt_broadcast(program, version, procedure,
 *			xdrargs, argsp,	xdrres, resp, eachresult)
 *		like pmap_rmtcall, except the call is broadcasted to all
 *		locally connected nets.  For each valid response received,
 *		the procedure eachresult is called.  Its form is:
 *			done = eachresult(resp, raddr)
 *				bool_t done;
 *				void * resp;
 *				struct sockaddr_in *raddr;
 *		where resp points to the results of the call and raddr contains
 *		the IP address (raddr->sin_addr) of the responder to the
 *		broadcast.
 */

extern bool_t		pmap_set(u_long, u_long, u_int, u_short);
extern bool_t		pmap_unset(u_long, u_long);
extern struct pmaplist	*pmap_getmaps(struct sockaddr_in *);
extern u_short		pmap_getport(struct sockaddr_in *, u_long,u_long,u_int);
extern enum clnt_stat	pmap_rmtcall(struct sockaddr_in *, u_long,u_long,u_long,
			    xdrproc_t, void *, xdrproc_t, void *,
			    struct timeval, u_long *);
extern enum clnt_stat	clnt_broadcast(u_long, u_long, u_long,
			    xdrproc_t, void *, xdrproc_t, void *,
			    bool_t (*)(void *, struct sockaddr_in *));
extern enum clnt_stat	clnt_multicast(u_long, u_long, u_long,
			    xdrproc_t, void *, xdrproc_t, void *,
			    bool_t (*)(void *, struct sockaddr_in *));
extern enum clnt_stat	clnt_broadmulti(u_long, u_long, u_long,
			    xdrproc_t, void *, xdrproc_t, void *,
			    bool_t (*)(void *, struct sockaddr_in *),
			    u_char, u_char);
extern enum clnt_stat	clnt_broadcast_exp(u_long, u_long, u_long,
			    xdrproc_t, void *, xdrproc_t, void *,
			    bool_t (*)(void *, struct sockaddr_in *), int,int);
extern enum clnt_stat	clnt_multicast_exp(u_long, u_long, u_long,
			    xdrproc_t, void *, xdrproc_t, void *,
			    bool_t (*)(void *, struct sockaddr_in *), int,int);
extern enum clnt_stat	clnt_broadmulti_exp(u_long, u_long, u_long,
			    xdrproc_t, void *, xdrproc_t, void *,
			    bool_t (*)(void *, struct sockaddr_in *), int,int,
			    u_char, u_char);

/*
 * Set the retry and total timeouts for RPCs to the portmapper.  These
 * timeouts are used explicitly by pmap_set() and pmap_getport(), and
 * implicitly by clnt*_create().
 */
extern void	pmap_settimeouts(struct timeval, struct timeval);

/*
 * Set the retry timeout for clnt_rmtcall().  Note that the total timeout
 * per call is an argument to clnt_rmtcall().
 */
extern void	pmap_setrmtcalltimeout(struct timeval);

/*
 * Set the timeout backoff iterator for clnt_broadcast().  The initial timeout,
 * timeo, is stored in *tv by first().  Subsequent timeouts are computed in
 * *tv by next(), which returns 1 until the backoff limit, wait, is reached,
 * and which thereafter returns 0.
 *	clnt_setbroadcastbackoff(first, next)
 *		void (*first)(struct timeval *tv, int timeo);
 *		int (*next)(struct timeval *tv, int wait);
 */
extern void	clnt_setbroadcastbackoff(
			void (*)(struct timeval *, int),
			int (*)(struct timeval *, int));

#ifdef __cplusplus
}
#endif

#endif /* !__RPC_PMAP_CLNT_H__ */
