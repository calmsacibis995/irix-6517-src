/*
 * efsinfo - print number of each type of file and block usage
 *
 * efsinfo efsfilesystem
 */
static char ident[] = "@(#) efsinfo.c $Revision: 1.2 $";

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <ustat.h>

#include "libefs.h"

int ifreg, ifdir, iflnk, ififo, ifchr, ifblk, ifsock, ifind;
int blksreg, blksdir, blkslnk, blksind;

main(int argc, char **argv)
{
	char *efsfile;
	EFS_MOUNT *mp;
	efs_ino_t maxinum, inum;
	int blkcount;
	struct efs_dinode *dp;
	int nblocks;
	int debug;

	if (argc < 2) {
		fprintf(stderr, "usage: efsinfo [-d] efsfile|efsdev\n");
		exit(1);
	}

	if (strcmp(argv[1],"-d") == 0) {
		debug = 1;
		efsfile = argv[2];
	} else {
		debug = 0;
		efsfile = argv[1];
	}

	ifmountedwarn(efsfile);

	if ((mp = efs_mount(efsfile, O_RDONLY)) == 0) {
		fprintf(stderr, "efs_mount(%s, O_RDONLY) failed\n", efsfile);
		exit(1);
	}

	blkcount = 0;
	maxinum = efs_readinodes(mp, stdout, &blkcount, debug);
	if (maxinum == 0) {
		fprintf(stderr, "efs_readinodes failed\n");
		exit(1);
	}

	for (inum = 2; inum <= maxinum; inum++) {
		dp = INUMTODI(mp, inum);
		if (dp->di_mode == 0)	/* free inode */
			continue;

		switch (dp->di_mode & IFMT) {
		case IFREG:	ifreg++;	break;
		case IFDIR:	ifdir++;	break;
		case IFLNK:	iflnk++;	break;
		case IFIFO:	ififo++;	break;
		case IFCHR:	ifchr++;	break;
		case IFBLK:	ifblk++;	break;
		case IFSOCK:	ifsock++;	break;
		default:
			fprintf(stderr,"unknown mode 0x%x inum=%d\n",
				dp->di_mode & IFMT, inum);
		}

		if (dp->di_size == 0)
			continue;

		if (dp->di_numextents > EFS_DIRECTEXTENTS) {
			ifind++;
			blksind += DIDATA(dp, didata *)->indirblks;
			nblocks = DATABLKS(DIDATA(dp, didata*)->ex,
					dp->di_numextents);
		} else {
			nblocks = DATABLKS(dp->di_u.di_extents,
					dp->di_numextents);
		}

		if (S_ISDIR(dp->di_mode)) {
			blksdir += DIDATA(dp,didata*)->dirblocks;
		} else if (S_ISREG(dp->di_mode)) {
			blksreg += nblocks;
		} else if (S_ISLNK(dp->di_mode)) {
			blkslnk += nblocks;
		}
	}
	prcounts();
}

ifmountedwarn(char *name)
{
        struct ustat ustatarea;
        struct stat sb;

        if (stat(name, &sb) < 0) {
		perror(name);
		exit(1);
	}

        if (((sb.st_mode & S_IFMT) != S_IFCHR) &&
            ((sb.st_mode & S_IFMT) != S_IFBLK)) {
                return;
	}

        if (ustat(sb.st_rdev,&ustatarea) >= 0) {
                fprintf(stderr, "WARNING: %s is mounted, info may be stale\n",
                        name);
        }
}


prcounts()
{
printf(
"   ifreg    ifdir    iflnk    ififo    ifchr    ifblk   ifsock ifind   itotal\n");
printf("%8d %8d %8d %8d %8d %8d %8d %5d %8d\n",
ifreg, ifdir, iflnk, ififo, ifchr, ifblk, ifsock, ifind,
ifreg + ifdir + iflnk + ififo + ifchr + ifblk + ifsock);

printf(
" blksreg  blksdir  blkslnk                                     bkind  blksall\n");
printf("%8d %8d %8d                                     %5d %8d\n",
blksreg, blksdir, blkslnk, blksind,
blksreg + blksdir + blkslnk + blksind);
}
