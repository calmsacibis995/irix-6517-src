/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.		     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)uts-comm:fs/procfs/prvnops.c	1.15"*/
#ident	"$Revision: 1.2 $"

#include <sys/types.h>
#include <sys/cred.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/mac_label.h>
#include <sys/attributes.h>
#include <ksys/behavior.h>

/*ARGSUSED*/
int
mac_pipe_attr_get(
	bhv_desc_t *bdp,
	char *name,
	char *value,
	int *valuelenp,
	int flags,
	struct cred *cred)
{
	size_t nlen = strlen(name) + 1;

	if ((flags & ATTR_ROOT) == 0)
		return (ENOATTR);

	if (nlen == sizeof(SGI_MAC_FILE) && !bcmp(name, SGI_MAC_FILE, nlen)) {
		int attrlen = mac_size(cred->cr_mac);
		if (*valuelenp < attrlen) {
			*valuelenp = attrlen;
			return (E2BIG);
		}
		bcopy(cred->cr_mac, value, *valuelenp = attrlen);
		return (0);
	}
	return (ENOATTR);
}

/*ARGSUSED*/
int
mac_pipe_attr_set(
	bhv_desc_t *bdp,
	char *name,
	char *value,
	int valuelen,
	int flags,
	struct cred *cred)
{
#if 0
	/*REFERENCED*/
	vnode_t *vp = BHV_TO_VNODE(bdp);
	cmn_err(CE_PANIC, "mac_pipe_attr_set %s(%d)", __FILE__, __LINE__);
#endif
	return 0;
}
