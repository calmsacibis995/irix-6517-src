/*
 * Dump out the superblock of an efs.
 *
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/efs/tools/RCS/dumpsb.c,v $
 * $Revision: 1.2 $
 * $Date: 1997/06/03 20:58:07 $
 */
#include "efs.h"

char *progname;
struct efs *fs;
int fs_fd;
union {
	struct	efs fs;
	char	block[BBTOB(BTOBB(sizeof(struct efs)))];
} sblock;

main(argc, argv)
	int argc;
	char *argv[];
{
	register int i;
	efs_daddr_t bn;

	progname = argv[0];
	if (argc < 2) {
		fprintf(stderr, "usage: %s special\n", progname);
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

	printf("size=%d firstcg=%d cgfsize=%d cgisize=%d sectors=%d\n",
			fs->fs_size, fs->fs_firstcg, fs->fs_cgfsize,
			fs->fs_cgisize, fs->fs_sectors);
	printf("heads=%d ncg=%d bmsize=%d time=%s",
			 fs->fs_heads, fs->fs_ncg,
			 fs->fs_bmsize, ctime(&fs->fs_time));
	printf("magic=%x dirty=%d fname=\"%.6s\" fpack=\"%.6s\"\n",
			 fs->fs_magic, fs->fs_dirty, fs->fs_fname,
			 fs->fs_fpack);
	printf("tinode=%d tfree=%d checksum=%x\n",
			  fs->fs_tinode, fs->fs_tfree, fs->fs_checksum);

	printf(" cg firstbn firsti lastbn lasti firstdbn\n");
	for (i = 0; i < fs->fs_ncg; i++) {
		bn = fs->fs_firstcg + i * fs->fs_cgfsize;
		printf("%3d %7d %6d %6d %5d %8d\n",
			    i, bn, i * fs->fs_ipcg,
			    bn + fs->fs_cgfsize - 1,
			    (i + 1) * fs->fs_ipcg - 1,
			    bn + fs->fs_cgisize);
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
