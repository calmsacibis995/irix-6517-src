#ident "$Revision: 1.3 $"

/*
 * Get the bitmap off the disk.
 */
#include "efs.h"

void
efs_bget(void)
{
	int bmblock;

	bmblock = fs->fs_bmblock ? fs->fs_bmblock : EFS_BITMAPBB;
	lseek(fs_fd, bmblock * BBSIZE, SEEK_SET);
	bitmap = (char *) calloc(1, BBTOB(BTOBB(fs->fs_bmsize)));
	if (!bitmap) {
		fprintf(stderr, "%s: out of memory\n", progname);
		exit(1);
	}
	if (read(fs_fd, bitmap, BBTOB(BTOBB(fs->fs_bmsize))) !=
		BBTOB(BTOBB(fs->fs_bmsize)))
		error();
}
