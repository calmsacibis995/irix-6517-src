#ident "$Revision: 1.18 $"

#include "versions.h"
#include <sys/param.h>
#include <sys/uuid.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
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
#if VERS >= V_62
#include <sys/fs/xfs_da_btree.h>
#include <sys/fs/xfs_dir_leaf.h>
#endif
#include "sim.h"
#include "bit.h"
#include "type.h"
#include "faddr.h"
#include "fprint.h"
#include "field.h"
#include "dir.h"
#include "io.h"
#include "data.h"
#include "mount.h"

static int	dir_leaf_entries_count(void *obj, int startoff);
static int	dir_leaf_hdr_count(void *obj, int startoff);
static int	dir_leaf_name_count(void *obj, int startoff);
static int	dir_leaf_namelist_count(void *obj, int startoff);
static int	dir_leaf_namelist_offset(void *obj, int startoff, int idx);
static int	dir_node_btree_count(void *obj, int startoff);
static int	dir_node_hdr_count(void *obj, int startoff);

const field_t	dir_hfld[] = {
	{ "", FLDT_DIR, OI(0), C1, 0, TYP_NONE },
	{ NULL }
};

#define	LOFF(f)	bitize(offsetof(xfs_dir_leafblock_t, f))
#if VERS >= V_62
#define	NOFF(f)	bitize(offsetof(xfs_da_intnode_t, f))
#else
#define	NOFF(f)	bitize(offsetof(xfs_dir_intnode_t, f))
#endif
const field_t	dir_flds[] = {
	{ "lhdr", FLDT_DIR_LEAF_HDR, OI(LOFF(hdr)), dir_leaf_hdr_count,
	  FLD_COUNT, TYP_NONE },
	{ "nhdr", FLDT_DIR_NODE_HDR, OI(NOFF(hdr)), dir_node_hdr_count,
	  FLD_COUNT, TYP_NONE },
#if VERS >= V_62
	{ "entries", FLDT_DIR_LEAF_ENTRY, OI(LOFF(entries)),
	  dir_leaf_entries_count, FLD_ARRAY|FLD_COUNT, TYP_NONE },
#else
	{ "leaves", FLDT_DIR_LEAF_ENTRY, OI(LOFF(leaves)),
	  dir_leaf_entries_count, FLD_ARRAY|FLD_COUNT, TYP_NONE },
#endif
	{ "btree", FLDT_DIR_NODE_ENTRY, OI(NOFF(btree)),
	  dir_node_btree_count, FLD_ARRAY|FLD_COUNT, TYP_NONE },
	{ "namelist", FLDT_DIR_LEAF_NAME, dir_leaf_namelist_offset,
	  dir_leaf_namelist_count, FLD_ARRAY|FLD_OFFSET|FLD_COUNT, TYP_NONE },
	{ NULL }
};

#if VERS >= V_62
#define	BOFF(f)	bitize(offsetof(xfs_da_blkinfo_t, f))
#else
#define	BOFF(f)	bitize(offsetof(xfs_dir_blkinfo_t, f))
#endif
const field_t	dir_blkinfo_flds[] = {
	{ "forw", FLDT_DIRBLOCK, OI(BOFF(forw)), C1, 0, TYP_INODATA },
	{ "back", FLDT_DIRBLOCK, OI(BOFF(back)), C1, 0, TYP_INODATA },
	{ "magic", FLDT_UINT16X, OI(BOFF(magic)), C1, 0, TYP_NONE },
#if VERS >= V_65
	{ "pad", FLDT_UINT16X, OI(BOFF(pad)), C1, FLD_SKIPALL, TYP_NONE },
#endif
	{ NULL }
};

#define	LEOFF(f)	bitize(offsetof(xfs_dir_leaf_entry_t, f))
const field_t	dir_leaf_entry_flds[] = {
	{ "hashval", FLDT_UINT32X, OI(LEOFF(hashval)), C1, 0, TYP_NONE },
	{ "nameidx", FLDT_UINT16D, OI(LEOFF(nameidx)), C1, 0, TYP_NONE },
	{ "namelen", FLDT_UINT8D, OI(LEOFF(namelen)), C1, 0, TYP_NONE },
	{ "pad2", FLDT_UINT8X, OI(LEOFF(pad2)), C1, FLD_SKIPALL, TYP_NONE },
	{ NULL }
};

#define	LHOFF(f)	bitize(offsetof(xfs_dir_leaf_hdr_t, f))
const field_t	dir_leaf_hdr_flds[] = {
	{ "info", FLDT_DIR_BLKINFO, OI(LHOFF(info)), C1, 0, TYP_NONE },
	{ "count", FLDT_UINT16D, OI(LHOFF(count)), C1, 0, TYP_NONE },
	{ "namebytes", FLDT_UINT16D, OI(LHOFF(namebytes)), C1, 0, TYP_NONE },
	{ "firstused", FLDT_UINT16D, OI(LHOFF(firstused)), C1, 0, TYP_NONE },
	{ "holes", FLDT_UINT8D, OI(LHOFF(holes)), C1, 0, TYP_NONE },
	{ "pad1", FLDT_UINT8X, OI(LHOFF(pad1)), C1, FLD_SKIPALL, TYP_NONE },
	{ "freemap", FLDT_DIR_LEAF_MAP, OI(LHOFF(freemap)),
	  CI(XFS_DIR_LEAF_MAPSIZE), FLD_ARRAY, TYP_NONE },
	{ NULL }
};

