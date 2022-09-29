#ifndef _FS_XFS_MOUNT_H
#define	_FS_XFS_MOUNT_H

#ident	"$Revision: 1.102 $"

struct cred;
struct vfs;
struct vnode;
struct xfs_ihash;
struct xfs_chash;
struct xfs_inode;
struct xfs_perag;
struct xfs_qm;
struct xfs_quotainfo;

#if defined(INTERRUPT_LATENCY_TESTING)
#define	SPLDECL(s)	       
#define	AIL_LOCK_T		mutex_t
#define	AIL_LOCKINIT(x,y)	mutex_init(x,MUTEX_DEFAULT, y)
#define	AIL_LOCK_DESTROY(x)	mutex_destroy(x)
#define	AIL_LOCK(mp,s)		mutex_lock(&(mp)->m_ail_lock, PZERO)
#define	AIL_UNLOCK(mp,s)	mutex_unlock(&(mp)->m_ail_lock)
#else	/* !INTERRUPT_LATENCY_TESTING */
#define	SPLDECL(s)		int s
#define	AIL_LOCK_T		lock_t
#define	AIL_LOCKINIT(x,y)	spinlock_init(x,y)
#define	AIL_LOCK_DESTROY(x)	spinlock_destroy(x)
#define	AIL_LOCK(mp,s)		s=mutex_spinlock(&(mp)->m_ail_lock)
#define	AIL_UNLOCK(mp,s)	mutex_spinunlock(&(mp)->m_ail_lock, s)
#endif /* !INTERRUPT_LATENCY_TESTING */

typedef struct xfs_trans_reservations {
	uint	tr_write;	/* extent alloc trans */
	uint	tr_itruncate;	/* truncate trans */
	uint	tr_rename;	/* rename trans */
	uint	tr_link;	/* link trans */
	uint	tr_remove;	/* unlink trans */
	uint	tr_symlink;	/* symlink trans */
	uint	tr_create;	/* create trans */
	uint	tr_mkdir;	/* mkdir trans */
	uint	tr_ifree;	/* inode free trans */
	uint	tr_ichange;	/* inode update trans */
	uint	tr_growdata;	/* fs data section grow trans */
	uint	tr_swrite;	/* sync write inode trans */
	uint	tr_addafork;	/* cvt inode to attributed trans */
	uint	tr_writeid;	/* write setuid/setgid file */
	uint	tr_attrinval;	/* attr fork buffer invalidation */
	uint	tr_attrset;	/* set/create an attribute */
	uint	tr_attrrm;	/* remove an attribute */
	uint	tr_clearagi;	/* clear bad agi unlinked ino bucket */
	uint	tr_growrtalloc;	/* grow realtime allocations */
	uint	tr_growrtzero;	/* grow realtime zeroing */
	uint	tr_growrtfree;	/* grow realtime freeing */
} xfs_trans_reservations_t;

