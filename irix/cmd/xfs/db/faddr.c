#ident	"$Revision: 1.15 $"

#include "versions.h"
#include <sys/param.h>
#include <sys/uuid.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_bit.h>
#if VERS >= V_62
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#if VERS >= V_654
#include <sys/fs/xfs_dir2_sf.h>
#endif
#else
#include <sys/fs/xfs_dir.h>
#endif
#include <sys/fs/xfs_dinode.h>
#include "sim.h"
#include "data.h"
#include "type.h"
#include "faddr.h"
#include "inode.h"
#include "io.h"
#include "bit.h"
#include "bmap.h"
#include "output.h"
#include "mount.h"

void
fa_agblock(
	void		*obj,
	int		bit,
	typnm_t		next)
{
	xfs_agblock_t	bno;

	if (cur_agno == NULLAGNUMBER) {
		dbprintf("no current allocation group, cannot set new addr\n");
		return;
	}
	bno = (xfs_agblock_t)getbitval(obj, bit, bitsz(bno), BVUNSIGNED);
	if (bno == NULLAGBLOCK) {
		dbprintf("null block number, cannot set new addr\n");
		return;
	}
	assert(typtab[next].typnm == next);
	set_cur(&typtab[next], XFS_AGB_TO_DADDR(mp, cur_agno, bno), blkbb,
		DB_RING_ADD, NULL);
}

/*ARGSUSED*/
void
fa_agino(
	void		*obj,
	int		bit,
	typnm_t		next)
{
	xfs_agino_t	agino;

	if (cur_agno == NULLAGNUMBER) {
		dbprintf("no current allocation group, cannot set new addr\n");
		return;
	}
	agino = (xfs_agino_t)getbitval(obj, bit, bitsz(agino), BVUNSIGNED);
	if (agino == NULLAGINO) {
		dbprintf("null inode number, cannot set new addr\n");
		return;
	}
	set_cur_inode(XFS_AGINO_TO_INO(mp, cur_agno, agino));
}

/*ARGSUSED*/
void
fa_attrblock(
	void		*obj,
	int		bit,
	typnm_t		next)
{
	bmap_ext_t	bm;
	__uint32_t	bno;
	xfs_dfsbno_t	dfsbno;
	int		nex;

	bno = (__uint32_t)getbitval(obj, bit, bitsz(bno), BVUNSIGNED);
	if (bno == 0) {
		dbprintf("null attribute block number, cannot set new addr\n");
		return;
	}
	nex = 1;
	bmap(bno, 1, XFS_ATTR_FORK, &nex, &bm);
	if (nex == 0) {
		dbprintf("attribute block is unmapped\n");
		return;
	}
	dfsbno = bm.startblock + (bno - bm.startoff);
	assert(typtab[next].typnm == next);
	set_cur(&typtab[next], (__int64_t)XFS_FSB_TO_DADDR(mp, dfsbno), blkbb,
		DB_RING_ADD, NULL);
}

void
fa_cfileoffa(
	void		*obj,
	int		bit,
	typnm_t		next)
{
	bmap_ext_t	bm;
	xfs_dfiloff_t	bno;
	xfs_dfsbno_t	dfsbno;
	int		nex;

	bno = (xfs_dfiloff_t)getbitval(obj, bit, BMBT_STARTOFF_BITLEN,
		BVUNSIGNED);
	if (bno == NULLDFILOFF) {
		dbprintf("null block number, cannot set new addr\n");
		return;
	}
	nex = 1;
	bmap(bno, 1, XFS_ATTR_FORK, &nex, &bm);
	if (nex == 0) {
		dbprintf("file block is unmapped\n");
		return;
	}
	dfsbno = bm.startblock + (bno - bm.startoff);
	assert(typtab[next].typnm == next);
	set_cur(&typtab[next], XFS_FSB_TO_DADDR(mp, dfsbno), blkbb, DB_RING_ADD,
		NULL);
}

