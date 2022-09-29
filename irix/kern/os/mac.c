/*
 * Copyright 1990, Silicon Graphics, Inc. 
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
#ident	"$Revision: 1.47 $"

#include "sys/types.h"
#include "sys/systm.h"
#include "sys/debug.h"
#include "sys/cmn_err.h"
#include "ksys/cred.h"
#include "sys/kabi.h"
#include "sys/proc.h"
#include "sys/vfs.h"
#include "sys/param.h"
#include "sys/errno.h"
#include "sys/vfs.h"
#include "sys/vnode.h"
#include "sys/fstyp.h"
#include "ksys/vfile.h"
#include "ksys/fdt.h"
#include "sys/uio.h"
#include "sys/pathname.h"
#include "sys/mac_label.h"
#include "sys/eag.h"
#include "sys/capability.h"
#include "sys/sat.h"
#include "sys/attributes.h"
#include "os/proc/pproc_private.h"	/* XXX bogus */
#include "ksys/fdt.h"

extern mac_label *mac_high_low_lp;
extern mac_label *mac_low_high_lp;

/*
 * Copy in a mac label.
 */
int
mac_copyin_label(mac_label *src, mac_label **result)
{
	mac_label ml;

	if (copyin((void *) src, (void *) &ml, sizeof(ml))) {
		*result = NULL;
		return(EFAULT);
	}

	*result = mac_add_label(&ml);
	return (*result == NULL ? EINVAL : 0);
}

/*
 * Copy the cred structure for the write. Set the label.
 */
static void
mac_setumac(mac_label *lp)
{
	proc_t *p = curprocp;
	struct cred *cr;

	(void) pcred_lock(p);
	cr = crcopy(p);
	cr->cr_mac = lp;
	pcred_push(p);
}

/* 
 * get and set the process label.
 */
int
mac_getplabel(mac_label *lp)
{
	cred_t *cr = get_current_cred();

	if (lp == NULL)
		return (EFAULT);

	if (copyout((caddr_t)cr->cr_mac, (caddr_t)lp, mac_size(cr->cr_mac)))
		return (EFAULT);

	return (0);
}

int
mac_setplabel(mac_label *lp, int userspace)
{
	int i;
	cred_t *cr = get_current_cred();

	if (userspace) {
		mac_label *nlp;

		if (i = mac_copyin_label(lp, &nlp))
			return (i);
		lp = nlp;
	}
	else {
		if ((lp = mac_add_label(lp)) == NULL)
			return EINVAL;
	}
	/*
	 * Audit the old label
	 */
	_SAT_SAVE_ATTR(SAT_MAC_LABEL_TOKEN, curuthread);

	/*
	 * If changing the label to the same value it's a noop.
	 */
	if (lp == cr->cr_mac) {
		_SAT_SETPLABEL(lp, 0);
		return (0);
	}
	/*
	 * Processes are never allowed to set "equal" sensitivity.
	 */
	if (lp->ml_msen_type == MSEN_EQUAL_LABEL) {
		_SAT_SETPLABEL(lp, EINVAL);
		return (EINVAL);
	}
	/*
	 * Changing the moldy state of the process requires CAP_MAC_MLD
	 * if capabilities are installed.
	 * NOTICE! CAP_MAC_RELABEL_SUBJ is NOT sufficient to change the
	 * moldyness of a process.
	 */
	if (mac_is_moldy(lp) != mac_is_moldy(cr->cr_mac) &&
	    !_CAP_CRABLE(cr, CAP_MAC_MLD)) {
		_SAT_SETPLABEL(lp, EPERM);
		return (EPERM);
	}
	
	/*
	 * Any other change requires CAP_MAC_RELABEL_SUBJ.
	 * Check for "equal" integrity.
	 */
	if (lp->ml_mint_type == MINT_EQUAL_LABEL || !mac_equ(lp, cr->cr_mac)) {
		if (!_CAP_CRABLE(cr, CAP_MAC_RELABEL_SUBJ)) {
			_SAT_SETPLABEL(lp, EPERM);
			return (EPERM);
		}
	}

	/*
	 * Transformation is acceptable.
	 */
	mac_setumac(lp);
	_SAT_SETPLABEL(lp, 0);
	return (0);
}

