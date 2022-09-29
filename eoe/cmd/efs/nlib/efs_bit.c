#ident "$Revision: 1.2 $"

/*
 * Read/write bitmap
 */
#include "libefs.h"

int
efs_bget(EFS_MOUNT *mp)
{
	char *bitmap;
	int bmblock;

	if (mp->m_bitmap)	/* it's already there */
		return 0;

	bmblock = mp->m_fs->fs_bmblock ? mp->m_fs->fs_bmblock : EFS_BITMAPBB;
	if ((bitmap = (char *)calloc(1, EFS_BBTOB(EFS_BTOBB(mp->m_fs->fs_bmsize))))
			== NULL) {
		efs_errpr(
			"malloc failed reading bitmap, dev=0x%x\n", mp->m_dev);
		return -1;
	}
	if (efs_readb(mp->m_fd, bitmap, bmblock, EFS_BTOBB(mp->m_fs->fs_bmsize))
			!= EFS_BTOBB(mp->m_fs->fs_bmsize)) {
		efs_errpr("efs_readb failed reading bitmap, dev=0x%x: %s\n",
			mp->m_dev, strerror(errno));
		return -1;
	}
	mp->m_bitmap = bitmap;
	return 0;
}

int
efs_bput(EFS_MOUNT *mp)
{
        int bmblock;
	char *bm = mp->m_bitmap;

        bmblock = mp->m_fs->fs_bmblock ? mp->m_fs->fs_bmblock : EFS_BITMAPBB;

	if (efs_writeb(mp->m_fd, bm, bmblock, EFS_BTOBB(mp->m_fs->fs_bmsize)) !=
                        EFS_BTOBB(mp->m_fs->fs_bmsize)) {
                efs_errpr("efs_writeb failed writing bitmap, dev=0x%x: %s\n",
                        mp->m_dev, strerror(errno));
                return -1;
        }
	return 0;
}

void
efs_bfree(EFS_MOUNT *mp)
{
	free(mp->m_bitmap);
	mp->m_bitmap = 0;
}
