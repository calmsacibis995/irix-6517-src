#ident "$Revision: 1.139 $"

/* 
 * Mkfs for xfs.
 */

#define _KERNEL	1
#include <sys/param.h>
#undef _KERNEL
#include <sys/var.h>
#include <sys/time.h>
#define _KERNEL 1
#include <sys/buf.h>
#include <sys/uuid.h>
#include <sys/sysmacros.h>
#undef _KERNEL
#include <sys/vnode.h>
#include <sys/cred.h>
#include <sys/sysmacros.h>
#include <sys/mman.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/dirent.h>
#include <stdio.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <diskinfo.h>
#include <bstring.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <mountinfo.h>
#include <sys/fs/xfs_macros.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_dir.h>
#include <sys/fs/xfs_dir2.h>
#include <sys/fs/xfs_mount.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_btree.h>
#include <sys/fs/xfs_alloc.h>
#include <sys/fs/xfs_ialloc.h>
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#include <sys/fs/xfs_dir2_sf.h>
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_inode_item.h>
#include <sys/fs/xfs_inode.h>
#include <sys/fs/xfs_bmap.h>
#include <sys/fs/xfs_rtalloc.h>
#include <sys/fs/xfs_da_btree.h>
#include <sys/fs/xfs_dir_leaf.h>
#include <sys/fs/xfs_fsops.h>
#include <sys/fs/xfs_bit.h>
#include <sys/xlv_base.h>
#include "sim.h"
#include "xfs_mkfs.h"
#include "maxtrres.h"

/*
 * Prototypes for internal functions.
 */

STATIC void
conflict(
	char	opt,
	char	*tab[],
	int	oldidx,
	int	newidx);

STATIC long long
cvtnum(
	int	blocksize,
	char	*s);

STATIC void
fail(
	char	*msg,
	int	i);

STATIC long
getnum(
	char	**pp);

STATIC void
getres(
	xfs_trans_t	*tp,
	uint		blocks);

STATIC char *
getstr(
	char	**pp);

STATIC int
ialloc(
	xfs_trans_t	**tp,
	xfs_inode_t	*pip,
	mode_t		mode,
	ushort		nlink,
	dev_t		rdev,
	cred_t		*cr,
	xfs_inode_t	**ipp);

STATIC void
illegal(
	char	*value,
	char	*opt);

STATIC int
ispow2(
	unsigned int	i);

STATIC unsigned int
log2_roundup(
	unsigned int	i);

STATIC int
max_trans_res(
	xfs_mount_t	*mp);

STATIC int
newfile(
	xfs_trans_t	*tp,
	xfs_inode_t	*ip,
	xfs_bmap_free_t	*flist,
	xfs_fsblock_t	*first,
	int		dolocal,
	int		logit,
	char		*buf,
	int		len);

STATIC char *
newregfile(
	char		**pp,
	int		*len);

STATIC void
parseproto(
	xfs_mount_t	*mp,
	xfs_inode_t	*pip,
	char		**pp,
	char		*name);

STATIC buf_t*
readbuf(
	dev_t	dev,
	daddr_t	blkno,
	int	len);

STATIC void
reqval(
	char	opt,
	char	*tab[],
	int	idx);

STATIC void
res_failed(
	int	err);

STATIC void
respec(
	char	opt,
	char	*tab[],
	int	idx);

STATIC void
rtinit(
	xfs_mount_t	*mp);

STATIC char *
setup_proto(
	char	*fname);

STATIC void
unknown(
	char	opt,
	char	*s);

STATIC void
usage(void);

STATIC void
writebuf(
	buf_t	*bp);

/*
 * option tables for getsubopt calls
 */
char	*bopts[] = {
#define	B_LOG		0
	"log",
#define	B_SIZE		1
	"size",
	NULL
};

char	*dopts[] = {
#define	D_AGCOUNT	0
	"agcount",
#define	D_FILE		1
	"file",
#define	D_NAME		2
	"name",
#define	D_SIZE		3
	"size",
#define D_SUNIT		4
	"sunit",
#define D_SWIDTH	5
	"swidth",
#define D_UNWRITTEN	6
	"unwritten",
	NULL
};

char	*iopts[] = {
#define	I_ALIGN		0
	"align",
#define	I_LOG		1
	"log",
#define	I_MAXPCT	2
	"maxpct",
#define	I_PERBLOCK	3
	"perblock",
#define	I_SIZE		4
	"size",
	NULL
};

char	*lopts[] = {
#define	L_AGNUM		0
	"agnum",
#define	L_INTERNAL	1
	"internal",
#define	L_SIZE		2
	"size",
#ifdef MKFS_SIMULATION
#define	L_FILE		3
	"file",
#define	L_NAME		4
	"name",
#endif
	NULL
};

char	*nopts[] = {
#define	N_LOG		0
	"log",
#define	N_SIZE		1
	"size",
#define	N_VERSION	2
	"version",
	NULL,
};

char	*ropts[] = {
#define	R_EXTSIZE	0
	"extsize",
#define	R_SIZE		1
	"size",
#ifdef MKFS_SIMULATION
#define	R_FILE		2
	"file",
#define	R_NAME		3
	"name",
#endif
	NULL
};

/*
 * max transaction reservation values
 * version 1:
 * first dimension log(blocksize) (base XFS_MIN_BLOCKSIZE_LOG)
 * second dimension log(inodesize) (base XFS_DINODE_MIN_LOG)
 * version 2:
 * first dimension log(blocksize) (base XFS_MIN_BLOCKSIZE_LOG)
 * second dimension log(inodesize) (base XFS_DINODE_MIN_LOG)
 * third dimension log(dirblocksize) (base XFS_MIN_BLOCKSIZE_LOG)
 */
#define	DFL_B	(XFS_MAX_BLOCKSIZE_LOG + 1 - XFS_MIN_BLOCKSIZE_LOG)
#define	DFL_I	(XFS_DINODE_MAX_LOG + 1 - XFS_DINODE_MIN_LOG)
#define	DFL_D	(XFS_MAX_BLOCKSIZE_LOG + 1 - XFS_MIN_BLOCKSIZE_LOG)

static const int max_trres_v1[DFL_B][DFL_I] = {
	{ MAXTRRES_B9_I8_D9_V1, 0, 0, 0 },
	{ MAXTRRES_B10_I8_D10_V1, MAXTRRES_B10_I9_D10_V1, 0, 0 },
	{ MAXTRRES_B11_I8_D11_V1, MAXTRRES_B11_I9_D11_V1,
	  MAXTRRES_B11_I10_D11_V1, 0 },
	{ MAXTRRES_B12_I8_D12_V1, MAXTRRES_B12_I9_D12_V1,
	  MAXTRRES_B12_I10_D12_V1, MAXTRRES_B12_I11_D12_V1 },
	{ MAXTRRES_B13_I8_D13_V1, MAXTRRES_B13_I9_D13_V1,
	  MAXTRRES_B13_I10_D13_V1, MAXTRRES_B13_I11_D13_V1 },
	{ MAXTRRES_B14_I8_D14_V1, MAXTRRES_B14_I9_D14_V1,
	  MAXTRRES_B14_I10_D14_V1, MAXTRRES_B14_I11_D14_V1 },
	{ MAXTRRES_B15_I8_D15_V1, MAXTRRES_B15_I9_D15_V1,
	  MAXTRRES_B15_I10_D15_V1, MAXTRRES_B15_I11_D15_V1 },
	{ MAXTRRES_B16_I8_D16_V1, MAXTRRES_B16_I9_D16_V1,
	  MAXTRRES_B16_I10_D16_V1, MAXTRRES_B16_I11_D16_V1 },
};

