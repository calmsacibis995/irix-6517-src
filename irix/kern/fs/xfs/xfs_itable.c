#ident	"$Revision: 1.61 $"

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/vnode.h>
#include <sys/systm.h>
#include <sys/sema.h>
#include <sys/immu.h>
#include <sys/kmem.h>
#include <sys/time.h>
#include <sys/debug.h>
#include <ksys/vfile.h>
#include <ksys/fdt.h>
#include <sys/vfs.h>
#include <sys/syssgi.h>
#include <sys/capability.h>
#include <sys/kthread.h>
#include <sys/uuid.h>
#include <sys/hwgraph.h>
#include "xfs_macros.h"
#include "xfs_types.h"
#include "xfs_inum.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_dir.h"
#include "xfs_dir2.h"
#include "xfs_mount.h"
#include "xfs_ag.h"
#include "xfs_alloc_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_btree.h"
#include "xfs_attr_sf.h"
#include "xfs_dir_sf.h"
#include "xfs_dir2_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_ialloc.h"
#include "xfs_itable.h"
#include "xfs_error.h"

/*
 * Return stat information for one inode.
 * Return 0 if ok, else errno.
 */
STATIC int				/* error status */
xfs_bulkstat_one(
	xfs_mount_t	*mp,		/* mount point for filesystem */
	xfs_trans_t	*tp,		/* transaction pointer */
	xfs_ino_t	ino,		/* inode number to get data for */
	void		*buffer,	/* buffer to place output in */
	daddr_t		bno,		/* starting bno of inode cluster */
	void		*dibuff,	/* on-disk inode buffer */
	int		*stat)		/* BULKSTAT_RV_... */
{
	xfs_bstat_t	*buf;		/* return buffer */
	int		error;		/* error value */
	xfs_dinode_t	*dip;		/* dinode inode pointer */
	xfs_dinode_core_t *dic;		/* dinode core info pointer */
	xfs_inode_t	*ip = NULL;	/* incore inode pointer */

	buf = (xfs_bstat_t *)buffer;
	dip = (xfs_dinode_t *)dibuff;
	if (! buf || ino == mp->m_sb.sb_rbmino || ino == mp->m_sb.sb_rsumino ||
	    (XFS_SB_VERSION_HASQUOTA(&mp->m_sb) &&
	     (ino == mp->m_sb.sb_uquotino || ino == mp->m_sb.sb_pquotino))) {
		*stat = BULKSTAT_RV_NOTHING;
		return XFS_ERROR(EINVAL);
	}

	if (dip == NULL) {
		/* We're not being passed a pointer to a dinode.  This happens
		 * if BULKSTAT_FG_IGET is selected.  Do the iget.
		 */
		error = xfs_iget(mp, tp, ino, XFS_ILOCK_SHARED, &ip, bno);
		if (error) {
			*stat = BULKSTAT_RV_NOTHING;
			return error;
		}
		ASSERT(ip != NULL);
		ASSERT(ip->i_blkno != (daddr_t)0);
		if (ip->i_d.di_mode == 0) {
			xfs_iput(ip, XFS_ILOCK_SHARED);
			*stat = BULKSTAT_RV_NOTHING;
			return XFS_ERROR(ENOENT);
		}
		dic = &ip->i_d;
		ASSERT(dic != NULL);

		/* xfs_iget returns the following without needing
		 * further change.
		 */
		buf->bs_nlink = dic->di_nlink;
		buf->bs_projid = dic->di_projid;

	} else {
		dic = &dip->di_core;
		ASSERT(dic != NULL);

		/*
		 * The inode format changed when we moved the link count and
		 * made it 32 bits long.  If this is an old format inode,
		 * convert it in memory to look like a new one.  If it gets
		 * flushed to disk we will convert back before flushing or
		 * logging it.  We zero out the new projid field and the old link
		 * count field.  We'll handle clearing the pad field (the remains
		 * of the old uuid field) when we actually convert the inode to
		 * the new format. We don't change the version number so that we
		 * can distinguish this from a real new format inode.
		 */
		if (dic->di_version == XFS_DINODE_VERSION_1) {
			buf->bs_nlink = dic->di_onlink;
			buf->bs_projid = 0;
		}
		else {
			buf->bs_nlink = dic->di_nlink;
			buf->bs_projid = dic->di_projid;
		}

	}

	buf->bs_ino = ino;
	buf->bs_mode = dic->di_mode;
	buf->bs_uid = dic->di_uid;
	buf->bs_gid = dic->di_gid;
	buf->bs_size = dic->di_size;
	buf->bs_atime.tv_sec = dic->di_atime.t_sec;
	buf->bs_atime.tv_nsec = dic->di_atime.t_nsec;
	buf->bs_mtime.tv_sec = dic->di_mtime.t_sec;
	buf->bs_mtime.tv_nsec = dic->di_mtime.t_nsec;
	buf->bs_ctime.tv_sec = dic->di_ctime.t_sec;
	buf->bs_ctime.tv_nsec = dic->di_ctime.t_nsec;
	/*
	 * convert di_flags to bs_xflags.
	 */
	buf->bs_xflags =
		((dic->di_flags & XFS_DIFLAG_REALTIME) ?
			XFS_XFLAG_REALTIME : 0) |
		((dic->di_flags & XFS_DIFLAG_PREALLOC) ?
			XFS_XFLAG_PREALLOC : 0) |
		(XFS_CFORK_Q(dic) ?
			XFS_XFLAG_HASATTR : 0);
	buf->bs_extsize = dic->di_extsize << mp->m_sb.sb_blocklog;
	buf->bs_extents = dic->di_nextents;
	buf->bs_gen = dic->di_gen;
	bzero(buf->bs_pad, sizeof(buf->bs_pad));
	buf->bs_dmevmask = dic->di_dmevmask;
	buf->bs_dmstate = dic->di_dmstate;
	buf->bs_aextents = dic->di_anextents;
	switch (dic->di_format) {
	case XFS_DINODE_FMT_DEV:
		if ( ip ) {
			buf->bs_rdev = ip->i_df.if_u2.if_rdev;
		} else {
			buf->bs_rdev = dip->di_u.di_dev;
		}

		buf->bs_blksize = BLKDEV_IOSIZE;
		buf->bs_blocks = 0;
		break;
	case XFS_DINODE_FMT_LOCAL:
	case XFS_DINODE_FMT_UUID:
		buf->bs_rdev = 0;
		buf->bs_blksize = mp->m_sb.sb_blocksize;
		buf->bs_blocks = 0;
		break;
	case XFS_DINODE_FMT_EXTENTS:
	case XFS_DINODE_FMT_BTREE:
		buf->bs_rdev = 0;
		buf->bs_blksize = mp->m_sb.sb_blocksize;
		if ( ip ) {
			buf->bs_blocks = dic->di_nblocks + ip->i_delayed_blks;
		} else {
			buf->bs_blocks = dic->di_nblocks;
		}
		break;
	}

	if (ip) {
		xfs_iput(ip, XFS_ILOCK_SHARED);
	}

	*stat = BULKSTAT_RV_DIDONE;
	return 0;
}

