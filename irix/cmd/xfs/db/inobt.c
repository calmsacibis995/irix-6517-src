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
#include "inobt.h"
#include "print.h"
#include "bit.h"
#include "mount.h"

static int	inobt_key_count(void *obj, int startoff);
static int	inobt_key_offset(void *obj, int startoff, int idx);
static int	inobt_ptr_count(void *obj, int startoff);
static int	inobt_ptr_offset(void *obj, int startoff, int idx);
static int	inobt_rec_count(void *obj, int startoff);
static int	inobt_rec_offset(void *obj, int startoff, int idx);

const field_t	inobt_hfld[] = {
	{ "", FLDT_INOBT, OI(0), C1, 0, TYP_NONE },
	{ NULL }
};

#define	OFF(f)	bitize(offsetof(xfs_inobt_block_t, bb_ ## f))
const field_t	inobt_flds[] = {
	{ "magic", FLDT_UINT32X, OI(OFF(magic)), C1, 0, TYP_NONE },
	{ "level", FLDT_UINT16D, OI(OFF(level)), C1, 0, TYP_NONE },
	{ "numrecs", FLDT_UINT16D, OI(OFF(numrecs)), C1, 0, TYP_NONE },
	{ "leftsib", FLDT_AGBLOCK, OI(OFF(leftsib)), C1, 0, TYP_INOBT },
	{ "rightsib", FLDT_AGBLOCK, OI(OFF(rightsib)), C1, 0, TYP_INOBT },
	{ "recs", FLDT_INOBTREC, inobt_rec_offset, inobt_rec_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_NONE },
	{ "keys", FLDT_INOBTKEY, inobt_key_offset, inobt_key_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_NONE },
	{ "ptrs", FLDT_INOBTPTR, inobt_ptr_offset, inobt_ptr_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_INOBT },
	{ NULL }
};

#define	KOFF(f)	bitize(offsetof(xfs_inobt_key_t, ir_ ## f))
const field_t	inobt_key_flds[] = {
	{ "startino", FLDT_AGINO, OI(KOFF(startino)), C1, 0, TYP_INODE },
	{ NULL }
};

#define	ROFF(f)	bitize(offsetof(xfs_inobt_rec_t, ir_ ## f))
const field_t	inobt_rec_flds[] = {
	{ "startino", FLDT_AGINO, OI(ROFF(startino)), C1, 0, TYP_INODE },
	{ "freecount", FLDT_INT32D, OI(ROFF(freecount)), C1, 0, TYP_NONE },
	{ "free", FLDT_INOFREE, OI(ROFF(free)), C1, 0, TYP_NONE },
	{ NULL }
};

/*ARGSUSED*/
static int
inobt_key_count(
	void			*obj,
	int			startoff)
{
	xfs_inobt_block_t	*block;

	assert(startoff == 0);
	block = obj;
	if (block->bb_level == 0)
		return 0;
	return block->bb_numrecs;
}

/*ARGSUSED*/
static int
inobt_key_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_inobt_block_t	*block;
	xfs_inobt_key_t		*kp;

	assert(startoff == 0);
	block = obj;
	assert(block->bb_level > 0);
	kp = XFS_BTREE_KEY_ADDR(mp->m_sb.sb_blocksize, xfs_inobt, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_inobt, 0));
	return bitize((int)((char *)kp - (char *)block));
}

/*ARGSUSED*/
static int
inobt_ptr_count(
	void			*obj,
	int			startoff)
{
	xfs_inobt_block_t	*block;

	assert(startoff == 0);
	block = obj;
	if (block->bb_level == 0)
		return 0;
	return block->bb_numrecs;
}

/*ARGSUSED*/
static int
inobt_ptr_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_inobt_block_t	*block;
	xfs_inobt_ptr_t		*pp;

	assert(startoff == 0);
	block = obj;
	assert(block->bb_level > 0);
	pp = XFS_BTREE_PTR_ADDR(mp->m_sb.sb_blocksize, xfs_inobt, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_inobt, 0));
	return bitize((int)((char *)pp - (char *)block));
}

/*ARGSUSED*/
static int
inobt_rec_count(
	void			*obj,
	int			startoff)
{
	xfs_inobt_block_t	*block;

	assert(startoff == 0);
	block = obj;
	if (block->bb_level > 0)
		return 0;
	return block->bb_numrecs;
}

/*ARGSUSED*/
static int
inobt_rec_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_inobt_block_t	*block;
	xfs_inobt_rec_t		*rp;

	assert(startoff == 0);
	block = obj;
	assert(block->bb_level == 0);
	rp = XFS_BTREE_REC_ADDR(mp->m_sb.sb_blocksize, xfs_inobt, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_inobt, 1));
	return bitize((int)((char *)rp - (char *)block));
}

/*ARGSUSED*/
int
inobt_size(
	void	*obj,
	int	startoff,
	int	idx)
{
	return bitize(mp->m_sb.sb_blocksize);
}