void
fa_cfileoffd(
	void		*obj,
	int		bit,
	typnm_t		next)
{
	bbmap_t		bbmap;
	bmap_ext_t	*bmp;
	xfs_dfiloff_t	bno;
	xfs_dfsbno_t	dfsbno;
	int		nb;
	int		nex;

	bno = (xfs_dfiloff_t)getbitval(obj, bit, BMBT_STARTOFF_BITLEN,
		BVUNSIGNED);
	if (bno == NULLDFILOFF) {
		dbprintf("null block number, cannot set new addr\n");
		return;
	}
	nex = nb = next == TYP_DIR2 ? mp->m_dirblkfsbs : 1;
	bmp = malloc(nb * sizeof(*bmp));
	bmap(bno, nb, XFS_DATA_FORK, &nex, bmp);
	if (nex == 0) {
		dbprintf("file block is unmapped\n");
		free(bmp);
		return;
	}
	dfsbno = bmp->startblock + (bno - bmp->startoff);
	assert(typtab[next].typnm == next);
	if (nex > 1)
		make_bbmap(&bbmap, nex, bmp);
	set_cur(&typtab[next], XFS_FSB_TO_DADDR(mp, dfsbno), nb * blkbb,
		DB_RING_ADD, nex > 1 ? &bbmap: NULL);
	free(bmp);
}

void
fa_cfsblock(
	void		*obj,
	int		bit,
	typnm_t		next)
{
	xfs_dfsbno_t	bno;

	bno = (xfs_dfsbno_t)getbitval(obj, bit, BMBT_STARTBLOCK_BITLEN,
		BVUNSIGNED);
	if (bno == NULLDFSBNO) {
		dbprintf("null block number, cannot set new addr\n");
		return;
	}
	assert(typtab[next].typnm == next);
	set_cur(&typtab[next], XFS_FSB_TO_DADDR(mp, bno), blkbb, DB_RING_ADD,
		NULL);
}

void
fa_dfiloffa(
	void		*obj,
	int		bit,
	typnm_t		next)
{
	bmap_ext_t	bm;
	xfs_dfiloff_t	bno;
	xfs_dfsbno_t	dfsbno;
	int		nex;

	bno = (xfs_dfiloff_t)getbitval(obj, bit, bitsz(bno), BVUNSIGNED);
	if (bno == NULLDFILOFF) {
		dbprintf("null block number, cannot set new addr\n");
		return;
	}
	nex = 1;
	bmap(bno, 1, XFS_ATTR_FORK, &nex, &bm);
	if (nex == 0) {
		dbprintf("file block is unmapped\n");
		return;
	}
	dfsbno = bm.startblock + (bno - bm.startoff);
	assert(typtab[next].typnm == next);
	set_cur(&typtab[next], XFS_FSB_TO_DADDR(mp, dfsbno), blkbb, DB_RING_ADD,
		NULL);
}

void
fa_dfiloffd(
	void		*obj,
	int		bit,
	typnm_t		next)
{
	bbmap_t		bbmap;
	bmap_ext_t	*bmp;
	xfs_dfiloff_t	bno;
	xfs_dfsbno_t	dfsbno;
	int		nb;
	int		nex;

	bno = (xfs_dfiloff_t)getbitval(obj, bit, bitsz(bno), BVUNSIGNED);
	if (bno == NULLDFILOFF) {
		dbprintf("null block number, cannot set new addr\n");
		return;
	}
	nex = nb = next == TYP_DIR2 ? mp->m_dirblkfsbs : 1;
	bmp = malloc(nb * sizeof(*bmp));
	bmap(bno, nb, XFS_DATA_FORK, &nex, bmp);
	if (nex == 0) {
		dbprintf("file block is unmapped\n");
		free(bmp);
		return;
	}
	dfsbno = bmp->startblock + (bno - bmp->startoff);
	assert(typtab[next].typnm == next);
	if (nex > 1)
		make_bbmap(&bbmap, nex, bmp);
	set_cur(&typtab[next], XFS_FSB_TO_DADDR(mp, dfsbno), nb * blkbb,
		DB_RING_ADD, nex > 1 ? &bbmap : NULL);
	free(bmp);
}

void
fa_dfsbno(
	void		*obj,
	int		bit,
	typnm_t		next)
{
	xfs_dfsbno_t	bno;

	bno = (xfs_dfsbno_t)getbitval(obj, bit, bitsz(bno), BVUNSIGNED);
	if (bno == NULLDFSBNO) {
		dbprintf("null block number, cannot set new addr\n");
		return;
	}
	assert(typtab[next].typnm == next);
	set_cur(&typtab[next], XFS_FSB_TO_DADDR(mp, bno), blkbb, DB_RING_ADD,
		NULL);
}

