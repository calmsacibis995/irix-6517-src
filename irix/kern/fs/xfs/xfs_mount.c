#ident	"$Revision: 1.183 $"

#include <limits.h>
#ifdef SIM
#define _KERNEL	1
#endif /* SIM */
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/sysmacros.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/uuid.h>
#include <sys/grio.h>
#include <sys/debug.h>
#ifdef SIM
#undef _KERNEL
#endif /* SIM */
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/open.h>
#include <sys/cred.h>
#include <sys/cmn_err.h>
#ifdef SIM
#include <bstring.h>
#else
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/var.h>
#include <fs/specfs/spec_lsnode.h>
#endif /* SIM */
#include <stddef.h>
#include "xfs_macros.h"
#include "xfs_types.h"
#include "xfs_inum.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_dir.h"
#include "xfs_dir2.h"
#include "xfs_mount.h"
#include "xfs_alloc_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_btree.h"
#include "xfs_ialloc.h"
#include "xfs_attr_sf.h"
#include "xfs_dir_sf.h"
#include "xfs_dir2_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_alloc.h"
#include "xfs_rtalloc.h"
#include "xfs_bmap.h"
#include "xfs_error.h"
#include "xfs_bit.h"
#include "xfs_rw.h"
#include "xfs_quota.h"
#include "xfs_fsops.h"
#if CELL || NOTYET
#include "xfs_cxfs.h"
#endif

#ifdef SIM
#include "sim.h"
#endif /* SIM */


STATIC int	xfs_mod_incore_sb_unlocked(xfs_mount_t *, xfs_sb_field_t, int, int);
STATIC void	xfs_sb_relse(buf_t *);
#ifndef SIM
STATIC void	xfs_mount_reset_sbqflags(xfs_mount_t *);
STATIC void	xfs_mount_log_sbunit(xfs_mount_t *, __int64_t);
STATIC void	xfs_uuid_mount(xfs_mount_t *);
STATIC void	xfs_uuid_unmount(xfs_mount_t *);

mutex_t		xfs_uuidtabmon;		/* monitor for uuidtab */
STATIC int	xfs_uuidtab_size;
STATIC uuid_t	*xfs_uuidtab;
#endif	/* !SIM */

/*
 * Return a pointer to an initialized xfs_mount structure.
 */
xfs_mount_t *
xfs_mount_init(void)
{
	xfs_mount_t *mp;

	mp = kmem_zalloc(sizeof(*mp), KM_SLEEP);

	AIL_LOCKINIT(&mp->m_ail_lock, "xfs_ail");
	spinlock_init(&mp->m_sb_lock, "xfs_sb");
	mutex_init(&mp->m_ilock, MUTEX_DEFAULT, "xfs_ilock");
	initnsema(&mp->m_growlock, 1, "xfs_grow");
	/*
	 * Initialize the AIL.
	 */
	xfs_trans_ail_init(mp);

	return mp;
}	/* xfs_mount_init */
	
/*
 * Free up the resources associated with a mount structure.  Assume that
 * the structure was initially zeroed, so we can tell which fields got
 * initialized.
 */
void
xfs_mount_free(xfs_mount_t *mp)
{
	if (mp->m_ihash)
		xfs_ihash_free(mp);
	if (mp->m_chash)
		xfs_chash_free(mp);

	if (mp->m_perag) {
		mrfree(&mp->m_peraglock);
		kmem_free(mp->m_perag,
			  sizeof(xfs_perag_t) * mp->m_sb.sb_agcount);
	}

#if 0
	/*
	 * XXXdpd - Doesn't work now for shutdown case.
	 * Should at least free the memory.
	 */
	ASSERT(mp->m_ail.ail_back == (xfs_log_item_t*)&(mp->m_ail));
	ASSERT(mp->m_ail.ail_forw == (xfs_log_item_t*)&(mp->m_ail));
#endif
	AIL_LOCK_DESTROY(&mp->m_ail_lock);
	spinlock_destroy(&mp->m_sb_lock);
	mutex_destroy(&mp->m_ilock);
	freesema(&mp->m_growlock);

	if (mp->m_fsname != NULL) {
		kmem_free(mp->m_fsname, mp->m_fsname_len);
	}
	if (mp->m_quotainfo != NULL) {
		xfs_qm_unmount_quotadestroy(mp);
	}
	VFS_REMOVEBHV(XFS_MTOVFS(mp), &mp->m_bhv);
	kmem_free(mp, sizeof(xfs_mount_t));
}	/* xfs_mount_free */


/*
 * Check the validity of the SB found.
 */
STATIC int
xfs_mount_validate_sb(
	xfs_mount_t	*mp,
	xfs_sb_t	*sbp)
{
	/*
	 * If the log device and data device have the 
	 * same device number, the log is internal. 
	 * Consequently, the sb_logstart should be non-zero.  If
	 * we have a zero sb_logstart in this case, we may be trying to mount
	 * a volume filesystem in a non-volume manner.
	 */
	if (sbp->sb_magicnum != XFS_SB_MAGIC || !XFS_SB_GOOD_VERSION(sbp))
		return XFS_ERROR(EWRONGFS);
	if (sbp->sb_logstart == 0 && mp->m_logdev == mp->m_dev)
		return XFS_ERROR(EFSCORRUPTED);

	/* 
	 * More sanity checking. These were stolen directly from
	 * xfs_repair.
	 */
	if (sbp->sb_blocksize <= 0 					||
	    sbp->sb_agcount <= 0 					||
	    sbp->sb_sectsize <= 0 					||
	    sbp->sb_blocklog < XFS_MIN_BLOCKSIZE_LOG			||
	    sbp->sb_blocklog > XFS_MAX_BLOCKSIZE_LOG 			||
	    sbp->sb_inodesize < XFS_DINODE_MIN_SIZE 			||
	    sbp->sb_inodesize > XFS_DINODE_MAX_SIZE 			||
	    (sbp->sb_rextsize * sbp->sb_blocksize > XFS_MAX_RTEXTSIZE) 	||
	    (sbp->sb_rextsize * sbp->sb_blocksize < XFS_MIN_RTEXTSIZE) 	||
	    sbp->sb_imax_pct > 100)
		return XFS_ERROR(EFSCORRUPTED);

	/* 
	 * sanity check ag count, size fields against data size field 
	 */
	if (sbp->sb_dblocks == 0 ||
	    sbp->sb_dblocks >
	     (xfs_drfsbno_t)sbp->sb_agcount * sbp->sb_agblocks ||
	    sbp->sb_dblocks < (xfs_drfsbno_t)(sbp->sb_agcount - 1) * 
			      sbp->sb_agblocks + XFS_MIN_AG_BLOCKS)
		return XFS_ERROR(EFSCORRUPTED);

#if !XFS_BIG_FILESYSTEMS
	if (sbp->sb_dblocks > INT_MAX || sbp->sb_rblocks > INT_MAX)  {
		cmn_err(CE_WARN,
"XFS:  File systems greater than 1TB not supported on this system.\n");
		return XFS_ERROR(E2BIG);
	}
#endif

#ifndef SIM
	/*
	 * Except for from mkfs, don't let partly-mkfs'ed filesystems mount.
	 */
	if (sbp->sb_inprogress) 
		return XFS_ERROR(EFSCORRUPTED);
#endif	
	return (0);
}