/*
 * Return stat information in bulk (by-inode) for the filesystem.
 */
int					/* error status */
xfs_bulkstat(
	xfs_mount_t		*mp,	/* mount point for filesystem */
	xfs_trans_t		*tp,	/* transaction pointer */
	ino64_t			*lastinop, /* last inode returned */
	int			*ubcountp, /* size of buffer/count returned */
	bulkstat_one_pf		formatter, /* func that'd fill a single buf */
	size_t			statstruct_size, /* sizeof struct filling */
	caddr_t			ubuffer, /* buffer with inode stats */
	int			flag, 	/* BULKSTAT_FG_QUICK|BULKSTAT_FG_IGET */
	int			*done)	/* 1 if there're more stats to get */
{
	xfs_agblock_t		agbno;	/* allocation group block number */
	buf_t			*agbp;	/* agi header buffer */
	xfs_agi_t		*agi;	/* agi header data */
	xfs_agino_t		agino;	/* inode # in allocation group */
	xfs_agnumber_t		agno;	/* allocation group number */
	daddr_t			bno;	/* inode cluster start daddr */
	int			chunkidx; /* current index into inode chunk */
	int			clustidx; /* current index into inode cluster */
	xfs_btree_cur_t		*cur;	/* btree cursor for ialloc btree */
	int			end_of_ag; /* set if we've seen the ag end */
	int			error;	/* error code */
	int                     fmterror;/* bulkstat formatter result */
	__int32_t		gcnt;	/* current btree rec's count */
	xfs_inofree_t		gfree;	/* current btree rec's free mask */
	xfs_agino_t		gino;	/* current btree rec's start inode */
	int			i;	/* loop index */
	int			icount;	/* count of inodes good in irbuf */
	xfs_ino_t		ino;	/* inode number (filesystem) */
	xfs_inobt_rec_t		*irbp;	/* current irec buffer pointer */
	xfs_inobt_rec_t		*irbuf;	/* start of irec buffer */
	xfs_inobt_rec_t		*irbufend; /* end of good irec buffer entries */
	xfs_ino_t		lastino; /* last inode number returned */
	int			nbcluster; /* # of blocks in a cluster */
	int			nicluster; /* # of inodes in a cluster */
	int			nimask;	/* mask for inode clusters */
	int			nirbuf;	/* size of irbuf */
	int			rval;	/* return value error code */
	int			tmp;	/* result value from btree calls */
	int			ubcount; /* size of user's buffer */
	int			ubleft;	/* spaces left in user's buffer */
	caddr_t			ubufp;	/* current pointer into user's buffer */
	buf_t			*bp;	/* ptr to on-disk inode cluster buf */
	xfs_dinode_t		*dip;	/* ptr into bp for specific inode */
	xfs_inode_t		*ip;	/* ptr to in-core inode struct */
	vfs_t			*vfsp;

	/*
	 * Check that the device is valid/mounted and mark it busy
	 * for the duration of this call.
	 */
	vfsp = XFS_MTOVFS(mp);
	if (error = vfs_busy(vfsp))
		return error;
	
	/*
	 * Get the last inode value, see if there's nothing to do.
	 */
	ino = (xfs_ino_t)*lastinop;
	dip = NULL;
	agno = XFS_INO_TO_AGNO(mp, ino);
	agino = XFS_INO_TO_AGINO(mp, ino); 
	if (agno >= mp->m_sb.sb_agcount ||
	    ino != XFS_AGINO_TO_INO(mp, agno, agino)) {
		*done = 1;
		*ubcountp = 0;
		vfs_unbusy(vfsp);
		return 0;
	}
	ubcount = ubleft = *ubcountp;
	*ubcountp = 0;
	*done = 0;
	fmterror = 0;
	ubufp = ubuffer;
	nicluster = mp->m_sb.sb_blocksize >= XFS_INODE_CLUSTER_SIZE(mp) ?
		mp->m_sb.sb_inopblock :
		(XFS_INODE_CLUSTER_SIZE(mp) >> mp->m_sb.sb_inodelog);
	nimask = ~(nicluster - 1);
	nbcluster = nicluster >> mp->m_sb.sb_inopblog;
	/* 
	 * Lock down the user's buffer. If a buffer was not sent, as in the case
	 * disk quota code calls here, we skip this.
	 */
	if (ubuffer &&
	    (error = useracc(ubuffer, ubcount * statstruct_size,
			(B_READ|B_PHYS), NULL))) {
		vfs_unbusy(vfsp);
		return error;
	}
	/*
	 * Allocate a page-sized buffer for inode btree records.
	 * We could try allocating something smaller, but for normal
	 * calls we'll always (potentially) need the whole page.
	 */
	irbuf = kmem_alloc(NBPC, KM_SLEEP);
	nirbuf = NBPC / sizeof(*irbuf);
	/* 
	 * Loop over the allocation groups, starting from the last 
	 * inode returned; 0 means start of the allocation group.
	 */
	rval = 0;
	while (ubleft > 0 && agno < mp->m_sb.sb_agcount) {
		bp = NULL;
		mrlock(&mp->m_peraglock, MR_ACCESS, PINOD);
		error = xfs_ialloc_read_agi(mp, tp, agno, &agbp);
		mrunlock(&mp->m_peraglock);
		if (error) {
			/*
			 * Skip this allocation group and go to the next one.
			 */
			agno++;
			agino = 0;
			continue;
		}
		agi = XFS_BUF_TO_AGI(agbp);
		/*
		 * Allocate and initialize a btree cursor for ialloc btree.
		 */
		cur = xfs_btree_init_cursor(mp, tp, agbp, agno, XFS_BTNUM_INO,
			(xfs_inode_t *)0, 0);
		irbp = irbuf;
		irbufend = irbuf + nirbuf;
		end_of_ag = 0;
		/*
		 * If we're returning in the middle of an allocation group,
		 * we need to get the remainder of the chunk we're in.
		 */
		if (agino > 0) {
			/*
			 * Lookup the inode chunk that this inode lives in.
			 */
			error = xfs_inobt_lookup_le(cur, agino, 0, 0, &tmp);
			if (!error &&	/* no I/O error */
			    tmp &&	/* lookup succeeded */
					/* got the record, should always work */
			    !(error = xfs_inobt_get_rec(cur, &gino, &gcnt,
				    &gfree, &i)) &&
			    i == 1 &&
					/* this is the right chunk */
			    agino < gino + XFS_INODES_PER_CHUNK &&
					/* lastino was not last in chunk */
			    (chunkidx = agino - gino + 1) <
				    XFS_INODES_PER_CHUNK &&
					/* there are some left allocated */
			    XFS_INOBT_MASKN(chunkidx,
				    XFS_INODES_PER_CHUNK - chunkidx) & ~gfree) {
				/*
				 * Grab the chunk record.  Mark all the
				 * uninteresting inodes (because they're 
				 * before our start point) free.
				 */
				for (i = 0; i < chunkidx; i++) {
					if (XFS_INOBT_MASK(i) & ~gfree)
						gcnt++;
				}
				gfree |= XFS_INOBT_MASKN(0, chunkidx);
				irbp->ir_startino = gino;
				irbp->ir_freecount = gcnt;
				irbp->ir_free = gfree;
				irbp++;
				agino = gino + XFS_INODES_PER_CHUNK;
				icount = XFS_INODES_PER_CHUNK - gcnt;
			} else {
				/*
				 * If any of those tests failed, bump the
				 * inode number (just in case).
				 */
				agino++;
				icount = 0;
			}
			/*
			 * In any case, increment to the next record.
			 */
			if (!error)
				error = xfs_inobt_increment(cur, 0, &tmp);
		} else {
			/*
			 * Start of ag.  Lookup the first inode chunk.
			 */
			error = xfs_inobt_lookup_ge(cur, 0, 0, 0, &tmp);
			icount = 0;
		}
		/*
		 * Loop through inode btree records in this ag,
		 * until we run out of inodes or space in the buffer.
		 */
		while (irbp < irbufend && icount < ubcount) {
			/*
			 * Loop as long as we're unable to read the
			 * inode btree.
			 */
			while (error) {
				agino += XFS_INODES_PER_CHUNK;
				if (XFS_AGINO_TO_AGBNO(mp, agino) >=
				    agi->agi_length)
					break;
				error = xfs_inobt_lookup_ge(cur, agino, 0, 0,
							    &tmp);
			}
			/*
			 * If ran off the end of the ag either with an error,
			 * or the normal way, set end and stop collecting.
			 */
			if (error ||
			    (error = xfs_inobt_get_rec(cur, &gino, &gcnt,
				    &gfree, &i)) ||
			    i == 0) {
				end_of_ag = 1;
				break;
			}
			/*
			 * If this chunk has any allocated inodes, save it.
			 */
			if (gcnt < XFS_INODES_PER_CHUNK) {
				irbp->ir_startino = gino;
				irbp->ir_freecount = gcnt;
				irbp->ir_free = gfree;
				irbp++;
				icount += XFS_INODES_PER_CHUNK - gcnt;
			}
			/*
			 * Set agino to after this chunk and bump the cursor.
			 */
			agino = gino + XFS_INODES_PER_CHUNK;
			error = xfs_inobt_increment(cur, 0, &tmp);
		}
		/*
		 * Drop the btree buffers and the agi buffer.
		 * We can't hold any of the locks these represent
		 * when calling iget.
		 */
		xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);
		xfs_trans_brelse(tp, agbp);
		/*
		 * Now format all the good inodes into the user's buffer.
		 */
		irbufend = irbp;
		for (irbp = irbuf; irbp < irbufend && ubleft > 0; irbp++) {
			/*
			 * Read-ahead the next chunk's worth of inodes.
			 */
			if (&irbp[1] < irbufend) {
				/*
				 * Loop over all clusters in the next chunk.
				 * Do a readahead if there are any allocated
				 * inodes in that cluster.
				 */
				for (agbno = XFS_AGINO_TO_AGBNO(mp,
							irbp[1].ir_startino),
				     chunkidx = 0;
				     chunkidx < XFS_INODES_PER_CHUNK;
				     chunkidx += nicluster,
				     agbno += nbcluster) {
					if (XFS_INOBT_MASKN(chunkidx,
							    nicluster) &
					    ~(irbp[1].ir_free))
						xfs_btree_reada_bufs(mp, agno,
							agbno, nbcluster);
				}
			}
			/*
			 * Now process this chunk of inodes.
			 */
			for (agino = irbp->ir_startino, chunkidx = 0, clustidx = 0;
			     ubleft > 0 &&
				irbp->ir_freecount < XFS_INODES_PER_CHUNK;
			     chunkidx++, clustidx++, agino++) {
				ASSERT(chunkidx < XFS_INODES_PER_CHUNK);
				/*
				 * Recompute agbno if this is the
				 * first inode of the cluster.
				 * 
				 * Careful with clustidx.   There can be 
				 * multple clusters per chunk, a single
				 * cluster per chunk or a cluster that has
				 * inodes represented from several different
				 * chunks (if blocksize is large).
				 * 
				 * Because of this, the starting clustidx is 
				 * initialized to zero in this loop but must
				 * later be reset after reading in the cluster
				 * buffer.
				 */
				if ((chunkidx & (nicluster - 1)) == 0) {
					agbno = XFS_AGINO_TO_AGBNO(mp,
							irbp->ir_startino) +
						((chunkidx & nimask) >>
						 mp->m_sb.sb_inopblog);

					if (flag == BULKSTAT_FG_QUICK) {
						ino = XFS_AGINO_TO_INO(mp, agno,
						                       agino);
						bno = XFS_AGB_TO_DADDR(mp, agno,
						                       agbno);
	
						/* 
						 * Get the inode cluster buffer
						 */
						ASSERT(xfs_inode_zone != NULL);
						ip = kmem_zone_zalloc(xfs_inode_zone,
						                      KM_SLEEP);
						ip->i_ino = ino;
						ip->i_dev = mp->m_dev;
						ip->i_mount = mp;
						if (bp)
							xfs_trans_brelse(tp, bp);
						error = xfs_itobp(mp, tp, ip,
						                  &dip, &bp, bno);
						kmem_zone_free(xfs_inode_zone, ip);
						if (XFS_TEST_ERROR(error != 0,
								   mp, XFS_ERRTAG_BULKSTAT_READ_CHUNK,
								   XFS_RANDOM_BULKSTAT_READ_CHUNK)) {
							break;
						}
						clustidx = ((caddr_t)dip - 
						          bp->b_un.b_addr)/
						          mp->m_sb.sb_inodesize;
					}
				}
				/*
				 * Skip if this inode is free.
				 */
				if (XFS_INOBT_MASK(chunkidx) & irbp->ir_free)
					continue;
				/*
				 * Count used inodes as free so we can tell
				 * when the chunk is used up.
				 */
				irbp->ir_freecount++;
				ino = XFS_AGINO_TO_INO(mp, agno, agino);
				bno = XFS_AGB_TO_DADDR(mp, agno, agbno);
				if (flag == BULKSTAT_FG_QUICK) {
					dip = (xfs_dinode_t *)(bp->b_un.b_addr + 
					      (clustidx << mp->m_sb.sb_inodelog));

					if (dip->di_core.di_magic 
					            != XFS_DINODE_MAGIC
					    || !XFS_DINODE_GOOD_VERSION(
					            dip->di_core.di_version))
						continue;
				}

				/*
				 * Get the inode and fill in a single buffer.
				 * BULKSTAT_FG_QUICK uses dip to fill it in.
				 * BULKSTAT_FG_IGET uses igets.
				 * See: xfs_bulkstat_one & dm_bulkstat_one.
				 * This is also used to count inodes/blks, etc
				 * in xfs_qm_quotacheck.
				 */
				error = formatter(mp, tp, ino, ubufp, bno, dip,
					&fmterror);
				if (fmterror == BULKSTAT_RV_NOTHING)
					continue;
				if (fmterror == BULKSTAT_RV_GIVEUP) {
					ubleft = 0;
					ASSERT(error);
					rval = error;
					break;
				}
				if (ubufp)
					ubufp += statstruct_size; 
				ubleft--;
				lastino = ino;
			}
		}

		if (bp)
			xfs_trans_brelse(tp, bp);

		/*
		 * Set up for the next loop iteration.
		 */
		if (ubleft > 0) {
			if (end_of_ag) {
				agno++;
				agino = 0;
			} else
				agino = XFS_INO_TO_AGINO(mp, lastino);
		} else
			break;
	}
	/*
	 * Done, we're either out of filesystem or space to put the data.
	 */
	kmem_free(irbuf, NBPC);
	if (ubuffer)
		unuseracc(ubuffer, ubcount * statstruct_size, (B_READ|B_PHYS));
	*ubcountp = ubcount - ubleft;
	if (agno >= mp->m_sb.sb_agcount) {
		/*
		 * If we ran out of filesystem, mark lastino as off
		 * the end of the filesystem, so the next call
		 * will return immediately.
		 */
		*lastinop = (ino64_t)XFS_AGINO_TO_INO(mp, agno, 0);
		*done = 1;
	} else
		*lastinop = (ino64_t)lastino;
	vfs_unbusy(vfsp);
	return rval;
}

