/* Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.      */
/* Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T        */
/* All Rights Reserved          */

/* THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF       */
/* UNIX System Laboratories, Inc.                       */
/* The copyright notice above does not evidence any     */
/* actual or intended publication of such source code.  */

/* #ident       "@(#)uts-comm:fs/procfs/prvnops.c       1.15" */
#ident	"$Revision: 1.2 $"

#include <sys/types.h>
#include <ksys/cred.h>
#include <sys/debug.h>
#include <sys/dirent.h>
#include <sys/fpu.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/fs_subr.h>
#include <sys/kmem.h>
#include <sys/ksignal.h>
#include <sys/param.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/sat.h>
#include <sys/mac_label.h>
#include <sys/mac.h>
#include <sys/capability.h>
#include <sys/attributes.h>
#include <sys/cmn_err.h>
#include <ksys/vproc.h>
#include <string.h>
#include "autofs.h"

extern mac_label *mac_low_high_lp;

/* ARGSUSED */
int
mac_autofs_attr_get (
	bhv_desc_t * bdp,
	char *name,
	char *value,
	int *valuelenp,
	int flags,
	struct cred *cred)
{
	size_t len = strlen (name) + 1;

	if ((flags & ATTR_ROOT) == 0)
		return (ENOATTR);

	if (len == sizeof (SGI_MAC_FILE) && !bcmp (name, SGI_MAC_FILE, len))
	{
		if (*valuelenp < MAC_PL_SIZE)
		{
			*valuelenp = MAC_PL_SIZE;
			return (E2BIG);
		}

		/*
		 * Make mount points dblow.
		 */
		bcopy (mac_low_high_lp, value, MAC_PL_SIZE);
		*valuelenp = MAC_PL_SIZE;
		return 0;
	}
	else
	{
		return (ENOATTR);
	}
}

/* ARGSUSED */
int
mac_autofs_attr_set (
	bhv_desc_t * bdp,
	char *name,
	char *value,
	int valuelenp,
	int flags,
	struct cred *cred)
{
	size_t len = strlen (name) + 1;
	mac_t mac;

	if ((flags & ATTR_ROOT) == 0)
		return (ENOATTR);

	if (len == sizeof (SGI_MAC_FILE) && !bcmp (name, SGI_MAC_FILE, len))
	{
		/* 
		 * Make mount points dblow.
		 */
		mac = mac_add_label ((mac_t) value);
		return (mac == mac_low_high_lp ? 0 : EINVAL);
	}
	else
	{
		return (ENOATTR);
	}

}

static struct
{
	attrlist_t attr_list;
	struct
	{
		u_int32_t a_valuelen;
		char a_name[sizeof (SGI_MAC_FILE)];
	}
	entry;
} synthlist = 
{
	{1, 0, sizeof (attrlist_t)},
	{MAC_PL_SIZE, SGI_MAC_FILE},
};

/* ARGSUSED */
int
mac_autofs_attr_list (
	bhv_desc_t * bdp,
	char *buffer,
	int bufsize,
	int flags,
	attrlist_cursor_kern_t * cursor,
	struct cred *cred)
{
	if ((flags & ATTR_ROOT) == 0)
		return (ENOATTR);

	if (bufsize < sizeof (synthlist))
		return (EINVAL);

	bcopy (&synthlist, buffer, sizeof (synthlist));
	return (0);
}
