
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	__MAC_LABEL_
#define	__MAC_LABEL_

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.44 $"

/*
 * Allow for 26 (A-Z) MSEN label types.
 * Expanding this will require a bigger table in mac_dom.c
 */
#define MSEN_MIN_LABEL_NAME	'A'	/* No label name less than this */
#define MSEN_MAX_LABEL_NAME	'Z'	/* No label name greater than this */
#define MSEN_LABEL_NAME_COUNT	26	/* Number of label types */
#define MSEN_MIN_LEVEL		0	/* No level less than this 	*/
#define MSEN_MAX_LEVEL		255	/* No level greater than this 	*/
#define MSEN_MIN_CATEGORY	0	/* No category less than this 	*/
#define MSEN_MAX_CATEGORY	65534	/* No category greater than this */

/*
 * MSEN label type names. Choose an upper case ASCII character.
 */
#define MSEN_ADMIN_LABEL	'A'	/* Admin: low < admin != tcsec < high */
#define MSEN_EQUAL_LABEL	'E'	/* Wildcard - always equal */
#define MSEN_HIGH_LABEL		'H'	/* System High - always dominates */
#define MSEN_MLD_HIGH_LABEL	'I'	/* System High, multi-level dir */
#define MSEN_LOW_LABEL		'L'	/* System Low - always dominated */
#define MSEN_MLD_LABEL		'M'	/* TCSEC label on a multi-level dir */
#define MSEN_MLD_LOW_LABEL	'N'	/* System Low, multi-level dir */
#define MSEN_TCSEC_LABEL	'T'	/* TCSEC label */
#define MSEN_UNKNOWN_LABEL	'U'	/* unknown label */

/*
 * Allow for 26 (a-z) MINT label types.
 * Expanding this will require a bigger table in mac_dom.c
 */
#define MINT_MIN_LABEL_NAME	'a'	/* No label name less than this */
#define MINT_MAX_LABEL_NAME	'z'	/* No label name greater than this */
#define MINT_LABEL_NAME_COUNT	26	/* Number of label types */
#define MINT_MIN_GRADE		0	/* No grade less than this 	*/
#define MINT_MAX_GRADE		255	/* No grade greater than this 	*/
#define MINT_MIN_DIVISION	0	/* No division less than this 	*/
#define MINT_MAX_DIVISION	65534	/* No division greater than this */

/*
 * MINT label type names. Choose a lower case ASCII character.
 */
#define MINT_BIBA_LABEL		'b'	/* Dual of a TCSEC label */
#define MINT_EQUAL_LABEL	'e'	/* Wildcard - always equal */
#define MINT_HIGH_LABEL		'h'	/* High Grade - always dominates */
#define MINT_LOW_LABEL		'l'	/* Low Grade - always dominated */

/*
 * Mount attribute names
 */
#define	MAC_MOUNT_IP		"mac-ip"
#define	MAC_MOUNT_DEFAULT	"mac-default"

/*
 * XFS extended attribute names
 */
#define	SGI_BI_FILE	"SGI_BI_FILE"
#define	SGI_BI_IP	"SGI_BI_IP"
#define	SGI_BI_NOPLANG	"SGI_BI_NOPLANG"
#define	SGI_BI_PROCESS	"SGI_BI_PROCESS"
#define	SGI_BLS_FILE	"SGI_BLS_FILE"
#define	SGI_BLS_IP	"SGI_BLS_IP"
#define	SGI_BLS_NOPLANG	"SGI_BLS_NOPLANG"
#define	SGI_BLS_PROCESS	"SGI_BLS_PROCESS"
#define	SGI_MAC_FILE	"SGI_MAC_FILE"
#define	SGI_MAC_IP	"SGI_MAC_IP"
#define	SGI_MAC_NOPLANG	"SGI_MAC_NOPLANG"
#define	SGI_MAC_PROCESS	"SGI_MAC_PROCESS"
#define	SGI_MAC_POINTER	"SGI_MAC_POINTER"

