#ident "$Revision: 1.15 $"

#include "versions.h"
#include <sys/param.h>
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
#include "sim.h"
#include "data.h"
#include "type.h"
#include "faddr.h"
#include "fprint.h"
#include "field.h"
#include "bmroot.h"
#include "io.h"
#include "print.h"
#include "bit.h"
#include "mount.h"

#if VERS >= V_62
static int	bmroota_key_count(void *obj, int startoff);
static int	bmroota_key_offset(void *obj, int startoff, int idx);
static int	bmroota_ptr_count(void *obj, int startoff);
static int	bmroota_ptr_offset(void *obj, int startoff, int idx);
#endif
static int	bmrootd_key_count(void *obj, int startoff);
static int	bmrootd_key_offset(void *obj, int startoff, int idx);
static int	bmrootd_ptr_count(void *obj, int startoff);
static int	bmrootd_ptr_offset(void *obj, int startoff, int idx);

#define	OFF(f)	bitize(offsetof(xfs_bmdr_block_t, bb_ ## f))
#if VERS >= V_62
const field_t	bmroota_flds[] = {
	{ "level", FLDT_UINT16D, OI(OFF(level)), C1, 0, TYP_NONE },
	{ "numrecs", FLDT_UINT16D, OI(OFF(numrecs)), C1, 0, TYP_NONE },
	{ "keys", FLDT_BMROOTAKEY, bmroota_key_offset, bmroota_key_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_NONE },
	{ "ptrs", FLDT_BMROOTAPTR, bmroota_ptr_offset, bmroota_ptr_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_BMAPBTA },
	{ NULL }
};
#endif
const field_t	bmrootd_flds[] = {
	{ "level", FLDT_UINT16D, OI(OFF(level)), C1, 0, TYP_NONE },
	{ "numrecs", FLDT_UINT16D, OI(OFF(numrecs)), C1, 0, TYP_NONE },
	{ "keys", FLDT_BMROOTDKEY, bmrootd_key_offset, bmrootd_key_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_NONE },
	{ "ptrs", FLDT_BMROOTDPTR, bmrootd_ptr_offset, bmrootd_ptr_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_BMAPBTD },
	{ NULL }
};

#define	KOFF(f)	bitize(offsetof(xfs_bmdr_key_t, br_ ## f))
#if VERS >= V_62
const field_t	bmroota_key_flds[] = {
	{ "startoff", FLDT_DFILOFFA, OI(KOFF(startoff)), C1, 0, TYP_NONE },
	{ NULL }
};
#endif
const field_t	bmrootd_key_flds[] = {
	{ "startoff", FLDT_DFILOFFD, OI(KOFF(startoff)), C1, 0, TYP_NONE },
	{ NULL }
};

#if VERS >= V_62
static int
bmroota_key_count(
	void			*obj,
	int			startoff)
{
	xfs_bmdr_block_t	*block;
#ifndef NDEBUG
	xfs_dinode_t		*dip = obj;
#endif

	assert(bitoffs(startoff) == 0);
	assert(obj == iocur_top->data);
	block = (xfs_bmdr_block_t *)((char *)obj + byteize(startoff));
	assert(XFS_DFORK_Q(dip) && (char *)block == XFS_DFORK_APTR(dip));
	assert(block->bb_level > 0);
	return block->bb_numrecs;
}

static int
bmroota_key_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_bmdr_block_t	*block;
	/* REFERENCED */
	xfs_dinode_t		*dip;
	xfs_bmdr_key_t		*kp;

	assert(bitoffs(startoff) == 0);
	assert(obj == iocur_top->data);
	dip = obj;
	block = (xfs_bmdr_block_t *)((char *)obj + byteize(startoff));
	assert(XFS_DFORK_Q(dip) && (char *)block == XFS_DFORK_APTR(dip));
	assert(block->bb_level > 0);
	kp = XFS_BTREE_KEY_ADDR(iocur_top->len, xfs_bmdr, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(XFS_DFORK_ASIZE(dip, mp), xfs_bmdr, 0));
	return bitize((int)((char *)kp - (char *)block));
}

static int
bmroota_ptr_count(
	void			*obj,
	int			startoff)
{
	xfs_bmdr_block_t	*block;
#ifndef NDEBUG
	xfs_dinode_t		*dip = obj;
#endif

	assert(bitoffs(startoff) == 0);
	assert(obj == iocur_top->data);
	block = (xfs_bmdr_block_t *)((char *)obj + byteize(startoff));
	assert(XFS_DFORK_Q(dip) && (char *)block == XFS_DFORK_APTR(dip));
	assert(block->bb_level > 0);
	return block->bb_numrecs;
}

