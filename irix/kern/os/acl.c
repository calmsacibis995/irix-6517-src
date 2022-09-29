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
#ident	"$Revision: 1.18 $"

#include "sys/types.h"
#include "sys/systm.h"
#include "sys/debug.h"
#include "sys/cmn_err.h"
#include "sys/cred.h"
#include "sys/vfs.h"
#include "sys/param.h"
#include "sys/errno.h"
#include "sys/vfs.h"
#include "sys/vnode.h"
#include "sys/fstyp.h"
#include "ksys/fdt.h"
#include "ksys/vfile.h"
#include "sys/uio.h"
#include "sys/pathname.h"
#include "sys/acl.h"
#include "sys/sat.h"
#include "sys/stat.h"
#include "sys/attributes.h"
#include "sys/uthread.h"
#include "ksys/fdt.h"

static int	acl_setmode(vnode_t *, struct acl *);

/*
 * Simply print a message to the effect that ACLs are enabled.
 */
void
acl_confignote(void)
{
	cmn_err(CE_CONT, "Access Control Lists Enabled.\n");
}

/*
 * The access control process to determine the access permission:
 *	if uid == file owner id, use the file owner bits.
 *	if gid == file owner group id, use the file group bits.
 *	scan ACL for a maching user or group, and use matched entry
 *	permission. Use total permissions of all matching group entries,
 *	until all acl entries are exhausted. The final permission produced
 *	by matching acl entry or entries needs to be & with group permission.
 *	if not owner, owning group, or matching entry in ACL, use file
 *	other bits.
 */
static int
acl_capability_check(mode_t mode, struct cred *cr)
{
	if ((mode & ACL_READ) && !cap_able_cred(cr, CAP_DAC_READ_SEARCH))
		return EACCES;
	if ((mode & ACL_WRITE) && !cap_able_cred(cr, CAP_DAC_WRITE))
		return EACCES;
	if ((mode & ACL_EXECUTE) && !cap_able_cred(cr, CAP_DAC_EXECUTE))
		return EACCES;
	return 0;
}

int
acl_access(uid_t fuid, gid_t fgid, struct acl *fap, mode_t md, struct cred *cr)
{
	int i;
	struct acl_entry matched;
	int maskallows = -1;	/* true, but not 1, either */
	int allows;

	/*
	 * Invalid type
	 */
	matched.ae_tag = 0;
	/*
	 * Normalize the bits for comparison
	 */
	md >>= 6;

	for (i = 0; i < fap->acl_cnt && matched.ae_tag != ACL_USER; i++) {
		/*
		 * Break out if we've got a user_obj entry or 
		 * a user entry and the mask
		 */
		if (matched.ae_tag == ACL_USER_OBJ)
			break;
		if (matched.ae_tag == ACL_USER) {
			if (maskallows != -1)
				break;
			if (fap->acl_entry[i].ae_tag != ACL_MASK)
				continue;
		}
		/*
		 * True iff this entry allows the requested access
		 */
		allows = ((fap->acl_entry[i].ae_perm & md) == md);

		switch (fap->acl_entry[i].ae_tag) {
		case ACL_USER_OBJ:
			if (fuid != cr->cr_uid)
				continue;
			matched.ae_tag = ACL_USER_OBJ;
			matched.ae_perm = allows;
			break;
		case ACL_USER:
			if (fap->acl_entry[i].ae_id != cr->cr_uid)
				continue;
			matched.ae_tag = ACL_USER;
			matched.ae_perm = allows;
			break;
		case ACL_GROUP_OBJ:
			if ((matched.ae_tag == ACL_GROUP_OBJ ||
			    matched.ae_tag == ACL_GROUP) && !allows)
				continue;
			if (!groupmember(fgid, cr))
				continue;
			matched.ae_tag = ACL_GROUP_OBJ;
			matched.ae_perm = allows;
			break;
		case ACL_GROUP:
			if ((matched.ae_tag == ACL_GROUP_OBJ ||
			    matched.ae_tag == ACL_GROUP) && !allows)
				continue;
			if (!groupmember(fap->acl_entry[i].ae_id, cr))
				continue;
			matched.ae_tag = ACL_GROUP;
			matched.ae_perm = allows;
			break;
		case ACL_MASK:
			maskallows = allows;
			break;
		case ACL_OTHER_OBJ:
			if (matched.ae_tag != 0)
				continue;
			matched.ae_tag = ACL_OTHER_OBJ;
			matched.ae_perm = allows;
			break;
		}
	}
	/*
	 * First possibility is that no matched entry allows access.
	 * The capability to override DAC may exist, so check for it.
	 */
	switch (matched.ae_tag) {
	case ACL_OTHER_OBJ:
	case ACL_USER_OBJ:
		if (matched.ae_perm)
			return 0;
		break;
	case ACL_USER:
	case ACL_GROUP_OBJ:
	case ACL_GROUP:
		if (maskallows && matched.ae_perm)
			return 0;
		break;
	case 0:
		break;
	}
	return acl_capability_check(md, cr);
}

