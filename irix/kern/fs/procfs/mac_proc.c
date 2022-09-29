/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)uts-comm:fs/procfs/prvnops.c	1.15"*/
#ident	"$Revision: 1.3 $"

#include <sys/types.h>
#include <sys/cred.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/mac_label.h>
#include <sys/attributes.h>
#include <ksys/behavior.h>
#include <ksys/cred.h>
#include <ksys/vproc.h>
#include "prdata.h"
#include "os/proc/pproc_private.h"

int
mac_proc_attr_get(
	bhv_desc_t *bdp,
	char *name,
	char *value,
	int *valuelenp,
	int flags,
	struct cred *cred)
{
	extern struct mac_label *mac_low_high_lp;
	vnode_t *vp = BHV_TO_VNODE(bdp);
	prnode_t *pnp = BHVTOPRNODE(bdp);
	size_t len = strlen(name) + 1;
	int macf, macp, error;
	cred_t *pcr;

	if ((flags & ATTR_ROOT) == 0)
		return (ENOATTR);

	/* check name */
	macf = (len == sizeof(SGI_MAC_FILE) && !bcmp(name, SGI_MAC_FILE, len));
	macp = (len == sizeof(SGI_MAC_PROCESS) &&
		!bcmp(name, SGI_MAC_PROCESS, len));
	if (!macf && !macp)
		return (ENOATTR);

	/* handle the directory case */
	if (vp->v_type == VDIR) {
		if (macf) {
			if (*valuelenp < MAC_PL_SIZE) {
				*valuelenp = MAC_PL_SIZE;
				return (E2BIG);
			}
			bcopy(mac_low_high_lp, value,
			      *valuelenp = MAC_PL_SIZE);
			return (0);
		}
		return (ENOATTR);
	}

	/* handle the regular file case */
	if (pnp->pr_pflags & PRINVAL)
		return (EAGAIN);

	if (error = prlock(pnp, ZYES, PRNONULL))
		return (error);

	/* get cred */
	pcr = pcred_access(pnp->pr_proc);

	/* check access to this file */
	if (error = mac_access(pcr->cr_mac, cred, VREAD)) {
		pcred_unaccess(pnp->pr_proc);
		prunlock(pnp);
		return (error);
	}

	if (macf || macp) {
		int attrlen = mac_size(pcr->cr_mac);
		if (*valuelenp < attrlen) {
			*valuelenp = attrlen;
			error = E2BIG;
		} else {
			bcopy(pcr->cr_mac, value, *valuelenp = attrlen);
		}
	} else
		error = ENOATTR;

	pcred_unaccess(pnp->pr_proc);
	prunlock(pnp);
	return (error);
}
