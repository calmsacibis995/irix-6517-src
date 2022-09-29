/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: xcred_linkage.c,v 65.3 1998/03/23 16:42:53 gwehrman Exp $";
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
 * $Log: xcred_linkage.c,v $
 * Revision 65.3  1998/03/23 16:42:53  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 20:00:54  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:20:34  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.192.1  1996/10/02  19:03:20  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:51:46  damon]
 *
 * Revision 1.1.187.2  1994/06/09  14:25:48  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:37:46  annie]
 * 
 * Revision 1.1.187.1  1994/02/04  20:36:36  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:20:47  devsrc]
 * 
 * Revision 1.1.185.1  1993/12/07  17:38:35  jaffe
 * 	New File from Transarc 1.0.3a
 * 	[1993/12/02  21:57:09  jaffe]
 * 
 * $EndLog$
 */
/* Copyright (C) 1994, 1993 Transarc Corporation - All rights reserved */

/*
 * This file contains the symbol linkage table that we use in the
 * SunOS 5.x kernel to force references to exported symbols in the
 * DFS core module that are not otherwise used internally.
 */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/xcred/RCS/xcred_linkage.c,v 65.3 1998/03/23 16:42:53 gwehrman Exp $ */

#include <dcedfs/xcred.h>

#if defined(_KERNEL) && defined(AFS_SUNOS5_ENV)
void *xcred_symbol_linkage[] = {
    (void *)xcred_Init,
    (void *)xcred_Create,
    (void *)xcred_Hold,
    (void *)xcred_Release,
    (void *)xcred_AssociateCreds,
    (void *)xcred_UCredToXCred,
    (void *)xcred_PutProp,
    (void *)xcred_GetProp,
    (void *)xcred_FindByPag,
    (void *)xcred_DeleteEntry,
    (void *)xcred_Delete,
    (void *)xcred_EnumerateProp,
    (void *)xcred_GetUFlags,
    (void *)xcred_SetUFlags
};
#endif /* _KERNEL && AFS_SUNOS5_ENV */