int
mac_vsetlabel(vnode_t *vp, mac_label *mlp)
{
	int error;

	VOP_ATTR_SET(vp, SGI_MAC_FILE, (char *)mlp, mac_size(mlp),
		     ATTR_ROOT, sys_cred, error);
	
	return error;
}

mac_label *
mac_vtolp(vnode_t *vp)
{
	int error;
	mac_label ml, *mlp;
	int mls = sizeof(ml);

	ASSERT(vp);

	VOP_ATTR_GET(vp, SGI_MAC_FILE, (char *)&ml, &mls, ATTR_ROOT,
		     sys_cred, error);

	if (error) {
#ifdef DEBUG
		cmn_err(CE_NOTE, "mac_vtolp: %s(%d) error=%d v_type=%d",
			__FILE__, __LINE__, error, vp ? vp->v_type : -1);
#endif
		return mac_high_low_lp;
	}

	if ((mlp = mac_add_label(&ml)) == NULL) {
#ifdef DEBUG
		cmn_err(CE_WARN, "mac_vtolp: %s(%d) mac_invalid(%x)",
			__FILE__, __LINE__, &ml);
#endif
		return mac_high_low_lp;
	}

	return mlp;
}

/* 
 * get and set the file label.
 */
int
mac_get(char *fname, int fdes, mac_label *lp)
{
	vnode_t *vp;
	int error = 0;
	mac_label *mlp;
	vfile_t *fp;

	_SAT_PN_BOOK(SAT_FILE_ATTR_READ, curuthread);

	if (fdes != -1 && fname != NULL)
		return (EINVAL);
	if (fdes == -1 && fname == NULL)
		return (EINVAL);

	if (fname) {
		if (error = lookupname(fname, UIO_USERSPACE, NO_FOLLOW,
				       NULLVPP, &vp, NULL))
			return (error);
	}
	else {
		if (error = getf(fdes, &fp))
			return (error);
		if (!VF_IS_VNODE(fp))
			return (EINVAL);
		vp = VF_TO_VNODE(fp);
		VN_HOLD(vp);
	}

	if ((mlp = mac_vtolp(vp)) == NULL) {
		/*
		 * XXX:casey
		 * This should never happen - the underlying file system
		 * dependent code should provide something.
		 */
		VN_RELE(vp);
		_SAT_ACCESS(SAT_FILE_ATTR_READ, error);
		return (EACCES);
	}
	if (mac_access(mlp, get_current_cred(), VREAD)) {
		VN_RELE(vp);
		_SAT_ACCESS(SAT_FILE_ATTR_READ, error);
		return (EACCES);
	}

	if (copyout((caddr_t)mlp, (caddr_t)lp, mac_size(mlp)))
		error = EFAULT;

	VN_RELE(vp);

	_SAT_ACCESS(SAT_FILE_ATTR_READ, error);
	return (error);
}

