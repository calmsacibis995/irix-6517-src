/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: dacl_linkage.c,v 65.3 1998/03/23 16:36:42 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: dacl_linkage.c,v $
 * Revision 65.3  1998/03/23 16:36:42  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 20:00:38  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:20:16  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.141.1  1996/10/02  18:15:41  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:46:34  damon]
 *
 * Revision 1.1.136.2  1994/06/09  14:18:40  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:30:41  annie]
 * 
 * Revision 1.1.136.1  1994/02/04  20:28:33  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:17:23  devsrc]
 * 
 * Revision 1.1.134.1  1993/12/07  17:32:40  jaffe
 * 	New File from Transarc 1.0.3a
 * 	[1993/12/02  21:55:50  jaffe]
 * 
 * $EndLog$
 */
/* Copyright (C) 1993 Transarc Corporation - All rights reserved */

/*
 * This file contains the symbol linkage table that we use in the
 * SunOS 5.x kernel to resolve references to symbols in the DFS core
 * module.  We must list each symbol here for which we defined a macro
 * in dacl.p.h and dacl_mgruuids.h.
 * NOTE: The symbols must appear in exactly the same order as their
 * declarations in dacl.p.h and dacl_mgruuids.h!
 */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/security/dacl/RCS/dacl_linkage.c,v 65.3 1998/03/23 16:36:42 gwehrman Exp $ */

#include <dcedfs/dacl.h>
#include <dcedfs/dacl_mgruuids.h>

#if defined(_KERNEL) && defined(AFS_SUNOS5_ENV)
void *dacl_symbol_linkage[] = {
    (void *)dacl_SetSysAdminGroupID,
    (void *)dacl_GetSysAdminGroupID,
    (void *)dacl_SetLocalCellID,
    (void *)dacl_GetLocalCellID,
    (void *)dacl_epi_CheckAccessAllowedPac,
    (void *)dacl_epi_CheckAccessPac,
    (void *)dacl_AddPermBitsToOnePermset,
    (void *)dacl_ChmodOnePermset,
    (void *)dacl_OnePermsetToPermBits,
    (void *)dacl_epi_FlattenAcl,
    (void *)dacl_FlattenAcl,
    (void *)dacl_FlattenAclWithModeBits,
    (void *)dacl_epi_ValidateBuffer,
    (void *)dacl_ExtractPermBits,
    (void *)dacl_ExtractMinimumPermSet,
    (void *)dacl_ParseAcl,
    (void *)dacl_FreeAclEntries,
    (void *)dacl_PacFromUcred,
    (void *)dacl_PacListFromUcred,
    (void *)dacl_InitEpiAcl,
    (void *)dacl_ChangeRealm,
    (void *)dacl_ChangeUnauthMask,
    (void *)&dacl_localCellID,
    (void *)&episodeAclMgmtUuid
};
#endif /* _KERNEL && AFS_SUNOS5_ENV */
