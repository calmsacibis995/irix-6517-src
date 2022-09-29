/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_uio_mach.h,v $
 * Revision 65.1  1997/10/24 14:29:52  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:44  jdoak
 * Initial revision
 *
 * Revision 64.1  1997/02/14  19:46:51  dce
 * *** empty log message ***
 *
 * Revision 1.3  1996/05/03  02:14:40  bhim
 * Added define for osi_uio_limit.
 *
 * Revision 1.2  1996/04/06  00:17:54  bhim
 * No Message Supplied
 *
 * Revision 1.1.823.2  1994/06/09  14:15:23  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:27:54  annie]
 *
 * Revision 1.1.823.1  1994/02/04  20:24:27  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:15:36  devsrc]
 * 
 * Revision 1.1.821.2  1993/12/10  19:47:39  jaffe
 * 	Make sure that we initialze the syncronous bit on uio buffer to 0
 * 	[1993/12/10  19:47:17  jaffe]
 * 
 * Revision 1.1.821.1  1993/12/07  17:29:42  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  16:00:16  jaffe]
 * 
 * Revision 1.1.2.4  1993/01/21  14:50:14  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  14:52:08  cjd]
 * 
 * Revision 1.1.2.3  1992/09/25  18:32:14  jaffe
 * 	Remove duplicate HEADER and LOG entries
 * 	[1992/09/25  12:27:48  jaffe]
 * 
 * Revision 1.1.2.2  1992/08/31  20:23:55  jaffe
 * 	Transarc delta: jaffe-ot4719-cleanup-gcc-Wall-in-osi 1.3
 * 	  Selected comments:
 * 	    Fixed many compiler warnings in the osi directory.
 * 	    Reworked ALL of the header files.  All files in the osi directory now
 * 	    have NO machine specific ifdefs.  All machine specific code is in the
 * 	    machine specific subdirectories.  To make this work, additional flags
 * 	    were added to the afs/param.h file so that we can tell if a particular
 * 	    platform has any additional changes for a given osi header file.
 * 	    new file for machine specific additions for the osi_uio.h file.
 * 	    Corrected errors that appeared while trying to build everything on AIX3.2
 * 	    cleanup for OSF1 compilation
 * 	[1992/08/30  03:14:12  jaffe]
 * 
 * Revision 1.1.1.2  1992/08/30  03:14:12  jaffe
 * 	Transarc delta: jaffe-ot4719-cleanup-gcc-Wall-in-osi 1.3
 * 	  Selected comments:
 * 	    Fixed many compiler warnings in the osi directory.
 * 	    Reworked ALL of the header files.  All files in the osi directory now
 * 	    have NO machine specific ifdefs.  All machine specific code is in the
 * 	    machine specific subdirectories.  To make this work, additional flags
 * 	    were added to the afs/param.h file so that we can tell if a particular
 * 	    platform has any additional changes for a given osi header file.
 * 	    new file for machine specific additions for the osi_uio.h file.
 * 	    Corrected errors that appeared while trying to build everything on AIX3.2
 * 	    cleanup for OSF1 compilation
 * 
 * $EndLog$
 */
/*
 *      Copyright (C) 1991, 1990 Transarc Corporation
 *      All rights reserved.
 */

#ifndef TRANSARC_OSI_UIO_MACH_H
#define	TRANSARC_OSI_UIO_MACH_H

#include <sys/uio.h>

/* 
 * This file contains standard macros that are used to isolate OS
 * system dependencies for uio servides
 */

/*
 * uio vector related definitions
 */
#define	osi_uio_iov		uio_iov
#define	osi_uio_iovcnt		uio_iovcnt
#define	osi_uio_offset		uio_offset
#define osi_uio_limit		uio_limit
#define	osi_uio_resid		uio_resid
#define	osi_uio_seg		uio_segflg
#define osi_uio_fmode		uio_fmode
#define	OSI_UIOSYS		UIO_SYSSPACE
#define	OSI_UIOUSER		UIO_USERSPACE
#define osi_InitUIO(up)		(up)->uio_fmode = 0; (up)->uio_pmp = NULL; \
				(up)->uio_pio = 0; (up)->uio_pbuf = 0

/* SGIMIPS defines FOLLOW_LINK as FOLLOW */
#define FOLLOW_LINK	FOLLOW

#ifdef	KERNEL
typedef uio_seg_t osi_uio_seg_t;
#endif

#define osi_uio_set_offset(u, o) \
  (hfitsin32(o) ? hget32(((u).osi_uio_offset), (o)) , 0 : !0)

#define osi_uio_get_offset(u, o) (hset32((o), (u).osi_uio_offset))

#ifdef KERNEL
#define osi_uiomove(cp, n, rw, uio) uiomove (cp, n, rw, uio)
#else /*  ! KERNEL */
#define osi_uiomove(cp, n, rw, uio) osi_user_uiomove (cp, n, rw, uio)
#endif /* KERNEL */

#endif /* TRANSARC_OSI_UIO_MACH_H */
