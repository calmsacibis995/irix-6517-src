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
#ident	"$Revision: 1.7 $"

#ifdef OBSOLETE
#include <sys/types.h>
#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/mac_label.h>
/*
#include "auth_unix.h"
#include "bootparam.h"
#include "export.h"
#include "klm_prot.h"
#include "lockmgr.h"
#include "mount.h"
#include "nfs.h"
#include "nfs_clnt.h"
#include "pmap_prot.h"
#include "rnode.h"
#include "rpc_msg.h"
#include "svc.h"
#include "svc_auth.h"
*/
#include "types.h"
#include "xdr.h"
#include "auth.h"
#include "clnt.h"

extern int	nfs_getattr(struct vnode *, struct vattr *, int, struct cred *);

mac_label *
mac_nfs_getlabel( ino_t number, struct vnode *vp )
{
	return (mac_eag_getlabel(vp->v_vfsp, number));
}

int
mac_nfs_setlabel( mac_label *lp, struct vnode *vp, struct cred *cr, int domold )
{
	int error;
	vattr_t *vap;

	if (mac_is_moldy(lp)) {
		if (domold == MAC_MOLDOK) {
			if (vp->v_type != VDIR)
				return (ENOTDIR);
		}
		else
			lp = mac_unmold(lp);
	}
	/*
	 * Fetch the inode number.
	 */
	vap = kern_malloc(sizeof(vattr_t));
	vap->va_mask = AT_MAC;
	if (!(error = nfs_getattr(vp, vap, 0, cr)))
		error =  mac_eag_setlabel(vp->v_vfsp, vap->va_nodeid, lp);

	kern_free(vap);
	return (error);
}

#endif /* OBSOLETE */
