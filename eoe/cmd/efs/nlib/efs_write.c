#ident "$Revision: 1.3 $"

/*
 * Write into a file.  Data is always appended.
 */
#include "libefs.h"

void
efs_write(EFS_MOUNT *mp, efs_ino_t ino, char *data, int len)
{
	struct efs_dinode *di;
	int byteoffset, amount;
	efs_daddr_t bn;
	int fd = mp->m_fd;
	char buf[EFS_BIG_BBSIZE];

	di = efs_iget(mp, ino);
	while (len) {
		byteoffset = di->di_size % EFS_BBSIZE;
		amount = len;
		if (byteoffset + amount > EFS_BBSIZE)
			amount = EFS_BBSIZE - byteoffset;
		bn = efs_bmap(mp, di, di->di_size, amount);
		lseek(fd, EFS_BBTOB(bn), SEEK_SET);
		if (byteoffset) {
			if (read(fd, buf, EFS_BBSIZE) != EFS_BBSIZE)
				error();
		}
		bcopy(data, &buf[byteoffset], amount);
		lseek(fd, EFS_BBTOB(bn), SEEK_SET);
		if (write(fd, buf, EFS_BBSIZE) != EFS_BBSIZE)
			error();
		data += amount;
		len -= amount;
	}
	efs_iput(mp, di, ino);
}
