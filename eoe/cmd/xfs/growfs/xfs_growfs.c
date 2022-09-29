#include <sys/param.h>
#include <sys/stat.h>
#include <sys/syssgi.h>
#include <sys/statvfs.h>
#include <sys/dkio.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/fs/xfs_fsops.h>
#include <diskinvent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <mntent.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

long long getsize(dev_t, int);
char *makechr(dev_t, int, int *);
void usage(void);

char	*tmp;	/* location of temp files */

main(int argc, char **argv)
{
	int			aflag;	/* fake flag, do all pieces */
	int			c;	/* current option character */
	long long		ddsize;	/* device size in 512-byte blocks */
	int			dfd;	/* device file descriptor */
	int			dflag;	/* -d flag */
	int			dirversion; /* directory version number */
	long long		dlsize;	/* device size in 512-byte blocks */
	long long		drsize;	/* device size in 512-byte blocks */
	long long		dsize;	/* new data size in fs blocks */
	int			error;	/* we have hit an error */
	long			esize;	/* new rt extent size */
	int			ffd;	/* mount point file descriptor */
	char			*fname;	/* mount point name */
	xfs_fsop_geom_t		geo;	/* current fs geometry */
	xlv_getdev_t		getdev;	/* xlv devices */
	int			iflag;	/* -i flag */
	int			isint;	/* log is currently internal */
	int			ispart;	/* set if is partition/lv not xlv */
	int			istemp;	/* makechr path is temporary */
	int			lflag;	/* -l flag */
	long long		lsize;	/* new log size in fs blocks */
	int			maxpct;	/* -m flag value */
	int			mflag;	/* -m flag */
	int			nflag;	/* -n flag */
	xfs_fsop_geom_t		ngeo;	/* new fs geometry */
	char			*path;	/* device pathname */
	int			rflag;	/* -r flag */
	long long		rsize;	/* new rt size in fs blocks */
	struct stat64		stb;	/* stat structure */
	struct statvfs64	stv;	/* statvfs structure */
	int			unwritten; /* unwritten extent flag */
	int			xflag;	/* -x flag */

	opterr = 0;
	aflag = dflag = iflag = lflag = mflag = nflag = rflag = xflag = 0;
	esize = 0;
	dsize = lsize = rsize = 0LL;
	while ((c = getopt(argc, argv, "dD:e:ilL:m:nrR:x")) != EOF) {
		switch (c) {
		case 'D':
			dsize = atoll(optarg);
			/* fall through */
		case 'd':
			dflag = 1;
			break;
		case 'e':
			esize = atol(optarg);
			rflag = 1;
			break;
		case 'i':
			lflag = iflag = 1;
			break;
		case 'L':
			lsize = atoll(optarg);
			/* fall through */
		case 'l':
			lflag = 1;
			break;
		case 'm':
			mflag = 1;
			maxpct = atoi(optarg);
			break;
		case 'n':
			nflag = 1;
			break;
		case 'R':
			rsize = atoll(optarg);
			/* fall through */
		case 'r':
			rflag = 1;
			break;
		case 'x':
			lflag = xflag = 1;
			break;
		case '?':
			usage();
		}
	}
	if (argc - optind != 1)
		usage();
	if (iflag && xflag)
		usage();
	if (dflag + lflag + rflag == 0)
		aflag = 1;
	fname = argv[optind];
	ffd = open(fname, O_RDONLY);
	if (ffd < 0 || fstat64(ffd, &stb) < 0 || fstatvfs64(ffd, &stv) < 0) {
		perror(fname);
		return 1;
	}
	if (!S_ISDIR(stb.st_mode)) {
		fprintf(stderr, "%s is not a directory name\n", fname);
		return 1;
	}
	if (strcmp(stv.f_basetype, MNTTYPE_XFS) != 0) {
		fprintf(stderr, "%s is not in an XFS filesystem\n", fname);
		return 1;
	}
	if ((tmp = getenv("TMPDIR")) == NULL)
		tmp = "/tmp";
	path = makechr(stb.st_dev, 0, &istemp);
	dfd = open(path, O_RDONLY);
	if (istemp)
		unlink(path);
	free(path);
	if (dfd < 0) {
		perror("open temp device");
		return 1;
	}
	ispart = ioctl(dfd, DIOCGETXLVDEV, &getdev) < 0;
	close(dfd);
	if (syssgi(SGI_XFS_FSOPERATIONS, ffd, XFS_FS_GEOMETRY, (void *)0, &geo) < 0) {
		perror("syssgi(SGI_XFS_FSOPERATIONS: XFS_FS_GEOMETRY)");
		return 1;
	}
	isint = geo.logstart > 0;
	unwritten = geo.flags & XFS_FSOP_GEOM_FLAGS_EXTFLG ? 1 : 0;
	dirversion = geo.flags & XFS_FSOP_GEOM_FLAGS_DIRV2 ? 2 : 1;
	printf("meta-data=%-22s isize=%-6d agcount=%d, agsize=%d blks\n"
	       "data     =%-22s bsize=%-6d blocks=%lld, imaxpct=%d\n"
	       "         =%-22s sunit=%-6d swidth=%d blks, unwritten=%d\n"
	       "naming   =version %-14d bsize=%-6d\n"
	       "log      =%-22s bsize=%-6d blocks=%d\n"
	       "realtime =%-22s extsz=%-6d blocks=%lld, rtextents=%lld\n",
		fname, geo.inodesize, geo.agcount, geo.agblocks,
		"", geo.blocksize, geo.datablocks, geo.imaxpct,
		"", geo.sunit, geo.swidth, unwritten,
		dirversion, geo.dirblocksize,
		isint ? "internal" : "external", geo.blocksize, geo.logblocks,
		geo.rtblocks ? "external" : "none",
		geo.rtextsize * geo.blocksize, geo.rtblocks, geo.rtextents);
	if (ispart) {
		ddsize = getsize(stb.st_dev, 0);
		dlsize = geo.logblocks * (geo.blocksize / BBSIZE);
		drsize = 0;
	} else {
		ddsize = getsize(getdev.data_subvol_dev, 1);
		dlsize = getdev.log_subvol_dev ?
			getsize(getdev.log_subvol_dev, 1) :
			geo.logblocks * (geo.blocksize / BBSIZE);
		drsize = getdev.rt_subvol_dev ?
			getsize(getdev.rt_subvol_dev, 1) : 0;
	}
	error = 0;
	if (dflag | aflag) {
		xfs_growfs_data_t	in;

		if (!mflag)
			maxpct = geo.imaxpct;
		if (!dsize)
			dsize = ddsize / (geo.blocksize / BBSIZE);
		else if (dsize > ddsize / (geo.blocksize / BBSIZE)) {
			fprintf(stderr,
				"data size %lld too large, maximum is %lld\n",
				dsize, ddsize / (geo.blocksize / BBSIZE));
			error = 1;
		}
		if (!error && dsize < geo.datablocks) {
			fprintf(stderr, "data size %lld too small, old size is %lld\n",
				dsize, geo.datablocks);
			error = 1;
		} else if (!error &&
			   dsize == geo.datablocks && maxpct == geo.imaxpct) {
			if (dflag)
				fprintf(stderr,
					"data size unchanged, skipping\n");
			if (mflag)
				fprintf(stderr,
					"inode max pct unchanged, skipping\n");
		} else if (!error && !nflag) {
			in.newblocks = dsize;
			in.imaxpct = maxpct;
			if (syssgi(SGI_XFS_FSOPERATIONS, ffd, XFS_GROWFS_DATA,
					&in, (void *)NULL) < 0) {
				if (errno == EWOULDBLOCK)
					fprintf(stderr,
			"conflicting growfs operation in progress already\n");
				else
					perror(
			"syssgi(SGI_XFS_FSOPERATIONS: XFS_GROWFS_DATA)");
				error = 1;
			}
		}
	}
	if (!error && (rflag | aflag)) {
		xfs_growfs_rt_t	in;

		if (!esize)
			esize = (long)geo.rtextsize;
		if (!rsize)
			rsize = drsize / (geo.blocksize / BBSIZE);
		else if (rsize > drsize / (geo.blocksize / BBSIZE)) {
			fprintf(stderr,
			"realtime size %lld too large, maximum is %lld\n",
				rsize, drsize / (geo.blocksize / BBSIZE));
			error = 1;
		}
		if (!error && rsize < geo.rtblocks) {
			fprintf(stderr,
			"realtime size %lld too small, old size is %lld\n",
				rsize, geo.rtblocks);
			error = 1;
		} else if (!error && rsize == geo.rtblocks) {
			if (rflag)
				fprintf(stderr,
					"realtime size unchanged, skipping\n");
		} else if (!error && !nflag) {
			in.newblocks = rsize;
			in.extsize = esize;
			if (syssgi(SGI_XFS_FSOPERATIONS, ffd, XFS_GROWFS_RT,
					&in, (void *)NULL) < 0) {
				if (errno == EWOULDBLOCK)
					fprintf(stderr,
			"conflicting growfs operation in progress already\n");
				else if (errno == ENOSYS)
					fprintf(stderr,
					"realtime growth not supported yet\n");
				else
					perror(
				"syssgi(SGI_XFS_FSOPERATIONS: XFS_GROWFS_RT)");
				error = 1;
			}
		}
	}
	if (!error && (lflag | aflag)) {
		xfs_growfs_log_t	in;

		if (!lsize)
			lsize = dlsize / (geo.blocksize / BBSIZE);
		if (iflag)
			in.isint = 1;
		else if (xflag)
			in.isint = 0;
		else if (ispart)
			in.isint = 1;
		else
			in.isint = getdev.log_subvol_dev == 0;
		if (lsize == geo.logblocks && (in.isint == isint)) {
			if (lflag)
				fprintf(stderr,
					"log size unchanged, skipping\n");
		} else if (!nflag) {
			in.newblocks = lsize;
			if (syssgi(SGI_XFS_FSOPERATIONS, ffd, XFS_GROWFS_LOG,
					&in, (void *)NULL) < 0) {
				if (errno == EWOULDBLOCK)
					fprintf(stderr,
			"conflicting growfs operation in progress already\n");
				else if (errno == ENOSYS)
					fprintf(stderr,
					"log growth not supported yet\n");
				else
					perror(
				"syssgi(SGI_XFS_FSOPERATIONS: XFS_GROWFS_LOG)");
				error = 1;
			}
		}
	}
	if (nflag)
		return 0;
	if (syssgi(SGI_XFS_FSOPERATIONS, ffd, XFS_FS_GEOMETRY, (void *)0, &ngeo) < 0) {
		perror("syssgi(SGI_XFS_FSOPERATIONS: XFS_FS_GEOMETRY)");
		return 1;
	}
	if (geo.datablocks != ngeo.datablocks)
		printf("data blocks changed from %lld to %lld\n",
			geo.datablocks, ngeo.datablocks);
	if (geo.imaxpct != ngeo.imaxpct)
		printf("inode max percent changed from %d to %d\n",
			geo.imaxpct, ngeo.imaxpct);
	if (geo.logblocks != ngeo.logblocks)
		printf("log blocks changed from %d to %d\n",
			geo.logblocks, ngeo.logblocks);
	if ((geo.logstart == 0) != (ngeo.logstart == 0))
		printf("log changed from %s to %s\n",
			geo.logstart ? "internal" : "external",
			ngeo.logstart ? "internal" : "external");
	if (geo.rtblocks != ngeo.rtblocks)
		printf("realtime blocks changed from %lld to %lld\n",
			geo.rtblocks, ngeo.rtblocks);
	if (geo.rtextsize != ngeo.rtextsize)
		printf("realtime extent size changed from %d to %d\n",
			geo.rtextsize, ngeo.rtextsize);
	return 0;
}

