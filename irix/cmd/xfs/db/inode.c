#ident "$Revision: 1.27 $"

#include "versions.h"
#include <sys/param.h>
#include <sys/uuid.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_btree.h>
#include <sys/fs/xfs_dir.h>
#if VERS >= V_62
#include <sys/fs/xfs_dir_sf.h>
#if VERS >= V_654
#include <sys/fs/xfs_dir2_sf.h>
#include <sys/fs/xfs_dir2.h>
#endif
#include <sys/fs/xfs_attr_sf.h>
#endif
#include <sys/fs/xfs_dinode.h>
#include "sim.h"
#include "command.h"
#include "data.h"
#include "type.h"
#include "faddr.h"
#include "fprint.h"
#include "field.h"
#include "inode.h"
#include "io.h"
#include "print.h"
#include "block.h"
#include "bit.h"
#include "output.h"
#include "mount.h"

#if VERS >= V_62
static int	inode_a_bmbt_count(void *obj, int startoff);
static int	inode_a_bmx_count(void *obj, int startoff);
static int	inode_a_count(void *obj, int startoff);
static int	inode_a_offset(void *obj, int startoff, int idx);
static int	inode_a_sfattr_count(void *obj, int startoff);
static int	inode_core_nlinkv2_count(void *obj, int startoff);
static int	inode_core_onlink_count(void *obj, int startoff);
static int	inode_core_projid_count(void *obj, int startoff);
#endif
static int	inode_core_nlinkv1_count(void *obj, int startoff);
static int	inode_f(int argc, char **argv);
static int	inode_u_bmbt_count(void *obj, int startoff);
static int	inode_u_bmx_count(void *obj, int startoff);
static int	inode_u_c_count(void *obj, int startoff);
static int	inode_u_dev_count(void *obj, int startoff);
static int	inode_u_muuid_count(void *obj, int startoff);
static int	inode_u_sfdir_count(void *obj, int startoff);
#if VERS >= V_654
static int	inode_u_sfdir2_count(void *obj, int startoff);
#endif
static int	inode_u_symlink_count(void *obj, int startoff);

static const cmdinfo_t	inode_cmd =
	{ "inode", NULL, inode_f, 0, 1, 1, "[inode#]",
	  "set current inode", NULL };

const field_t	inode_hfld[] = {
	{ "", FLDT_INODE, OI(0), C1, 0, TYP_NONE },
	{ NULL }
};

#define	OFF(f)	bitize(offsetof(xfs_dinode_t, di_ ## f))
const field_t	inode_flds[] = {
	{ "core", FLDT_DINODE_CORE, OI(OFF(core)), C1, 0, TYP_NONE },
	{ "next_unlinked", FLDT_AGINO, OI(OFF(next_unlinked)), C1, 0,
	  TYP_INODE },
	{ "u", FLDT_DINODE_U, OI(OFF(u)), C1, 0, TYP_NONE },
#if VERS >= V_62
	{ "a", FLDT_DINODE_A, inode_a_offset, inode_a_count,
	  FLD_COUNT|FLD_OFFSET, TYP_NONE },
#endif
	{ NULL }
};

