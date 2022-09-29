#ident "$Revision: 1.3 $"

#include <sys/types.h>
#include <sys/sema.h>
#include <sys/uuid.h>
#include <sys/vfs.h>
#include <sys/buf.h>

#include "xfs_macros.h"
#include "xfs_types.h"
#include "xfs_inum.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_dir.h"
#include "xfs_dir2.h"
#include "xfs_mount.h"
#include "xfs_bmap_btree.h"
#include "xfs_attr_sf.h"
#include "xfs_dir_sf.h"
#include "xfs_dir2_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"

/*
 * Return 32 bits of the Inode number from the xfs fid structure.
 */
unsigned int
xfs_get_fid_ino(struct fid *fidp)
{
	return ((xfs_fid_t *)fidp)->fid_ino;
}

/*
 * Return the generation number from the xfs fid structure.
 */
unsigned int
xfs_get_fid_gen(struct fid *fidp)
{
	return ((xfs_fid_t *)fidp)->fid_gen;
}

/*
 * Initialize the fid structure field with values for inode and generation
 * numbers. (Only works for 32-bit inode numbers!)
 */
void
xfs_set_fid_fields(struct fid *fidp, unsigned int ino, unsigned int gen)
{
	xfs_fid_t *xfsfid = (xfs_fid_t *) fidp;

	xfsfid->fid_len = sizeof(xfs_fid_t) - sizeof(xfsfid->fid_len);
	xfsfid->fid_pad = 0;
	xfsfid->fid_gen = gen;
	xfsfid->fid_ino = ino;
}

/*
 * Return the filesystem name from the XFS mount structure.
 */
char *
xfs_get_mount_fsname(struct vfs *vfsp)
{
	char *fsname;
	bhv_desc_t *bdp;

	bdp = bhv_lookup_unlocked(VFS_BHVHEAD(vfsp), &xfs_vfsops);
	fsname = (char *)(XFS_BHVTOM(bdp))->m_fsname;
	return fsname;
}
