#ident "$Revision: 1.4 $"

#include "types.h"
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/cred.h>
#include <sys/systm.h>
#include <assert.h>
#include "xdr.h"
#include "nfs.h"
#include "nfs_clnt.h"
#include "nfs3_clnt.h"
#include "rnode.h"
#include "nfs3_rnode.h"
#include "auth.h"
#include "svc.h"
#include "xattr.h"

int
acl_nfs_iaccess(bhv_desc_t *bdp, mode_t mode, struct cred *cr)
{
	struct acl acl;
	struct vattr va;
	int error, flags = ATTR_ROOT | ATTR_TRUST, size = sizeof(acl);

	/*
	 * Look in the cache for the ACL. If it's not there,
	 * ask the server for it. If we still can't get it,
	 * assume there isn't one.
	 */
	acl.acl_cnt = ACL_NOT_PRESENT;
	if (!getxattr_cache(bdp, SGI_ACL_FILE, (char *) &acl, &size, flags,
			    cr, &error))
		if (!error)
			error = getxattr_otw(bdp, SGI_ACL_FILE, (char *) &acl,
					     &size, flags, cr);

	if (error || acl.acl_cnt == ACL_NOT_PRESENT)
		return (-1);
	error = nfs3getattr(bdp, &va, 0, cr);
	if (error)
		return (error);
	acl_sync_mode(va.va_mode, &acl);
	return (acl_access(va.va_uid, va.va_gid, &acl, mode, cr));
}

int
acl_nfs_get (bhv_desc_t *bdp, xattr_cache_t *eap, int type, int flags,
	     cred_t *cr)
{
	struct acl acl;
	int size = sizeof (acl);
	char *name;

	switch (type)
	{
		case XATTR_ACL:
			name = SGI_ACL_FILE;
			break;
		case XATTR_DEF_ACL:
			name = SGI_ACL_DEFAULT;
			break;
		default:
			return (1);
	}

	if (getxattr_otw (bdp, name, (char *) &acl, &size, flags, cr))
		return (1);

	switch (type)
	{
		case XATTR_ACL:
			if (acl_invalid (&acl))
				return (1);
			mrupdate(&eap->ea_lock);
			bcopy(&acl, &eap->ea_acl, sizeof(acl));
			xattr_cache_mark (eap, type);
			mrunlock(&eap->ea_lock);
			break;
		case XATTR_DEF_ACL:
			if (acl_invalid (&acl))
				return (1);
			mrupdate(&eap->ea_lock);
			bcopy(&acl, &eap->ea_def_acl, sizeof(acl));
			xattr_cache_mark (eap, type);
			mrunlock(&eap->ea_lock);
			break;
	}
	return (0);
}
