#ident "$Revision: 1.11 $"

#include "types.h"
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/pfdat.h>
#include <sys/cred.h>
#include <sys/debug.h>
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

extern void nfs_purge_caches(bhv_desc_t *, cred_t *);

static xattr_cache_t *
xattr_cache_alloc (void)
{
	xattr_cache_t *eap = kmem_alloc(sizeof(*eap), KM_SLEEP);

	/* initialize fields */
	eap->ea_state = 0;
	mrinit(&eap->ea_lock, "xattr cache rwlock");

	/* return buffer */
	return (eap);
}

void
xattr_cache_free (xattr_cache_t *eap)
{
	mrfree(&eap->ea_lock);
	kmem_free(eap, sizeof(*eap));
}

__inline static const void *
xattr_value (xattr_cache_t *eap, int type, int *valuelenp)
{
	switch (type)
	{
		case XATTR_MAC:
			*valuelenp = _MAC_SIZE(eap->ea_mac);
			return ((void *) eap->ea_mac);
		case XATTR_MSEN:
			*valuelenp = _MSEN_SIZE(eap->ea_msen);
			return ((void *) eap->ea_msen);
		case XATTR_MINT:
			*valuelenp = _MINT_SIZE(eap->ea_mint);
			return ((void *) eap->ea_mint);
		case XATTR_CAP:
			*valuelenp = sizeof(eap->ea_cap);
			return ((void *) &eap->ea_cap);
		case XATTR_ACL:
			*valuelenp = sizeof(eap->ea_acl);
			return ((void *) &eap->ea_acl);
		case XATTR_DEF_ACL:
			*valuelenp = sizeof(eap->ea_def_acl);
			return ((void *) &eap->ea_def_acl);
		default:
			return ((void *) NULL);
	}
}

__inline static int
xattr_cache_copy (xattr_cache_t *eap, int type, void *value, int *valuelenp)
{
	int valid;

	if (valid = xattr_cache_valid(eap, type)) {
		mraccess(&eap->ea_lock);
		if (valid = xattr_cache_valid(eap, type)) {
			const void *attr = xattr_value(eap, type, valuelenp);
			if (attr != NULL)
				bcopy(attr, value, (size_t) *valuelenp);
			else
				valid = 0;
		}
		mrunlock(&eap->ea_lock);
	}
	return (valid);
}

static int
xattr_cache (bhv_desc_t *bdp, char *name, char *value, int *valuelenp,
	     int *typep, int *foundp)
{
	size_t len = strlen(name) + 1;
	struct rnode *rp = BHVTOR(bdp);
	xattr_cache_t *eap = rp->r_xattr;

	*typep = 0;
	if (len == sizeof (SGI_MAC_FILE) && !bcmp(name, SGI_MAC_FILE, len)) {
		if (*valuelenp < sizeof (*eap->ea_mac)) {
			*valuelenp = sizeof (*eap->ea_mac);
			return (E2BIG);
		}
		if (mac_enabled) {
			*typep = XATTR_MAC;
		}
		goto copy;
	}
	if (len == sizeof (SGI_BLS_FILE) && !bcmp(name, SGI_BLS_FILE, len)) {
		if (*valuelenp < sizeof (*eap->ea_msen)) {
			*valuelenp = sizeof (*eap->ea_msen);
			return (E2BIG);
		}
		if (mac_enabled) {
			*typep = XATTR_MSEN;
		}
		goto copy;
	}
	if (len == sizeof (SGI_BI_FILE) && !bcmp(name, SGI_BI_FILE, len)) {
		if (*valuelenp < sizeof (*eap->ea_mint)) {
			*valuelenp = sizeof (*eap->ea_mint);
			return (E2BIG);
		}
		if (mac_enabled) {
			*typep = XATTR_MINT;
		}
		goto copy;
	}
	if (len == sizeof (SGI_CAP_FILE) && !bcmp(name, SGI_CAP_FILE, len)) {
		if (*valuelenp < sizeof (eap->ea_cap)) {
			*valuelenp = sizeof (eap->ea_cap);
			return (E2BIG);
		}
		if (cap_enabled) {
			*typep = XATTR_CAP;
		}
		goto copy;
	}
	if (len == sizeof (SGI_ACL_FILE) && !bcmp(name, SGI_ACL_FILE, len)) {
		if (*valuelenp < sizeof (eap->ea_acl)) {
			*valuelenp = sizeof (eap->ea_acl);
			return (E2BIG);
		}
		if (acl_enabled) {
			*typep = XATTR_ACL;
		}
		goto copy;
	}
	if (len == sizeof (SGI_ACL_DEFAULT) && !bcmp(name, SGI_ACL_DEFAULT, len)) {
		if (*valuelenp < sizeof (eap->ea_def_acl)) {
			*valuelenp = sizeof (eap->ea_def_acl);
			return (E2BIG);
		}
		if (acl_enabled) {
			*typep = XATTR_DEF_ACL;
		}
		goto copy;
	}
copy:
	*foundp = xattr_cache_copy (eap, *typep, value, valuelenp);
	return (0);
}