int
mac_set(char *fname, int fdes, mac_label *lp)
{
	vnode_t *vp;
	int error;
	mac_label *new_label;
	vfile_t *fp;
	cred_t *cr = get_current_cred();

	if (error = mac_copyin_label(lp, &new_label))
		return (error);

	if (new_label->ml_msen_type == MSEN_EQUAL_LABEL ||
	    new_label->ml_mint_type == MINT_EQUAL_LABEL) {
		if (!_CAP_ABLE(CAP_MAC_RELABEL_OPEN))
			return (EPERM);
	}

	if (!mac_equ(new_label, cr->cr_mac)) {
		if (mac_dom(new_label, cr->cr_mac)) {
			if (!_CAP_ABLE(CAP_MAC_UPGRADE))
				return (EPERM);
		}
		else if (!_CAP_ABLE(CAP_MAC_DOWNGRADE))
			return (EPERM);
	}

	if (fdes != -1 && fname != NULL)
		return (EINVAL);
	if (fdes == -1 && fname == NULL)
		return (EINVAL);

	_SAT_PN_BOOK(SAT_FILE_ATTR_WRITE, curuthread);

	if (fname) {
		if (error = lookupname(fname, UIO_USERSPACE, NO_FOLLOW,
				       NULLVPP, &vp, NULL))
			return (error);
	}
	else {
		if (error = getf(fdes, &fp))
			return (error);
		if (!VF_IS_VNODE(fp))
			return (EINVAL);
		vp = VF_TO_VNODE(fp);
		VN_HOLD(vp);
	}

	/*
	 * Verify that the caller has CAP_MAC_RELABEL_OPEN if required.
	 * Do a vanilla MAC write access check if that passes.
	 */
	if (vp->v_count > 1 && !_CAP_ABLE(CAP_MAC_RELABEL_OPEN))
		error = EBUSY;
	else
		error = mac_vaccess(vp, cr, VWRITE);

	/*
	 * Verify ownership
	 */
	if (!error) {
		vattr_t va;

		va.va_mask = AT_UID;
		VOP_GETATTR(vp, &va, 0, sys_cred, error);
		if (!error && va.va_uid != cr->cr_uid &&
		    !_CAP_CRABLE(cr, CAP_FOWNER))
			error = EACCES;
	}

	if (!error) {
		/*
		 * Only directories may be made MOLDY.
		 */
		if (mac_is_moldy(new_label) && vp->v_type != VDIR)
			error = ENOTDIR;
		/*
		 * Better not try to update a read-only file system.
		 */
		else if (vp->v_vfsp->vfs_flag & VFS_RDONLY)
			error = EROFS;
		/*
		 * Apply the change.
		 */
		else 
			error = mac_vsetlabel(vp, new_label);
	}


	VN_RELE(vp);
	_SAT_SETLABEL(new_label, error);
	return (error);
}

/*
 * Simply print a message to the effect that MAC is enabled.
 */
void
mac_confignote(void)
{
	cmn_err(CE_CONT, "Mandatory Access Control Enabled.\n");
}

/*
 * Derive the appropriate moldy subdirectory name for a label
 * on a file system.
 */
static void
mac_moldy_pathname(mac_label *lp, pathname_t *moldname)
{
	int i;
	int name_size = 3;		/* msen type, '-', and mint type */
	char *name = moldname->pn_path;
	char *letters = "bcdfhklmnrstvwxz"; /* All non-descending consonants */
	unsigned short *ep;

	*name++ = lp->ml_msen_type;
	if (lp->ml_level != 0) {
		*name++ = letters[(lp->ml_level >> 4) & 0x0f];
		*name++ = letters[lp->ml_level & 0x0f];
		name_size += 2;
	}
	*name++ = '-';
	*name++ = lp->ml_mint_type;
	if (lp->ml_grade != 0) {
		*name++ = letters[(lp->ml_grade >> 4) & 0x0f];
		*name++ = letters[lp->ml_grade & 0x0f];
		name_size += 2;
	}

	for (i = 0, ep = &lp->ml_list[0]; i < lp->ml_catcount; i++, ep++) {
		*name++ = '+';
		*name++ = letters[(*ep >> 4) & 0x0f];
		*name++ = letters[*ep & 0x0f];
		name_size += 3;
	}
	for (i = 0; i < lp->ml_divcount; i++, ep++) {
		*name++ = '-';
		*name++ = letters[(*ep >> 4) & 0x0f];
		*name++ = letters[*ep & 0x0f];
		name_size += 3;
	}

	*name = '\0';
	moldname->pn_pathlen = name_size;
	return;
}

/*
 * Moldy directory processing.
 * If passed in a moldy directory's vnode and the process isn't moldy
 * attempt to create a subdirectory at the user's label.
 * If it fails with anything other than EEXISTS, drop out.
 * Prepend the subdirectory's name to the caller's path and
 * return back to lookuppn.
 *
 * Sure, it sounds easy, but watch the error handling and credential
 * flipping.
 */
