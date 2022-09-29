/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)uts-comm:mem/move.c	1.5"*/
#ident	"$Revision: 3.34 $"

#include <sys/types.h>
#include <sys/buf.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/pfdat.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/immu.h>

/*
 * copy n bytes from from to to.  This doesn't handle the case
 * where data is moved from user seg to user seg.
 */
int
gencopy(
	register void *from,
	register void *to,
	register int n,
	register int bcopyok)
{
	if (n==0)
		return 0;

	if (IS_KUSEG(from) && IS_KUSEG(to))
		return 1;

	if (IS_KUSEG(from))
		return copyin(from,to,n);

	if (IS_KUSEG(to))
		return copyout(from,to,n);

	if( bcopyok ) {
		bcopy(from,to,n);
		return 0;
	}

	return 1;
}

int
biomove(register struct buf *bp,
	register u_int pboff,
	register size_t n,
	enum uio_rw rw,
	register struct uio *uiop)
{
	register pfd_t *pfd;
	register size_t len;
	register caddr_t vaddr;
	int error;

	if (n == 0)
		return 0;

	if (BP_ISMAPPED(bp)) {
		return uiomove(bp->b_un.b_addr + pboff, n, rw, uiop);
	}

	pfd = bp->b_pages;
	while (pboff >= NBPP) {
		pfd = pfd->pf_pchain;
		pboff -= NBPP;
	}

	len = MIN(n, NBPP - pboff);

	do {
		ASSERT(pfd);
    		vaddr = page_mapin_pfn(pfd,
			(bp->b_flags & B_UNCACHED ? VM_UNCACHED : 0),
			0, pfdattopfn(pfd));
		error = uiomove(vaddr + pboff, len, rw, uiop);
		if (IS_KSEG2(vaddr))
			kvfree(vaddr, 1);
		pboff = 0;
		pfd = pfd->pf_pchain;
		n -= len;
		len = MIN(n, NBPP);
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
	size_t cnt;
	int error;

	while (n > 0 && uio->uio_resid) {
		iov = uio->uio_iov;
		cnt = iov->iov_len;
		if (cnt == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			continue;
		}
		if (cnt > n)
			cnt = n;
		switch (uio->uio_segflg) {

		case UIO_USERSPACE:
		case UIO_USERISPACE:
			if (rw == UIO_READ)
				error = copyout(cp, iov->iov_base, cnt);
			else
				error = copyin(iov->iov_base, cp, cnt);
			if (error)
				return EFAULT;
			break;

		case UIO_SYSSPACE:
			if (rw == UIO_READ)
				bcopy(cp, iov->iov_base, cnt);
			else
				bcopy(iov->iov_base, cp, cnt);
			break;
		}
		iov->iov_base = (char *)iov->iov_base + cnt;
		iov->iov_len -= cnt;
		uio->uio_resid -= cnt;
		uio->uio_offset += cnt;
		cp = (char *)cp + cnt;
		n -= cnt;
	}
	return 0;
}

/* function: ureadc()
 * purpose:  transfer a character value into the address space
 *           delineated by a uio and update fields within the
 *           uio for next character. Return 0 for success, EFAULT
 *           for error.
 */

int
ureadc(int val, register struct uio *uiop)
{
	register struct iovec *iovp;
	unsigned char c;

	/*
	 * first determine if uio is valid.  uiop should be
	 * non-NULL and the resid count > 0.
	 */
	if (!(uiop && uiop->uio_resid > 0))
		return EFAULT;

	/*
	 * scan through iovecs until one is found that is non-empty.
	 * Return EFAULT if none found.
	 */
	for (;;) {
		if (uiop->uio_iovcnt <= 0)
			return EFAULT;
		iovp = uiop->uio_iov;
		if (iovp->iov_len > 0)
			break;
		uiop->uio_iovcnt--;
		uiop->uio_iov++;
	}

	/*
	 * Transfer character to uio space.
	 */
	c = val;

	switch (uiop->uio_segflg) {

	case UIO_USERISPACE:
	case UIO_USERSPACE:
		if (copyout(&c, iovp->iov_base, sizeof c))
			return EFAULT;
		break;

	case UIO_SYSSPACE:	/* can do direct copy since kernel-kernel */
		*(unsigned char *)iovp->iov_base = c;
		break;

	default:
		return EFAULT;	/* invalid segflg value */
	}

	/*
	 * bump up/down iovec and uio members to reflect transfer.
	 */
	iovp->iov_base = (char *)iovp->iov_base + 1;
	iovp->iov_len--;
	uiop->uio_resid--;
	uiop->uio_offset++;
	return 0;
}


/* function: uwritec()
 * purpose:  return a character value from the address space
 *           delineated by a uio and update fields within the
 *           uio for next character. Return the character for success,
 *           -1 for error.
 */

int
uwritec(register struct uio *uiop)
{
	register struct iovec *iovp;
	unsigned char c;

	/* verify we were passed a valid uio structure.
	 * (1) non-NULL uiop, (2) positive resid count
	 * (3) there is an iovec with positive length
	 */

	if (!(uiop && uiop->uio_resid > 0))
		return -1;

	for (;;) {
		if (uiop->uio_iovcnt <= 0)
			return -1;
		iovp = uiop->uio_iov;
		if (iovp->iov_len > 0)
			break;
		uiop->uio_iovcnt--;
		uiop->uio_iov++;
	}

	/*
	 * Get the character from the uio address space.
	 */
	switch (uiop->uio_segflg) {

	case UIO_USERISPACE:
	case UIO_USERSPACE:
		if (copyin(iovp->iov_base, &c, sizeof c))
			return -1;
		break;

	case UIO_SYSSPACE:
		c = *(char *)iovp->iov_base;
		break;

	default:
		return -1; /* invalid segflg */
	}

	/*
	 * Adjust fields of iovec and uio appropriately.
	 */
	iovp->iov_base = (char *)iovp->iov_base + 1;
	iovp->iov_len--;
	uiop->uio_resid--;
	uiop->uio_offset++;
	return c;
}

/*
 * uioupdate -
 * Update "uio" to reflect that "n" bytes of data were
 * (or were not) moved.  The state of the iovecs are not
 * updated and are not consistent with uio_resid.  Positive
 * "n" means n bytes were copied.  Negative "n" means "uncopy"
 * n bytes.
 */
void
uioupdate(struct uio *uiop, size_t n)
{
	if ((n > 0) && (n > uiop->uio_resid))
		n = uiop->uio_resid;
	uiop->uio_resid -= n;
	uiop->uio_offset += n;
}

/*
 * Drop the next n chars out of *uiop.
 */
void
uioskip(register uio_t	*uiop, register size_t n)
{
	if (n > uiop->uio_resid)
		return;
	while (n != 0) {
		register iovec_t	*iovp = uiop->uio_iov;
		register size_t		niovb = MIN(iovp->iov_len, n);

		if (niovb == 0) {
			uiop->uio_iov++;
			uiop->uio_iovcnt--;
			continue;
		}
		iovp->iov_base = (char *)iovp->iov_base + niovb;
		uiop->uio_offset += niovb;
		iovp->iov_len -= niovb;
		uiop->uio_resid -= niovb;
		n -= niovb;
	}
}

/*
 * Move MIN(ruio->uio_resid, wuio->uio_resid) bytes from addresses described
 * by ruio to those described by wuio.  Both uio structures are updated to
 * reflect the move. Returns 0 on success or a non-zero errno on failure.
 */
int
uiomvuio(
	register uio_t *ruio,
	register uio_t *wuio)
{
	register iovec_t *riov;
	register iovec_t *wiov;
	register size_t n;
	size_t cnt;
	int kerncp, error;

	n = MIN(ruio->uio_resid, wuio->uio_resid);
	kerncp = ruio->uio_segflg == UIO_SYSSPACE &&
		 wuio->uio_segflg == UIO_SYSSPACE;

	riov = ruio->uio_iov;
	wiov = wuio->uio_iov;
	while (n) {
		while (!wiov->iov_len) {
			wiov = ++wuio->uio_iov;
			wuio->uio_iovcnt--;
		}
		while (!riov->iov_len) {
			riov = ++ruio->uio_iov;
			ruio->uio_iovcnt--;
		}
		cnt = MIN(wiov->iov_len, MIN(riov->iov_len, n));

		if (kerncp)
			bcopy(riov->iov_base, wiov->iov_base, cnt);
		else {
			error = (ruio->uio_segflg == UIO_SYSSPACE) ?
				copyout(riov->iov_base, wiov->iov_base, cnt) :
				copyin(riov->iov_base, wiov->iov_base, cnt);
			if (error)
				return error;
		}

		riov->iov_base = (char *)riov->iov_base + cnt;
		riov->iov_len -= cnt;
		ruio->uio_resid -= cnt;
		ruio->uio_offset += cnt;
		wiov->iov_base = (char *)wiov->iov_base + cnt;
		wiov->iov_len -= cnt;
		wuio->uio_resid -= cnt;
		wuio->uio_offset += cnt;
		n -= cnt;
	}
	return 0;
}

/*
 * NOTE: these should be recoded for speed
 * XXXbe no kidding -- really, these should go the way of the dodo
 *       (copyinstr and copystr are the new functions).
 */

/*
 *	Read in a pathname from kernel space.
 *	RETURNS:
 *		-1 - if supplied address was not valid
 *		-2 - if pathname length is > maxbufsize - 1
 *		length otherwise (not including '\0')
 */
static int
spath(caddr_t from, caddr_t to, int maxbufsize)
{
	register caddr_t source = from;

	while (--maxbufsize >= 0) {
		if ((*to++ = *from++) == 0)
			return (from - source) - 1;
	}
	return -2;
}

int
copyinstr(
	char *from,	/* Source address (user space) */
	char *to,	/* Destination address */
	size_t max,	/* Maximum number of characters to move */
	size_t *np)	/* Number of characters moved is returned here */
{
	int n;

	if (np)
		*np = 0;
	switch (n = upath(from, to, max)) {
	case -2:
		return ENAMETOOLONG;
	case -1:
		return EFAULT;
	default:
		if (np)
			*np = n;	/* Include null byte */
		return 0;
	}
	/* NOTREACHED */
}

int
copystr(
	char *from,	/* Source address (system space) */
	char *to,	/* Destination address */
	size_t max,	/* Maximum number of characters to move */
	size_t *np)	/* Number of characters moved is returned here */
{
	int n;

	if (np)
		*np = 0;
	switch (n = spath(from, to, max)) {
	case -2:
		return ENAMETOOLONG;
	case -1:
		return EFAULT;
	default:
		if (np)
			*np = n + 1;	/* Include null byte */
		return 0;
	}
	/* NOTREACHED */
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
#ifdef _VCE_AVOIDANCE
		if (vce_avoidance) {
			int	color;
			int	old_color;
			color = vcache_color(pfd->pf_pageno);
			old_color = pfd_to_vcolor(pfd);
			if (old_color < 0)
				pfd_set_vcolor(pfd,color);
			else if (old_color != color)
				(void) color_validation(pfd,color,0,0);
		}
#endif /* _VCE_AVOIDANCE */
		vaddr = page_mapin(pfd, (bp->b_flags & B_UNCACHED ?
					 VM_UNCACHED : 0), 0);
		bzero(vaddr + pboff, len);
		page_mapout(vaddr);
		pboff = 0;
		pfd = pfd->pf_pchain;
		n -= len;
		len = MIN(n, NBPP);
	} while (n != 0);
}

#endif
