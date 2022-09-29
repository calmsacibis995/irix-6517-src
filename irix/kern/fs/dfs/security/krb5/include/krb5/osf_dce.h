/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: osf_dce.h,v $
 * Revision 65.4  1999/05/04 19:19:32  gwehrman
 * Replaced code use for non-kernel builds.  This code is still used when
 * building the user space libraries.  Fix for bug 691629.
 *
 * Revision 65.3  1999/02/05 15:44:21  mek
 * Fix reference to pthread.h
 *
 * Revision 65.2  1997/10/24 22:03:13  gwehrman
 * Changed include for pthread.h
 * to dce/pthread.h.
 *
 * Revision 65.1  1997/10/20  19:47:47  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.4.2  1996/02/18  23:02:55  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:21:28  marty]
 *
 * Revision 1.1.4.1  1995/12/08  17:43:41  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/08  16:57:19  root]
 * 
 * Revision 1.1.2.2  1992/12/29  14:01:25  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/28  20:53:40  zeliff]
 * 
 * Revision 1.1  1992/01/19  14:48:10  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/

/*  osf_dce.h V=6 04/30/91 //littl/prgy/krb5/include/krb5
**
** Copyright (c) Hewlett-Packard Company 1991
** Unpublished work. All Rights Reserved.
**
*/
/* 
** 
** Make sure code builds correctly in the DCE environment.  In particular
** the kerberos code will be running in a PTHREAD environment so make sure
** pthread.h is included.  The reference implementation of the DCE uses
** CMA - so make sure the CMA wrappers for stdio functions are also used.
*/

#ifndef krb_osf_dce_h__included
#define krb_osf_dce_h__included

/*
 * Include cma library clib wrapper definitions.  This will provide a
 * multi-thread safe version of clib functions.
 */
#ifdef SGIMIPS
#ifndef _KERNEL
#include <dce/pthread.h>
#endif /* !_KERNEL */
#else /* SGIMIPS */
#include <pthread.h>
#endif /* SGIMIPS */
#ifdef CMA_INCLUDE
#   include <dce/cma_stdio.h>
#endif /* CMA_INCLUDE */

#endif  /* krb_osf_dce_h__included */