/*
 * Return stat information in bulk (by-inode) for the filesystem.
 * Special case for non-sequential one inode bulkstat.
 */
STATIC int				/* error status */
xfs_bulkstat_single(
	xfs_mount_t		*mp,	/* mount point for filesystem */
	ino64_t			*lastinop, /* inode to return */
	caddr_t			buffer,	/* buffer with inode stats */
	int			*done)	/* 1 if there're more stats to get */
{
	xfs_bstat_t		bstat;	/* one bulkstat result structure */
	int			count;	/* count value for bulkstat call */
	int			error;	/* return value */
	xfs_ino_t		ino;	/* filesystem inode number */
	int			res;	/* result from bs1 */

	ino = (xfs_ino_t)*lastinop;
	error = xfs_bulkstat_one(mp, NULL, ino, &bstat, 0, 0, &res);
	if (error) {
		/*
		 * Special case way failed, do it the "long" way
		 * to see if that works.
		 */
		(*lastinop)--;
		count = 1;
		if (xfs_bulkstat(mp, NULL, lastinop, &count, xfs_bulkstat_one, 
				sizeof(bstat), buffer, BULKSTAT_FG_IGET, done))
			return error;
		if (count == 0 || (xfs_ino_t)*lastinop != ino)
			return error == EFSCORRUPTED ?
				XFS_ERROR(EINVAL) : error;
		else
			return 0;
	}
	*done = 0;
	if (copyout(&bstat, buffer, sizeof(bstat)))
		return XFS_ERROR(EFAULT);
	return 0;
}

