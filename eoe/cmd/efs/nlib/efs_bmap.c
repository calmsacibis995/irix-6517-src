#ident "$Revision: 1.3 $"

/*
 * Map a byte offset in an inode to a disk block number.
 */
#include "libefs.h"

efs_daddr_t
efs_bmap(EFS_MOUNT *mp, struct efs_dinode *di, off_t offset, off_t count)
{
	off_t fileoff;
	extent *ex;

#ifdef BBDEBUG
	printf("efs_bmap: di->di_size == 0x%x\n", di->di_size);
#endif

	/* insure that file is big enough, first */
	if (offset + count > di->di_size)
		efs_extend(mp, di, offset + count - di->di_size);

	offset = EFS_BTOBBT(offset);
	fileoff = 0;
	ex = &di->di_u.di_extents[0];
	while (fileoff + ex->ex_length <= offset) {
		fileoff += ex->ex_length;
		ex++;
	}

	/*
	 * Block lies within this extent.  Return where.
	 */
	return (ex->ex_bn + offset - fileoff);
}
