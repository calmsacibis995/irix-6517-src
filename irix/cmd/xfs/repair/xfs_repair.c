#define _SGI_MP_SOURCE

#include "vsn.h"
#define _KERNEL 1
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/sysmacros.h>
#undef _KERNEL

#include <sys/prctl.h>
#include <sys/sema.h>
#include <sys/stat.h>
#include <sys/uuid.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <ulocks.h>
#include <errno.h>
#include <string.h>
#include <bstring.h>
#include <assert.h>
#if VERS >= V_64
#include <ksys/behavior.h>
#endif
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_ialloc.h>
#include <sys/fs/xfs_btree.h>
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#include <sys/fs/xfs_dir.h>
#if VERS >= V_654
#include <sys/fs/xfs_dir2.h>
#include <sys/fs/xfs_dir2_sf.h>
#endif
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_inode.h>
#include <sys/fs/xfs_mount.h>
#include <sys/fs/xfs_quota.h>

#if VERS <= V_64
#include <sys/avl.h>
#else
#include "avl.h"
#endif
#include "avl64.h"

#include "sim.h"
#include "globals.h"
#include "versions.h"
#include "agheader.h"
#include "protos.h"
#include "incore.h"
#include "err_protos.h"

#ifdef DEBUG_MALLOC
#include <malloc.h>
#endif

#define	rounddown(x, y)	(((x)/(y))*(y))

extern void	phase1(xfs_mount_t *);
extern void	phase2(xfs_mount_t *, sim_init_t *);
extern void	phase3(xfs_mount_t *);
extern void	phase4(xfs_mount_t *);
extern void	phase5(xfs_mount_t *);
extern void	phase6(xfs_mount_t *);
extern void	phase7(xfs_mount_t *);
extern void	incore_init(xfs_mount_t *);

#define		XR_MAX_SECT_SIZE	(64 * 1024)

/*
 * option tables for getsubopt calls
 */

/*
 * -o (user-supplied override options)
 */

char *o_opts[] = {
#define ASSUME_XFS	0
	"assume_xfs",
#define PRE_65_BETA	1
	"fs_is_pre_65_beta",
	NULL
};

void
usage(void)
{
	do_warn("usage:  xfs_repair [-n] [-o subopt[=value]] fs-device\n");
}

static char *err_message[] =  {
	"no error",
	"bad magic number",
	"bad blocksize field",
	"bad blocksize log field",
	"bad version number",
	"filesystem mkfs-in-progress bit set",
	"inconsistent filesystem geometry information",
	"bad inode size or inconsistent with number of inodes/block",
	"bad sector size",
	"AGF geometry info conflicts with filesystem geometry",
	"AGI geometry info conflicts with filesystem geometry",
	"AG superblock geometry info conflicts with filesystem geometry",
	"attempted to perform I/O beyond EOF",
	"inconsistent filesystem geometry in realtime filesystem component",
	"maximum indicated percentage of inodes > 100%",
	"inconsistent inode alignment value",
	"not enough secondary superblocks with matching geometry",
	"bad stripe unit in superblock",
	"bad stripe width in superblock",
	"bad shared version number in superblock"
};

char *
err_string(int err_code)
{
	if (err_code < XR_OK || err_code >= XR_BAD_ERR_CODE)
		do_abort("bad error code - %d\n", err_code);

	return(err_message[err_code]);
}

static void
noval(char opt, char *tbl[], int idx)
{
	do_warn("-%c %s option cannot have a value\n", opt, tbl[idx]);
	usage();
	exit(1);
}

static void
respec(char opt, char *tbl[], int idx)
{
	do_warn("-%c ", opt);
	if (tbl)
		do_warn("%s ", tbl[idx]);
	do_warn("option respecified\n");

	usage();
	exit(1);
}

static void
unknown(char opt, char *s)
{
	do_warn("unknown option -%c %s\n", opt, s);
	usage();
	exit(1);
}

/*
 * sets only the global argument flags and variables
 */