int
mac_moldy_path(vnode_t *vp, char *ct, pathname_t *pnp, cred_t *cred)
{
	mac_label *mlp;
	pathname_t moldname;
	vnode_t *nvp;
	vattr_t va;
	int error;

	/*
	 * Don't redirect on anything other than directories.
	 */
	if (vp->v_type != VDIR)
		return 0;
	/*
	 * Don't redirect moldy processes.
	 * This should probably be done before the v_type check, but
	 * that's a cheaper operation and I'd like to see the messages
	 * for debug purposes.
	 */
	if (mac_is_moldy(cred->cr_mac))
		return 0;

	/*
	 * Don't redirect on non-moldy directories
	 */
	if ((mlp = mac_vtolp(vp)) == NULL) {
#ifdef DEBUG
		cmn_err(CE_WARN, "mac_moldy_path: %s(%d)\n", __FILE__,__LINE__);
#endif
		return 0;
	}

	if (!mac_is_moldy(mlp))
		return 0;
	/*
	 * Get the attributes of the directory.
	 * Get them all in case we have to create a
	 * sub-directory with the same attributes.
	 * Use sys_cred to get an msenhigh/mintequal label and a
	 * superuser uid.
	 */
	va.va_mask = AT_ALL;
	VOP_GETATTR(vp, &va, 0, sys_cred, error);
	if (error) {
#ifdef	DEBUG
		cmn_err(CE_WARN, "mac_moldy_path: failed to get attrs\n");
#endif	/* DEBUG */
		return error;
	}

	pn_alloc(&moldname);
	/*
	 * If we got to a moldy directory via ".." keep going backwords.
	 */
	if (ct[0] == '.' && ct[1] == '.' && ct[2] == '\0') {
		moldname.pn_path[0] = '.';
		moldname.pn_path[1] = '.';
		moldname.pn_path[2] = '\0';
		moldname.pn_pathlen = 2;
	}
	/*
	 * Otherwise all conditions are meet.
	 * This is a moldy directory and redirection should be done.
	 * Get the name to redirect to.
	 */
	else {
		mac_moldy_pathname(cred->cr_mac, &moldname);
		/*
		 * Create the sub-directory with the same attributes.
		 * Use sys_cred to get an msenhigh/mintequal label and a
		 * superuser uid.
		 *
		 * Don't let a failure put you off. It usually means that the
		 * sub-directory already exists or you're on a read only
		 * file system, or something else which is OKay.
		 */
		VOP_MKDIR(vp, moldname.pn_path, &va, &nvp, sys_cred, error);
		/*
		 * If the sub-directory isn't new don't set its label!
		 */
		if (error == 0) {
			/*
			 * Set attributes of the newly created sub-directory.
			 */
			va.va_mask = AT_MODE | AT_UID | AT_GID;
			VOP_SETATTR(nvp, &va, 0, sys_cred, error);
			if (error) {
#ifdef	DEBUG
				cmn_err(CE_WARN,
				    "failed to set owner moldy subdir (%d)\n",
				    error);
#endif	/* DEBUG */
			}
			if (error = mac_vsetlabel(nvp, cred->cr_mac)) {
#ifdef	DEBUG
				cmn_err(CE_WARN,
				    "failed to relabel moldy subdir (%d)\n",
				    error);
#endif	/* DEBUG */
			}
			VN_RELE(nvp);
		}
	}

	/*
	 * Prepend the moldy sub-directory name to the look-up path.
	 * NOTE: ignore an error return from VOP_MKDIR or VOP_SETATTR.
	 */
	error = pn_insert(pnp, &moldname);
	ASSERT(error == 0);

	pn_free(&moldname);
	/*
	 * Return an indication that the path was inserted.
	 */
	return error ? error : -1;
}

