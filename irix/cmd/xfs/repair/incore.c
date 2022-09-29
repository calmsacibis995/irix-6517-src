#ident "$Revision: 1.26 $"

#define _SGI_MP_SOURCE

#include "vsn.h"
#define _KERNEL 1
#include <sys/param.h>
#include <sys/buf.h>
#undef _KERNEL

#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <bstring.h>
#include <assert.h>
#if VERS <= V_64
#include <sys/avl.h>
#else
#include "avl.h"
#endif
#include <sys/sema.h>
#include <sys/stat.h>
#include <sys/uuid.h>
#if VERS >= V_64
#include <ksys/behavior.h>
#endif
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_bit.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_ialloc.h>
#include <sys/fs/xfs_ialloc_btree.h>
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
#include "incore.h"
#include "agheader.h"
#include "protos.h"
#include "err_protos.h"

/*
 * push a block allocation record onto list.  assumes list
 * if set to NULL if empty.
 */
void
record_allocation(ba_rec_t *addr, ba_rec_t *list)
{
	addr->next = list;
	list = addr;

	return;
}

void
free_allocations(ba_rec_t *list)
{
	ba_rec_t *current = list;

	while (list != NULL)  {
		list = list->next;
		free(current);
		current = list;
	}

	return;
}

/* ba bmap setupstuff.  setting/getting state is in incore.h  */

void
setup_bmap(xfs_agnumber_t agno, xfs_agblock_t numblocks, xfs_drtbno_t rtblocks)
{
	int i;
	xfs_drfsbno_t size;

	if ((ba_bmap = malloc(agno*sizeof(__uint64_t *))) == NULL)  {
		do_error("couldn't allocate block map pointers\n");
		return;
	}
	for (i = 0; i < agno; i++)  {
		if ((ba_bmap[i] = memalign(sizeof(__uint64_t),
				roundup(numblocks * (NBBY/XR_BB),
				sizeof(__uint64_t)))) == NULL)  {
			do_error("couldn't allocate block map, size = %d\n",
				numblocks);
			return;
		}
		bzero(ba_bmap[i], roundup(numblocks*(NBBY/XR_BB),
						sizeof(__uint64_t)));
	}

	if (rtblocks == 0)  {
		rt_ba_bmap = NULL;
		return;
	}

	size = roundup(rtblocks * (NBBY/XR_BB), sizeof(__uint64_t));

	if ((rt_ba_bmap = memalign(sizeof(__uint64_t), size)) == NULL)  {
			do_error(
			"couldn't allocate real-time block map, size = %llu\n",
				rtblocks);
			return;
	}

	/*
	 * start all real-time as free blocks
	 */
	set_bmap_rt(rtblocks);

	return;
}

/* ARGSUSED */
void
teardown_rt_bmap(xfs_mount_t *mp)
{
	if (rt_ba_bmap != NULL)  {
		free(rt_ba_bmap);
		rt_ba_bmap = NULL;
	}

	return;
}

/* ARGSUSED */
void
teardown_ag_bmap(xfs_mount_t *mp, xfs_agnumber_t agno)
{
	assert(ba_bmap[agno] != NULL);

	free(ba_bmap[agno]);
	ba_bmap[agno] = NULL;

	return;
}

/* ARGSUSED */
void
teardown_bmap_finish(xfs_mount_t *mp)
{
	free(ba_bmap);
	ba_bmap = NULL;

	return;
}

void
teardown_bmap(xfs_mount_t *mp)
{
	xfs_agnumber_t i;

	for (i = 0; i < mp->m_sb.sb_agcount; i++)  {
		teardown_ag_bmap(mp, i);
	}

	teardown_rt_bmap(mp);
	teardown_bmap_finish(mp);

	return;
}

/*
 * block map initialization routines -- realtime, log, fs
 */
