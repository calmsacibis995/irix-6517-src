/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)librpc:rpc_comdata.c	1.2.4.1"

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*	PROPRIETARY NOTICE (Combined)
*
* This source code is unpublished proprietary information
* constituting, or derived under license from AT&T's UNIX(r) System V.
* In addition, portions of such source code were derived from Berkeley
* 4.3 BSD under license from the Regents of the University of
* California.
*
*
*
*	Copyright Notice 
*
* Notice of copyright on this source code product does not indicate 
*  publication.
*
*	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
*	(c) 1983,1984,1985,1986,1987,1988,1989,1990  AT&T.
*	(c) 1990,1991  UNIX System Laboratories, Inc.
*          All rights reserved.
*/ 
#if defined(__STDC__) 
        #pragma weak rpc_createerr	= _rpc_createerr
        #pragma weak svc_fdset		= _svc_fdset
#endif
#include <libc_synonyms.h>
#include <libnsl_synonyms.h>

/*
 * This file should only contain common data (global data) that is exported
 * by public interfaces
 */
#include <rpc/rpc.h>

struct opaque_auth _null_auth;
fd_set svc_fdset = { 0 };
struct rpc_createerr rpc_createerr = { 0 };
void (*_svc_getreqset_proc)();
char *_rawcombuf;