/*
 *  Set the mac_enabled flag which is used to turn on MAC
 *  functionality through out the kernel.  Also, overide the
 *  the super user policy set in cap_init().  This policy flag
 *  should really be a tunable parameter, but until that is
 *  implemented, this is a convenient place to ensure that the
 *  policy is changed when Trix is installed.  Note:  mac_init()
 *  is called after cap_init() in main().
 */

void
mac_init()
{
	mac_enabled = 1;
	cap_enabled = CAP_SYS_NO_SUPERUSER;

}

int
mac_revoke(char *fname)
{
	vnode_t *vp;
	int error;

	if (!_CAP_ABLE(CAP_DEVICE_MGT))
		return (EPERM);

	if (error = lookupname(fname, UIO_USERSPACE, NO_FOLLOW, NULLVPP, &vp,
			       NULL))
		return (error);

	if (!(vp->v_flag & VSHARE)) {
		VN_RELE(vp);
#ifdef DEBUG
		cmn_err(CE_WARN,"mac_revoke: %s on non-VSHARE",
			get_current_name());
#endif
		return (EINVAL);
	}
	if (vp->v_type != VCHR) {
		VN_RELE(vp);
#ifdef DEBUG
		cmn_err(CE_WARN,"mac_revoke: %s on non-VCHR",
			get_current_name());
#endif
		return (EINVAL);
	}
	if (vp->v_count < 1) {
		VN_RELE(vp);
#ifdef DEBUG
		cmn_err(CE_WARN,"mac_revoke: %s on <1 count",
			get_current_name());
#endif
		return (EINVAL);
	}

	/*
	 * Do a vanilla MAC write access check.
	 */
	if (!(error = mac_vaccess(vp, get_current_cred(), VWRITE)))
		vn_kill(vp);

	VN_RELE(vp);

	return (error);
}

int
mac_vaccess(struct vnode *vp, struct cred *cr, mode_t mode)
{
	mac_label *mlp = mac_vtolp(vp);

	if (mlp)
		return mac_access(mlp, cr, mode);

#ifdef DEBUG
	cmn_err(CE_WARN,"mac_vaccess: %s(%d)", __FILE__, __LINE__);
#endif
	return 0;
}

void
mac_mountroot(struct vfs *vfsp)
{
	vfsp->vfs_mac = kern_malloc(sizeof(mac_vfs_t));
	vfsp->vfs_mac->mv_ipmac = mac_low_high_lp;
	vfsp->vfs_mac->mv_default = mac_low_high_lp;
}

void
mac_mount(struct vfs *vfsp, char *attrs)
{
	mac_label ml;

	if (!attrs)
		return;

	if (vfsp->vfs_mac == NULL) {
		vfsp->vfs_mac = kern_malloc(sizeof(mac_vfs_t));
		ASSERT(vfsp->vfs_mac);
		vfsp->vfs_mac->mv_default = NULL;
		vfsp->vfs_mac->mv_ipmac = NULL;
	}

	if (eag_parseattr(MAC_MOUNT_DEFAULT, attrs, &ml))
		vfsp->vfs_mac->mv_default = mac_add_label(&ml);

	if (eag_parseattr(MAC_MOUNT_IP, attrs, &ml))
		vfsp->vfs_mac->mv_ipmac = mac_add_label(&ml);
}

/*
 * Set the initial MAC value. Used after the creation of a symlink
 * because VOP_SYMLINK doesn't have the decency to return a vnode.
 */
int
mac_initial_path(char *fname)
{
	cred_t *cr = get_current_cred();
	mac_label *mlp;
	mac_label *lp = cr->cr_mac;
	vnode_t *vp;
	int error;

	if (error = lookupname(fname, UIO_USERSPACE, NO_FOLLOW, NULLVPP, &vp,
			       NULL))
		return (error);

	if (mac_is_moldy(lp)) {
		lp = mac_add_label(mlp = mac_demld(lp));
		kern_free(mlp);
	}

	error = mac_vsetlabel(vp, lp);

	VN_RELE(vp);
	return (error);
}