void
set_bmap_rt(xfs_drtbno_t num)
{
	xfs_drtbno_t j;
	xfs_drtbno_t size;

	/*
	 * for now, initialize all realtime blocks to be free
	 * (state == XR_E_FREE)
	 */
	size = howmany(num * (NBBY/XR_BB), sizeof(__uint64_t));

	for (j = 0; j < size; j++)
		rt_ba_bmap[j] = 0x2222222222222222LL;
	
	return;
}

void
set_bmap_log(xfs_mount_t *mp)
{
	xfs_dfsbno_t	logend, i;

	if (mp->m_sb.sb_logstart == 0)
		return;

	logend = mp->m_sb.sb_logstart + mp->m_sb.sb_logblocks;

	for (i = mp->m_sb.sb_logstart; i < logend ; i++)  {
		set_fsbno_state(mp, i, XR_E_INUSE_FS);
	}

	return;
}

void
set_bmap_fs(xfs_mount_t *mp)
{
	xfs_agnumber_t	i;
	xfs_agblock_t	j;
	xfs_agblock_t	end;

	/*
	 * AG header is 4 sectors
	 */
	end = howmany(4 * mp->m_sb.sb_sectsize, mp->m_sb.sb_blocksize);

	for (i = 0; i < mp->m_sb.sb_agcount; i++)
		for (j = 0; j < end; j++)
			set_agbno_state(mp, i, j, XR_E_INUSE_FS);

	return;
}

#if 0
void
set_bmap_fs_bt(xfs_mount_t *mp)
{
	xfs_agnumber_t	i;
	xfs_agblock_t	j;
	xfs_agblock_t	begin;
	xfs_agblock_t	end;

	begin = bnobt_root;
	end = inobt_root + 1;

	for (i = 0; i < mp->m_sb.sb_agcount; i++)  {
		/*
		 * account for btree roots
		 */
		for (j = begin; j < end; j++)
			set_agbno_state(mp, i, j, XR_E_INUSE_FS);
	}

	return;
}
#endif

void
incore_init(xfs_mount_t *mp)
{
	int agcount = mp->m_sb.sb_agcount;
	extern void incore_ino_init(xfs_mount_t *);
	extern void incore_ext_init(xfs_mount_t *);

	/* init block alloc bmap */

	setup_bmap(agcount, mp->m_sb.sb_agblocks, mp->m_sb.sb_rextents);
	incore_ino_init(mp);
	incore_ext_init(mp);

	/* initialize random globals now that we know the fs geometry */

	inodes_per_block = mp->m_sb.sb_inopblock;

	return;
}

#if defined(XR_BMAP_TRACE) || defined(XR_BMAP_DBG)
int
get_agbno_state(xfs_mount_t *mp, xfs_agnumber_t agno,
		xfs_agblock_t ag_blockno)
{
	__uint64_t *addr;

	addr = ba_bmap[(agno)] + (ag_blockno)/XR_BB_NUM;

	return((*addr >> (((ag_blockno)%XR_BB_NUM)*XR_BB)) & XR_BB_MASK);
}

void set_agbno_state(xfs_mount_t *mp, xfs_agnumber_t agno,
	xfs_agblock_t ag_blockno, int state)
{
	__uint64_t *addr;

	addr = ba_bmap[(agno)] + (ag_blockno)/XR_BB_NUM;

	*addr = (((*addr) &
	  (~((__uint64_t) XR_BB_MASK << (((ag_blockno)%XR_BB_NUM)*XR_BB)))) |
	 (((__uint64_t) (state)) << (((ag_blockno)%XR_BB_NUM)*XR_BB)));
}

int
get_fsbno_state(xfs_mount_t *mp, xfs_dfsbno_t blockno)
{
	return(get_agbno_state(mp, XFS_FSB_TO_AGNO(mp, blockno),
			XFS_FSB_TO_AGBNO(mp, blockno)));
}

void
set_fsbno_state(xfs_mount_t *mp, xfs_dfsbno_t blockno, int state)
{
	set_agbno_state(mp, XFS_FSB_TO_AGNO(mp, blockno),
		XFS_FSB_TO_AGBNO(mp, blockno), state);

	return;
}
#endif