void
process_args(int argc, char **argv)
{
	char *options;
	int c;
	int res;

	fs_is_dirty = 0;
	verbose = 0;
	no_modify = 0;
	isa_file = 0;
	dumpcore = 0;
	full_backptrs = 0;
	delete_attr_ok = 1;
	force_geo = 0;
	assume_xfs = 0;
	clear_sunit = 0;
	sb_inoalignmt = 0;
	sb_unit = 0;
	sb_width = 0;
	pre_65_beta = 0;

#if VERS == V_53
	fs_attributes_allowed = 0;
	fs_inode_nlink_allowed = 0;
	fs_quotas_allowed = 0;
	fs_aligned_inodes_allowed = 0;
	fs_sb_feature_bits_allowed = 0;
	fs_has_extflgbit_allowed = 0;
#elif VERS >= V_62
	fs_attributes_allowed = 1;
	fs_inode_nlink_allowed = 1;
	fs_quotas_allowed = 1;
	fs_aligned_inodes_allowed = 1;
	fs_sb_feature_bits_allowed = 1;
	fs_has_extflgbit_allowed = 1;
#if VERS >= V_65
	pre_65_beta = 0;
	fs_shared_allowed = 1;
#else
	pre_65_beta = 1;
	fs_shared_allowed = 0;
#endif
#endif /* VERS */

	/*
	 * XXX have to add suboption processing here
	 * attributes, quotas, nlinks, aligned_inos, sb_fbits
	 */
	while ((c = getopt(argc, argv, "o:fnDvV:")) != EOF)  {
		switch (c) {
		case 'D':
			dumpcore = 1;
			break;
		case 'o':
			options = optarg;
			while (*options != '\0')  {
				char *val;

				switch (getsubopt(&options, o_opts, &val))  {
				case ASSUME_XFS:
					if (val)
						noval('o', o_opts, ASSUME_XFS);
					if (assume_xfs)
						respec('o', o_opts, ASSUME_XFS);
					assume_xfs = 1;
					break;
				case PRE_65_BETA:
					if (val)
						noval('o', o_opts, PRE_65_BETA);
					if (pre_65_beta)
						respec('o', o_opts,
							PRE_65_BETA);
					pre_65_beta = 1;
					break;
				default:
					unknown('o', val);
					break;
				}
			}
			break;
		case 'f':
			isa_file = 1;
			break;
		case 'n':
			no_modify = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'V':
			res = sscanf(optarg, "%d", &verbose);

			if (res != 1 || verbose <= 0)  {
				usage();
				exit(1);
			}
			break;
		case '?':
			usage();
			exit(1);
			break;
		}
	}

	if (argc - optind != 1)  {
		usage();
		exit(1);
	}

	if ((fs_name = argv[optind]) == NULL)  {
		usage();
		exit(1);
	}
}

void
do_msg(int do_abort, char *msg, va_list args)
{
	vfprintf(stderr, msg, args);

	if (do_abort)  {
		if (dumpcore)
			abort();
		else
			exit(1);
		/* NOTREACHED */
	}

	return;
}

void
do_error(char *msg, ...)
{
	va_list args;

	fprintf(stderr, "\nfatal error -- ");

	va_start(args, msg);
	do_msg(1, msg, args);
	/* NOTREACHED */
}

/*
 * like do_error, only the error is internal, no system
 * error so no oserror processing
 */
void
do_abort(char *msg, ...)
{
	va_list args;

	va_start(args, msg);
	do_msg(1, msg, args);
	/* NOTREACHED*/
}

void
do_warn(char *msg, ...)
{
	va_list args;

	fs_is_dirty = 1;

	va_start(args, msg);
	do_msg(0, msg, args);
	va_end(args);

	return;
}

/* no formatting */

void
do_log(char *msg, ...)
{
	va_list args;

	va_start(args, msg);
	do_msg(0, msg, args);
	va_end(args);

	return;
}

