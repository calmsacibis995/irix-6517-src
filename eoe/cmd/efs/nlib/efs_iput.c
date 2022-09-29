#ident "$Revision: 1.6 $"

/*
 * Write EFS inodes.
 */
#include "libefs.h"

int 
efs_iput(EFS_MOUNT *mp, struct efs_dinode *dip, efs_ino_t ino)

{
  	struct efs_dinode inos[EFS_INOPBB];

	if (efs_readb(mp->m_fd, (char*) inos, EFS_ITOBB(mp->m_fs, ino), 1)!=1) {
		efs_errpr("efs_readb(inode %d) failed: %s\n", strerror(errno));
		return -1;
	}

	memcpy(&(inos[ino % EFS_INOPBB]), dip, sizeof(struct efs_dinode));

	if (efs_writeb(mp->m_fd, (char*) inos, EFS_ITOBB(mp->m_fs, ino), 1)!=1){
		efs_errpr("efs_writeb(inode %d) failed: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}
