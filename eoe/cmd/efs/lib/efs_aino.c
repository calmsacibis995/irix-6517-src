#ident "$Revision: 1.5 $"

/*
 * Allocate a new inode, returning the i-number.
 */
#include "efs.h"

static efs_ino_t lastino = (efs_ino_t)2;

efs_ino_t
efs_allocino(void)
{
	efs_ino_t inum;
	efs_ino_t lasti;
	struct efs_dinode *di;
	struct efs_dinode inos[EFS_INOPBB];

	inum = lastino;
	for (;;) {
		lseek(fs_fd, BBTOB(EFS_ITOBB(fs, inum)), SEEK_SET);
		if (read(fs_fd, (char *) &inos[0], sizeof(inos)) !=
			sizeof(inos))
			error();
		lasti = ((inum / EFS_INOPBB) * EFS_INOPBB) + EFS_INOPBB;
		di = &inos[inum % EFS_INOPBB];
		while (inum < lasti) {
			if (di->di_mode == 0) {
				lastino = inum + 1;
				fs->fs_tinode--;
				return (inum);
			}
			inum++;
			di++;
		}
	}
}

efs_ino_t
efs_lastallocino(void)
{
	return lastino - 1;
}
