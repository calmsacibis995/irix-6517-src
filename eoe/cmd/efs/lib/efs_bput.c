#ident "$Revision: 1.4 $"

/*
 * Put the bitmap to the disk.
 */
#include "efs.h"

void
efs_bput(void)
{
	int bmblock;

	bmblock = fs->fs_bmblock ? fs->fs_bmblock : EFS_BITMAPBB;
	lseek(fs_fd, bmblock * BBSIZE, SEEK_SET);
	if (write(fs_fd, bitmap, BBTOB(BTOBB(fs->fs_bmsize))) !=
		BBTOB(BTOBB(fs->fs_bmsize)))
		error();
}
