/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: compat_rpcVers.h,v $
 * Revision 65.1  1997/10/20 19:20:38  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.12.1  1996/10/02  17:54:36  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:42:20  damon]
 *
 * Revision 1.1.7.1  1994/06/09  14:13:38  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:26:39  annie]
 * 
 * Revision 1.1.5.3  1993/01/19  16:09:01  cjd
 * 	embedded copyright notice
 * 	[1993/01/19  15:12:32  cjd]
 * 
 * Revision 1.1.5.2  1992/11/24  17:55:38  bolinger
 * 	Change include file install directory from .../afs to .../dcedfs.
 * 	[1992/11/22  19:10:35  bolinger]
 * 
 * Revision 1.1.3.3  1992/01/24  03:46:12  devsrc
 * 	Merging dfs6.3
 * 	[1992/01/24  00:14:40  devsrc]
 * 
 * Revision 1.1.3.2  1992/01/23  19:51:32  zeliff
 * 	Moving files onto the branch for dfs6.3 merge
 * 	[1992/01/23  18:35:49  zeliff]
 * 	Revision 1.1.1.3  1992/01/23  22:18:21  devsrc
 * 	Fixed logs
 * 
 * $EndLog$
 */
/*
 *	compat_rpcVers.h -- header file for DFS versioning mechanism
 *
 * Copyright (C) 1991 Transarc Corporation
 * All rights reserved.
 */

#ifndef TRANSARC_COMPAT_RPCVERS_H
#define TRANSARC_COMPAT_RPCVERS_H

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/common_data.h>

/* An interface description structure that is used for 
 * RPC versioning. Servers fill this structure(s) with the
 * interface(s) they support and return a set of such structures
 * back to the client which is requesting it.
 */

#define MATCH_GOOD    0
#define MATCH_MEDIUM  1
#define MATCH_BAD     2

typedef struct dfs_interfaceArray {
        unsigned long count;
        rpc_if_id_t if_id;
        dfs_interfaceDescription *interfaces[MAXINTERFACESPERVERSION];
} dfs_interfaceArray;


IMPORT void dfs_installInterfaceDescription _TAKES((
				       rpc_if_handle_t if_spec,
				       rpc_if_handle_t orig_spec,
				       unsigned32 vers_provider,
				       unsigned_char_t *if_text, 
				       error_status_t *code
				       ));

IMPORT void dfs_printInterfaceDescription _TAKES ((
				       dfs_interfaceList *interfaces,
				       error_status_t *code
				       ));

IMPORT void dfs_GetServerInterfaces _TAKES((
				       rpc_if_handle_t if_spec,
				       dfs_interfaceList *interfaces,
				       error_status_t *code
				       ));

IMPORT long dfs_sameInterface _TAKES ((
				       rpc_if_handle_t if1,
				       unsigned int if1_provider,
				       dfs_interfaceList *serverInterfaces
				       ));

#endif /* TRANSARC_COMPAT_RPCVERS_H */
