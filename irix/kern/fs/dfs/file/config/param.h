/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: param.h,v $
 * Revision 65.9  1999/07/21 19:01:14  gwehrman
 * Defined SGIMIPS for this development header.  Fix for bug 657352.
 *
 * Revision 65.8  1998/07/29 19:39:15  gwehrman
 * Defined AFS_SMALLSTACK_ENV.  This enables code that uses heap allocation
 * instead of the stack for large local variables to keep the stack frame size
 * small.  Bug 621717.
 *
 * Revision 65.7  1998/03/31  18:09:13  lmc
 * Added a definition for the dfs file type as a character string, which
 * is passed to mount to identify it as a dfs mount point.
 *
 * Revision 65.6  1998/03/19  23:47:26  bdr
 * Misc 64 bit file changes and/or code cleanup for compiler warnings/remarks.
 *
 * Revision 65.5  1998/03/03  15:51:17  lmc
 * Added a define for OSI_MOUNT_TYPE_DFS.  The name changed in 1.2.2.  Do
 * we need to coexist with AFS?
 *
 * Revision 65.4  1998/03/02  22:29:10  bdr
 * Cleanup comments around AFS_64BIT_HAS_SCALAR_TYPE define.
 *
 * Revision 65.3  1998/02/27  00:05:05  lmc
 * Changes to compile using 64bit macros.  Copied from bdr, and needed
 * for compiles.
 *
 * Revision 65.1  1997/10/21  20:05:19  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:53  jdoak
 * Initial revision
 *
 * Revision 64.2  1997/04/16  18:50:42  lmc
 * KMEMUSER only defined if not previously defined.
 *
 * Revision 64.1  1997/02/14  19:49:37  dce
 * *** empty log message ***
 *
 * Revision 1.4  1996/05/29  18:04:49  bhim
 * Modified AFS_FSTYPE_DFS to be "dfs".
 *
 * Revision 1.3  1996/04/18  10:06:41  vrk
 * Redefined AFS_FSTYPE_DFS as "dfs_cm".
 *
 * Revision 1.2  1996/04/15  18:40:45  vrk
 * Undefined AFS_DYNAMIC. To be added later once dynamic loading support is
 * added.
 *
 * Revision 1.1  1996/04/04  18:56:49  bhim
 * Initial revision
 *
 * Revision 1.1.14.2  1994/06/09  13:55:19  annie
 * 	fixed copyright in src/file
 * 	[1994/06/08  21:29:52  annie]
 *
 * Revision 1.1.14.1  1994/02/04  20:09:31  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:08:15  devsrc]
 * 
 * Revision 1.1.12.1  1993/12/07  17:15:30  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  13:29:42  jaffe]
 * 
 * Revision 1.1.4.5  1993/01/18  20:53:13  cjd
 * 	embedded copyright notice
 * 	[1993/01/18  17:39:36  cjd]
 * 
 * Revision 1.1.4.4  1992/10/28  21:38:55  jaffe
 * 	Removed duplicate TALog entries.
 * 	[1992/10/28  21:03:57  jaffe]
 * 
 * Revision 1.1.4.3  1992/09/15  13:11:57  jaffe
 * 	Transarc delta: jaffe-ot4608-ucred-cleanup 1.3
 * 	  Selected comments:
 * 	    use osi macros for ucred access.  New macros are osi_GetNGroups,
 * 	    osi_SetNGroups, osi_GetUID, osi_GetRUID, and osi_GetGID.  New
 * 	    constants are OSI_NGROUPS, OSI_NOGROUP, and OSI_MAXGROUPS.
 * 	    add HAS_OSI_CRED_MACH to the params.
 * 	    include osi_cred.h for appropriate definitions.
 * 	[1992/09/14  19:33:09  jaffe]
 * 
 * Revision 1.1.4.2  1992/08/31  19:22:48  jaffe
 * 	Transarc delta: jaffe-ot4719-cleanup-gcc-Wall-in-osi 1.3
 * 	  Selected comments:
 * 	    Fixed many compiler warnings in the osi directory.
 * 	    Reworked ALL of the header files.  All files in the osi directory now
 * 	    have NO machine specific ifdefs.  All machine specific code is in the
 * 	    machine specific subdirectories.  To make this work, additional flags
 * 	    were added to the afs/param.h file so that we can tell if a particular
 * 	    platform has any additional changes for a given osi header file.
 * 	    Added defines for machine specific files for osi_vmm.h, osi_uio.h,
 * 	    osi_buf.h, and osi_net.h
 * 	    Corrected errors that appeared while trying to build everything on AIX3.2
 * 	    cleanup for OSF1 compilation
 * 
 * 	Transarc delta: jaffe-ot4719-cleanup-gcc-Wall-in-osi 1.3
 * 	  Selected comments:
 * 	    Fixed many compiler warnings in the osi directory.
 * 	    Reworked ALL of the header files.  All files in the osi directory now
 * 	    have NO machine specific ifdefs.  All machine specific code is in the
 * 	    machine specific subdirectories.  To make this work, additional flags
 * 	    were added to the afs/param.h file so that we can tell if a particular
 * 	    platform has any additional changes for a given osi header file.
 * 	    Added defines for machine specific files for osi_vmm.h, osi_uio.h,
 * 	    osi_buf.h, and osi_net.h
 * 	    Corrected errors that appeared while trying to build everything on AIX3.2
 * 	    cleanup for OSF1 compilation
 * 
 * 	$TALog: param.h,v $
 * 	Revision 1.7  1993/07/30  18:17:15  bwl
 * 	Port DFS 1.0.3 to HP/UX, adapting HP's changes (which were made to the
 * 	1.0.2a code base) to our own code base.
 * 
 * 	Various changes from HP.
 * 	[from r1.6 by delta bwl-o-db3961-port-103-to-HP, r1.1]
 * 
 * Revision 1.6  1993/01/19  21:21:24  jaffe
 * 	import OSF changes since 2.3 drop.
 * 	Mostly OSF copyright changes.
 * 	[from r1.5 by delta osf-revdrop-01-19-93, r1.1]
 * 
 * 	Revision 1.4  1992/07/29  21:35:38  jaffe
 * 	Fixed many compiler warnings in the osi directory.
 * 	Reworked ALL of the header files.  All files in the osi directory now
 * 	have NO machine specific ifdefs.  All machine specific code is in the
 * 	machine specific subdirectories.  To make this work, additional flags
 * 	were added to the afs/param.h file so that we can tell if a particular
 * 	platform has any additional changes for a given osi header file.
 * 
 * $EndLog$
 */
