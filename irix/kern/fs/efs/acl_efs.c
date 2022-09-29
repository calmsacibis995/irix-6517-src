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
#ident	"$Revision: 1.4 $"

#ifdef OBSOLETE

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/acl.h>
#include <sys/cred.h>
#include <fs/efs/efs_inode.h>

/*
 * int 
 * acl_efs_iaccess(ip, cr, md)
 *    The access control process to determine the access permission:
 *     if uid == file owner id, use the file owner bits.
 *     if gid == file owner group id, use the file group bits.
 *     scan ACL for a maching user or group, and use matched entry
 *     permission. Use total permissions of all matching group entries,
 *     until all acl entries are exhausted. The final permission produced
 *     by matching acl entry or entries needs to be & with group permission.
 *     if not owner, owning group, or matching entry in ACL, use file
 *     other bits.
 */

int
acl_efs_iaccess(struct inode *ip, mode_t md, struct cred *cr)
{
	struct acl *ap;
	struct acl_acs aclacs_data;
        int error = 0;
	
	
	/*
	 * get the acl and go the entries
	 */
	
	ap = (struct acl *)kern_malloc(sizeof(struct acl));
	if (error = acl_eag_get(itovfs(ip), ip->i_number, ap, NULL)) {
		kern_free(ap);
		if (error == ENODATA) {
			return -1;
		}
		return error;
	}

	aclacs_data.own_uid = ip->i_uid;
	aclacs_data.own_gid = ip->i_gid;
	aclacs_data.own_mode = ip->i_mode;
	aclacs_data.ap = ap;
	error = acl_access(&aclacs_data, md, cr);
	kern_free(ap);
	return error;
}


/*
 * void
 * acl_efs_inherit(struct inode *pip, struct inode *ip)
 *   This function retrieves the parent inode's acl, processes it
 *   and let the child inode inherited the acl that it should.
 */

