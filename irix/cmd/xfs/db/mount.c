#ident "$Revision: 1.4 $"

struct cred;

#include "versions.h"
#include <sys/param.h>
#include <sys/uuid.h>
#include <sys/vfs.h>
#include <stdlib.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_btree.h>
#if VERS >= V_62
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#if VERS >= V_654
#include <sys/fs/xfs_dir.h>
#include <sys/fs/xfs_dir2.h>
#include <sys/fs/xfs_dir2_sf.h>
#endif
#else
#include <sys/fs/xfs_dir.h>
#endif
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_bit.h>
#include <sys/fs/xfs_rtalloc.h>
#if VERS >= V_654
#include <sys/fs/xfs_da_btree.h>
#include <sys/fs/xfs_dir2_data.h>
#include <sys/fs/xfs_dir2_leaf.h>
#include <sys/fs/xfs_dir2_node.h>
#endif
#include "io.h"
#include "mount.h"
#include "malloc.h"

xfs_mount_t	*mp;

static void
compute_maxlevels(
	xfs_mount_t	*mp,
	int		whichfork)
{
	int		level;
	uint		maxblocks;
	uint		maxleafents;
	int		maxrootrecs;
	int		minleafrecs;
	int		minnoderecs;
	int		sz;

	maxleafents = (whichfork == XFS_DATA_FORK) ? MAXEXTNUM : MAXAEXTNUM;
	minleafrecs = mp->m_bmap_dmnr[0];
	minnoderecs = mp->m_bmap_dmnr[1];
	sz = mp->m_sb.sb_inodesize;
	maxrootrecs = (int)XFS_BTREE_BLOCK_MAXRECS(sz, xfs_bmdr, 0);
	maxblocks = (maxleafents + minleafrecs - 1) / minleafrecs;
	for (level = 1; maxblocks > 1; level++) {
		if (maxblocks <= maxrootrecs)
			maxblocks = 1;
		else
			maxblocks = (maxblocks + minnoderecs - 1) / minnoderecs;
	}
	mp->m_bm_maxlevels[whichfork] = level;
}

/* ARGSUSED */
xfs_mount_t *
dbmount(void)
{
	void		*bufp;
	int		i;
	xfs_mount_t	*mp;
	xfs_sb_t	*sbp;

	mp = xcalloc(1, sizeof(*mp));
	bufp = NULL;
	if (read_bbs(XFS_SB_DADDR, 1, &bufp, NULL))
		return NULL;
	mp->m_sb = *(xfs_sb_t *)bufp;
	xfree(bufp);
	sbp = &mp->m_sb;
	mp->m_blkbit_log = sbp->sb_blocklog + XFS_NBBYLOG;
	mp->m_blkbb_log = sbp->sb_blocklog - BBSHIFT;
	mp->m_agno_log = xfs_highbit32(sbp->sb_agcount - 1) + 1;
	mp->m_agino_log = sbp->sb_inopblog + sbp->sb_agblklog;
	mp->m_litino =
		(int)(sbp->sb_inodesize -
		      (sizeof(xfs_dinode_core_t) + sizeof(xfs_agino_t)));
	mp->m_blockmask = sbp->sb_blocksize - 1;
	mp->m_blockwsize = sbp->sb_blocksize >> XFS_WORDLOG;
	mp->m_blockwmask = mp->m_blockwsize - 1;
	for (i = 0; i < 2; i++) {
		mp->m_alloc_mxr[i] =
			(uint)XFS_BTREE_BLOCK_MAXRECS(sbp->sb_blocksize,
				xfs_alloc, i == 0);
		mp->m_alloc_mnr[i] =
			(uint)XFS_BTREE_BLOCK_MINRECS(sbp->sb_blocksize,
				xfs_alloc, i == 0);
		mp->m_bmap_dmxr[i] =
			(uint)XFS_BTREE_BLOCK_MAXRECS(sbp->sb_blocksize,
				xfs_bmbt, i == 0);
		mp->m_bmap_dmnr[i] =
			(uint)XFS_BTREE_BLOCK_MINRECS(sbp->sb_blocksize,
				xfs_bmbt, i == 0);
		mp->m_inobt_mxr[i] =
			(uint)XFS_BTREE_BLOCK_MAXRECS(sbp->sb_blocksize,
				xfs_inobt, i == 0);
		mp->m_inobt_mnr[i] =
			(uint)XFS_BTREE_BLOCK_MINRECS(sbp->sb_blocksize,
				xfs_inobt, i == 0);
	}
	compute_maxlevels(mp, XFS_DATA_FORK);
	compute_maxlevels(mp, XFS_ATTR_FORK);
	mp->m_bsize = XFS_FSB_TO_BB(mp, 1);
	mp->m_ialloc_inos = (int)MAX(XFS_INODES_PER_CHUNK, sbp->sb_inopblock);
	mp->m_ialloc_blks = mp->m_ialloc_inos >> sbp->sb_inopblog;
	if (sbp->sb_rblocks) {
		mp->m_rsumlevels = sbp->sb_rextslog + 1;
		mp->m_rsumsize =
			(uint)sizeof(xfs_suminfo_t) * mp->m_rsumlevels *
			sbp->sb_rbmblocks;
		if (sbp->sb_blocksize)
			mp->m_rsumsize =
				roundup(mp->m_rsumsize, sbp->sb_blocksize);
	}
#if VERS >= V_654
	if (XFS_SB_VERSION_HASDIRV2(sbp)) {
		mp->m_dirversion = 2;
		mp->m_dirblksize =
			1 << (sbp->sb_dirblklog + sbp->sb_blocklog);
		mp->m_dirblkfsbs = 1 << sbp->sb_dirblklog;
		mp->m_dirdatablk =
			XFS_DIR2_DB_TO_DA(mp, XFS_DIR2_DATA_FIRSTDB(mp));
		mp->m_dirleafblk =
			XFS_DIR2_DB_TO_DA(mp, XFS_DIR2_LEAF_FIRSTDB(mp));
		mp->m_dirfreeblk =
			XFS_DIR2_DB_TO_DA(mp, XFS_DIR2_FREE_FIRSTDB(mp));
	} else
#endif
	{
		mp->m_dirversion = 1;
		mp->m_dirblksize = sbp->sb_blocksize;
		mp->m_dirblkfsbs = 1;
	}
	return mp;
}

/*
 * Stubs called out of sim_init.
 */
void
binit(void)
{
}

void
vn_init(void)
{
}

/* ARGSUSED */
void
xfs_init(vfssw_t *vfswp, int fstype)
{
}
