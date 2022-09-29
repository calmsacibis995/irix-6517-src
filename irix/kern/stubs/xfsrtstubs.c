#ident "$Revision: 1.10 $"
/*
 * xfsrt stubs
 */
#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/vfs.h>
#include <sys/buf.h>
#include <ksys/behavior.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_dir.h>
#include <sys/fs/xfs_mount.h>

extern int nopkg(void);

int xfs_rtallocate_extent() { return nopkg(); }
int xfs_rtfree_extent() { return nopkg(); }
int xfs_growfs_rt() { return nopkg(); }
int xfs_rtpick_extent() { return nopkg(); }

int
xfs_rtmount_init(xfs_mount_t *mp)
{
	if (mp->m_sb.sb_rblocks == 0)
		return 0;
	return nopkg();
}

int
xfs_rtmount_inodes(xfs_mount_t *mp)
{
	if (mp->m_sb.sb_rblocks == 0)
		return 0;
	return nopkg();
}
