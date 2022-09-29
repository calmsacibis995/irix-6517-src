#ident	"$Revision: 1.36 $"

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/cmn_err.h>
#include <sys/uuid.h>
#include "xfs_macros.h"
#include "xfs_types.h"
#include "xfs_inum.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_dir.h"
#include "xfs_mount.h"
#include "xfs_ag.h"
#include "xfs_alloc_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_btree.h"
#include "xfs_error.h"
#include "xfs_alloc.h"
#include "xfs_ialloc.h"
#include "xfs_fsops.h"
#include "xfs_itable.h"
#include "xfs_rw.h"
#include "xfs_trans_space.h"
#include "xfs_rtalloc.h"

/*
 * File system operations
 */

STATIC int
xfs_fs_geometry(
	xfs_mount_t		*mp,
	xfs_fsop_geom_t		*geo,
	int			new_version)
{
	geo->blocksize = mp->m_sb.sb_blocksize;
	geo->rtextsize = mp->m_sb.sb_rextsize;
	geo->agblocks = mp->m_sb.sb_agblocks;
	geo->agcount = mp->m_sb.sb_agcount;
	geo->logblocks = mp->m_sb.sb_logblocks;
	geo->sectsize = mp->m_sb.sb_sectsize;
	geo->inodesize = mp->m_sb.sb_inodesize;
	geo->imaxpct = mp->m_sb.sb_imax_pct;
	geo->datablocks = mp->m_sb.sb_dblocks;
	geo->rtblocks = mp->m_sb.sb_rblocks;
	geo->rtextents = mp->m_sb.sb_rextents;
	geo->logstart = mp->m_sb.sb_logstart;
	geo->uuid = mp->m_sb.sb_uuid;
	if (new_version >= 2) {
		geo->sunit = mp->m_sb.sb_unit;
		geo->swidth = mp->m_sb.sb_width;
	}
	if (new_version >= 3) {
		geo->version = XFS_FSOP_GEOM_VERSION;
		geo->flags =
			(XFS_SB_VERSION_HASATTR(&mp->m_sb) ?
				XFS_FSOP_GEOM_FLAGS_ATTR : 0) |
			(XFS_SB_VERSION_HASNLINK(&mp->m_sb) ?
				XFS_FSOP_GEOM_FLAGS_NLINK : 0) |
			(XFS_SB_VERSION_HASQUOTA(&mp->m_sb) ?
				XFS_FSOP_GEOM_FLAGS_QUOTA : 0) |
			(XFS_SB_VERSION_HASALIGN(&mp->m_sb) ?
				XFS_FSOP_GEOM_FLAGS_IALIGN : 0) |
			(XFS_SB_VERSION_HASDALIGN(&mp->m_sb) ?
				XFS_FSOP_GEOM_FLAGS_DALIGN : 0) |
			(XFS_SB_VERSION_HASSHARED(&mp->m_sb) ?
				XFS_FSOP_GEOM_FLAGS_SHARED : 0) |
			(XFS_SB_VERSION_HASEXTFLGBIT(&mp->m_sb) ?
				XFS_FSOP_GEOM_FLAGS_EXTFLG : 0) |
			(XFS_SB_VERSION_HASDIRV2(&mp->m_sb) ?
				XFS_FSOP_GEOM_FLAGS_DIRV2 : 0);
		geo->logsectsize = mp->m_sb.sb_sectsize;	/* XXX */
		geo->rtsectsize = mp->m_sb.sb_sectsize;		/* XXX */
		geo->dirblocksize = mp->m_dirblksize;
	}
	return 0;
}