/*
 * XFS extended attribute name lengths, not including trailing \0
 */
#define SGI_BI_FILE_SIZE	(sizeof (SGI_BI_FILE) - 1)
#define SGI_BI_IP_SIZE		(sizeof (SGI_BI_IP) - 1)
#define SGI_BI_NOPLANG_SIZE	(sizeof (SGI_BI_NOPLANG) - 1)
#define SGI_BI_PROCESS_SIZE	(sizeof (SGI_BI_PROCESS) - 1)
#define SGI_BLS_FILE_SIZE	(sizeof (SGI_BLS_FILE) - 1)
#define SGI_BLS_IP_SIZE		(sizeof (SGI_BLS_IP) - 1)
#define SGI_BLS_NOPLANG_SIZE	(sizeof (SGI_BLS_NOPLANG) - 1)
#define SGI_BLS_PROCESS_SIZE	(sizeof (SGI_BLS_PROCESS) - 1)
#define SGI_MAC_FILE_SIZE	(sizeof (SGI_MAC_FILE) - 1)
#define SGI_MAC_IP_SIZE		(sizeof (SGI_MAC_IP) - 1)
#define SGI_MAC_NOPLANG_SIZE	(sizeof (SGI_MAC_NOPLANG) - 1)
#define SGI_MAC_PROCESS_SIZE	(sizeof (SGI_MAC_PROCESS) - 1)
#define SGI_MAC_POINTER_SIZE	(sizeof (SGI_MAC_POINTER) - 1)

/*
 * XFS extended attribute name indexes.
 * Once a MAC attribute name has been looked up, the index can be used
 * thereafter.
 */
#define SGI_BI_FILE_INDEX	0
#define SGI_BI_IP_INDEX		1
#define SGI_BI_NOPLANG_INDEX	2
#define SGI_BI_PROCESS_INDEX	3
#define SGI_BLS_FILE_INDEX	4
#define SGI_BLS_IP_INDEX	5
#define SGI_BLS_NOPLANG_INDEX	6
#define SGI_BLS_PROCESS_INDEX	7
#define SGI_MAC_FILE_INDEX	8
#define SGI_MAC_IP_INDEX	9
#define SGI_MAC_NOPLANG_INDEX	10
#define SGI_MAC_PROCESS_INDEX	11
#define SGI_MAC_POINTER_INDEX	12

#define MAC_ATTR_LIST_MAX	12

/*
 * Layout of either a Bell & LaPadula Sensitivity label
 * or a Biba Integrity label.
 */
#define MAC_MAX_SETS	250

typedef struct	mac_b_label {
	unsigned char	b_type;		/* label type */
	unsigned char	b_hier;		/* Hierarchical part  */
	unsigned short	b_nonhier;	/* Non-Hierarchical part count */
	unsigned short	b_list[MAC_MAX_SETS];
} mac_b_label;

typedef mac_b_label msen_label;
typedef mac_b_label mint_label;

/*
 *
 * XXX:casey
 *	The composite MAC label may be headed the way of PlanG.
 *
 * Layout of a composite MAC label.
 *
 * ml_list contains the list of categories (MSEN) followed by the list of
 * divisions (MINT). This is actually a header for the data structure which
 * will have an ml_list with more than one element.
 *
 *	-------------------------------
 *	| ml_msen_type | ml_mint_type |
 *	-------------------------------
 *	| ml_level     | ml_grade     |
 *	-------------------------------
 *	| ml_catcount                 |
 *	-------------------------------
 *	| ml_divcount                 |
 *	-------------------------------
 *	| category 1                  |
 *	| . . .                       |
 *	| category N                  | (where N = ml_catcount)
 *	-------------------------------
 *	| division 1                  |
 *	| . . .                       |
 *	| division M                  | (where M = ml_divcount)
 *	-------------------------------
 */

