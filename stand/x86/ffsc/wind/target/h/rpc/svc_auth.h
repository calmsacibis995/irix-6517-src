/* svc_auth.h - Service side of rpc authentication. */

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
/*      @(#)svc_auth.h 1.1 86/02/03 SMI      */

/*
 * svc_auth.h, Service side of rpc authentication.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

/*
modification history
--------------------
01g,22sep92,rrr  added support for c++
01f,26may92,rrr  the tree shuffle
01e,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01d,24oct90,shl  deleted redundant function declarations.
01c,05oct90,shl  added ANSI function prototypes.
                 added copyright notice.
01b,27oct89,hjb  added modification history and #ifndef's to avoid multiple
		 inclusion.
*/

#ifndef __INCsvc_authh
#define __INCsvc_authh

#ifdef __cplusplus
extern "C" {
#endif


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern	  enum auth_stat  _authenticate (struct svc_req *rqst,
					 struct rpc_msg *msg);
extern	  enum auth_stat  _svcauth_null (void);

#else

extern	  enum auth_stat  _authenticate ();
extern	  enum auth_stat  _svcauth_null ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCsvc_authh */