STATIC int
xfs_growfs_data(
	xfs_mount_t		*mp,		/* mount point for filesystem */
	xfs_growfs_data_t	*in)		/* growfs data input struct */
{
	xfs_agf_t		*agf;
	xfs_agi_t		*agi;
	xfs_agnumber_t		agno;
	xfs_extlen_t		agsize;
	xfs_alloc_rec_t		*arec;
	xfs_btree_sblock_t	*block;
	buf_t			*bp;
	int			bsize;
	int			bucket;
	int			dpct;
	int			error;
	xfs_agnumber_t		nagcount;
	xfs_rfsblock_t		nb;
	xfs_rfsblock_t		new;
	xfs_rfsblock_t		nfree;
	xfs_agnumber_t		oagcount;
	int			pct;
	xfs_sb_t		*sbp;
	int			sectbb;
	xfs_trans_t		*tp;

	nb = in->newblocks;
	pct = in->imaxpct;
	if (nb < mp->m_sb.sb_dblocks || pct < 0 || pct > 100)
		return XFS_ERROR(EINVAL);
	dpct = pct - mp->m_sb.sb_imax_pct;
	error = xfs_read_buf(mp, mp->m_ddev_targp, XFS_FSB_TO_BB(mp, nb) - 1, 1,
		0, &bp);
	if (error)
		return error;
	ASSERT(bp);
	brelse(bp);

	nagcount = (nb / mp->m_sb.sb_agblocks) +
		   ((nb % mp->m_sb.sb_agblocks) != 0);
	if (nb % mp->m_sb.sb_agblocks &&
	    nb % mp->m_sb.sb_agblocks < XFS_MIN_AG_BLOCKS) {
		nagcount--;
		nb = nagcount * mp->m_sb.sb_agblocks;
		if (nb < mp->m_sb.sb_dblocks)
			return XFS_ERROR(EINVAL);
	}
	new = nb - mp->m_sb.sb_dblocks;
	oagcount = mp->m_sb.sb_agcount;
	if (nagcount > oagcount) {
		mrlock(&mp->m_peraglock, MR_UPDATE, PINOD);
		mp->m_perag =
			kmem_realloc(mp->m_perag,
				sizeof(xfs_perag_t) * nagcount, KM_SLEEP);
		bzero(&mp->m_perag[oagcount],
			(nagcount - oagcount) * sizeof(xfs_perag_t));
		mrunlock(&mp->m_peraglock);
	}
	tp = xfs_trans_alloc(mp, XFS_TRANS_GROWFS);
	if (error = xfs_trans_reserve(tp, XFS_GROWFS_SPACE_RES(mp),
			XFS_GROWDATA_LOG_RES(mp), 0, 0, 0)) {
		xfs_trans_cancel(tp, 0);
		return error;
	}
	/* new ag's */
	sectbb = BTOBB(mp->m_sb.sb_sectsize);
	bsize = mp->m_sb.sb_blocksize;
	nfree = 0;
	for (agno = nagcount - 1; agno >= oagcount; agno--, new -= agsize) {
		/*
		 * AG freelist header block
		 */
		bp = get_buf(mp->m_dev, XFS_AG_DADDR(mp, agno, XFS_AGF_DADDR),
			     sectbb, 0);
		bp->b_target =  mp->m_ddev_targp;
		agf = XFS_BUF_TO_AGF(bp);
		bzero(agf, mp->m_sb.sb_sectsize);
		agf->agf_magicnum = XFS_AGF_MAGIC;
		agf->agf_versionnum = XFS_AGF_VERSION;
		agf->agf_seqno = agno;
		if (agno == nagcount - 1)
			agsize =
				nb -
				(agno * (xfs_rfsblock_t)mp->m_sb.sb_agblocks);
		else
			agsize = mp->m_sb.sb_agblocks;
		agf->agf_length = agsize;
		agf->agf_roots[XFS_BTNUM_BNOi] = XFS_BNO_BLOCK(mp);
		agf->agf_roots[XFS_BTNUM_CNTi] = XFS_CNT_BLOCK(mp);
		agf->agf_levels[XFS_BTNUM_BNOi] = 1;
		agf->agf_levels[XFS_BTNUM_CNTi] = 1;
		agf->agf_flfirst = 0;
		agf->agf_fllast = XFS_AGFL_SIZE - 1;
		agf->agf_flcount = 0;
		agf->agf_freeblks = agf->agf_length - XFS_PREALLOC_BLOCKS(mp);
		agf->agf_longest = agf->agf_freeblks;
		error = xfs_bwrite(mp, bp);
		if (error) {
			goto error0;
		}
		/*
		 * AG inode header block
		 */
		bp = get_buf(mp->m_dev, XFS_AG_DADDR(mp, agno, XFS_AGI_DADDR),
			sectbb, 0);
		bp->b_target =  mp->m_ddev_targp;
		agi = XFS_BUF_TO_AGI(bp);
		bzero(agi, mp->m_sb.sb_sectsize);
		agi->agi_magicnum = XFS_AGI_MAGIC;
		agi->agi_versionnum = XFS_AGI_VERSION;
		agi->agi_seqno = agno;
		agi->agi_length = agsize;
		agi->agi_count = 0;
		agi->agi_root = XFS_IBT_BLOCK(mp);
		agi->agi_level = 1;
		agi->agi_freecount = 0;
		agi->agi_newino = NULLAGINO;
		agi->agi_dirino = NULLAGINO;
		for (bucket = 0; bucket < XFS_AGI_UNLINKED_BUCKETS; bucket++)
			agi->agi_unlinked[bucket] = NULLAGINO;
		error = xfs_bwrite(mp, bp);
		if (error) {
			goto error0;
		}
		/*
		 * BNO btree root block
		 */
		bp = get_buf(mp->m_dev,
			XFS_AGB_TO_DADDR(mp, agno, XFS_BNO_BLOCK(mp)),
			BTOBB(bsize), 0);
		bp->b_target =  mp->m_ddev_targp;
		block = XFS_BUF_TO_SBLOCK(bp);
		bzero(block, bsize);
		block->bb_magic = XFS_ABTB_MAGIC;
		block->bb_level = 0;
		block->bb_numrecs = 1;
		block->bb_leftsib = block->bb_rightsib = NULLAGBLOCK;
		arec = XFS_BTREE_REC_ADDR(bsize, xfs_alloc, block, 1,
			mp->m_alloc_mxr[0]);
		arec->ar_startblock = XFS_PREALLOC_BLOCKS(mp);
		arec->ar_blockcount = agsize - arec->ar_startblock;
		error = xfs_bwrite(mp, bp);
		if (error) {
			goto error0;
		}
		/*
		 * CNT btree root block
		 */
		bp = get_buf(mp->m_dev,
			XFS_AGB_TO_DADDR(mp, agno, XFS_CNT_BLOCK(mp)),
			BTOBB(bsize), 0);
		bp->b_target =  mp->m_ddev_targp;
		block = XFS_BUF_TO_SBLOCK(bp);
		bzero(block, bsize);
		block->bb_magic = XFS_ABTC_MAGIC;
		block->bb_level = 0;
		block->bb_numrecs = 1;
		block->bb_leftsib = block->bb_rightsib = NULLAGBLOCK;
		arec = XFS_BTREE_REC_ADDR(bsize, xfs_alloc, block, 1,
			mp->m_alloc_mxr[0]);
		arec->ar_startblock = XFS_PREALLOC_BLOCKS(mp);
		arec->ar_blockcount = agsize - arec->ar_startblock;
		nfree += arec->ar_blockcount;
		error = xfs_bwrite(mp, bp);
		if (error) {
			goto error0;
		}
		/*
		 * INO btree root block
		 */
		bp = get_buf(mp->m_dev,
			XFS_AGB_TO_DADDR(mp, agno, XFS_IBT_BLOCK(mp)),
			BTOBB(bsize), 0);
		bp->b_target = mp->m_ddev_targp;
		block = XFS_BUF_TO_SBLOCK(bp);
		bzero(block, bsize);
		block->bb_magic = XFS_IBT_MAGIC;
		block->bb_level = 0;
		block->bb_numrecs = 0;
		block->bb_leftsib = block->bb_rightsib = NULLAGBLOCK;
		error = xfs_bwrite(mp, bp);
		if (error) {
			goto error0;
		}		
	}
	xfs_trans_agblocks_delta(tp, nfree);
	/*
	 * There are new blocks in the old last a.g.
	 */
	if (new) {
		/*
		 * Change the agi length.
		 */
		error = xfs_ialloc_read_agi(mp, tp, agno, &bp);
		if (error) {
			goto error0;
		}
		ASSERT(bp);
		agi = XFS_BUF_TO_AGI(bp);
		agi->agi_length += new;
		ASSERT(agno < mp->m_sb.sb_agcount - 1 ||
		       agi->agi_length == mp->m_sb.sb_agblocks);
		xfs_ialloc_log_agi(tp, bp, XFS_AGI_LENGTH);
		/*
		 * Change agf length.
		 */
		error = xfs_alloc_read_agf(mp, tp, agno, 0, &bp);
		if (error) {
			goto error0;
		}
		ASSERT(bp);
		agf = XFS_BUF_TO_AGF(bp);
		agf->agf_length += new;
		ASSERT(agf->agf_length == agi->agi_length);
		/*
		 * Free the new space.
		 */
		error = xfs_free_extent(tp,
			XFS_AGB_TO_FSB(mp, agno, agf->agf_length - new), new);
		if (error) {
			goto error0;
		}
	}
	if (nagcount > oagcount)
		xfs_trans_mod_sb(tp, XFS_TRANS_SB_AGCOUNT, nagcount - oagcount);
	if (nb > mp->m_sb.sb_dblocks)
		xfs_trans_mod_sb(tp, XFS_TRANS_SB_DBLOCKS,
				 nb - mp->m_sb.sb_dblocks);
	if (nfree)
		xfs_trans_mod_sb(tp, XFS_TRANS_SB_FDBLOCKS, nfree);
	if (dpct)
		xfs_trans_mod_sb(tp, XFS_TRANS_SB_IMAXPCT, dpct);
	error = xfs_trans_commit(tp, 0, NULL);
	if (error) {
		return error;
	}
	if (mp->m_sb.sb_imax_pct)
		mp->m_maxicount =
			((mp->m_sb.sb_dblocks * mp->m_sb.sb_imax_pct) / 100) <<
			mp->m_sb.sb_inopblog;
	else
		mp->m_maxicount = 0;
	for (agno = 1; agno < nagcount; agno++) {
	        error = xfs_read_buf(mp, mp->m_ddev_targp,
				  XFS_AGB_TO_DADDR(mp, agno, XFS_SB_BLOCK(mp)),
				  BTOBB(bsize), 0, &bp);
		if (error) {
			xfs_fs_cmn_err(CE_WARN, mp,
			"error %d reading secondary superblock for ag %d\n",
				error, agno);
			break;
		}
		sbp = XFS_BUF_TO_SBP(bp);
		*sbp = mp->m_sb;
		/*
		 * If we get an error writing out the alternate superblocks,
		 * just issue a warning and continue.  The real work is
		 * already done and committed.
		 */
		if (!(error = xfs_bwrite(mp, bp))) {
			continue;
		} else {
			xfs_fs_cmn_err(CE_WARN, mp,
		"write error %d updating secondary superblock for ag %d\n",
				error, agno);
			break; /* no point in continuing */
		}
	}
	return 0;

 error0:
	xfs_trans_cancel(tp, XFS_TRANS_ABORT);
	return error;
}

