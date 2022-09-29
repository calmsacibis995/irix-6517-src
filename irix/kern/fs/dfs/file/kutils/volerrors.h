/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: volerrors.h,v $
 * Revision 65.1  1997/10/20 19:20:12  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.9.1  1996/10/02  17:53:34  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:41:37  damon]
 *
 * Revision 1.1.4.1  1994/06/09  14:12:26  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:25:51  annie]
 * 
 * Revision 1.1.2.2  1993/01/19  16:06:36  cjd
 * 	embedded copyright notice
 * 	[1993/01/19  14:16:35  cjd]
 * 
 * Revision 1.1  1992/01/19  02:55:20  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/
/* Copyright (C) 1989, 1990 Transarc Corporation - All rights reserved */

#ifndef	_VOLERRORSH_
#define	_VOLERRORSH_

/* 
 * NOTE: These volume-related codes are used by the cache manager. This include file 
 * should be removed and cm should use an error header from episode 
 */

#define VNOVNODE	102	/* Bad vnode number quoted */
#define VNOVOL		103	/* Volume not attached; doesn't exist */
#define VBUSY		110	/* Volume temporarily unavailable; try again. */
#define VMOVED		111	/* Volume has moved to another server */

#endif	/* _VOLERRORSH_ */