void
calc_mkfs(xfs_mount_t *mp)
{
	xfs_agblock_t	fino_bno;
	int		do_inoalign;
#if VERS < V_65
	int		inoalign_mask;
#endif

#if VERS >= V_65
	do_inoalign = mp->m_sinoalign;
#else
	/*
	 * adapted from xfs_mountfs_int() code
	 */
	if (XFS_SB_VERSION_HASALIGN(&mp->m_sb) &&
	    mp->m_sb.sb_inoalignmt >=
	    XFS_B_TO_FSBT(mp, mp->m_inode_cluster_size))
		inoalign_mask = mp->m_sb.sb_inoalignmt - 1;
	else
		inoalign_mask = 0;

	/*
	 * inode stripe alignment is necessary (and on) only
	 * when the geometry is such that without it, you could
	 * wind up with an otherwise aligned inode chunk that
	 * crosses a stripe boundary.
	 */
	if (mp->m_sb.sb_unit && inoalign_mask &&
			!(mp->m_sb.sb_unit & inoalign_mask))
		do_inoalign = 1;
	else
		do_inoalign = 0;
#endif

	/*
	 * pre-calculate geometry of ag 0.  We know what it looks
	 * like because we know what mkfs does -- 3 btree roots,
	 * and some number of blocks to prefill the agfl.
	 */
	bnobt_root = howmany(4 * mp->m_sb.sb_sectsize, mp->m_sb.sb_blocksize);
	bcntbt_root = bnobt_root + 1;
	inobt_root = bnobt_root + 2;
	fino_bno = inobt_root + XFS_MIN_FREELIST_RAW(1, 1, mp) + 1;

	/*
	 * ditto the location of the first inode chunks in the fs ('/')
	 */
	if (XFS_SB_VERSION_HASDALIGN(&mp->m_sb) && do_inoalign)  {
		first_prealloc_ino = XFS_OFFBNO_TO_AGINO(mp, roundup(fino_bno,
					mp->m_sb.sb_unit), 0);
	} else if (XFS_SB_VERSION_HASALIGN(&mp->m_sb) && 
					mp->m_sb.sb_inoalignmt > 1)  {
		first_prealloc_ino = XFS_OFFBNO_TO_AGINO(mp,
					roundup(fino_bno,
						mp->m_sb.sb_inoalignmt),
					0);
	} else  {
		first_prealloc_ino = XFS_OFFBNO_TO_AGINO(mp, fino_bno, 0);
	}

	assert(XFS_IALLOC_BLOCKS(mp) > 0);

	if (XFS_IALLOC_BLOCKS(mp) > 1)
		last_prealloc_ino = first_prealloc_ino + XFS_INODES_PER_CHUNK;
	else
		last_prealloc_ino = XFS_OFFBNO_TO_AGINO(mp, fino_bno + 1, 0);

	/*
	 * now the first 3 inodes in the system
	 */
	if (mp->m_sb.sb_rootino != first_prealloc_ino)  {
		do_warn(
	"sb root inode value %llu inconsistent with calculated value %llu\n",
		mp->m_sb.sb_rootino, first_prealloc_ino);

		if (!no_modify)
			do_warn(
			"resetting superblock root inode pointer to %llu\n",
				first_prealloc_ino);
		else
			do_warn(
			"would reset superblock root inode pointer to %llu\n",
				first_prealloc_ino);

		/*
		 * just set the value -- safe since the superblock
		 * doesn't get flushed out if no_modify is set
		 */
		mp->m_sb.sb_rootino = first_prealloc_ino;
	}

	if (mp->m_sb.sb_rbmino != first_prealloc_ino + 1)  {
		do_warn(
"sb realtime bitmap inode %llu inconsistent with calculated value %llu\n",
		mp->m_sb.sb_rbmino, first_prealloc_ino + 1);

		if (!no_modify)
			do_warn(
		"resetting superblock realtime bitmap ino pointer to %llu\n",
				first_prealloc_ino + 1);
		else
			do_warn(
		"would reset superblock realtime bitmap ino pointer to %llu\n",
				first_prealloc_ino + 1);

		/*
		 * just set the value -- safe since the superblock
		 * doesn't get flushed out if no_modify is set
		 */
		mp->m_sb.sb_rbmino = first_prealloc_ino + 1;
	}

	if (mp->m_sb.sb_rsumino != first_prealloc_ino + 2)  {
		do_warn(
"sb realtime summary inode %llu inconsistent with calculated value %llu\n",
		mp->m_sb.sb_rsumino, first_prealloc_ino + 2);

		if (!no_modify)
			do_warn(
		"resetting superblock realtime summary ino pointer to %llu\n",
				first_prealloc_ino + 2);
		else
			do_warn(
		"would reset superblock realtime summary ino pointer to %llu\n",
				first_prealloc_ino + 2);

		/*
		 * just set the value -- safe since the superblock
		 * doesn't get flushed out if no_modify is set
		 */
		mp->m_sb.sb_rsumino = first_prealloc_ino + 2;
	}

}