static const int max_trres_v2[DFL_B][DFL_I][DFL_D] = {
	{ { MAXTRRES_B9_I8_D9_V2, MAXTRRES_B9_I8_D10_V2, MAXTRRES_B9_I8_D11_V2,
	    MAXTRRES_B9_I8_D12_V2, MAXTRRES_B9_I8_D13_V2, MAXTRRES_B9_I8_D14_V2,
	    MAXTRRES_B9_I8_D15_V2, MAXTRRES_B9_I8_D16_V2 },
	  { 0, 0, 0, 0, 0, 0, 0, 0 },
	  { 0, 0, 0, 0, 0, 0, 0, 0 },
	  { 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ { 0, MAXTRRES_B10_I8_D10_V2, MAXTRRES_B10_I8_D11_V2,
	    MAXTRRES_B10_I8_D12_V2, MAXTRRES_B10_I8_D13_V2,
	    MAXTRRES_B10_I8_D14_V2, MAXTRRES_B10_I8_D15_V2,
	    MAXTRRES_B10_I8_D16_V2 },
	  { 0, MAXTRRES_B10_I9_D10_V2, MAXTRRES_B10_I9_D11_V2,
	    MAXTRRES_B10_I9_D12_V2, MAXTRRES_B10_I9_D13_V2,
	    MAXTRRES_B10_I9_D14_V2, MAXTRRES_B10_I9_D15_V2,
	    MAXTRRES_B10_I9_D16_V2 },
	  { 0, 0, 0, 0, 0, 0, 0, 0 },
	  { 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ { 0, 0, MAXTRRES_B11_I8_D11_V2, MAXTRRES_B11_I8_D12_V2,
	    MAXTRRES_B11_I8_D13_V2, MAXTRRES_B11_I8_D14_V2,
	    MAXTRRES_B11_I8_D15_V2, MAXTRRES_B11_I8_D16_V2 },
	  { 0, 0, MAXTRRES_B11_I9_D11_V2, MAXTRRES_B11_I9_D12_V2,
	    MAXTRRES_B11_I9_D13_V2, MAXTRRES_B11_I9_D14_V2,
	    MAXTRRES_B11_I9_D15_V2, MAXTRRES_B11_I9_D16_V2 },
	  { 0, 0, MAXTRRES_B11_I10_D11_V2, MAXTRRES_B11_I10_D12_V2,
	    MAXTRRES_B11_I10_D13_V2, MAXTRRES_B11_I10_D14_V2,
	    MAXTRRES_B11_I10_D15_V2, MAXTRRES_B11_I10_D16_V2 },
	  { 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ { 0, 0, 0, MAXTRRES_B12_I8_D12_V2, MAXTRRES_B12_I8_D13_V2,
	    MAXTRRES_B12_I8_D14_V2, MAXTRRES_B12_I8_D15_V2,
	    MAXTRRES_B12_I8_D16_V2 },
	  { 0, 0, 0, MAXTRRES_B12_I9_D12_V2, MAXTRRES_B12_I9_D13_V2,
	    MAXTRRES_B12_I9_D14_V2, MAXTRRES_B12_I9_D15_V2,
	    MAXTRRES_B12_I9_D16_V2 },
	  { 0, 0, 0, MAXTRRES_B12_I10_D12_V2, MAXTRRES_B12_I10_D13_V2,
	    MAXTRRES_B12_I10_D14_V2, MAXTRRES_B12_I10_D15_V2,
	    MAXTRRES_B12_I10_D16_V2 },
	  { 0, 0, 0, MAXTRRES_B12_I11_D12_V2, MAXTRRES_B12_I11_D13_V2,
	    MAXTRRES_B12_I11_D14_V2, MAXTRRES_B12_I11_D15_V2,
	    MAXTRRES_B12_I11_D16_V2 } },
	{ { 0, 0, 0, 0, MAXTRRES_B13_I8_D13_V2, MAXTRRES_B13_I8_D14_V2,
	    MAXTRRES_B13_I8_D15_V2, MAXTRRES_B13_I8_D16_V2 },
	  { 0, 0, 0, 0, MAXTRRES_B13_I9_D13_V2, MAXTRRES_B13_I9_D14_V2,
	    MAXTRRES_B13_I9_D15_V2, MAXTRRES_B13_I9_D16_V2 },
	  { 0, 0, 0, 0, MAXTRRES_B13_I10_D13_V2, MAXTRRES_B13_I10_D14_V2,
	    MAXTRRES_B13_I10_D15_V2, MAXTRRES_B13_I10_D16_V2 },
	  { 0, 0, 0, 0, MAXTRRES_B13_I11_D13_V2, MAXTRRES_B13_I11_D14_V2,
	    MAXTRRES_B13_I11_D15_V2, MAXTRRES_B13_I11_D16_V2 } },
	{ { 0, 0, 0, 0, 0, MAXTRRES_B14_I8_D14_V2, MAXTRRES_B14_I8_D15_V2,
	    MAXTRRES_B14_I8_D16_V2 },
	  { 0, 0, 0, 0, 0, MAXTRRES_B14_I9_D14_V2, MAXTRRES_B14_I9_D15_V2,
	    MAXTRRES_B14_I9_D16_V2 },
	  { 0, 0, 0, 0, 0, MAXTRRES_B14_I10_D14_V2, MAXTRRES_B14_I10_D15_V2,
	    MAXTRRES_B14_I10_D16_V2 },
	  { 0, 0, 0, 0, 0, MAXTRRES_B14_I11_D14_V2, MAXTRRES_B14_I11_D15_V2,
	    MAXTRRES_B14_I11_D16_V2 } },
	{ { 0, 0, 0, 0, 0, 0, MAXTRRES_B15_I8_D15_V2, MAXTRRES_B15_I8_D16_V2 },
	  { 0, 0, 0, 0, 0, 0, MAXTRRES_B15_I9_D15_V2, MAXTRRES_B15_I9_D16_V2 },
	  { 0, 0, 0, 0, 0, 0, MAXTRRES_B15_I10_D15_V2,
	    MAXTRRES_B15_I10_D16_V2 },
	  { 0, 0, 0, 0, 0, 0, MAXTRRES_B15_I11_D15_V2,
	    MAXTRRES_B15_I11_D16_V2 } },
	{ { 0, 0, 0, 0, 0, 0, 0, MAXTRRES_B16_I8_D16_V2 },
	  { 0, 0, 0, 0, 0, 0, 0, MAXTRRES_B16_I9_D16_V2 },
	  { 0, 0, 0, 0, 0, 0, 0, MAXTRRES_B16_I10_D16_V2 },
	  { 0, 0, 0, 0, 0, 0, 0, MAXTRRES_B16_I11_D16_V2, } },
};

/*
 * Use this before we have a superblock, else would use XFS_DTOBT
 */
#define	DTOBT(d)	((xfs_drfsbno_t)((d) >> (blocklog - BBSHIFT)))

/*
 * Use this for block reservations needed for mkfs's conditions
 * (basically no fragmentation).
 */
#define	MKFS_BLOCKRES_INODE	\
	((uint)(XFS_IALLOC_BLOCKS(mp) + (XFS_IN_MAXLEVELS(mp) - 1)))
#define	MKFS_BLOCKRES(rb)	\
	((uint)(MKFS_BLOCKRES_INODE + XFS_DA_NODE_MAXDEPTH + \
	(XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK) - 1) + (rb)))

#define	OPTSTRING	"b:d:i:l:n:p:qr:C"

main(
	int			argc,
	char			**argv)
{
	__uint64_t		agcount;
	xfs_agf_t		*agf;
	xfs_agi_t		*agi;
	xfs_agnumber_t		agno;
	__uint64_t		agsize;
	xfs_alloc_rec_t		*arec;
	xfs_btree_sblock_t	*block;
	int			blflag;
	int			blocklog;
	int			blocksize;
	int			bsflag;
	int			bsize;
	int			bucket_index;
	buf_t			*buf;
	int			c;
	int			daflag;
	xfs_drfsbno_t		dblocks;
	char			*dfile;
	int			dirblocklog;
	int			dirblocksize;
	int			dirversion;
	int                     do_overlap_checks;
	char			*dsize;
	int			dsunit;
	int			dswidth;
	int			extent_flagging;
	int			i;
	int			iaflag;
	int			ilflag;
	int			imaxpct;
	int			imflag;
	int			inodelog;
	int			inopblock;
	int			ipflag;
	int			isflag;
	int			isize;
	int			laflag;
	int			lalign;
	int			liflag;
	xfs_agnumber_t		logagno;
	xfs_drfsbno_t		logblocks;
	char			*logfile;
	int			loginternal;
	char			*logsize;
	xfs_dfsbno_t		logstart;
	int			lsflag;
	int			min_logblocks;
	mnt_check_state_t       *mnt_check_state;
	int                     mnt_partition_count;
	xfs_mount_t		*mp;
	xfs_mount_t		mpbuf;
	xfs_extlen_t		nbmblocks;
	int			nlflag;
	int			nodsflag;
	xfs_alloc_rec_t		*nrec;
	int			nsflag;
	int			nvflag;
	char			*options;
	char			*protofile;
	char			*protostring;
	int			qflag;
	xfs_drfsbno_t		rtblocks;
	xfs_extlen_t		rtextblocks;
	xfs_drtbno_t		rtextents;
	char			*rtextsize;
	char			*rtfile;
	char			*rtsize;
	buf_t			*sbbuf;
	xfs_sb_t		*sbp;
	int			sectlog;
	sim_init_t		si;
	uint_t			status;
	xfs_trans_t		*tp;
	__uint64_t		tmp_agsize;
	int			worst_freelist;
	int 			xlv_dsunit;
	int			xlv_dswidth;

	xlog_debug = 0;
	blflag = bsflag = 0;
	daflag = 0;
	ilflag = imflag = ipflag = isflag = 0;
	liflag = laflag = loginternal = lsflag = 0; 
	nlflag = nsflag = nvflag = 0;
	dirblocklog = dirblocksize = 0;
	qflag = 0;
	imaxpct = 0;
	iaflag = XFS_IFLAG_ALIGN;
	bzero(&si, sizeof(si));
	si.notvolok = 1;
	dsize = logsize = rtsize = rtextsize = protofile = NULL;
	opterr = 0;
	dsunit = dswidth = nodsflag = lalign = 0;
	do_overlap_checks = 1;
	extent_flagging = 1;

	while ((c = getopt(argc, argv, OPTSTRING)) != EOF) {
		switch (c) {
		case 'C':
			do_overlap_checks = 0;
			break;
		case 'b':
			options = optarg;
			while (*options != '\0') {
				char	*value;

				switch (getsubopt(&options, bopts, &value)) {
				case B_LOG:
					if (!value)
						reqval('b', bopts, B_LOG);
					if (blflag)
						respec('b', bopts, B_LOG);
					if (bsflag)
						conflict('b', bopts, B_SIZE,
							 B_LOG);
					blocklog = atoi(value);
					if (blocklog <= 0)
						illegal(value, "b log");
					blocksize = 1 << blocklog;
					blflag = 1;
					break;
				case B_SIZE:
					if (!value)
						reqval('b', bopts, B_SIZE);
					if (bsflag)
						respec('b', bopts, B_SIZE);
					if (blflag)
						conflict('b', bopts, B_LOG,
							 B_SIZE);
					blocksize = cvtnum(0, value);
					if (blocksize <= 0 ||
					    !ispow2(blocksize))
						illegal(value, "b size");
					blocklog = xfs_highbit32(blocksize);
					bsflag = 1;
					break;
				default:
					unknown('b', value);
				}
			}
			break;
		case 'd':
			options = optarg;
			while (*options != '\0') {
				char	*value;

				switch (getsubopt(&options, dopts, &value)) {
				case D_AGCOUNT:
					if (!value)
						reqval('d', dopts, D_AGCOUNT);
					if (daflag)
						respec('d', dopts, D_AGCOUNT);
					agcount = (__uint64_t)atoll(value);
					if ((__int64_t)agcount <= 0)
						illegal(value, "d agcount");
					daflag = 1;
					break;
				case D_FILE:
					if (!value)
						value = "1";
					si.disfile = atoi(value);
					if (si.disfile < 0 || si.disfile > 1)
						illegal(value, "d file");
					if (si.disfile)
						si.dcreat = 1;
					break;
				case D_NAME:
					if (!value)
						reqval('d', dopts, D_NAME);
					if (si.dname)
						respec('d', dopts, D_NAME);
					si.dname = value;
					break;
				case D_SIZE:
					if (!value)
						reqval('d', dopts, D_SIZE);
					if (dsize)
						respec('d', dopts, D_SIZE);
					dsize = value;
					break;
				case D_SUNIT:
					if (!value)
						reqval('d', dopts, D_SUNIT);
					if (dsunit)
						respec('d', dopts, D_SUNIT);
					dsunit = cvtnum(0, value);
					break;
				case D_SWIDTH:
					if (!value)
						reqval('d', dopts, D_SWIDTH);
					if (dswidth)
						respec('d', dopts, D_SWIDTH);
					dswidth = cvtnum(0, value);
					break;
				case D_UNWRITTEN:
					if (!value)
					    reqval('d', dopts, D_UNWRITTEN);
					i = atoi(value);
					if (i < 0 || i > 1)
					    illegal(value, "d unwritten");
					extent_flagging = i;
					break;
				default:
					unknown('d', value);
				}
			}
			break;
		case 'i':
			options = optarg;
			while (*options != '\0') {
				char	*value;

				switch (getsubopt(&options, iopts, &value)) {
				case I_ALIGN:
					if (!value)
						value = "1";
					iaflag = atoi(value);
					if (iaflag < 0 || iaflag > 1)
						illegal(value, "i align");
					break;
				case I_LOG:
					if (!value)
						reqval('i', iopts, I_LOG);
					if (ilflag)
						respec('i', iopts, I_LOG);
					if (ipflag)
						conflict('i', iopts, I_PERBLOCK,
							 I_LOG);
					if (isflag)
						conflict('i', iopts, I_SIZE,
							 I_LOG);
					inodelog = atoi(value);
					if (inodelog <= 0)
						illegal(value, "i log");
					isize = 1 << inodelog;
					ilflag = 1;
					break;
				case I_MAXPCT:
					if (!value)
						reqval('i', iopts, I_MAXPCT);
					if (imflag)
						respec('i', iopts, I_MAXPCT);
					imaxpct = atoi(value);
					if (imaxpct < 0 || imaxpct > 100)
						illegal(value, "i maxpct");
					imflag = 1;
					break;
				case I_PERBLOCK:
					if (!value)
						reqval('i', iopts, I_PERBLOCK);
					if (ilflag)
						conflict('i', iopts, I_LOG,
							 I_PERBLOCK);
					if (ipflag)
						respec('i', iopts, I_PERBLOCK);
					if (isflag)
						conflict('i', iopts, I_SIZE,
							 I_PERBLOCK);
					inopblock = atoi(value);
					if (inopblock <
						XFS_MIN_INODE_PERBLOCK ||
					    !ispow2(inopblock))
						illegal(value, "i perblock");
					ipflag = 1;
					break;
				case I_SIZE:
					if (!value)
						reqval('i', iopts, I_SIZE);
					if (ilflag)
						conflict('i', iopts, I_LOG,
							 I_SIZE);
					if (ipflag)
						conflict('i', iopts, I_PERBLOCK,
							 I_SIZE);
					if (isflag)
						respec('i', iopts, I_SIZE);
					isize = cvtnum(0, value);
					if (isize <= 0 || !ispow2(isize))
						illegal(value, "i size");
					inodelog = xfs_highbit32(isize);
					isflag = 1;
					break;
				default:
					unknown('i', value);
				}
			}
			break;
		case 'l':
			options = optarg;
			while (*options != '\0') {
				char	*value;

				switch (getsubopt(&options, lopts, &value)) {
				case L_AGNUM:
					if (laflag)
						respec('l', lopts, L_AGNUM);
					logagno = atoi(value);
					laflag = 1;
					loginternal = 1;
					break;
#ifdef MKFS_SIMULATION
				case L_FILE:
					if (!value)
						value = "1";
					if (loginternal)
						conflict('l', lopts, L_INTERNAL,
							 L_FILE);
					si.lisfile = atoi(value);
					if (si.lisfile < 0 || si.lisfile > 1)
						illegal(value, "l file");
					if (si.lisfile)
						si.lcreat = 1;
					break;
#endif
				case L_INTERNAL:
					if (!value)
						value = "1";
#ifdef MKFS_SIMULATION
					if (si.logname)
						conflict('l', lopts, L_NAME,
							 L_INTERNAL);
					if (si.lisfile)
						conflict('l', lopts, L_FILE,
							 L_INTERNAL);
#endif
					if (liflag)
						respec('l', lopts, L_INTERNAL);
					loginternal = atoi(value);
					if (loginternal < 0 || loginternal > 1)
						illegal(value, "l internal");
					liflag = 1;
					break;
#ifdef MKFS_SIMULATION
				case L_NAME:
					if (!value)
						reqval('l', lopts, L_NAME);
					if (loginternal)
						conflict('l', lopts, L_INTERNAL,
							 L_NAME);
					if (si.logname)
						respec('l', lopts, L_NAME);
					si.logname = value;
					break;
#endif
				case L_SIZE:
					if (!value)
						reqval('l', lopts, L_SIZE);
					if (logsize)
						respec('l', lopts, L_SIZE);
					logsize = value;
					lsflag = 1;
					break;
				default:
					unknown('l', value);
				}
			}
			break;
		case 'n':
			options = optarg;
			while (*options != '\0') {
				char	*value;

				switch (getsubopt(&options, nopts, &value)) {
				case N_LOG:
					if (!value)
						reqval('n', nopts, N_LOG);
					if (nlflag)
						respec('n', nopts, N_LOG);
					if (nsflag)
						conflict('n', nopts, N_SIZE,
							 N_LOG);
					dirblocklog = atoi(value);
					if (dirblocklog <= 0)
						illegal(value, "n log");
					dirblocksize = 1 << dirblocklog;
					nlflag = 1;
					break;
				case N_SIZE:
					if (!value)
						reqval('n', nopts, N_SIZE);
					if (nsflag)
						respec('n', nopts, N_SIZE);
					if (nlflag)
						conflict('n', nopts, N_LOG,
							 N_SIZE);
					dirblocksize = cvtnum(0, value);
					if (dirblocksize <= 0 ||
					    !ispow2(dirblocksize))
						illegal(value, "n size");
					dirblocklog =
						xfs_highbit32(dirblocksize);
					nsflag = 1;
					break;
				case N_VERSION:
					if (!value)
						reqval('n', nopts, N_VERSION);
					if (nvflag)
						respec('n', nopts, N_VERSION);
					dirversion = atoi(value);
					if (dirversion < 1 || dirversion > 2)
						illegal(value, "n version");
					nvflag = 1;
					break;
				default:
					unknown('n', value);
				}
			}
			break;
		case 'p':
			if (protofile)
				respec('p', 0, 0);
			protofile = optarg;
			break;
		case 'q':
			qflag = 1;
			break;
		case 'r':
			options = optarg;
			while (*options != '\0') {
				char	*value;

				switch (getsubopt(&options, ropts, &value)) {
				case R_EXTSIZE:
					if (!value)
						reqval('r', ropts, R_EXTSIZE);
					if (rtextsize)
						respec('r', ropts, R_EXTSIZE);
					rtextsize = value;
					break;
#ifdef MKFS_SIMULATION
				case R_FILE:
					if (!value)
						value = "1";
					si.risfile = atoi(value);
					if (si.risfile < 0 || si.risfile > 1)
						illegal(value, "r file");
					if (si.risfile)
						si.rcreat = 1;
					break;
				case R_NAME:
					if (!value)
						reqval('r', ropts, R_NAME);
					if (si.rtname)
						respec('r', ropts, R_NAME);
					si.rtname = value;
					break;
#endif
				case R_SIZE:
					if (!value)
						reqval('r', ropts, R_SIZE);
					if (rtsize)
						respec('r', ropts, R_SIZE);
					rtsize = value;
					break;

				default:
					unknown('r', value);
				}
			}
			break;
		case '?':
			unknown(optopt, "");
		}
	}
	if (argc - optind > 1) {
		fprintf(stderr, "extra arguments\n");
		usage();
	} else if (argc - optind == 1) {
		dfile = si.volname = argv[optind];
		if (si.dname) {
			fprintf(stderr,
				"cannot specify both %s and -d name=%s\n",
				si.volname, si.dname);
			usage();
		}
	} else
		dfile = si.dname;
	/* option post-processing */
	/*
	 * blocksize first, other things depend on it
	 */
	if (!blflag && !bsflag) {
		blocklog = XFS_DFL_BLOCKSIZE_LOG;
		blocksize = 1 << XFS_DFL_BLOCKSIZE_LOG;
	}
	if (blocksize < XFS_MIN_BLOCKSIZE || blocksize > XFS_MAX_BLOCKSIZE) {
		fprintf(stderr, "illegal block size %d\n", blocksize);
		usage();
	}
	if (!nvflag)
		dirversion = (nsflag || nlflag) ? 2 : XFS_DFL_DIR_VERSION;
	switch (dirversion) {
	case 1:
		if ((nsflag || nlflag) && dirblocklog != blocklog) {
			fprintf(stderr, "illegal directory block size %d\n",
				dirblocksize);
			usage();
		}
		break;
	case 2:
		if (nsflag || nlflag) {
			if (dirblocksize < blocksize ||
			    dirblocksize > XFS_MAX_BLOCKSIZE) {
				fprintf(stderr,
					"illegal directory block size %d\n",
					dirblocksize);
				usage();
			}
		} else {
			if (blocksize < (1 << XFS_MIN_REC_DIRSIZE))
				dirblocklog = XFS_MIN_REC_DIRSIZE;
			else
				dirblocklog = blocklog;
			dirblocksize = 1 << dirblocklog;
		}
		break;
	}
	if (!daflag)
		agcount = 8;
	if (si.disfile && (!dsize || !si.dname)) {
		fprintf(stderr,
			"if -d file then -d name and -d size are required\n");
		usage();
	}
	if (dsize) {
		__uint64_t dbytes;

		dbytes = cvtnum(blocksize, dsize);
		if (dbytes % XFS_MIN_BLOCKSIZE) {
			fprintf(stderr,
			"illegal data length %lld, not a multiple of %d\n",
				dbytes, XFS_MIN_BLOCKSIZE);
			usage();
		}
		dblocks = (xfs_drfsbno_t)(dbytes >> blocklog);
		if (dbytes % blocksize)
			fprintf(stderr,
	"warning: data length %lld not a multiple of %d, truncated to %lld\n",
				dbytes, blocksize, dblocks << blocklog);
	}
	if (ipflag) {
		inodelog = blocklog - xfs_highbit32(inopblock);
		isize = 1 << inodelog;
	} else if (!ilflag && !isflag) {
		inodelog = XFS_DINODE_DFL_LOG;
		isize = 1 << inodelog;
	}
#ifdef MKFS_SIMULATION
	if (si.lisfile && (!logsize || !si.logname)) {
		fprintf(stderr,
			"if -l file then -l name and -l size are required\n");
		usage();
	}
#endif
	if (logsize) {
		__uint64_t logbytes;

		logbytes = cvtnum(blocksize, logsize);
		if (logbytes % XFS_MIN_BLOCKSIZE) {
			fprintf(stderr,
			"illegal log length %lld, not a multiple of %d\n",
				logbytes, XFS_MIN_BLOCKSIZE);
			usage();
		}
		logblocks = (xfs_drfsbno_t)(logbytes >> blocklog);
		if (logbytes % blocksize)
			fprintf(stderr,
	"warning: log length %lld not a multiple of %d, truncated to %lld\n",
				logbytes, blocksize, logblocks << blocklog);
	}
#ifdef MKFS_SIMULATION
	if (si.risfile && (!rtsize || !si.rtname)) {
		fprintf(stderr,
			"if -r file then -r name and -r size are required\n");
		usage();
	}
#endif
	if (rtsize) {
		__uint64_t rtbytes;

		rtbytes = cvtnum(blocksize, rtsize);
		if (rtbytes % XFS_MIN_BLOCKSIZE) {
			fprintf(stderr,
			"illegal rt length %lld, not a multiple of %d\n",
				rtbytes, XFS_MIN_BLOCKSIZE);
			usage();
		}
		rtblocks = (xfs_drfsbno_t)(rtbytes >> blocklog);
		if (rtbytes % blocksize)
			fprintf(stderr,
	"warning: rt length %lld not a multiple of %d, truncated to %lld\n",
				rtbytes, blocksize, rtblocks << blocklog);
	}
	/*
	 * If specified, check rt extent size against its constraints.
	 */
	if (rtextsize) {
		__uint64_t rtextbytes;

		rtextbytes = cvtnum(blocksize, rtextsize);
		if (rtextbytes % blocksize) {
			fprintf(stderr,
			"illegal rt extent size %lld, not a multiple of %d\n",
				rtextbytes, blocksize);
			usage();
		}
		if (rtextbytes > XFS_MAX_RTEXTSIZE) {
			fprintf(stderr,
				"rt extent size %s too large, maximum %d\n",
				rtextsize, XFS_MAX_RTEXTSIZE);
			usage();
		}
		if (rtextbytes < XFS_MIN_RTEXTSIZE) {
			fprintf(stderr,
				"rt extent size %s too small, minimum %d\n",
				rtextsize, XFS_MIN_RTEXTSIZE);
			usage();
		}
		rtextblocks = (xfs_extlen_t)(rtextbytes >> blocklog);
	} else {
		/*
		 * If realtime extsize has not been specified by the user,
		 * and the underlying volume is striped, then set rtextblocks
		 * to the stripe width.
		 */
		int dummy1, rswidth;
		__uint64_t rtextbytes;
		dummy1 = rswidth = 0;
		xlv_get_subvol_stripe(dfile, XLV_SUBVOL_TYPE_RT, &dummy1, 
						&rswidth);

		/* check that rswidth is a multiple of fs blocksize */
		if (rswidth && !(BBTOB(rswidth) % blocksize)) {
			rswidth = DTOBT(rswidth);
			rtextbytes = rswidth << blocklog;
			if (XFS_MIN_RTEXTSIZE <= rtextbytes &&
                                (rtextbytes <= XFS_MAX_RTEXTSIZE))  {
       		                 rtextblocks = rswidth;
			} else {
				rtextblocks = XFS_DFL_RTEXTSIZE >> blocklog;
			}
		} else
			rtextblocks = XFS_DFL_RTEXTSIZE >> blocklog;
	}

	/*
	 * Check some argument sizes against mins, maxes.
	 */
	if (isize > blocksize / XFS_MIN_INODE_PERBLOCK ||
	    isize < XFS_DINODE_MIN_SIZE ||
	    isize > XFS_DINODE_MAX_SIZE) {
		int	maxsz;

		fprintf(stderr, "illegal inode size %d\n", isize);
		maxsz = MIN(blocksize / XFS_MIN_INODE_PERBLOCK,
			    XFS_DINODE_MAX_SIZE);
		if (XFS_DINODE_MIN_SIZE == maxsz)
			fprintf(stderr,
			"allowable inode size with %d byte blocks is %d\n",
				blocksize, XFS_DINODE_MIN_SIZE);
		else
			fprintf(stderr,
	"allowable inode size with %d byte blocks is between %d and %d\n",
				blocksize, XFS_DINODE_MIN_SIZE, maxsz);
		usage();
	}

	if (dsunit && !dswidth || !dsunit && dswidth) {
		fprintf(stderr,
"both sunit and swidth options have to be specified\n");
		usage();
	}

	if (dsunit && dswidth % dsunit != 0) {
		fprintf(stderr,
"mount: stripe width (%d) has to be a multiple of the stripe unit (%d)\n",
			dswidth, dsunit);
		return 1;
	}

	/* other global variables */
	sectlog = 9;		/* i.e. 512 bytes */
	/*
	 * Initialize.  This will open the log and rt devices as well, if
	 * we were given a volume.
	 */
	if (!xfs_sim_init(&si))
		usage();
	if (!si.ddev) {
		fprintf(stderr, "no device name given in argument list\n");
		usage();
	}

	if (!si.disfile && do_overlap_checks) {

	        /*
		 * do partition overlap check
		 * If this is a straight file we assume that it's been created
		 * before the call to mnt_check_init()
		 */

                if (mnt_check_init(&mnt_check_state) == -1) {
                        fprintf(stderr,
				"unable to initialize mount checking "
				"routines, bypassing protection checks.\n");
		} else {

		        mnt_partition_count =
			        mnt_find_mount_conflicts(mnt_check_state, dfile);

			/* 
			 * ignore -1 return codes, since 3rd party devices may not
			 * be part of hinv.
			 */

			if (mnt_partition_count > 0) {
			        if (mnt_causes_test(mnt_check_state, MNT_CAUSE_MOUNTED)) {
				        fprintf(stderr,
						"mkfs_xfs: %s is already in use.\n",
						dfile);
				} else if (mnt_causes_test(mnt_check_state, MNT_CAUSE_OVERLAP)) {
				        fprintf(stderr,
						"mkfs_xfs: %s overlaps partition(s) "
						"already in use.\n",
						dfile);
				} else {
				        mnt_causes_show(mnt_check_state, stderr, "mkfs_xfs");
				}
				fprintf(stderr, "\n");
				fflush(stderr);
				mnt_plist_show(mnt_check_state, stderr, "mkfs_xfs");
				fprintf(stderr, "\n");
			}
			mnt_check_end(mnt_check_state);
			if (mnt_partition_count > 0) {
			        usage();
			}
		}
	}
	
	if (!liflag)
		loginternal = si.logdev == 0;
#ifdef MKFS_SIMULATION
	if (si.logname)
		logfile = si.logname;
	else
#endif
	if (loginternal)
		logfile = "internal log";
	else if (si.volname && si.logdev)
		logfile = "volume log";
	else {
		fprintf(stderr, "no log subvolume or internal log\n");
		usage();
	}
#ifdef MKFS_SIMULATION
	if (si.rtname)
		rtfile = si.rtname;
	else
#endif
	if (si.volname && si.rtdev)
		rtfile = "volume rt";
	else
		rtfile = "none";
	if (dsize && si.dsize > 0 && dblocks > DTOBT(si.dsize)) {
		fprintf(stderr,
"size %s specified for data subvolume is too large, maximum is %lld blocks\n",
			dsize, DTOBT(si.dsize));
		usage();
	} else if (!dsize && si.dsize > 0)
		dblocks = DTOBT(si.dsize);
	else if (!dsize) {
		fprintf(stderr, "can't get size of data subvolume\n");
		usage();
	} 
	if (dblocks < XFS_MIN_DATA_BLOCKS) {
		fprintf(stderr,
		"size %lld of data subvolume is too small, minimum %d blocks\n",
			dblocks, XFS_MIN_DATA_BLOCKS);
		usage();
	}
	if (si.logdev && loginternal) {
		fprintf(stderr, "can't have both external and internal logs\n");
		usage();
	}
	if (dirversion == 1)
		i = max_trres_v1[blocklog - XFS_MIN_BLOCKSIZE_LOG]
				[inodelog - XFS_DINODE_MIN_LOG];
	else
		i = max_trres_v2[blocklog - XFS_MIN_BLOCKSIZE_LOG]
				[inodelog - XFS_DINODE_MIN_LOG]
				[dirblocklog - XFS_MIN_BLOCKSIZE_LOG];
	min_logblocks = MAX(XFS_MIN_LOG_BLOCKS, i * XFS_MIN_LOG_FACTOR);
	if (logsize && si.logBBsize > 0 && logblocks > DTOBT(si.logBBsize)) {
		fprintf(stderr,
"size %s specified for log subvolume is too large, maximum is %lld blocks\n",
			logsize, DTOBT(si.logBBsize));
		usage();
	} else if (!logsize && si.logBBsize > 0)
		logblocks = DTOBT(si.logBBsize);
	else if (logsize && !si.logdev && !loginternal) {
		fprintf(stderr,
			"size specified for non-existent log subvolume\n");
		usage();
	} else if (loginternal && logsize && logblocks >= dblocks) {
		fprintf(stderr, "size %lld too large for internal log\n",
			logblocks);
		usage();
	} else if (!loginternal && !si.logdev)
		logblocks = 0;
	else if (loginternal && !logsize)
		logblocks = MAX(XFS_DFL_LOG_SIZE, i * XFS_DFL_LOG_FACTOR);
	if (logblocks < min_logblocks) {
		fprintf(stderr,
		"log size %lld blocks too small, minimum size is %d blocks\n",
			logblocks, min_logblocks);
		usage();
	}
	if (logblocks > XFS_MAX_LOG_BLOCKS) {
		fprintf(stderr,
		"log size %lld blocks too large, maximum size is %d blocks\n",
			logblocks, XFS_MAX_LOG_BLOCKS);
		usage();
	}
	if ((logblocks << blocklog) > XFS_MAX_LOG_BYTES) {
		fprintf(stderr,
		"log size %lld bytes too large, maximum size is %d bytes\n",
			logblocks << blocklog, XFS_MAX_LOG_BYTES);
		usage();
	}
	if (rtsize && si.rtsize > 0 && rtblocks > DTOBT(si.rtsize)) {
		fprintf(stderr,
"size %s specified for rt subvolume is too large, maximum is %lld blocks\n",
			rtsize, DTOBT(si.rtsize));
		usage();
	} else if (!rtsize && si.rtsize > 0)
		rtblocks = DTOBT(si.rtsize);
	else if (rtsize && !si.rtdev) {
		fprintf(stderr,
			"size specified for non-existent rt subvolume\n");
		usage();
	}
	if (si.rtdev) {
		rtextents = rtblocks / rtextblocks;
		nbmblocks = (xfs_extlen_t)howmany(rtextents, NBBY * blocksize);
	} else {
		rtextents = rtblocks = 0;
		nbmblocks = 0;
	}
	agsize = dblocks / agcount + (dblocks % agcount != 0);
	/*
	 * If the ag size is too small, complain if agcount was specified,
	 * and fix it otherwise.
	 */
	if (agsize < XFS_AG_MIN_BLOCKS(blocklog)) {
		if (daflag) {
			fprintf(stderr,
				"too many allocation groups for size\n");
			fprintf(stderr, "need at most %lld allocation groups\n",
				dblocks / XFS_AG_MIN_BLOCKS(blocklog) +
				(dblocks % XFS_AG_MIN_BLOCKS(blocklog) != 0));
			usage();
		}
		agsize = XFS_AG_MIN_BLOCKS(blocklog);
		if (dblocks < agsize)
			agcount = 1;
		else {
			agcount = dblocks / agsize;
			agsize = dblocks / agcount + (dblocks % agcount != 0);
		}
	}
	/*
	 * If the ag size is too large, complain if agcount was specified,
	 * and fix it otherwise.
	 */
	else if (agsize > XFS_AG_MAX_BLOCKS(blocklog)) {
		if (daflag) {
			fprintf(stderr, "too few allocation groups for size\n");
			fprintf(stderr,
				"need at least %lld allocation groups\n",
				dblocks / XFS_AG_MAX_BLOCKS(blocklog) + 
				(dblocks % XFS_AG_MAX_BLOCKS(blocklog) != 0));
			usage();
		}
		agsize = XFS_AG_MAX_BLOCKS(blocklog);
		agcount = dblocks / agsize + (dblocks % agsize != 0);
		agsize = dblocks / agcount + (dblocks % agcount != 0);
	}
	/*
	 * If agcount was not specified, and agsize is larger than
	 * we'd like, make it the size we want.
	 */
	if (!daflag && agsize > XFS_AG_BEST_BLOCKS(blocklog)) {
		agsize = XFS_AG_BEST_BLOCKS(blocklog);
		agcount = dblocks / agsize + (dblocks % agsize != 0);
		agsize = dblocks / agcount + (dblocks % agcount != 0);
	}
	/*
	 * If agcount is too large, make it smaller.
	 */
	if (agcount > XFS_MAX_AGNUMBER + 1) {
		agcount = XFS_MAX_AGNUMBER + 1;
		agsize = dblocks / agcount + (dblocks % agcount != 0);
		if (agsize > XFS_AG_MAX_BLOCKS(blocklog)) {
			/*
			 * We're confused.
			 */
			fprintf(stderr, "can't compute agsize/agcount\n");
			exit(1);
		}
	}

	xlv_dsunit = xlv_dswidth = 0;
	xlv_get_subvol_stripe(dfile, XLV_SUBVOL_TYPE_DATA, &xlv_dsunit, 
				&xlv_dswidth);
	if (dsunit) {

		if (xlv_dsunit && xlv_dsunit != dsunit) {
			fprintf(stderr, 
  "Specified data stripe unit %d is not the same as the xlv stripe unit %d\n", 
				dsunit, xlv_dsunit);
			exit(1);
		}
		if (xlv_dswidth && xlv_dswidth != dswidth) {
			fprintf(stderr, 
"Specified data stripe width (%d) is not the same as the xlv stripe width (%d)\n",
				dswidth, xlv_dswidth);
			exit(1);
		}

	} else {
		dsunit = xlv_dsunit;
		dswidth = xlv_dswidth;
		nodsflag = 1;
	}

	/*
	 * If dsunit is a multiple of fs blocksize, then check that is a
	 * multiple of the agsize too
	 */
	if (dsunit && !(BBTOB(dsunit) % blocksize) && 
	    dswidth && !(BBTOB(dswidth) % blocksize)) {

		/* convert from 512 byte blocks to fs blocksize */
		dsunit = DTOBT(dsunit);
		dswidth = DTOBT(dswidth);

		/* 
		 * agsize is not a multiple of dsunit
		 */
		if ((agsize % dsunit) != 0) {
                	/*
                 	 * round up to stripe unit boundary. Also make sure 
			 * that agsize is still larger than 
			 * XFS_AG_MIN_BLOCKS(blocklog)
		 	 */
                	tmp_agsize = ((agsize + (dsunit - 1))/ dsunit) * dsunit;
                	if ((tmp_agsize >= XFS_AG_MIN_BLOCKS(blocklog)) &&
			    (tmp_agsize <= XFS_AG_MAX_BLOCKS(blocklog)) &&
			    !daflag) {
				agsize = tmp_agsize;
				agcount = dblocks/agsize + 
						(dblocks % agsize != 0);
                	} else {
				if (nodsflag)
					dsunit = dswidth = 0;
				else { 
					fprintf(stderr,
"Allocation group size %lld is not a multiple of the stripe unit %d\n",
						agsize, dsunit);
					exit(1);
				}
        		}
		}
	} else {

		if (nodsflag)
			dsunit = dswidth = 0;
		else { 
			fprintf(stderr,
"Stripe unit(%d) or stripe width(%d) is not a multiple of the block size(%d)\n",
				dsunit, dswidth, blocksize); 	
			exit(1);
		}
	}

	protostring = setup_proto(protofile);
	bsize = 1 << (blocklog - BBSHIFT);
	sbbuf = get_buf(si.ddev, XFS_SB_DADDR, 1, 0);
	if (!sbbuf || geterror(sbbuf))
		fail("get sbbuf", geterror(sbbuf));
	mp = &mpbuf;
	sbp = &mp->m_sb;
	bzero(sbp, sizeof(*sbp));
	sbp->sb_blocklog = (__uint8_t)blocklog;
	sbp->sb_agblklog = (__uint8_t)log2_roundup((unsigned int)agsize);
	mp->m_blkbb_log = sbp->sb_blocklog - BBSHIFT;
	if (loginternal) {
		if (logblocks > agsize - XFS_PREALLOC_BLOCKS(mp)) {
			fprintf(stderr,
	"internal log size %lld too large, must fit in allocation group\n",
				logblocks);
			usage();
		}
		if (laflag) {
			if (logagno >= agcount) {
				fprintf(stderr,
			"log ag number %d too large, must be less than %lld\n",
					logagno, agcount);
				usage();
			}
		} else
			logagno = (xfs_agnumber_t)(agcount / 2);

		logstart = XFS_AGB_TO_FSB(mp, logagno, XFS_PREALLOC_BLOCKS(mp));
		/*
		 * Align the logstart at stripe unit boundary.
		 */
		if (dsunit && ((logstart % dsunit) != 0)) {
			logstart = ((logstart + (dsunit - 1))/dsunit) * dsunit;

			/* 
			 * Make sure that the log size is a multiple of the
			 * stripe unit
			 */
			if ((logblocks % dsunit) != 0) 
			   if (!lsflag) 
				logblocks = ((logblocks + (dsunit - 1))
							/dsunit) * dsunit;
			   else {
				fprintf(stderr,
	"internal log size %lld is not a multiple of the stripe unit %d\n", 
					logblocks, dsunit);
				usage();
			   }

			if(logblocks > agsize - XFS_FSB_TO_AGBNO(mp,logstart)) {
				fprintf(stderr,
"Due to stripe alignment, the internal log size %lld is too large, must fit in allocation group\n",
					logblocks);
				usage();
			}
			lalign = 1;
		}
	} else
		logstart = 0;
	sbp->sb_magicnum = XFS_SB_MAGIC;
	sbp->sb_blocksize = blocksize;
	sbp->sb_dblocks = dblocks;
	sbp->sb_rblocks = rtblocks;
	sbp->sb_rextents = rtextents;
	uuid_create(&sbp->sb_uuid, &status);
	sbp->sb_logstart = logstart;
	sbp->sb_rootino = sbp->sb_rbmino = sbp->sb_rsumino = NULLFSINO;
	sbp->sb_rextsize = rtextblocks;
	sbp->sb_agblocks = (xfs_agblock_t)agsize;
	sbp->sb_agcount = (xfs_agnumber_t)agcount;
	sbp->sb_rbmblocks = nbmblocks;
	sbp->sb_logblocks = (xfs_extlen_t)logblocks;
	sbp->sb_sectsize = 1 << sectlog;
	sbp->sb_inodesize = (__uint16_t)isize;
	sbp->sb_inopblock = (__uint16_t)(blocksize / isize);
	sbp->sb_sectlog = (__uint8_t)sectlog;
	sbp->sb_inodelog = (__uint8_t)inodelog;
	sbp->sb_inopblog = (__uint8_t)(blocklog - inodelog);
	sbp->sb_rextslog =
		(__uint8_t)(rtextents ?
			xfs_highbit32((unsigned int)rtextents) : 0);
	sbp->sb_inprogress = 1;	/* mkfs is in progress */
	sbp->sb_imax_pct = imflag ? imaxpct : XFS_DFL_IMAXIMUM_PCT;
	sbp->sb_icount = 0;
	sbp->sb_ifree = 0;
	sbp->sb_fdblocks = dblocks - agcount * XFS_PREALLOC_BLOCKS(mp) -
		(loginternal ? logblocks : 0);
	sbp->sb_frextents = 0;	/* will do a free later */
	sbp->sb_uquotino = sbp->sb_pquotino = 0;
	sbp->sb_qflags = 0;
	sbp->sb_unit = dsunit;
	sbp->sb_width = dswidth;
	if (dirversion == 2)
		sbp->sb_dirblklog = dirblocklog - blocklog;
	if (iaflag)  {
		sbp->sb_inoalignmt = XFS_INODE_BIG_CLUSTER_SIZE >> blocklog;
		iaflag = sbp->sb_inoalignmt != 0;
	} else
		sbp->sb_inoalignmt = 0;
	sbp->sb_versionnum =
		XFS_SB_VERSION_MKFS(iaflag, dsunit != 0, extent_flagging,
			dirversion == 2);
	bzero(sbbuf->b_un.b_addr, BBSIZE);
	*XFS_BUF_TO_SBP(sbbuf) = *sbp;
	writebuf(sbbuf);
	if (!qflag)
		printf(
		   "meta-data=%-22s isize=%-6d agcount=%lld, agsize=%lld blks\n"
		   "data     =%-22s bsize=%-6d blocks=%lld, imaxpct=%d\n"
		   "         =%-22s sunit=%-6d swidth=%d blks, unwritten=%d\n"
		   "naming   =version %-14d bsize=%-6d\n"
		   "log      =%-22s bsize=%-6d blocks=%lld\n"
		   "realtime =%-22s extsz=%-6d blocks=%lld, rtextents=%lld\n",
			dfile, isize, agcount, agsize,
			"", blocksize, dblocks, sbp->sb_imax_pct,
			"", dsunit, dswidth, extent_flagging,
			dirversion, dirversion == 1 ? blocksize : dirblocksize,
			logfile, 1 << blocklog, logblocks,
			rtfile, rtextblocks << blocklog, rtblocks, rtextents);
	/*
	 * If the data area is a file, then grow it out to its final size
	 * so that the reads for the end of the device in the mount code
	 * will succeed.
	 */
	if (si.disfile && 
	    growfile(bmajor(si.ddev), dblocks * blocksize) < 0)
		fail("Growing the data section file failed", 0);
	/*
	 * Zero the log if there is one.
	 */
	if (loginternal)
		si.logdev = si.ddev;
	if (si.logdev && !dev_zero(si.logdev, XFS_FSB_TO_DADDR(mp, logstart),
				   (xfs_extlen_t)XFS_FSB_TO_BB(mp, logblocks)))
		fail("dev_zero of log failed", 0);
	mp = xfs_mount(si.ddev, si.logdev, si.rtdev);
	if (si.logdev &&
	    XFS_FSB_TO_B(mp, logblocks) <
	    XFS_MIN_LOG_FACTOR * max_trans_res(mp))
		fail("log size too small for transaction reservations", 0);
	sbp = &mp->m_sb;
	worst_freelist = 0;
	for (agno = 0; agno < agcount; agno++) {
		/*
		 * Superblock.
		 */
		buf = get_buf(mp->m_dev, XFS_AG_DADDR(mp, agno, XFS_SB_DADDR),
			1, 0);
		if (!buf || geterror(buf))
			fail("get sb", geterror(buf));
		bzero(buf->b_un.b_addr, BBSIZE);
		*XFS_BUF_TO_SBP(buf) = *sbp;
		writebuf(buf);
		/*
		 * AG header block: freespace
		 */
		buf = get_buf(mp->m_dev, XFS_AG_DADDR(mp, agno, XFS_AGF_DADDR),
			1, 0);
		if (!buf || geterror(buf))
			fail("get agf", geterror(buf));
		agf = XFS_BUF_TO_AGF(buf);
		bzero(agf, BBSIZE);
		if (agno == agcount - 1)
			agsize = dblocks - (xfs_drfsbno_t)(agno * agsize);
		agf->agf_magicnum = XFS_AGF_MAGIC;
		agf->agf_versionnum = XFS_AGF_VERSION;
		agf->agf_seqno = agno;
		agf->agf_length = (xfs_agblock_t)agsize;
		agf->agf_roots[XFS_BTNUM_BNOi] = XFS_BNO_BLOCK(mp);
		agf->agf_roots[XFS_BTNUM_CNTi] = XFS_CNT_BLOCK(mp);
		agf->agf_levels[XFS_BTNUM_BNOi] = 1;
		agf->agf_levels[XFS_BTNUM_CNTi] = 1;
		agf->agf_flfirst = 0;
		agf->agf_fllast = XFS_AGFL_SIZE - 1;
		agf->agf_flcount = 0;
		agf->agf_freeblks =
			(xfs_extlen_t)(agsize - XFS_PREALLOC_BLOCKS(mp));
		if (loginternal && agno == logagno) {
			agf->agf_freeblks -= logblocks;
			agf->agf_longest = agsize - 
				XFS_FSB_TO_AGBNO(mp, logstart) - logblocks;
		} else
			agf->agf_longest = agf->agf_freeblks;
		if (XFS_MIN_FREELIST(agf, mp) > worst_freelist)
			worst_freelist = XFS_MIN_FREELIST(agf, mp);
		writebuf(buf);
		/*
		 * AG header block: inodes
		 */
		buf = get_buf(mp->m_dev, XFS_AG_DADDR(mp, agno, XFS_AGI_DADDR),
			      1, 0);
		if (!buf || geterror(buf))
			fail("get agi", geterror(buf));
		agi = XFS_BUF_TO_AGI(buf);
		bzero(agi, BBSIZE);
		agi->agi_magicnum = XFS_AGI_MAGIC;
		agi->agi_versionnum = XFS_AGI_VERSION;
		agi->agi_seqno = agno;
		agi->agi_length = (xfs_agblock_t)agsize;
		agi->agi_count = 0;
		agi->agi_root = XFS_IBT_BLOCK(mp);
		agi->agi_level = 1;
		agi->agi_freecount = 0;
		agi->agi_newino = NULLAGINO;
		agi->agi_dirino = NULLAGINO;
		for (bucket_index = 0;
		     bucket_index < XFS_AGI_UNLINKED_BUCKETS;
		     bucket_index++) {
			agi->agi_unlinked[bucket_index] = NULLAGINO;
		}
		writebuf(buf);
		/*
		 * BNO btree root block
		 */
		buf = get_buf(mp->m_dev,
			      XFS_AGB_TO_DADDR(mp, agno, XFS_BNO_BLOCK(mp)),
			      bsize, 0);
		if (!buf || geterror(buf))
			fail("get bnobt root", geterror(buf));
		block = XFS_BUF_TO_SBLOCK(buf);
		bzero(block, blocksize);
		block->bb_magic = XFS_ABTB_MAGIC;
		block->bb_level = 0;
		block->bb_numrecs = 1;
		block->bb_leftsib = block->bb_rightsib = NULLAGBLOCK;
		arec = XFS_BTREE_REC_ADDR(blocksize, xfs_alloc, block, 1,
			XFS_BTREE_BLOCK_MAXRECS(blocksize, xfs_alloc, 1));
		arec->ar_startblock = XFS_PREALLOC_BLOCKS(mp);
		if (loginternal && agno == logagno) {
			if (lalign) {
				/*
				 * Have to insert two records
				 */
				arec->ar_blockcount = 
				  (xfs_extlen_t)(XFS_FSB_TO_AGBNO(mp, logstart)
				  		   - (arec->ar_startblock));
				nrec = arec + 1;
				nrec->ar_startblock = arec->ar_startblock +
						      arec->ar_blockcount;
				arec = nrec;
				block->bb_numrecs++;
			} 
			arec->ar_startblock += logblocks;
		} 
		arec->ar_blockcount =
			(xfs_extlen_t)(agsize - arec->ar_startblock);
		writebuf(buf);
		/*
		 * CNT btree root block
		 */
		buf = get_buf(mp->m_dev,
			      XFS_AGB_TO_DADDR(mp, agno, XFS_CNT_BLOCK(mp)),
			      bsize, 0);
		if (!buf || geterror(buf))
			fail("get cntbt root", geterror(buf));
		block = XFS_BUF_TO_SBLOCK(buf);
		bzero(block, blocksize);
		block->bb_magic = XFS_ABTC_MAGIC;
		block->bb_level = 0;
		block->bb_numrecs = 1;
		block->bb_leftsib = block->bb_rightsib = NULLAGBLOCK;
		arec = XFS_BTREE_REC_ADDR(blocksize, xfs_alloc, block, 1,
			XFS_BTREE_BLOCK_MAXRECS(blocksize, xfs_alloc, 1));
		arec->ar_startblock = XFS_PREALLOC_BLOCKS(mp);
		if (loginternal && agno == logagno) {
			if (lalign) {
				arec->ar_blockcount =
				  (xfs_extlen_t)(XFS_FSB_TO_AGBNO(mp, logstart)	
						   - (arec->ar_startblock));
				nrec = arec + 1;
				nrec->ar_startblock = arec->ar_startblock +
						      arec->ar_blockcount;
				arec = nrec;
				block->bb_numrecs++;
			}
			arec->ar_startblock += logblocks;
		}	
		arec->ar_blockcount =
			(xfs_extlen_t)(agsize - arec->ar_startblock);
		writebuf(buf);
		/*
		 * INO btree root block
		 */
		buf = get_buf(mp->m_dev,
			      XFS_AGB_TO_DADDR(mp, agno, XFS_IBT_BLOCK(mp)),
			      bsize, 0);
		if (!buf || geterror(buf))
			fail("get inobt root", geterror(buf));
		block = XFS_BUF_TO_SBLOCK(buf);
		bzero(block, blocksize);
		block->bb_magic = XFS_IBT_MAGIC;
		block->bb_level = 0;
		block->bb_numrecs = 0;
		block->bb_leftsib = block->bb_rightsib = NULLAGBLOCK;
		writebuf(buf);
	}
	/*
	 * Touch last block, make fs the right size if it's a file.
	 */
	buf = get_buf(mp->m_dev, XFS_FSB_TO_BB(mp, dblocks - 1LL), bsize, 0);
	if (!buf || geterror(buf))
		fail("get last filesystem block", geterror(buf));
	bzero(buf->b_un.b_addr, blocksize);
	writebuf(buf);
	/*
	 * Write zeroes in the last partition/logical volume 512 byte block.
	 * This will overwrite the potential last efs superblock.
	 */
	if (si.dsize > 0) {
		buf = get_buf(mp->m_dev, si.dsize - 1, 1, 0);
		if (!buf || geterror(buf))
			fail("get last partition/logical volume block",
				geterror(buf));
		bzero(buf->b_un.b_addr, BBSIZE);
		writebuf(buf);
	}
	/*
	 * Make sure we can write the last block in the realtime area.
	 */
	if (mp->m_rtdev && rtblocks > 0) {
		buf = get_buf(mp->m_rtdev,
			      XFS_FSB_TO_BB(mp, rtblocks - 1LL), bsize, 0);
		if (!buf || geterror(buf))
			fail("get last rtblock", geterror(buf));
		bzero(buf->b_un.b_addr, blocksize);
		writebuf(buf);
	}
	/*
	 * BNO, CNT free block list
	 */
	for (agno = 0; agno < agcount; agno++) {
		xfs_alloc_arg_t	args;

		args.tp = tp = xfs_trans_alloc(mp, 0);
		args.mp = mp;
		args.agno = agno;
		args.minlen = args.total = args.minleft = 0;
		args.alignment = 1;
		args.pag = &mp->m_perag[agno];
		if (i = xfs_trans_reserve(tp, worst_freelist, BBTOB(128),
				0, 0, 0))
			res_failed(i);
		(void)xfs_alloc_fix_freelist(&args, 0);
		xfs_trans_commit(tp, 0, NULL);
	}
	/*
	 * Allocate the root inode and anything else in the proto file.
	 */
	mp->m_rootip = NULL;
	parseproto(mp, NULL, &protostring, NULL);
	/*
	 * protect ourselves against possible stupidity
	 */
	if (XFS_INO_TO_AGNO(mp, mp->m_sb.sb_rootino) != 0)
		fail("root inode not created in AG 0, created in AG",
			XFS_INO_TO_AGNO(mp, mp->m_sb.sb_rootino));
	/*
	 * write out multiple copies of superblocks with the rootinode field set
	 */
	if (mp->m_sb.sb_agcount > 1)  {
		/*
		 * the last superblock
		 */
		buf = readbuf(mp->m_dev,
			XFS_AGB_TO_DADDR(mp, mp->m_sb.sb_agcount-1, XFS_SB_DADDR),
			mp->m_sb.sb_sectsize);
		XFS_BUF_TO_SBP(buf)->sb_rootino = mp->m_sb.sb_rootino;
		writebuf(buf);
		/*
		 * and one in the middle for luck
		 */
		if (mp->m_sb.sb_agcount > 2)  {
			buf = readbuf(mp->m_dev,
				XFS_AGB_TO_DADDR(mp, (mp->m_sb.sb_agcount-1)/2,
					XFS_SB_DADDR),
				mp->m_sb.sb_sectsize);
			XFS_BUF_TO_SBP(buf)->sb_rootino = mp->m_sb.sb_rootino;
			writebuf(buf);
		}
	}
	/*
	 * Done, flush all blocks and inodes by unmounting.
	 */
	xfs_iflush_all(mp, 0);
	bflush(mp->m_dev);
	/*
	 * Force the (incomplete) superblock to be written.
	 */
	buf = xfs_getsb(mp, 0);
	bwrite(buf);
	/*
	 * Mark the filesystem ok.
	 */
	buf = xfs_getsb(mp, 0);
	XFS_BUF_TO_SBP(buf)->sb_inprogress = 0;
	brelse(buf);
	/*
	 * The unmount will force out the superblock.
	 */
	xfs_umount(mp);
	if (si.rtdev)
		dev_close(si.rtdev);
	if (si.logdev)
		dev_close(si.logdev);
	dev_close(si.ddev);
	return 0;
}

STATIC void
conflict(
	char	opt,
	char	*tab[],
	int	oldidx,
	int	newidx)
{
	fprintf(stderr, "cannot specify both -%c %s and -%c %s\n",
		opt, tab[oldidx], opt, tab[newidx]);
	usage();
}

STATIC long long
cvtnum(
	int		blocksize,
	char		*s)
{
	long long	i;
	char		*sp;

	i = strtoll(s, &sp, 0);
	if (i == 0 && sp == s)
		return -1LL;
	if (*sp == '\0')
		return i;
	if (*sp == 'b' && blocksize && sp[1] == '\0')
		return i * blocksize;
	if (*sp == 'k' && sp[1] == '\0')
		return 1024LL * i;
	if (*sp == 'm' && sp[1] == '\0')
		return 1024LL * 1024LL * i;
	if (*sp == 'g' && sp[1] == '\0')
		return 1024LL * 1024LL * 1024LL * i;
	return -1LL;
}

STATIC void
fail(
	char	*msg,
	int	i)
{
	fprintf(stderr, "%s %d\n", msg, i);
#ifdef DEBUG
	abort();
#endif
	exit(1);
}

STATIC long
getnum(
	char	**pp)
{
	char	*s;

	s = getstr(pp);
	return atol(s);
}

STATIC void
getres(
	xfs_trans_t	*tp,
	uint		blocks)
{
	int		i;
	xfs_mount_t	*mp;
	uint		r;

	mp = tp->t_mountp;
	for (r = MKFS_BLOCKRES(blocks); r >= blocks; r--) {
		i = xfs_trans_reserve(tp, r, BBTOB(128), 0,
			XFS_TRANS_PERM_LOG_RES, XFS_DEFAULT_PERM_LOG_COUNT);
		if (i == 0)
			return;
	}
	res_failed(i);
	/* NOTREACHED */
}

STATIC char *
getstr(
	char	**pp)
{
	int	c;
	char	*p;
	char	*rval;

	p = *pp;
	while (c = *p) {
		switch (c) {
		case ' ':
		case '\t':
		case '\n':
			p++;
			continue;
		case ':':
			p++;
			while (*p++ != '\n')
				;
			continue;
		default:
			rval = p;
			while (c != ' ' && c != '\t' && c != '\n' && c != '\0')
				c = *++p;
			*p++ = '\0';
			*pp = p;
			return rval;
		}
	}
	if (!c) {
		fprintf(stderr, "premature EOF in prototype file\n");
		exit(1);
	}
	/* NOTREACHED */
}

/*
 * Wrapper around call to xfs_ialloc. Takes care of committing and
 * allocating a new transaction as needed.
 */
STATIC int
ialloc(
	xfs_trans_t	**tp,
	xfs_inode_t	*pip,
	mode_t		mode,
	ushort		nlink,
	dev_t		rdev,
	cred_t		*cr,
	xfs_inode_t	**ipp)
{
	boolean_t    	call_again;
	int		i;
	buf_t        	*ialloc_context;
	xfs_inode_t  	*ip;
	uint		log_count;
	uint		log_res;
	xfs_trans_t	*ntp;
	int		error;

	call_again = B_FALSE;
	ialloc_context = (buf_t *)0;
	/*
	 * Typically dfltprid (modified using systune) is -1.
	 * Eventhough we can get this value via getdfltprojuser(3C),
	 * we assign zero here, since that has been our practice wrt
	 * inodes.
	 */
	error = xfs_ialloc(*tp, pip, mode, nlink, rdev, cr, (xfs_prid_t) 0,
			   1, &ialloc_context, &call_again, &ip);
	if (error) {
		return error;
	}
	if (call_again) {
		xfs_trans_bhold(*tp, ialloc_context);
		log_res = xfs_trans_get_log_res(*tp);
		log_count = xfs_trans_get_log_count(*tp);
		ntp = xfs_trans_dup(*tp);
		xfs_trans_commit(*tp, 0, NULL);
		*tp = ntp;
		if (i = xfs_trans_reserve(*tp, 0, log_res, 0,
					  XFS_TRANS_PERM_LOG_RES,
					  log_count))
			res_failed(i);
		xfs_trans_bjoin(*tp, ialloc_context);
		error = xfs_ialloc(*tp, pip, mode, nlink, rdev, cr, 
				   (xfs_prid_t) 0, 1, &ialloc_context, 
				   &call_again, &ip);
		if (error) {
			return error;
		}
	}
	*ipp = ip;
	return error;
}

STATIC void
illegal(
	char	*value,
	char	*opt)
{
	fprintf(stderr, "illegal value %s for -%s option\n", value, opt);
	usage();
}

STATIC int
ispow2(
	unsigned int	i)
{
	return (i & (i - 1)) == 0;
}

STATIC unsigned int
log2_roundup(
	unsigned int	i)
{
	unsigned int	rval;

	for (rval = 0; rval < NBBY * sizeof(i); rval++) {
		if ((1 << rval) >= i)
			break;
	}
	return rval;
}

STATIC int
max_trans_res(
	xfs_mount_t			*mp)
{
	uint				*p;
	int				rval;
	xfs_trans_reservations_t	*tr;

	for (rval = 0, tr = &mp->m_reservations, p = (uint *)tr;
	     p < (uint *)(tr + 1);
	     p++) {
		if ((int)*p > rval)
			rval = (int)*p;
	}
	return rval;
}

STATIC int
newfile(
	xfs_trans_t	*tp,
	xfs_inode_t	*ip,
	xfs_bmap_free_t	*flist,
	xfs_fsblock_t	*first,
	int		dolocal,
	int		logit,
	char		*buf,
	int		len)
{
	buf_t		*bp;
	daddr_t		d;
	int		error;
	int		flags;
	xfs_bmbt_irec_t	map;
	xfs_mount_t	*mp;
	xfs_extlen_t	nb;
	int		nmap;

	mp = ip->i_mount;
	if (dolocal && len <= XFS_IFORK_DSIZE(ip)) {
		xfs_idata_realloc(ip, len, XFS_DATA_FORK);
		if (buf)
			bcopy(buf, ip->i_df.if_u1.if_data, len);
		ip->i_d.di_size = len;
		ip->i_df.if_flags &= ~XFS_IFEXTENTS;
		ip->i_df.if_flags |= XFS_IFINLINE;
		ip->i_d.di_format = XFS_DINODE_FMT_LOCAL;
		flags = XFS_ILOG_DDATA;
	} else if (len > 0) {
		nb = XFS_B_TO_FSB(mp, len);
		nmap = 1;
		error = xfs_bmapi(tp, ip, 0, nb, XFS_BMAPI_WRITE, first, nb,
				  &map, &nmap, flist);
		if (error) {
			fail("mkfs encountered an error allocating space for a file", error);
		}
		if (nmap != 1) {
			fprintf(stderr, "couldn't allocate space for file\n");
			exit(1);
		}
		d = XFS_FSB_TO_DADDR(mp, map.br_startblock);
		bp = xfs_trans_get_buf(logit ? tp : 0, mp->m_ddev_targp, d,
			nb << mp->m_blkbb_log, 0);
		if (!bp || geterror(bp))
			fail("xget newfile", geterror(bp));
		bcopy(buf, bp->b_un.b_addr, len);
		if (len < bp->b_bcount)
			bzero(bp->b_un.b_addr + len, bp->b_bcount - len);
		if (logit)
			xfs_trans_log_buf(tp, bp, 0, bp->b_bcount - 1);
		else
			writebuf(bp);
		flags = 0;
	}
	ip->i_d.di_size = len;
	return flags;
}

STATIC char *
newregfile(
	char		**pp,
	int		*len)
{
	char		*buf;
	int		fd;
	char		*fname;
	long		size;

	fname = getstr(pp);
	if ((fd = open(fname, O_RDONLY)) < 0 || (size = filesize(fd)) < 0) {
		perror(fname);
		exit(1);
	}
	if (*len = (int)size) {
		buf = malloc(size);
		if (read(fd, buf, size) < size) {
			perror(fname);
			exit(1);
		}
	} else
		buf = 0;
	close(fd);
	return buf;
}

STATIC void
parseproto(
	xfs_mount_t	*mp,
	xfs_inode_t	*pip,
	char		**pp,
	char		*name)
{
	char		*buf;
	int		committed;
	int		error;
	xfs_fsblock_t	first;
	int		flags;
	xfs_bmap_free_t	flist;
	int		fmt;
	int		gid;
	int		i;
	xfs_inode_t	*ip;
	int		len;
	int		majdev;
	int		mindev;
	int		mode;
	char		*mstr;
	xfs_trans_t	*tp;
	int		uid;
	int		val;
	int		isroot = 0;
	cred_t		zerocr;

	bzero(&zerocr, sizeof(zerocr));
	mstr = getstr(pp);
	switch (mstr[0]) {
	case '-':
		fmt = IFREG;
		break;
	case 'b':
		fmt = IFBLK;
		break;
	case 'c':
		fmt = IFCHR;
		break;
	case 'd':
		fmt = IFDIR;
		break;
	case 'l':
		fmt = IFLNK;
		break;
	case 'p':
		fmt = IFIFO;
		break;
	default:
		fprintf(stderr, "bad format string %s\n", mstr);
		exit(1);
	}
	mode = 0;
	switch (mstr[1]) {
	case '-':
		break;
	case 'u':
		mode |= ISUID;
		break;
	default:
		fprintf(stderr, "bad format string %s\n", mstr);
		exit(1);
	}
	switch (mstr[2]) {
	case '-':
		break;
	case 'g':
		mode |= ISGID;
		break;
	default:
		fprintf(stderr, "bad format string %s\n", mstr);
		exit(1);
	}
	val = 0;
	for (i = 3; i < 6; i++) {
		if (mstr[i] < '0' || mstr[i] > '7') {
			fprintf(stderr, "bad format string %s\n", mstr);
			exit(1);
		}
		val = val * 8 + mstr[i] - '0';
	}
	mode |= val;
	uid = (int)getnum(pp);
	gid = (int)getnum(pp);
	tp = xfs_trans_alloc(mp, 0);
	flags = XFS_ILOG_CORE;
	XFS_BMAP_INIT(&flist, &first);
	switch (fmt) {
	case IFREG:
		buf = newregfile(pp, &len);
		getres(tp, XFS_B_TO_FSB(mp, len));
		error = ialloc(&tp, pip, mode|IFREG, 1, mp->m_dev, &zerocr, &ip);
		if (error) {
			fail("Inode allocation failed", error);
		}
		flags |= newfile(tp, ip, &flist, &first, 0, 0, buf, len);
		if (buf)
			free(buf);
		xfs_trans_ijoin(tp, pip, XFS_ILOCK_EXCL);
		error = XFS_DIR_CREATENAME(mp, tp, pip, name, strlen(name), ip->i_ino, &first, &flist, 1);
		if (error)
			fail("directory createname error", error);
		xfs_trans_ihold(tp, pip);
		break;
	case IFBLK:
	case IFCHR:
		getres(tp, 0);
		majdev = (int)getnum(pp) & L_MAXMAJ;
		mindev = (int)getnum(pp) & L_MAXMIN;
		error = ialloc(&tp, pip, mode|fmt, 1, makedev(majdev, mindev),
			       &zerocr, &ip);
		if (error) {
			fail("Inode allocation failed", error);
		}
		xfs_trans_ijoin(tp, pip, XFS_ILOCK_EXCL);
		error = XFS_DIR_CREATENAME(mp, tp, pip, name, strlen(name), ip->i_ino, &first, &flist, 1);
		if (error)
			fail("directory createname error", error);
		xfs_trans_ihold(tp, pip);
		flags |= XFS_ILOG_DEV;
		break;
	case IFIFO:
		getres(tp, 0);
		error = ialloc(&tp, pip, mode|IFIFO, 1, mp->m_dev, &zerocr,
			       &ip);
		if (error) {
			fail("Inode allocation failed", error);
		}
		xfs_trans_ijoin(tp, pip, XFS_ILOCK_EXCL);
		error = XFS_DIR_CREATENAME(mp, tp, pip, name, strlen(name), ip->i_ino, &first, &flist, 1);
		if (error)
			fail("directory createname error", error);
		xfs_trans_ihold(tp, pip);
		break;
	case IFLNK:
		buf = getstr(pp);
		len = (int)strlen(buf);
		getres(tp, XFS_B_TO_FSB(mp, len));
		error = ialloc(&tp, pip, mode|IFLNK, 1, mp->m_dev, &zerocr,
			       &ip);
		if (error) {
			fail("Inode allocation failed", error);
		}
		flags |= newfile(tp, ip, &flist, &first, 1, 1, buf, len);
		xfs_trans_ijoin(tp, pip, XFS_ILOCK_EXCL);
		error = XFS_DIR_CREATENAME(mp, tp, pip, name, strlen(name), ip->i_ino, &first, &flist, 1);
		if (error)
			fail("directory createname error", error);
		xfs_trans_ihold(tp, pip);
		break;
	case IFDIR:
		getres(tp, 0);
		error = ialloc(&tp, pip, mode|IFDIR, 1, mp->m_dev, &zerocr,
			       &ip);
		if (error) {
			fail("Inode allocation failed", error);
		}
		ip->i_d.di_uid = uid;
		ip->i_d.di_gid = gid;
		ip->i_d.di_nlink++;		/* account for . */
		if (!pip) {
			pip = ip;
			mp->m_sb.sb_rootino = ip->i_ino;
			xfs_mod_sb(tp, XFS_SB_ROOTINO);
			mp->m_rootip = ip;
			isroot = 1;
		} else {
			xfs_trans_ijoin(tp, pip, XFS_ILOCK_EXCL);
			error = XFS_DIR_CREATENAME(mp, tp, pip, name, strlen(name), ip->i_ino, &first, &flist, 1);
			if (error)
				fail("directory createname error", error);
			pip->i_d.di_nlink++;
			xfs_trans_ihold(tp, pip);
			xfs_trans_log_inode(tp, pip, XFS_ILOG_CORE);
		}
		XFS_DIR_INIT(mp, tp, ip, pip);
		xfs_trans_log_inode(tp, ip, flags);
		error = xfs_bmap_finish(&tp, &flist, first, &committed);
		if (error) {
			fail("Directory creation failed", error);
		}
		xfs_trans_ihold(tp, ip);
		xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES, NULL);
		/*
		 * RT initialization.  Do this here to ensure that
		 * the RT inodes get placed after the root inode.
		 */
		if (isroot)
			rtinit(mp);
		tp = NULL;
		for (;;) {
			name = getstr(pp);
			if (strcmp(name, "$") == 0)
				break;
			parseproto(mp, ip, pp, name);
		}
		xfs_iput(ip, XFS_ILOCK_EXCL);
		return;
	}
	ip->i_d.di_uid = uid;
	ip->i_d.di_gid = gid;
	xfs_trans_log_inode(tp, ip, flags);
	error = xfs_bmap_finish(&tp, &flist, first, &committed);
	if (error) {
		fail("Error encountered creating a file in the proto spec", error);
	}
	xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES, NULL);
}

