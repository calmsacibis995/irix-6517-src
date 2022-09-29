
#define _KERNEL 1
#include <sys/param.h>
#undef _KERNEL
#include <sys/sema.h>
#define _KERNEL 1
#include <sys/buf.h>
#include <sys/sysmacros.h>
#include <sys/uio.h>
#include <sys/immu.h>
#include <sys/debug.h>
#undef _KERNEL
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <bstring.h>
#include <errno.h>
#include "sim.h"

short		MAJOR[255];

dev_t
dev_open(
	char	*path,
	int	creat,
	int	readonly,
	int	raw)
{
	int	fd;
	dev_t	devt;

	if ((fd = open(path,
			(readonly ? O_RDONLY : O_RDWR) |
			(creat ? O_CREAT|O_TRUNC : 0),
			0666)) < 0) {
		perror(path);
		exit(1);
	}

	MAJOR[fd] = (char)fd;
	devt = makedev(fd, raw);
	return (devt);
}

void
dev_close(
	dev_t	dev)
{
	int	fd;

	fd = bmajor(dev);
	fsync(fd);
	close(fd);
	MAJOR[fd] = (char)-1;
}


int
dev_grow(
	dev_t	dev,
	uint	size)
{
	int	fd;
	off_t	off;
	int	ret;

	fd = bmajor(dev);
	if ((off = lseek(fd, size - 1, SEEK_SET)) == -1)
		return -1;
	if ((uint)(off + 1) != size)
		return -1;
	if ((ret = (int)write(fd, "", 1)) == -1)
		return -1;
	if (ret != 1)
		return -1;
	return 0;
}

int
dev_zero(
	dev_t	dev,
	daddr_t	start,
	uint	len)
{
	daddr_t	bno;
	int	cdev;
	int	fd;
	uint	nblks;
	int	s;
	int	size;
	char	*z;

	size = BDSTRAT_SIZE <= BBTOB(len) ? BDSTRAT_SIZE : BBTOB(len);
	z = memalign(getpagesize(), size);
	ASSERT(z != NULL);
	bzero(z, size);
	fd = bmajor(dev);
	cdev = minor(dev) == 1;
	for (bno = start; bno < start + len; ) {
		nblks = (uint)BTOBB(size);
		if (bno + nblks > start + len)
			nblks = (uint)(start + len - bno);
		s = xwriteb(fd, z, bno, nblks, cdev);
		if (s < nblks) {
			perror("dev_zero: writeb failed");
#ifdef DEBUG
			abort();
#endif
			return 0;
		}
		bno += nblks;
	}
	free(z);
	return 1;
}
