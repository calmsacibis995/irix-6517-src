/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: xvfs_export.h,v $
 * Revision 65.1  1997/10/20 19:20:23  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.135.1  1996/10/02  19:04:08  damon
 * 	New DFS from Transarc
 * 	[1996/10/01  18:59:40  damon]
 *
 * Revision 1.1.130.2  1994/06/09  14:26:25  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:38:16  annie]
 * 
 * Revision 1.1.130.1  1994/02/04  20:37:15  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:21:08  devsrc]
 * 
 * Revision 1.1.128.1  1993/12/07  17:39:08  jaffe
 * 	New File from Transarc 1.0.3a
 * 	[1993/12/02  21:57:22  jaffe]
 * 
 * $EndLog$
 */
/*
 * Copyright (C) 1994, 1993 Transarc Corporation - All rights reserved
 */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/xvnode/RCS/xvfs_export.h,v 65.1 1997/10/20 19:20:23 jdoak Exp $*/

/*
 * Declarations of xvfs functions that are called directly from
 * outside the core component.
 */
#ifndef TRANSARC_XVFS_EXPORT_H
#define TRANSARC_XVFS_EXPORT_H

#include <dcedfs/osi.h>
#include <dcedfs/xvfs_vnode.h>

extern int xvfs_ConvertDev(struct vnode **avpp);
extern int xvfs_InitFromXFuns(
    struct xvfs_xops *axfuns,
    struct xvfs_vnodeops *afuns,
    struct osi_vnodeops *ofuncs);
extern int xvfs_convert(struct vnode *avp);
extern void xvfs_InitFromVFSOps(
    struct osi_vfsops *aofuns,
    struct xvfs_vfsops *afuns,
    int (*getvolfn)()
);
extern int xvfs_InitFromXOps(
    struct xvfs_xops *aofuns,
    struct xvfs_vnodeops *afuns
);
extern void xvfs_SetAdminGroupID(long id);
extern void xvfs_NullTxvattr(struct Txvattr *txvattrp);

#endif /* !TRANSARC_XVFS_EXPORT_H */