typedef struct	mac_label {
	unsigned char	ml_msen_type;	/* MSEN label type */
	unsigned char	ml_mint_type;	/* MINT label type */
	unsigned char	ml_level;	/* Hierarchical level  */
	unsigned char	ml_grade;	/* Hierarchical grade  */
	unsigned short	ml_catcount;	/* Category count */
	unsigned short	ml_divcount;	/* Division count */
					/* Category set, then Division set */
	unsigned short	ml_list[MAC_MAX_SETS];
} mac_label;

/*
 * Number of bytes required for a "pseudo" label
 */
#define MAC_PL_SIZE	8

#ifdef _KERNEL
/*
 * Mode definitions for mac_access().
 * These need to be the same as VWRITE/IWRITE and VREAD/IREAD, respectivly.
 */
#define	MACEXEC		00100	/* VEXEC in vnode.h */
#define	MACWRITE	00200	/* VWRITE in vnode.h */
#define MACREAD		00400	/* VREAD in vnode.h */
#define	MACWRITEUP	01000	/* VSVTX in vnode.h */

/*
 * Moldy flags
 */
#define MAC_NOMOLD	0	/* Moldy not permitted */
#define MAC_MOLDOK	1	/* Moldy permitted */

/*
 * Data to add to the vfs entry.
 */
struct mac_vfs {
	struct mac_label *mv_default;   /* use if SGI_MAC_FILE is absent */
	struct mac_label *mv_ipmac;     /* use for network access */
};
typedef struct mac_vfs mac_vfs_t;

#endif /* _KERNEL */

/* function prototypes */

#ifdef _KERNEL
struct bhv_desc;
struct cred;
struct inode;
struct mac_vfs;
struct mounta;
struct snode;
struct rnode;
struct vattr;
struct vfs;
struct vnode;
struct pathname;
struct xattr_cache;
struct xfs_inode;
struct xfs_da_args;
struct attrlist_cursor_kern;

union rval;

extern int mac_copyin_label( mac_label *, mac_label ** );
extern mac_label *mac_add_label( mac_label * );
extern mac_label *mac_unmold( mac_label * );
extern int mac_inrange( mac_label *, mac_label *, mac_label * );
extern int mac_mint_equ( mac_label *, mac_label * );
extern int mac_cat_equ( mac_label *, mac_label * );

#else  /* _KERNEL */
extern int sgi_revoke( char * );
#endif /* _KERNEL */

extern int mac_is_moldy( mac_label * );
extern int msen_is_moldy( msen_label * );
extern mac_label *mac_dup( mac_label * );
extern mac_label *mac_demld( mac_label * );
extern mac_label *mac_set_moldy( mac_label * );
/* extern int mac_label_devs( char *, mac_label *, uid_t ); */

#ifdef _KERNEL
extern int mac_invalid( mac_label * );

/* Define macros choosing stub functions or real functions here */
extern int mac_enabled;

/*
 * We mark all of the MAC routines as infrequent in order to cause any
 * code necessary to call them to be compiled out of line.  This penalizes
 * sites that use MAC slightly and rewards those that don't.  Since the MAC
 * calls already involve an out-of-line function call this seems like a good
 * tradeoff.
 */

static __inline void mac_never(void) {}
#pragma mips_frequency_hint NEVER mac_never

extern int mac_get( char *, int, mac_label * );
#define _MAC_GET(a,b,c) \
	(mac_enabled? (mac_never(), mac_get(a,b,c)): ENOSYS)

extern int mac_set( char *, int, mac_label * );
#define _MAC_SET(a,b,c) \
	(mac_enabled? (mac_never(), mac_set(a,b,c)): ENOSYS)

#define _MAC_GETLABEL(a,b) \
	(mac_enabled? (mac_never(), mac_get(a,-1,b)): ENOSYS)

#define _MAC_SETLABEL(a,b) \
	(mac_enabled? (mac_never(), mac_set(a,-1,b)): ENOSYS)

extern int mac_getplabel( mac_label * );
#define _MAC_GETPLABEL(a) \
	(mac_enabled? (mac_never(), mac_getplabel(a)): ENOSYS)

