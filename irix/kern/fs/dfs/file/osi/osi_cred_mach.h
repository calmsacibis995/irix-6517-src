/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_cred_mach.h,v $
 * Revision 65.2  1998/03/06 00:09:46  bdr
 * Added osi_pcred_lock, osi_pcred_unlock, and osi_set_thread_creds defines.
 *
 * Revision 65.1  1997/10/24  14:29:50  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:44  jdoak
 * Initial revision
 *
 * Revision 64.1  1997/02/14  19:46:50  dce
 * *** empty log message ***
 *
 * Revision 1.2  1996/04/06  00:17:28  bhim
 * No Message Supplied
 *
 * Revision 1.1.80.3  1994/07/28  17:37:58  mckeen
 * 	Set OSI_HAS_CR_PAG, was set before seems to have
 * 	been lost in a merge
 * 	[1994/07/27  20:11:16  mckeen]
 *
 * Revision 1.1.80.2  1994/06/09  14:14:59  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:27:35  annie]
 * 
 * Revision 1.1.80.1  1994/02/04  20:23:51  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:15:23  devsrc]
 * 
 * Revision 1.1.78.1  1993/12/07  17:29:24  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  15:57:27  jaffe]
 * 
 * Revision 1.1.2.4  1993/01/21  14:50:00  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  14:51:46  cjd]
 * 
 * Revision 1.1.2.3  1992/09/15  20:31:25  jaffe
 * 	sync with Transarc
 * 	[92/09/15            jaffe]
 * 
 * Revision 1.1.2.2  1992/09/15  13:15:34  jaffe
 * 	Transarc delta: jaffe-ot4608-ucred-cleanup 1.3
 * 	  Selected comments:
 * 	    use osi macros for ucred access.  New macros are osi_GetNGroups,
 * 	    osi_SetNGroups, osi_GetUID, osi_GetRUID, and osi_GetGID.  New
 * 	    constants are OSI_NGROUPS, OSI_NOGROUP, and OSI_MAXGROUPS.
 * 	    add initial start at this file.  There is still more work to go before
 * 	    this file will compile.
 * 	    include osi_cred.h for appropriate definitions.
 * 	    fixed typo. GetGID takes only one argument.
 * 	[1992/09/14  20:14:39  jaffe]
 * 
 * $EndLog$
 */
/*
 *      Copyright (C) 1992 Transarc Corporation
 *      All rights reserved.
 */

#ifndef TRANSARC_OSI_CRED_MACH_H
#define	TRANSARC_OSI_CRED_MACH_H

#include <sys/param.h>
#ifdef SGIMIPS
#include <sys/cred.h>
#endif
#if 0
#include <sys/user.h>
#endif

typedef cred_t osi_cred_t;

#define OSI_MAXGROUPS	NGROUPS

/*
 * Set true if the cred struct has a member containing the length of the
 * cr_groups list; otherwise, the list is assumed to be terminated by
 * the special value OSI_NOGROUP.
 */
#define OSI_CR_GROUPS_COUNTED	1

/*
 * Set true if the cred struct has a member that records the PAG; if
 * false, the PAG is stored in the first two cr_groups entries.
 */
#define OSI_HAS_CR_PAG		0

#define osi_SetNGroups(c,n) ((c)->cr_ngroups = (n))
#define osi_GetNGroups(c) ((c)->cr_ngroups)

#define osi_SetUID(c, u)  ((c)->cr_uid = (u))
#define osi_GetUID(c)     ((c)->cr_uid)

#define osi_GetRUID(c)    ((c)->cr_ruid)

#define osi_SetGID(c, g)  ((c)->cr_gid = (g))
#define osi_GetGID(c)     ((c)->cr_gid)

#if 0
#define osi_getucred()	  curprocp->p_cred
#define osi_setucred(c)	  curprocp->p_cred = c
#endif
#define osi_getucred()	  get_current_cred()
#define osi_setucred(c)   set_current_cred(c)

#ifdef KERNEL
#define osi_pcred_lock(p)
#define osi_pcred_unlock(p)
#define osi_set_thread_creds(p, cr)
#endif /* KERNEL */
#endif /* TRANSARC_OSI_CRED_MACH_H */
