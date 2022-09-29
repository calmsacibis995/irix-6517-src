#ident	"$Revision: 1.17 $"


#define _KERNEL 1

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/buf.h>
#include <sys/sema.h>

#undef _KERNEL
#include <bstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/ktrace.h>
#include <sys/vnode.h>
#include <sys/uuid.h>

#include <sys/fs/xfs_macros.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_sb.h>		/* depends on xfs_types.h, xfs_inum.h */
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_dir.h>
#include <sys/fs/xfs_dir2.h>
#include <sys/fs/xfs_mount.h>		/* depends on xfs_trans.h & xfs_sb.h */
#include <sys/fs/xfs_buf_item.h>
#include <sys/fs/xfs_extfree_item.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_alloc.h>
#include <sys/fs/xfs_bmap.h>
#include <sys/fs/xfs_btree.h>
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#include <sys/fs/xfs_dir2_sf.h>
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_inode.h>		/* depends on xfs_inode_item.h */
#include <sys/fs/xfs_log_priv.h>	/* depends on all above */
#include <sys/fs/xfs_log_recover.h>

#include "sim.h"		/* must be last include file */

#ifdef FS_DEBUG
/*
 * This returns a pointer to a static buffer, so it can only
 * be used ONCE PER PRINTF CALL.
 */
static char *
fmtll(__int64_t i, char c)
{
	static char rval[24];

	if (c == 'x')
		sprintf(rval, "0x%llx", i);
	else if (c == 'd')
		sprintf(rval, "%lld", i);
	else if (c == 'o')
		sprintf(rval, "0%llo", i);
	return rval;
}


static char *
print_fmtino(xfs_ino_t ino, xfs_mount_t *mp)
{
	static char rval[50];

	if (mp)
		sprintf(rval, "%lld[%x:%x:%x]", ino, XFS_INO_TO_AGNO(mp, ino),
			XFS_INO_TO_AGBNO(mp, ino), XFS_INO_TO_OFFSET(mp, ino));
	else
		sprintf(rval, "%lld", ino);
	return rval;
}


static char *
print_fmtfsblock(xfs_fsblock_t bno, xfs_mount_t *mp)
{
	static char rval[50];

	if (bno == NULLFSBLOCK)
		sprintf(rval, "NULLFSBLOCK");
	else if (ISNULLSTARTBLOCK(bno))
		sprintf(rval, "NULLSTARTBLOCK(%d)", STARTBLOCKVAL(bno));
	else if (mp)
		sprintf(rval, "%lld[%x:%x]", (xfs_dfsbno_t)bno,
			XFS_FSB_TO_AGNO(mp, bno), XFS_FSB_TO_AGBNO(mp, bno));
	else
		sprintf(rval, "%lld", (xfs_dfsbno_t)bno);
	return rval;
}


static char *
print_fmtsize(size_t i)
{
	static char rval[20];

	/* ICK - is there any easy way to make this compile-time? */
	if (sizeof(i) == sizeof(int))
		sprintf(rval, "0x%x", i);
	else
		sprintf(rval, "0x%llx", i);
	return rval;
}


static char *
print_fmtmode(int m)
{
	static char rval[16];

	sprintf(rval, "%c%c%c%c%c%c%c%c%c%c%c%c%c",
		"?fc?dxb?r?l?S?m?"[(m & IFMT) >> 12],
		m & ISUID ? 'u' : '-',
		m & ISGID ? 'g' : '-',
		m & ISVTX ? 'v' : '-',
		m & IREAD ? 'r' : '-',
		m & IWRITE ? 'w' : '-',
		m & IEXEC ? 'x' : '-',
		m & (IREAD >> 3) ? 'r' : '-',
		m & (IWRITE >> 3) ? 'w' : '-',
		m & (IEXEC >> 3) ? 'x' : '-',
		m & (IREAD >> 6) ? 'r' : '-',
		m & (IWRITE >> 6) ? 'w' : '-',
		m & (IEXEC >> 6) ? 'x' : '-');
	return rval;
}


static char *
print_fmtformat(xfs_dinode_fmt_t f)
{
	static char *t[] = {
		"dev",
		"local",
		"extents",
		"btree",
		"uuid"
	};

	return t[f];
}


static char *
print_fmtuuid(uuid_t *uu)
{
	static char rval[40];
	uint *ip = (uint *)uu;

	ASSERT(sizeof(*uu) == 16);
	sprintf(rval, "%x:%x:%x:%x", ip[0], ip[1], ip[2], ip[3]);
	return rval;
}


