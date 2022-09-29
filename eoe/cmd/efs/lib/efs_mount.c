#ident "$Revision: 1.4 $"

/*
 * "Mount" the efs filesystem - read in the superblock and initialize it.
 */
#include "efs.h"

int
efs_mount(void)
{
	lseek(fs_fd, (long) EFS_SUPERBOFF, SEEK_SET);
	if (read(fs_fd, (char *) fs, BBTOB(BTOBB(sizeof(*fs)))) !=
		BBTOB(BTOBB(sizeof(*fs))))
		error();
	if (!IS_EFS_MAGIC(fs->fs_magic)) 
		return (-1);
	EFS_SETUP_SUPERB(fs);
	return (0);
}
