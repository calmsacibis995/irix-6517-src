/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_uio.h,v $
 * Revision 65.3  1999/07/21 19:01:14  gwehrman
 * Defined SGIMIPS for this development header.  Fix for bug 657352.
 *
 * Revision 65.2  1998/05/29 20:10:58  bdr
 * modify prototype for osi_uio_skip to reflect new function
 * arg types.  64 bit file work.
 *
 * Revision 65.1  1997/10/20  19:17:45  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.120.1  1996/10/02  18:11:55  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:45:16  damon]
 *
 * Revision 1.1.115.2  1994/06/09  14:17:19  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:29:29  annie]
 * 
 * Revision 1.1.115.1  1994/02/04  20:26:56  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:16:35  devsrc]
 * 
 * Revision 1.1.113.1  1993/12/07  17:31:30  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  16:14:15  jaffe]
 * 
 * Revision 1.1.6.7  1993/01/21  14:53:07  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  14:56:51  cjd]
 * 
 * Revision 1.1.6.6  1993/01/13  18:18:03  shl
 * 	Transarc delta: mcinerny-ot6065-widen-volop-syscall-interface-to-64-bits 1.7
 * 	  Selected comments:
 * 	    The volops read, write, and truncate need to take hyper offsets.
 * 	    Accomplish this by creating a new struct io_rwHyperDesc whose position
 * 	    member is a hyper (for read & write).  For truncate, just pass a
 * 	    pointer to a hyper.
 * 	    Add a macro for setting the uio.offset member via a hyper.  By default
 * 	    it sets only the lower 32 bits and returns an error if the hyper won't
 * 	    fit.
 * 	    Need to include stds.h in sycall.h
 * 	    NOTE: this modification will need to be changed when imported into a
 * 	    post-2.1 build due to the renaming of the include tree!
 * 	    Fix compilation errors.
 * 	    Fix run-time kernel dump.
 * 	    Replace structure assignment (hyper = hyper) with hset(hyper, hyper),
 * 	    to be consistent (even though it amounts to the same thing).
 * 	    Fix the VOLOP_READ DIR and VOLOP_APPENDDIR ops so that they don't use an
 * 	    io_rwHyperDesc structure as if it were an io_rwDesc one.
 * 	[1993/01/12  21:44:58  shl]
 * 
 * Revision 1.1.6.5  1992/12/10  22:27:56  marty
 * 	Upgrade DFS to OSF/1 1.1.1.  [OT defect 5236]
 * 	[1992/12/10  22:15:30  marty]
 * 
 * Revision 1.1.6.4  1992/11/24  18:24:03  bolinger
 * 	Change include file install directory from .../afs to .../dcedfs.
 * 	[1992/11/22  19:15:54  bolinger]
 * 	Revision 1.1.8.2  1992/11/30  23:12:57  marty
 * 	OSF 1.1.1 port
 * 
 * Revision 1.1.6.3  1992/10/13  18:12:04  marty
 * 	OSC1.1.1 upgrade.  Changes are osc1.0.4 compatible
 * 	[1992/10/13  17:12:26  marty]
 * 
 * Revision 1.1.7.3  1992/10/08  15:10:57  marty
 * 	Remove "(" and ")" from #ifdef statement.
 * 	[1992/10/08  15:10:34  marty]
 * 
 * Revision 1.1.7.2  1992/10/02  18:36:28  garyf
 * 	OSF/1 needs uio.h too
 * 	add new include file for user space uio definitions
 * 	[1992/10/02  18:29:32  garyf]
 * 
 * Revision 1.1.6.2  1992/08/31  20:49:13  jaffe
 * 	Transarc delta: jaffe-ot4719-cleanup-gcc-Wall-in-osi 1.3
 * 	  Selected comments:
 * 	    Fixed many compiler warnings in the osi directory.
 * 	    Reworked ALL of the header files.  All files in the osi directory now
 * 	    have NO machine specific ifdefs.  All machine specific code is in the
 * 	    machine specific subdirectories.  To make this work, additional flags
 * 	    were added to the afs/param.h file so that we can tell if a particular
 * 	    platform has any additional changes for a given osi header file.
 * 	    Remove all machine specific ifdefs to the osi_uio_mach.h file in
 * 	    the machine specific directories.
 * 	    Corrected errors that appeared while trying to build everything on AIX3.2
 * 	    include <dcedfs/stds.h> since we now use _TAKES, and some files don't preinclude
 * 	    the stds file.
 * 	    cleanup for OSF1 compilation
 * 	[1992/08/30  03:41:26  jaffe]
 * 
 * Revision 1.1.3.6  1992/05/22  20:39:52  garyf
 * 	redefine OSF1 uiomove, add osi_uiomove to
 * 	hide platform differences in uiomove
 * 	[1992/05/22  03:13:13  garyf]
 * 
 * Revision 1.1.3.5  1992/05/05  04:28:09  mason
 * 	delta jdp-ot2611-osi-add-copyrights 1.2
 * 	[1992/04/24  22:38:04  mason]
 * 
 * Revision 1.1.3.4  1992/01/29  21:22:20  carl
 * 	Fixed definition of osi_user_uiomove
 * 	[1992/01/29  21:21:21  carl]
 * 
 * Revision 1.1.3.3  1992/01/24  03:47:24  devsrc
 * 	Merging dfs6.3
 * 	[1992/01/24  00:17:34  devsrc]
 * 
 * Revision 1.1.3.2  1992/01/23  20:20:46  zeliff
 * 	Moving files onto the branch for dfs6.3 merge
 * 	[1992/01/23  18:37:16  zeliff]
 * 	Revision 1.1.1.3  1992/01/23  22:20:02  devsrc
 * 	Fixed logs
 * 
 * $EndLog$
*/

/* Copyright (C) 1992 Transarc Corporation - All rights reserved */

#ifndef TRANSARC_OSI_UIO_H
#define	TRANSARC_OSI_UIO_H
#ifndef SGIMIPS
#define SGIMIPS
#endif /* !SGIMIPS */

#include <dcedfs/param.h>
#include <dcedfs/stds.h>

#include <dcedfs/osi_uio_mach.h>

/*
 * This file contains standard macros that are used to isolate OS
 * system dependencies for uio services
 */
extern void osi_uiomove_unit(
  u_long prevNum,
  u_long n,
  struct uio *uio,
  caddr_t *baseP,
  u_long *lenP
);

#if !defined(KERNEL)
extern int osi_user_uiomove(
  caddr_t cp,
  u_long n,
  enum uio_rw rw,
  struct uio *uio
);
#endif

extern int osi_uio_copy(
  struct uio *inuiop,
  struct uio *outuiop,
  struct iovec *outvecp
);

extern int osi_uio_trim(struct uio *uiop, long size);
#ifdef SGIMIPS
extern int osi_uio_skip(struct uio *uiop, afs_hyper_t size);
#else  /* SGIMIPS */
extern int osi_uio_skip(struct uio *uiop, long size);
#endif /* SGIMIPS */

#endif /* TRANSARC_OSI_UIO_H */