typedef struct xfs_mount {
	bhv_desc_t		m_bhv;		/* vfs xfs behavior */
	xfs_tid_t		m_tid;		/* next unused tid for fs */
	AIL_LOCK_T		m_ail_lock;	/* fs AIL mutex */
	xfs_ail_entry_t		m_ail;		/* fs active log item list */
	uint			m_ail_gen;	/* fs AIL generation count */
	xfs_sb_t		m_sb;		/* copy of fs superblock */
	lock_t			m_sb_lock;	/* sb counter mutex */
	struct buf		*m_sb_bp;	/* buffer for superblock */
	char			*m_fsname; 	/* filesystem name */
	int			m_fsname_len;	/* strlen of fs name */
	int			m_bsize;	/* fs logical block size */
	xfs_agnumber_t		m_agfrotor;	/* last ag where space found */
	xfs_agnumber_t		m_agirotor;	/* last ag dir inode alloced */
	int			m_ihsize;	/* size of next field */
	struct xfs_ihash	*m_ihash;	/* fs private inode hash table*/
	struct xfs_inode	*m_inodes;	/* active inode list */
	mutex_t			m_ilock;	/* inode list mutex */
	uint			m_ireclaims;	/* count of calls to reclaim*/
	uint			m_readio_log;	/* min read size log bytes */
	uint			m_readio_blocks; /* min read size blocks */
	uint			m_writeio_log;	/* min write size log bytes */
	uint			m_writeio_blocks; /* min write size blocks */
	void			*m_log;		/* log specific stuff */
	int			m_logbufs;	/* number of log buffers */
	int			m_logbsize;	/* size of each log buffer */
	uint			m_rsumlevels;	/* rt summary levels */
	uint			m_rsumsize;	/* size of rt summary, bytes */
	struct xfs_inode	*m_rbmip;	/* pointer to bitmap inode */
	struct xfs_inode	*m_rsumip;	/* pointer to summary inode */
	struct xfs_inode	*m_rootip;	/* pointer to root directory */
	struct xfs_quotainfo	*m_quotainfo;	/* disk quota information */
	buftarg_t		m_ddev_targ;	/* ptr to data device */
	buftarg_t		m_logdev_targ;	/* ptr to log device */
	buftarg_t		m_rtdev_targ;	/* ptr to rt device */
	buftarg_t		*m_ddev_targp;	/* saves taking the address */
#define m_ddevp		m_ddev_targ.specvp
#define m_logdevp	m_logdev_targ.specvp
#define m_rtdevp	m_rtdev_targ.specvp
#define m_dev		m_ddev_targ.dev
#define m_logdev	m_logdev_targ.dev
#define m_rtdev		m_rtdev_targ.dev
	__uint8_t		m_dircook_elog;	/* log d-cookie entry bits */
	__uint8_t		m_blkbit_log;	/* blocklog + NBBY */
	__uint8_t		m_blkbb_log;	/* blocklog - BBSHIFT */
	__uint8_t		m_agno_log;	/* log #ag's */
	__uint8_t		m_agino_log;	/* #bits for agino in inum */
	__uint8_t		m_nreadaheads;	/* #readahead buffers */
	__uint16_t		m_inode_cluster_size;/* min inode buf size */
	uint			m_blockmask;	/* sb_blocksize-1 */
	uint			m_blockwsize;	/* sb_blocksize in words */
	uint			m_blockwmask;	/* blockwsize-1 */
	uint			m_alloc_mxr[2];	/* XFS_ALLOC_BLOCK_MAXRECS */
	uint			m_alloc_mnr[2];	/* XFS_ALLOC_BLOCK_MINRECS */
	uint			m_bmap_dmxr[2];	/* XFS_BMAP_BLOCK_DMAXRECS */
	uint			m_bmap_dmnr[2];	/* XFS_BMAP_BLOCK_DMINRECS */
	uint			m_inobt_mxr[2];	/* XFS_INOBT_BLOCK_MAXRECS */
	uint			m_inobt_mnr[2];	/* XFS_INOBT_BLOCK_MINRECS */
	uint			m_ag_maxlevels;	/* XFS_AG_MAXLEVELS */
	uint			m_bm_maxlevels[2]; /* XFS_BM_MAXLEVELS */
	uint			m_in_maxlevels;	/* XFS_IN_MAXLEVELS */
	struct xfs_perag	*m_perag;	/* per-ag accounting info */
	mrlock_t		m_peraglock;	/* lock for m_perag (pointer) */
	sema_t			m_growlock;	/* growfs mutex */
	int			m_fixedfsid[2];	/* unchanged for life of FS */
	uint			m_dmevmask;	/* DMI events for this FS */
	uint			m_flags;	/* global mount flags */
	uint			m_attroffset;	/* inode attribute offset */
	int			m_da_node_ents;	/* how many entries in danode */
	int			m_ialloc_inos;	/* inodes in inode allocation */
	int			m_ialloc_blks;	/* blocks in inode allocation */
	int			m_litino;	/* size of inode union area */
	int			m_inoalign_mask;/* mask sb_inoalignmt if used */
	uint			m_qflags;	/* quota status flags */
	xfs_trans_reservations_t m_reservations;/* precomputed res values */
	__uint64_t		m_maxicount;	/* maximum inode count */
	__uint64_t		m_resblks;	/* total reserved blocks */
	__uint64_t		m_resblks_avail;/* available reserved blocks */
#if XFS_BIG_FILESYSTEMS
	xfs_ino_t		m_inoadd;	/* add value for ino64_offset */
#endif
	int			m_dalign;	/* stripe unit */
	int			m_swidth;	/* stripe width */
	int			m_sinoalign;	/* stripe unit inode alignmnt */
	int			m_attr_magicpct;/* 37% of the blocksize */
	int			m_dir_magicpct;	/* 37% of the dir blocksize */
	__uint8_t		m_mk_sharedro;	/* mark shared ro on unmount */
	__uint8_t		m_dirversion;	/* 1 or 2 */
	xfs_dirops_t		m_dirops;	/* table of dir funcs */
	int			m_dirblksize;	/* directory block sz--bytes */
	int			m_dirblkfsbs;	/* directory block sz--fsbs */
	xfs_dablk_t		m_dirdatablk;	/* blockno of dir data v2 */
	xfs_dablk_t		m_dirleafblk;	/* blockno of dir non-data v2 */
	xfs_dablk_t		m_dirfreeblk;	/* blockno of dirfreeindex v2 */
	int			m_chsize;	/* size of next field */
	struct xfs_chash	*m_chash;	/* fs private inode per-cluster
						 * hash table */
#if CELL_IRIX
	int			m_export[VFS_EILIMIT/sizeof(int)];
						/* Info to export to other
						 * cells. */
#endif
} xfs_mount_t;

