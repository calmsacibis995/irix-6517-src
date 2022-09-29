#ident "$Revision: 1.4 $"

/*
 * Map a byte offset in an inode to a disk block number.
 */
#include "efs.h"

efs_daddr_t
efs_bmap(struct efs_dinode *di, off_t offset, off_t count)
{
	off_t fileoff;
	extent *ex;

	/* insure that file is big enough, first */
	if (offset + count > di->di_size)
		efs_extend(di, offset + count - di->di_size);

	offset = BTOBBT(offset);
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