/*
 * xfs_readsb 
 * 
 * Does the initial read of the superblock.  This has been split out from
 * xfs_mountfs_int so that the cxfs v1 array mount code can get at the
 * unique id for the file system before deciding whether we are going
 * to mount things as a cxfs client or server.
 */
int
xfs_readsb(xfs_mount_t *mp, dev_t dev)
{
	buf_t		*bp;
	xfs_sb_t	*sbp;
	int		error = 0;

	ASSERT(mp->m_sb_bp == 0);

	/*
	 * Allocate a (locked) buffer to hold the superblock.
	 * This will be kept around at all time to optimize
	 * access to the superblock.
	 */
	bp = ngetrbuf(BBTOB(BTOBB(sizeof(xfs_sb_t))));
	ASSERT(bp != NULL);
	ASSERT((bp->b_flags & B_BUSY) && valusema(&bp->b_lock) <= 0);

	/*
	 * Initialize and read in the superblock buffer.
	 */
	bp->b_edev = dev;
	bp->b_relse = xfs_sb_relse;
	bp->b_blkno = XFS_SB_DADDR;		
	bp->b_flags |= B_READ;
	bp->b_target = mp->m_ddev_targp;
	xfsbdstrat(mp, bp);
	if (error = iowait(bp)) {
		goto err;
	}

	/*
	 * Initialize the mount structure from the superblock.
	 * But first do some basic consistency checking.
	 */
	sbp = XFS_BUF_TO_SBP(bp);
	if (error = xfs_mount_validate_sb(mp, sbp)) {
		goto err;
	}

	mp->m_sb_bp = bp;
	mp->m_sb = *sbp;				/* bcopy structure */
	brelse(bp);
	ASSERT(valusema(&bp->b_lock) > 0);
	return 0;

 err:
	nfreerbuf(bp);
	return error;
}

/*
 * xfs_mountfs_int
 *
 * This function does the following on an initial mount of a file system:
 *	- reads the superblock from disk and init the mount struct
 *	- if we're an o32 kernel, do a size check on the superblock
 *		so we don't mount terabyte filesystems
 *	- init mount struct realtime fields
 *	- allocate inode hash table for fs
 *	- init directory manager
 *	- perform recovery and init the log manager
 *
 *	- if read_rootinos is set to 1 (typical case), the
 *		root inodes will be read in.  The flag is set
 *		to 0 only in special cases when the filesystem
 *		is thought to be corrupt but we want a mount
 *		structure set up so we can use the XFS_* macros.
 *		If read_rootinos is 0, the superblock *must* be good.
 *		and the filesystem should be unmounted and remounted
 *		before any real xfs filesystem code can be run against
 *		the filesystem.  This flag is used only by simulation
 *		code for fixing up the filesystem btrees.
 *
 *	- if cxfs_mount is set to 1 then we are actually importing an
 *		already mounted filesystem into a cell. We do not go
 *		near the log, and do not mess with a bunch of stuff.
 */