extern int mac_setplabel( mac_label *, int );
#define _MAC_SETPLABEL(a,b) \
	(mac_enabled? (mac_never(), mac_setplabel(a,b)): ENOSYS)

extern int mac_revoke( char * );
#define _MAC_REVOKE(a)	 \
	(mac_enabled? (mac_never(), mac_revoke(a)): ENOSYS)

extern int mac_dom( mac_label *, mac_label * );
#define _MAC_DOM(a,b) \
	(mac_enabled? (mac_never(), mac_dom(a,b)): 1)

extern int mac_equ( mac_label *, mac_label * );
#define _MAC_EQU(a,b) \
	(mac_enabled? (mac_never(), mac_equ(a,b)): 1)

extern int mac_access( mac_label *, struct cred *, mode_t );
#define _MAC_ACCESS(l,c,i) \
	(mac_enabled? (mac_never(), mac_access(l,c,i)): 0)

extern int mac_vaccess( struct vnode *, struct cred *, mode_t );
#define _MAC_VACCESS(v,c,m) \
	(mac_enabled? (mac_never(), mac_vaccess(v,c,m)): 0)

extern int mac_vsetlabel( struct vnode *, mac_label * );
#define _MAC_VSETLABEL(v,m) \
	(mac_enabled? (mac_never(), mac_vsetlabel(v,m)): 0)

extern ssize_t mac_size( mac_label * );
#define _MAC_SIZE(l) \
	(mac_enabled? (mac_never(), mac_size(l)): 0)

#define _MSEN_SIZE(l) \
	(mac_enabled? (mac_never(), msen_size(l)): 0)

#define _MINT_SIZE(l) \
	(mac_enabled? (mac_never(), mint_size(l)): 0)

#define _MAC_IS_MOLDY(l) \
	(mac_enabled? (mac_never(), mac_is_moldy(l)): 0)

#define _MAC_DEMLD(l) \
	(mac_enabled? (mac_never(), mac_demld(l)): 0)

extern int mac_moldy_path( struct vnode *, char *, struct pathname *,
			  struct cred * );
#define _MAC_MOLDY_PATH(v,s,p,c) \
	(mac_enabled? (mac_never(), mac_moldy_path(v,s,p,c)): 0)

extern int mac_initial_path( char * );
#define _MAC_INITIAL_PATH(p) \
	(mac_enabled? (mac_never(), mac_initial_path(p)): 0)

extern mac_label *mac_vtolp( struct vnode * );
#define _MAC_VTOLP(v) \
	(mac_enabled? (mac_never(), mac_vtolp(v)): NULL)

extern mac_label * mac_efs_getlabel( struct inode * );
#define _MAC_EFS_GETLABEL(i) \
	(mac_enabled? (mac_never(), mac_efs_getlabel(i)): NULL)

extern int mac_efs_setlabel( mac_label *, struct inode *, int );
#define _MAC_EFS_SETLABEL(v,i,m) \
	(mac_enabled? (mac_never(), mac_efs_setlabel(v,i,m)): 0)

extern int mac_efs_iaccess( struct inode *, struct cred *, mode_t );
#define _MAC_EFS_IACCESS(i,c,m) \
	(mac_enabled? (mac_never(), mac_efs_iaccess(i,c,m)): 0)

extern mac_label * mac_xfs_getlabel(struct xfs_inode *);
#define _MAC_XFS_GETLABEL(i) \
	(mac_enabled? (mac_never(), mac_xfs_getlabel(i)): NULL)

extern int mac_xfs_setlabel(mac_label *, struct xfs_inode *, int);
#define _MAC_XFS_SETLABEL(v,i,m) \
	(mac_enabled? (mac_never(), mac_xfs_setlabel(v,i,m)): 0)

extern int mac_xfs_iaccess(struct xfs_inode *, mode_t, struct cred *);
#define _MAC_XFS_IACCESS(i,m,c) \
	(mac_enabled? (mac_never(), mac_xfs_iaccess(i,m,c)): 0)