/*
 * Return inode number table for the filesystem.
 */
STATIC int				/* error status */
xfs_inumbers(
	xfs_mount_t	*mp,		/* mount point for filesystem */
	xfs_trans_t	*tp,		/* transaction pointer */
	ino64_t		*lastino,	/* last inode returned */
	int		*count,		/* size of buffer/count returned */
	caddr_t		ubuffer)	/* buffer with inode descriptions */
{
	buf_t		*agbp;
	xfs_agino_t	agino;
	xfs_agnumber_t	agno;
	int		bcount;
	xfs_inogrp_t	*buffer;
	int		bufidx;
	xfs_btree_cur_t	*cur;
	int		error;
	__int32_t	gcnt;
	xfs_inofree_t	gfree;
	xfs_agino_t	gino;
	int		i;
	xfs_ino_t	ino;
	int		left;
	int		tmp;

	ino = (xfs_ino_t)*lastino;
	agno = XFS_INO_TO_AGNO(mp, ino);
	agino = XFS_INO_TO_AGINO(mp, ino);
	left = *count;
	*count = 0;
	bcount = MIN(left, NBPP / sizeof(*buffer));
	buffer = kmem_alloc(bcount * sizeof(*buffer), KM_SLEEP);
	error = bufidx = 0;
	cur = NULL;
	agbp = NULL;
	while (left > 0 && agno < mp->m_sb.sb_agcount) {
		if (agbp == NULL) {
			mrlock(&mp->m_peraglock, MR_ACCESS, PINOD);
			error = xfs_ialloc_read_agi(mp, tp, agno, &agbp);
			mrunlock(&mp->m_peraglock);
			if (error) {
				/*
				 * If we can't read the AGI of this ag,
				 * then just skip to the next one.
				 */
				ASSERT(cur == NULL);
				agbp = NULL;
				agno++;
				agino = 0;
				continue;
			}
			cur = xfs_btree_init_cursor(mp, tp, agbp, agno,
				XFS_BTNUM_INO, (xfs_inode_t *)0, 0);
			error = xfs_inobt_lookup_ge(cur, agino, 0, 0, &tmp);
			if (error) {
				xfs_btree_del_cursor(cur, XFS_BTREE_ERROR);
				cur = NULL;
				xfs_trans_brelse(tp, agbp);
				agbp = NULL;
				/*
				 * Move up the the last inode in the current
				 * chunk.  The lookup_ge will always get
				 * us the first inode in the next chunk.
				 */
				agino += XFS_INODES_PER_CHUNK - 1;
				continue;
			}
		}
		if ((error = xfs_inobt_get_rec(cur, &gino, &gcnt, &gfree,
			&i)) ||
		    i == 0) {
			xfs_trans_brelse(tp, agbp);
			agbp = NULL;
			xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);
			cur = NULL;
			agno++;
			agino = 0;
			continue;
		}
		agino = gino + XFS_INODES_PER_CHUNK - 1;
		buffer[bufidx].xi_startino = XFS_AGINO_TO_INO(mp, agno, gino);
		buffer[bufidx].xi_alloccount = XFS_INODES_PER_CHUNK - gcnt;
		buffer[bufidx].xi_allocmask = ~gfree;
		bufidx++;
		left--;
		if (bufidx == bcount) {
			if (copyout((caddr_t)buffer, ubuffer,
					bufidx * sizeof(*buffer))) {
				error = XFS_ERROR(EFAULT);
				break;
			}
			ubuffer += bufidx * sizeof(*buffer);
			*count += bufidx;
			bufidx = 0;
		}
		if (left) {
			error = xfs_inobt_increment(cur, 0, &tmp);
			if (error) {
				xfs_btree_del_cursor(cur, XFS_BTREE_ERROR);
				cur = NULL;
				xfs_trans_brelse(tp, agbp);
				agbp = NULL;
				/*
				 * The agino value has already been bumped.
				 * Just try to skip up to it.
				 */
				agino += XFS_INODES_PER_CHUNK;
				continue;
			}
		}
	}
	if (!error) {
		if (bufidx) {
			if (copyout((caddr_t)buffer, ubuffer,
					bufidx * sizeof(*buffer)))
				error = XFS_ERROR(EFAULT);
			else
				*count += bufidx;
		}
		*lastino = XFS_AGINO_TO_INO(mp, agno, agino);
	}
	kmem_free(buffer, bcount * sizeof(*buffer));
	if (cur)
		xfs_btree_del_cursor(cur, (error ? XFS_BTREE_ERROR :
					   XFS_BTREE_NOERROR));
	if (agbp)
		xfs_trans_brelse(tp, agbp);
	return error;
}

