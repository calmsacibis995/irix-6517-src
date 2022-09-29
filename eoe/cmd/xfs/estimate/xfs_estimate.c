#ident "$Revision: 1.12 $"
/*
 * Copyright 1994, Silicon Graphics, Inc.
 */
/*
 * estimate space of an xfs filesystem
 * bugs - doesn't handle real-time partition
 * author: jeffg@sgi.com
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/var.h>
#include <sys/buf.h>
#include <sys/uuid.h>
#include <sys/vnode.h>
#include <sys/uuid.h>
#include <sys/cred.h>
#include <sys/sysmacros.h>
#include <sys/mman.h>
#include <sys/dirent.h>
#include <sys/kmem.h>
#include <stdio.h>
#include <ftw.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <diskinfo.h>
#include <bstring.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <libgen.h>
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
#include <sys/fs/xfs_ialloc.h>
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#include <sys/fs/xfs_dir2_sf.h>
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_inode.h>
#include <sys/fs/xfs_alloc.h>
#include <sys/fs/xfs_bmap.h>
#include <sys/fs/xfs_rtalloc.h>
#include <sys/fs/xfs_bit.h>
#include <sys/fs/xfs_da_btree.h>
#include <sys/fs/xfs_dir_leaf.h>

/* stolen from xfs_mkfs.c */
STATIC unsigned long long
cvtnum(char *s)
{
    unsigned long long i;
    char *sp;

    i = strtoll(s, &sp, 0);
    if (i == 0 && sp == s)
	return 0LL;
    if (*sp == '\0')
	return i;
    if (*sp =='k' && sp[1] == '\0')
	return 1024LL * i;
    if (*sp =='m' && sp[1] == '\0')
	return 1024LL * 1024LL * i;
    if (*sp =='g' && sp[1] == '\0')
	return 1024LL * 1024LL * 1024LL * i;
    return 0LL;
}

int ffn(const char *, const struct stat64 *, int, struct FTW *);

#define BLOCKSIZE	4096
#define INODESIZE	256
#define PERDIRENTRY	\
	(sizeof(xfs_dir_leaf_entry_t) + sizeof(xfs_dir_leaf_name_t))
#define LOGSIZE		1000

#define FBLOCKS(n) ((n)/blocksize)
#define RFBYTES(n) ((n) - (FBLOCKS(n) * blocksize))

unsigned long long dirsize=0;		/* bytes */
unsigned long long logsize=LOGSIZE*BLOCKSIZE;	/* bytes */
unsigned long long fullblocks=0;	/* FS blocks */
unsigned long long isize=0;		/* inodes bytes */
unsigned long long blocksize=BLOCKSIZE;
unsigned long long nslinks=0;		/* number of symbolic links */
unsigned long long nfiles=0;		/* number of regular files */
unsigned long long ndirs=0;		/* number of directories */
unsigned long long nspecial=0;		/* number of special files */
unsigned long long verbose=0;		/* verbose mode TRUE/FALSE */

int __debug = 0;
int ilog = 0;
int elog = 0;

void
help(char *progname)
{
    printf("usage:\t%s [opts] directory [directory ...]\n", basename(progname));
    printf("\t-b blocksize (fundamental filesystem blocksize)\n");
    printf("\t-i logsize (internal log size)\n");
    printf("\t-e logsize (external log size)\n");
    printf("\t-v prints more verbose messages\n");
    printf("\t-h prints this usage message\n");
    printf("note:\tblocksize may have 'k' appended to indicate x1024\n");
    printf("\tlogsize may also have 'm' appended to indicate (1024 x 1024)\n");
}