int
xfs_mountfs_int(vfs_t *vfsp, xfs_mount_t *mp, dev_t dev, int read_rootinos,
	int cxfs_mount)
{
	buf_t		*bp;
	xfs_sb_t	*sbp = &(mp->m_sb);
	int		error = 0;
	int		i;
	xfs_inode_t	*rip;
	vnode_t		*rvp = 0;
	int		readio_log;
	int		writeio_log;
	vmap_t		vmap;
	daddr_t		d;
	extern dev_t	rootdev;		/* from sys/systm.h */
#ifndef SIM
	__uint64_t	ret64;
	uint		quotaflags, quotaondisk, rootqcheck, needquotacheck;
	boolean_t	needquotamount;
	__int64_t	update_flags;
#endif
	int		noio;

	if (vfsp->vfs_flag & VFS_REMOUNT)   /* Can't remount XFS filesystems */
		return 0;

	noio = dev == 0 && mp->m_sb_bp != NULL;
	if (mp->m_sb_bp == NULL) {
		if (error = xfs_readsb(mp, dev)) {
			return (error);
		}
	}
	mp->m_agfrotor = mp->m_agirotor = 0;
	mp->m_blkbit_log = sbp->sb_blocklog + XFS_NBBYLOG;
	mp->m_blkbb_log = sbp->sb_blocklog - BBSHIFT;
	mp->m_agno_log = xfs_highbit32(sbp->sb_agcount - 1) + 1;
	mp->m_agino_log = sbp->sb_inopblog + sbp->sb_agblklog;
	mp->m_litino = sbp->sb_inodesize -
		((uint)sizeof(xfs_dinode_core_t) + (uint)sizeof(xfs_agino_t));
	mp->m_blockmask = sbp->sb_blocksize - 1;
	mp->m_blockwsize = sbp->sb_blocksize >> XFS_WORDLOG;
	mp->m_blockwmask = mp->m_blockwsize - 1;

	/*
	 * Check is sb_agblocks is aligned at stripe boundary
	 * If sb_agblocks is NOT aligned turn off m_dalign since
	 * allocator alignment is within an ag, therefore ag has
	 * to be aligned at stripe boundary.
	 */
#ifndef SIM
	update_flags = 0LL;
#endif
	if (mp->m_dalign && !cxfs_mount) {
		/*
		 * If stripe unit and stripe width are not multiples
		 * of the fs blocksize turn off alignment.
		 */
		if ((BBTOB(mp->m_dalign) & mp->m_blockmask) ||
		    (BBTOB(mp->m_swidth) & mp->m_blockmask)) {
			if (mp->m_flags & XFS_MOUNT_RETERR) {
				error = XFS_ERROR(EINVAL);
				goto error1;
			}
			mp->m_dalign = mp->m_swidth = 0;
		} else {
			/*
			 * Convert the stripe unit and width to FSBs.
			 */
			mp->m_dalign = XFS_BB_TO_FSBT(mp, mp->m_dalign);
			if (mp->m_dalign && (sbp->sb_agblocks % mp->m_dalign)) {
				if (mp->m_flags & XFS_MOUNT_RETERR) {
					error = XFS_ERROR(EINVAL);
					goto error1;
				}
				mp->m_dalign = 0;
				mp->m_swidth = 0;
			} else if (mp->m_dalign) {
				mp->m_swidth = XFS_BB_TO_FSBT(mp, mp->m_swidth);
			} else {
				if (mp->m_flags & XFS_MOUNT_RETERR) {
					error = XFS_ERROR(EINVAL);
					goto error1;
				}
				mp->m_swidth = 0;
			}
		}
		
#ifndef SIM
		/* 
		 * Update superblock with new values
		 * and log changes
		 */
		if (XFS_SB_VERSION_HASDALIGN(sbp)) { 
			if (sbp->sb_unit != mp->m_dalign) {
				sbp->sb_unit = mp->m_dalign;
				update_flags |= XFS_SB_UNIT;
			}
			if (sbp->sb_width != mp->m_swidth) {
				sbp->sb_width = mp->m_swidth;
				update_flags |= XFS_SB_WIDTH;
			}
		}
#endif
	} else if ((mp->m_flags & XFS_MOUNT_NOALIGN) != XFS_MOUNT_NOALIGN &&
		    XFS_SB_VERSION_HASDALIGN(&mp->m_sb)) {
			mp->m_dalign = sbp->sb_unit;
			mp->m_swidth = sbp->sb_width;
	}


	/*
	 * Setup for attributes, in case they get created.
	 * This value is for inodes getting attributes for the first time,
	 * the per-inode value is for old attribute values.
	 */
	ASSERT(sbp->sb_inodesize >= 256 && sbp->sb_inodesize <= 2048);
	switch (sbp->sb_inodesize) {
	case 256:
		mp->m_attroffset = XFS_LITINO(mp) - XFS_BMDR_SPACE_CALC(2);
		break;
	case 512:
	case 1024:
	case 2048:
		mp->m_attroffset = XFS_BMDR_SPACE_CALC(12);
		break;
	default:
		ASSERT(0);
	}
	ASSERT(mp->m_attroffset < XFS_LITINO(mp));

	for (i = 0; i < 2; i++) {
		mp->m_alloc_mxr[i] = XFS_BTREE_BLOCK_MAXRECS(sbp->sb_blocksize,
			xfs_alloc, i == 0);
		mp->m_alloc_mnr[i] = XFS_BTREE_BLOCK_MINRECS(sbp->sb_blocksize,
			xfs_alloc, i == 0);
	}
	for (i = 0; i < 2; i++) {
		mp->m_bmap_dmxr[i] = XFS_BTREE_BLOCK_MAXRECS(sbp->sb_blocksize,
			xfs_bmbt, i == 0);
		mp->m_bmap_dmnr[i] = XFS_BTREE_BLOCK_MINRECS(sbp->sb_blocksize,
			xfs_bmbt, i == 0);
	}
	for (i = 0; i < 2; i++) {
		mp->m_inobt_mxr[i] = XFS_BTREE_BLOCK_MAXRECS(sbp->sb_blocksize,
			xfs_inobt, i == 0);
		mp->m_inobt_mnr[i] = XFS_BTREE_BLOCK_MINRECS(sbp->sb_blocksize,
			xfs_inobt, i == 0);
	}
	xfs_alloc_compute_maxlevels(mp);
	xfs_bmap_compute_maxlevels(mp, XFS_DATA_FORK);
	xfs_bmap_compute_maxlevels(mp, XFS_ATTR_FORK);
	xfs_ialloc_compute_maxlevels(mp);
	mp->m_bsize = XFS_FSB_TO_BB(mp, 1);
	vfsp->vfs_bsize = (u_int)XFS_FSB_TO_B(mp, 1);
	mp->m_ialloc_inos = (int)MAX(XFS_INODES_PER_CHUNK, sbp->sb_inopblock);
	mp->m_ialloc_blks = mp->m_ialloc_inos >> sbp->sb_inopblog;
	if (sbp->sb_imax_pct) {
		/* Make sure the maximum inode count is a multiple of the
		 * units we allocate inodes in.
		 */
		mp->m_maxicount = (sbp->sb_dblocks * sbp->sb_imax_pct) / 100;
		mp->m_maxicount = ((mp->m_maxicount / mp->m_ialloc_blks) *
				   mp->m_ialloc_blks)  <<
				   sbp->sb_inopblog;
	} else
		mp->m_maxicount = 0;

	/*
	 * XFS uses the uuid from the superblock as the unique
	 * identifier for fsid.  We can not use the uuid from the volume
	 * since a single partition filesystem is identical to a single
	 * partition volume/filesystem.
	 */
#ifndef SIM
	if (!cxfs_mount) {
		xfs_uuid_mount(mp);	/* make sure it's really unique */
		ret64 = uuid_hash64(&sbp->sb_uuid, (uint *)&i);
		bcopy(&ret64, &vfsp->vfs_fsid, sizeof(ret64));
	}
#endif

	/*
	 * Set the default minimum read and write sizes unless
	 * already specified in a mount option.
	 * We use smaller I/O sizes when the file system
	 * is being used for NFS service (wsync mount option).
	 */
	if (!(mp->m_flags & XFS_MOUNT_DFLT_IOSIZE)) {
		if (mp->m_flags & XFS_MOUNT_WSYNC) {
			readio_log = XFS_WSYNC_READIO_LOG;
			writeio_log = XFS_WSYNC_WRITEIO_LOG;
		} else {
#if !defined(SIM) && _MIPS_SIM != _ABI64
			if (physmem <= 8192) {		/* <= 32MB */
				readio_log = XFS_READIO_LOG_SMALL;
				writeio_log = XFS_WRITEIO_LOG_SMALL;
			} else {
				readio_log = XFS_READIO_LOG_LARGE;
				writeio_log = XFS_WRITEIO_LOG_LARGE;
			}
#else
			readio_log = XFS_READIO_LOG_LARGE;
			writeio_log = XFS_WRITEIO_LOG_LARGE;
#endif
		}
	} else {
		readio_log = mp->m_readio_log;
		writeio_log = mp->m_writeio_log;
	}

#if !defined(SIM) && _MIPS_SIM != _ABI64
	/*
	 * Set the number of readahead buffers to use based on
	 * physical memory size.
	 */
	if (physmem <= 4096)		/* <= 16MB */
		mp->m_nreadaheads = XFS_RW_NREADAHEAD_16MB;
	else if (physmem <= 8192)	/* <= 32MB */
		mp->m_nreadaheads = XFS_RW_NREADAHEAD_32MB;
	else
		mp->m_nreadaheads = XFS_RW_NREADAHEAD_K32;
#else
	mp->m_nreadaheads = XFS_RW_NREADAHEAD_K64;
#endif
	if (sbp->sb_blocklog > readio_log) {
		mp->m_readio_log = sbp->sb_blocklog;
	} else {
		mp->m_readio_log = readio_log;
	}
	mp->m_readio_blocks = 1 << (mp->m_readio_log - sbp->sb_blocklog);
	if (sbp->sb_blocklog > writeio_log) {
		mp->m_writeio_log = sbp->sb_blocklog;
	} else {
		mp->m_writeio_log = writeio_log;
	}
	mp->m_writeio_blocks = 1 << (mp->m_writeio_log - sbp->sb_blocklog);

	/*
	 * Set the inode cluster size based on the physical memory
	 * size.  This may still be overridden by the file system
	 * block size if it is larger than the chosen cluster size.
	 */
#if !defined(SIM) && _MIPS_SIM != _ABI64
	if (physmem <= btoc(32 * 1024 * 1024)) { /* <= 32 MB */
		mp->m_inode_cluster_size = XFS_INODE_SMALL_CLUSTER_SIZE;
	} else {
		mp->m_inode_cluster_size = XFS_INODE_BIG_CLUSTER_SIZE;
	}
#else
	mp->m_inode_cluster_size = XFS_INODE_BIG_CLUSTER_SIZE;
#endif
	/*
	 * Set whether we're using inode alignment.
	 */
	if (XFS_SB_VERSION_HASALIGN(&mp->m_sb) &&
	    mp->m_sb.sb_inoalignmt >=
	    XFS_B_TO_FSBT(mp, mp->m_inode_cluster_size))
		mp->m_inoalign_mask = mp->m_sb.sb_inoalignmt - 1;
	else
		mp->m_inoalign_mask = 0;
	/*
	 * If we are using stripe alignment, check whether
	 * the stripe unit is a multiple of the inode alignment
	 */
	if (mp->m_dalign && mp->m_inoalign_mask &&
	    !(mp->m_dalign & mp->m_inoalign_mask))
		mp->m_sinoalign = mp->m_dalign;
	else
		mp->m_sinoalign = 0;
	/*
	 * Check that the data (and log if separate) are an ok size.
	 */
	d = (daddr_t)XFS_FSB_TO_BB(mp, mp->m_sb.sb_dblocks);
	if (XFS_BB_TO_FSB(mp, d) != mp->m_sb.sb_dblocks) {
		error = XFS_ERROR(E2BIG);
		goto error1;
	}
	if (!noio) {
		error = xfs_read_buf(mp, mp->m_ddev_targp, d - 1, 1, 0, &bp);
		if (!error) {
			brelse(bp);
		} else {
			if (error == ENOSPC) {
				error = XFS_ERROR(E2BIG);
			}
			goto error1;
		}
	}

	if (!noio && !cxfs_mount && mp->m_logdev && mp->m_logdev != mp->m_dev) {
		d = (daddr_t)XFS_FSB_TO_BB(mp, mp->m_sb.sb_logblocks);
		if (XFS_BB_TO_FSB(mp, d) != mp->m_sb.sb_logblocks) {
			error = XFS_ERROR(E2BIG);
			goto error1;
		}
		error = xfs_read_buf(mp, &mp->m_logdev_targ, d - 1, 1, 0, &bp);
		if (!error) {
			brelse(bp);
		} else {
			if (error == ENOSPC) {
				error = XFS_ERROR(E2BIG);
			}
			goto error1;
		}
	}

	/*
	 * Initialize realtime fields in the mount structure
	 */
	if (error = xfs_rtmount_init(mp))
		goto error1;

	/*
	 * For cxfs case we are done now
	 */
	if (cxfs_mount) {
		return(0);
	}

	/*
	 *  Copies the low order bits of the timestamp and the randomly
	 *  set "sequence" number out of a UUID.
	 */
	uuid_getnodeuniq(&sbp->sb_uuid, mp->m_fixedfsid);

	/*
	 *  The vfs structure needs to have a file system independent
	 *  way of checking for the invariant file system ID.  Since it
	 *  can't look at mount structures it has a pointer to the data
	 *  in the mount structure.
	 *
	 *  File systems that don't support user level file handles (i.e.
	 *  all of them except for XFS) will leave vfs_altfsid as NULL.
	 */
	vfsp->vfs_altfsid = (fsid_t *)mp->m_fixedfsid;
	mp->m_dmevmask = 0;	/* not persistent; set after each mount */

	/*
	 * Select the right directory manager.
	 */
	mp->m_dirops =
		XFS_SB_VERSION_HASDIRV2(&mp->m_sb) ?
			xfsv2_dirops :
			xfsv1_dirops;

	/*
	 * Initialize directory manager's entries.
	 */
	XFS_DIR_MOUNT(mp);

	/*
	 * Initialize the attribute manager's entries.
	 */
	mp->m_attr_magicpct = (mp->m_sb.sb_blocksize * 37) / 100;

	/*
	 * Initialize the precomputed transaction reservations values.
	 */
	xfs_trans_init(mp);
	if (noio) {
		ASSERT(read_rootinos == 0 && cxfs_mount == 0);
		return 0;
	}

	/*
	 * Allocate and initialize the inode hash table for this
	 * file system.
	 */
	xfs_ihash_init(mp);
	xfs_chash_init(mp);

	/*
	 * Allocate and initialize the per-ag data.
	 */
	mrinit(&mp->m_peraglock, "xperag");
	mp->m_perag =
		kmem_zalloc(sbp->sb_agcount * sizeof(xfs_perag_t), KM_SLEEP);

	/*
	 * log's mount-time initialization. Perform 1st part recovery if needed
	 */
	if (sbp->sb_logblocks > 0) {		/* check for volume case */
		error = xfs_log_mount(mp, mp->m_logdev,
				      XFS_FSB_TO_DADDR(mp, sbp->sb_logstart),
				      XFS_FSB_TO_BB(mp, sbp->sb_logblocks));
		if (error) {
			goto error2;
		}
	} else {	/* No log has been defined */
		error = XFS_ERROR(EFSCORRUPTED);
		goto error2;
	}

	/*
	 * Mkfs calls mount before the root inode is allocated.
	 */
	if (read_rootinos == 1 && sbp->sb_rootino != NULLFSINO) {
		/*
		 * Get and sanity-check the root inode.
		 * Save the pointer to it in the mount structure.
		 */
		error = xfs_iget(mp, NULL, sbp->sb_rootino, XFS_ILOCK_EXCL,
				 &rip, 0);
		if (error) {
			goto error2;
		}
		ASSERT(rip != NULL);
		rvp = XFS_ITOV(rip);
		if ((rip->i_d.di_mode & IFMT) != IFDIR) {
			VMAP(rvp, vmap);
			prdev("Root inode %d is not a directory",
			      (int)rip->i_dev, rip->i_ino);
			xfs_iunlock(rip, XFS_ILOCK_EXCL);
			VN_RELE(rvp);
			vn_purge(rvp, &vmap);
			error = XFS_ERROR(EFSCORRUPTED);
			goto error2;
		}
		VN_FLAGSET(rvp, VROOT);
		mp->m_rootip = rip;				/* save it */
		xfs_iunlock(rip, XFS_ILOCK_EXCL);
	}

	/*
	 * Initialize realtime inode pointers in the mount structure
	 */
	if (read_rootinos == 1 && (error = xfs_rtmount_inodes(mp))) {
		/*
		 * Free up the root inode.
		 */
		VMAP(rvp, vmap);
		VN_RELE(rvp);
		vn_purge(rvp, &vmap);
		goto error2;
	}

	
#ifndef SIM 
	/*
	 * If fs is not mounted readonly, then update the superblock
	 * unit and width changes.
	 */
	if (update_flags && !(vfsp->vfs_flag & VFS_RDONLY))
		xfs_mount_log_sbunit(mp, update_flags);	

	quotaflags = 0;
	needquotamount = B_FALSE;
	quotaondisk = XFS_SB_VERSION_HASQUOTA(&mp->m_sb) &&
		mp->m_sb.sb_qflags & (XFS_UQUOTA_ACCT|XFS_PQUOTA_ACCT);
	/*
	 * Figure out if we'll need to do a quotacheck.
	 * The requirements are a little different depending on whether
	 * this fs is root or not.
	 */
	rootqcheck = (mp->m_dev == rootdev && quotaondisk && 
		      ((mp->m_sb.sb_qflags & XFS_UQUOTA_ACCT &&
			(mp->m_sb.sb_qflags & XFS_UQUOTA_CHKD) == 0) ||
		       (mp->m_sb.sb_qflags & XFS_PQUOTA_ACCT &&
			(mp->m_sb.sb_qflags & XFS_PQUOTA_CHKD) == 0)));
	needquotacheck = rootqcheck ||  XFS_QM_NEED_QUOTACHECK(mp);
	if (XFS_IS_QUOTA_ON(mp) || quotaondisk) {
		/*
		 * Call mount_quotas at this point only if we won't have to do
		 * a quotacheck.
		 */
		if (quotaondisk && !needquotacheck) {
			/*
			 * If xfsquotas pkg isn't not installed,
			 * we have to reset the quotachk'd bit.
			 * If an error occured, qm_mount_quotas code
			 * has already disabled quotas. So, just finish
			 * mounting, and get on with the boring life 
			 * without disk quotas.
			 */
			if (xfs_qm_mount_quotas(mp))
				xfs_mount_reset_sbqflags(mp);
		} else {
			/*
			 * Clear the quota flags, but remember them. This
			 * is so that the quota code doesn't get invoked
			 * before we're ready. This can happen when an
			 * inode goes inactive and wants to free blocks,
			 * or via xfs_log_mount_finish.
			 */
			quotaflags = mp->m_qflags;
			mp->m_qflags = 0;
			needquotamount = B_TRUE;
		}
	}
#endif		
	/*
	 * Finish recovering the file system.  This part needed to be
	 * delayed until after the root and real-time bitmap inodes
	 * were consistently read in.
	 */
#ifndef SIM
	error = xfs_log_mount_finish(mp);
#endif
	if (error) {
		goto error2;
	}


#ifndef SIM 
	if (needquotamount) {
		ASSERT(mp->m_qflags == 0);
		mp->m_qflags = quotaflags; 
		if (xfs_qm_mount_quotas(mp))
			xfs_mount_reset_sbqflags(mp);
	}

#if defined(DEBUG) && defined(XFS_LOUD_RECOVERY)
	if (! (XFS_IS_QUOTA_ON(mp))) {
		xfs_fs_cmn_err(CE_NOTE, mp, "Disk quotas not turned on");
	} else {
		xfs_fs_cmn_err(CE_NOTE, mp, "Disk quotas turned on");
	}
#endif
#endif /* !SIM */

#ifdef QUOTADEBUG
	if (XFS_IS_QUOTA_ON(mp)) {
		extern int	xfs_qm_internalqcheck(xfs_mount_t *mp);
		if (xfs_qm_internalqcheck(mp))
			debug("qcheck failed");
	}
#endif

	/* Make superblock exportable in cellular case */
#if CELL_IRIX
	{
		extern	void	cxfs_export(vfs_t *, xfs_mount_t *);

		cxfs_export(vfsp, mp);
	}
#endif
	return (0);

 error2:
	xfs_ihash_free(mp);
	xfs_chash_free(mp);
	mrfree(&mp->m_peraglock);
	kmem_free(mp->m_perag, sbp->sb_agcount * sizeof(xfs_perag_t));
	mp->m_perag = NULL;
	/* FALLTHROUGH */
 error1:
	xfs_freesb(mp);
	return error;
}	/* xfs_mountfs_int */