static void
xattr_default (xattr_cache_t *eap, int type, vfs_t *vfsp)
{
	mrupdate(&eap->ea_lock);
	switch (type)
	{
		case XATTR_MAC:
		case XATTR_MSEN:
		case XATTR_MINT:
			if (_MAC_NFS_DEFAULT (eap, type, vfsp->vfs_mac))
				type = 0;
			break;
		case XATTR_CAP:
			eap->ea_cap.cap_effective = CAP_INVALID;
			eap->ea_cap.cap_permitted = CAP_INVALID;
			eap->ea_cap.cap_inheritable = CAP_INVALID;
			break;
		case XATTR_ACL:
			eap->ea_acl.acl_cnt = ACL_NOT_PRESENT;
			break;
		case XATTR_DEF_ACL:
			eap->ea_def_acl.acl_cnt = ACL_NOT_PRESENT;
			break;
	}
	xattr_cache_mark (eap, type);
	mrunlock(&eap->ea_lock);
}

static int
cap_nfs_get (bhv_desc_t *bdp, xattr_cache_t *eap, int type, int flags,
	     cred_t *cr)
{
	cap_set_t cap;
	int size = sizeof(cap);
	char *name = SGI_CAP_FILE;

	/* go on the wire for the attribute */
	if (getxattr_otw (bdp, name, (char *) &cap, &size, flags, cr))
		return (1);

	/* check validity */
	if ((cap.cap_inheritable & CAP_ALL_ON) != cap.cap_inheritable ||
	    (cap.cap_permitted & CAP_ALL_ON) != cap.cap_permitted ||
	    (cap.cap_effective & CAP_ALL_ON) != cap.cap_effective)
		return (1);

	/* copy it to cache */
	mrupdate(&eap->ea_lock);
	bcopy(&cap, &eap->ea_cap, sizeof(cap));
	xattr_cache_mark (eap, type);
	mrunlock(&eap->ea_lock);
	return (0);
}

static void
reload_xattr_cache (bhv_desc_t *bdp, int flags, cred_t *cr, int type, int dflt)
{
	xattr_cache_t *eap;
	vnode_t *vp = BHV_TO_VNODE(bdp);
	struct rnode *rp = BHVTOR(bdp);

	/* lock the rnode and allocate a cache if we don't have one */
	if (rp->r_xattr == NULL) {
		mutex_enter(&rp->r_statelock);
		if (rp->r_xattr == NULL)
			rp->r_xattr = xattr_cache_alloc ();
		mutex_exit(&rp->r_statelock);
	}
	eap = rp->r_xattr;

	/* use only default attributes */
	if (dflt) {
		xattr_default(eap, type, vp->v_vfsp);
		return;
	}

	switch (type)
	{
		case XATTR_MAC:
		case XATTR_MSEN:
		case XATTR_MINT:
			if (_MAC_NFS_GET (bdp, eap, type, flags, cr))
				xattr_default(eap, type, vp->v_vfsp);
			break;
		case XATTR_CAP:
			if (cap_nfs_get (bdp, eap, type, flags, cr))
				xattr_default (eap, type, vp->v_vfsp);
			break;
		case XATTR_ACL:
		case XATTR_DEF_ACL:
			if (_ACL_NFS_GET (bdp, eap, type, flags, cr))
				xattr_default(eap, type, vp->v_vfsp);
			break;
	}
}

