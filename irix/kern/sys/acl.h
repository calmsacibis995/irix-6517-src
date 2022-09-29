/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __SYS_ACL_H__
#define __SYS_ACL_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 1.10 $"

#include <sys/types.h>

/*
 * Data types for Access Control Lists (ACLs)
 */
#define SGI_ACL_FILE	"SGI_ACL_FILE"
#define SGI_ACL_DEFAULT	"SGI_ACL_DEFAULT"

#define SGI_ACL_FILE_SIZE	12
#define SGI_ACL_DEFAULT_SIZE	15

#define ACL_NOT_PRESENT	-1
/*
 * Number of "base" ACL entries
 * (USER_OBJ, GROUP_OBJ, MASK, & OTHER_OBJ)
 */
#define NACLBASE	4
#define ACL_MAX_ENTRIES 25	/* Arbitrarily chosen number */

/*
 * Data types required by POSIX P1003.1eD15
 */
typedef ushort	acl_perm_t;
typedef int	acl_type_t;
typedef int	acl_tag_t;

/*
 * On-disk representation of an ACL.
 */
struct acl_entry {
	acl_tag_t 	ae_tag;
	uid_t	ae_id;
	acl_perm_t	ae_perm;	
};
typedef struct acl_entry * acl_entry_t;

struct acl {
	int			acl_cnt;	/* Number of entries */
	struct acl_entry	acl_entry[ACL_MAX_ENTRIES];
};

/*
 * Values for acl_tag_t
 */
#define ACL_USER_OBJ		0x01			/* owner */
#define ACL_USER		0x02			/* additional users */
#define ACL_GROUP_OBJ		0x04			/* group */
#define ACL_GROUP		0x08			/* additional groups */
#define ACL_MASK		0x10			/* mask entry */
#define ACL_OTHER_OBJ		0x20			/* other entry */
/*
 * Values for acl_type_t
 */
#define ACL_TYPE_ACCESS		0
#define ACL_TYPE_DEFAULT	1
/*
 * Values for acl_perm_t
 */
#define ACL_READ	04
#define ACL_WRITE	02
#define ACL_EXECUTE	01

typedef struct acl * acl_t;
typedef struct acl_entry * acl_permset_t; /* XXX:casey */

/*
 * User-space POSIX data types and functions.
 */
#ifndef _KERNEL

extern int acl_add_perm(acl_permset_t, acl_perm_t);
extern int acl_calc_mask(acl_t *);
extern int acl_clear_perms(acl_permset_t);
extern int acl_copy_entry(acl_entry_t, acl_entry_t);
extern ssize_t acl_copy_ext(void *, acl_t, ssize_t);
extern acl_t acl_copy_int(const void *);
extern int acl_create_entry(acl_t *, acl_entry_t *);
extern int acl_delete_def_file(const char *);
extern int acl_delete_entry(acl_t, acl_entry_t);
extern int acl_delete_perm(acl_permset_t, acl_perm_t);
extern acl_t acl_dup(acl_t);
extern int acl_free(void *);
extern acl_t acl_from_text(const char *);
extern int acl_get_entry(acl_t, int, acl_entry_t);
extern acl_t acl_get_fd(int);
extern acl_t acl_get_file(const char *, acl_type_t);
extern int acl_get_permset(acl_entry_t, acl_permset_t);
extern void *acl_get_qualifier(acl_entry_t);
extern int acl_get_tag_type(acl_entry_t, acl_tag_t *);
extern acl_t acl_init(int);
extern int acl_set_fd(int, acl_t);
extern int acl_set_file(const char *, acl_type_t, acl_t);
extern int acl_set_permset(acl_entry_t, acl_permset_t);
extern int acl_set_qualifier(acl_entry_t,const void *);
extern int acl_set_tag_type(acl_entry_t, acl_tag_t);
extern ssize_t acl_size(acl_t);
extern char *acl_to_text(acl_t, ssize_t *);
extern char *acl_to_short_text(acl_t, ssize_t *);
extern int acl_valid(acl_t);