/*
 * wrapper routine for the kernel
 */
int
xfs_mountfs(vfs_t *vfsp, xfs_mount_t *mp, dev_t dev)
{
	return(xfs_mountfs_int(vfsp, mp, dev, 1, 0));
}

#ifdef SIM
STATIC xfs_mount_t *
xfs_mount_int(dev_t dev, dev_t logdev, dev_t rtdev, int read_rootinos)
{
	int		error;
	xfs_mount_t	*mp;
	vfs_t		*vfsp;

	mp = xfs_mount_init();
	vfsp = kmem_zalloc(sizeof(vfs_t), KM_SLEEP);
	VFS_INIT(vfsp);
	vfs_insertbhv(vfsp, &mp->m_bhv, &xfs_vfsops, mp);
	mp->m_dev = dev;
	mp->m_rtdev = rtdev;
	mp->m_logdev = logdev;
	mp->m_ddev_targp = &mp->m_ddev_targ;
	vfsp->vfs_dev = dev;

	error = xfs_mountfs_int(vfsp, mp, dev, read_rootinos, 0);
	if (error) {
		kmem_free(mp, sizeof(*mp));
		return 0;
	}

	/*
	 * Call the log's mount-time initialization.
	 */
	if (logdev) {
		xfs_sb_t *sbp;
		xfs_fsblock_t logstart;

		sbp = XFS_BUF_TO_SBP(mp->m_sb_bp);
		logstart = sbp->sb_logstart;
		xfs_log_mount(mp, logdev, XFS_FSB_TO_DADDR(mp, logstart),
			      XFS_FSB_TO_BB(mp, sbp->sb_logblocks));
	}

	return mp;
}