#define	COFF(f)	bitize(offsetof(xfs_dinode_core_t, di_ ## f))
const field_t	inode_core_flds[] = {
	{ "magic", FLDT_UINT16X, OI(COFF(magic)), C1, 0, TYP_NONE },
	{ "mode", FLDT_UINT16O, OI(COFF(mode)), C1, 0, TYP_NONE },
	{ "version", FLDT_INT8D, OI(COFF(version)), C1, 0, TYP_NONE },
	{ "format", FLDT_DINODE_FMT, OI(COFF(format)), C1, 0, TYP_NONE },
#if VERS >= V_62
	{ "nlinkv1", FLDT_UINT16D, OI(COFF(onlink)), inode_core_nlinkv1_count,
	  FLD_COUNT, TYP_NONE },
	{ "nlinkv2", FLDT_UINT32D, OI(COFF(nlink)), inode_core_nlinkv2_count,
	  FLD_COUNT, TYP_NONE },
	{ "onlink", FLDT_UINT16D, OI(COFF(onlink)), inode_core_onlink_count,
	  FLD_COUNT, TYP_NONE },
	{ "projid", FLDT_UINT16D, OI(COFF(projid)), inode_core_projid_count,
	  FLD_COUNT, TYP_NONE },
#else
	{ "nlink", FLDT_UINT16D, OI(COFF(nlink)), inode_core_nlinkv1_count,
	  FLD_COUNT, TYP_NONE },
#endif
	{ "uid", FLDT_UINT32D, OI(COFF(uid)), C1, 0, TYP_NONE },
	{ "gid", FLDT_UINT32D, OI(COFF(gid)), C1, 0, TYP_NONE },
	{ "atime", FLDT_TIMESTAMP, OI(COFF(atime)), C1, 0, TYP_NONE },
	{ "mtime", FLDT_TIMESTAMP, OI(COFF(mtime)), C1, 0, TYP_NONE },
	{ "ctime", FLDT_TIMESTAMP, OI(COFF(ctime)), C1, 0, TYP_NONE },
	{ "size", FLDT_FSIZE, OI(COFF(size)), C1, 0, TYP_NONE },
	{ "nblocks", FLDT_DRFSBNO, OI(COFF(nblocks)), C1, 0, TYP_NONE },
	{ "extsize", FLDT_EXTLEN, OI(COFF(extsize)), C1, 0, TYP_NONE },
	{ "nextents", FLDT_EXTNUM, OI(COFF(nextents)), C1, 0, TYP_NONE },
#if VERS >= V_62
	{ "naextents", FLDT_AEXTNUM, OI(COFF(anextents)), C1, 0, TYP_NONE },
#else
	{ "naextents", FLDT_AEXTNUM, OI(COFF(nattrextents)), C1, 0, TYP_NONE },
#endif
	{ "forkoff", FLDT_UINT8D, OI(COFF(forkoff)), C1, 0, TYP_NONE },
#if VERS >= V_62
	{ "aformat", FLDT_DINODE_FMT, OI(COFF(aformat)), C1, 0, TYP_NONE },
#endif
	{ "dmevmask", FLDT_UINT32X, OI(COFF(dmevmask)), C1, 0, TYP_NONE },
	{ "dmstate", FLDT_UINT16D, OI(COFF(dmstate)), C1, 0, TYP_NONE },
	{ "flags", FLDT_UINT16X, OI(COFF(flags)), C1, FLD_SKIPALL, TYP_NONE },
#if VERS >= V_61
	{ "newrtbm", FLDT_UINT1,
	  OI(COFF(flags) + bitsz(__uint16_t) - XFS_DIFLAG_NEWRTBM_BIT - 1), C1,
	  0, TYP_NONE },
	{ "prealloc", FLDT_UINT1,
	  OI(COFF(flags) + bitsz(__uint16_t) - XFS_DIFLAG_PREALLOC_BIT - 1), C1,
	  0, TYP_NONE },
#endif
	{ "realtime", FLDT_UINT1,
	  OI(COFF(flags) + bitsz(__uint16_t) - XFS_DIFLAG_REALTIME_BIT - 1), C1,
	  0, TYP_NONE },
	{ "gen", FLDT_UINT32D, OI(COFF(gen)), C1, 0, TYP_NONE },
	{ NULL }
};

#define	TOFF(f)	bitize(offsetof(xfs_timestamp_t, t_ ## f))
const field_t	timestamp_flds[] = {
	{ "sec", FLDT_TIME, OI(TOFF(sec)), C1, 0, TYP_NONE },
	{ "nsec", FLDT_NSEC, OI(TOFF(nsec)), C1, 0, TYP_NONE },
	{ NULL }
};

