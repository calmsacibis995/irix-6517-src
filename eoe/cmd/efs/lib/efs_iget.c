#ident "$Revision: 1.4 $"

/*
 * Get an efs inode from the disk.
 */
#include "efs.h"

struct efs_dinode *
efs_iget(efs_ino_t ino)
{
	static struct efs_dinode inos[EFS_INOPBB];

	lseek(fs_fd, BBTOB(EFS_ITOBB(fs, ino)), SEEK_SET);
	if (read(fs_fd, (char *) &inos[0], sizeof(inos)) != sizeof(inos))
		error();
	return &inos[ino % EFS_INOPBB];
}
