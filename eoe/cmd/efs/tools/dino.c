/*
 * Dump an inode out.  This is not meant as a complete diagnostic tool, just
 * something handy to look at inodes.
 *
 * Written by: Kipp Hickman
 *
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/efs/tools/RCS/dino.c,v $
 * $Revision: 1.4 $
 * $Date: 1997/06/03 20:58:06 $
 */
#include "efs.h"

char *progname;
int fs_fd;
struct efs *fs;
union {
	struct	efs fs;
	char	data[BBTOB(BTOBB(sizeof(struct efs)))];
} sblock;

main(argc, argv)
	int argc;
	char *argv[];
{
	efs_ino_t inum;
	register struct efs_dinode *di;
	register extent *ex;
	register int i;

	progname = argv[0];
	if (argc != 3) {
		fprintf(stderr, "usage: %s special inumber\n", progname);
		exit(-1);
	}

	if ((fs_fd = open(argv[1], O_RDONLY)) < 0) {
		fprintf(stderr, "%s: can't open %s\n", progname, argv[1]);
		exit(-1);
	}
	fs = &sblock.fs;
	if (efs_mount()) {
		fprintf(stderr, "%s: %s is not an extent filesystem\n",
				progname, argv[1]);
		exit(-1);
	}
	inum = atoi(argv[2]);
	if ((inum < EFS_ROOTINO) ||
	    (inum > fs->fs_ipcg * fs->fs_ncg)) {
		fprintf(stderr, "%s: inode number %d is not valid\n",
				progname, inum);
		exit(-1);
	}

	di = efs_iget(inum);
	printf("inode %d at disk block %d offset %d\n",
		      inum, EFS_ITOBB(fs, inum),
		      EFS_ITOO(fs, inum) * sizeof(struct efs_dinode));
	printf("mode=%07o nlink=%d uid=%d gid=%d size=%d\n",
			  di->di_mode, di->di_nlink, di->di_uid,
			  di->di_gid, di->di_size);
	printf("atime=%s", ctime(&di->di_atime));
	printf("mtime=%s", ctime(&di->di_mtime));
	printf("ctime=%s", ctime(&di->di_ctime));
	printf("gen=%d numextents=%d vers=%d dev=%x\n",
		       di->di_gen, di->di_numextents, di->di_version,
		       di->di_u.di_dev);
	if (di->di_numextents > EFS_DIRECTEXTENTS)
		dumpindir(di);
	else
	if (di->di_numextents) {
		printf("Extent Magic #   Disk Block   Length   Offset    Cg\n");
		ex = &di->di_u.di_extents[0];
		for (i = 0; i < di->di_numextents; i++, ex++) {
			printf("%6d  %6d %12d   %6d   %6d  %4d\n",
				     i, ex->ex_magic, ex->ex_bn,
				     ex->ex_length,
				     ex->ex_offset,
				     EFS_BBTOCG(fs, ex->ex_bn));
		}
	}
}

/*
 * Print out indirect extent information
 */
dumpindir(di)
    register struct efs_dinode *di;
{
    register struct extent *ex;
    register int i, j, k;
    char buf[BBSIZE];
    int left;
    char header = 0;
    long bn;

    left = di->di_numextents;
    for (i = 0; i < di->di_u.di_extents[0].ex_offset; i++) {
	printf("Indirect extent #%d: mag=%d bn=%d len=%d\n",
			 i,
			 di->di_u.di_extents[i].ex_magic,
			 di->di_u.di_extents[i].ex_bn,
			 di->di_u.di_extents[i].ex_length);
    }
    for (i = 0; i < di->di_u.di_extents[0].ex_offset; i++) {
	bn = di->di_u.di_extents[i].ex_bn;
	for (j = 0; j < di->di_u.di_extents[i].ex_length; j++, bn++) {
	    lseek(fs_fd, BBTOB(bn), 0);
	    if (read(fs_fd, buf, BBSIZE) != BBSIZE)
		    error();
	    ex = (extent *) buf;
	    for (k = 0; k < BBSIZE / sizeof(extent); k++, ex++) {
		if (!header) {
		    printf("Extent Magic #   Disk Block   Length   Offset    Cg\n");
		    header = 1;
		}
		printf("%6d  %6d %12d   %6d   %6d  %4d\n",
			     (j * BBSIZE / sizeof(extent)) + k,
			     ex->ex_magic,
			     ex->ex_bn,
			     ex->ex_length,
			     ex->ex_offset, EFS_BBTOCG(fs, ex->ex_bn));
		if (--left == 0)
			return;
	    }
	}
    }
}

error()
{
	int old_errno;

	old_errno = errno;
	fprintf(stderr, "%s: i/o error\n", progname);
	errno = old_errno;
	perror(progname);
	exit(-1);
}
