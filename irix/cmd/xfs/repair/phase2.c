#ident "$Revision: 1.15 $"

#include "vsn.h"
#define _KERNEL 1
#include <sys/param.h>
#include <sys/buf.h>
#undef _KERNEL

#if VERS <= V_64
#include <sys/avl.h>
#else
#include "avl.h"
#endif
#include <sys/sema.h>
#include <sys/uuid.h>
#include <stdlib.h>
#include <assert.h>
#if VERS >= V_64
#include <ksys/behavior.h>
#endif
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_bit.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#include <sys/fs/xfs_dir.h>
#if VERS >= V_654
#include <sys/fs/xfs_dir2.h>
#include <sys/fs/xfs_dir2_sf.h>
#endif
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_ialloc.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_mount.h>

#include "sim.h"
#include "globals.h"
#include "agheader.h"
#include "protos.h"
#include "err_protos.h"
#include "incore.h"

void	set_mp(xfs_mount_t *mpp);
void	scan_ag(xfs_agnumber_t agno);

void
zero_log(xfs_mount_t *mp, sim_init_t *simargs)
{
	dev_t logdev;

	if (mp->m_sb.sb_logstart == 0)
		logdev = simargs->logdev;
	else
		logdev = simargs->ddev;

	if (!dev_zero(logdev, XFS_FSB_TO_DADDR(mp, mp->m_sb.sb_logstart),
			(xfs_extlen_t)XFS_FSB_TO_BB(mp, mp->m_sb.sb_logblocks)))
		do_error("could not zero log\n");

	return;
}

/*
 * ok, at this point, the fs is mounted but the root inode may be
 * trashed and the ag headers haven't been checked.  So we have
 * a valid xfs_mount_t and superblock but that's about it.  That
 * means we can use macros that use mount/sb fields in calculations
 * but I/O or btree routines that depend on space maps or inode maps
 * being correct are verboten.
 */

void
phase2(xfs_mount_t *mp, sim_init_t *simargs)
{
	xfs_agnumber_t		i;
	xfs_agblock_t		b;
	int			j;
	ino_tree_node_t		*ino_rec;

	if (no_modify)  {
		do_log(
		"Phase 2 - scan filesystem freespace and inode maps...\n");
	} else  {
		do_log("Phase 2 - zero log...\n");

		zero_log(mp, simargs);

		do_log(
		"        - scan filesystem freespace and inode maps...\n");
	}

	/* now we can start using the buffer cache routines */

	set_mp(mp);

	/*
	 * account for space used by ag headers and log if internal
	 */
	set_bmap_log(mp);
	set_bmap_fs(mp);

	bad_ino_btree = 0;

	for (i = 0; i < mp->m_sb.sb_agcount; i++)  {
		scan_ag(i);
#ifdef XR_INODE_TRACE
		print_inode_list(i);
#endif
	}

	/*
	 * make sure we know about the root inode chunk
	 */
	if ((ino_rec = find_inode_rec(0, mp->m_sb.sb_rootino)) == NULL)  {
		assert(mp->m_sb.sb_rbmino == mp->m_sb.sb_rootino + 1 &&
			mp->m_sb.sb_rsumino == mp->m_sb.sb_rootino + 2);
		do_warn("root inode chunk not found\n");

		/*
		 * mark the first 3 used, the rest are free
		 */
		ino_rec = set_inode_used_alloc(0,
				(xfs_agino_t) mp->m_sb.sb_rootino);
		set_inode_used(ino_rec, 1);
		set_inode_used(ino_rec, 2);

		for (j = 3; j < XFS_INODES_PER_CHUNK; j++)
			set_inode_free(ino_rec, j);

		/*
		 * also mark blocks
		 */
		for (b = 0; b < mp->m_ialloc_blks; b++)  {
			set_agbno_state(mp, 0,
				b + XFS_INO_TO_AGBNO(mp, mp->m_sb.sb_rootino),
				XR_E_INO);
		}
	} else  {
		do_log("        - found root inode chunk\n");

		/*
		 * blocks are marked, just make sure they're in use
		 */
		if (is_inode_free(ino_rec, 0))  {
			do_warn("root inode marked free, ");
			set_inode_used(ino_rec, 0);
			if (!no_modify)
				do_warn("correcting\n");
			else
				do_warn("would correct\n");
		}

		if (is_inode_free(ino_rec, 1))  {
			do_warn("realtime bitmap inode marked free, ");
			set_inode_used(ino_rec, 1);
			if (!no_modify)
				do_warn("correcting\n");
			else
				do_warn("would correct\n");
		}

		if (is_inode_free(ino_rec, 2))  {
			do_warn("realtime summary inode marked free, ");
			set_inode_used(ino_rec, 2);
			if (!no_modify)
				do_warn("correcting\n");
			else
				do_warn("would correct\n");
		}
	}

	return;
}
