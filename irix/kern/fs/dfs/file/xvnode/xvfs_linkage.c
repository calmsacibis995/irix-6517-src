/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: xvfs_linkage.c,v 65.3 1998/03/23 16:37:25 gwehrman Exp $";
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
 * $Log: xvfs_linkage.c,v $
 * Revision 65.3  1998/03/23 16:37:25  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 20:00:47  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:20:23  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.136.1  1996/10/02  19:04:10  damon
 * 	New DFS from Transarc
 * 	[1996/10/01  18:59:41  damon]
 *
 * $EndLog$
 */
/* Copyright (C) 1996, 1993 Transarc Corporation - All rights reserved */

/*
 * This file contains the symbol linkage table that we use in the
 * SunOS 5.x kernel to force references to exported symbols in the
 * DFS core module that are not otherwise used internally.
 */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/xvnode/RCS/xvfs_linkage.c,v 65.3 1998/03/23 16:37:25 gwehrman Exp $ */

#include <dcedfs/xvfs_export.h>

#if defined(_KERNEL) && defined(AFS_SUNOS5_ENV)
void *xvfs_symbol_linkage[] = {
    (void *)xvfs_ConvertDev,
    (void *)xvfs_convert,
    (void *)xvfs_InitFromVFSOps,
    (void *)xvfs_InitFromXOps,
    (void *)xvfs_SetAdminGroupID,
    (void *)xvfs_NullTxvattr
};
#endif /* _KERNEL && AFS_SUNOS5_ENV */