STATIC int
xfs_growfs_log(
	xfs_mount_t		*mp,	/* mount point for filesystem */
	xfs_growfs_log_t	*in)	/* growfs log input struct */
{
	xfs_extlen_t		nb;

	nb = in->newblocks;
	if (nb < XFS_MIN_LOG_BLOCKS || nb < XFS_B_TO_FSB(mp, XFS_MIN_LOG_BYTES))
		return XFS_ERROR(EINVAL);
	if (nb == mp->m_sb.sb_logblocks &&
	    in->isint == (mp->m_sb.sb_logstart != 0))
		return XFS_ERROR(EINVAL);
	/*
	 * Moving the log is hard, need new interfaces to sync
	 * the log first, hold off all activity while moving it.
	 * Can have shorter or longer log in the same space,
	 * or transform internal to external log or vice versa.
	 */
	return XFS_ERROR(ENOSYS);
}

STATIC int
xfs_fs_counts(
	xfs_mount_t		*mp,
	xfs_fsop_counts_t	*cnt)
{
	int			s;

	s = XFS_SB_LOCK(mp);
	cnt->freedata = mp->m_sb.sb_fdblocks;
	cnt->freertx = mp->m_sb.sb_frextents;
	cnt->freeino = mp->m_sb.sb_ifree;
	cnt->allocino = mp->m_sb.sb_icount;
	XFS_SB_UNLOCK(mp, s);
	return 0;
}


