/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: sysincludes.h,v $
 * Revision 65.5  1998/12/04 17:26:37  bobo
 * O32 build changes
 *
 * Revision 65.4  1998/02/27  00:15:47  lmc
 * Put the definition of mprot_t inside an #ifndef _KERNEL.  Otherwise
 * it gets doubly defined in the kernel.
 *
 * Revision 65.1  1997/10/24  14:29:52  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:44  jdoak
 * Initial revision
 *
 * Revision 64.1  1997/02/14  19:46:52  dce
 * *** empty log message ***
 *
 * Revision 1.4  1996/04/10  18:12:37  bhim
 * Added sysmacros.h under KERNEL only to avoid redefines of major, minor and
 * makedev.
 *
 * Revision 1.3  1996/04/09  16:51:44  bhim
 * Added sys/sysmacros.h among includes.
 *
 * Revision 1.2  1996/04/06  00:18:02  bhim
 * No Message Supplied
 *
 * Revision 1.1.727.2  1994/06/09  14:15:27  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:27:57  annie]
 *
 * Revision 1.1.727.1  1994/02/04  20:24:35  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:15:39  devsrc]
 * 
 * Revision 1.1.725.2  1994/01/20  18:43:31  annie
 * 	added copyright header
 * 	[1994/01/20  18:39:46  annie]
 * 
 * Revision 1.1.725.1  1993/12/07  17:29:47  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  16:01:05  jaffe]
 * 
 * Revision 1.1.6.2  1993/07/19  19:33:33  zeliff
 * 	HP port of DFS
 * 	[1993/07/19  18:25:57  zeliff]
 * 
 * 	GAMERA Merge : #include <locale.h>
 * 	[1993/04/09  20:37:37  toml]
 * 
 * Revision 1.1.4.3  1993/07/16  20:19:25  kissel
 * 	*** empty log message ***
 * 	[1993/06/21  14:35:09  kissel]
 * 
 * 	Remove typo.
 * 	[1992/10/15  22:04:04  toml]
 * 
 * 	Initial revision for HPUX.
 * 	[1992/10/14  16:57:17  toml]
 * 
 * Revision 1.1.2.2  1993/06/04  15:12:18  kissel
 * 	Initial HPUX RP version.
 * 	[1993/06/03  21:44:35  kissel]
 * 
 * $EndLog$
*/
#ifndef	TRANSARC_OSI_SYSINCLUDES_H
#define	TRANSARC_OSI_SYSINCLUDES_H 1

#include <sys/types.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/cred.h>
#include <stdarg.h>
#include <stddef.h>

#ifdef SGIMIPS
/*  mprot_t is defined in sys/types.h, but is only picked up if
	compiling with _KERNEL.  We need vnode.h to get the vattr
	structure, which means we need sys/vnode.h, which uses
	mprot_t.  Its bad news to have a type defined in more than
	one place, but there isn't much choice.
 */
#ifdef SGIMIPS
#if !defined(_KERNEL) && !defined(_STANDALONE) && !defined(_KMEMUSER)
typedef uchar_t mprot_t;
#endif
#else /* SGIMIPS */
#ifndef _KERNEL
typedef uchar_t mprot_t;
#endif
#endif /* SGIMIPS */

#include <sys/vnode.h>
#include <sys/vfs.h>
#endif

#ifdef	KERNEL
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/mount.h>
#include <sys/systm.h>
#else	/* KERNEL */

#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <strings.h>
#include <locale.h>
	
#endif	/* KERNEL */

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/buf.h>
#include <sys/file.h>

#endif	/* TRANSARC_OSI_SYSINCLUDES_H */

