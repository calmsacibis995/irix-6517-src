#ident "$Revision: 1.10 $"

#include <sys/types.h>
#include <sys/uuid.h>
#include <assert.h>
#include <stddef.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_btree.h>
#include "sim.h"
#include "data.h"
#include "type.h"
#include "faddr.h"
#include "fprint.h"
#include "field.h"
#include "bmapbt.h"
#include "print.h"
#include "bit.h"
#include "mount.h"

#if VERS >= V_62
static int	bmapbta_key_count(void *obj, int startoff);
static int	bmapbta_key_offset(void *obj, int startoff, int idx);
static int	bmapbta_ptr_count(void *obj, int startoff);
static int	bmapbta_ptr_offset(void *obj, int startoff, int idx);
static int	bmapbta_rec_count(void *obj, int startoff);
static int	bmapbta_rec_offset(void *obj, int startoff, int idx);
#endif
static int	bmapbtd_key_count(void *obj, int startoff);
static int	bmapbtd_key_offset(void *obj, int startoff, int idx);
static int	bmapbtd_ptr_count(void *obj, int startoff);
static int	bmapbtd_ptr_offset(void *obj, int startoff, int idx);
static int	bmapbtd_rec_count(void *obj, int startoff);
static int	bmapbtd_rec_offset(void *obj, int startoff, int idx);

#if VERS >= V_62
const field_t	bmapbta_hfld[] = {
	{ "", FLDT_BMAPBTA, OI(0), C1, 0, TYP_NONE },
	{ NULL }
};
#endif
const field_t	bmapbtd_hfld[] = {
	{ "", FLDT_BMAPBTD, OI(0), C1, 0, TYP_NONE },
	{ NULL }
};

#define	OFF(f)	bitize(offsetof(xfs_bmbt_block_t, bb_ ## f))
#if VERS >= V_62
const field_t	bmapbta_flds[] = {
	{ "magic", FLDT_UINT32X, OI(OFF(magic)), C1, 0, TYP_NONE },
	{ "level", FLDT_UINT16D, OI(OFF(level)), C1, 0, TYP_NONE },
	{ "numrecs", FLDT_UINT16D, OI(OFF(numrecs)), C1, 0, TYP_NONE },
	{ "leftsib", FLDT_DFSBNO, OI(OFF(leftsib)), C1, 0, TYP_BMAPBTA },
	{ "rightsib", FLDT_DFSBNO, OI(OFF(rightsib)), C1, 0, TYP_BMAPBTA },
	{ "recs", FLDT_BMAPBTAREC, bmapbta_rec_offset, bmapbta_rec_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_NONE },
	{ "keys", FLDT_BMAPBTAKEY, bmapbta_key_offset, bmapbta_key_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_NONE },
	{ "ptrs", FLDT_BMAPBTAPTR, bmapbta_ptr_offset, bmapbta_ptr_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_BMAPBTA },
	{ NULL }
};
#endif
const field_t	bmapbtd_flds[] = {
	{ "magic", FLDT_UINT32X, OI(OFF(magic)), C1, 0, TYP_NONE },
	{ "level", FLDT_UINT16D, OI(OFF(level)), C1, 0, TYP_NONE },
	{ "numrecs", FLDT_UINT16D, OI(OFF(numrecs)), C1, 0, TYP_NONE },
	{ "leftsib", FLDT_DFSBNO, OI(OFF(leftsib)), C1, 0, TYP_BMAPBTD },
	{ "rightsib", FLDT_DFSBNO, OI(OFF(rightsib)), C1, 0, TYP_BMAPBTD },
	{ "recs", FLDT_BMAPBTDREC, bmapbtd_rec_offset, bmapbtd_rec_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_NONE },
	{ "keys", FLDT_BMAPBTDKEY, bmapbtd_key_offset, bmapbtd_key_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_NONE },
	{ "ptrs", FLDT_BMAPBTDPTR, bmapbtd_ptr_offset, bmapbtd_ptr_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_BMAPBTD },
	{ NULL }
};

#define	KOFF(f)	bitize(offsetof(xfs_bmbt_key_t, br_ ## f))
#if VERS >= V_62
const field_t	bmapbta_key_flds[] = {
	{ "startoff", FLDT_DFILOFFA, OI(KOFF(startoff)), C1, 0, TYP_ATTR },
	{ NULL }
};
#endif
const field_t	bmapbtd_key_flds[] = {
	{ "startoff", FLDT_DFILOFFD, OI(KOFF(startoff)), C1, 0, TYP_INODATA },
	{ NULL }
};

#if VERS >= V_62
const field_t	bmapbta_rec_flds[] = {
	{ "startoff", FLDT_CFILEOFFA, OI(BMBT_STARTOFF_BITOFF), C1, 0,
	  TYP_ATTR },
	{ "startblock", FLDT_CFSBLOCK, OI(BMBT_STARTBLOCK_BITOFF), C1, 0,
	  TYP_ATTR },
	{ "blockcount", FLDT_CEXTLEN, OI(BMBT_BLOCKCOUNT_BITOFF), C1, 0,
	  TYP_NONE },
	{ "extentflag", FLDT_CEXTFLG, OI(BMBT_EXNTFLAG_BITOFF), C1, 0,
	  TYP_NONE },
	{ NULL }
};
#endif
const field_t	bmapbtd_rec_flds[] = {
	{ "startoff", FLDT_CFILEOFFD, OI(BMBT_STARTOFF_BITOFF), C1, 0,
	  TYP_INODATA },
	{ "startblock", FLDT_CFSBLOCK, OI(BMBT_STARTBLOCK_BITOFF), C1, 0,
	  TYP_INODATA },
	{ "blockcount", FLDT_CEXTLEN, OI(BMBT_BLOCKCOUNT_BITOFF), C1, 0,
	  TYP_NONE },
	{ "extentflag", FLDT_CEXTFLG, OI(BMBT_EXNTFLAG_BITOFF), C1, 0,
	  TYP_NONE },
	{ NULL }
};

