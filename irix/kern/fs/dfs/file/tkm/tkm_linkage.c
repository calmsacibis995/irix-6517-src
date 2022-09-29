/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkm_linkage.c,v 65.3 1998/03/23 16:27:57 gwehrman Exp $";
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
 * $Log: tkm_linkage.c,v $
 * Revision 65.3  1998/03/23 16:27:57  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 19:58:40  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:18:05  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.115.1  1996/10/02  18:46:04  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:48:34  damon]
 *
 * Revision 1.1.110.2  1994/06/09  14:21:35  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:32:58  annie]
 * 
 * Revision 1.1.110.1  1994/02/04  20:31:50  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:18:40  devsrc]
 * 
 * Revision 1.1.108.1  1993/12/07  17:34:47  jaffe
 * 	New File from Transarc 1.0.3a
 * 	[1993/12/02  21:56:18  jaffe]
 * 
 * $EndLog$
 */
/* Copyright (C) 1993,1994 Transarc Corporation - All rights reserved */

/*
 * This file contains the symbol linkage table that we use in the
 * SunOS 5.x kernel to resolve references to symbols in the DFS core
 * module.  We must list each symbol here for which we defined a macro
 * in tkm_tokens.h and tkm_race.h.
 * NOTE: The symbols must appear in exactly the same order as their
 * declarations in tkm_tokens.h and tkm_race.h!
 */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkm/RCS/tkm_linkage.c,v 65.3 1998/03/23 16:27:57 gwehrman Exp $ */

#include <dcedfs/tkm_tokens.h>
#include <dcedfs/tkm_race.h>

#if defined(_KERNEL) && defined(AFS_SUNOS5_ENV)
void *tkm_symbol_linkage[] = {
    (void *)tkm_Init,
    (void *)tkm_GetToken,
    (void *)tkm_ReturnToken,
    (void *)tkm_GetRightsHeld,
    (void *)tkm_StartRacingCall,
    (void *)tkm_InitRace,
    (void *)tkm_EndRacingCall,
    (void *)tkm_RegisterTokenRace
};
#endif /* _KERNEL && AFS_SUNOS5_ENV */