/*
 * Flags for m_flags.
 */
#define	XFS_MOUNT_WSYNC		0x00000001	/* for nfs - all metadata ops
						   must be synchronous except
						   for space allocations */
#if XFS_BIG_FILESYSTEMS
#define	XFS_MOUNT_INO64		0x00000002
#endif
#define XFS_MOUNT_ROOTQCHECK	0x00000004
			     /* 0x00000008	-- currently unused */
#define XFS_MOUNT_FS_SHUTDOWN	0x00000010	/* atomic stop of all filesystem
						   operations, typically for
						   disk errors in metadata */
#define XFS_MOUNT_NOATIME	0x00000020	/* don't modify inode access
						   times on reads */
#define XFS_MOUNT_RETERR	0x00000040      /* return alignment errors to
                                                   user */
#define XFS_MOUNT_NOALIGN	0x00000080	/* turn off stripe alignment 
						   allocations */
#define XFS_MOUNT_UNSHARED      0x00000100      /* unshared mount */
#define XFS_MOUNT_REGISTERED    0x00000200      /* registered with cxfs master
                                                   cell logic */
#define XFS_MOUNT_NORECOVERY   	0x00000400      /* no recovery - dirty fs */
#define XFS_MOUNT_SHARED    	0x00000800      /* shared mount */
#define XFS_MOUNT_DFLT_IOSIZE  	0x00001000      /* set default i/o size */
#define XFS_MOUNT_OSYNCISDSYNC 	0x00002000      /* treat o_sync like o_dsync */

#define XFS_FORCED_SHUTDOWN(mp)	((mp)->m_flags & XFS_MOUNT_FS_SHUTDOWN)

/*
 * Default minimum read and write sizes.
 */
#define	XFS_READIO_LOG_SMALL	15	/* <= 32MB memory */
#define	XFS_WRITEIO_LOG_SMALL	15
#define	XFS_READIO_LOG_LARGE	16	/* > 32MB memory */
#define	XFS_WRITEIO_LOG_LARGE	16

/*
 * max and min values for UIO and mount-option defined I/O sizes
 * min value can't be less than a page.  Lower limit for 4K machines
 * is 8K because that's what was tested.
 */
#define XFS_MAX_IO_LOG		16	/* 64K */
#define	XFS_UIO_MAX_WRITEIO_LOG	16
#define	XFS_UIO_MAX_READIO_LOG	16

