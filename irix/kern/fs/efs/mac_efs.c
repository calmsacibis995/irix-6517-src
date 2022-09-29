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
#ident	"$Revision: 1.9 $"

#include <sys/types.h>
#include <sys/cred.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/mac_label.h>
#include <sys/attributes.h>
#include <ksys/behavior.h>
#include <fs/efs/efs_inode.h>

extern struct mac_label *mac_equal_equal_lp;

/*ARGSUSED*/
int
mac_efs_iaccess( struct inode *ip, struct cred *cr, mode_t mode )
{
	return (mac_access(mac_equal_equal_lp, cr, mode) ? EACCES : 0);
}

/*ARGSUSED*/
int
mac_efs_attr_get(
        bhv_desc_t *bdp,
        char *name,
        char *value,
        int *valuelenp,
        int flags,
        struct cred *cred)
{
	size_t len = strlen(name) + 1;

	if ((flags & ATTR_ROOT) == 0)
		return (ENOATTR);

	if (len == sizeof(SGI_MAC_FILE) && !bcmp(name, SGI_MAC_FILE, len)) {
		if (*valuelenp < MAC_PL_SIZE) {
			*valuelenp = MAC_PL_SIZE;
			return (E2BIG);
		}
		bcopy(mac_equal_equal_lp, value, *valuelenp = MAC_PL_SIZE);
		return (0);
	}
	return (ENOATTR);
}


/*ARGSUSED*/
int
mac_efs_attr_set(
        bhv_desc_t *bdp,
        char *name,
        char *value,
        int valuelenp,
        int flags,
        struct cred *cred)
{
	return 0;
}