void
acl_efs_inherit(struct inode *pip, struct inode *ip)
{
	struct acl *ap = NULL;
	struct acl *dap = NULL;
	struct acl_entry *aclp;
	struct acl_entry *aclentp;
	struct acl_entry *user_1p = NULL;
	struct acl_entry *group_1p = NULL;
	struct acl_entry *baseacl = NULL;
	u_short owner_perm = (ip->i_mode >> 6) & 07;
	u_short group_perm = (ip->i_mode >> 3) & 07;
	u_short other_perm = ip->i_mode & 07;
	int entries = 0;
	int group_obj = 0;
	int groups = 0;
	int users = 0;
	int error = 0;
	int count = 0;
	int zap_acl = 0;
	int zap_dacl = 0;
	int i;


	ASSERT((pip->i_mode & IFMT) == IFDIR);

	/*
	 * clean up the ACL area for the new file
	 */
	
	ap = (struct acl *)kern_malloc(sizeof(struct acl));
	if (error = acl_eag_get(itovfs(ip), ip->i_number, ap, NULL)) {
		goto errout;
	}
	if (ap->acl_cnt > 0) {
		zap_acl++;
	}
	if (error = acl_eag_get(itovfs(ip), ip->i_number, NULL, ap)) {
		goto errout;
	}
	if (ap->acl_cnt > 0) {
		zap_dacl++;
	}

	/*
	 * Get the default acl from parent 
	 */

	dap = (struct acl *) kern_malloc(sizeof(struct acl));
	if (error = acl_eag_get(itovfs(pip), pip->i_number, NULL, dap)) {
		goto errout;
	}

	if (dap->acl_cnt == 0) {
		goto out;
	}

	/*
	 * go through the default acl to generate the acl to be
	 * inherited.
	 */

	ASSERT(acl_invalid(dap) == 0);

	baseacl = (struct acl_entry *)kern_malloc(NACLBASE * 
	                                          sizeof(struct acl_entry));
	baseacl[0].ae_type = USER_OBJ;
	baseacl[1].ae_type = GROUP_OBJ;
	baseacl[2].ae_type = CLASS_OBJ;
	baseacl[3].ae_type = OTHER_OBJ;
	baseacl[0].ae_id = 0;
	baseacl[1].ae_id = 0;
	baseacl[2].ae_id = 0;
	baseacl[3].ae_id = 0;
       	aclp = dap->acl_entry;
	for (i = 0; i < dap->acl_cnt; i++, aclp++) {
		aclp->ae_type &= ~ACL_DEFAULT;
		switch(aclp->ae_type) {
		case USER_OBJ:
			owner_perm &= (aclp->ae_perm & 07);
			baseacl[0].ae_perm = owner_perm;
			break;

		case USER:
			entries++;
			users++;
			if (!user_1p) {
				user_1p = aclp;
			}
			break;

		case GROUP_OBJ:
			group_obj++;
			baseacl[1].ae_perm = aclp->ae_perm;
			break;

		case GROUP:
			groups++;
			entries++;
			if (!group_1p) {
				group_1p = aclp;
			}
			break;

		case CLASS_OBJ:
			group_perm &= (aclp->ae_perm & 07);
			baseacl[2].ae_perm = group_perm;
			break;

		case OTHER_OBJ:
			other_perm &= (aclp->ae_perm & 07);
			baseacl[3].ae_perm = other_perm;
			break;
		}
	}

	ap->acl_cnt = entries + NACLBASE;
	aclentp = ap->acl_entry;

	/*
	 * put in the USER_OBJ base acl entry
	 */

	bcopy((char *)&baseacl[0], (char *)aclentp, sizeof(struct acl_entry));
	count++;
	aclentp++;

	/*
	 * put in all the USER acl entries
	 */

	if (users) {
		bcopy((char *)user_1p, (char *)aclentp, 
		      users * sizeof(struct acl_entry));
		aclentp += users;
		count += users;
	}

	/*
	 * put in the GROUP_OBJ base acl entry
	 */

	if (ap->acl_cnt == NACLBASE) {
		baseacl[1].ae_perm = baseacl[2].ae_perm;
	}
	bcopy((char *)&baseacl[1], (char *)aclentp, sizeof(struct acl_entry));
	count++;
	aclentp++;

	/*
	 * put in all the GROUP acl entries
	 */

	if (groups) {
		bcopy((char *)group_1p, (char *)aclentp, 
		      groups * sizeof(struct acl_entry));
		aclentp += groups;
		count += groups;
	}

	/*
	 * put in the CLASS_OBJ base acl entry
	 */

	bcopy((char *)&baseacl[2], (char *)aclentp, sizeof(struct acl_entry));
	count++;
	aclentp++;

	/*
	 * put in the OTHER_OBJ base acl entry
	 */

	bcopy((char *)&baseacl[3], (char *)aclentp, sizeof(struct acl_entry));
	count++;

	ASSERT(count == ap->acl_cnt);

	if ((ip->i_mode & IFMT) == IFDIR) {
		
		/*
		 * if the new inode is a directory, inherit its parent's
		 * default acl as its own default acl
		 */

		if (error = acl_eag_set(itovfs(ip), ip->i_number, ap, dap)) {
			goto errout;
		}
		zap_acl = 0;
	} else {
		if (error = acl_eag_set(itovfs(ip), ip->i_number, ap, NULL)) {
			goto errout;
		}
		zap_dacl = 0;
	}
	
	/*
	 * set the mode bits after successfully set the acl
	 */
	
	ip->i_mode &= ~(IREAD | IWRITE | IEXEC);
	ip->i_mode |= (owner_perm << 6) | (group_perm << 3) | other_perm;
	
out:
	if (zap_acl) {
		bzero(ap, sizeof(struct acl));
		if (error = acl_eag_set(itovfs(ip), ip->i_number, ap, NULL)) {
			goto errout;
		}
	}
	if (zap_dacl) {
		if (error = acl_eag_set(itovfs(ip), ip->i_number, NULL, ap)) {
			goto errout;
		}
	}

errout:
	if (ap) {
		kern_free(ap);
	}
	if (dap) {
		kern_free(dap);
	}
	if (baseacl) {
		kern_free(baseacl);
	}
	if (error) {
		if (error != ENODATA) {
			cmn_err(CE_WARN, "Error occurred in ACL inheritance\n");		}
	}	
}		
 
/*
 * int
 * acl_efs_mtoacl(struct inode *ip)
 *    The function reflects the mode change of a file to its corresponding
 *    ACL entries
 */

int
acl_efs_mtoacl(struct inode *ip)
{
	struct acl *ap;
	struct acl_entry *aclentp;
	u_short user_perm = (ip->i_mode >> 6) & 07;
	u_short class_perm = (ip->i_mode >> 3) & 07;
	u_short other_perm = ip->i_mode & 07;
	int i, error;

	/*
	 * get the acl of the ip if it has one
	 */
	
	ap = (struct acl *)kern_malloc(sizeof(struct acl));
	if ((error = acl_eag_get(itovfs(ip),ip->i_number,ap,NULL)) == ENODATA) {
		error = 0;
		goto out;
	}
	
	/*
	 * go through the mode related base acl entries, and make them
	 * consistent with the mode bits
	 */
	
	if (acl_invalid(ap)) {
		error = EINVAL;
		goto out;
	}

	aclentp = ap->acl_entry;
	for (i = 0; i < ap->acl_cnt; i++, aclentp++) {
		switch(aclentp->ae_type) {
		case USER_OBJ:
			aclentp->ae_perm = user_perm;
			break;

		case GROUP_OBJ:
			if (ap->acl_cnt == NACLBASE) {
				aclentp->ae_perm = class_perm;
			}
			break;

		case CLASS_OBJ:
			aclentp->ae_perm = class_perm;
			break;

		case OTHER_OBJ:
			aclentp->ae_perm = other_perm;
			break;
		}
	}

	error = acl_eag_set(itovfs(ip), ip->i_number, ap, NULL);
		
out:
	kern_free(ap);
	return error;
}

#endif /* OBSOLETE */
