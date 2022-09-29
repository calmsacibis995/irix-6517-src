/*
 * Copyright 1993, Silicon Graphics, Inc. 
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

#ifdef OBSOLETE

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/acl.h>
#include <sys/cred.h>
#include <sys/fs/efs_inode.h>

int 
acl_nfs_vaccess(struct vnode *vp, struct vattr *v, mode_t mode, 
		struct cred *cr)
{

	struct acl *ap;
	struct acl_acs aclacs_data;
        int error = 0;
	
	
	/*
	 * get the acl and go the entries
	 */
	
	ap = (struct acl *)kern_malloc(sizeof(struct acl));
	if (error = acl_eag_get(vp->v_vfsp, v->va_nodeid, ap, NULL)) {
		kern_free(ap);
		if (error == ENODATA) {
			return -1;
		}
		return error;
	}

	aclacs_data.own_uid = v->va_uid;
	aclacs_data.own_gid = v->va_gid;
	aclacs_data.own_mode = v->va_mode;
	aclacs_data.ap = ap;
	error = acl_access(&aclacs_data, mode, cr);
	kern_free(ap);
	return error;
}

#endif /* OBSOLETE */