main(int argc, char **argv)
{
    unsigned long long est;
    extern int optind;
    extern char *optarg;
    char dname[40];
    int c;

    while ((c = getopt (argc, argv, "b:hdve:i:")) != EOF) {
	switch (c) {
	case 'b':
	    blocksize=cvtnum(optarg);
	    if (blocksize <= 0LL) {
		printf("blocksize %llu too small\n", blocksize);
		help(argv[0]);
		exit(1);
	    } else if (blocksize > 64LL * 1024LL) {
		printf("blocksize %llu too large\n", blocksize);
		help(argv[0]);
		exit(1);
	    }
	    break;
	case 'i':
	    if (elog) {
		printf("already have external log noted, can't have both\n");
		help(argv[0]);
		exit(1);
	    }
	    logsize=cvtnum(optarg);
	    ilog++;
	    break;
	case 'e':
	    if (ilog) {
		printf("already have internal log noted, can't have both\n");
		help(argv[0]);
		exit(1);
	    }
	    logsize=cvtnum(optarg);
	    elog++;
	    break;
	case 'v':
	    verbose = 1;
	    break;
	case 'd':
	    __debug++;
	    break;
	default:
	case 'h':
	    help(argv[0]);
	    exit(0);
	}
    }

    if (optind == argc) {
	help(argv[0]);
	exit(1);
    }

    if (!elog && !ilog) {
	ilog=1;
	logsize=LOGSIZE * blocksize;
    }
    if (verbose)
	printf("directory                               bsize   blocks    megabytes    logsize\n");

    for ( ; optind < argc; optind++) {
	dirsize=0LL;		/* bytes */
	fullblocks=0LL;		/* FS blocks */
	isize=0LL;		/* inodes bytes */
	nslinks=0LL;		/* number of symbolic links */
	nfiles=0LL;		/* number of regular files */
	ndirs=0LL;		/* number of directories */
	nspecial=0LL;		/* number of special files */

	nftw64(argv[optind], ffn, 40, FTW_PHYS | FTW_MOUNT);

	if (__debug) {
	    printf("dirsize=%llu\n", dirsize);
	    printf("fullblocks=%llu\n", fullblocks);
	    printf("isize=%llu\n", isize);

	    printf("%llu regular files\n", nfiles);
	    printf("%llu symbolic links\n", nslinks);
	    printf("%llu directories\n", ndirs);
	    printf("%llu special files\n", nspecial);
	}

	est = FBLOCKS(isize) + 8	/* blocks for inodes */
	    + FBLOCKS(dirsize) + 1	/* blocks for directories */
	    + fullblocks  		/* blocks for file contents */
	    + (8 * 16)			/* fudge for overhead blks (per ag) */
	    + FBLOCKS(isize / INODESIZE); /* add 1 byte/inode for maps */

	if (ilog)
	    est += (logsize / blocksize);

	if (!verbose) {
	    printf("%s will take about %.1f megabytes\n",
		argv[optind], (double)est*(double)blocksize/(1024.0*1024.0));
	} else {
	    /* avoid running over 39 characters in field */
	    strncpy(dname, argv[optind], 40);
	    dname[39] = '\0';
	    printf("%-39s %5llu %8llu %10.1fMB %10llu\n",
		dname, blocksize, est,
		(double)est*(double)blocksize/(1024.0*1024.0), logsize);
	}

	if (!verbose && elog) {
	    printf("\twith the external log using %llu blocks ",
		logsize/blocksize);
	    printf("or about %.1f megabytes\n",
		(double)logsize/(1024.0*1024.0));
	}
    }
    return 0;
}

/* ARGSUSED */
int ffn(const char *path, const struct stat64 *stb, int flags, struct FTW *f)
{
    /* cases are in most-encountered to least-encountered order */
    dirsize+=PERDIRENTRY+strlen(path);
    isize+=INODESIZE;
    switch (S_IFMT & stb->st_mode) {
    case S_IFREG:			/* regular files */
	fullblocks+=FBLOCKS(stb->st_blocks * 512 + blocksize-1);
	if (stb->st_blocks * 512 < stb->st_size)
	    fullblocks++;		/* add one bmap block here */
	nfiles++;
	break;
    case S_IFLNK:			/* symbolic links */
	if (stb->st_size >= (INODESIZE - (sizeof(xfs_dinode_t)+4)))
	    fullblocks+=FBLOCKS(stb->st_size + blocksize-1);
	nslinks++;
	break;
    case S_IFDIR:			/* directories */
	dirsize+=blocksize;	/* fudge upwards */
	if (stb->st_size >= blocksize)
	    dirsize+=blocksize;
	ndirs++;
	break;
    case S_IFIFO:			/* named pipes */
    case S_IFCHR:			/* Character Special device */
    case S_IFBLK:			/* Block Special device */
    case S_IFSOCK:			/* socket */
	nspecial++;
	break;
    }
    return 0;
}