const field_t	inode_u_flds[] = {
	{ "bmbt", FLDT_BMROOTD, 0, inode_u_bmbt_count, FLD_COUNT, TYP_NONE },
	{ "bmx", FLDT_BMAPBTDREC, 0, inode_u_bmx_count, FLD_ARRAY|FLD_COUNT,
	  TYP_NONE },
	{ "c", FLDT_CHARNS, 0, inode_u_c_count, FLD_COUNT, TYP_NONE },
	{ "dev", FLDT_DEV, 0, inode_u_dev_count, FLD_COUNT, TYP_NONE },
	{ "muuid", FLDT_UUID, 0, inode_u_muuid_count, FLD_COUNT, TYP_NONE },
	{ "sfdir", FLDT_DIRSHORT, 0, inode_u_sfdir_count, FLD_COUNT, TYP_NONE },
#if VERS >= V_654
	{ "sfdir2", FLDT_DIR2SF, 0, inode_u_sfdir2_count, FLD_COUNT, TYP_NONE },
#endif
	{ "symlink", FLDT_CHARNS, 0, inode_u_symlink_count, FLD_COUNT,
	  TYP_NONE },
	{ NULL }
};

#if VERS >= V_62
const field_t	inode_a_flds[] = {
	{ "bmbt", FLDT_BMROOTA, 0, inode_a_bmbt_count, FLD_COUNT, TYP_NONE },
	{ "bmx", FLDT_BMAPBTAREC, 0, inode_a_bmx_count, FLD_ARRAY|FLD_COUNT,
	  TYP_NONE },
	{ "sfattr", FLDT_ATTRSHORT, 0, inode_a_sfattr_count, FLD_COUNT,
	  TYP_NONE },
	{ NULL }
};
#endif

static const char	*dinode_fmt_name[] =
	{ "dev", "local", "extents", "btree", "uuid" };
static const int	dinode_fmt_name_size =
	sizeof(dinode_fmt_name) / sizeof(dinode_fmt_name[0]);

/*ARGSUSED*/
int
fp_dinode_fmt(
	void			*obj,
	int			bit,
	int			count,
	char			*fmtstr,
	int			size,
	int			arg,
	int			base,
	int			array)
{
	int			bitpos;
	xfs_dinode_fmt_t	f;
	int			i;

	for (i = 0, bitpos = bit; i < count; i++, bitpos += size) {
		f = (xfs_dinode_fmt_t)getbitval(obj, bitpos, size, BVSIGNED);
		if (array)
			dbprintf("%d:", i + base);
		if (f < 0 || f >= dinode_fmt_name_size)
			dbprintf("%d", (int)f);
		else
			dbprintf("%d (%s)", (int)f, dinode_fmt_name[(int)f]);
		if (i < count - 1)
			dbprintf(" ");
	}
	return 1;
}

#if VERS >= V_62
/*ARGSUSED*/
static int
inode_a_bmbt_count(
	void		*obj,
	int		startoff)
{
	xfs_dinode_t	*dip;

	assert(bitoffs(startoff) == 0);
	assert(obj == iocur_top->data);
	dip = obj;
	if (!XFS_DFORK_Q(dip))
		return 0;
	assert((char *)XFS_DFORK_APTR(dip) - (char *)dip == byteize(startoff));
	return dip->di_core.di_aformat == XFS_DINODE_FMT_BTREE;
}

/*ARGSUSED*/
static int
inode_a_bmx_count(
	void		*obj,
	int		startoff)
{
	xfs_dinode_t	*dip;

	assert(bitoffs(startoff) == 0);
	assert(obj == iocur_top->data);
	dip = obj;
	if (!XFS_DFORK_Q(dip))
		return 0;
	assert((char *)XFS_DFORK_APTR(dip) - (char *)dip == byteize(startoff));
	return dip->di_core.di_aformat == XFS_DINODE_FMT_EXTENTS ?
		dip->di_core.di_anextents : 0;
}