/*
 * Convert file descriptor of a file in the filesystem to
 * a mount structure pointer.
 */
int						/* error status */
xfs_fd_to_mp(
	int		fd,			/* file descriptor to convert */
	int		wperm,			/* need write perm on device */
	xfs_mount_t	**mpp,			/* return mount pointer */
	int		rperm)			/* set if root per on file fd */
{
	dev_t		dev;
	int		error;
	vfile_t		*fp;
	vfs_t		*vfsp;
	vnode_t		*vp;
	bhv_desc_t 	*bdp;
	extern int	xfs_fstype;

	if (error = getf(fd, &fp))
		return XFS_ERROR(error);
	if (!VF_IS_VNODE(fp))
		return XFS_ERROR(EINVAL);

	vp = VF_TO_VNODE(fp);
	if (vp->v_type == VBLK || vp->v_type == VCHR) {
		if (wperm && !(fp->vf_flag & FWRITE))
			return XFS_ERROR(EPERM);
		if (vp->v_type == VCHR && dev_is_vertex(vp->v_rdev))
			dev = chartoblock(vp->v_rdev);
		else
			dev = vp->v_rdev;
		vfsp = vfs_devsearch(dev, xfs_fstype);
		if (vfsp == NULL)
			vfsp = vp->v_vfsp;
	} else {
		if (rperm && !_CAP_ABLE(CAP_DEVICE_MGT))
			return XFS_ERROR(EPERM);
		vfsp = vp->v_vfsp;
	}
	bdp = bhv_lookup_unlocked(VFS_BHVHEAD(vfsp), &xfs_vfsops);
	if (!bdp)
		return XFS_ERROR(EINVAL);
	*mpp = XFS_BHVTOM(bdp);
	return 0;
}