#define	LMOFF(f)	bitize(offsetof(xfs_dir_leaf_map_t, f))
const field_t	dir_leaf_map_flds[] = {
	{ "base", FLDT_UINT16D, OI(LMOFF(base)), C1, 0, TYP_NONE },
	{ "size", FLDT_UINT16D, OI(LMOFF(size)), C1, 0, TYP_NONE },
	{ NULL }
};

#define	LNOFF(f)	bitize(offsetof(xfs_dir_leaf_name_t, f))
const field_t	dir_leaf_name_flds[] = {
	{ "inumber", FLDT_DIR_INO, OI(LNOFF(inumber)), C1, 0, TYP_INODE },
	{ "name", FLDT_CHARNS, OI(LNOFF(name)), dir_leaf_name_count, FLD_COUNT,
	  TYP_NONE },
	{ NULL }
};

#if VERS >= V_62
#define	EOFF(f)	bitize(offsetof(xfs_da_node_entry_t, f))
#else
#define	EOFF(f)	bitize(offsetof(xfs_dir_node_entry_t, f))
#endif
const field_t	dir_node_entry_flds[] = {
	{ "hashval", FLDT_UINT32X, OI(EOFF(hashval)), C1, 0, TYP_NONE },
	{ "before", FLDT_DIRBLOCK, OI(EOFF(before)), C1, 0, TYP_INODATA },
	{ NULL }
};

#if VERS >= V_62
#define	HOFF(f)	bitize(offsetof(xfs_da_node_hdr_t, f))
#else
#define	HOFF(f)	bitize(offsetof(xfs_dir_node_hdr_t, f))
#endif
const field_t	dir_node_hdr_flds[] = {
	{ "info", FLDT_DIR_BLKINFO, OI(HOFF(info)), C1, 0, TYP_NONE },
	{ "count", FLDT_UINT16D, OI(HOFF(count)), C1, 0, TYP_NONE },
	{ "level", FLDT_UINT16D, OI(HOFF(level)), C1, 0, TYP_NONE },
	{ NULL }
};

/*ARGSUSED*/
static int
dir_leaf_entries_count(
	void			*obj,
	int			startoff)
{
	xfs_dir_leafblock_t	*block;
	
	assert(startoff == 0);
	block = obj;
	if (block->hdr.info.magic != XFS_DIR_LEAF_MAGIC)
		return 0;
	return block->hdr.count;
}

/*ARGSUSED*/
static int
dir_leaf_hdr_count(
	void			*obj,
	int			startoff)
{
	xfs_dir_leafblock_t	*block;
	
	assert(startoff == 0);
	block = obj;
	return block->hdr.info.magic == XFS_DIR_LEAF_MAGIC;
}

static int
dir_leaf_name_count(
	void			*obj,
	int			startoff)
{
	xfs_dir_leafblock_t	*block;
	xfs_dir_leaf_entry_t	*e;
	int			i;
	int			off;

	assert(bitoffs(startoff) == 0);
	off = byteize(startoff);
	block = obj;
	if (block->hdr.info.magic != XFS_DIR_LEAF_MAGIC)
		return 0;
	for (i = 0; i < block->hdr.count; i++) {
#if VERS >= V_62
		e = &block->entries[i];
#else
		e = &block->leaves[i];
#endif
		if (e->nameidx == off)
			return e->namelen;
	}
	return 0;
}

/*ARGSUSED*/
int
dir_leaf_name_size(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dir_leafblock_t	*block;
	xfs_dir_leaf_entry_t	*e;

	assert(startoff == 0);
	block = obj;
	if (block->hdr.info.magic != XFS_DIR_LEAF_MAGIC)
		return 0;
#if VERS >= V_62
	e = &block->entries[idx];
#else
	e = &block->leaves[idx];
#endif
	return bitize((int)XFS_DIR_LEAF_ENTSIZE_BYENTRY(e));
}

/*ARGSUSED*/
static int
dir_leaf_namelist_count(
	void			*obj,
	int			startoff)
{
	xfs_dir_leafblock_t	*block;

	assert(startoff == 0);
	block = obj;
	if (block->hdr.info.magic != XFS_DIR_LEAF_MAGIC)
		return 0;
	return block->hdr.count;
}

/*ARGSUSED*/
static int
dir_leaf_namelist_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dir_leafblock_t	*block;
	xfs_dir_leaf_entry_t	*e;

	assert(startoff == 0);
	block = obj;
#if VERS >= V_62
	e = &block->entries[idx];
#else
	e = &block->leaves[idx];
#endif
	return bitize(e->nameidx);
}

/*ARGSUSED*/
static int
dir_node_btree_count(
	void			*obj,
	int			startoff)
{
#if VERS >= V_62
	xfs_da_intnode_t	*block;
#else
	xfs_dir_intnode_t	*block;
#endif

	assert(startoff == 0);		/* this is a base structure */
	block = obj;
#if VERS >= V_62
	if (block->hdr.info.magic != XFS_DA_NODE_MAGIC)
#else
	if (block->hdr.info.magic != XFS_DIR_NODE_MAGIC)
#endif
		return 0;
	return block->hdr.count;
}

/*ARGSUSED*/
static int
dir_node_hdr_count(
	void			*obj,
	int			startoff)
{
#if VERS >= V_62
	xfs_da_intnode_t	*block;
#else
	xfs_dir_intnode_t	*block;
#endif

	assert(startoff == 0);
	block = obj;
#if VERS >= V_62
	return block->hdr.info.magic == XFS_DA_NODE_MAGIC;
#else
	return block->hdr.info.magic == XFS_DIR_NODE_MAGIC;
#endif
}

/*ARGSUSED*/
int
dir_size(
	void	*obj,
	int	startoff,
	int	idx)
{
	return bitize(mp->m_sb.sb_blocksize);
}