#endif /* _KERNEL */

/* function prototypes */

#ifdef _KERNEL
struct xfs_inode;
struct cred;
struct vattr;
struct vnode;
struct bhv_desc;
struct xattr_cache;

extern void acl_confignote(void);
extern void acl_sync_mode(mode_t, struct acl *);
extern int  acl_vaccess(struct vnode *, mode_t, struct cred *);
extern int  acl_vchmod(struct vnode *, struct vattr *);
extern int  acl_vtoacl(struct vnode *, struct acl *, struct acl *);
extern int  acl_access(uid_t, gid_t, struct acl *, mode_t, struct cred *);
extern int  acl_inherit(struct vnode *, struct vnode *, struct vattr *);
extern int  acl_invalid(struct acl *);
extern int  acl_xfs_iaccess(struct xfs_inode *, mode_t, struct cred *);
extern int  acl_nfs_iaccess(struct bhv_desc *, mode_t, struct cred *);
extern int  acl_hwg_iaccess(dev_t, uid_t, gid_t, mode_t, mode_t, struct cred *);
extern int  acl_hwg_get(dev_t, char *, char *, int *, int);
extern int  acl_hwg_match(const char *, int);
extern int  acl_nfs_get(struct bhv_desc *, struct xattr_cache *, int, int,
			struct cred *);
extern int  acl_get(char *, int, struct acl *, struct acl *);
extern int  acl_set(char *, int, struct acl *, struct acl *);

/* Define macros choosing stub functions or real functions here */
extern int acl_enabled;

#define _ACL_CONFIGNOTE()	((acl_enabled)? acl_confignote(): (void)0)
#define _ACL_VACCESS(v,m,c)	((acl_enabled)? acl_vaccess(v,m,c) : -1)
#define _ACL_VCHMOD(v,a)	((acl_enabled)? acl_vchmod(v,a) : -1)
#define _ACL_VTOACL(v,a,d)	((acl_enabled)? acl_vtoacl(v,a,d) : 0)
#define _ACL_INHERIT(p,c,v)	((acl_enabled)? acl_inherit(p,c,v) : 0)
#define _ACL_XFS_IACCESS(a,b,c)	((acl_enabled)? acl_xfs_iaccess(a,b,c) : -1)

#define _ACL_NFS_IACCESS(i,m,c)	((acl_enabled) ? acl_nfs_iaccess(i,m,c) : -1)
#define _ACL_HWG_IACCESS(v,u,g,m,fm,c) ((acl_enabled) ? acl_hwg_iaccess(v,u,g,m,fm,c) : -1)
#define _ACL_HWG_GET(a,b,c,d,e) ((acl_enabled) ? acl_hwg_get(a,b,c,d,e) : -1)
#define _ACL_HWG_SET(n,f)	((acl_enabled) ? acl_hwg_match(n,f) : 0)
#define _ACL_HWG_REMOVE(n,f)	((acl_enabled) ? acl_hwg_match(n,f) : 0)
#define	_ACL_NFS_GET(b,e,r,f,c)	((acl_enabled) ? acl_nfs_get(b,e,r,f,c) : 1)

#define _ACL_GET(a,b,c,d)	((acl_enabled)? acl_get(a,b,c,d) : ENOSYS)
#define _ACL_SET(a,b,c,d)	((acl_enabled)? acl_set(a,b,c,d) : ENOSYS)

#else /* _KERNEL */

extern struct acl *sgi_acl_strtoacl(char *);
extern int sgi_acl_acltostr(struct acl *, char *);
extern int sgi_acl_get(char *, int, struct acl *, struct acl *);
extern int sgi_acl_set(char *, int, struct acl *, struct acl *);

#endif /* _KERNEL */

extern int dac_installed;	/* flag to tell if DAC is installed */

#ifdef __cplusplus
}
#endif

#endif	/* __SYS_ACL_H_ */
