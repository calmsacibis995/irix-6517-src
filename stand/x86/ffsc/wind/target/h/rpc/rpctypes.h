/* rpctypes.h - Rpc additions to <sys/types.h> */

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
/*      @(#)types.h 1.1 86/02/03 SMI      */

/*
modification history
--------------------
02i,22sep92,rrr  added support for c++
02h,19aug92,smb  changed systime.h to sys/times.h
02g,04jul92,smb  added include vxWorks.h
02f,26may92,rrr  the tree shuffle
02e,04oct91,rrr  passed through the ansification filter
		  -changed includes to have absolute path from h/
		  -fixed #else and #endif
		  -changed copyright notice
02d,25oct90,dnw  added definitions of TRUE and FALSE if not already defined.
		 changed utime.h to systime.h.
02c,05oct90,shl  added copyright notice.
02b,29oct89,hjb  upgraded to release 4.0
*/

#ifndef __INCrpctypesh
#define __INCrpctypesh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"

/*
 * Rpc additions to <sys/types.h>
 */

#define	bool_t	int
#define	enum_t	int

#ifndef	FALSE
#define	FALSE	(0)
#endif

#ifndef	TRUE
#define	TRUE	(1)
#endif

#define __dontcare__	-1

#define mem_alloc(bsize)	malloc(bsize)
#define mem_free(ptr, bsize)	free(ptr)

#ifndef _TYPES_		/* if types.h is not include yet, include it here */
#include "sys/types.h"
#endif

#include "sys/times.h"			/* 4.0 */

#ifndef INADDR_LOOPBACK			/* 4.0 */
#define 	INADDR_LOOPBACK		((u_long) 0x7F000001)
#endif

#ifndef MAXHOSTNAMELEN			/* 4.0 */
#define		MAXHOSTNAMELEN		64
#endif

#ifdef __cplusplus
}
#endif

#endif /* __INCrpctypesh */
