#ident "$Revision: 1.5 $"

#include "libefs.h"

/*
 * Because the (4.0.X) kernel may spin forever in
 * rdwrb()->iphysio()->iomap() calling bmapwait() if we try to readb
 * something big (like a 2048 blocks).  256 is arbitrarily smaller.
 */
#define	MAXPHYSIO	256

efs_rdwrb(int rdwr, int fd, char *buf, efs_daddr_t blk, int nblks)
{
	int ret;

	lseek64(fd, BBSIZE * (off64_t)blk, SEEK_SET);
	if (rdwr == SGI_READB)
		ret = read(fd, buf, BBSIZE * nblks);
	else
		ret = write(fd, buf, BBSIZE * nblks);
	return ret == -1 ? -1 : ret/BBSIZE;
}
