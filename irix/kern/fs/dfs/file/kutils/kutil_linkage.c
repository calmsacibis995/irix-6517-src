/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: kutil_linkage.c,v 65.3 1998/03/23 16:36:06 gwehrman Exp $";
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
 * $Log: kutil_linkage.c,v $
 * Revision 65.3  1998/03/23 16:36:06  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 20:00:28  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:20:12  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.25.1  1996/10/02  17:53:23  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:41:35  damon]
 *
 * Revision 1.1.20.2  1994/06/09  14:12:17  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:25:44  annie]
 * 
 * Revision 1.1.20.1  1994/02/04  20:21:47  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:14:31  devsrc]
 * 
 * Revision 1.1.18.1  1993/12/07  17:27:24  jaffe
 * 	New File from Transarc 1.0.3a
 * 	[1993/12/02  21:30:30  jaffe]
 * 
 * $EndLog$
 */
/* Copyright (C) 1995, 1993 Transarc Corporation - All rights reserved */

/*
 * This file contains the symbol linkage table that we use in the
 * SunOS 5.x kernel to force references to exported symbols in the
 * DFS core module that are not otherwise used internally.
 */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/kutils/RCS/kutil_linkage.c,v 65.3 1998/03/23 16:36:06 gwehrman Exp $ */

#include <dcedfs/com_locks.h>
#include <dcedfs/syscall.h>
#include <dcedfs/krpc_pool.h>

#if defined(_KERNEL) && defined(AFS_SUNOS5_ENV)
void *kutil_symbol_linkage[] = {
    (void *)afs_set_syscall,
    (void *)&afscall_timeSynchDistance,
    (void *)&afscall_timeSynchDispersion,
    (void *)vnl_init,
    (void *)vnl_idset,
    (void *)vnl_blocked,
    (void *)vnl_adjust,
    (void *)vnl_alloc,
    (void *)vnl_cleanup,
    (void *)vnl_sleep,
    (void *)krpc_PoolInit,
    (void *)krpc_PoolAlloc,
    (void *)krpc_PoolFree,
    (void *)krpc_AddConcurrentCalls,
    (void *)krpc_SetConcurrentProc,
    (void *)vnl_ridset
};
#endif /* _KERNEL && AFS_SUNOS5_ENV */
