#ident "$Revision: 1.3 $"

/*
 * Update the superblock to the efs disk.  Recompute the checksum and
 * update the time stamp.
 */
#include "efs.h"

void
efs_update(void)
{
	time((time_t *)&fs->fs_time);
	efs_checksum();
	lseek(fs_fd, (long) EFS_SUPERBOFF, SEEK_SET);
	if (write(fs_fd, (char *) fs, BBTOB(BTOBB(sizeof(*fs)))) !=
		 BBTOB(BTOBB(sizeof(*fs))))
		error();
	if (bitmap) {
		lseek(fs_fd, (long) EFS_BITMAPBOFF, SEEK_SET);
		if (write(fs_fd, bitmap, BBTOB(BTOBB(fs->fs_bmsize))) !=
			 BBTOB(BTOBB(fs->fs_bmsize)))
			error();
	}
}