#define _MAC_XFS_ATTR_DESTROY(x) \
	/* Shared pointers - no distruction required */

extern int mac_xfs_attr_get(struct xfs_inode *, char *, char *, int *);
#define _MAC_XFS_ATTR_GET(i,n,v,l) \
	(mac_enabled? (mac_never(), mac_xfs_attr_get(i,n,v,l)): -1)

extern int mac_xfs_attr_set(struct xfs_inode *, char *, char *, int);
#define _MAC_XFS_ATTR_SET(i,n,v,l) \
	(mac_enabled? (mac_never(), mac_xfs_attr_set(i,n,v,l)): -1)

extern int mac_nfs_default(struct xattr_cache *, int, struct mac_vfs *);
#define	_MAC_NFS_DEFAULT(e,t,m) \
	(mac_enabled? (mac_never(), mac_nfs_default(e,t,m)): 1)

extern int mac_nfs_get(struct bhv_desc *, struct xattr_cache *, int, int,
		       struct cred * );
#define _MAC_NFS_GET(b,e,r,f,c) \
	(mac_enabled? (mac_never(), mac_nfs_get(b,e,r,f,c)): 1)

extern int mac_nfs_iaccess(struct bhv_desc *, mode_t, struct cred *);
#define _MAC_NFS_IACCESS(i,m,c) \
	(mac_enabled? (mac_never(), mac_nfs_iaccess(i,m,c)): 0)

extern int mac_hwg_iaccess(dev_t, mode_t, struct cred *);
#define _MAC_HWG_IACCESS(i,m,c) \
	(mac_enabled? (mac_never(), mac_hwg_iaccess(i,m,c)): 0)

extern int mac_hwg_get(dev_t, char *, char *, int *, int);
#define _MAC_HWG_GET(a,b,c,d,e) \
	(mac_enabled? (mac_never(), mac_hwg_get(a,b,c,d,e)): -1)

extern int mac_hwg_match(const char *, int);
#define _MAC_HWG_SET(n,f) \
	(mac_enabled? (mac_never(), mac_hwg_match(n,f)): 0)

#define _MAC_HWG_REMOVE(n,f) \
	(mac_enabled? (mac_never(), mac_hwg_match(n,f)): 0)

extern int mac_efs_attr_get(struct bhv_desc *, char *, char *, int *, int,
			    struct cred *);
#define _MAC_EFS_ATTR_GET(v,n,a,b,f,c) \
	(mac_enabled? (mac_never(), mac_efs_attr_get(v,n,a,b,f,c)): -1)

extern int mac_efs_attr_set(struct bhv_desc *, char *, char *, int, int,
			    struct cred *);
#define _MAC_EFS_ATTR_SET(v,n,a,b,f,c) \
	(mac_enabled? (mac_never(), mac_efs_attr_set(v,n,a,b,f,c)): -1)

extern int mac_proc_attr_get(struct bhv_desc *, char *, char *, int *, int,
			     struct cred *);
#define _MAC_PROC_ATTR_GET(v,n,a,b,f,c) \
	(mac_enabled? (mac_never(), mac_proc_attr_get(v,n,a,b,f,c)): -1)

extern int mac_fifo_attr_get(struct bhv_desc *, char *, char *, int *, int,
			     struct cred *);
#define _MAC_FIFO_ATTR_GET(v,n,a,b,f,c) \
	(mac_enabled? (mac_never(), mac_fifo_attr_get(v,n,a,b,f,c)): -1)

extern int mac_pipe_attr_get(struct bhv_desc *, char *, char *, int *, int,
			     struct cred *);
#define _MAC_PIPE_ATTR_GET(v,n,a,b,f,c) \
	(mac_enabled? (mac_never(), mac_pipe_attr_get(v,n,a,b,f,c)): -1)

extern int mac_pipe_attr_set(struct bhv_desc *, char *, char *, int, int,
			     struct cred *);
