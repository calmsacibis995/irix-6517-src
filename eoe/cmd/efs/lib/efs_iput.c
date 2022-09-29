#ident "$Revision: 1.4 $"

/*
 * Put an efs inode to the disk.
 */
#include "efs.h"

void
efs_iput(struct efs_dinode *di, efs_ino_t ino)
{
	static struct efs_dinode inos[EFS_INOPBB];

	lseek(fs_fd, BBTOB(EFS_ITOBB(fs, ino)), SEEK_SET);
	if (read(fs_fd, (char *) &inos[0], sizeof(inos)) != sizeof(inos))
		error();
	inos[ino % EFS_INOPBB] = *di;
	lseek(fs_fd, BBTOB(EFS_ITOBB(fs, ino)), SEEK_SET);
	if (write(fs_fd, (char *) &inos[0], sizeof(inos)) != sizeof(inos))
		error();
}