static int
bmroota_ptr_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_bmdr_block_t	*block;
	xfs_dinode_t		*dip;
	xfs_bmdr_ptr_t		*pp;

	assert(bitoffs(startoff) == 0);
	assert(obj == iocur_top->data);
	dip = obj;
	block = (xfs_bmdr_block_t *)((char *)obj + byteize(startoff));
	assert(XFS_DFORK_Q(dip) && (char *)block == XFS_DFORK_APTR(dip));
	assert(block->bb_level > 0);
	pp = XFS_BTREE_PTR_ADDR(iocur_top->len, xfs_bmdr, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(XFS_DFORK_ASIZE(dip, mp), xfs_bmdr, 0));
	return bitize((int)((char *)pp - (char *)block));
}

/*ARGSUSED*/
int
bmroota_size(
	void			*obj,
	int			startoff,
	int			idx)
{
#ifndef NDEBUG
	xfs_bmdr_block_t	*block;
#endif
	xfs_dinode_t		*dip;

	assert(bitoffs(startoff) == 0);
	assert(obj == iocur_top->data);
	assert(idx == 0);
	dip = obj;
#ifndef NDEBUG
	block = (xfs_bmdr_block_t *)((char *)obj + byteize(startoff));
#endif
	assert(XFS_DFORK_Q(dip) && (char *)block == XFS_DFORK_APTR(dip));
	return bitize((int)XFS_DFORK_ASIZE(dip, mp));
}
#endif

static int
bmrootd_key_count(
	void			*obj,
	int			startoff)
{
	xfs_bmdr_block_t	*block;
#ifndef NDEBUG
	xfs_dinode_t		*dip = obj;
#endif

	assert(bitoffs(startoff) == 0);
	assert(obj == iocur_top->data);
	block = (xfs_bmdr_block_t *)((char *)obj + byteize(startoff));
	assert((char *)block == XFS_DFORK_DPTR(dip));
	assert(block->bb_level > 0);
	return block->bb_numrecs;
}

static int
bmrootd_key_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_bmdr_block_t	*block;
	/* REFERENCED */
	xfs_dinode_t		*dip;
	xfs_bmdr_key_t		*kp;

	assert(bitoffs(startoff) == 0);
	assert(obj == iocur_top->data);
	dip = obj;
	block = (xfs_bmdr_block_t *)((char *)obj + byteize(startoff));
	assert(block->bb_level > 0);
	kp = XFS_BTREE_KEY_ADDR(iocur_top->len, xfs_bmdr, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(XFS_DFORK_DSIZE(dip, mp), xfs_bmdr, 0));
	return bitize((int)((char *)kp - (char *)block));
}

static int
bmrootd_ptr_count(
	void			*obj,
	int			startoff)
{
	xfs_bmdr_block_t	*block;
#ifndef NDEBUG
	xfs_dinode_t		*dip = obj;
#endif

	assert(bitoffs(startoff) == 0);
	assert(obj == iocur_top->data);
	block = (xfs_bmdr_block_t *)((char *)obj + byteize(startoff));
	assert((char *)block == XFS_DFORK_DPTR(dip));
	assert(block->bb_level > 0);
	return block->bb_numrecs;
}

static int
bmrootd_ptr_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_bmdr_block_t	*block;
	/* REFERENCED */
	xfs_dinode_t		*dip;
	xfs_bmdr_ptr_t		*pp;

	assert(bitoffs(startoff) == 0);
	assert(obj == iocur_top->data);
	dip = obj;
	block = (xfs_bmdr_block_t *)((char *)obj + byteize(startoff));
	assert(block->bb_level > 0);
	pp = XFS_BTREE_PTR_ADDR(iocur_top->len, xfs_bmdr, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(XFS_DFORK_DSIZE(dip, mp), xfs_bmdr, 0));
	return bitize((int)((char *)pp - (char *)block));
}

/*ARGSUSED*/
int
bmrootd_size(
	void			*obj,
	int			startoff,
	int			idx)
{
	/* REFERENCED */
	xfs_dinode_t		*dip;

	assert(bitoffs(startoff) == 0);
	assert(obj == iocur_top->data);
	assert(idx == 0);
	dip = obj;
	return bitize((int)XFS_DFORK_DSIZE(dip, mp));
}