/*
 * xfs_mount is the function used by the simulation environment
 * to start the file system.
 */
xfs_mount_t *
xfs_mount(dev_t dev, dev_t logdev, dev_t rtdev)
{
	return(xfs_mount_int(dev, logdev, rtdev, 1));
}

/*
 * xfs_mount_setup is used by the simulation environment to
 * mount a filesystem where everything but the superblock
 * might be trashed.  Beware:  the root inodes are NOT read in.
 */
xfs_mount_t *
xfs_mount_partial(dev_t dev, dev_t logdev, dev_t rtdev)
{
	return(xfs_mount_int(dev, logdev, rtdev, 0));
}
#endif /* SIM */

/*
 * xfs_unmountfs
 * 
 * This code is common to all unmounting paths: via non-root unmount,
 * the simulator/mkfs path, and possibly the root unmount path.
 * 
 * This flushes out the inodes,dquots and the superblock, unmounts the
 * log and makes sure that incore structures are freed.
 */
int
xfs_unmountfs(xfs_mount_t *mp, int vfs_flags, struct cred *cr)
{
	buf_t		*sbp;
	xfs_sb_t	*sb;
	/* REFERENCED */
	int		error;
	/* REFERENCED */
	int		unused;
#ifndef SIM
	int		ndquots;
#if defined(DEBUG) || defined(INDUCE_IO_ERROR)
	int64_t		fsid;
#endif
#endif

	xfs_iflush_all(mp, XFS_FLUSH_ALL);
	
#ifndef SIM
	/*
	 * Purge the dquot cache. 
	 * None of the dquots should really be busy at this point.
	 */
	if (mp->m_quotainfo) {
		while (ndquots = xfs_qm_dqpurge_all(mp, 
						  XFS_QMOPT_UQUOTA|
						  XFS_QMOPT_PQUOTA|
						  XFS_QMOPT_UMOUNTING)) {
			delay(ndquots * 10);
		}
	}
#endif
	/*
	 * Flush out the log synchronously so that we know for sure
	 * that nothing is pinned.  This is important because bflush()
	 * will skip pinned buffers.
	 */
	xfs_log_force(mp, (xfs_lsn_t)0, XFS_LOG_FORCE | XFS_LOG_SYNC);
	
	
	binval(mp->m_dev);
	bflushed(mp->m_dev);
	if (mp->m_rtdev != NODEV) {
		binval(mp->m_rtdev);
	}
	/*
	 * skip superblock write if fs is read-only, or
	 * if we are doing a forced umount.
	 */
	sbp = xfs_getsb(mp, 0);
	if (!(XFS_MTOVFS(mp)->vfs_flag & VFS_RDONLY ||
	      mp->m_flags & XFS_MOUNT_FS_SHUTDOWN)) {
		/*
		 * mark shared-readonly if desired
		 */
		sb = XFS_BUF_TO_SBP(sbp);
		if (mp->m_mk_sharedro) {
			if (!(sb->sb_flags & XFS_SBF_READONLY))
				sb->sb_flags |= XFS_SBF_READONLY;
			if (!XFS_SB_VERSION_HASSHARED(sb))
				XFS_SB_VERSION_ADDSHARED(sb);
			xfs_fs_cmn_err(CE_NOTE, mp,
				"Unmounting, marking shared read-only");
		}
		sbp->b_flags &= ~(B_DONE | B_READ);
		sbp->b_flags |= B_WRITE;
		bwait_unpin(sbp);
		ASSERT(sbp->b_edev == mp->m_dev);
		xfsbdstrat(mp, sbp);
		/* Nevermind errors we might get here. */
		error = iowait(sbp);
		if (error && mp->m_mk_sharedro)
			xfs_fs_cmn_err(CE_ALERT, mp, "Superblock write error detected while unmounting.  Filesystem may not be marked shared readonly");
	}
	
	xfs_log_unmount(mp);			/* Done! No more fs ops. */

#ifndef SIM
	spec_unmounted(mp->m_ddevp);
#endif

	if (mp->m_ddevp) {
		VOP_CLOSE(mp->m_ddevp, vfs_flags, L_TRUE, cr, unused);
		VN_RELE(mp->m_ddevp);
	}
	if (mp->m_rtdevp) {
		VOP_CLOSE(mp->m_rtdevp, vfs_flags, L_TRUE, cr, unused);
		VN_RELE(mp->m_rtdevp);
	}
	if (mp->m_logdevp && mp->m_logdevp != mp->m_ddevp) {
		VOP_CLOSE(mp->m_logdevp, vfs_flags, L_TRUE, cr, unused);
		VN_RELE(mp->m_logdevp);
	}

	nfreerbuf(sbp);

	/*
	 * All inodes from this mount point should be freed.
	 */
	ASSERT(mp->m_inodes == NULL);

#ifndef SIM
	/*
	 * We may have bufs that are in the process of getting written still.
	 * We must wait for the I/O completion of those. The sync flag here
	 * does a two pass iteration thru the bufcache.
	 */
	if (XFS_FORCED_SHUTDOWN(mp)) {
		(void)incore_relse(mp->m_dev, 0, 1); /* synchronous */
	}
	xfs_uuid_unmount(mp);

#if defined(DEBUG) || defined(INDUCE_IO_ERROR)
	/*
	 * clear all error tags on this filesystem
	 */
	bcopy(&(XFS_MTOVFS(mp)->vfs_fsid), &fsid, sizeof(int64_t));
	(void) xfs_errortag_clearall_umount(fsid, mp->m_fsname, 0);
#endif

#endif /* !SIM */
#if CELL || NOTYET
	cxfs_unmount(mp);
#endif /* CELL || NOTYET */
	xfs_mount_free(mp);
	return 0;
}	/* xfs_unmountfs */
		