/*ARGSUSED*/
static int
inode_a_count(
	void		*obj,
	int		startoff)
{
	xfs_dinode_t	*dip;

	assert(startoff == 0);
	dip = obj;
	return XFS_DFORK_Q(dip);
}

/*ARGSUSED*/
static int
inode_a_offset(
	void		*obj,
	int		startoff,
	int		idx)
{
	xfs_dinode_t	*dip;

	assert(startoff == 0);
	assert(idx == 0);
	dip = obj;
	assert(XFS_DFORK_Q(dip));
	return bitize((int)((char *)XFS_DFORK_APTR(dip) - (char *)dip));
}

/*ARGSUSED*/
static int
inode_a_sfattr_count(
	void		*obj,
	int		startoff)
{
	xfs_dinode_t	*dip;

	assert(bitoffs(startoff) == 0);
	assert(obj == iocur_top->data);
	dip = obj;
	if (!XFS_DFORK_Q(dip))
		return 0;
	assert((char *)XFS_DFORK_APTR(dip) - (char *)dip == byteize(startoff));
	return dip->di_core.di_aformat == XFS_DINODE_FMT_LOCAL;
}

/*ARGSUSED*/
int
inode_a_size(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_attr_shortform_t	*asf;
	xfs_dinode_t		*dip;

	assert(startoff == 0);
	assert(idx == 0);
	dip = obj;
	switch (dip->di_core.di_aformat) {
	case XFS_DINODE_FMT_LOCAL:
		asf = (xfs_attr_shortform_t *)XFS_DFORK_APTR(dip);
		return bitize((int)asf->hdr.totsize);
	case XFS_DINODE_FMT_EXTENTS:
		return (int)(dip->di_core.di_anextents * bitsz(xfs_bmbt_rec_t));
	case XFS_DINODE_FMT_BTREE:
		return bitize((int)XFS_DFORK_ASIZE(dip, mp));
	default:
		return 0;
	}
}
#endif

/*ARGSUSED*/
static int
inode_core_nlinkv1_count(
	void			*obj,
	int			startoff)
{
	xfs_dinode_core_t	*dic;

	assert(startoff == 0);
	assert(obj == iocur_top->data);
	dic = obj;
	return dic->di_version == XFS_DINODE_VERSION_1;
}

#if VERS >= V_62
/*ARGSUSED*/
static int
inode_core_nlinkv2_count(
	void			*obj,
	int			startoff)
{
	xfs_dinode_core_t	*dic;

	assert(startoff == 0);
	assert(obj == iocur_top->data);
	dic = obj;
	return dic->di_version == XFS_DINODE_VERSION_2;
}

/*ARGSUSED*/
static int
inode_core_onlink_count(
	void			*obj,
	int			startoff)
{
	xfs_dinode_core_t	*dic;

	assert(startoff == 0);
	assert(obj == iocur_top->data);
	dic = obj;
	return dic->di_version == XFS_DINODE_VERSION_2;
}

/*ARGSUSED*/
static int
inode_core_projid_count(
	void			*obj,
	int			startoff)
{
	xfs_dinode_core_t	*dic;

	assert(startoff == 0);
	assert(obj == iocur_top->data);
	dic = obj;
	return dic->di_version == XFS_DINODE_VERSION_2;
}
#endif

static int
inode_f(
	int		argc,
	char		**argv)
{
	xfs_ino_t	ino;
	char		*p;

	if (argc == 1) {
		ino = strtoull(argv[0], &p, 0);
		if (*p != '\0') {
			dbprintf("bad value for inode number %s\n", argv[0]);
			return 0;
		}
		set_cur_inode(ino);
	} else if (iocur_top->ino == NULLFSINO)
		dbprintf("no current inode\n");
	else
		dbprintf("current inode number is %lld\n", iocur_top->ino);
	return 0;
}

