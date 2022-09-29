/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: xaggr_linkage.c,v 65.3 1998/03/23 16:42:47 gwehrman Exp $";
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
 * $Log: xaggr_linkage.c,v $
 * Revision 65.3  1998/03/23 16:42:47  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 20:00:53  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:20:33  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.184.1  1996/10/02  21:11:32  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:51:37  damon]
 *
 * Revision 1.1.179.2  1994/06/09  14:25:36  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:37:33  annie]
 * 
 * Revision 1.1.179.1  1994/02/04  20:36:15  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:20:40  devsrc]
 * 
 * Revision 1.1.177.1  1993/12/07  17:38:21  jaffe
 * 	New File from Transarc 1.0.3a
 * 	[1993/12/02  21:57:04  jaffe]
 * 
 * $EndLog$
 */
/* Copyright (C) 1993 Transarc Corporation - All rights reserved */

/*
 * This file contains the symbol linkage table that we use in the
 * SunOS 5.x kernel to resolve references to symbols in the DFS core
 * module.
 */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/xaggr/RCS/xaggr_linkage.c,v 65.3 1998/03/23 16:42:47 gwehrman Exp $ */

#include <dcedfs/aggr.h>

#if defined(_KERNEL) && defined(AFS_SUNOS5_ENV)
void *xaggr_symbol_linkage[] = {
    (void *)agOpvec
};
#endif /* _KERNEL && AFS_SUNOS5_ENV */