#ifdef SIM
/*
 * xfs_umount is the function used by the simulation environment
 * to stop the file system.
 */
void
xfs_umount(xfs_mount_t *mp)
{
	xfs_unmountfs(mp, 0, NULL);
}
#endif /* SIM */


/*
 * xfs_mod_sb() can be used to copy arbitrary changes to the
 * in-core superblock into the superblock buffer to be logged.
 * It does not provide the higher level of locking that is
 * needed to protect the in-core superblock from concurrent
 * access.
 */
void
xfs_mod_sb(xfs_trans_t *tp, __int64_t fields)
{
	buf_t		*bp;
	int		first;
	int		last;
	xfs_mount_t	*mp;
	xfs_sb_t	*sbp;
	static const short offsets[] = {
		offsetof(xfs_sb_t, sb_magicnum),
		offsetof(xfs_sb_t, sb_blocksize),
		offsetof(xfs_sb_t, sb_dblocks),
		offsetof(xfs_sb_t, sb_rblocks),
		offsetof(xfs_sb_t, sb_rextents),
		offsetof(xfs_sb_t, sb_uuid),
		offsetof(xfs_sb_t, sb_logstart),
		offsetof(xfs_sb_t, sb_rootino),
		offsetof(xfs_sb_t, sb_rbmino),
		offsetof(xfs_sb_t, sb_rsumino),
		offsetof(xfs_sb_t, sb_rextsize),
		offsetof(xfs_sb_t, sb_agblocks),
		offsetof(xfs_sb_t, sb_agcount),
		offsetof(xfs_sb_t, sb_rbmblocks),
		offsetof(xfs_sb_t, sb_logblocks),
		offsetof(xfs_sb_t, sb_versionnum),
		offsetof(xfs_sb_t, sb_sectsize),
		offsetof(xfs_sb_t, sb_inodesize),
		offsetof(xfs_sb_t, sb_inopblock),
		offsetof(xfs_sb_t, sb_fname[0]),
		offsetof(xfs_sb_t, sb_fpack[0]),
		offsetof(xfs_sb_t, sb_blocklog),
		offsetof(xfs_sb_t, sb_sectlog),
		offsetof(xfs_sb_t, sb_inodelog),
		offsetof(xfs_sb_t, sb_inopblog),
		offsetof(xfs_sb_t, sb_agblklog),
		offsetof(xfs_sb_t, sb_rextslog),
		offsetof(xfs_sb_t, sb_inprogress),
		offsetof(xfs_sb_t, sb_imax_pct),
		offsetof(xfs_sb_t, sb_icount),
		offsetof(xfs_sb_t, sb_ifree),
		offsetof(xfs_sb_t, sb_fdblocks),
		offsetof(xfs_sb_t, sb_frextents),
		offsetof(xfs_sb_t, sb_uquotino),
		offsetof(xfs_sb_t, sb_pquotino),
		offsetof(xfs_sb_t, sb_qflags),
		offsetof(xfs_sb_t, sb_flags),
		offsetof(xfs_sb_t, sb_shared_vn),
		offsetof(xfs_sb_t, sb_inoalignmt),
		offsetof(xfs_sb_t, sb_unit),
		offsetof(xfs_sb_t, sb_width),
		offsetof(xfs_sb_t, sb_dirblklog),
		sizeof(xfs_sb_t)
	};
 
	ASSERT(fields);
	if (!fields)
		return;
	mp = tp->t_mountp;
	bp = xfs_trans_getsb(tp, mp, 0);
	sbp = XFS_BUF_TO_SBP(bp);
	first = sizeof(xfs_sb_t);
	last = 0;
	while (fields) {
		xfs_sb_field_t	f;
		int		first1;
		int		last1;

		f = (xfs_sb_field_t)xfs_lowbit64((__uint64_t)fields);
		ASSERT((1LL << f) & XFS_SB_MOD_BITS);
		first1 = offsets[f];
		last1 = offsets[f + 1] - 1;
		bcopy((caddr_t)&mp->m_sb + first1, (caddr_t)sbp + first1,
			last1 - first1 + 1);
		if (first1 < first)
			first = first1;
		if (last1 > last)
			last = last1;
		fields &= ~(1LL << f);
	}
	xfs_trans_log_buf(tp, bp, first, last);
}


