/* rpc.h - remote procedure call header */

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
/*	@(#)rpc.h 1.1 86/02/03 SMI	*/

/*
 * rpc.h, Just includes the billions of rpc header files necessary to
 * do remote procedure calling.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

/*
modification history
--------------------
01h,22sep92,rrr  added support for c++
01g,26may92,rrr  the tree shuffle
		  -changed includes to have absolute path from h/
01f,19nov91,rrr  shut up some ansi warnings.
01e,04oct91,rrr  passed through the ansification filter
		  -changed includes to have absolute path from h/
		  -fixed #else and #endif
		  -changed copyright notice
01d,05oct90,shl  added copyright notice.
01c,26oct89,hjb  upgraded to release 4.0 and changed __RPC_HEADER__ to
		 INCrpch.
01b,22jun88,dnw  added __RPC_HEADER__ test to prevent multiple includes.
*/

#ifndef __INCrpch
#define __INCrpch

#ifdef __cplusplus
extern "C" {
#endif

#include "rpc/rpctypes.h"		/* some typedefs */
#include "netinet/in.h"

/* external data representation interfaces */
#include "rpc/xdr.h"		/* generic (de)serializer */

/* Client side only authentication */
#include "rpc/auth.h"		/* generic authenticator (client side) */

/* Client side (mostly) remote procedure call */
#include "rpc/clnt.h"		/* generic rpc stuff */

/* semi-private protocol headers */
#include "rpc/rpc_msg.h"	/* protocol for rpc messages */
#include "rpc/auth_unix.h"	/* protocol for unix style cred */

/*
 * uncomment-out the next line if you are building the rpc library with
 * DES authentication (see the README file in the secure_rpc/ directory).
 */
/* #include "auth_des.h" */				/* 4.0 */


/* Server side only remote procedure callee */
#include "rpc/svc.h"		/* service manager and multiplexer */
#include "rpc/svc_auth.h"	/* service side authenticator */

/*
 * comment out the next include if the running on sun OS or on a version
 * of Unix based on NFSSRC.  These systems will already have the structures
 * defined by "rpcnetdb.h" included in "netdb.h"
 *
 * VxWorks doesn't support this.
 */
/* routines for parsing /etc/rpc */				/* 4.0 */
/* #include "rpcnetdb.h" */	/* structures and routines to parse /etc/rpc */

#ifdef __cplusplus
}
#endif

#endif /* __INCrpch */