/*
 * Syssgi interface for bulkstat and inode-table.
 */
int					/* error status */
xfs_itable(
	int		opc,		/* op code */
	int		fd,		/* file descriptor of file in fs. */
	void		*lastip,	/* last inode number pointer */
	int		icount,		/* count of entries in buffer */
	void		*ubuffer,	/* buffer with inode descriptions */
	void		*ocount)	/* output count */
{
	int		count;		/* count of records returned */
	int		error;		/* error return value */
	ino64_t		inlast;		/* last inode number */
	xfs_mount_t	*mp;		/* mount point for filesystem */
	int		done;		/* = 1 if there are more stats to get 
					   and if bulkstat should be called
					   again. This is unused in syssgi
					   but used in dmi */

	if (error = xfs_fd_to_mp(fd, 0, &mp, 1))
		return error;
	if (XFS_FORCED_SHUTDOWN(mp))
		return XFS_ERROR(EIO);
	if (copyin((void *)lastip, &inlast, sizeof(inlast)))
		return XFS_ERROR(EFAULT);
	if ((count = icount) <= 0)
		return XFS_ERROR(EINVAL);
	switch (opc) {
	case SGI_FS_INUMBERS:
		error = xfs_inumbers(mp, NULL, &inlast, &count, ubuffer);
		break;
	case SGI_FS_BULKSTAT:
		if (count == 1 && inlast != 0) {
			inlast++;
			error = xfs_bulkstat_single(mp, &inlast, ubuffer, 
				&done);
		} else {
			error = xfs_bulkstat(mp, NULL, &inlast, &count,
				(bulkstat_one_pf)xfs_bulkstat_one, 
				sizeof(xfs_bstat_t), ubuffer, 
				BULKSTAT_FG_QUICK, &done);
		}
		break;
	case SGI_FS_BULKSTAT_SINGLE:
		error = xfs_bulkstat_single(mp, &inlast, ubuffer, &done);
		break;
	default:
		error = XFS_ERROR(EINVAL);
		break;
	}
	if (error)
		return error;
	if (ocount != NULL) {
		if (copyout(&inlast, lastip, sizeof(inlast)) ||
		    copyout(&count, ocount, sizeof(count)))
			return XFS_ERROR(EFAULT);
	}
	return 0;
}