int
main(int argc, char **argv)
{
	xfs_mount_t	*temp_mp;
	xfs_mount_t	*mp;
	xfs_sb_t	*sb;
	buf_t		*sbp;
	extern		void sim_init(sim_init_t *);
	sim_init_t	simargs;
	xfs_mount_t	xfs_m;

	temp_mp = &xfs_m;
	xlog_debug = 0;		/* turn off XFS logging */
	setbuf(stdout, NULL);

	process_args(argc, argv);
	sim_init(&simargs);

	/* do phase1 to make sure we have a superblock */

	phase1(temp_mp);

	if (no_modify && primary_sb_modified)  {
		do_warn("primary superblock would have been modified.\n");
		do_warn("cannot proceed further in no_modify mode.\n");
		do_warn("exiting now.\n");
		exit(1);
	}

	mp = xfs_mount_partial(simargs.ddev, simargs.logdev, simargs.rtdev);

	if (mp == NULL)  {
		fprintf(stderr,
		"xfs_repair:  cannot repair this filesystem.  Sorry.\n");
		exit(1);
	}

	/*
	 * set XFS-independent status vars from the mount/sb structure
	 */
	glob_agcount = mp->m_sb.sb_agcount;

	chunks_pblock = mp->m_sb.sb_inopblock / XFS_INODES_PER_CHUNK;
	max_symlink_blocks = howmany(MAXPATHLEN - 1, mp->m_sb.sb_blocksize);
	inodes_per_cluster = XFS_INODE_CLUSTER_SIZE(mp) >> mp->m_sb.sb_inodelog;

	/*
	 * calculate what mkfs would do to this filesystem
	 */
	calc_mkfs(mp);

	/*
	 * check sb filesystem stats and initialize in-core data structures
	 */
	incore_init(mp);

	if (parse_sb_version(&mp->m_sb))  {
		do_warn(
		      "Found unsupported filesystem features.  Exiting now.\n");
		return(1);
	}

	/* make sure the per-ag freespace maps are ok so we can mount the fs */

	phase2(mp, &simargs);

	phase3(mp);

	phase4(mp);

	if (no_modify)
		printf("No modify flag set, skipping phase 5\n");
	else
		phase5(mp);

	/*
	 * slightly dangerous optimization here -- the earlier phases
	 * read inode buffers in MAX(blocksize, chunk-sized) pieces.
	 * The simulation will read inodes in 8K or 1 block buffers.
	 * So we invalidate the buffers to clear out the buffer cache.
	 */
	binval(mp->m_dev);

	if (!bad_ino_btree)  {
		phase6(mp);

		phase7(mp);
	} else  {
		do_warn(
	"Inode allocation btrees are too corrupted, skipping phases 6 and 7\n");
	}

	if (lost_quotas && !have_uquotino && !have_pquotino)  {
		if (!no_modify)  {
			do_warn(
	"Warning:  no quota inodes were found.  Quotas disabled.\n");
		} else  {
			do_warn(
	"Warning:  no quota inodes were found.  Quotas would be disabled.\n");
		}
	} else if (lost_quotas)  {
		if (!no_modify)  {
			do_warn(
	"Warning:  quota inodes were cleared.  Quotas disabled.\n");
		} else  {
			do_warn(
"Warning:  quota inodes would be cleared.  Quotas would be disabled.\n");
		}
	} else  {
		if (lost_uquotino)  {
			if (!no_modify)  {
				do_warn(
		"Warning:  user quota information was cleared.\n");
				do_warn(
"User quotas can not be enforced until limit information is recreated.\n");
			} else  {
				do_warn(
		"Warning:  user quota information would be cleared.\n");
				do_warn(
"User quotas could not be enforced until limit information was recreated.\n");
			}
		}

		if (lost_pquotino)  {
			if (!no_modify)  {
				do_warn(
		"Warning:  project quota information was cleared.\n");
				do_warn(
"Project quotas can not be enforced until limit information is recreated.\n");
			} else  {
				do_warn(
		"Warning:  project quota information would be cleared.\n");
				do_warn(
"Project quotas could not be enforced until limit information was recreated.\n");
			}
		}
	}

	if (no_modify)  {
		do_log(
	"No modify flag set, skipping filesystem flush and exiting.\n");
		if (fs_is_dirty)
			return(1);

		return(0);
	}

	/*
	 * Done, flush all blocks and inodes by unmounting.
	 */
	xfs_iflush_all(mp, 0);
	bflush(mp->m_dev);

	/*
	 * Clear the quota flags if they're on.
	 *
	 * XXX - should make this into a transaction
	 */
	sbp = xfs_getsb(mp, 0);
	sb = XFS_BUF_TO_SBP(sbp);

	if (sb->sb_qflags & (XFS_UQUOTA_CHKD|XFS_PQUOTA_CHKD))  {
		do_warn(
		"Note - quota info will be regenerated on next quota mount.\n");
		sb->sb_qflags &= ~(XFS_UQUOTA_CHKD|XFS_PQUOTA_CHKD);
	}

	if (clear_sunit) {
		do_warn(
"Note - stripe unit (%d) and width (%d) fields have been reset.\n" \
"Please set with mount -o sunit=<value>,swidth=<value>\n", 
			sb->sb_unit, sb->sb_width);
		sb->sb_unit = 0;
		sb->sb_width = 0;
	} 

	bwrite(sbp);

	/*
	 * The unmount will force out the superblock (again).
	 */
	xfs_umount(mp);

	if (simargs.rtdev)
		dev_close(simargs.rtdev);
	if (simargs.logdev)
		dev_close(simargs.logdev);
	dev_close(simargs.ddev);

	do_log("done\n");

	return(0);
}