/*
 * xfs_reserve_blocks is called to set m_resblks
 * in the in-core mount table. The number of unused reserved blocks
 * is kept in m_resbls_avail.
 *
 * Reserve the requested number of blocks if available. Otherwise return
 * as many as possible to satisfy the request. The actual number
 * reserved are returned in outval
 *
 * A null inval pointer indicates that only the current reserved blocks
 * available  should  be returned no settings are changed.
 */

STATIC int
xfs_reserve_blocks(
	xfs_mount_t *mp, 
	__uint64_t *inval, 
	xfs_fsops_getblks_t *outval)
{
	long long lcounter, delta;
	__uint64_t request;
	int s;

	/* If inval is null, report current values and return */

	if (inval == (__uint64_t *)NULL) {
		outval->resblks = mp->m_resblks;
		outval->resblks_avail = mp->m_resblks_avail;
		return(0);
	}
	
	request = *inval;
	s = XFS_SB_LOCK(mp);

	/*
	 * If our previous reservation was larger than the current value, 
	 * then move any unused blocks back to the free pool.
	 */

	if (mp->m_resblks > request) { 
		lcounter = mp->m_resblks_avail - request;
		if (lcounter  > 0) {		/* release unused blocks */
			mp->m_sb.sb_fdblocks += lcounter;
			mp->m_resblks_avail -= lcounter;
		}
		mp->m_resblks = request;
	} else {
		delta = request - mp->m_resblks;
		lcounter = mp->m_sb.sb_fdblocks;
		lcounter -= delta;
		if (lcounter < 0) {
			/* We can't satisfy the request, just get what we can */
			mp->m_resblks += mp->m_sb.sb_fdblocks;
			mp->m_resblks_avail += mp->m_sb.sb_fdblocks;
			mp->m_sb.sb_fdblocks = 0;
		} else {
			mp->m_sb.sb_fdblocks = lcounter;
			mp->m_resblks = request;
			mp->m_resblks_avail += delta;
		}
	}

	outval->resblks = mp->m_resblks;
	outval->resblks_avail = mp->m_resblks_avail;
	XFS_SB_UNLOCK(mp, s);
	return(0);
}

