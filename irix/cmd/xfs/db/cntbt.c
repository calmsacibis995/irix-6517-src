#ident "$Revision: 1.9 $"

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
#include "cntbt.h"
#include "print.h"
#include "bit.h"
#include "mount.h"

static int	cntbt_key_count(void *obj, int startoff);
static int	cntbt_key_offset(void *obj, int startoff, int idx);
static int	cntbt_ptr_count(void *obj, int startoff);
static int	cntbt_ptr_offset(void *obj, int startoff, int idx);
static int	cntbt_rec_count(void *obj, int startoff);
static int	cntbt_rec_offset(void *obj, int startoff, int idx);

const field_t	cntbt_hfld[] = {
	{ "", FLDT_CNTBT, OI(0), C1, 0, TYP_NONE },
	{ NULL }
};

#define	OFF(f)	bitize(offsetof(xfs_alloc_block_t, bb_ ## f))
const field_t	cntbt_flds[] = {
	{ "magic", FLDT_UINT32X, OI(OFF(magic)), C1, 0, TYP_NONE },
	{ "level", FLDT_UINT16D, OI(OFF(level)), C1, 0, TYP_NONE },
	{ "numrecs", FLDT_UINT16D, OI(OFF(numrecs)), C1, 0, TYP_NONE },
	{ "leftsib", FLDT_AGBLOCK, OI(OFF(leftsib)), C1, 0, TYP_CNTBT },
	{ "rightsib", FLDT_AGBLOCK, OI(OFF(rightsib)), C1, 0, TYP_CNTBT },
	{ "recs", FLDT_CNTBTREC, cntbt_rec_offset, cntbt_rec_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_NONE },
	{ "keys", FLDT_CNTBTKEY, cntbt_key_offset, cntbt_key_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_NONE },
	{ "ptrs", FLDT_CNTBTPTR, cntbt_ptr_offset, cntbt_ptr_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_CNTBT },
	{ NULL }
};

#define	KOFF(f)	bitize(offsetof(xfs_alloc_key_t, ar_ ## f))
const field_t	cntbt_key_flds[] = {
	{ "blockcount", FLDT_EXTLEN, OI(KOFF(blockcount)), C1, 0, TYP_NONE },
	{ "startblock", FLDT_AGBLOCK, OI(KOFF(startblock)), C1, 0, TYP_DATA },
	{ NULL }
};

#define	ROFF(f)	bitize(offsetof(xfs_alloc_rec_t, ar_ ## f))
const field_t	cntbt_rec_flds[] = {
	{ "startblock", FLDT_AGBLOCK, OI(ROFF(startblock)), C1, 0, TYP_DATA },
	{ "blockcount", FLDT_EXTLEN, OI(ROFF(blockcount)), C1, 0, TYP_NONE },
	{ NULL }
};

/*ARGSUSED*/
static int
cntbt_key_count(
	void			*obj,
	int			startoff)
{
	xfs_alloc_block_t	*block;

	assert(startoff == 0);
	block = obj;
	if (block->bb_level == 0)
		return 0;
	return block->bb_numrecs;
}

/*ARGSUSED*/
static int
cntbt_key_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_alloc_block_t	*block;
	xfs_alloc_key_t		*kp;

	assert(startoff == 0);
	block = obj;
	assert(block->bb_level > 0);
	kp = XFS_BTREE_KEY_ADDR(mp->m_sb.sb_blocksize, xfs_alloc, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_alloc, 0));
	return bitize((int)((char *)kp - (char *)block));
}

/*ARGSUSED*/
static int
cntbt_ptr_count(
	void			*obj,
	int			startoff)
{
	xfs_alloc_block_t	*block;

	assert(startoff == 0);
	block = obj;
	if (block->bb_level == 0)
		return 0;
	return block->bb_numrecs;
}

/*ARGSUSED*/
static int
cntbt_ptr_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_alloc_block_t	*block;
	xfs_alloc_ptr_t		*pp;

	assert(startoff == 0);
	block = obj;
	assert(block->bb_level > 0);
	pp = XFS_BTREE_PTR_ADDR(mp->m_sb.sb_blocksize, xfs_alloc, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_alloc, 0));
	return bitize((int)((char *)pp - (char *)block));
}

/*ARGSUSED*/
static int
cntbt_rec_count(
	void			*obj,
	int			startoff)
{
	xfs_alloc_block_t	*block;

	assert(startoff == 0);
	block = obj;
	if (block->bb_level > 0)
		return 0;
	return block->bb_numrecs;
}

/*ARGSUSED*/
static int
cntbt_rec_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_alloc_block_t	*block;
	xfs_alloc_rec_t		*rp;

	assert(startoff == 0);
	block = obj;
	assert(block->bb_level == 0);
	rp = XFS_BTREE_REC_ADDR(mp->m_sb.sb_blocksize, xfs_alloc, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_alloc, 1));
	return bitize((int)((char *)rp - (char *)block));
}

/*ARGSUSED*/
int
cntbt_size(
	void	*obj,
	int	startoff,
	int	idx)
{
	return bitize(mp->m_sb.sb_blocksize);
}
