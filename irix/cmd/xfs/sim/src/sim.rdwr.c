#include <sys/param.h>
#include <unistd.h>
#include "sim.h"

#define	BBTOOFF64(bbs)	(((off64_t)(bbs)) << BBSHIFT)

/* ARGSUSED */
int
xreadb(
	int	fd,
	void	*buf,
	daddr_t	bno,
	int	nblks,
	int	israw)
{
	int	rval;

	if (lseek64(fd, BBTOOFF64(bno), SEEK_SET) < 0)
		return -1;
	rval = (int)read(fd, buf, BBTOB(nblks));
	if (rval < 0)
		return -1;
	else
		return (int)BTOBBT(rval);
}

/* ARGSUSED */
int
xwriteb(
	int	fd,
	void	*buf,
	daddr_t	bno,
	int	nblks,
	int	israw)
{
	int	rval;

	if (lseek64(fd, BBTOOFF64(bno), SEEK_SET) < 0)
		return -1;
	rval = (int)write(fd, buf, BBTOB(nblks));
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
	return ftruncate64(fd, (off64_t)size);
}
