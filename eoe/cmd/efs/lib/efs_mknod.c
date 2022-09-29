#ident "$Revision: 1.4 $"

/*
 * Create a new inode.
 */
#include "efs.h"

void
efs_mknod(efs_ino_t ino, ushort mode, ushort uid, ushort gid)
{
	struct efs_dinode *di;

	di = efs_iget(ino);
	memset((char *) di, sizeof(struct efs_dinode), 0);
	di->di_mode = mode;
	di->di_nlink = 1;
	di->di_uid = uid;
	di->di_gid = gid;
	time((time_t *)&di->di_atime);
	di->di_mtime = di->di_atime;
	di->di_ctime = di->di_atime;
	efs_iput(di, ino);
}
