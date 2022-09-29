/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tpq_linkage.c,v 65.3 1998/03/23 16:28:16 gwehrman Exp $";
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
 * $Log: tpq_linkage.c,v $
 * Revision 65.3  1998/03/23 16:28:16  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 19:58:44  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:18:11  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.130.1  1996/10/02  18:49:14  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:49:25  damon]
 *
 * Revision 1.1.124.2  1994/06/09  14:23:10  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:34:51  annie]
 * 
 * Revision 1.1.124.1  1994/02/04  20:33:25  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:19:21  devsrc]
 * 
 * Revision 1.1.122.1  1993/12/07  17:36:09  jaffe
 * 	New File from Transarc 1.0.3a
 * 	[1993/12/02  21:56:40  jaffe]
 * 
 * $EndLog$
 */
/* Copyright (C) 1994, 1993 Transarc Corporation - All rights reserved */

/*
 * This file contains the symbol linkage table that we use in the
 * SunOS 5.x kernel to force references to exported symbols in the
 * DFS core module that are not otherwise used internally.
 */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tpq/RCS/tpq_linkage.c,v 65.3 1998/03/23 16:28:16 gwehrman Exp $ */

#include <dcedfs/tpq.h>

#if defined(_KERNEL) && defined(AFS_SUNOS5_ENV)
void *tpq_symbol_linkage[] = {
    (void *)tpq_Init,
    (void *)tpq_Adjust,
    (void *)tpq_QueueRequest,
    (void *)tpq_DequeueRequest,
    (void *)tpq_ShutdownPool,
    (void *)tpq_Stat,
    (void *)tpq_SetArgument,
    (void *)tpq_GetPriority,
    (void *)tpq_SetPriority,
    (void *)tpq_GetGracePeriod,
    (void *)tpq_SetGracePeriod,
    (void *)tpq_GetRescheduleInterval,
    (void *)tpq_SetRescheduleInterval,
    (void *)tpq_GetDropDeadTime,
    (void *)tpq_SetDropDeadTime,
    (void *)tpq_Pardo,
    (void *)tpq_ForAll,
    (void *)tpq_CreateParSet,
    (void *)tpq_AddParSet,
    (void *)tpq_WaitParSet
};
#endif /* _KERNEL && AFS_SUNOS5_ENV */