/*
 * xfs_mod_incore_sb_unlocked() is a utility routine common used to apply
 * a delta to a specified field in the in-core superblock.  Simply
 * switch on the field indicated and apply the delta to that field.
 * Fields are not allowed to dip below zero, so if the delta would
 * do this do not apply it and return EINVAL.
 *
 * The SB_LOCK must be held when this routine is called.
 */
STATIC int
xfs_mod_incore_sb_unlocked(xfs_mount_t *mp, xfs_sb_field_t field, 
                          int delta, int rsvd)
{
	int		scounter;	/* short counter for 32 bit fields */
	long long	lcounter;	/* long counter for 64 bit fields */
	long long res_used, rem;

	/*
	 * With the in-core superblock spin lock held, switch
	 * on the indicated field.  Apply the delta to the
	 * proper field.  If the fields value would dip below
	 * 0, then do not apply the delta and return EINVAL.
	 */
	switch (field) {
	case XFS_SBS_ICOUNT:
		lcounter = (long long)mp->m_sb.sb_icount;
		lcounter += delta;
		if (lcounter < 0) {
			ASSERT(0);
			return (XFS_ERROR(EINVAL));
		}
		mp->m_sb.sb_icount = lcounter;
		return (0);
	case XFS_SBS_IFREE:
		lcounter = (long long)mp->m_sb.sb_ifree;
		lcounter += delta;
		if (lcounter < 0) {
			ASSERT(0);
			return (XFS_ERROR(EINVAL));
		}
		mp->m_sb.sb_ifree = lcounter;
		return (0);
	case XFS_SBS_FDBLOCKS:

		lcounter = (long long)mp->m_sb.sb_fdblocks;
		res_used = (long long)(mp->m_resblks - mp->m_resblks_avail);

		if (delta > 0) {		/* Putting blocks back */
			if (res_used > delta) {
				mp->m_resblks_avail += delta;
			} else {
				rem = delta - res_used;
				mp->m_resblks_avail = mp->m_resblks;
				lcounter += rem;
			}
		} else {				/* Taking blocks away */

			lcounter += delta;

		/*
		 * If were out of blocks, use any available reserved blocks if 
		 * were allowed to.
		 */

			if (lcounter < 0) {
				if (rsvd) {
					lcounter = (long long)mp->m_resblks_avail + delta;
					if (lcounter < 0) {
						return (XFS_ERROR(ENOSPC));
					}
					mp->m_resblks_avail = lcounter;
					return (0);
				} else { 	/* not reserved */
					return (XFS_ERROR(ENOSPC));
				}
			} 
		}

		mp->m_sb.sb_fdblocks = lcounter;
		return (0);
	case XFS_SBS_FREXTENTS:
		lcounter = (long long)mp->m_sb.sb_frextents;
		lcounter += delta;
		if (lcounter < 0) {
			return (XFS_ERROR(ENOSPC));
		}
		mp->m_sb.sb_frextents = lcounter;
		return (0);
	case XFS_SBS_DBLOCKS:
		lcounter = (long long)mp->m_sb.sb_dblocks;
		lcounter += delta;
		if (lcounter < 0) {
			ASSERT(0);
			return (XFS_ERROR(EINVAL));
		}
		mp->m_sb.sb_dblocks = lcounter;
		return (0);
	case XFS_SBS_AGCOUNT:
		scounter = mp->m_sb.sb_agcount;
		scounter += delta;
		if (scounter < 0) {
			ASSERT(0);
			return (XFS_ERROR(EINVAL));
		}
		mp->m_sb.sb_agcount = scounter;
		return (0);
	case XFS_SBS_IMAX_PCT:
		scounter = mp->m_sb.sb_imax_pct;
		scounter += delta;
		if (scounter < 0) {
			ASSERT(0);
			return (XFS_ERROR(EINVAL));
		}
		mp->m_sb.sb_imax_pct = scounter;
		return (0);
	case XFS_SBS_REXTSIZE:
		scounter = mp->m_sb.sb_rextsize;
		scounter += delta;
		if (scounter < 0) {
			ASSERT(0);
			return (XFS_ERROR(EINVAL));
		}
		mp->m_sb.sb_rextsize = scounter;
		return (0);
	case XFS_SBS_RBMBLOCKS:
		scounter = mp->m_sb.sb_rbmblocks;
		scounter += delta;
		if (scounter < 0) {
			ASSERT(0);
			return (XFS_ERROR(EINVAL));
		}
		mp->m_sb.sb_rbmblocks = scounter;
		return (0);
	case XFS_SBS_RBLOCKS:
		lcounter = (long long)mp->m_sb.sb_rblocks;
		lcounter += delta;
		if (lcounter < 0) {
			ASSERT(0);
			return (XFS_ERROR(EINVAL));
		}
		mp->m_sb.sb_rblocks = lcounter;
		return (0);
	case XFS_SBS_REXTENTS:
		lcounter = (long long)mp->m_sb.sb_rextents;
		lcounter += delta;
		if (lcounter < 0) {
			ASSERT(0);
			return (XFS_ERROR(EINVAL));
		}
		mp->m_sb.sb_rextents = lcounter;
		return (0);
	case XFS_SBS_REXTSLOG:
		scounter = mp->m_sb.sb_rextslog;
		scounter += delta;
		if (scounter < 0) {
			ASSERT(0);
			return (XFS_ERROR(EINVAL));
		}
		mp->m_sb.sb_rextslog = scounter;
		return (0);
	default:
		ASSERT(0);
		return (XFS_ERROR(EINVAL));
	}
}

/*
 * xfs_mod_incore_sb() is used to change a field in the in-core
 * superblock structure by the specified delta.  This modification
 * is protected by the SB_LOCK.  Just use the xfs_mod_incore_sb_unlocked()
 * routine to do the work.
 */
int
xfs_mod_incore_sb(xfs_mount_t *mp, xfs_sb_field_t field, int delta, int rsvd)
{
	int	s;
	int	status;

	s = XFS_SB_LOCK(mp);
	status = xfs_mod_incore_sb_unlocked(mp, field, delta, rsvd);
	XFS_SB_UNLOCK(mp, s);
	return (status);
}



/*
 * xfs_mod_incore_sb_batch() is used to change more than one field
 * in the in-core superblock structure at a time.  This modification
 * is protected by a lock internal to this module.  The fields and
 * changes to those fields are specified in the array of xfs_mod_sb
 * structures passed in.
 *
 * Either all of the specified deltas will be applied or none of
 * them will.  If any modified field dips below 0, then all modifications
 * will be backed out and EINVAL will be returned.
 */