/*ARGSUSED*/
void
fa_dirblock(
	void		*obj,
	int		bit,
	typnm_t		next)
{
	bbmap_t		bbmap;
	bmap_ext_t	*bmp;
	__uint32_t	bno;
	xfs_dfsbno_t	dfsbno;
	int		nex;

	bno = (__uint32_t)getbitval(obj, bit, bitsz(bno), BVUNSIGNED);
	if (bno == 0) {
		dbprintf("null directory block number, cannot set new addr\n");
		return;
	}
	nex = mp->m_dirblkfsbs;
	bmp = malloc(nex * sizeof(*bmp));
	bmap(bno, mp->m_dirblkfsbs, XFS_DATA_FORK, &nex, bmp);
	if (nex == 0) {
		dbprintf("directory block is unmapped\n");
		free(bmp);
		return;
	}
	dfsbno = bmp->startblock + (bno - bmp->startoff);
	assert(typtab[next].typnm == next);
	if (nex > 1)
		make_bbmap(&bbmap, nex, bmp);
	set_cur(&typtab[next], (__int64_t)XFS_FSB_TO_DADDR(mp, dfsbno),
		(int)XFS_FSB_TO_DADDR(mp, mp->m_dirblkfsbs), DB_RING_ADD, 
		nex > 1 ? &bbmap : NULL);
	free(bmp);
}

void
fa_drfsbno(
	void		*obj,
	int		bit,
	typnm_t		next)
{
	xfs_drfsbno_t	bno;

	bno = (xfs_drfsbno_t)getbitval(obj, bit, bitsz(bno), BVUNSIGNED);
	if (bno == NULLDRFSBNO) {
		dbprintf("null block number, cannot set new addr\n");
		return;
	}
	assert(typtab[next].typnm == next);
	set_cur(&typtab[next], (__int64_t)XFS_FSB_TO_BB(mp, bno), blkbb,
		DB_RING_ADD, NULL);
}

/*ARGSUSED*/
void
fa_drtbno(
	void	*obj,
	int	bit,
	typnm_t	next)
{
	xfs_drtbno_t	bno;

	bno = (xfs_drtbno_t)getbitval(obj, bit, bitsz(bno), BVUNSIGNED);
	if (bno == NULLDRTBNO) {
		dbprintf("null block number, cannot set new addr\n");
		return;
	}
	/* need set_cur to understand rt subvolume */
}

/*ARGSUSED*/
void
fa_ino(
	void		*obj,
	int		bit,
	typnm_t		next)
{
	xfs_ino_t	ino;

	assert(next == TYP_INODE);
	ino = (xfs_ino_t)getbitval(obj, bit, bitsz(ino), BVUNSIGNED);
	if (ino == NULLFSINO) {
		dbprintf("null inode number, cannot set new addr\n");
		return;
	}
	set_cur_inode(ino);
}

#if VERS >= V_654
/*ARGSUSED*/
void
fa_ino4(
	void		*obj,
	int		bit,
	typnm_t		next)
{
	xfs_ino_t	ino;
	xfs_dir2_ino4_t	ino4;

	assert(next == TYP_INODE);
	ino = (xfs_ino_t)getbitval(obj, bit, bitsz(ino4), BVUNSIGNED);
	if (ino == NULLFSINO) {
		dbprintf("null inode number, cannot set new addr\n");
		return;
	}
	set_cur_inode(ino);
}

/*ARGSUSED*/
void
fa_ino8(
	void		*obj,
	int		bit,
	typnm_t		next)
{
	xfs_ino_t	ino;
	xfs_dir2_ino8_t	ino8;

	assert(next == TYP_INODE);
	ino = (xfs_ino_t)getbitval(obj, bit, bitsz(ino8), BVUNSIGNED);
	if (ino == NULLFSINO) {
		dbprintf("null inode number, cannot set new addr\n");
		return;
	}
	set_cur_inode(ino);
}
#endif