/*
 * (C) Copyright 1992 Transarc Corporation - All rights reserved */

#ifndef	_PARAM_SGIMIPS_H
#define	_PARAM_SGIMIPS_H
#ifndef SGIMIPS
#define SGIMIPS
#endif /* !SGIMIPS */

/*
 * DFS-related definitions for SGIMIPS.
 */

/* osi machine specific files */
#define HAS_OSI_USER_MACH_H 
#define HAS_OSI_INTR_MACH_H 
#define HAS_OSI_UIO_MACH_H  
#define HAS_OSI_CRED_MACH_H 
#define HAS_OSI_NET_MACH_H   
#define HAS_OSI_KVNODE_MACH_H 
#define HAS_OSI_VMM_MACH_H
#define HAS_OSI_DEVICE_MACH_H
#define HAS_OSI_BUF_MACH_H
#define AFS_SIGACTION_ENV

#undef HAS_OSI_CALLTRACE_MACH_H

/* 
 * File system entry (used if mount.h doesn't define MOUNT_AFS) 
 */
#define AFS_MOUNT_AFS3		3
#define AFS_MOUNT_DFS		4 
#define AFS_MOUNT_AFS		4  /* for now, let's not force the issue */
#ifdef SGIMIPS
				/*  This has changed in 1.2.2.  What do we
					want to do more permanently?  Do
					afs and dfs need to be able to 
					coexist?  */
#define OSI_MOUNT_TYPE_DFS	4
#endif
#define AFS_MOUNT_AGFS		5
#if 0
#define AFS_MOUNT_EPISODE       dont!use!me!i!am!dynamic!now
#endif

#ifdef SGIMIPS
#define OSI_MOUNT_TYPE_NAME	"dfs"
#endif
#ifdef _KERNEL
#ifndef KERNEL
#define KERNEL
#endif
#else
#ifndef _KMEMUSER
#define _KMEMUSER
#endif
#endif


/* 
 * Machine / Operating system information 
 */
#define SYS_NAME	"sgimips"	/* to be reviewed */

#if 0
#define SYS_NAME_ID 
#endif

/*
 * #define AFS_DEFAULT_ENV for non-reference platforms.
 * This will cause the 'portable' code to be compiled, or a #error
 * in situations where it was difficult to supply such code.
 */
#undef  AFS_DEFAULT_ENV			
#define AFS_IRIX_ENV             1	/* true for all IRIX systems */

/* 
 * Extra kernel definitions
 */
#ifdef KERNEL
#define AFS_SYSVLOCK		1
#define AFS_SMALLSTACK_ENV		/* use heap allocation */
#endif /* KERNEL */

/*
 * Says that RPC and DFS are dynamically loaded into the kernel at runtime.
 */
/* VRK - add this later....
#define AFS_DYNAMIC     1 
*/

/*
 * Says that the CM caches to any VFS file system.
 * If undefined, CM caches only to UFS file systems.
 */
#define AFS_VFSCACHE    1 
	
#if	defined(AFS_DEBUG) && defined(KERNEL)
/* this makes kernel debugging a whole lot easier */
#define STATIC
#endif

/* struct uio has the uio_fmode field */
#define AFS_UIOFMODE	1

/* Temporary fix for File system type */
#define AFS_FSTYPE_DFS	"dfs"

/*
 * These are all new defines for 1.2.2.  We have a 64 bit scalar type
 * that we can use for 64 bit and 32 bit machines.  This type is used
 * for an afs_hyper_t.  
 * The macro's in stds.h use the following defines for their work.
 */
/* We have a 64bit scalar type */

#define AFS_64BIT_HAS_SCALAR_TYPE 1
#define afs_64bit_scalar_type __int64_t
#define AFS_64BIT_ZERO 0L
#define AFS_64BIT_LOW_MASK  0xffffffff
#endif        /* _PARAM_SGIMIPS_H */