void
inode_init(void)
{
	add_command(&inode_cmd);
}

typnm_t
inode_next_type(void)
{
	switch (iocur_top->mode & IFMT) {
	case IFDIR:
#if VERS >= V_654
		return XFS_DIR_IS_V2(mp) ? TYP_DIR2 : TYP_DIR;
#else
		return TYP_DIR;
#endif
	case IFLNK:
		return TYP_SYMLINK;
	case IFREG:
		if (iocur_top->ino == mp->m_sb.sb_rbmino)
			return TYP_RTBITMAP;
		else if (iocur_top->ino == mp->m_sb.sb_rsumino)
			return TYP_RTSUMMARY;
#if VERS >= V_62
		else if (iocur_top->ino == mp->m_sb.sb_uquotino ||
			 iocur_top->ino == mp->m_sb.sb_pquotino)
			return TYP_DQBLK;
#endif
		else
			return TYP_DATA;
	default:
		return TYP_NONE;
	}
}

/*ARGSUSED*/
int
inode_size(
	void	*obj,
	int	startoff,
	int	idx)
{
	return bitize(mp->m_sb.sb_inodesize);
}

/*ARGSUSED*/
static int
inode_u_bmbt_count(
	void		*obj,
	int		startoff)
{
	xfs_dinode_t	*dip;

	assert(bitoffs(startoff) == 0);
	assert(obj == iocur_top->data);
	dip = obj;
	assert((char *)&dip->di_u - (char *)dip == byteize(startoff));
	return dip->di_core.di_format == XFS_DINODE_FMT_BTREE;
}

/*ARGSUSED*/
static int
inode_u_bmx_count(
	void		*obj,
	int		startoff)
{
	xfs_dinode_t	*dip;

	assert(bitoffs(startoff) == 0);
	assert(obj == iocur_top->data);
	dip = obj;
	assert((char *)&dip->di_u - (char *)dip == byteize(startoff));
	return dip->di_core.di_format == XFS_DINODE_FMT_EXTENTS ?
		dip->di_core.di_nextents : 0;
}

/*ARGSUSED*/
static int
inode_u_c_count(
	void		*obj,
	int		startoff)
{
	xfs_dinode_t	*dip;

	assert(bitoffs(startoff) == 0);
	assert(obj == iocur_top->data);
	dip = obj;
	assert((char *)&dip->di_u - (char *)dip == byteize(startoff));
	return dip->di_core.di_format == XFS_DINODE_FMT_LOCAL &&
	       (dip->di_core.di_mode & IFMT) == IFREG ?
		(int)dip->di_core.di_size : 0;
}

/*ARGSUSED*/
static int
inode_u_dev_count(
	void		*obj,
	int		startoff)
{
	xfs_dinode_t	*dip;

	assert(bitoffs(startoff) == 0);
	assert(obj == iocur_top->data);
	dip = obj;
	assert((char *)&dip->di_u - (char *)dip == byteize(startoff));
	return dip->di_core.di_format == XFS_DINODE_FMT_DEV;
}

/*ARGSUSED*/
static int
inode_u_muuid_count(
	void		*obj,
	int		startoff)
{
	xfs_dinode_t	*dip;

	assert(bitoffs(startoff) == 0);
	assert(obj == iocur_top->data);
	dip = obj;
	assert((char *)&dip->di_u - (char *)dip == byteize(startoff));
	return dip->di_core.di_format == XFS_DINODE_FMT_UUID;
}

/*ARGSUSED*/
static int
inode_u_sfdir_count(
	void		*obj,
	int		startoff)
{
	xfs_dinode_t	*dip;

	assert(bitoffs(startoff) == 0);
	assert(obj == iocur_top->data);
	dip = obj;
	assert((char *)&dip->di_u - (char *)dip == byteize(startoff));
	return dip->di_core.di_format == XFS_DINODE_FMT_LOCAL &&
	       (dip->di_core.di_mode & IFMT) == IFDIR
#if VERS >= V_654
	       && XFS_DIR_IS_V1(mp)
#endif
	       ;
}

