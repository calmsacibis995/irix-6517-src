/* pmap_rmt.h - client interface to pmap rpc service header file - 4.0 */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/* @(#)pmap_rmt.h	2.1 88/07/29 4.0 RPCSRC; from 1.2 88/02/08 SMI */
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

/*
 * Structures and XDR routines for parameters to and replies from
 * the portmapper remote-call-service.
 *
 * Copyright (C) 1986, Sun Microsystems, Inc.
 */

/*
modification history
--------------------
01f,22sep92,rrr  added support for c++
01e,26may92,rrr  the tree shuffle
01d,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01c,24oct90,shl  deleted redundant function declarations. moved clnt_broadcast
		 and pmap_rmtcall prototypes to pmap_clnt.h.
01b,05oct90,shl  added ANSI function prototypes.
                 added copyright notice.
01a,26oct89,hjb  first vxWorks version.
*/

#ifndef __INCpmap_rmth
#define __INCpmap_rmth

#ifdef __cplusplus
extern "C" {
#endif

struct rmtcallargs {
	u_long prog, vers, proc, arglen;
	caddr_t args_ptr;
	xdrproc_t xdr_args;
};

struct rmtcallres {
	u_long *port_ptr;
	u_long resultslen;
	caddr_t results_ptr;
	xdrproc_t xdr_results;
};

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern	  bool_t   xdr_rmtcall_args (XDR *xdrs, struct rmtcallargs *cap);
extern	  bool_t   xdr_rmtcallres (XDR *xdrs, struct rmtcallres *crp);

#else

extern	  bool_t   xdr_rmtcall_args ();
extern	  bool_t   xdr_rmtcallres ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCpmap_rmth */
