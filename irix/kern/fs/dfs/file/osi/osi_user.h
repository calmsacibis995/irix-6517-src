/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_user.h,v $
 * Revision 65.2  1997/12/16 17:05:39  lmc
 *  1.2.2 initial merge of file directory
 *
 * Revision 65.1  1997/10/20  19:17:45  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.921.1  1996/10/02  18:11:56  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:45:17  damon]
 *
 * Revision 1.1.916.2  1994/06/09  14:17:20  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:29:30  annie]
 * 
 * Revision 1.1.916.1  1994/02/04  20:26:58  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:16:36  devsrc]
 * 
 * Revision 1.1.914.1  1993/12/07  17:31:32  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  16:14:40  jaffe]
 * 
 * Revision 1.1.5.5  1993/01/21  14:53:10  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  14:56:55  cjd]
 * 
 * Revision 1.1.5.4  1992/11/24  18:24:06  bolinger
 * 	Change include file install directory from .../afs to .../dcedfs.
 * 	[1992/11/22  19:16:00  bolinger]
 * 
 * Revision 1.1.5.3  1992/10/27  21:25:16  jaffe
 * 	Transarc delta: bab-ot4988-no-modes-in-initial-acls 1.2
 * 	  Selected comments:
 * 	    Handle the case of dummy ACL's.
 * 	    Add osi_GetCMASK, symmetric with osi_SetCMASK.
 * 	Transarc delta: kazar-ot5556-pass-umask-to-create-ops 1.2
 * 	  Selected comments:
 * 	    pass umask to create ops for default acl handling
 * 	    way to get umask
 * 	    Fix unresolved symbol during link.
 * 	[1992/10/27  14:42:37  jaffe]
 * 
 * Revision 1.1.5.2  1992/08/31  20:49:19  jaffe
 * 	Transarc delta: jaffe-ot4719-cleanup-gcc-Wall-in-osi 1.3
 * 	  Selected comments:
 * 	    Fixed many compiler warnings in the osi directory.
 * 	    Reworked ALL of the header files.  All files in the osi directory now
 * 	    have NO machine specific ifdefs.  All machine specific code is in the
 * 	    machine specific subdirectories.  To make this work, additional flags
 * 	    were added to the afs/param.h file so that we can tell if a particular
 * 	    platform has any additional changes for a given osi header file.
 * 	    Remove all machine specific ifdefs to the osi_user_mach.h file in
 * 	    the machine specific directories.
 * 	    Corrected errors that appeared while trying to build everything on AIX3.2
 * 	    cleanup for OSF1 compilation
 * 	[1992/08/30  03:42:08  jaffe]
 * 
 * Revision 1.1.2.3  1992/05/05  04:29:20  mason
 * 	delta jdp-ot2611-osi-add-copyrights 1.2
 * 	[1992/04/24  22:38:50  mason]
 * 
 * Revision 1.1.2.2  1992/04/21  14:17:04  mason
 * 	Transarc delta: cburnett-ot2480-move_osi_SetCMASK 1.1
 * 	  Files modified:
 * 	    osi: osi_port.h, osi_user.h
 * 	  Selected comments:
 * 	    [ move this function to osi_user.h where interfaces regarding the ublock
 * 	      should be]
 * 	    [ added osi_SetCMASK.  Also added back include of sys/user.h per
 * 	      the agreed to strategy regarding portability of system includes.
 * 	      eventually the include of user.h in sysincludes.h will be removed.
 * 	      Any current code that expects to get struct user from sysincludes.h
 * 	      should be changed to include osi_user.h]
 * 	Transarc delta: cburnett-ot2547-add_user.h_include_to_osi_user.h 1.1
 * 	  Files modified:
 * 	    osi: osi_user.h
 * 	  Selected comments:
 * 	    [ add back include of user.h]
 * 	    [ add include of user.h ]
 * 	[1992/04/20  22:51:20  mason]
 * 
 * Revision 1.1  1992/01/19  02:43:15  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/

/* Copyright (C) 1992 Transarc Corporation - All rights reserved */

#ifndef TRANSARC_OSI_USER_H
#define	TRANSARC_OSI_USER_H

#include <dcedfs/param.h>
#include <dcedfs/osi_user_mach.h>
/*
 * Set or get cmask value in u area.
 */
#ifdef SGIMIPS
extern void osi_SetCMASK(int v);
#define osi_GetCMASK()		(curuthread->ut_cmask)
#else
#define osi_SetCMASK(v)         (u.u_cmask = (v))
#define osi_GetCMASK()		(u.u_cmask)
#endif


#endif /* TRANSARC_OSI_USER_H */
