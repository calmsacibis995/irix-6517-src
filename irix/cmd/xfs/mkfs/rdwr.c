#ident "$Revision: 1.5 $"

#include <sys/param.h>
#include <sys/syssgi.h>
#include <unistd.h>
#include "sim.h"

int
xreadb(
	int	fd,
	void	*buf,
	daddr_t	bno,
	int	nblks,
	int	israw)
{
	int	rval;

	if (israw)
		rval = (int)syssgi(SGI_READB, fd, buf, bno, nblks);
	else {
		if (lseek(fd, BBTOB(bno), SEEK_SET) < 0)
			return -1;
		rval = (int)read(fd, buf, BBTOB(nblks));
	}
	if (rval < 0)
		return -1;
	else
		return (int)BTOBBT(rval);
}

int
xwriteb(
	int	fd,
	void	*buf,
	daddr_t	bno,
	int	nblks,
	int	israw)
{
	int	rval;

	if (israw)
		rval = (int)syssgi(SGI_WRITEB, fd, buf, bno, nblks);
	else {
		if (lseek(fd, BBTOB(bno), SEEK_SET) < 0)
			return -1;
		rval = (int)write(fd, buf, BBTOB(nblks));
	}
	if (rval < 0)
		return -1;
	else
		return (int)BTOBBT(rval);
}

int
growfile(
	int		fd,
	long long	size)
{
	return ftruncate(fd, (off_t)size);
}