#if VERS >= V_62
/*ARGSUSED*/
static int
bmapbta_key_count(
	void			*obj,
	int			startoff)
{
	xfs_bmbt_block_t	*block;

	assert(startoff == 0);
	block = obj;
	if (block->bb_level == 0)
		return 0;
	return block->bb_numrecs;
}

/*ARGSUSED*/
static int
bmapbta_key_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_bmbt_block_t	*block;
	xfs_bmbt_key_t		*kp;

	assert(startoff == 0);
	block = obj;
	assert(block->bb_level > 0);
	kp = XFS_BTREE_KEY_ADDR(mp->m_sb.sb_blocksize, xfs_bmbt, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_bmbt, 0));
	return bitize((int)((char *)kp - (char *)block));
}

/*ARGSUSED*/
static int
bmapbta_ptr_count(
	void			*obj,
	int			startoff)
{
	xfs_bmbt_block_t	*block;

	assert(startoff == 0);
	block = obj;
	if (block->bb_level == 0)
		return 0;
	return block->bb_numrecs;
}

/*ARGSUSED*/
static int
bmapbta_ptr_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_bmbt_block_t	*block;
	xfs_bmbt_ptr_t		*pp;

	assert(startoff == 0);
	block = obj;
	assert(block->bb_level > 0);
	pp = XFS_BTREE_PTR_ADDR(mp->m_sb.sb_blocksize, xfs_bmbt, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_bmbt, 0));
	return bitize((int)((char *)pp - (char *)block));
}

/*ARGSUSED*/
static int
bmapbta_rec_count(
	void			*obj,
	int			startoff)
{
	xfs_bmbt_block_t	*block;

	assert(startoff == 0);
	block = obj;
	if (block->bb_level > 0)
		return 0;
	return block->bb_numrecs;
}

/*ARGSUSED*/
static int
bmapbta_rec_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_bmbt_block_t	*block;
	xfs_bmbt_rec_t		*rp;

	assert(startoff == 0);
	block = obj;
	assert(block->bb_level == 0);
	rp = XFS_BTREE_REC_ADDR(mp->m_sb.sb_blocksize, xfs_bmbt, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_bmbt, 1));
	return bitize((int)((char *)rp - (char *)block));
}

/*ARGSUSED*/
int
bmapbta_size(
	void	*obj,
	int	startoff,
	int	idx)
{
	return bitize(mp->m_sb.sb_blocksize);
}
#endif

/*ARGSUSED*/
static int
bmapbtd_key_count(
	void			*obj,
	int			startoff)
{
	xfs_bmbt_block_t	*block;

	assert(startoff == 0);
	block = obj;
	if (block->bb_level == 0)
		return 0;
	return block->bb_numrecs;
}

/*ARGSUSED*/
static int
bmapbtd_key_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_bmbt_block_t	*block;
	xfs_bmbt_key_t		*kp;

	assert(startoff == 0);
	block = obj;
	assert(block->bb_level > 0);
	kp = XFS_BTREE_KEY_ADDR(mp->m_sb.sb_blocksize, xfs_bmbt, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_bmbt, 0));
	return bitize((int)((char *)kp - (char *)block));
}

/*ARGSUSED*/
static int
bmapbtd_ptr_count(
	void			*obj,
	int			startoff)
{
	xfs_bmbt_block_t	*block;

	assert(startoff == 0);
	block = obj;
	if (block->bb_level == 0)
		return 0;
	return block->bb_numrecs;
}

/*ARGSUSED*/
static int
bmapbtd_ptr_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_bmbt_block_t	*block;
	xfs_bmbt_ptr_t		*pp;

	assert(startoff == 0);
	block = obj;
	assert(block->bb_level > 0);
	pp = XFS_BTREE_PTR_ADDR(mp->m_sb.sb_blocksize, xfs_bmbt, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_bmbt, 0));
	return bitize((int)((char *)pp - (char *)block));
}

/*ARGSUSED*/
static int
bmapbtd_rec_count(
	void			*obj,
	int			startoff)
{
	xfs_bmbt_block_t	*block;

	assert(startoff == 0);
	block = obj;
	if (block->bb_level > 0)
		return 0;
	return block->bb_numrecs;
}

/*ARGSUSED*/
static int
bmapbtd_rec_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_bmbt_block_t	*block;
	xfs_bmbt_rec_t		*rp;

	assert(startoff == 0);
	block = obj;
	assert(block->bb_level == 0);
	rp = XFS_BTREE_REC_ADDR(mp->m_sb.sb_blocksize, xfs_bmbt, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_bmbt, 1));
	return bitize((int)((char *)rp - (char *)block));
}

/*ARGSUSED*/
int
bmapbtd_size(
	void	*obj,
	int	startoff,
	int	idx)
{
	return bitize(mp->m_sb.sb_blocksize);
}