static void
print_btalloc(xfs_alloc_block_t *bt, int bsz)
{
	int i;

	printf("magic 0x%x level %d numrecs %d leftsib 0x%x rightsib 0x%x\n",
		bt->bb_magic, bt->bb_level, bt->bb_numrecs,
		bt->bb_leftsib, bt->bb_rightsib);
	if (bt->bb_level == 0) {
		int mxr;

		mxr = XFS_BTREE_BLOCK_MAXRECS(bsz, xfs_alloc, 1);
		for (i = 1; i <= bt->bb_numrecs; i++) {
			xfs_alloc_rec_t *r;

			r = XFS_BTREE_REC_ADDR(bsz, xfs_alloc, bt, i, mxr);
			printf("rec %d startblock 0x%x blockcount %d\n",
				i, r->ar_startblock, r->ar_blockcount);
		}
	} else {
		int mxr;

		mxr = XFS_BTREE_BLOCK_MAXRECS(bsz, xfs_alloc, 0);
		for (i = 1; i <= bt->bb_numrecs; i++) {
			xfs_alloc_key_t *k;
			xfs_alloc_ptr_t *p;

			k = XFS_BTREE_KEY_ADDR(bsz, xfs_alloc, bt, i, mxr);
			p = XFS_BTREE_PTR_ADDR(bsz, xfs_alloc, bt, i, mxr);
			printf("key %d startblock 0x%x blockcount %d ptr 0x%x\n",
				i, k->ar_startblock, k->ar_blockcount, *p);
		}
	}
}


static void
print_convert_extent(xfs_bmbt_rec_32_t	*rp,
		     xfs_dfiloff_t	*op,
		     xfs_dfsbno_t	*sp,
		     xfs_dfilblks_t	*cp)
{
	xfs_dfiloff_t o;
	xfs_dfsbno_t s;
	xfs_dfilblks_t c;

	o = (((xfs_dfiloff_t)rp->l0) << 23) |
	    (((xfs_dfiloff_t)rp->l1) >> 9);
	s = (((xfs_dfsbno_t)(rp->l1 & 0x000001ff)) << 43) |
	    (((xfs_dfsbno_t)rp->l2) << 11) |
	    (((xfs_dfsbno_t)rp->l3) >> 21);
	c = (xfs_dfilblks_t)(rp->l3 & 0x001fffff);
	*op = o;
	*sp = s;
	*cp = c;
}


static void
print_btbmap(xfs_bmbt_block_t *bt, int bsz)
{
	int i;

	printf("magic 0x%x level %d numrecs %d leftsib %s ",
		bt->bb_magic, bt->bb_level, bt->bb_numrecs,
		fmtll(bt->bb_leftsib, 'x'));
	printf("rightsib %s\n",
		fmtll(bt->bb_rightsib, 'x'));
	if (bt->bb_level == 0) {
		int mxr;

		mxr = XFS_BTREE_BLOCK_MAXRECS(bsz, xfs_bmbt, 1);
		for (i = 1; i <= bt->bb_numrecs; i++) {
			xfs_bmbt_rec_32_t *r;
			xfs_dfiloff_t o;
			xfs_dfsbno_t s;
			xfs_dfilblks_t c;

			r = (xfs_bmbt_rec_32_t *)XFS_BTREE_REC_ADDR(bsz,
				xfs_bmbt, bt, i, 0 mxr);
			print_convert_extent(r, &o, &s, &c);
			printf("rec %d startoff %s ",
				i, fmtll(o, 'd'));
			printf("startblock %s ",
				fmtll(s, 'x'));
			printf("blockcount %s\n",
				fmtll(c, 'd'));
		}
	} else {
		int mxr;

		mxr = XFS_BTREE_BLOCK_MAXRECS(bsz, xfs_bmbt, 0);
		for (i = 1; i <= bt->bb_numrecs; i++) {
			xfs_bmbt_key_t *k;
			xfs_bmbt_ptr_t *p;

			k = XFS_BTREE_KEY_ADDR(bsz, xfs_bmbt, bt, i, mxr);
			p = XFS_BTREE_PTR_ADDR(bsz, xfs_bmbt, bt, i, mxr);
			printf("key %d startoff %s ",
				i, fmtll(k->br_startoff, 'd'));
			printf("ptr %s\n", fmtll(*p, 'x'));
		}
	}
}


static void
print_btinobt(xfs_inobt_block_t *bt, int bsz)
{
	int i;

	printf("magic 0x%x level %d numrecs %d leftsib 0x%x rightsib 0x%x\n",
		bt->bb_magic, bt->bb_level, bt->bb_numrecs,
		bt->bb_leftsib, bt->bb_rightsib);
	if (bt->bb_level == 0) {
		int mxr;

		mxr = XFS_BTREE_BLOCK_MAXRECS(bsz, xfs_inobt, 1);
		for (i = 1; i <= bt->bb_numrecs; i++) {
			xfs_inobt_rec_t *r;

			r = XFS_BTREE_REC_ADDR(bsz, xfs_inobt, bt, i, mxr);
			printf("rec %d startino 0x%x freecount %d, free %s\n",
				i, r->ir_startino, r->ir_freecount,
				fmtll(r->ir_free, 'x'));
		}
	} else {
		int mxr;

		mxr = XFS_BTREE_BLOCK_MAXRECS(bsz, xfs_inobt, 0);
		for (i = 1; i <= bt->bb_numrecs; i++) {
			xfs_inobt_key_t *k;
			xfs_inobt_ptr_t *p;

			k = XFS_BTREE_KEY_ADDR(bsz, xfs_inobt, bt, i, mxr);
			p = XFS_BTREE_PTR_ADDR(bsz, xfs_inobt, bt, i, mxr);
			printf("key %d startino 0x%x ptr 0x%x\n",
				i, k->ir_startino, *p);
		}
	}
}