long long
getsize(dev_t dev, int isxlv)
{
	int		istemp;	/* makechr path is a temporary */
	char		*path;	/* device pathname */
	long long	rval;	/* size of device in BBs */

	path = makechr(dev, isxlv, &istemp);
	rval = findsize(path);
	if (istemp)
		unlink(path);
	free(path);
	return rval;
}

char *
makechr(dev_t dev, int isxlv, int *istemp)
{
	char	path[MAXPATHLEN];
	int	pathlen = sizeof(path);

	if (dev_to_rawpath(dev, path, &pathlen, NULL) != NULL) {
		*istemp = 0;
		return strdup(path);
	}
	if (!isxlv) {
		fprintf(stderr, "no raw device for filesystem\n");
		exit(1);
	}
	strcpy(path, "/dev/rxlv/xfsXXXXXX");
	(void)mktemp(path);
	if (mknod(path, S_IFCHR | 0600, dev) < 0) {
		perror("mknod");
		exit(1);
	}
	*istemp = 1;
	return strdup(path);
}

void
usage(void)
{
	fprintf(stderr, "usage: xfs_growfs [-dlr] [-n] [-ix] [-D|L|R size] [-e extsize] [-m imaxpct] fsys\n");
	fprintf(stderr, "\t-d\tgrow data/metadata section\n");
	fprintf(stderr, "\t-l\tgrow log section\n");
	fprintf(stderr, "\t-r\tgrow realtime section\n");
	fprintf(stderr, "\t-n\tdon't change anything, just show geometry\n");
	fprintf(stderr, "\t-i\tconvert log from external to internal\n");
	fprintf(stderr, "\t-x\tconvert log from internal to external\n");
	fprintf(stderr, "\t-D size\tgrow data/metadata section to size blks\n");
	fprintf(stderr, "\t-L size\tgrow/shrink log section to size blks\n");
	fprintf(stderr, "\t-R size\tgrow realtime section to size blks\n");
	fprintf(stderr, "\t-e size\tset rt extent size to size blks\n");
	fprintf(stderr, "\t-m imaxpct\tset inode max percent to imaxpct\n");
	fprintf(stderr, "\tfsys\tthe file system mount point\n");
	exit(2);
	/* NOTREACHED */
}
