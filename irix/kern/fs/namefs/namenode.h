/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _FS_NAMEFS_NAMENODE_H	/* wrapper symbol for kernel use */
#define _FS_NAMEFS_NAMENODE_H	/* subject to change without notice */

/*#ident	"@(#)uts-3b2:fs/namefs/namenode.h	1.4"*/
#ident	"$Revision: 1.9 $"

/*
 * This structure is used to pass a file descriptor from user
 * level to the kernel. It is first used by fattach() and then
 * by NAMEFS.
 */
struct namefd {
	int fd;
};

#ifdef _KERNEL

#include <sys/types.h>
#include <sys/sema.h>
#include <sys/vnode.h>

/*
 * Each NAMEFS object is identified by a struct namenode/vnode pair.
 *
 * MP locking protocols:
 *	nm_vattr			namelock/nameunlock
 *	nm_filevp,nm_filep,nm_mountpt	read-only
 *	nm_nextp, nm_backp		nameallocmon
 */
struct namenode {
	bhv_desc_t	nm_bhv_desc;	/* namefs behavior descriptor */
	bhv_desc_t	nm_vfsbhv;	/* vfs namefs behavior descriptor */
	mutex_t		nm_lock;	/* mutual exclusion for get/setattr */
	struct vattr    nm_vattr;	/* attributes of mounted file desc.*/
	struct vnode	*nm_filevp;	/* file desc. prior to mounting */
	struct vfile	*nm_filep;	/* file pointer of nm_filevp */
	struct vnode	*nm_mountpt;	/* mount point prior to mounting */
	struct namenode *nm_nextp;	/* next link in the linked list */
	struct namenode *nm_backp;	/* back link in linked list */
};


/*
 * Macros to convert a vnode to a namenode, and vice versa.
 */
#define	BHVTONM(bdp) ((struct namenode *)(BHV_PDATA(bdp)))
#define NMTOBHV(nm) (&((nm)->nm_bhv_desc))
#define NMTOV(nm) ((struct vnode *)BHV_VOBJ(NMTOBHV(nm)))

/*
 * macros to convert vfs namefs behavior descriptor to namenode 
 */
#define VFS_BHVTONM(bdp)	BHVTONM(bdp)


extern struct namenode	*namefind(struct vnode *, struct vnode *);
extern void		nameinsert(struct namenode *);
extern void		nameremove(struct namenode *);
extern void		namefree(struct namenode *);
extern void		nmclearid(struct namenode *);
extern u_short		nmgetid(void);
extern int		nm_unmountall(struct vnode *, struct cred *);

extern mutex_t	nameallocmon;
extern struct vnodeops	nm_vnodeops;
#endif	/* _KERNEL */

#endif	/* _FS_NAMEFS_NAMENODE_H */