STATIC buf_t *
readbuf(
	dev_t	dev,
	daddr_t	blkno,
	int	len)
{
	int	error;
	buf_t	*bp;

	bp = bread(dev, blkno, len);
	if (bp == NULL || (error = geterror(bp)))
		fail("read failed", error);
	return(bp);
}

STATIC void
reqval(
	char	opt,
	char	*tab[],
	int	idx)
{
	fprintf(stderr, "-%c %s option requires a value\n", opt, tab[idx]);
	usage();
}

STATIC void
res_failed(
	int	err)
{
	if (err == ENOSPC) {
		fprintf(stderr, "ran out of disk space!\n");
#ifdef DEBUG
		abort();
#endif
		exit(1);
	} else
		fail("xfs_trans_reserve returned ", err);
}

STATIC void
respec(
	char	opt,
	char	*tab[],
	int	idx)
{
	fprintf(stderr, "-%c ", opt);
	if (tab)
		fprintf(stderr, "%s ", tab[idx]);
	fprintf(stderr, "option respecified\n");
	usage();
}

/*
 * Allocate the realtime bitmap and summary inodes, and fill in data if any.
 */
STATIC void
rtinit(
	xfs_mount_t	*mp)
{
	xfs_dfiloff_t	bno;
	int		committed;
	xfs_dfiloff_t	ebno;
	xfs_bmbt_irec_t	*ep;
	int		error;
	xfs_fsblock_t	first;
	xfs_bmap_free_t	flist;
	int		i;
	xfs_bmbt_irec_t	map[XFS_BMAP_MAX_NMAP];
	xfs_extlen_t	nsumblocks;
	int		nmap;
	xfs_inode_t	*rbmip;
	xfs_inode_t	*rsumip;
	xfs_trans_t	*tp;
	cred_t		zerocr;

	/*
	 * First, allocate the inodes.
	 */
	tp = xfs_trans_alloc(mp, 0);
	if (i = xfs_trans_reserve(tp, MKFS_BLOCKRES_INODE, BBTOB(128), 0,
				  XFS_TRANS_PERM_LOG_RES,
				  XFS_DEFAULT_PERM_LOG_COUNT))
		res_failed(i);
	bzero(&zerocr, sizeof(zerocr));
	error = ialloc(&tp, mp->m_rootip, IFREG, 1, mp->m_dev, &zerocr, &rbmip);
	if (error) {
		fail("Realtime bitmap inode allocation failed", error);
	}
	/*
	 * Do our thing with rbmip before allocating rsumip,
	 * because the next call to ialloc() may
	 * commit the transaction in which rbmip was allocated.
	 */
	mp->m_sb.sb_rbmino = rbmip->i_ino;
	rbmip->i_d.di_size = mp->m_sb.sb_rbmblocks * mp->m_sb.sb_blocksize;
	rbmip->i_d.di_flags = XFS_DIFLAG_NEWRTBM;
	*(__uint64_t *)&rbmip->i_d.di_atime = 0;
	xfs_trans_log_inode(tp, rbmip, XFS_ILOG_CORE);
	xfs_mod_sb(tp, XFS_SB_RBMINO);
	xfs_trans_ihold(tp, rbmip);
	mp->m_rbmip = rbmip;
	error = ialloc(&tp, mp->m_rootip, IFREG, 1, mp->m_dev, &zerocr, &rsumip);
	if (error) {
		fail("Realtime bitmap inode allocation failed", error);
	}
	mp->m_sb.sb_rsumino = rsumip->i_ino;
	rsumip->i_d.di_size = mp->m_rsumsize;
	xfs_trans_log_inode(tp, rsumip, XFS_ILOG_CORE);
	xfs_mod_sb(tp, XFS_SB_RSUMINO);
	xfs_trans_ihold(tp, rsumip);
	xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES, NULL);
	mp->m_rsumip = rsumip;
	/*
	 * Next, give the bitmap file some blocks.
	 * Fill them in to all zeroes.
	 */
	tp = xfs_trans_alloc(mp, 0);
	if (i = xfs_trans_reserve(tp,
				  mp->m_sb.sb_rbmblocks +
				      (XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK) - 1),
				  BBTOB(128), 0, XFS_TRANS_PERM_LOG_RES,
				  XFS_DEFAULT_PERM_LOG_COUNT))
		res_failed(i);
	xfs_trans_ijoin(tp, rbmip, XFS_ILOCK_EXCL);
	bno = 0;
	XFS_BMAP_INIT(&flist, &first);
	while (bno < mp->m_sb.sb_rbmblocks) {
		nmap = XFS_BMAP_MAX_NMAP;
		error = xfs_bmapi(tp, rbmip, bno,
				  (xfs_extlen_t)(mp->m_sb.sb_rbmblocks - bno),
				  XFS_BMAPI_WRITE, &first, mp->m_sb.sb_rbmblocks,
				  map, &nmap, &flist);
		if (error) {
			fail("Allocation of the realtime bitmap failed", error);
		}
		for (i = 0, ep = map; i < nmap; i++, ep++) {
			if (!dev_zero(mp->m_dev,
				      XFS_FSB_TO_DADDR(mp, ep->br_startblock),
				      XFS_FSB_TO_BB(mp, ep->br_blockcount)))
				fail("dev_zero of rtbitmap failed", 0);
			bno += ep->br_blockcount;
		}
	}

	error = xfs_bmap_finish(&tp, &flist, first, &committed);
	if (error) {
		fail("Allocation of the realtime bitmap failed", error);
	}
	xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES, NULL);
	/*
	 * Give the summary file some blocks.
	 * Fill them in to all zeroes.
	 */
	tp = xfs_trans_alloc(mp, 0);
	nsumblocks = mp->m_rsumsize >> mp->m_sb.sb_blocklog;
	if (i = xfs_trans_reserve(tp,
			nsumblocks + (XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK) - 1),
			BBTOB(128), 0, XFS_TRANS_PERM_LOG_RES,
			XFS_DEFAULT_PERM_LOG_COUNT))
		res_failed(i);
	xfs_trans_ijoin(tp, rsumip, XFS_ILOCK_EXCL);
	bno = 0;
	XFS_BMAP_INIT(&flist, &first);
	while (bno < nsumblocks) {
		nmap = XFS_BMAP_MAX_NMAP;
		error = xfs_bmapi(tp, rsumip, bno,
				  (xfs_extlen_t)(nsumblocks - bno),
				  XFS_BMAPI_WRITE, &first, nsumblocks,
				  map, &nmap, &flist);
		if (error) {
			fail("Allocation of the realtime bitmap failed", error);
		}
		for (i = 0, ep = map; i < nmap; i++, ep++) {
			if (!dev_zero(mp->m_dev,
				      XFS_FSB_TO_DADDR(mp, ep->br_startblock),
				      XFS_FSB_TO_BB(mp, ep->br_blockcount)))
				fail("dev_zero of rtsummary failed", 0);
			bno += ep->br_blockcount;
		}
	}
	error = xfs_bmap_finish(&tp, &flist, first, &committed);
	if (error) {
		fail("Allocation of the realtime bitmap failed", error);
	}
	xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES, NULL);
	/*
	 * Free the whole area using transactions.
	 * Do one transaction per bitmap block.
	 */
	for (bno = 0; bno < mp->m_sb.sb_rextents; bno = ebno) {
		tp = xfs_trans_alloc(mp, 0);
		if (i = xfs_trans_reserve(tp, 0, BBTOB(128), 0,
					  XFS_TRANS_PERM_LOG_RES,
					  XFS_DEFAULT_PERM_LOG_COUNT))
			res_failed(i);
		XFS_BMAP_INIT(&flist, &first);
		ebno = XFS_RTMIN(mp->m_sb.sb_rextents,
			bno + NBBY * mp->m_sb.sb_blocksize);
		error = xfs_rtfree_extent(tp, bno, (xfs_extlen_t)(ebno - bno));
		if (error) {
			fail("Error initializing the realtime bitmap", error);
		}
		error = xfs_bmap_finish(&tp, &flist, first, &committed);
		if (error) {
			fail("Error initializing the realtime bitmap", error);
		}
		xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES, NULL);
	}
}