#if VERS >= V_654
/*ARGSUSED*/
static int
inode_u_sfdir2_count(
	void		*obj,
	int		startoff)
{
	xfs_dinode_t	*dip;

	assert(bitoffs(startoff) == 0);
	assert(obj == iocur_top->data);
	dip = obj;
	assert((char *)&dip->di_u - (char *)dip == byteize(startoff));
	return dip->di_core.di_format == XFS_DINODE_FMT_LOCAL &&
	       (dip->di_core.di_mode & IFMT) == IFDIR &&
	       XFS_DIR_IS_V2(mp);
}
#endif

/*ARGSUSED*/
int
inode_u_size(
	void		*obj,
	int		startoff,
	int		idx)
{
	xfs_dinode_t	*dip;

	assert(startoff == 0);
	assert(idx == 0);
	dip = obj;
	switch (dip->di_core.di_format) {
	case XFS_DINODE_FMT_DEV:
		return bitsz(dev_t);
	case XFS_DINODE_FMT_LOCAL:
		return bitize((int)dip->di_core.di_size);
	case XFS_DINODE_FMT_EXTENTS:
		return (int)(dip->di_core.di_nextents * bitsz(xfs_bmbt_rec_t));
	case XFS_DINODE_FMT_BTREE:
		return bitize((int)XFS_DFORK_DSIZE(dip, mp));
	case XFS_DINODE_FMT_UUID:
		return bitsz(uuid_t);
	default:
		return 0;
	}
}

/*ARGSUSED*/
static int
inode_u_symlink_count(
	void		*obj,
	int		startoff)
{
	xfs_dinode_t	*dip;

	assert(bitoffs(startoff) == 0);
	assert(obj == iocur_top->data);
	dip = obj;
	assert((char *)&dip->di_u - (char *)dip == byteize(startoff));
	return dip->di_core.di_format == XFS_DINODE_FMT_LOCAL &&
	       (dip->di_core.di_mode & IFMT) == IFLNK ?
		(int)dip->di_core.di_size : 0;
}

void
set_cur_inode(
	xfs_ino_t	ino)
{
	xfs_agblock_t	agbno;
	xfs_agino_t	agino;
	xfs_agnumber_t	agno;
	xfs_dinode_t	*dip;
	int		offset;

	agno = XFS_INO_TO_AGNO(mp, ino);
	agino = XFS_INO_TO_AGINO(mp, ino);
	agbno = XFS_AGINO_TO_AGBNO(mp, agino);
	offset = XFS_AGINO_TO_OFFSET(mp, agino);
	if (agno >= mp->m_sb.sb_agcount || agbno >= mp->m_sb.sb_agblocks ||
	    offset >= mp->m_sb.sb_inopblock ||
	    XFS_AGINO_TO_INO(mp, agno, agino) != ino) {
		dbprintf("bad inode number %lld\n", ino);
		return;
	}
	cur_agno = agno;
	/*
	 * First set_cur to the block with the inode
	 * then use off_cur to get the right part of the buffer.
	 */
	assert(typtab[TYP_INODE].typnm == TYP_INODE);

	/* ingore ring update here, do it explicitly below */
	set_cur(&typtab[TYP_INODE], XFS_AGB_TO_DADDR(mp, agno, agbno),
		blkbb, DB_RING_IGN, NULL);
	off_cur(offset << mp->m_sb.sb_inodelog, mp->m_sb.sb_inodesize);
	dip = iocur_top->data;
	iocur_top->ino = ino;
	iocur_top->mode = dip->di_core.di_mode;
	if ((iocur_top->mode & IFMT) == IFDIR)
		iocur_top->dirino = ino;

	/* track updated info in ring */
	ring_add();
}
