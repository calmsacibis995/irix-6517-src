#ident	"$Revision: 1.9 $"

#include "vsn.h"
#define _KERNEL 1
#include <sys/param.h>
#include <sys/buf.h>
#undef _KERNEL
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#if VERS <= V_64
#include <sys/avl.h>
#else
#include "avl.h"
#endif

#include <sys/sema.h>
#include <sys/uuid.h>
#include <bstring.h>
#include <stdio.h>
#if VERS >= V_64
#include <ksys/behavior.h>
#endif
#include <sys/fs/xfs_macros.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_bit.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_ialloc.h>
#include <sys/fs/xfs_rtalloc.h>
#include <sys/fs/xfs_btree.h>
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#include <sys/fs/xfs_dir.h>
#if VERS >= V_654
#include <sys/fs/xfs_dir2.h>
#include <sys/fs/xfs_dir2_sf.h>
#endif
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_mount.h>

#include "sim.h"
#include "globals.h"
#include "agheader.h"
#include "incore.h"
#include "dinode.h"
#include "protos.h"
#include "err_protos.h"
#include "rt.h"

void
rtinit(xfs_mount_t *mp)
{
	if (mp->m_sb.sb_rblocks == 0)
		return;
	
	/*
	 * realtime init -- blockmap initialization is
	 * handled by incore_init()
	 */
	/*
	sumfile = calloc(mp->m_rsumsize, 1);
	*/
	if ((btmcompute = calloc(mp->m_sb.sb_rbmblocks *
			mp->m_sb.sb_blocksize, 1)) == NULL)
		do_error(
		"couldn't allocate memory for incore realtime bitmap.\n");

	if ((sumcompute = calloc(mp->m_rsumsize, 1)) == NULL)
		do_error(
		"couldn't allocate memory for incore realtime summary info.\n");
}

/*
 * generate the real-time bitmap and summary info based on the
 * incore realtime extent map.
 */
int
generate_rtinfo(xfs_mount_t	*mp,
		xfs_rtword_t	*words,
		xfs_suminfo_t	*sumcompute)
{
	xfs_drtbno_t	extno;
	xfs_drtbno_t	start_ext;
	int		bitsperblock;
	int		bmbno;
	xfs_rtword_t	freebit;
	xfs_rtword_t	bits;
	int		start_bmbno;
	int		i;
	int		offs;
	int		log;
	int		len;
	int		in_extent;

	assert(mp->m_rbmip == NULL);

	bitsperblock = mp->m_sb.sb_blocksize * NBBY;
	extno = 0;
	bmbno = 0;
	in_extent = 0;

	/*
	 * slower but simple, don't play around with trying to set
	 * things one word at a time, just set bit as required.
	 * Have to * track start and end (size) of each range of
	 * free extents to set the summary info properly.
	 */
	while (extno < mp->m_sb.sb_rextents)  {
		freebit = 1;
		*words = 0;
		bits = 0;
		for (i = 0; i < sizeof(xfs_rtword_t) * NBBY &&
				extno < mp->m_sb.sb_rextents; i++, extno++)  {
			if (get_rtbno_state(mp, extno) == XR_E_FREE)  {
				sb_frextents++;
				bits |= freebit;

				if (in_extent == 0) {
					start_ext = extno;
					start_bmbno = bmbno;
					in_extent = 1;
				}
			} else if (in_extent == 1) {
				len = (int) (extno - start_ext);
				log = XFS_RTBLOCKLOG(len);
				offs = XFS_SUMOFFS(mp, log, start_bmbno);
				sumcompute[offs]++;
				in_extent = 0;
			}

			freebit <<= 1;
		}
		*words = bits;
		words++;

		if (extno % bitsperblock == 0)
			bmbno++;
	}
	if (in_extent == 1) {
		len = (int) (extno - start_ext);
		log = XFS_RTBLOCKLOG(len);
		offs = XFS_SUMOFFS(mp, log, start_bmbno);
		sumcompute[offs]++;
	}

	return(0);
}

#if 0
/*
 * returns 1 if bad, 0 if good
 */