/*
 * ACL validity checker.
 * acl_invalid(struct acl *aclp)
 *   This acl validation routine does the check of each acl entry read
 *   from disk makes sense.
 */

int 
acl_invalid(struct acl *aclp)
{
	int i;
	int count = 0;

	if (aclp->acl_cnt < 0 || aclp->acl_cnt > ACL_MAX_ENTRIES) {
		return EINVAL;
	}

	for (i = 0; i < aclp->acl_cnt; i++) {
		switch (aclp->acl_entry[i].ae_tag) {
		case ACL_USER_OBJ:
		case ACL_USER:
		case ACL_GROUP_OBJ:
		case ACL_GROUP:
		case ACL_MASK:
		case ACL_OTHER_OBJ:
			if (aclp->acl_entry[i].ae_id < 0) {
				return EINVAL;
			}
			count++;
			break;
		default:
			return EINVAL;
		}
	}
	if (count != aclp->acl_cnt)
		return EINVAL;

	return 0;
}

void
acl_init(void)
{
	acl_enabled = 1;
}

int
acl_vtoacl(vnode_t *vp, struct acl *access_acl, struct acl *default_acl)
{
	int error = 0;
	int i = sizeof(struct acl);
	vattr_t	va;

	if (access_acl != NULL) {
		/*
		 * Get the Access ACL and the mode.  If either cannot
		 * be obtained for some reason, invalidate the access ACL.
		 */
		VOP_ATTR_GET(vp, SGI_ACL_FILE, (char *) access_acl, &i,
			     ATTR_ROOT, sys_cred, error);

		if (!error) {
			/*
			 * Got the ACL, need the mode...
			 */
			va.va_mask = AT_MODE;
			VOP_GETATTR(vp, &va, 0, sys_cred, error);
		}

		if (error) {
			access_acl->acl_cnt = ACL_NOT_PRESENT;
		} else {
			/*
			 * We have a good ACL and the file mode,
			 * synchronize them...
			 */
			acl_sync_mode(va.va_mode, access_acl);
		}
	}

	if (default_acl != NULL) {
		VOP_ATTR_GET(vp, SGI_ACL_DEFAULT, (char *) default_acl, &i,
			     ATTR_ROOT, sys_cred, error);
		if (error)
			default_acl->acl_cnt = ACL_NOT_PRESENT;
	}
	return error;
}

int
acl_vaccess(vnode_t *vp, mode_t md, struct cred *cr)
{
	vattr_t va;
	struct acl acl;
	int error;

	if (error = acl_vtoacl(vp, &acl, NULL))
		return ACL_NOT_PRESENT;

	va.va_mask = AT_UID | AT_GID;
	VOP_GETATTR(vp, &va, 0, sys_cred, error);
	if (error)
		return error;

	return acl_access(va.va_uid, va.va_gid, &acl, md, cr);
}

/*
 * This function retrieves the parent directory's acl, processes it
 * and lets the child inherit the acl(s) that it should.
 */

int
acl_inherit(vnode_t *pvp, vnode_t *vp, vattr_t *vap)
{
	struct acl pdacl, cacl;
	int error = 0;

	/*
	 * If the parent does not have a default ACL, or it's an
	 * invalid ACL, we're done.
	 */
	if (pvp == NULL || vp == NULL)
		return (0);
	if (acl_vtoacl(pvp, NULL, &pdacl) || acl_invalid(&pdacl))
		return (0);

	/*
	 * Copy the default ACL of the containing directory to
	 * the access ACL of the new file and use the mode that
	 * was passed in to set up the correct initial values for
	 * the u::,g::[m::], and o:: entries.  This is what makes
	 * umask() "work" with ACL's.
	 */
	bcopy (&pdacl, &cacl, sizeof (cacl));
	acl_sync_mode(vap->va_mode, &cacl);

	/*
	 * Set the default and access acl on the file.  The mode is already
	 * set on the file, so we don't need to worry about that.
	 *
	 * If the new file is a directory, its default ACL is a copy of
	 * the containing directory's default ACL.
	 *
	 */
	if (vp->v_type == VDIR) {
		VOP_ATTR_SET(vp, SGI_ACL_DEFAULT, (char *) &pdacl,
			     sizeof (pdacl), ATTR_ROOT, sys_cred, error);
	}
	if (!error) {
		VOP_ATTR_SET(vp, SGI_ACL_FILE, (char *) &cacl,
			     sizeof (cacl), ATTR_ROOT, sys_cred, error);
	}

	return (error);
}

