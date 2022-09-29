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
#include <sys/mac_label.h>
#include <sys/attributes.h>
#include <ksys/behavior.h>

extern struct mac_label *mac_equal_equal_lp;

/*ARGSUSED*/
int
mac_fifo_attr_get(
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
