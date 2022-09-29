/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_user_mach.h,v $
 * Revision 65.2  1998/02/02 22:07:26  lmc
 * Fixed osi_getufilelimit() and osi_vfs_op for kudzu.
 *
 * Revision 65.1  1997/10/24  14:29:52  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:43  jdoak
 * Initial revision
 *
 * Revision 64.2  1997/03/05  19:18:26  lord
 * Changed how we identify threads in the kernel so that kthreads can
 * enter dfs code without panicing.
 *
 * Revision 64.1  1997/02/14  19:46:51  dce
 * *** empty log message ***
 *
 * Revision 1.5  1996/06/15  18:48:26  brat
 * Missed out a  "(" inadvertently.
 *
 * Revision 1.4  1996/06/15  18:45:07  brat
 * Typo in prev checkin. It should be osi_curproc, not osi_curprocp.
 *
 * Revision 1.3  1996/06/15  02:48:06  brat
 * osi_getufilelimit changed to return a system-defined value in case we are
 * accessed from within a kernel thread.
 *
 * Revision 1.2  1996/04/06  00:17:56  bhim
 * No Message Supplied
 *
 * Revision 1.1.724.2  1994/06/09  14:15:24  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:27:55  annie]
 *
 * Revision 1.1.724.1  1994/02/04  20:24:29  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:15:36  devsrc]
 * 
 * Revision 1.1.722.2  1994/01/20  18:43:30  annie
 * 	added copyright header
 * 	[1994/01/20  18:39:45  annie]
 * 
 * Revision 1.1.722.1  1993/12/07  17:29:43  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  16:00:32  jaffe]
 * 
 * Revision 1.1.6.2  1993/07/19  19:33:29  zeliff
 * 	HP port of DFS
 * 	[1993/07/19  18:25:54  zeliff]
 * 
 * Revision 1.1.4.3  1993/07/16  20:19:22  kissel
 * 	*** empty log message ***
 * 	[1993/06/21  14:34:52  kissel]
 * 
 * 	Initial GAMERA branch
 * 	[1993/04/02  11:57:02  mgm]
 * 
 * Revision 1.1.2.2  1993/06/04  15:11:57  kissel
 * 	Initial HPUX RP version.
 * 	[1993/06/03  21:44:12  kissel]
 * 
 * Revision 1.1.2.3  1993/01/14  19:22:08  toml
 * 	Make proper #define for osi_getufilelimit for HPUX.
 * 	[1993/01/14  19:20:58  toml]
 * 
 * Revision 1.1.2.2  1992/10/15  21:01:45  toml
 * 	Initial revision.
 * 	[1992/10/14  16:57:09  toml]
 * 
 * $EndLog$
 */

#ifndef TRANSARC_OSI_USER_MACH_H
#define	TRANSARC_OSI_USER_MACH_H

/*
 * machine specific definitions for function to obtain user resource limits
 * Note in SGIMIPS, file size is in bytes, unlike HPUX where it is in 512-byte blocks.
 */

#endif /* TRANSARC_OSI_USER_MACH_H */