int
check_summary(xfs_mount_t *mp)
{
	xfs_drfsbno_t	bno;
	xfs_suminfo_t	*csp;
	xfs_suminfo_t	*fsp;
	int		log;
	int		error = 0;

	error = 0;
	csp = sumcompute;
	fsp = sumfile;
	for (log = 0; log < mp->m_rsumlevels; log++) {
		for (bno = 0;
		     bno < mp->m_sb.sb_rbmblocks;
		     bno++, csp++, fsp++) {
			if (*csp != *fsp) {
				do_warn(
	"rt summary mismatch, size %d block %llu, file: %d, computed: %d\n",
						log, bno, *fsp, *csp);
				error = 1;
			}
		}
	}

	return(error);
}

/*
 * examine the real-time bitmap file and compute summary
 * info off it.  Should probably be changed to compute
 * the summary information off the incore computed bitmap
 * instead of the realtime bitmap file
 */
void
process_rtbitmap(xfs_mount_t	*mp,
		xfs_dinode_t	*dino,
		blkmap_t	*blkmap)
{
	int		error;
	int		bit;
	int		bitsperblock;
	int		bmbno;
	int		end_bmbno;
	xfs_dfsbno_t	bno;
	buf_t		*bp;
	xfs_drtbno_t	extno;
	int		i;
	int		len;
	int		log;
	int		offs;
	int		prevbit;
	int		start_bmbno;
	int		start_bit;
	xfs_rtword_t	*words;

	assert(mp->m_rbmip == NULL);

	bitsperblock = mp->m_sb.sb_blocksize * NBBY;
	prevbit = 0;
	extno = 0;
	error = 0;

	end_bmbno = howmany(dino->di_core.di_size, mp->m_sb.sb_blocksize);

	for (bmbno = 0; bmbno < end_bmbno; bmbno++) {
		bno = blkmap_get(blkmap, bmbno);

		if (bno == NULLDFSBNO) {
			do_warn("can't find block %d for rtbitmap inode\n",
					bmbno);
			error = 1;
			continue;
		}
		bp = read_buf(mp->m_dev, XFS_FSB_TO_DADDR(mp, bno),
			XFS_FSB_TO_BB(mp, 1), BUF_TRYLOCK);
		if (bp == NULL || geterror(bp)) {
			do_warn("can't read block %d for rtbitmap inode\n",
					bmbno);
			error = 1;
			continue;
		}
		words = (xfs_rtword_t *)bp->b_un.b_addr;
		for (bit = 0;
		     bit < bitsperblock && extno < mp->m_sb.sb_rextents;
		     bit++, extno++) {
			if (isset(words, bit)) {
				set_rtbno_state(mp, extno, XR_E_FREE);
				sb_frextents++;
				if (prevbit == 0) {
					start_bmbno = bmbno;
					start_bit = bit;
					prevbit = 1;
				}
			} else if (prevbit == 1) {
				len = (bmbno - start_bmbno) * bitsperblock +
					(bit - start_bit);
				log = XFS_RTBLOCKLOG(len);
				offs = XFS_SUMOFFS(mp, log, start_bmbno);
				sumcompute[offs]++;
				prevbit = 0;
			}
		}
		brelse(bp);
		if (extno == mp->m_sb.sb_rextents)
			break;
	}
	if (prevbit == 1) {
		len = (bmbno - start_bmbno) * bitsperblock + (bit - start_bit);
		log = XFS_RTBLOCKLOG(len);
		offs = XFS_SUMOFFS(mp, log, start_bmbno);
		sumcompute[offs]++;
	}
}

/*
 * copy the real-time summary file data into memory
 */
void
process_rtsummary(xfs_mount_t	*mp,
		xfs_dinode_t	*dino,
		blkmap_t	*blkmap)
{
	xfs_fsblock_t	bno;
	buf_t		*bp;
	char		*bytes;
	int		sumbno;

	for (sumbno = 0; sumbno < blkmap->count; sumbno++) {
		bno = blkmap_get(blkmap, sumbno);
		if (bno == NULLDFSBNO) {
			do_warn("block %d for rtsummary inode is missing\n",
					sumbno);
			error++;
			continue;
		}
		bp = read_buf(mp->m_dev, XFS_FSB_TO_DADDR(mp, bno),
			XFS_FSB_TO_BB(mp, 1), BUF_TRYLOCK);
		if (bp == NULL || geterror(bp)) {
			do_warn("can't read block %d for rtsummary inode\n",
					sumbno);
			error++;
			continue;
		}
		bytes = bp->b_un.b_addr;
		bcopy(bytes, (char *)sumfile + sumbno * mp->m_sb.sb_blocksize,
			mp->m_sb.sb_blocksize);
		brelse(bp);
	}
}
#endif
