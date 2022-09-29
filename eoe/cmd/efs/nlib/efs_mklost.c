#ident "$Revision: 1.3 $"

/*
 * Make the lost+found directory.  Just grow it, sinces its already
 * got "." and "..".
 */
#include "libefs.h"

void
efs_mklostandfound(EFS_MOUNT *mp, efs_ino_t ino)
{
	int i;
	char *blkbuf;

	blkbuf = (char *)malloc(EFS_DIRBSIZE);

	/* just append some empty blocks */
	bzero(blkbuf, EFS_DIRBSIZE);
	((struct efs_dirblk *)blkbuf)->magic = EFS_DIRBLK_MAGIC;
	for (i = 0; i < EFS_BBTOB(EFS_BTOBB(1024*10)); i += EFS_DIRBSIZE) {
		efs_write(mp, ino, blkbuf, EFS_DIRBSIZE);
	}

	free(blkbuf);
}
