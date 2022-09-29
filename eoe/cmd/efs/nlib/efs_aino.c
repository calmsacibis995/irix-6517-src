#ident "$Revision: 1.3 $"

/*
 * Allocate a new inode, returning the i-number.
 */
#include "libefs.h"

efs_ino_t
efs_allocino(EFS_MOUNT *mp)
{
	static efs_ino_t lastino = (efs_ino_t)2;
	efs_ino_t inum;
	efs_ino_t lasti;
	struct efs_dinode *di;
	struct efs_dinode *inos;

	inos = (struct efs_dinode *)malloc(EFS_INOPBB);

	inum = lastino;
	for (;;) {
		if (efs_readb(mp->m_fd, (char *) &inos[0],
			EFS_ITOBB(mp->m_fs, inum), 1) != 1)
			error();
		lasti = ((inum / EFS_INOPBB) * EFS_INOPBB) + EFS_INOPBB;
		di = &inos[inum % EFS_INOPBB];
		while (inum < lasti) {
			if (di->di_mode == 0) {
				lastino = inum + 1;
				mp->m_fs->fs_tinode--;
				free(inos);
				return (inum);
			}
			inum++;
			di++;
		}
	}
}