int
getxattr_cache (bhv_desc_t *bdp, char *name, char *value, int *valuelenp,
		int flags, cred_t *cr, int *error)
{
	int found, type;
	vnode_t *vp = BHV_TO_VNODE(bdp);
	mntinfo_t *mi = vfs_bhvtomi(bdp);
	int dodefault = (vp->v_vfsp->vfs_flag & VFS_DEFXATTR) != 0;

	if (dodefault) {
		if (!(flags & ATTR_ROOT))
			return (*error = ENOATTR);
	} else {
		if ((mi->mi_flags & MIF_NOAC) || !(flags & ATTR_ROOT))
			return (*error = 0);
	}

	if (*error = xattr_cache (bdp, name, value, valuelenp, &type, &found))
		return (*error);

	if (!found && type) {
		reload_xattr_cache (bdp, flags, cr, type, dodefault);
		if (*error = xattr_cache (bdp, name, value, valuelenp,
					  &type, &found))
			return (*error);
	}

	return (found ? 1 : (dodefault ? (*error = ENOATTR) : 0));
}

/*
 * Get extended attributes over-the-wire.
 * Return 0 if successful, otherwise error.
 */
int
getxattr_otw (bhv_desc_t *bdp, char *name, char *value, int *valuelenp,
	      int flags, cred_t *cr)
{
	int rlock_held;
	rnode_t *rp = BHVTOR(bdp);
	int error;
	vnode_t *vp = BHV_TO_VNODE(bdp);
	struct GETXATTR1args args;
	struct GETXATTR1res res;

	args.object = *bhvtofh3(bdp);
	args.name = name;
	args.length = *valuelenp;
	if (vp->v_vfsp->vfs_flag & VFS_DOXATTR)
		args.flags = flags | ATTR_TRUST;
	else
		args.flags = flags & ~ATTR_TRUST;

	res.resok.size = res.resok.data.len = args.length;

	res.status = (enum nfsstat3) ENOATTR;
	res.resok.obj_attributes.attributes = FALSE;
	res.resfail.obj_attributes.attributes = FALSE;

	error = rlock_nohang(rp);

	if (error) {
		return error;
	}

	if (args.length == 0)
		res.resok.data.value = NULL;
	else
		res.resok.data.value = (char *) kmem_alloc (args.length,
							    KM_SLEEP);

	error = xattrcall (VN_BHVTOMI(bdp), XATTRPROC1_GETXATTR,
			   xdr_GETXATTR1args, (caddr_t) &args,
			   xdr_GETXATTR1res, (caddr_t) &res,
			   cr, &res.status);

	if (!error) {
		rlock_held = 1;
		error = geterrno3(res.status);
		if (!error) {
			nfs3_cache_post_op_attr(bdp, &res.resok.obj_attributes,
						cr, &rlock_held);
			if (rlock_held)
				runlock(rp);
			*valuelenp = res.resok.data.len;
			bcopy(res.resok.data.value, value, *valuelenp);
		}
		else {
			nfs3_cache_post_op_attr(bdp,
						&res.resfail.obj_attributes,
						cr, &rlock_held);
			if (rlock_held)
				runlock(rp);
			PURGE_STALE_FH(error, bdp, cr);
		}
	} else {
		runlock(rp);
	}

	if (res.resok.data.value != NULL)
		kmem_free((caddr_t) res.resok.data.value, res.resok.size);
	return (error);
}
