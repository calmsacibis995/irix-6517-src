#ifndef __SYS_NFS_CLNT_H__
#define __SYS_NFS_CLNT_H__

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*      @(#)nfs_clnt.h	1.2 88/07/15 NFSSRC4.0 from 2.19 88/02/08 SMI      */
/*	Copyright (C) 1988, Sun Microsystems Inc.	*/

#ident "$Revision: 1.60 $"

#include <netinet/in.h>
#include <sys/sema.h>
#include <sys/fstyp.h> /* for FSTYPSZ */
#include <sys/mkdev.h> /* for MAXMIN */

/*
 * NFS arguments to the mount system call.
 */
struct nfs_args {
	struct sockaddr_in	*addr;		/* file server address */
	fhandle_t		*fh;		/* File handle to be mounted */
	int			flags;		/* flags */
	u_int			wsize;		/* write size in bytes */
	u_int			rsize;		/* read size in bytes */
	u_int			timeo;		/* initial timeout in .1 secs */
	u_int			retrans;	/* times to retry send */
	char			*hostname;	/* server's name */
	u_int			acregmin;	/* attr cache file min secs */
	u_int			acregmax;	/* attr cache file max secs */
	u_int			acdirmin;	/* attr cache dir min secs */
	u_int			acdirmax;	/* attr cache dir max secs */
	u_int			symttl;		/* symlink cache time-to-live */
	char			base[FSTYPSZ];	/* base type for statvfs */
	u_int			namemax;	/* name length for statvfs */
	u_int 			fh_len;		/* length for a v3 filehandle */
	u_int			bdsauto;	/* automatic direct I/O size */
	u_int			bdswindow;	/* TCP window size.  XXX */
	u_int			bdsbuflen;	/* server buffer size */
	pid_t			pid;		/* pid of user level server */
	u_int			maxthreads;	/* max async threads this mnt */
};

/*
 * NFS mount option flags
 */
#define	NFSMNT_SOFT	0x001		/* soft mount (hard is default) */
#define	NFSMNT_WSIZE	0x002		/* set write size */
#define	NFSMNT_RSIZE	0x004		/* set read size */
#define	NFSMNT_TIMEO	0x008		/* set initial timeout */
#define	NFSMNT_RETRANS	0x010		/* set number of request retrys */
#define	NFSMNT_HOSTNAME	0x020		/* set hostname for error printf */
#define	NFSMNT_NOINT	0x040		/* disallow interrupts on hard mount */
#define	NFSMNT_NOAC	0x080		/* don't cache attributes */
#define	NFSMNT_ACREGMIN	0x0100		/* set min secs for file attr cache */
#define	NFSMNT_ACREGMAX	0x0200		/* set max secs for file attr cache */
#define	NFSMNT_ACDIRMIN	0x0400		/* set min secs for dir attr cache */
#define	NFSMNT_ACDIRMAX	0x0800		/* set max secs for dir attr cache */
#define	NFSMNT_PRIVATE	0x1000		/* mount private, single-client tree */
#define	NFSMNT_SYMTTL	0x2000		/* set symlink cache time-to-live */
#define	NFSMNT_LOOPBACK	0x4000		/* local mount via loopback interface */
#define	NFSMNT_BASETYPE	0x8000		/* base type for statvfs */
#define	NFSMNT_NAMEMAX	0x10000		/* namemax for statvfs */
#define	NFSMNT_SHORTUID	0x20000		/* don't allow writes by uid's > 0xffff */
#define NFSMNT_ASYNCNLM	0x40000		/* use async NLM calls */
#define NFSMNT_BDS	0x80000		/* Support BDS direct I/O */
#define NFSMNT_BDSAUTO	0x100000	/* Support BDS direct I/O automatically */
#define NFSMNT_BDSWINDOW 0x200000	/* TCP window size for BDS */
#define NFSMNT_TCP	0x400000 	/* use tcp if possible */
#define NFSMNT_BDSBUFLEN 0x800000	/* # of bytes BDS server should buffer */
#define	NFSMNT_SILENT	0x1000000	/* do not print log messages */
#define NFSMNT_PID	0x2000000 	/* process id of server */
#define NFSMNT_THREADS	0x4000000 	/* max async threads */

/*
 * File identifier struct for loopback (local, user-level) NFS servers.
 */
struct loopfid {
	u_short	lfid_len;	/* bytes in fid, excluding lfid_len */
	u_short	lfid_pad;	/* explicit struct padding */
	u_long	lfid_fno;	/* file number, from nodeid attribute */
};

#ifdef _KERNEL
struct irix5_nfs_args {
	app32_ptr_t		addr;		/* file server address */
	app32_ptr_t		fh;		/* File handle to be mounted */
	int			flags;		/* flags */
	u_int			wsize;		/* write size in bytes */
	u_int			rsize;		/* read size in bytes */
	u_int			timeo;		/* initial timeout in .1 secs */
	u_int			retrans;	/* times to retry send */
	app32_ptr_t		hostname;	/* server's name */
	u_int			acregmin;	/* attr cache file min secs */
	u_int			acregmax;	/* attr cache file max secs */
	u_int			acdirmin;	/* attr cache dir min secs */
	u_int			acdirmax;	/* attr cache dir max secs */
	u_int			symttl;		/* symlink cache time-to-live */
	char			base[FSTYPSZ];	/* base type for statvfs */
	u_int			namemax;	/* name length for statvfs */
	u_int 			fh_len;		/* length for a v3 filehandle */
	u_int			bdsauto;	/* automatic direct I/O size */
	u_int			bdswindow;	/* TCP window size.  XXX */
	u_int			bdsbuflen;	/* server buffer size */
	pid_t			pid;		/* pid of user level server */
	u_int			maxthreads;	/* max async threads this mnt */
};
/*
 * vfs behavior pointer to mount info
 */
#define	vfs_bhvtomi(bdp)	((struct mntinfo *)BHV_PDATA(bdp))

/*
 * vnode behavior pointer to mount info
 */
#define rtomi(rp)	((rp)->r_mi)
#define vn_bhvtomi(bdp) (rtomi((struct rnode *)BHV_PDATA(bdp)))

/*
 * NFS rnode to buffer cache block size
 */
#define	rtoblksz(rp)	(rtomi(rp)->mi_bsize)

#endif /* _KERNEL */

#define	HOSTNAMESZ	32	/* size of name for error messages */
#define	ACREGMIN	3	/* min secs to hold cached file attr */
#define	ACREGMAX	60	/* max secs to hold cached file attr */
#define	ACDIRMIN	30	/* min secs to hold cached dir attr */
#define	ACDIRMAX	60	/* max secs to hold cached dir attr */
#define	SYMTTL		3600	/* max secs to hold cached symlink */

#if defined _KERNEL || defined _KMEMUSER

#define BDS_SOCKETS	3	/* # of vsockets to cache */

struct nfs_async_reqs;

/*
 * NFS private data per mounted file system
 */
struct mntinfo {
	bhv_desc_t	mi_bhv;		/* vfs nfs behavior */
	struct sockaddr_in mi_addr;	/* server's address */
	struct sockaddr_in mi_lmaddr;	/* server's lock manager address */
	struct vnode	*mi_rootvp;	/* root vnode */
	struct rnode	*mi_root;	/* root rnode */
	u_int		mi_flags;	/* use atomic ops to set/clear mi_flags */
					/* [V] == contents are volatile */
#define MIF_HARD	0x00001		/* hard or soft mount */
#define MIF_PRINTED	0x00002		/* [V] not responding message printed */
#define MIF_INT		0x00004		/* interrupts allowed on hard mount */
#define MIF_DOWN	0x00008		/* [V] server is down */
#define MIF_NOAC	0x00010		/* don't cache attributes */
#define MIF_PRIVATE	0x00020		/* cache assuming private export */
#define MIF_SHORTUID	0x00040		/* don't allow writes by uids>0xffff */
#define MIF_LOOPBACK	0x00080		/* loopback mount; ok to export */
#define MIF_READDIR	0x00100		/* [V] use readdir instead of readdirplus */
#define MIF_LINK	0x00200		/* [V] server supports link */
#define MIF_SYMLINK	0x00400		/* [V] server supports symlink */
#define MIF_DEAD_irix6.6	0x00800		/* [X] locks have been reclaimed */
#define MIF_BDS		0x01000		/* BDS server is present */
#define MIF_TCP		0x02000		/* use tcp if possible */
#define MIF_STALE	0x04000		/* [V] user level server gone */
#define MIF_SILENT	0x08000		/* print log messages? */
#define MIF_LOCALLOOP	0x10000		/* loopback mount of local FS */
#define MIF_NO_SERVER	0x20000		/* [V] server probably down - no new async */
	void            *mi_con;        /* tcp connection info */
	u_int		mi_nlm;		/* lock manager flags */
	u_int		mi_refct;	/* active vnodes for this vfs */
	u_int		mi_tsize;	/* transfer size (bytes) */
	u_int		mi_stsize;	/* server's max transfer size (bytes) */
	u_int		mi_bsize;	/* buffer cache block size */
	u_int		mi_mntno;	/* kludge to set client rdev for stat*/
	u_long		mi_rootfsid;	/* remote root's attributed fsid */
	struct fsidmap	*mi_fsidmaps;	/* nohide exports fsid mappings */
	u_int		mi_timeo;	/* inital timeout in 10th sec */
	u_int		mi_retrans;	/* times to retry request */
	char		mi_hostname[HOSTNAMESZ];	/* server's hostname */
	int		mi_acregmin;	/* min secs to hold cached file attr */
	int		mi_acregmax;	/* max secs to hold cached file attr */
	int		mi_acdirmin;	/* min secs to hold cached dir attr */
	int		mi_acdirmax;	/* max secs to hold cached dir attr */
	u_int		mi_symttl;	/* cached symlink time-to-live */
	u_long		mi_rdestroys;	/* count of rnodes destroyed */
	struct rnode	*mi_rnodes;	/* list of active and cached rnodes */
	mutex_t		mi_lock;	/* mutual exclusion */
	char		mi_basetype[FSTYPSZ];	/* base type for statvfs */
	u_int		mi_namemax;	/* namemax for statvfs */
	struct mac_label *mi_ipmac;	/* MAC label for communications */
	u_long		mi_prog;	/* RPC program number */
	u_long		mi_vers;	/* RPC program version number */
	u_long		mi_nlmvers;	/* RPC program version number for NLM */
	char		**mi_rfsnames;	/* mapping to proc names */
	/*
	 * async I/O management
	 */
	struct nfs_async_reqs *mi_async_reqs; /* head of async request list */
	struct nfs_async_reqs *mi_async_tail; /* tail of async request list */
	kcondvar_t	mi_async_reqs_cv;
	u_short		mi_threads;	/* number of active async threads */
	u_short		mi_max_threads; /* max number of async threads */
	kcondvar_t	mi_async_cv;
	u_int		mi_async_count; /* number of entries on async list */
	kmutex_t	mi_async_lock;	/* lock to protect async list */
	u_int		mi_rdahead;	/* read_ahead blocks */
	struct mntinfo	*mi_next;	/* next in mntinfo list */
	struct mntinfo	*mi_prev;	/* previous in mntinfo list */
	u_int		mi_bdsauto;	/* do direct I/O for blocks this big */
	u_int		mi_bdswindow;	/* XXX TCP window size for BDS */
	u_int		mi_bdsbuflen;	/* requested BDS buffer length */
	pid_t		mi_pid;		/* process id of server */
	struct vsocket	*(mi_bds_vslist[BDS_SOCKETS]); /* cached vsockets */
	int		mi_bds_nextvs;	/* next index into vslist */
	lock_t		mi_bds_vslock;	/* lock protecting nextvs and vslist */
	long		mi_bds_blksize;	/* for stat's st_blksize */
	unsigned	mi_bds_maxiosz;	/* remote server's d_maxiosz */
	sv_t		mi_nohang_wait; /* place where nohang procs wait */
};
typedef struct mntinfo	mntinfo_t;

extern void nfscon_free(struct mntinfo *mi);
extern int nfscon_get(struct mntinfo *mi);

/*
 * flag values for mi_nlm
 * these are used for lock manager communication
 */
#define MI_NLM_LOCK			0x80000000		/* mi_nlm lock bit for */
#define MI_NLM_SYNC			0x00000001		/* do synchronous NLM calls */
#define MI_NLM_RECLAIM		0x00000002		/* locks reclaimed */
#define MI_NLM_RECLAIMING	0x00000004		/* lock reclaiming in progress */
#define MI_NLMASYNC_TEST	0x00000008		/* use async NLM test */
#define MI_NLM4_SYNC		0x00000010		/* do synchronous NLM4 calls */
#define MI_NLM4ASYNC_TEST	0x00000020		/* use async NLM4 test */

/*
 * by default, do synchronous NLM calls
 */
#define MI_NLM_DEFAULT	MI_NLM_SYNC
#define MI_NLM4_DEFAULT	MI_NLM4_SYNC

struct fsidmap {
	u_long		fsid_remote;	/* remote attributed fsid */
	long		fsid_local;	/* local, client-unique fsid */
	struct fsidmap	*fsid_next;	/* other mappings */
};

#endif /* _KERNEL || _KMEMUSER */

#ifdef _KERNEL

/*
 * enum to specifiy cache flushing action when file data is stale
 */
enum staleflush {NOFLUSH, SFLUSH};

/*
 * Locking macros for SP and MP mntinfo locking.
 */
#define milock(mi)	mutex_lock(&(mi)->mi_lock, PINOD)
#define miunlock(mi)	mutex_unlock(&(mi)->mi_lock)
#define milock_mp(mi)	mp_mutex_lock(&(mi)->mi_lock, PINOD)
#define miunlock_mp(mi)	mp_mutex_unlock(&(mi)->mi_lock)

/*
 * NFS minor device number allocation bitmap.
 *	MAXMIN is defined in mkdev.h
 */
#define MINORMAP_SIZE	(512/NBBY)
#define MINORMAP_GROW	(256/NBBY)
#define MINORMAP_MAX	(MAXMIN/NBBY)

struct minormap {
	u_char	*vec;			/* ptr to bitmap */
	u_long	size;			/* size of bitmap in bytes */
	lock_t	lock;			/* structure lock */
};

extern struct minormap	nfs_minormap;
extern dev_t		nfs_major;
#define is_nfs_dev(x)	(getemajor(x) == nfs_major)
#define NFS_SWAPFILE	"/swap/_swap"

extern int		vfs_getnum(struct minormap *);
extern void		vfs_putnum(struct minormap *, int);

struct cred;
struct nfsfattr;
struct vattr;
struct vnode;

/*
 * Attribute cache operations exported from nfs_vnodeops.c to other NFS
 * client-side modules.
 */
extern mode_t	setdirmode(bhv_desc_t *, mode_t);
extern gid_t	setdirgid(bhv_desc_t *);
extern dev_t	nfs_cmpdev(dev_t dev);
extern dev_t	nfs_expdev(dev_t dev);
extern int	nfs_attrcache(struct bhv_desc *, struct nfsfattr *,
			enum staleflush, int *);
extern void	setdiropargs(struct nfsdiropargs *, char *, bhv_desc_t *);
extern void	nfs_rwunlock(bhv_desc_t *, enum vrwlock);
extern void	nfs_rwlock(bhv_desc_t *, enum vrwlock);
extern int	nfs_rwlock_nohang(bhv_desc_t *, enum vrwlock);
extern char	*inet_ntoa(struct in_addr);
extern char	*newname(void);
extern int	nfs_getattr(bhv_desc_t *, struct vattr *, int, struct cred *);
extern int	makenfsroot(struct vfs *, struct sockaddr_in *, fhandle_t *,
			char *, struct rnode **, int);
extern int	nfs_lockctl(bhv_desc_t *, struct flock *,
			int, struct cred *);
extern int	rfscall(struct mntinfo *, int, xdrproc_t, caddr_t,
			xdrproc_t, caddr_t, struct cred *, int);
extern int	rfscall_tcp(struct mntinfo *, int, xdrproc_t, caddr_t,
			xdrproc_t, caddr_t, struct cred *, int);
extern void	nattr_to_iattr(struct nfsfattr *, bhv_desc_t *);
extern void	vattr_to_sattr(struct vattr *, struct nfssattr *);
extern void	nattr_to_vattr(struct rnode *, struct nfsfattr *,
			struct vattr *);
extern int	do_bio(struct buf *, struct rnode *, struct cred *, int);
extern int	nfs_rwlock_nowait(struct rnode *);
extern int	nfs_rmount(struct vfs *, enum whymountroot);
extern int	nfs_sync(bhv_desc_t *, int, struct cred *);

extern struct vnodeops	nfs_vnodeops;
extern struct vfsops	nfs_vfsops;
#endif	/* _KERNEL */
#endif /* !__SYS_NFS_CLNT_H__ */
