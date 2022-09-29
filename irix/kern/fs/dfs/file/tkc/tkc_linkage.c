/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkc_linkage.c,v 65.3 1998/03/23 16:27:25 gwehrman Exp $";
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
 * $Log: tkc_linkage.c,v $
 * Revision 65.3  1998/03/23 16:27:25  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 19:58:33  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:18:02  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.138.1  1996/10/02  21:01:26  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:48:10  damon]
 *
 * Revision 1.1.133.2  1994/06/09  14:20:36  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:32:02  annie]
 * 
 * Revision 1.1.133.1  1994/02/04  20:30:27  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:18:11  devsrc]
 * 
 * Revision 1.1.131.1  1993/12/07  17:33:57  jaffe
 * 	New File from Transarc 1.0.3a
 * 	[1993/12/02  21:56:06  jaffe]
 * 
 * $EndLog$
 */
/* Copyright (C) 1993 Transarc Corporation - All rights reserved */

/*
 * This file contains the symbol linkage table that we use in the
 * SunOS 5.x kernel to force references to exported symbols in the
 * DFS core module that are not otherwise used internally.
 */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkc/RCS/tkc_linkage.c,v 65.3 1998/03/23 16:27:25 gwehrman Exp $ */

#include <dcedfs/tkc.h>

#if defined(_KERNEL) && defined(AFS_SUNOS5_ENV)
void *tkc_symbol_linkage[] = {
    (void *)tkc_Init,
    (void *)tkc_Put,
    (void *)tkc_FindVcache,
    (void *)tkc_FlushVnode,
    (void *)tkc_GetTokens,
    (void *)tkc_PutTokens,
    (void *)tkc_Get
};
#endif /* _KERNEL && AFS_SUNOS5_ENV */
