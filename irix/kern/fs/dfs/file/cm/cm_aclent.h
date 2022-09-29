/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: cm_aclent.h,v $
 * Revision 65.1  1997/10/20 19:17:22  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.42.1  1996/10/02  17:07:08  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:05:37  damon]
 *
 * Revision 1.1.36.2  1994/06/09  13:53:21  annie
 * 	fixed copyright in src/file
 * 	[1994/06/08  21:27:31  annie]
 * 
 * Revision 1.1.36.1  1994/02/04  20:07:31  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:07:13  devsrc]
 * 
 * Revision 1.1.34.1  1993/12/07  17:14:01  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  13:05:17  jaffe]
 * 
 * Revision 1.1.2.3  1993/01/18  20:49:18  cjd
 * 	embedded copyright notice
 * 	[1993/01/18  17:35:01  cjd]
 * 
 * Revision 1.1.2.2  1992/10/27  20:10:20  jaffe
 * 	Transarc delta: tu-ot4507-ot4713-ot4824-superdelta 1.1
 * 	  Selected comments:
 * 	    This super-delta was created from configuration dfs-102, revision 1.48,
 * 	    by importing the following deltas:
 * 	    kazar-ot5055-cm-conn-bad-returns 1.1
 * 	    tu-ot4507-after-tsr-merge 1.1
 * 	    tu-ot4507-cm-tsr-part-2 1.9
 * 	    tu-ot4713-add-one-more-tokenflag 1.2
 * 	    tu-ot4834-cm-using-a-new-tgt 1.4
 * 	    From tu-ot4834-cm-using-a-new-tgt 1.4:
 * 	    In current DFS 101 implementation there is no such mechanism for
 * 	    kinit to inform DFS's Cache Manager (CM) that a TGT ticket has been refreshed.
 * 	    Furthermore, there is also no mechanism for the CM itself to determine whether
 * 	    a TGT ticket is about to expire or not before the CM can issue an authenticated
 * 	    remote call. Eventually, the authenticated rpc call gets rejected due to using
 * 	    an expired TGT.
 * 	    This enhancement is to let 'kinit' inform the CM that a TGT has been
 * 	    renewed. So that, the CM can subsequently take appropriate actions to extend
 * 	    the lifetime of each authenticated rpc connection.
 * 	    Add a new field, tgtLifeTime in cm_aclent.
 * 	    same as above
 * 	    same
 * 	    more fixes
 * 	[1992/10/27  13:59:05  jaffe]
 * 
 * Revision 1.1  1992/01/19  02:46:08  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/
/* Copyright (C) 1989, 1990 Transarc Corporation - All rights reserved */

#ifndef _CM_ACLENT_H_
#define _CM_ACLENT_H_ 


#include <sys/time.h>

/*
 * Structure to hold an acl entry for a cached file
 */
struct cm_aclent {
    struct squeue lruq;		/* for quick removal from LRUQ */
    struct cm_aclent *next;	/* next guy same vnode */
    struct cm_scache *back;	/* back ptr to vnode */
    long randomUid;		/* random users' ids and access rights. */
    long randomAccess;		/* watch for more rights in acl.h */
    time_t tgtLifetime;		/* TGT's expiration time */
};

extern void cm_InitACLCache _TAKES((long));
extern long cm_GetACLCache _TAKES((struct cm_scache *,
				   struct cm_rrequest *,
				   long *));
extern void cm_AddACLCache _TAKES((struct cm_scache *,
				   struct cm_rrequest *,
				   long));
extern void cm_FreeAllACLEnts _TAKES((struct cm_scache *));
extern void cm_InvalidateACLUser _TAKES((struct cm_scache *, long));

#endif  /* _CM_ACLENT_H_ */
