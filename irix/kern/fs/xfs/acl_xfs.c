/*
 * Copyright 1992, Silicon Graphics, Inc. 
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
 */
#ident	"$Revision: 1.3 $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/eag.h>
#include <sys/mac_label.h>
#include <sys/systm.h>
#include <sys/attributes.h>
#include <sys/capability.h>
#include <sys/acl.h>
#include <sys/uuid.h>
#include <sys/buf.h>
#include "xfs_types.h"
#include "xfs_inum.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_dir.h"
#include "xfs_dir2.h"
#include "xfs_mount.h"
#include "xfs_alloc_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_itable.h"
#include "xfs_btree.h"
#include "xfs_ialloc.h"
#include "xfs_alloc.h"
#include "xfs_bmap.h"
#include "xfs_attr_sf.h"
#include "xfs_dir_sf.h"
#include "xfs_dir2_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode_item.h"
#include "xfs_inode.h"
#include "xfs_da_btree.h"
#include "xfs_attr.h"

int
acl_xfs_iaccess( xfs_inode_t *ip, mode_t mode, struct cred *cr )
{
	struct acl acl;

	/*
	 * If the file has no ACL return -1.
	 */
	if (xfs_attr_fetch(ip, SGI_ACL_FILE, (char *)&acl, sizeof(struct acl)))
		return -1;

	/*
	 * If the file has an empty ACL return -1.
	 */
	if (acl.acl_cnt == ACL_NOT_PRESENT)
		return -1;

	/*
	 * Synchronize ACL with mode bits
	 */
	acl_sync_mode(ip->i_d.di_mode, &acl);

	return acl_access(ip->i_d.di_uid, ip->i_d.di_gid, &acl, mode, cr);
}
