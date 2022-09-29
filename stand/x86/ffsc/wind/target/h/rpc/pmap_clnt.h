/* portmap_clnt.h - Supplies C routines to get to portmap services. */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */
/*	@(#)pmap_clnt.h 1.1 86/02/03 SMI	*/

/*
 * portmap_clnt.h
 * Supplies C routines to get to portmap services.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

/*
modification history
--------------------
01h,22sep92,rrr  added support for c++
01g,26may92,rrr  the tree shuffle
01f,04oct91,rrr  passed through the ansification filter
		  -fixed broken prototype
		  -fixed #else and #endif
		  -changed copyright notice
01e,21jan91,shl  fxied typo in prototypes.
01d,24oct90,shl  deleted redundant function declarations.
01c,05oct90,shl  added ANSI function prototypes.
                 added copyright notice.
01b,27oct89,hjb  added modification history and #ifndef's to avoid multiple
		 inclusion.
*/

#ifndef __INCpmap_clnth
#define __INCpmap_clnth

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Usage:
 *	success = pmap_set(program, version, protocol, port);
 *	success = pmap_unset(program, version);
 *	port = pmap_getport(address, program, version, protocol);
 *	head = pmap_getmaps(address);
 *	clnt_stat = pmap_rmtcall(address, program, version, procedure,
 *		xdrargs, argsp, xdrres, resp, tout, port_ptr)
 *		(works for udp only.)
 * 	clnt_stat = clnt_broadcast(program, version, procedure,
 *		xdrargs, argsp,	xdrres, resp, eachresult)
 *		(like pmap_rmtcall, except the call is broadcasted to all
 *		locally connected nets.  For each valid response received,
 *		the procedure eachresult is called.  Its form is:
 *	done = eachresult(resp, raddr)
 *		bool_t done;
 *		caddr_t resp;
 *		struct sockaddr_in raddr;
 *		where resp points to the results of the call and raddr is the
 *		address if the responder to the broadcast.
 */


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern	  bool_t       pmap_set (u_long program, u_long version,
				 u_long protocol, int port);
extern	  bool_t       pmap_unset (u_long program, u_long version);
extern	  struct       pmaplist *pmap_getmaps (struct sockaddr_in *address);
extern	  u_short      pmap_getport (struct sockaddr_in *address, u_long
				     program, u_long version, u_long protocol);
extern	  enum clnt_stat pmap_rmtcall (struct sockaddr_in *addr, u_long prog,
		       		u_long vers, u_long proc, xdrproc_t xdrargs,
				caddr_t argsp, xdrproc_t xdrres, caddr_t resp,
				struct timeval tout, u_long *port_ptr);
extern    enum clnt_stat clnt_broadcast (u_long prog, u_long vers, u_long proc,
				xdrproc_t xargs, caddr_t argsp,
				xdrproc_t xresults, caddr_t resultsp,
				bool_t (*eachresult)());


#else

extern	  bool_t       pmap_set ();
extern	  bool_t       pmap_unset ();
extern	  struct       pmaplist *pmap_getmaps ();
extern	  u_short      pmap_getport ();
extern    enum clnt_stat	pmap_rmtcall();
extern    enum clnt_stat	clnt_broadcast();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCpmap_clnth */