#define _MAC_PIPE_ATTR_SET(v,n,a,b,f,c) \
	(mac_enabled? (mac_never(), mac_pipe_attr_set(v,n,a,b,f,c)): -1)

extern int mac_fdfs_attr_get( struct bhv_desc *, char *, char *, int *, int,
			     struct cred *);
#define _MAC_FDFS_ATTR_GET(v,n,a,b,f,c) \
	(mac_enabled? (mac_never(), mac_fdfs_attr_get(v,n,a,b,f,c)): -1)

extern int mac_spec_attr_get( struct bhv_desc *, char *, char *, int *, int,
			     struct cred *);
#define _MAC_SPEC_ATTR_GET(v,n,a,b,f,c) \
	(mac_enabled? (mac_never(), mac_spec_attr_get(v,n,a,b,f,c)): -1)

extern int mac_autofs_attr_get( struct bhv_desc *, char *, char *, int *, int,
			       struct cred *);
#define _MAC_AUTOFS_ATTR_GET(v,n,a,b,f,c) \
	(mac_enabled? (mac_never(), mac_autofs_attr_get(v,n,a,b,f,c)): -1)

extern int mac_autofs_attr_set( struct bhv_desc *, char *, char *, int , int,
			       struct cred *);
#define _MAC_AUTOFS_ATTR_SET(v,n,a,b,f,c) \
	(mac_enabled? (mac_never(), mac_autofs_attr_set(v,n,a,b,f,c)): -1)

extern int mac_autofs_attr_list(struct bhv_desc *, char *, int, int ,
				struct attrlist_cursor_kern *, struct cred *);
#define _MAC_AUTOFS_ATTR_LIST(v,b,s,f,x,c) \
	(mac_enabled? (mac_never(), mac_autofs_attr_list(v,b,s,f,x,c)): -1)

extern void mac_init_cred(void);
#define _MAC_INIT_CRED() \
	(mac_enabled? (mac_never(), mac_init_cred()): (void)0)

extern void mac_confignote(void);
#define _MAC_CONFIGNOTE() \
	(mac_enabled? (mac_never(), mac_confignote()): (void)0)

extern void mac_mountroot( struct vfs * );
#define _MAC_MOUNTROOT(v) \
	(mac_enabled? (mac_never(), mac_mountroot(v)): (void)0)

extern void mac_mount( struct vfs *, char * );
#define _MAC_MOUNT(v,a) \
	(mac_enabled? (mac_never(), mac_mount(v,a)): (void)0)

extern void mac_sem_init( int );
#define _MAC_SEM_INIT(i) \
	(mac_enabled? (mac_never(), mac_sem_init(i)): (void)0)

extern int mac_sem_access( int, struct cred * );
#define _MAC_SEM_ACCESS(i,c) \
	(mac_enabled? (mac_never(), mac_sem_access(i,c)): 0)

extern void mac_sem_setlabel( int, mac_label * );
#define _MAC_SEM_SETLABEL(i,l) \
	(mac_enabled? (mac_never(), mac_sem_setlabel(i,l)): (void)0)

extern void mac_shm_init( int );
#define _MAC_SHM_INIT(i) \
	(mac_enabled? (mac_never(), mac_shm_init(i)): (void)0)

extern int mac_shm_access( int, struct cred * );
#define _MAC_SHM_ACCESS(i,c) \
	(mac_enabled? (mac_never(), mac_shm_access(i,c)): 0)

extern void mac_msg_init( int );
#define _MAC_MSG_INIT(i) \
	(mac_enabled? (mac_never(), mac_msg_init(i)): (void)0)

extern int mac_msg_access( int, struct cred * );
#define _MAC_MSG_ACCESS(i,c) \
	(mac_enabled? (mac_never(), mac_msg_access(i,c)): 0)

extern void mac_msg_setlabel( int, mac_label * );
#define _MAC_MSG_SETLABEL(i,l) \
	(mac_enabled? (mac_never(), mac_msg_setlabel(i,l)): (void)0)

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* __MAC_LABEL_ */
