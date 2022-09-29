/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_sysconfig_mach.h,v $
 * Revision 65.1  1997/10/24 14:29:51  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:43  jdoak
 * Initial revision
 *
 * Revision 64.1  1997/02/14  19:46:51  dce
 * *** empty log message ***
 *
 * Revision 1.1  1996/04/15  18:54:46  vrk
 * Initial revision
 *
 * Revision 1.1  1994/06/09  14:15:19  dcebuild
 * Original File from OSF
 *
 * Revision 1.1.10.2  1994/06/09  14:15:18  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:27:50  annie]
 *
 * Revision 1.1.10.1  1994/02/04  20:24:21  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:15:33  devsrc]
 * 
 * Revision 1.1.8.1  1994/01/28  20:43:48  annie
 * 	expand OSF copyright
 * 	[1994/01/28  20:42:22  annie]
 * 
 * Revision 1.1.6.2  1993/07/19  19:33:20  zeliff
 * 	HP port of DFS
 * 	[1993/07/19  18:25:48  zeliff]
 * 
 * Revision 1.1.4.3  1993/07/16  20:19:09  kissel
 * 	*** empty log message ***
 * 	[1993/06/21  14:34:32  kissel]
 * 
 * 	Initial GAMERA branch
 * 	[1993/04/02  11:58:18  mgm]
 * 
 * Revision 1.1.2.2  1993/06/04  15:11:38  kissel
 * 	Initial HPUX RP version.
 * 	[1993/06/03  21:43:42  kissel]
 * 
 * Revision 1.1.2.3  1993/03/15  16:40:00  toml
 * 	Kernel extensions support.
 * 	[1993/03/15  16:01:45  toml]
 * 
 * Revision 1.1.2.2  1992/10/15  21:01:20  toml
 * 	Initial revision.
 * 	[1992/10/14  16:56:59  toml]
 * 
 * $EndLog$
*/

#ifndef _OSI_SYSCONFIG_MACH_H
#define _OSI_SYSCONFIG_MACH_H

#ifdef HPUX  /* can be removed later - VRK */
#include <sys/kload.h>
typedef int   sysconfig_op_t;			/* configuration operation */
typedef	int filesys_config_t;
#endif /* HPUX */

#define SYSCONFIG_NOSPEC	??
#define SYSCONFIG_CONFIGURE	KLOAD_INIT_CONFIG
#define SYSCONFIG_UNCONFIGURE	KLOAD_INIT_UNCONFIG
#define SYSCONFIG_QUERY		??
#define SYSCONFIG_RECONFIGURE	??
#define SYSCONFIG_QUERYSIZE	??

/* VRK - couldn't figure out where and what 
 * KLOAD_INIT_CONFIG & KLOAD_INIT_UNCONFIG are to be defined as, so I 
 * am giving a arbitrary defn which might need re-look later.... 
 */
#define KLOAD_INIT_CONFIG 	1
#define KLOAD_INIT_UNCONFIG 	2



#endif	/* _OSI_SYSCONFIG_MACH_H */
