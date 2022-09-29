/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_vmm.h,v $
 * Revision 65.1  1997/10/20 19:17:45  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.523.1  1996/10/02  18:12:01  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:45:19  damon]
 *
 * Revision 1.1.518.2  1994/06/09  14:17:23  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:29:33  annie]
 * 
 * Revision 1.1.518.1  1994/02/04  20:27:00  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:16:37  devsrc]
 * 
 * Revision 1.1.516.1  1993/12/07  17:31:34  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  16:14:57  jaffe]
 * 
 * Revision 1.1.4.4  1993/01/21  14:53:18  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  14:57:08  cjd]
 * 
 * Revision 1.1.4.3  1992/11/24  18:24:13  bolinger
 * 	Change include file install directory from .../afs to .../dcedfs.
 * 	[1992/11/22  19:16:13  bolinger]
 * 
 * Revision 1.1.4.2  1992/08/31  20:49:32  jaffe
 * 	Transarc delta: jaffe-ot4719-cleanup-gcc-Wall-in-osi 1.3
 * 	  Selected comments:
 * 	    Fixed many compiler warnings in the osi directory.
 * 	    Reworked ALL of the header files.  All files in the osi directory now
 * 	    have NO machine specific ifdefs.  All machine specific code is in the
 * 	    machine specific subdirectories.  To make this work, additional flags
 * 	    were added to the afs/param.h file so that we can tell if a particular
 * 	    platform has any additional changes for a given osi header file.
 * 	    Remove all machine specific ifdefs to the osi_vmm_mach.h file in
 * 	    the machine specific directories.
 * 	    Corrected errors that appeared while trying to build everything on AIX3.2
 * 	    cleanup for OSF1 compilation
 * 	[1992/08/30  03:42:42  jaffe]
 * 
 * Revision 1.1.2.2  1992/04/21  14:18:42  mason
 * 	Transarc delta: cburnett-ot2477-fix_bld_bugs_w_aix31_vm 1.1
 * 	  Files modified:
 * 	    cm: cm_scache.c, cm_scache.h; libafs/RIOS: dfscore.exp
 * 	    osi: Makefile, osi_vmm.h; xvnode: xvfs_vnode.c
 * 	    xvnode/RIOS: xvfs_aix2vfs.c, xvfs_aixglue.c
 * 	  Selected comments:
 * 	    [ This change fixes compile bugs genereated when AFS_AIX31_VM is
 * 	      defined in config/RIOS/param.h.  It allows the AIX VMM integration
 * 	      pieces of Eposode to be enabled.  It should be noted however the
 * 	      the VMM integration of the DFS cache manager with AIX is not complete
 * 	      or functional.  It some later time a submission will be made to
 * 	      remove the AFS_AIX31_VM definition and move it into the Episode
 * 	      specific code so that Episode can be compiled with the VMM turned
 * 	      on and the cache manager without it.]
 * 	    [ new file to provide portability layer for vmm structures and
 * 	      interfaces]
 * 
 * 	File to shelter the code from VM specific include files.
 * 
 * 	$TALog: osi_vmm.h,v $
 * 	Revision 1.7  1994/11/01  21:57:55  cfe
 * 	Bring over the changes that the OSF made in going from their DCE 1.0.3
 * 	release to their DCE 1.1 release.
 * 	[from r1.6 by delta cfe-db6109-merge-1.0.3-to-1.1-diffs, r1.1]
 * 
 * 	Revision 1.6  1993/06/24  18:15:33  blake
 * 	We need to check vd_owner in efs_putpage, since it may also get called
 * 	from pvn_vpzero while we hold the vnode lock.  Also, take the opportunity
 * 	to nuke efs_putpage_under_lock; move SunOS vm includes into osi_vmm_mach.h;
 * 	create headers to declare vnode and vfs operations; shuffle declarations in
 * 	logbuf headers so that externally used types and functions appear in logbuf.h;
 * 	commented a number of SHARED Bournegol macros in conflict with new
 * 	osi_vmm_mach.h.
 * 
 * 	Always include osi_vmm_mach.h.
 * 	[from r1.5 by delta blake-db3608-VM-reorganization, r1.7]
 * 
 * Revision 1.5  1993/01/29  15:01:36  jaffe
 * 	Pick up files from the OSF up to the 2.4 submission.
 * 	[from r1.4 by delta osf-revdrop-01-27-93, r1.1]
 * 
 * 	[1992/04/20  22:53:38  mason]
 * 
 * $EndLog$
 */

/*
 *      Copyright (C) 1991, 1990 Transarc Corporation
 *      All rights reserved.
 */

#ifndef TRANSARC_OSI_VMM_H
#define	TRANSARC_OSI_VMM_H

#include <dcedfs/param.h>
/*
 * definitions relating to VM interfaces and data structures
 */
#include <dcedfs/osi_vmm_mach.h>

#endif /* TRANSARC_OSI_VMM_H */
