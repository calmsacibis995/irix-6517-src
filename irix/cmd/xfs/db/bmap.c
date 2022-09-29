#ident "$Revision: 1.18 $"

#include "versions.h"
#include <sys/param.h>
#include <sys/uuid.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_btree.h>
#if VERS >= V_62
#include <sys/fs/xfs_dir_sf.h>
#if VERS >= V_654
#include <sys/fs/xfs_dir2_sf.h>
#endif
#include <sys/fs/xfs_attr_sf.h>
#else
#include <sys/fs/xfs_dir.h>
#endif
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_bit.h>
#include "sim.h"
#include "command.h"
#include "data.h"
#include "type.h"
#include "bmap.h"
#include "io.h"
#include "inode.h"
#include "output.h"
#include "mount.h"

static int		bmap_f(int argc, char **argv);
static int		bmap_one_extent(xfs_bmbt_rec_32_t *ep,
					xfs_dfiloff_t *offp, xfs_dfiloff_t eoff,
					int *idxp, bmap_ext_t *bep);
static xfs_fsblock_t	select_child(xfs_dfiloff_t off, xfs_bmbt_key_t *kp,
				     xfs_bmbt_ptr_t *pp, int nrecs);

static const cmdinfo_t	bmap_cmd =
	{ "bmap", NULL, bmap_f, 0, 3, 0, "[-ad] [block [len]]",
	  "show block map for current file", NULL };

void
bmap(
	xfs_dfiloff_t		offset,
	xfs_dfilblks_t		len,
	int			whichfork,
	int			*nexp,
	bmap_ext_t		*bep)
{
	xfs_bmbt_block_t	*block;
	xfs_fsblock_t		bno;
	xfs_dfiloff_t		curoffset;
	xfs_dinode_t		*dip;
	xfs_dfiloff_t		eoffset;
	xfs_bmbt_rec_32_t	*ep;
	xfs_dinode_fmt_t	fmt;
	int			fsize;
	xfs_bmbt_key_t		*kp;
	int			n;
	int			nex;
	xfs_fsblock_t		nextbno;
	int			nextents;
	xfs_bmbt_ptr_t		*pp;
	xfs_bmdr_block_t	*rblock;
	typnm_t			typ;
	xfs_bmbt_rec_32_t	*xp;

	push_cur();
	set_cur_inode(iocur_top->ino);
	nex = *nexp;
	*nexp = 0;
	assert(nex > 0);
	dip = iocur_top->data;
	n = 0;
	eoffset = offset + len - 1;
	curoffset = offset;
	fmt = (xfs_dinode_fmt_t)XFS_DFORK_FORMAT(dip, whichfork);
	typ = whichfork == XFS_DATA_FORK ? TYP_BMAPBTD : TYP_BMAPBTA;
	assert(typtab[typ].typnm == typ);
	assert(fmt == XFS_DINODE_FMT_EXTENTS || fmt == XFS_DINODE_FMT_BTREE);
	if (fmt == XFS_DINODE_FMT_EXTENTS) {
		nextents = XFS_DFORK_NEXTENTS(dip, whichfork);
		xp = (xfs_bmbt_rec_32_t *)XFS_DFORK_PTR(dip, whichfork);
		for (ep = xp; ep < &xp[nextents] && n < nex; ep++) {
			if (!bmap_one_extent(ep, &curoffset, eoffset, &n, bep))
				break;
		}
	} else {
		push_cur();
		bno = NULLFSBLOCK;
		rblock = (xfs_bmdr_block_t *)XFS_DFORK_PTR(dip, whichfork);
		fsize = XFS_DFORK_SIZE(dip, mp, whichfork);
		pp = XFS_BTREE_PTR_ADDR(fsize, xfs_bmdr, rblock, 1,
			XFS_BTREE_BLOCK_MAXRECS(fsize, xfs_bmdr, 0));
		kp = XFS_BTREE_KEY_ADDR(fsize, xfs_bmdr, rblock, 1,
			XFS_BTREE_BLOCK_MAXRECS(fsize, xfs_bmdr, 0));
		bno = select_child(curoffset, kp, pp, rblock->bb_numrecs);
		for (;;) {
			set_cur(&typtab[typ], XFS_FSB_TO_DADDR(mp, bno),
				blkbb, DB_RING_IGN, NULL);
			block = (xfs_bmbt_block_t *)iocur_top->data;
			if (block->bb_level == 0)
				break;
			pp = XFS_BTREE_PTR_ADDR(mp->m_sb.sb_blocksize, xfs_bmbt,
				block, 1,
				XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize,
					xfs_bmbt, 0));
			kp = XFS_BTREE_KEY_ADDR(mp->m_sb.sb_blocksize, xfs_bmbt,
				block, 1,
				XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize,
					xfs_bmbt, 0));
			bno = select_child(curoffset, kp, pp,
				block->bb_numrecs);
		}
		for (;;) {
			nextbno = block->bb_rightsib;
			nextents = block->bb_numrecs;
			xp = (xfs_bmbt_rec_32_t *)XFS_BTREE_REC_ADDR(
				mp->m_sb.sb_blocksize, xfs_bmbt, block, 1,
				XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize,
					xfs_bmbt, 1));
			for (ep = xp; ep < &xp[nextents] && n < nex; ep++) {
				if (!bmap_one_extent(ep, &curoffset, eoffset,
						&n, bep)) {
					nextbno = NULLFSBLOCK;
					break;
				}
			}
			bno = nextbno;
			if (bno == NULLFSBLOCK)
				break;
			set_cur(&typtab[typ], XFS_FSB_TO_DADDR(mp, bno),
				blkbb, DB_RING_IGN, NULL);
			block = (xfs_bmbt_block_t *)iocur_top->data;
		}
		pop_cur();
	}
	pop_cur();
	*nexp = n;
}

