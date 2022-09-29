#ident "$Revision: 1.2 $"

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
mac_nfs_iaccess(bhv_desc_t *bdp, mode_t mode, struct cred *cr)
{
	struct mac_label lbl;
	int error, flags = ATTR_ROOT | ATTR_TRUST, size = sizeof(lbl);

	/*
	 * Look in the cache for the label. If it's not there,
	 * ask the server for it. If we still can't get it,
	 * refuse access.
	 */
	if (!getxattr_cache(bdp, SGI_MAC_FILE, (char *) &lbl, &size, flags,
			    cr, &error))
		if (!error)
			error = getxattr_otw(bdp, SGI_MAC_FILE, (char *) &lbl,
					     &size, flags, cr);

	return (error ? error : (mac_access(&lbl, cr, mode) ? EACCES : 0));
}

union mac_nfs_buf {
	mac_label	mac;
	mac_b_label	msen;
	mac_b_label	mint;
};

int
mac_nfs_get (bhv_desc_t *bdp, xattr_cache_t *eap, int type, int flags,
	     cred_t *cr)
{
	union mac_nfs_buf buf;
	void *ptr;
	int size;
	char *name;

	switch (type)
	{
		case XATTR_MAC:
			size = sizeof (buf.mac);
			name = SGI_MAC_FILE;
			break;
		case XATTR_MSEN:
			size = sizeof (buf.msen);
			name = SGI_BLS_FILE;
			break;
		case XATTR_MINT:
			size = sizeof (buf.mint);
			name = SGI_BI_FILE;
			break;
		default:
			return (1);
	}

	if (getxattr_otw (bdp, name, (char *) &buf, &size, flags, cr))
		return (1);

	switch (type)
	{
		case XATTR_MAC:
			ptr = mac_add_label (&buf.mac);
			if (ptr == NULL)
				return (1);
			mrupdate(&eap->ea_lock);
			eap->ea_mac = ptr;
			xattr_cache_mark (eap, type);
			mrunlock(&eap->ea_lock);
			break;
		case XATTR_MSEN:
			ptr = msen_add_label (&buf.msen);
			if (ptr == NULL)
				return (1);
			mrupdate(&eap->ea_lock);
			eap->ea_msen = ptr;
			xattr_cache_mark (eap, type);
			mrunlock(&eap->ea_lock);
			break;
		case XATTR_MINT:
			ptr = mint_add_label (&buf.mint);
			if (ptr == NULL)
				return (1);
			mrupdate(&eap->ea_lock);
			eap->ea_mint = ptr;
			xattr_cache_mark (eap, type);
			mrunlock(&eap->ea_lock);
			break;
	}
	return (0);
}

int
mac_nfs_default (xattr_cache_t *eap, int type, mac_vfs_t *mvp)
{
	extern mac_t mac_high_low_lp;
	mac_t defmac = mac_high_low_lp;

	switch (type)
	{
		case XATTR_MAC:
			if (mvp != NULL && mvp->mv_default != NULL)
				eap->ea_mac = mvp->mv_default;
			else
				eap->ea_mac = defmac;
			break;
		case XATTR_MSEN:
			if (mvp != NULL && mvp->mv_default != NULL)
				eap->ea_msen = msen_from_mac(mvp->mv_default);
			else
				eap->ea_msen = msen_from_mac(defmac);
			break;
		case XATTR_MINT:
			if (mvp != NULL && mvp->mv_default != NULL)
				eap->ea_mint = mint_from_mac(mvp->mv_default);
			else
				eap->ea_mint = mint_from_mac(defmac);
			break;
		default:
			return (1);
	}
	return (0);
}
