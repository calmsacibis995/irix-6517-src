/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_ufs_mach.h,v $
 * Revision 65.3  1998/03/06 19:47:57  gwehrman
 * Defined osi_CantCVTVP() as 0.
 *
 * Revision 65.2  1998/03/06 00:39:03  lmc
 * Add back osi_getufilelimit() to get the user file limit from
 * the uthread_t structure or if it isn't a uthread, set the limit
 * to RLIM_INFINITY.
 *
 * Revision 65.1  1997/10/24  14:29:51  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:44  jdoak
 * Initial revision
 *
 * Revision 64.1  1997/02/14  19:46:51  dce
 * *** empty log message ***
 *
 * Revision 1.2  1996/04/06  00:17:52  bhim
 * No Message Supplied
 *
 * Revision 1.1.122.2  1994/06/09  14:15:22  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:27:53  annie]
 *
 * Revision 1.1.122.1  1994/02/04  20:24:25  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:15:35  devsrc]
 * 
 * Revision 1.1.120.1  1993/12/07  17:29:41  jaffe
 * 	New File from Transarc 1.0.3a
 * 	[1993/12/02  21:31:03  jaffe]
 * 
 * $EndLog$
 */
/*
 * Copyright (C) Transarc Corp. 1993  All rights reserved
 */
/*
 * $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/osi/RCS/osi_ufs_mach.h,v 65.3 1998/03/06 19:47:57 gwehrman Exp $
 */

#define osi_CantCVTVP(vp)	0

#define osi_iput(x) 		panic("iput() call unsupported")
#define osi_iupdat(x,wait)	panic("iupdat() call unsupported")
#define osi_ilock(x)		panic("ilock() call unsupported")	
#define osi_irele(x)		panic("irele() call unsupported")
#define osi_iget(a, b, c)	panic("iget() call unsupported")

#ifdef SGIMIPS
#define osi_getufilelimit()     ((KT_CUR_ISUTHREAD()) ?  curprocp->p_rlimit[RLIMIT_FSIZE].rlim_cur : RLIM_INFINITY)
#endif

