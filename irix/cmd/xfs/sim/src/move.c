/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)uts-comm:mem/move.c	1.5"*/
#ident	"$Revision: 1.8 $"

#define	_KERNEL	1
#include <sys/types.h>
#include <sys/buf.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/pfdat.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/immu.h>

#include "sim.h"

int
biomove(register struct buf *bp,
	register u_int pboff,
	register size_t n,
	enum uio_rw rw,
	register struct uio *uiop)
{
	register pfd_t *pfd;
	register int len;
	register caddr_t vaddr;
	int error;

	if (n == 0)
		return 0;

	if (BP_ISMAPPED(bp)) {
		return uiomove(bp->b_un.b_addr + pboff, n, rw, uiop);
	}


	pboff += BBTOB(dpoff(bp->b_offset));
	pfd = bp->b_pages;
	while (pboff >= NBPP) {
		pfd = pfd->pf_pchain;
		pboff -= NBPP;
	}

	len = (int)MIN(n, NBPP - pboff);

	do {
		ASSERT(pfd);
		vaddr = page_mapin(pfd, (bp->b_flags & B_UNCACHED ?
					 VM_UNCACHED : 0), 0);
		error = uiomove(vaddr + pboff, len, rw, uiop);
		page_mapout(vaddr);
		pboff = 0;
		pfd = pfd->pf_pchain;
		n -= len;
		len = (int)MIN(n, NBPP);
	} while (!error && n != 0);

	return error;
}

/*
 * Move "n" bytes at byte address "cp"; "rw" indicates the direction
 * of the move, and the I/O parameters are provided in "uio", which is
 * update to reflect the data which was moved.  Returns 0 on success or
 * a non-zero errno on failure.
 */
int
uiomove(void *cp, size_t n, enum uio_rw rw, struct uio *uio)
{
	register struct iovec *iov;
	u_int cnt;
	int error;

	while (n > 0 && uio->uio_resid) {
		iov = uio->uio_iov;
		cnt = (u_int)iov->iov_len;
		if (cnt == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			continue;
		}
		if (cnt > n)
			cnt = (u_int)n;
		switch (uio->uio_segflg) {

		case UIO_USERSPACE:
			if (rw == UIO_READ)
				error = copyout(cp, iov->iov_base, cnt);
			else
				error = copyin(iov->iov_base, cp, cnt);
			if (error)
				return EFAULT;
			break;

		case UIO_USERISPACE:
			ASSERT(0);
			break;

		case UIO_SYSSPACE:
			if (rw == UIO_READ)
				bcopy(cp, iov->iov_base, cnt);
			else
				bcopy(iov->iov_base, cp, cnt);
			break;
		}
		iov->iov_base = (void *)((char *)iov->iov_base + cnt);
		iov->iov_len -= cnt;
		uio->uio_resid -= cnt;
		uio->uio_offset += cnt;
		cp = (void *)((char *)cp + cnt);
		n -= cnt;
	}
	return 0;
}

#ifdef _VCE_AVOIDANCE
void
biozero(register struct buf *bp,
	register u_int pboff,
	register int n)
{
	register pfd_t *pfd;
	register int len;
	register caddr_t vaddr;
	int error;

	if (n == 0)
		return;

	if (BP_ISMAPPED(bp)) {
		bzero(bp->b_un.b_addr + pboff, n);
		return;
	}


	pboff += BBTOB(dpoff(bp->b_offset));
	pfd = bp->b_pages;
	while (pboff >= NBPP) {
		pfd = pfd->pf_pchain;
		pboff -= NBPP;
	}

	len = MIN(n, NBPP - pboff);

	do {
		ASSERT(pfd);
		vaddr = page_mapin(pfd, (bp->b_flags & B_UNCACHED ?
					 VM_UNCACHED : 0), 0);
		bzero(vaddr + pboff, len);
		page_mapout(vaddr);
		pboff = 0;
		pfd = pfd->pf_pchain;
		n -= len;
		len = MIN(n, NBPP);
	} while (!error && n != 0);
}

#endif
