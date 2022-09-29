#ident "$Revision: 1.5 $"

/*
 * Make the lost+found directory.  Just grow it, sinces its already
 * got "." and "..".
 */
#include "efs.h"

void
efs_mklostandfound(efs_ino_t ino)
{
	int i;
	char blkbuf[EFS_DIRBSIZE];

	/* just append some empty blocks */
	bzero(blkbuf, sizeof(blkbuf));
#ifdef	EFS
	((struct efs_dirblk *)blkbuf)->magic = EFS_DIRBLK_MAGIC;
#endif
	for (i = 0; i < 1024*10; i += sizeof(blkbuf)) {
		efs_write(ino, blkbuf, sizeof(blkbuf));
	}
}