void
print_xbuf(buf_t *bp)
{
	void *d;
	xfs_agf_t *agf;
	xfs_agi_t *agi;
	xfs_sb_t *sb;
	xfs_alloc_block_t *bta;
	xfs_bmbt_block_t *btb;
	xfs_inobt_block_t *bti;
	struct xfs_dir_leafblock *leaf;
	struct xfs_dir_intnode *node;
	xfs_dinode_t *di;

	d = bp->b_un.b_addr;
	if ((agf = d)->agf_magicnum == XFS_AGF_MAGIC) {
		printf("buf 0x%x agf 0x%x\n", bp, agf);
		idbg_xagf(agf);
	} else if ((agi = d)->agi_magicnum == XFS_AGI_MAGIC) {
		printf("buf 0x%x agi 0x%x\n", bp, agi);
		idbg_xagi(agi);
	} else if ((bta = d)->bb_magic == XFS_ABTB_MAGIC) {
		printf("buf 0x%x abtbno 0x%x\n", bp, bta);
		xfs_btalloc(bta, BBTOB(bp->b_bcount));
	} else if ((bta = d)->bb_magic == XFS_ABTC_MAGIC) {
		printf("buf 0x%x abtcnt 0x%x\n", bp, bta);
		xfs_btalloc(bta, BBTOB(bp->b_bcount));
	} else if ((btb = d)->bb_magic == XFS_BMAP_MAGIC) {
		printf("buf 0x%x bmapbt 0x%x\n", bp, btb);
		xfs_btbmap(btb, BBTOB(bp->b_bcount));
	} else if ((bti = d)->bb_magic == XFS_IBT_MAGIC) {
		printf("buf 0x%x inobt 0x%x\n", bp, bti);
		xfs_btinobt(bti, BBTOB(bp->b_bcount));
	} else if ((leaf = d)->hdr.info.magic == XFS_DIR_LEAF_MAGIC) {
		printf("buf 0x%x dirleaf 0x%x\n", bp, leaf);
		idbg_xdirlf(leaf);
	} else if ((node = d)->hdr.info.magic == XFS_DIR_NODE_MAGIC) {
		printf("buf 0x%x dirnode 0x%x\n", bp, node);
		idbg_xdirnd(node);
	} else if ((di = d)->di_core.di_magic == XFS_DINODE_MAGIC) {
		printf("buf 0x%x dinode 0x%x\n", bp, di);
		xfs_inodebuf(bp);
	} else if ((sb = d)->sb_magicnum == XFS_SB_MAGIC) {
		printf("buf 0x%x sb 0x%x\n", bp, sb);
		idbg_xsb(sb);
	} else {
		printf("buf 0x%x unknown 0x%x\n", bp, d);
	}
}
#endif /* FS_DEBUG */


void
init_log_struct(xlog_t	*log,
		dev_t	log_dev,
		daddr_t blk_offset,
		int	num_bblks)
{
	log->l_dev	   = log_dev;
	log->l_logsize     = BBTOB(num_bblks);
	log->l_logBBstart  = blk_offset;
	log->l_logBBsize   = num_bblks;

	if ((log->l_mp = malloc(sizeof(xfs_mount_t))) == NULL) {
		fprintf(stderr, "malloc of xfs_mount_t failed\n");
		exit(1);
	}

	bzero(log->l_mp, sizeof(xfs_mount_t));

	/* I think this is all we need */
}	/* init_log_struct */

/* ARGSUSED */
void
print_trans(xlog_t	*log,
	    daddr_t	head_blk,
	    daddr_t	tail_blk)
{
	
}	/* print_trans */

daddr_t HEAD_BLK, TAIL_BLK;

/* ARGSUSED */
void
xfs_log_print_trans(dev_t	log_dev,
		    daddr_t	blk_offset,
		    int		num_bblks,
		    int		print_block_start,
		    uint	flags)
{
	xlog_t	log;

	init_log_struct(&log, log_dev, blk_offset, num_bblks);
	xlog_find_tail(&log, &HEAD_BLK, &TAIL_BLK, 0);
	printf("log device: 0x%x	block offset: %lld	# BBs: %d\n",
	       log_dev, (__int64_t)blk_offset, num_bblks);
	printf("tail: %lld   head: %lld\n",
		(__int64_t)TAIL_BLK, (__int64_t)HEAD_BLK);
	printf("=========================================================\n");
	if (print_block_start != -1) {
	    TAIL_BLK = print_block_start;
	    printf("NEW tail: %lld   head: %lld\n",
		    (__int64_t)TAIL_BLK, (__int64_t)HEAD_BLK);
        }
	print_trans(&log, HEAD_BLK, TAIL_BLK);
	xlog_debug = 3;
	xlog_recover(&log, 0);
}	/* xfs_log_print_trans */