#define ACL_ACCESS	0
#define ACL_DEFAULT	1

static int
acl_vget(vnode_t *vp, int kind, struct acl *acl)
{
	struct acl kacl;
	int size = sizeof(kacl);
	char	*attrname;
	int error = 0;
	vattr_t	va;

	attrname = (kind == ACL_ACCESS) ? SGI_ACL_FILE : SGI_ACL_DEFAULT;
#ifdef	SERIOUS_DEBUG
	cmn_err(CE_NOTE, "acl_vget 0x%x %s 0x%x", vp, attrname, acl);
#endif	/* SERIOUS_DEBUG */

	/*
	 * Get the ACL if there is one...
	 */
	bzero(&kacl, size);	/* Make sure we don't copyout random stack */
	VOP_ATTR_GET(vp, attrname, (char *) &kacl, &size, ATTR_ROOT,
		     sys_cred, error);

	if (!error && acl_invalid(&kacl)) {
#ifdef	SERIOUS_DEBUG
		cmn_err(CE_WARN, "Invalid acl fetched");
#endif	/* SERIOUS_DEBUG */
		error = EINVAL;
	}

	if (!error && (kind == ACL_ACCESS)) {
		/*
		 * For Access ACLs, get the mode for synchronization.
		 */
		va.va_mask = AT_MODE;
		VOP_GETATTR(vp, &va, 0, sys_cred, error);
	}

	/*
	 * If there was an error retrieving or validating the ACL or 
	 * an Access ACL and we had trouble synchronizing the mode with the
	 * ACL, then the ACL is deemed NOT PRESENT.
	 */
	if (error) {
		kacl.acl_cnt = ACL_NOT_PRESENT;
	} else if (kind == ACL_ACCESS) {
		/*
		 * Synchronize an Access ACL with the mode before
		 * copying it out.
		 */
		acl_sync_mode(va.va_mode, &kacl);
	}


	/*
	 * If the whole problem was that the requested ACL does not exist, then
	 * there is no problem.  Just copy out a NOT PRESENT ACL.  Otherwise,
	 * don't do the copyout (an error should leave user level data
	 * unchanged).
	 */
	if (error == ENOATTR)
		error = 0;

	if (!error && copyout((caddr_t)&kacl, (caddr_t)acl,
			      sizeof(struct acl))) {
		error = EFAULT;
	}
	return (error);
}

