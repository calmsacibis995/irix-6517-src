#ident "$Revision: 1.4 $"

/*
 * Write into a file.  Data is always appended.
 */
#include "efs.h"

void
efs_write(efs_ino_t ino, char *data, int len)
{
	struct efs_dinode *di;
	int byteoffset, amount;
	efs_daddr_t bn;
	char buf[BBSIZE];

	di = efs_iget(ino);
	while (len) {
		byteoffset = di->di_size % BBSIZE;
		amount = len;
		if (byteoffset + amount > BBSIZE)
			amount = BBSIZE - byteoffset;
		bn = efs_bmap(di, di->di_size, amount);
		lseek(fs_fd, BBTOB(bn), SEEK_SET);
		if (byteoffset) {
			if (read(fs_fd, buf, BBSIZE) != BBSIZE)
				error();
		}
		bcopy(data, &buf[byteoffset], amount);
		lseek(fs_fd, BBTOB(bn), SEEK_SET);
		if (write(fs_fd, buf, BBSIZE) != BBSIZE)
			error();
		data += amount;
		len -= amount;
	}
	efs_iput(di, ino);
}
