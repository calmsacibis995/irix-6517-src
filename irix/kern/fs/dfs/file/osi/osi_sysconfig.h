/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_sysconfig.h,v $
 * Revision 65.1  1997/10/20 19:17:45  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.127.1  1996/10/02  18:11:48  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:45:08  damon]
 *
 * Revision 1.1.122.2  1994/06/09  14:17:16  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:29:26  annie]
 * 
 * Revision 1.1.122.1  1994/02/04  20:26:48  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:16:33  devsrc]
 * 
 * Revision 1.1.120.1  1993/12/07  17:31:26  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  16:13:35  jaffe]
 * 
 * Revision 1.1.2.4  1993/01/21  14:53:01  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  14:56:41  cjd]
 * 
 * Revision 1.1.2.3  1992/11/24  18:24:00  bolinger
 * 	Change include file install directory from .../afs to .../dcedfs.
 * 	[1992/11/22  19:15:48  bolinger]
 * 
 * Revision 1.1.2.2  1992/10/02  21:25:13  toml
 * 	Initial revision.
 * 	[1992/10/02  19:35:43  toml]
 * 
 * $EndLog$
 */

#ifndef OSI_SYSCONFIG_H
#define	OSI_SYSCONFIG_H

/*
 * definitions relating to subsystem configuration
 */
#if	!defined(AFS_DYNAMIC) || defined(HPUX)
#include <dcedfs/osi_sysconfig_mach.h>
#endif

#endif /* OSI_SYSCONFIG_H */