#if defined(SIM) || defined(_STANDALONE)
#define XFS_MIN_IO_LOG		13	/* 8K for simulation */
#define	XFS_UIO_MIN_WRITEIO_LOG	13
#define	XFS_UIO_MIN_READIO_LOG	13
#elif _PAGESZ == 16384
#define XFS_MIN_IO_LOG		14	/* 16K */
#define	XFS_UIO_MIN_WRITEIO_LOG	14
#define	XFS_UIO_MIN_READIO_LOG	14
#elif _PAGESZ == 4096
#define XFS_MIN_IO_LOG		13	/* 8K */
#define	XFS_UIO_MIN_WRITEIO_LOG	13
#define	XFS_UIO_MIN_READIO_LOG	13
#else
#error	"Unknown page size"
#endif


/*
 * Synchronous read and write sizes.  This should be
 * better for NFSv2 wsync filesystems.
 */
#define	XFS_WSYNC_READIO_LOG	15	/* 32K */
#define	XFS_WSYNC_WRITEIO_LOG	14	/* 16K */

/* 
 * Flags sent to xfs_force_shutdown.
 */
#define XFS_METADATA_IO_ERROR	0x1
#define XFS_LOG_IO_ERROR	0x2
#define XFS_FORCE_UMOUNT	0x4
#define XFS_CORRUPT_INCORE	0x8	/* corrupt in-memory data structures */

/*
 * Macros for getting from mount to vfs and back.
 */
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_MTOVFS)
struct vfs *xfs_mtovfs(xfs_mount_t *mp);
#define	XFS_MTOVFS(mp)		xfs_mtovfs(mp)
#else
#define	XFS_MTOVFS(mp)		(bhvtovfs(&(mp)->m_bhv))
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_BHVTOM)
xfs_mount_t *xfs_bhvtom(bhv_desc_t *bdp);
#define	XFS_BHVTOM(bdp)	xfs_bhvtom(bdp)
#else
#define	XFS_BHVTOM(bdp)		((xfs_mount_t *)BHV_PDATA(bdp))
#endif
 
/*
 * This structure is for use by the xfs_mod_incore_sb_batch() routine.
 */
typedef struct xfs_mod_sb {
	xfs_sb_field_t	msb_field;	/* Field to modify, see below */
	int		msb_delta;	/* change to make to the specified field */
} xfs_mod_sb_t;

#define	XFS_MOUNT_ILOCK(mp)	mutex_lock(&((mp)->m_ilock), PINOD)
#define	XFS_MOUNT_IUNLOCK(mp)	mutex_unlock(&((mp)->m_ilock))
#define	XFS_SB_LOCK(mp)		mutex_spinlock(&(mp)->m_sb_lock)
#define	XFS_SB_UNLOCK(mp,s)	mutex_spinunlock(&(mp)->m_sb_lock,(s))

#ifdef SIM
xfs_mount_t	*xfs_mount(dev_t, dev_t, dev_t);
xfs_mount_t	*xfs_mount_partial(dev_t, dev_t, dev_t);
void		xfs_umount(xfs_mount_t *);
#endif

void		xfs_mod_sb(xfs_trans_t *, __int64_t);
xfs_mount_t	*xfs_mount_init(void);
void		xfs_mount_free(xfs_mount_t *mp);
int		xfs_mountfs(struct vfs *, xfs_mount_t *mp, dev_t);
int		xfs_unmountfs(xfs_mount_t *, int, struct cred *);
int		xfs_mod_incore_sb(xfs_mount_t *, xfs_sb_field_t, int, int);
int		xfs_mod_incore_sb_batch(xfs_mount_t *, xfs_mod_sb_t *, uint, int);
int		xfs_readsb(xfs_mount_t *mp, dev_t);
struct buf	*xfs_getsb(xfs_mount_t *, int);
void            xfs_freesb(xfs_mount_t *);
void		xfs_force_shutdown(struct xfs_mount *, int);
extern	struct vfsops xfs_vfsops;
#endif	/* !_FS_XFS_MOUNT_H */