int					/* error status */
xfs_fsoperations(
	int		fd,		/* file descriptor for fs */
	int		opcode,		/* operation code */
	void		*in,		/* input structure */
	void		*out)		/* output structure */
{
	int		error;
	void		*inb;
	xfs_mount_t	*mp;
	void		*outb;
	static const short	cisize[XFS_FSOPS_COUNT] =
	{
		0,				/* XFS_FS_GEOMETRY_V1 */
		sizeof(xfs_growfs_data_t),	/* XFS_GROWFS_DATA */
		sizeof(xfs_growfs_log_t),	/* XFS_GROWFS_LOG */
		sizeof(xfs_growfs_rt_t),	/* XFS_GROWFS_RT */
		0,				/* XFS_FS_COUNTS */
		sizeof(__uint64_t),		/* XFS_SET_RESBLKS */
		0,				/* XFS_GET_RESBLKS */
		0,				/* XFS_FS_GEOMETRY_V2 */
		0,				/* XFS_FS_GEOMETRY */
	};
	static const short	cosize[XFS_FSOPS_COUNT] =
	{
		sizeof(xfs_fsop_geom_v1_t),	/* XFS_FS_GEOMETRY_V1 */
		0,				/* XFS_GROWFS_DATA */
		0,				/* XFS_GROWFS_LOG */
		0,				/* XFS_GROWFS_RT */
		sizeof(xfs_fsop_counts_t),	/* XFS_FS_COUNTS */
		sizeof(xfs_fsops_getblks_t),	/* XFS_SET_RESBLKS */
		sizeof(xfs_fsops_getblks_t),	/* XFS_GET_RESBLKS */
		sizeof(xfs_fsop_geom_v2_t),	/* XFS_FS_GEOMETRY_V2 */
		sizeof(xfs_fsop_geom_t),	/* XFS_FS_GEOMETRY */
	};
	static const short	wperm[XFS_FSOPS_COUNT] =
	{
		0,	/* XFS_FS_GEOMETRY_V1 */
		1,	/* XFS_GROWFS_DATA */
		1,	/* XFS_GROWFS_LOG */
		1,	/* XFS_GROWFS_RT */
		0,	/* XFS_FS_COUNTS */
		1,	/* XFS_SET_RESBLKS */
		0,  	/* XFS_GET_RESBLKS */
		0,	/* XFS_FS_GEOMETRY_V2 */
		0,	/* XFS_FS_GEOMETRY */
	};


	if (opcode < 0 || opcode >= XFS_FSOPS_COUNT)
		return XFS_ERROR(EINVAL);
	if (error = xfs_fd_to_mp(fd, wperm[opcode], &mp, wperm[opcode]))
		return error;
	if (XFS_FORCED_SHUTDOWN(mp))
		return (EIO);

	if (cisize[opcode]) {
		inb = kmem_alloc(cisize[opcode], KM_SLEEP);
		if (copyin(in, inb, cisize[opcode])) {
			kmem_free(inb, cisize[opcode]);
			return XFS_ERROR(EFAULT);
		}
	} else
		inb = NULL;
	if (cosize[opcode])
		outb = kmem_alloc(cosize[opcode], KM_SLEEP);
	else
		outb = NULL;
	switch (opcode)
	{
	case XFS_FS_GEOMETRY_V1:
		error = xfs_fs_geometry(mp, (xfs_fsop_geom_t *)outb, 1);
		break;
	case XFS_FS_GEOMETRY_V2:
		error = xfs_fs_geometry(mp, (xfs_fsop_geom_t *)outb, 2);
		break;
	case XFS_FS_GEOMETRY:
		error = xfs_fs_geometry(mp, (xfs_fsop_geom_t *)outb, 3);
		break;
	case XFS_GROWFS_DATA:
		if (!cpsema(&mp->m_growlock))
			return XFS_ERROR(EWOULDBLOCK);
		error = xfs_growfs_data(mp, (xfs_growfs_data_t *)inb);
		vsema(&mp->m_growlock);
		break;
	case XFS_GROWFS_LOG:
		if (!cpsema(&mp->m_growlock))
			return XFS_ERROR(EWOULDBLOCK);
		error = xfs_growfs_log(mp, (xfs_growfs_log_t *)inb);
		vsema(&mp->m_growlock);
		break;
	case XFS_GROWFS_RT:
		if (!cpsema(&mp->m_growlock))
			return XFS_ERROR(EWOULDBLOCK);
		error = xfs_growfs_rt(mp, (xfs_growfs_rt_t *)inb);
		vsema(&mp->m_growlock);
		break;
	case XFS_FS_COUNTS:
		error = xfs_fs_counts(mp, (xfs_fsop_counts_t *)outb);
		break;
	case XFS_SET_RESBLKS:
		error = xfs_reserve_blocks(mp, (__uint64_t *)inb, 
					(xfs_fsops_getblks_t *)outb);
		break;
	case XFS_GET_RESBLKS:
		error = xfs_reserve_blocks(mp, (__uint64_t *)NULL, 
					(xfs_fsops_getblks_t *)outb);
		break;
	default:
		error = XFS_ERROR(EINVAL);
		break;
	}
	if (inb)
		kmem_free(inb, cisize[opcode]);
	if (!error && outb) {
		if (copyout(outb, out, cosize[opcode]))
			error = XFS_ERROR(EFAULT);
		kmem_free(outb, cosize[opcode]);
	}
	return error;
}
