/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_param.h,v $
 * Revision 65.1  1997/10/20 19:17:44  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.11.1  1996/10/02  18:11:43  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:45:06  damon]
 *
 * Revision 1.1.6.1  1994/06/09  14:17:11  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:29:22  annie]
 * 
 * Revision 1.1.4.4  1993/01/21  14:52:52  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  14:56:27  cjd]
 * 
 * Revision 1.1.4.3  1992/11/24  18:23:49  bolinger
 * 	Change include file install directory from .../afs to .../dcedfs.
 * 	[1992/11/22  19:15:29  bolinger]
 * 
 * Revision 1.1.4.2  1992/08/31  20:42:54  jaffe
 * 	Transarc delta: jaffe-ot4719-cleanup-gcc-Wall-in-osi 1.3
 * 	  Selected comments:
 * 	    Fixed many compiler warnings in the osi directory.
 * 	    Reworked ALL of the header files.  All files in the osi directory now
 * 	    have NO machine specific ifdefs.  All machine specific code is in the
 * 	    machine specific subdirectories.  To make this work, additional flags
 * 	    were added to the afs/param.h file so that we can tell if a particular
 * 	    platform has any additional changes for a given osi header file.
 * 	    Remove all machine specific ifdefs to the osi_param_mach.h file in
 * 	    the machine specific directories.
 * 	    Corrected errors that appeared while trying to build everything on AIX3.2
 * 	    cleanup for OSF1 compilation
 * 	[1992/08/30  03:37:24  jaffe]
 * 
 * Revision 1.1.2.2  1992/05/05  04:25:44  mason
 * 	delta jdp-ot2611-osi-add-copyrights 1.2
 * 	[1992/04/24  22:36:04  mason]
 * 
 * Revision 1.1  1992/01/19  02:43:02  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/* Copyright (C) 1992 Transarc Corporation - All rights reserved */

#ifndef TRANSARC_OSI_PARAM_H
#define	TRANSARC_OSI_PARAM_H

#include <dcedfs/param.h>

#ifdef HAS_OSI_PARAM_MACH_H
#include <dcedfs/osi_param_mach.h>
#endif

#endif