static int
bmap_f(
	int		argc,
	char		**argv)
{
	int		afork = 0;
	bmap_ext_t	be;
	int		c;
	xfs_dfiloff_t	co;
	int		dfork = 0;
	xfs_dinode_t	*dip;
	xfs_dfiloff_t	eo;
	xfs_dfilblks_t	len;
	int		nex;
	char		*p;
	int		whichfork;

#if VERS >= V_62
#define	OPTS	"ad"
#else
#define	OPTS	"d"
#endif

	if (iocur_top->ino == NULLFSINO) {
		dbprintf("no current inode\n");
		return 0;
	}
	optind = 0;
	while ((c = getopt(argc, argv, OPTS)) != EOF) {
		switch (c) {
		case 'a':
			afork = 1;
			break;
		case 'd':
			dfork = 1;
			break;
		default:
			dbprintf("bad option for bmap command\n");
			return 0;
		}
	}
	if (afork + dfork == 0) {
		push_cur();
		set_cur_inode(iocur_top->ino);
		dip = iocur_top->data;
		if (dip->di_core.di_nextents)
			dfork = 1;
#if VERS >= V_62
		if (dip->di_core.di_anextents)
			afork = 1;
#endif
		pop_cur();
	}
	if (optind < argc) {
		co = (xfs_dfiloff_t)strtoull(argv[optind], &p, 0);
		if (*p != '\0') {
			dbprintf("bad block number for bmap %s\n",
				argv[optind]);
			return 0;
		}
		optind++;
		if (optind < argc) {
			len = (xfs_dfilblks_t)strtoull(argv[optind], &p, 0);
			if (*p != '\0') {
				dbprintf("bad len for bmap %s\n", argv[optind]);
				return 0;
			}
			eo = co + len - 1;
		} else
			eo = co;
	} else {
		co = 0;
		eo = -1;
	}
	for (whichfork = XFS_DATA_FORK;
	     whichfork <= XFS_ATTR_FORK;
	     whichfork++) {
		if (whichfork == XFS_DATA_FORK && !dfork)
			continue;
		if (whichfork == XFS_ATTR_FORK && !afork)
			continue;
		for (;;) {
			nex = 1;
			bmap(co, eo - co + 1, whichfork, &nex, &be);
			if (nex == 0)
				break;
			dbprintf("%s offset %lld startblock %llu (%u/%u) count "
				 "%llu flag %u\n",
				whichfork == XFS_DATA_FORK ? "data" : "attr",
				be.startoff, be.startblock,
				XFS_FSB_TO_AGNO(mp, be.startblock),
				XFS_FSB_TO_AGBNO(mp, be.startblock),
				be.blockcount, be.flag);
			co = be.startoff + be.blockcount;
		}
	}
	return 0;
}

