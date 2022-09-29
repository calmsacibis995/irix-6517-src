#ident "$Revision: 1.14 $"

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
#include <sys/cred.h>
#include <sys/uuid.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <bstring.h>
#include <errno.h>
#if VERS >= V_64
#include <ksys/behavior.h>
#endif
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_bit.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_dir.h>
#include <sys/fs/xfs_mount.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_ialloc.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_btree.h>
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#if VERS >= V_654
#include <sys/fs/xfs_dir2.h>
#include <sys/fs/xfs_dir2_sf.h>
#endif
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_inode_item.h>
#include <sys/fs/xfs_inode.h>
#include <sys/fs/xfs_bmap.h>

#include "sim.h"
#include "globals.h"
#include "agheader.h"
#include "incore.h"
#include "protos.h"
#include "err_protos.h"
#include "dinode.h"
#include "versions.h"

extern int xfsr_trans_commit(xfs_trans_t *tp, uint flags);

void
set_nlinks(xfs_dinode_core_t	*dinoc,
		xfs_ino_t	ino,
		__uint32_t	nrefs,
		int		*dirty)
{
	if (!no_modify)  {
		if (dinoc->di_nlink != nrefs)  {
			*dirty = 1;
			do_warn("resetting inode %llu nlinks from %d to %d\n",
					ino, dinoc->di_nlink, nrefs);

			if (nrefs > XFS_MAXLINK_1)  {
				assert(fs_inode_nlink);
				do_warn(
"nlinks %d will overflow v1 ino, ino %llu will be converted to version 2\n",
					nrefs, ino);

			}
			dinoc->di_nlink = nrefs;
		}
	} else  {
		if (dinoc->di_nlink != nrefs)
			do_warn(
			"would have reset inode %llu nlinks from %d to %d\n",
				ino, dinoc->di_nlink, nrefs);
	}

	return;
}

void
phase7(xfs_mount_t *mp)
{
	ino_tree_node_t		*irec;
	xfs_inode_t		*ip;
	xfs_trans_t		*tp;
	int			i;
	int			j;
	int			error;
	int			dirty;
	xfs_ino_t		ino;
	__uint32_t		nrefs;

	if (!no_modify)
		printf("Phase 7 - verify and correct link counts...\n");
	else
		printf("Phase 7 - verify link counts...\n");

	tp = xfs_trans_alloc(mp, XFS_TRANS_REMOVE);

	error = xfs_trans_reserve(tp, (no_modify ? 0 : 10),
		XFS_REMOVE_LOG_RES(mp), 0, XFS_TRANS_PERM_LOG_RES,
		XFS_REMOVE_LOG_COUNT);

	assert(error == 0);

	/*
	 * for each ag, look at each inode 1 at a time using the
	 * sim code.  if the number of links is bad, reset it,
	 * log the inode core, commit the transaction, and
	 * allocate a new transaction
	 */
	for (i = 0; i < glob_agcount; i++)  {
		irec = findfirst_inode_rec(i);

		while (irec != NULL)  {
			for (j = 0; j < XFS_INODES_PER_CHUNK; j++)  {
				assert(is_inode_confirmed(irec, j));

				if (is_inode_free(irec, j))
					continue;

				assert(no_modify || is_inode_reached(irec, j));
				assert(no_modify ||
						is_inode_referenced(irec, j));

				nrefs = num_inode_references(irec, j);

				ino = XFS_AGINO_TO_INO(mp, i,
					irec->ino_startnum + j);

				error = xfs_trans_iget(mp, tp, ino,
							XFS_ILOCK_EXCL, &ip);

				if (error)  {
					if (!no_modify)
						do_error(
					"couldn't map inode %llu, err = %d\n",
							ino, error);
					else  {
						do_warn(
	"couldn't map inode %llu, err = %d, can't compare link counts\n",
							ino, error);
						continue;
					}
				}

				dirty = 0;

				/*
				 * compare and set links for all inodes
				 * but the lost+found inode.  we keep
				 * that correct as we go.
				 */
				if (ino != orphanage_ino)
					set_nlinks(&ip->i_d, ino, nrefs,
							&dirty);
				
				if (!dirty)  {
					xfs_trans_iput(tp, ip, XFS_ILOCK_EXCL);
				} else  {
					xfs_trans_log_inode(tp, ip,
							XFS_ILOG_CORE);
					/*
					 * no need to do a bmap finish since
					 * we're not allocating anything
					 */
					assert(error == 0);
					error = xfsr_trans_commit(tp,
						XFS_TRANS_RELEASE_LOG_RES|
						XFS_TRANS_SYNC);

					assert(error == 0);

					tp = xfs_trans_alloc(mp,
							XFS_TRANS_REMOVE);

					error = xfs_trans_reserve(tp,
						(no_modify ? 0 : 10),
						XFS_REMOVE_LOG_RES(mp),
						0, XFS_TRANS_PERM_LOG_RES,
						XFS_REMOVE_LOG_COUNT);
					assert(error == 0);
				}
			}
			irec = next_ino_rec(irec);
		}
	}

	/* 
	 * always have one unfinished transaction coming out
	 * of the loop.  cancel it.
	 */
	xfs_trans_cancel(tp, XFS_TRANS_RELEASE_LOG_RES);

	return;
}
