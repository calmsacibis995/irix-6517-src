/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: fl_opcodes.h,v $
 * Revision 65.1  1997/10/20 19:20:06  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.9.1  1996/10/02  17:45:52  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:36:58  damon]
 *
 * Revision 1.1.4.1  1994/06/09  14:06:54  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:22:09  annie]
 * 
 * Revision 1.1.2.2  1993/01/21  19:33:57  zeliff
 * 	Embedding copyright notices
 * 	[1993/01/19  19:47:25  zeliff]
 * 
 * Revision 1.1  1992/01/19  02:48:41  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/

/* Copyright (C) 1991 Transarc Corporation - All rights reserved */

/* fldb server's current opcodes */
#define	VLCREATEENTRY		501
#define	VLDELETEENTRY		502
#define	VLGETENTRYBYID		503
#define	VLGETENTRYBYNAME	504
#define	VLGETNEWVOLUMEID	505
#define	VLREPLACEENTRY	506
#define	VLUPDATEENTRY		507
#define	VLSETLOCK		508
#define	VLRELEASELOCK	509
#define	VLLISTENTRY		510
#define	VLGETSTATS		513
#define	VLPROBE		514
#define	VLADDADDRESS	515
#define	VLREMOVEADDRESS	516
#define	VLCHANGEADDRESS	517
#define	VLGETCELLINFO	519
#define	VLGETNEXTSRVBYID	520
#define	VLGETNEXTSRVBYNAME	521
#define	VLGETSITEINFO	522
#define	VLGENERATESITES	523
#define	VLLISTBYATTRIBUTES	524
#define	VLGETNEWVOLUMEIDS	525
#define	VLCREATESERVER	526
#define	VLGETCENTRYBYID	527
#define	VLGETCENTRYBYNAME	528
#define	VLGETCNEXTSRVBYID	529
#define	VLGETCNEXTSRVBYNAME	530
#define	VLEXPANDSITECOOKIE  531
#define	VLALTERSERVER	532
#define	VLGETSERVERINTERFACES	533

#define VL_LOWEST_OPCODE VLCREATEENTRY
#define VL_HIGHEST_OPCODE VLGETSERVERINTERFACES
#define VL_OPCODE_RANGE ((VL_HIGHEST_OPCODE - VL_LOWEST_OPCODE) + 1)
