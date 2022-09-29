/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: zlc_linkage.c,v 65.3 1998/03/23 16:28:24 gwehrman Exp $";
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
 * $Log: zlc_linkage.c,v $
 * Revision 65.3  1998/03/23 16:28:24  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 19:58:45  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:18:12  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.145.1  1996/10/02  19:05:07  damon
 * 	New DFS from Transarc
 * 	[1996/10/01  19:00:14  damon]
 *
 * Revision 1.1.140.2  1994/06/09  14:27:01  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:38:53  annie]
 * 
 * Revision 1.1.140.1  1994/02/04  20:38:03  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:21:28  devsrc]
 * 
 * Revision 1.1.138.1  1993/12/07  17:39:47  jaffe
 * 	New File from Transarc 1.0.3a
 * 	[1993/12/02  21:57:40  jaffe]
 * 
 * $EndLog$
 */
/* Copyright (C) 1995, 1993 Transarc Corporation - All rights reserved */

/*
 * This file contains the symbol linkage table that we use in the
 * SunOS 5.x kernel to resolve references to symbols in the DFS core
 * module.  We must list each symbol here for which we defined a macro
 * in zlc.h.
 * NOTE: The symbols must appear in exactly the same order as their
 * declarations in zlc.h!
 */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/zlc/RCS/zlc_linkage.c,v 65.3 1998/03/23 16:28:24 gwehrman Exp $ */

#include <dcedfs/zlc.h>

#if defined(_KERNEL) && defined(AFS_SUNOS5_ENV)
void *zlc_symbol_linkage[ZLC_SYMBOL_LINKS] = {
    (void *)zlc_Init,
    (void *)zlc_TryRemove,
    (void *)zlc_CleanVolume,
    (void *)zlc_SetRestartState
};
#endif /* _KERNEL && AFS_SUNOS5_ENV */