int
acl_get(char *fname, int fdes, struct acl *acl, struct acl *dacl)
{
	vnode_t *vp;
	vfile_t *fp;
	int error;
	int derror = 0;

	if (fdes != -1 && fname != NULL)
		return (EINVAL);
	if (fdes == -1 && fname == NULL)
		return (EINVAL);

	if (!acl && !dacl)
		return (EINVAL);

	_SAT_PN_BOOK(SAT_FILE_ATTR_READ, curuthread);

	if (fname) {
		if (error=lookupname(fname,UIO_USERSPACE,NO_FOLLOW,NULLVPP,&vp,						NULL))
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

	error = _MAC_VACCESS(vp, curuthread->ut_cred, VREAD);

	if (!error) {
		if (acl)
			error = acl_vget(vp, ACL_ACCESS, acl);
		if (dacl)
			derror = acl_vget(vp, ACL_DEFAULT, dacl);
	}
	
	VN_RELE(vp);

	/*
	 * It's not likely that this will happen.
	 */
	if (!error && derror)
		error = derror;

	_SAT_ACCESS(SAT_FILE_ATTR_READ, error);
	return (error);
}

static int
acl_vset(vnode_t *vp, struct acl *acl)
{
	int error;

#ifdef	NOISE
	cmn_err(CE_NOTE, "acl_vset 0x%x 0x%x", vp, acl);
#endif	/* NOISE */

	/*
	 * Check for an ACL deletion (the caller specifies a
	 * NOT PRESENT ACL).
	 */
	if (acl->acl_cnt == ACL_NOT_PRESENT) {
		/*
		 * Deletion, remove the ACL if there is one.
		 */
		VOP_ATTR_REMOVE(vp, SGI_ACL_FILE, ATTR_ROOT, sys_cred, error);
		return(error);
	}

	/*
	 * The incoming ACL exists, so set the file mode based on
	 * the incoming ACL.
	 */
	acl_setmode(vp, acl);

	/*
	 * Now set the ACL. Use the system credential to put it in
	 * the ROOT attribute space.
	 */
	VOP_ATTR_SET(vp, SGI_ACL_FILE, (char *) acl, sizeof(struct acl),
		     ATTR_ROOT, sys_cred, error);
	if (error == ENOATTR) {
		/* There was no Access ACL to delete, no big deal. */
		error = 0;
	}
	return error;
}

static int
dacl_vset(vnode_t *vp, struct acl *dacl)
{
	int	error = 0;

	if (dacl->acl_cnt != ACL_NOT_PRESENT) {
		/*  Apply the default ACL to the file */
		VOP_ATTR_SET(vp, SGI_ACL_DEFAULT, (char *) dacl,
			     sizeof (struct acl), ATTR_ROOT, sys_cred, error);
	} else {
		/*
		 * Delete the ACL on the file.  If none is there, ignore the
		 * error.  Report other errors to the caller.
		 */
		VOP_ATTR_REMOVE(vp, SGI_ACL_DEFAULT, ATTR_ROOT, sys_cred,
				error);
		if (error == ENOATTR) {
			/* There was no default ACL to delete, no big deal. */
			error = 0;
		}
	}
	return(error);
}

/*
 * Set the ACLs on a file system object.  Either or both the Access or
 * Default ACL may be set using this function.  If the 'acl' pointer is
 * non-NULL the Access ACL is set, if the 'dacl' pointer is non-NULL the
 * Default ACL is set.
 */
int
acl_set(char *fname, int fdes, struct acl *acl, struct acl *dacl)
{
	struct acl kacl;
	struct acl kdacl;
	vattr_t va;
	vnode_t *vp;
	vfile_t *fp;
	cred_t *cr = curuthread->ut_cred;
	int error;

	if (fdes != -1 && fname != NULL)
		return (EINVAL);
	if (fdes == -1 && fname == NULL)
		return (EINVAL);

	if (!acl && !dacl)
		return (EINVAL);
	if (acl && copyin((caddr_t)acl, (caddr_t)&kacl, sizeof(struct acl)))
		return (EFAULT);
	if (dacl && copyin((caddr_t)dacl, (caddr_t)&kdacl, sizeof(struct acl)))
		return (EFAULT);

	_SAT_PN_BOOK(SAT_FILE_ATTR_WRITE, curuthread);

	if (fname) {
		if (error=lookupname(fname,UIO_USERSPACE,NO_FOLLOW,NULLVPP,&vp,						NULL))
			return (error);
	} else {
		if (error = getf(fdes, &fp))
			return (error);
		if (!VF_IS_VNODE(fp))
			return (EINVAL);
		vp = VF_TO_VNODE(fp);
		VN_HOLD(vp);
	}

	/*
	 * Only directories may have default acls
	 * Better not try to update a read-only file system.
	 */
	if (dacl && vp->v_type != VDIR)
		error = ENOTDIR;
	else if (vp->v_vfsp->vfs_flag & VFS_RDONLY)
		error = EROFS;
	else {
		error = _MAC_VACCESS(vp, cr, VWRITE);
		if (!error) {
			va.va_mask = AT_UID;
			VOP_GETATTR(vp, &va, 0, cr, error);
			if (!error && va.va_uid != cr->cr_uid &&
			    !cap_able_cred(cr, CAP_FOWNER))
				error = EACCES;
		}
		if (!error && acl)
			/*
			 * Set the access ACL.
			 */
			error = acl_vset(vp, &kacl);
		if (!error && dacl)
			/*
			 * Set the default ACL.
			 */
			error = dacl_vset(vp, &kdacl);
	}

	VN_RELE(vp);
	_SAT_SETACL(acl ? &kacl : NULL, dacl ? &kdacl : NULL, error);
	return (error);
}

/*
 * Set up the correct mode on the file based on the supplied ACL.  This
 * makes sure that the mode on the file reflects the state of the
 * u::,g::[m::], and o:: entries in the ACL.  Since the mode is where
 * the ACL is going to get the permissions for these entries, we must
 * synchronize the mode whenever we set the ACL on a file.
 */
static int
acl_setmode(vnode_t *vp, struct acl *acl)
{
	vattr_t		va;
	acl_entry_t	gap = (acl_entry_t)0;
	acl_entry_t	ap;
	int		nomask = 1;
	int		i;
	int		error;

	if (acl->acl_cnt == ACL_NOT_PRESENT) {
		/*
		 * Nothing in the ACL, just return no error.
		 */
		return (0);
	}

	/*
	 * Copy the u::, g::, o::, and m:: bits from the ACL into the
	 * mode.  The m:: bits take precedence over the g:: bits.
	 */
	va.va_mask = AT_MODE;
	VOP_GETATTR(vp, &va, 0, sys_cred, error);
	if (error != 0) {
		return (error);
	}
	va.va_mask = AT_MODE;
	va.va_mode &= ~PERMMASK;
	ap = acl->acl_entry;
	for (i = 0; i < acl->acl_cnt; ++i) {
		switch (ap->ae_tag) {
		case ACL_USER_OBJ:
			va.va_mode |= ap->ae_perm << 6;
			break;
		case ACL_GROUP_OBJ:
			gap = ap;
			break;
		case ACL_MASK:
			nomask = 0;
			va.va_mode |= ap->ae_perm << 3;
			break;
		case ACL_OTHER_OBJ:
			va.va_mode |= ap->ae_perm;
			break;
		default:
			break;
		}
		ap++;
	}

	/*
	 * Set the group bits from ACL_GROUP_OBJ iff there's no
	 * ACL_MASK
	 */
	if (gap && nomask)
		va.va_mode |= gap->ae_perm << 3;

	VOP_SETATTR(vp, &va, 0, sys_cred, error);

	return error;
}

/*
 * The permissions for the special ACL entries (u::, g::[m::], o::) are
 * actually stored in the file mode (if there is both a group and a mask,
 * the group is stored in the ACL entry and the mask is stored on the file).
 * This allows the mode to remain automatically in sync with the ACL without
 * the need for a call-back to the ACL system at every point where the mode
 * could change.  This function takes the permissions from the specified mode
 * and places it in the supplied ACL.
 *
 * This implementation draws its validity from the fact that, when the
 * ACL was assigned, the mode was copied from the ACL (see acl_vset()
 * and acl_setmode()).  If the mode did not change, therefore, the mode
 * remains exactly what was taken from the special ACL entries at
 * assignment. If a subsequent chmod() was done, the POSIX spec says
 * that the change in mode must cause an update to the ACL seen at user
 * level and used for access checks.  Before and after a mode change,
 * therefore, the file mode most accurately reflects what the special
 * ACL entries should permit / deny.
 *
 * CAVEAT: If someone sets the SGI_ACL_FILE attribute directly,
 *         the existing mode bits will override whatever is in the
 *         ACL. Similarly, if there is a pre-existing ACL that was
 *         never in sync with its mode (owing to a bug in 6.5 and
 *         before), it will now magically (or mystically) be
 *         synchronized.  This could cause slight astonishment, but
 *         it is better than inconsistent permissions.
 *
 * The supplied ACL is a template that may contain any combination
 * of special entries.  These are treated as place holders when we fill
 * out the ACL.  This routine does not add or remove special entries, it
 * simply unites each special entry with its associated set of permissions.
 */
void
acl_sync_mode(mode_t mode, struct acl *acl)
{
	int i;
	int nomask = 1;
	acl_entry_t ap;
	acl_entry_t gap = NULL;

	/*
	 * Set ACL entries. POSIX1003.1eD16 requires that the MASK
	 * be set instead of the GROUP entry, if there is a MASK.
	 */
	for (ap = acl->acl_entry, i = 0; i < acl->acl_cnt; ap++, i++) {
		switch (ap->ae_tag) {
		case ACL_USER_OBJ:
			ap->ae_perm = (mode >> 6) & 0x7;
			break;
		case ACL_GROUP_OBJ:
			gap = ap;
			break;
		case ACL_MASK:
			nomask = 0;
			ap->ae_perm = (mode >> 3) & 0x7;
			break;
		case ACL_OTHER_OBJ:
			ap->ae_perm = mode & 0x7;
			break;
		default:
			break;
		}
	}
	/*
	 * Set the ACL_GROUP_OBJ iff there's no ACL_MASK
	 */
	if (gap && nomask)
		gap->ae_perm = (mode >> 3) & 0x7;
}