void
bmap_init(void)
{
	add_command(&bmap_cmd);
}

static int
bmap_one_extent(
	xfs_bmbt_rec_32_t	*ep,
	xfs_dfiloff_t		*offp,
	xfs_dfiloff_t		eoff,
	int			*idxp,
	bmap_ext_t		*bep)
{
	xfs_dfilblks_t		c;
	xfs_dfiloff_t		curoffset;
	int			f;
	int			idx;
	xfs_dfiloff_t		o;
	xfs_dfsbno_t		s;

	convert_extent(ep, &o, &s, &c, &f);
	curoffset = *offp;
	idx = *idxp;
	if (o + c <= curoffset)
		return 1;
	if (o > eoff)
		return 0;
	if (o < curoffset) {
		c -= curoffset - o;
		s += curoffset - o;
		o = curoffset;
	}
	if (o + c - 1 > eoff)
		c -= (o + c - 1) - eoff;
	bep[idx].startoff = o;
	bep[idx].startblock = s;
	bep[idx].blockcount = c;
	bep[idx].flag = f;
	*idxp = idx + 1;
	*offp = o + c;
	return 1;
}

void
convert_extent(
	xfs_bmbt_rec_32_t	*rp,
	xfs_dfiloff_t		*op,
	xfs_dfsbno_t		*sp,
	xfs_dfilblks_t		*cp,
	int			*fp)
{
	*fp = (((int)rp->l0) >> 31) & 1;
	*op = (((xfs_dfiloff_t)(rp->l0 & 0x7fffffff)) << 23) |
	      (((xfs_dfiloff_t)rp->l1) >> 9);
	*sp = (((xfs_dfsbno_t)(rp->l1 & 0x000001ff)) << 43) |
	      (((xfs_dfsbno_t)rp->l2) << 11) |
	      (((xfs_dfsbno_t)rp->l3) >> 21);
	*cp = (xfs_dfilblks_t)(rp->l3 & 0x001fffff);
}

void
make_bbmap(
	bbmap_t		*bbmap,
	int		nex,
	bmap_ext_t	*bmp)
{
	int		d;
	xfs_dfsbno_t	dfsbno;
	int		i;
	int		j;
	int		k;

	for (i = 0, d = 0; i < nex; i++) {
		dfsbno = bmp[i].startblock;
		for (j = 0; j < bmp[i].blockcount; j++, dfsbno++) {
			for (k = 0; k < blkbb; k++)
				bbmap->b[d++] =
					XFS_FSB_TO_DADDR(mp, dfsbno) + k;
		}
	}
}

static xfs_fsblock_t
select_child(
	xfs_dfiloff_t	off,
	xfs_bmbt_key_t	*kp,
	xfs_bmbt_ptr_t	*pp,
	int		nrecs)
{
	int		i;

	for (i = 0; i < nrecs; i++) {
		if (kp[i].br_startoff == off)
			return pp[i];
		if (kp[i].br_startoff > off) {
			if (i == 0)
				return pp[i];
			else
				return pp[i - 1];
		}
	}
	return pp[nrecs - 1];
}
