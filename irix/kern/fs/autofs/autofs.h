/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

/*
 * Autofs mount info - one per mount
 */

#ifndef	_SYS_FS_AUTOFS_H
#define	_SYS_FS_AUTOFS_H

#ifdef	__cplusplus
extern "C" {
#endif

#undef	MAXNAMLEN
#define	MAXNAMLEN	255

#include <netinet/in.h>
#include <sys/time.h>

/* Private well-known port */
#define AUTOFS_PORT 2048

struct autofs_args {
	char			*path;		/* autofs mountpoint */
	char			*opts;		/* default mount options */
	char			*map;		/* name of map */
	time_t			mount_to;	/* seconds fs remains */
						/* mounted after last ref */
	time_t 			rpc_to;		/* timeout for rpc calls */
	int			direct;		/* 1 = direct mount */
};

#ifdef	_KERNEL

struct irix5_autofs_args {
	app32_ptr_t		path;		/* autofs mountpoint */
	app32_ptr_t		opts;		/* default mount options */
	app32_ptr_t		map;		/* name of map */
	time_t			mount_to;	/* seconds fs remains */
						/* mounted after last ref */
	time_t 			rpc_to;		/* timeout for rpc calls */
	int			direct;		/* 1 = direct mount */
};

/*
 * The autonode is the "inode" for autofs.  It contains
 * all the information necessary to handle automatic mounting.
 */
typedef struct autonode {
	char		an_name[MAXNAMLEN + 1];
	u_short		an_mode;	/* file mode bits */
	uid_t		an_uid;
	gid_t		an_gid;
	ino_t		an_nodeid;	/* unique id */
	off_t		an_offset;	/* offset within parent dir */
	u_short		an_waiters;
	u_short		an_count;	/* ref count */
	u_int		an_mntflags;	/* mount flags */
	u_int		an_size;	/* size of directory */
	bhv_desc_t	an_bhv_desc;	/* autofs behavior descriptor */
	struct vnode an_vnode;		/* place hldr vnode for file */
	time_t		an_ref_time;    /* time last referred */
	int		an_error;	/* mount error */
	struct autonode *an_parent;	/* parent dir for .. lookup */
	struct autonode *an_next;	/* next sibling */
	struct autonode *an_dirents;	/* autonode list under this */
	struct autoinfo	*an_ainfo;	/* points to autoinfo */
	krwlock_t	an_rwlock;	/* serialize access for lookup */
					/* and enter */
	sema_t		an_mnt_wait;	/* sync sema for mount wait */
	kcondvar_t	an_cv_mount;	/* wakeup address */
	kcondvar_t	an_cv_umount;	/* wakeup address */
	kmutex_t	an_lock;	/* protection */
} autonode_t;


struct autoinfo {
	struct vnode		*ai_rootvp;	/* root vnode */
	struct autonode		*ai_root;	/* root autonode */	
	int		 	ai_refcnt;	/* reference count */
	char			*ai_path;	/* autofs mountpoint */
	int		 	ai_pathlen;	/* autofs mountpoint len */
	char			*ai_opts;	/* default mount options */
	int		 	ai_optslen;	/* default mount options len */
	char			*ai_map;	/* name of map */
	int		 	ai_maplen;	/* name of map len */
	time_t		 	ai_mount_to;
	time_t		 	ai_rpc_to;
	int		 	ai_direct;	/* 1 = direct mount */
	bhv_desc_t		ai_bhv;		/* autofs behavior descriptor */
};


/*
 * Mount flags
 */

#define	MF_MNTPNT		0x001		/* A mountpoint */
#define	MF_INPROG		0x002		/* mount in progress */
#define	MF_WAITING_MOUNT	0x004		/* thread waiting */
#define	MF_WAITING_UMOUNT	0x008
#define	MF_MOUNTED		0x010		/* mount taken place */
#define	MF_UNMOUNTING		0x020		/* unmount in prog */
#define	MF_DONTMOUNT		0x040		/* unmount failed, so don't */
						/* try another mount */
#define	MF_CHECKED		0x080		/* checked by unmount thread */
/*
 * Convert between vfs/autoinfo & vnode/autonode
 */
#define antovn(ap)      (&((ap)->an_vnode))
#define	bhvtoan(bdp)	((autonode_t *) (BHV_PDATA(bdp)))
#define	antobhv(ap)	(&((ap)->an_bhv_desc))
#define antoai(ap)	((ap)->an_ainfo)
#define vfs_bhvtoai(bdp)	((struct autoinfo *)BHV_PDATA(bdp))

extern kmutex_t autonode_list_lock;
extern struct autonode *autonode_list;
extern kmutex_t autonode_count_lock;
extern int anode_cnt, freeautonode_count;

extern int	do_mount(bhv_desc_t *, char *, struct cred *);
extern int	autodir_lookup(bhv_desc_t *, char *, bhv_desc_t **, struct cred *);
extern int	auto_direnter(autonode_t *, autonode_t *);
extern void	do_unmount(void);
extern autonode_t	*makeautonode(vtype_t, vfs_t *, struct autoinfo *, struct cred *);
extern void	freeautonode(autonode_t *anp);
extern void	auto_dprint(int var, int level, char *str, ...);

extern vfsops_t autofs_vfsops;
#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FS_AUTOFS_H */
