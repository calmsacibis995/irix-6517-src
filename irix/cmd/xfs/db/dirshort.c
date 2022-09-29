#ident "$Revision: 1.12 $"

#include "versions.h"
#include <sys/types.h>
#include <sys/uuid.h>
#include <assert.h>
#include <stddef.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_bmap_btree.h>
#if VERS >= V_62
#include <sys/fs/xfs_dir_sf.h>
#else
#include <sys/fs/xfs_dir.h>
#endif
#include "type.h"
#include "faddr.h"
#include "fprint.h"
#include "field.h"
#include "bit.h"
#include "dirshort.h"

static int	dir_sf_entry_name_count(void *obj, int startoff);
static int	dir_shortform_list_count(void *obj, int startoff);
static int	dir_shortform_list_offset(void *obj, int startoff, int idx);

#define	OFF(f)	bitize(offsetof(xfs_dir_shortform_t, f))
const field_t	dir_shortform_flds[] = {
	{ "hdr", FLDT_DIR_SF_HDR, OI(OFF(hdr)), C1, 0, TYP_NONE },
	{ "list", FLDT_DIR_SF_ENTRY, dir_shortform_list_offset,
	  dir_shortform_list_count, FLD_ARRAY|FLD_COUNT|FLD_OFFSET, TYP_NONE },
	{ NULL }
};

#define	HOFF(f)	bitize(offsetof(xfs_dir_sf_hdr_t, f))
const field_t	dir_sf_hdr_flds[] = {
	{ "parent", FLDT_DIR_INO, OI(HOFF(parent)), C1, 0, TYP_INODE },
	{ "count", FLDT_UINT8D, OI(HOFF(count)), C1, 0, TYP_NONE },
	{ NULL }
};

#define	EOFF(f)	bitize(offsetof(xfs_dir_sf_entry_t, f))
const field_t	dir_sf_entry_flds[] = {
	{ "inumber", FLDT_DIR_INO, OI(EOFF(inumber)), C1, 0, TYP_INODE },
	{ "namelen", FLDT_UINT8D, OI(EOFF(namelen)), C1, 0, TYP_NONE },
	{ "name", FLDT_CHARNS, OI(EOFF(name)), dir_sf_entry_name_count,
	  FLD_COUNT, TYP_NONE },
	{ NULL }
};

static int
dir_sf_entry_name_count(
	void			*obj,
	int			startoff)
{
	xfs_dir_sf_entry_t	*e;
	
	assert(bitoffs(startoff) == 0);
	e = (xfs_dir_sf_entry_t *)((char *)obj + byteize(startoff));
	return e->namelen;
}

int
dir_sf_entry_size(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dir_sf_entry_t	*e;
	int			i;
	xfs_dir_shortform_t	*sf;

	assert(bitoffs(startoff) == 0);
	sf = (xfs_dir_shortform_t *)((char *)obj + byteize(startoff));
	e = &sf->list[0];
	for (i = 0; i < idx; i++)
		e = XFS_DIR_SF_NEXTENTRY(e);
	return bitize((int)XFS_DIR_SF_ENTSIZE_BYENTRY(e));
}

static int
dir_shortform_list_count(
	void			*obj,
	int			startoff)
{
	xfs_dir_shortform_t	*sf;

	assert(bitoffs(startoff) == 0);
	sf = (xfs_dir_shortform_t *)((char *)obj + byteize(startoff));
	return sf->hdr.count;
}

static int
dir_shortform_list_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dir_sf_entry_t	*e;
	int			i;
	xfs_dir_shortform_t	*sf;

	assert(bitoffs(startoff) == 0);
	sf = (xfs_dir_shortform_t *)((char *)obj + byteize(startoff));
	e = &sf->list[0];
	for (i = 0; i < idx; i++)
		e = XFS_DIR_SF_NEXTENTRY(e);
	return bitize((int)((char *)e - (char *)sf));
}

/*ARGSUSED*/
int
dirshort_size(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dir_sf_entry_t	*e;
	int			i;
	xfs_dir_shortform_t	*sf;

	assert(bitoffs(startoff) == 0);
	assert(idx == 0);
	sf = (xfs_dir_shortform_t *)((char *)obj + byteize(startoff));
	e = &sf->list[0];
	for (i = 0; i < sf->hdr.count; i++)
		e = XFS_DIR_SF_NEXTENTRY(e);
	return bitize((int)((char *)e - (char *)sf));
}