STATIC char *
setup_proto(
	char	*fname)
{
	char		*buf;
	static char	dflt[] = "d--755 0 0 $";
	int		fd;
	long		size;

	if (!fname)
		return dflt;
	if ((fd = open(fname, O_RDONLY)) < 0 || (size = filesize(fd)) < 0) {
		perror(fname);
		exit(1);
	}
	buf = malloc(size + 1);
	if (read(fd, buf, size) < size) {
		perror(fname);
		exit(1);
	}
	if (buf[size - 1] != '\n') {
		fprintf(stderr, "proto file %s premature EOF\n", fname);
		exit(1);
	}
	buf[size] = '\0';
	/*
	 * Skip past the stuff there for compatibility, a string and 2 numbers.
	 */
	(void)getstr(&buf);	/* boot image name */
	(void)getnum(&buf);	/* block count */
	(void)getnum(&buf);	/* inode count */
	return buf;
}

STATIC void
unknown(
	char	opt,
	char	*s)
{
	fprintf(stderr, "unknown option -%c %s\n", opt, s);
	usage();
}

STATIC void
usage(void)
{
	fprintf(stderr,
       "usage: mkfs_xfs\n"
       "/* blocksize */		[-b log=n|size=num]\n"
       "/* data subvol */	[-d agcount=n,file,name=xxx,size=num,\n"
       "			    sunit=value,swidth=value,unwritten=0|1]\n"
       "/* inode size */	[-i log=n|perblock=n|size=num,maxpct=n]\n"
#ifdef MKFS_SIMULATION
       "/* log subvol */	[-l agnum=n,file,internal,name=xxx,size=num]\n"
#else
       "/* log subvol */	[-l agnum=n,internal,size=num]\n"
#endif
       "/* naming */		[-n log=n|size=num|version=n]\n"
       "/* prototype file */	[-p fname]\n"
       "/* quiet */		[-q]\n"
#ifdef MKFS_SIMULATION
       "/* realtime subvol */	[-r extsize=num,file,name=xxx,size=num]\n"
#else
       "/* realtime subvol */	[-r extsize=num,size=num]\n"
#endif
       "\t\t\tdevicename\n"
"devicename is required unless -d name=xxx is given\n"
"internal 1000 block log is default unless overridden or using XLV with log\n"
"num is xxx (bytes), or xxxb (blocks), or xxxk (xxx KB), or xxxm (xxx MB)\n"
"value is xxx (512 blocks)\n");
	exit(1);
}

STATIC void
writebuf(
	buf_t	*bp)
{
	int	error;

	if (error = bwrite(bp))
		fail("write failed", error);
}