int
xfs_mod_incore_sb_batch(xfs_mount_t *mp, xfs_mod_sb_t *msb, uint nmsb, int rsvd)
{
	int		s;
	int		status;
	xfs_mod_sb_t	*msbp;

	/*
	 * Loop through the array of mod structures and apply each
	 * individually.  If any fail, then back out all those
	 * which have already been applied.  Do all of this within
	 * the scope of the SB_LOCK so that all of the changes will
	 * be atomic.
	 */
	s = XFS_SB_LOCK(mp);
	msbp = &msb[0];
	for (msbp = &msbp[0]; msbp < (msb + nmsb); msbp++) {
		/*
		 * Apply the delta at index n.  If it fails, break
		 * from the loop so we'll fall into the undo loop
		 * below.
		 */
		status = xfs_mod_incore_sb_unlocked(mp, msbp->msb_field,
						    msbp->msb_delta, rsvd);
		if (status != 0) {
			break;
		}
	}

	/*
	 * If we didn't complete the loop above, then back out
	 * any changes made to the superblock.  If you add code
	 * between the loop above and here, make sure that you
	 * preserve the value of status. Loop back until
	 * we step below the beginning of the array.  Make sure
	 * we don't touch anything back there.
	 */
	if (status != 0) {
		msbp--;
		while (msbp >= msb) {
			status = xfs_mod_incore_sb_unlocked(mp,
				    msbp->msb_field, -(msbp->msb_delta), rsvd);
			ASSERT(status == 0);
			msbp--;
		}
	}
	XFS_SB_UNLOCK(mp, s);
	return (status);
}

		
/*
 * xfs_getsb() is called to obtain the buffer for the superblock.
 * The buffer is returned locked and read in from disk.
 * The buffer should be released with a call to xfs_brelse().
 *
 * If the flags parameter is BUF_TRYLOCK, then we'll only return
 * the superblock buffer if it can be locked without sleeping.
 * If it can't then we'll return NULL.
 */
buf_t *
xfs_getsb(xfs_mount_t	*mp,
	  int		flags)
{
	buf_t	*bp;

	ASSERT(mp->m_sb_bp != NULL);
	bp = mp->m_sb_bp;
	if (flags & BUF_TRYLOCK) {
		if (!cpsema(&bp->b_lock)) {
			return NULL;
		}
	} else {
		psema(&bp->b_lock, PRIBIO);
	}
	ASSERT(bp->b_flags & B_DONE);
	return (bp);
}

/*
 * Used to free the superblock along various error paths.
 */
void 
xfs_freesb(
        xfs_mount_t	*mp)
{
        buf_t	*bp;

	/*
	 * Use xfs_getsb() so that the buffer will be locked
	 * when we call nfreerbuf().
	 */
	bp = xfs_getsb(mp, 0);
	nfreerbuf(bp);
	mp->m_sb_bp = NULL;
}

/*
 * This is the brelse function for the private superblock buffer.
 * All it needs to do is unlock the buffer and clear any spurious
 * flags.
 */
STATIC void
xfs_sb_relse(buf_t *bp)
{
	ASSERT(bp->b_flags & B_BUSY);
	ASSERT(valusema(&bp->b_lock) <= 0);
	bp->b_flags &= ~(B_ASYNC | B_READ);
	bp->av_forw = NULL;
	bp->av_back = NULL;
	vsema(&bp->b_lock);
}

#ifndef SIM
/*
 * See if the uuid is unique among mounted xfs filesystems.
 * If it's not, allocate a new one so it is.
 */
STATIC void
xfs_uuid_mount(xfs_mount_t *mp)
{
	int	hole;
	int	i;
	uint_t	status;

	mutex_lock(&xfs_uuidtabmon, PVFS);
	for (i = 0, hole = -1; i < xfs_uuidtab_size; i++) {
		if (uuid_is_nil(&xfs_uuidtab[i], &status)) {
			hole = i;
			continue;
		}
		if (!uuid_equal(&mp->m_sb.sb_uuid, &xfs_uuidtab[i], &status))
			continue;
		uuid_create(&mp->m_sb.sb_uuid, &status);
		XFS_BUF_TO_SBP(mp->m_sb_bp)->sb_uuid = mp->m_sb.sb_uuid;
		i = -1;
		/* restart loop from the top */
	}
	if (hole < 0) {
		xfs_uuidtab = kmem_realloc(xfs_uuidtab,
			(xfs_uuidtab_size + 1) * sizeof(*xfs_uuidtab),
			KM_SLEEP);
		hole = xfs_uuidtab_size++;
	}
	xfs_uuidtab[hole] = mp->m_sb.sb_uuid;
	mutex_unlock(&xfs_uuidtabmon);
}

/*
 * Remove filesystem from the uuid table.
 */
STATIC void
xfs_uuid_unmount(xfs_mount_t *mp)
{
	int	i;
	uint_t	status;

	mutex_lock(&xfs_uuidtabmon, PVFS);
	for (i = 0; i < xfs_uuidtab_size; i++) {
		if (uuid_is_nil(&xfs_uuidtab[i], &status))
			continue;
		if (!uuid_equal(&mp->m_sb.sb_uuid, &xfs_uuidtab[i], &status))
			continue;
		uuid_create_nil(&xfs_uuidtab[i], &status);
		break;
	}
	ASSERT(i < xfs_uuidtab_size);
	mutex_unlock(&xfs_uuidtabmon);
}

/*
 * When xfsquotas isn't installed and the superblock had quotas, we need to
 * clear the quotaflags from superblock.
 */
STATIC void
xfs_mount_reset_sbqflags(
	xfs_mount_t	*mp)
{
	xfs_trans_t	*tp;
	int		s;

	mp->m_qflags = 0;
	/*
	 * It is OK to look at sb_qflags here in mount path,
	 * without SB_LOCK.
	 */
	if (mp->m_sb.sb_qflags == 0)
		return;
	s = XFS_SB_LOCK(mp);
	mp->m_sb.sb_qflags = 0;
	XFS_SB_UNLOCK(mp, s);

	/*
	 * if the fs is readonly, let the incore superblock run
	 * with quotas off but don't flush the update out to disk
	 */
	if (XFS_MTOVFS(mp)->vfs_flag & VFS_RDONLY)
		return;
#ifdef QUOTADEBUG	
	xfs_fs_cmn_err(CE_NOTE, mp, "Writing superblock quota changes");
#endif
	tp = xfs_trans_alloc(mp, XFS_TRANS_QM_SBCHANGE);
	if (xfs_trans_reserve(tp, 0, mp->m_sb.sb_sectsize + 128, 0, 0, 
				      XFS_DEFAULT_LOG_COUNT)) {
		xfs_trans_cancel(tp, 0);
		return;
	}
	xfs_mod_sb(tp, XFS_SB_QFLAGS);
	(void)xfs_trans_commit(tp, 0, NULL);
}

/*
 * Used to log changes to the superblock unit and width fields which could
 * be altered by the mount options. Only the first superblock is updated.
 */
STATIC void
xfs_mount_log_sbunit(
	xfs_mount_t *mp,
	__int64_t fields)
{
	xfs_trans_t *tp;

	ASSERT(fields & (XFS_SB_UNIT|XFS_SB_WIDTH));

	if (mp->m_sb.sb_unit == 0 && mp->m_sb.sb_width == 0)
		return;

	tp = xfs_trans_alloc(mp, XFS_TRANS_SB_UNIT);
	if (xfs_trans_reserve(tp, 0, mp->m_sb.sb_sectsize + 128, 0, 0, 
				XFS_DEFAULT_LOG_COUNT)) {
		xfs_trans_cancel(tp, 0);
		return;
	}
	xfs_mod_sb(tp, fields);
	(void)xfs_trans_commit(tp, 0, NULL);
}
	
#endif /* !SIM */
