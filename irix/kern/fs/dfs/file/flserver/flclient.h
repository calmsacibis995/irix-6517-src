/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: flclient.h,v $
 * Revision 65.1  1997/10/20 19:20:06  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.9.1  1996/10/02  17:45:59  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:37:00  damon]
 *
 * Revision 1.1.4.1  1994/06/09  14:07:03  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:22:16  annie]
 * 
 * Revision 1.1.2.3  1993/01/21  19:34:10  zeliff
 * 	Embedding copyright notices
 * 	[1993/01/19  19:47:43  zeliff]
 * 
 * Revision 1.1.2.2  1992/08/31  19:34:21  jaffe
 * 	Transarc delta: cfe-ot4029-portable-rpc-data-types 1.2
 * 	  Selected comments:
 * 	    If ``long'' could possibly mean ``64 bits'' any time soon, we need to keep
 * 	    our RPC interfaces from breaking.
 * 	    see above
 * 	    More of the same.  Forgot a couple of .idl files, and needed to change
 * 	    a couple of procedure signatures to match.
 * 	[1992/08/30  02:19:38  jaffe]
 * 
 * Revision 1.1  1992/01/19  02:48:46  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/

/* Copyright (C) 1991 Transarc Corporation - All rights reserved */

extern error_status_t VL_CreateEntry();
extern error_status_t VL_DeleteEntry();
extern error_status_t VL_GetEntryByID();
extern error_status_t VL_GetEntryByName();
extern error_status_t VL_GetNewVolumeId();
extern error_status_t VL_ReplaceEntry();
extern error_status_t VL_SetLock();
extern error_status_t VL_ReleaseLock();
extern error_status_t VL_ListEntry();
extern error_status_t VL_ListByAttributes();
extern error_status_t VL_GetStats();
extern error_status_t VL_AddAddress();
extern error_status_t VL_RemoveAddress();
extern error_status_t VL_ChangeAddress();
extern error_status_t VL_GetCellInfo();
extern error_status_t VL_GetNextServersByID();
extern error_status_t VL_GetNextServersByName();
extern error_status_t VL_GetSiteInfo();
extern error_status_t VL_GenerateSites();
extern error_status_t VL_Probe();
extern error_status_t VL_GetCEntryByID();
extern error_status_t VL_GetCEntryByName();
extern error_status_t VL_GetCNextServersByID();
extern error_status_t VL_GetCNextServersByName();
extern error_status_t VL_GetNewVolumeIds();
extern error_status_t VL_ExpandSiteCookie();
extern error_status_t VL_CreateServer();
extern error_status_t VL_AlterServer();
extern error_status_t VL_GetServerInterfaces();
