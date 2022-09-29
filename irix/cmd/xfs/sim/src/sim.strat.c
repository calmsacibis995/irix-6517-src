
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

char	bdstrat_buf[BDSTRAT_SIZE];

/* ARGSUSED */
void
bdopen(int maj, dev_t *dev, int a2, int a3, int a4)
{
	return;
}

/* ARGSUSED */
void
bdclose(int maj, dev_t dev, int a2, int a3, int a4)
{
	return;
}

void
_bdstrat(int maj, buf_t *bp)
{
	int	s;
	char	*addr;
	int	pgbboff;
		/* REFERENCED */
	int	error;
	int	cdev;
	uio_t	uio;
	iovec_t	iovec;

	if ((maj < 0) || (maj > 255)) {
		printf("bdstrat bad maj %d\n", maj);
		exit(1);
	}

	cdev = minor(bp->b_edev) == 1;
	if (bp->b_flags & B_PAGEIO) {
		ASSERT(bp->b_bcount <= BDSTRAT_SIZE);
		addr = bdstrat_buf;
	} else {
		addr = bp->b_un.b_addr;
	}

	if (bp->b_flags & B_READ) {
		s = xreadb(maj, addr, bp->b_blkno, (int)BTOBB(bp->b_bcount),
			cdev);
		if (s == -1) {
			perror("bdstrat: readb failed");
#ifdef DEBUG
			abort();
#endif
			errno = EIO;
		} else
			s = BBTOB(s);
		if (s < bp->b_bcount) {
			bp->b_flags |= B_ERROR;
			bp->b_error = (char)errno;
		}

		if (bp->b_flags & B_PAGEIO) {
			iovec.iov_base = addr;
			iovec.iov_len = bp->b_bcount;
			uio.uio_iov = &iovec;
			uio.uio_iovcnt = 1;
			uio.uio_offset = BBTOB(bp->b_offset);
			uio.uio_resid = bp->b_bcount;
			uio.uio_segflg = UIO_SYSSPACE;
			uio.uio_limit = 0xffffffff;
			uio.uio_fmode = 0;
			pgbboff = (int)dpoff(bp->b_offset);
			error = biomove(bp, pgbboff, bp->b_bcount,
					UIO_WRITE, &uio);
			ASSERT(error == 0);
		}

	} else {
		if (bp->b_flags & B_PAGEIO) {
			iovec.iov_base = addr;
			iovec.iov_len = bp->b_bcount;
			uio.uio_iov = &iovec;
			uio.uio_iovcnt = 1;
			uio.uio_offset = BBTOB(bp->b_offset);
			uio.uio_resid = bp->b_bcount;
			uio.uio_segflg = UIO_SYSSPACE;
			uio.uio_limit = 0xffffffff;
			uio.uio_fmode = 0;
			pgbboff = (int)dpoff(bp->b_offset);
			error = biomove(bp, pgbboff, bp->b_bcount,
					UIO_READ, &uio);
			ASSERT(error == 0);
		}

		s = xwriteb(maj, addr, bp->b_blkno, (int)BTOBB(bp->b_bcount),
			cdev);
		if (s == -1) {
			perror("bdstrat: writeb failed");
#ifdef DEBUG
			abort();
#endif
			errno = EIO;
		} else
			s = BBTOB(s);
		if (s < bp->b_bcount) {
			bp->b_flags |= B_ERROR;
			bp->b_error = (char)errno;
		}
	}

	iodone(bp);
}

void
bdstrat(struct bdevsw *devsw, buf_t *bp)
{
	__psint_t dev = (__psint_t)devsw;
	int maj;

	maj = bmajor(dev);
	bp->b_edev = makedev(maj, minor(bp->b_edev));
	_bdstrat(bmajor(bp->b_edev), bp);
}
