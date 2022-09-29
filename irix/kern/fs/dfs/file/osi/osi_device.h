/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_device.h,v $
 * Revision 65.1  1997/10/20 19:17:42  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.512.1  1996/10/02  18:10:33  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:44:53  damon]
 *
 * Revision 1.1.507.2  1994/06/09  14:16:54  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:29:06  annie]
 * 
 * Revision 1.1.507.1  1994/02/04  20:26:18  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:16:18  devsrc]
 * 
 * Revision 1.1.505.1  1993/12/07  17:30:55  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  16:10:39  jaffe]
 * 
 * Revision 1.1.4.4  1993/01/21  14:52:28  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  14:55:48  cjd]
 * 
 * Revision 1.1.4.3  1992/11/24  18:23:25  bolinger
 * 	Change include file install directory from .../afs to .../dcedfs.
 * 	[1992/11/22  19:14:45  bolinger]
 * 
 * Revision 1.1.4.2  1992/08/31  20:41:53  jaffe
 * 	Transarc delta: jaffe-ot4719-cleanup-gcc-Wall-in-osi 1.3
 * 	  Selected comments:
 * 	    Fixed many compiler warnings in the osi directory.
 * 	    Reworked ALL of the header files.  All files in the osi directory now
 * 	    have NO machine specific ifdefs.  All machine specific code is in the
 * 	    machine specific subdirectories.  To make this work, additional flags
 * 	    were added to the afs/param.h file so that we can tell if a particular
 * 	    platform has any additional changes for a given osi header file.
 * 	    Remove all machine specific ifdefs to the osi_device_mach.h file in
 * 	    the machine specific directories.
 * 	    Corrected errors that appeared while trying to build everything on AIX3.2
 * 	    cleanup for OSF1 compilation
 * 	[1992/08/30  03:33:57  jaffe]
 * 
 * Revision 1.1.2.2  1992/04/21  14:12:23  mason
 * 	Transarc delta: cburnett-ot2479-add_osi_device.h 1.1
 * 	  Files modified:
 * 	    osi: Makefile, osi_device.h
 * 	  Selected comments:
 * 	    [ add osi_device to hold porting interfaces relative to devices and
 * 	      device structures]
 * 	    [created]
 * 	Transarc delta: cburnett-ot2725-fix_assert_conflict_in_osi_device.h 1.1
 * 	  Files modified:
 * 	    osi: osi_device.h
 * 	  Selected comments:
 * 	    avoid assert conflict when user level code includes osi_device.h
 * 	    avoid assert conflict for AIX case.
 * 
 * 	A new file to shelter the code from specific includes regarding devices.
 * 
 * 	$TALog: osi_device.h,v $
 * 	Revision 1.8  1994/11/01  21:57:01  cfe
 * 	Bring over the changes that the OSF made in going from their DCE 1.0.3
 * 	release to their DCE 1.1 release.
 * 	[from r1.7 by delta cfe-db6109-merge-1.0.3-to-1.1-diffs, r1.1]
 * 
 * 	Revision 1.7  1993/02/23  19:55:58  blake
 * 	The final *planned* installment in this delta.  The major changes
 * 	this time are:
 * 
 * 	1) port/revision of asevent.c event code
 * 	2) replacing AIX-specific osi_getfh with a generic version;
 * 	   associated changes to afsd.c, cm_init.c
 * 	3) Added a mutex to provide osi_PreemptionOff/osi_RestorePreemption function
 * 	4) ported Episode logbuf, anode code to SunOS, incidental to checking
 * 	   osi changes.
 * 	[from r1.6 by delta blake-db3108-port-osi-layer-to-SunOS5.x, r1.6]
 * 
 * Revision 1.6  1993/01/29  15:01:21  jaffe
 * 	Pick up files from the OSF up to the 2.4 submission.
 * 	[from r1.5 by delta osf-revdrop-01-27-93, r1.1]
 * 
 * 	[1992/04/20  22:50:03  mason]
 * 
 * $EndLog$
 */
/*
 *      Copyright (C) 1990-1992 Transarc Corporation
 *      All rights reserved.
 */

#ifndef TRANSARC_OSI_DEVICE_H
#define	TRANSARC_OSI_DEVICE_H
/*
 * definitions relating to device interfaces and data structures
 */
#include <dcedfs/param.h>

/*
 * Don't keep resource usage statistics at user level.
 */
#ifndef _KERNEL
#define osi_inc_ru_oublock(n)
#define osi_inc_ru_inblock(n)
#endif

#include <dcedfs/osi_device_mach.h>

#endif /* TRANSARC_OSI_DEVICE_H */
