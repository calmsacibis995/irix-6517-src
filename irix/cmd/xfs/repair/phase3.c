#ident "$Revision: 1.22 $"

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
#include <sys/uuid.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#if VERS >= V_64
#include <ksys/behavior.h>
#endif
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_dir.h>
#include <sys/fs/xfs_mount.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_btree.h>
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#if VERS >= V_654
#include <sys/fs/xfs_dir2.h>
#include <sys/fs/xfs_dir2_sf.h>
#endif
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_ialloc.h>

#include "sim.h"
#include "globals.h"
#include "agheader.h"
#include "incore.h"
#include "protos.h"
#include "err_protos.h"
#include "dinode.h"

/*
 * walks an unlinked list, returns 1 on an error (bogus pointer) or
 * I/O error
 */
int
walk_unlinked_list(xfs_mount_t *mp, xfs_agnumber_t agno, xfs_agino_t start_ino)
{
	buf_t *bp;
	xfs_dinode_t *dip;
	xfs_agino_t current_ino = start_ino;
	xfs_agblock_t agbno;
	int state;

	while (current_ino != NULLAGINO)  {
		if (!verify_aginum(mp, agno, current_ino))
			return(1);
		if ((bp = get_agino_buf(mp, agno, current_ino, &dip)) == NULL)
			return(1);
		/*
		 * if this looks like a decent inode, then continue
		 * following the unlinked pointers.  If not, bail.
		 */
		if (verify_dinode(mp, dip, agno, current_ino) == 0)  {
			/*
			 * check if the unlinked list points to an unknown
			 * inode.  if so, put it on the uncertain inode list
			 * and set block map appropriately.
			 */
			if (find_inode_rec(agno, current_ino) == NULL)  {
				add_aginode_uncertain(agno, current_ino, 1);
				agbno = XFS_AGINO_TO_AGBNO(mp, current_ino);

				switch (state = get_agbno_state(mp,
							agno, agbno))  {
				case XR_E_UNKNOWN:
				case XR_E_FREE:
				case XR_E_FREE1:
					set_agbno_state(mp, agno, agbno,
						XR_E_INO);
					break;
				case XR_E_BAD_STATE:
					do_error(
						"bad state in block map %d\n",
						state);
					abort();
					break;
				default:
					/*
					 * the block looks like inodes
					 * so be conservative and try
					 * to scavenge what's in there.
					 * if what's there is completely
					 * bogus, it'll show up later
					 * and the inode will be trashed
					 * anyway, hopefully without
					 * losing too much other data
					 */
					set_agbno_state(mp, agno, agbno,
						XR_E_INO);
					break;
				}
			}
			current_ino = dip->di_next_unlinked;
		} else  {
			current_ino = NULLAGINO;;
		}
		brelse(bp);
	}

	return(0);
}

void
process_agi_unlinked(xfs_mount_t *mp, xfs_agnumber_t agno)
{
	xfs_agnumber_t i;
	buf_t *bp;
	xfs_agi_t *agip;
	int err = 0;
	int agi_dirty = 0;

	bp = read_buf(mp->m_dev,
		(daddr_t) XFS_AG_DADDR(mp, agno, XFS_AGI_DADDR),
		mp->m_sb.sb_sectsize/BBSIZE, BUF_TRYLOCK);

	if (bp == NULL || geterror(bp)) {
		do_error("cannot read agi block %lld for ag %u\n",
			(daddr_t) XFS_AG_DADDR(mp, agno, XFS_AGI_DADDR), agno);
		exit(1);
	}

	agip = XFS_BUF_TO_AGI(bp);

	assert(no_modify || agip->agi_seqno == agno);

	for (i = 0; i < XFS_AGI_UNLINKED_BUCKETS; i++)  {
		if (agip->agi_unlinked[i] != NULLAGINO)  {
			err += walk_unlinked_list(mp, agno,
						agip->agi_unlinked[i]);
			/*
			 * clear the list
			 */
			if (!no_modify)  {
				agip->agi_unlinked[i] = NULLAGINO;
				agi_dirty = 1;
			}
		}
	}

	if (err)
		do_warn("error following ag %d unlinked list\n", agno);

	assert(agi_dirty == 0 || agi_dirty && !no_modify);

	if (agi_dirty && !no_modify)
		bwrite(bp);
	else
		brelse(bp);
}

void
phase3(xfs_mount_t *mp)
{
	int i, j;

	printf("Phase 3 - for each AG...\n");
	if (!no_modify)
		printf("        - scan and clear agi unlinked lists...\n");
	else
		printf(
		"        - scan (but don't clear) agi unlinked lists...\n");

	/*
	 * first, let's look at the possibly bogus inodes
	 */
	for (i = 0; i < mp->m_sb.sb_agcount; i++)  {
		/*
		 * walk unlinked list to add more potential inodes to list
		 */
		process_agi_unlinked(mp, i);
		check_uncertain_aginodes(mp, i);
	}

	/* ok, now that the tree's ok, let's take a good look */

	printf(
	    "        - process known inodes and perform inode discovery...\n");

	for (i = 0; i < mp->m_sb.sb_agcount; i++)  {
		do_log("        - agno = %d\n", i);
		/*
		 * turn on directory processing (inode discovery) and 
		 * attribute processing (extra_attr_check)
		 */
		process_aginodes(mp, i, 1, 0, 1);
	}

	/*
	 * process newly discovered inode chunks
	 */
	printf("        - process newly discovered inodes...\n");
	do  {
		/*
		 * have to loop until no ag has any uncertain
		 * inodes
		 */
		j = 0;
		for (i = 0; i < mp->m_sb.sb_agcount; i++)  {
			j += process_uncertain_aginodes(mp, i);
#ifdef XR_INODE_TRACE
			fprintf(stderr,
				"\t\t phase 3 - process_uncertain_inodes returns %d\n", j);
#endif
		}
	} while (j != 0);
}

