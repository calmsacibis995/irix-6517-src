/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.22 88/02/08 
 */
#ident "$Revision: 3.28 $"


/*
 * xdr_mbuf.c, XDR implementation on kernel mbufs.
 */

#include "types.h"
#include <sys/debug.h>
#include <sys/mbuf.h>
#include <sys/mbuf_private.h>
#include <sys/pfdat.h>
#include <sys/buf.h>
#include <sys/systm.h>
#include <sys/tcp-param.h>
#include <sys/var.h>
#include <netinet/in.h>
#include "xdr.h"
#include "nfs.h"
#include "auth.h"
#include "svc.h"
#include "nfs_impl.h"

bool_t	xdrmbuf_getlong(), xdrmbuf_putlong(XDR *, long *);
bool_t	xdrmbuf_getbytes(), xdrmbuf_putbytes();
u_int	xdrmbuf_getpos(XDR *xdrs);
bool_t	xdrmbuf_setpos();
long *	xdrmbuf_inline();
void	xdrmbuf_destroy();
bool_t	xdrmbuf_getint(register XDR *, int *);
bool_t	xdrmbuf_putint(register XDR *, int *);
void	rbhold(struct mbuf *);
void	rbrele(struct mbuf *);

#define XDR_MISALIGNED(x)	\
	(((u_long)(x)&(sizeof(xdr_long_t)-1)) != 0)

/*
 * Xdr on mbufs operations vector.
 */
struct	xdr_ops xdrmbuf_ops = {
	xdrmbuf_getlong,
	xdrmbuf_putlong,
#if (_MIPS_SZLONG != _MIPS_SZINT)
	xdrmbuf_getint,
	xdrmbuf_putint,
#else
#define xdrmbuf_putint(x,p) xdrmbuf_putlong(x, (long *)(p))
#endif
	xdrmbuf_getbytes,
	xdrmbuf_putbytes,
	xdrmbuf_getpos,
	xdrmbuf_setpos,
	xdrmbuf_inline,
	xdrmbuf_destroy
};

/* move xdrs to point to next mbuf in chain */

#define xdrmbuf_pullup(xdrs)						\
{									\
	if ((xdrs)->x_base) {						\
		register struct mbuf *m =				\
			((struct mbuf *)(xdrs)->x_base)->m_next;	\
									\
		(xdrs)->x_base = (caddr_t)m;				\
		if (m == NULL) {					\
			(xdrs)->x_handy = 0;				\
			return (FALSE);					\
		}							\
		(xdrs)->x_private = mtod(m, caddr_t);			\
		(xdrs)->x_handy = m->m_len;				\
	} else {							\
		return (FALSE);						\
	}								\
}

/*
 * Initailize xdr stream.
 */
void
xdrmbuf_init(xdrs, m, op)
	register XDR		*xdrs;
	register struct mbuf	*m;
	enum xdr_op		 op;
{

	xdrs->x_op = op;
	xdrs->x_ops = &xdrmbuf_ops;
	xdrs->x_base = (caddr_t)m;
	xdrs->x_private = mtod(m, caddr_t);
	xdrs->x_handy = m->m_len;
}

/* ARGSUSED */
void
xdrmbuf_destroy(xdrs)
	XDR	*xdrs;
{
	/* do nothing */
}

bool_t
xdrmbuf_getlong(xdrs, lp)
	register XDR	*xdrs;
	long		*lp;
{
	bool_t xdrmbuf_getbytes(XDR *, caddr_t, u_int);

	if (xdrs->x_handy <= 0)
		xdrmbuf_pullup(xdrs);
	if (xdrs->x_handy < sizeof(xdr_long_t) || 
	    XDR_MISALIGNED(xdrs->x_private)) {
		xdr_long_t value;

		if (xdrmbuf_getbytes(xdrs, (caddr_t)&value,
				     sizeof(xdr_long_t)) == FALSE)
			return (FALSE);
		*lp = ntohl(value);
		return (TRUE);
	}
	*lp = ntohl(*((xdr_long_t *)(xdrs->x_private)));
	xdrs->x_private += sizeof(xdr_long_t);
	xdrs->x_handy -= sizeof(xdr_long_t);
	return (TRUE);
}

#define BYTES_TO_GET NBPP /* get more mbuf space in this increment */

bool_t
xdrmbuf_putlong(xdrs, lp)
	register XDR	*xdrs;
	long		*lp;
{

	if ((xdrs->x_handy -= sizeof(xdr_long_t)) < 0) {
		ASSERT(xdrs->x_handy == -sizeof(int));
		if (xdrs->x_base) {
			register struct mbuf *m;

			m = m_vget(M_WAIT, BYTES_TO_GET, MT_DATA);
			((struct mbuf *)xdrs->x_base)->m_next = m;
			xdrs->x_base = (caddr_t)m;
			xdrs->x_private = mtod(m, caddr_t);
			xdrs->x_handy = m->m_len - sizeof(xdr_long_t);
		} else {
			return (FALSE);
		}
	}
	*(xdr_long_t *)xdrs->x_private = htonl(*lp);
	xdrs->x_private += sizeof(xdr_long_t);
	return (TRUE);
}

#if (_MIPS_SZLONG != _MIPS_SZINT)
bool_t
xdrmbuf_getint(
	register XDR	*xdrs,
	int		*ip)
{
	bool_t xdrmbuf_getbytes(XDR *, caddr_t, u_int);

	if (xdrs->x_handy <= 0)
		xdrmbuf_pullup(xdrs);
	if (xdrs->x_handy < sizeof(int) || 
	    XDR_MISALIGNED(xdrs->x_private)) {
		int value;
		
		if (xdrmbuf_getbytes(xdrs, (caddr_t)&value,
				     sizeof(int)) == FALSE)
			return (FALSE);
		*ip = ntohl(value);
		return (TRUE);
	}
	*ip = ntohl(*((int *)(xdrs->x_private)));
	xdrs->x_private += sizeof(int);
	xdrs->x_handy -= sizeof(int);
	return (TRUE);
}

bool_t
xdrmbuf_putint(
	register XDR	*xdrs,
	int		*ip)
{

	if ((xdrs->x_handy -= sizeof(int)) < 0) {
		ASSERT(xdrs->x_handy == -sizeof(int));
		if (xdrs->x_base) {
			register struct mbuf *m;

			m = m_vget(M_WAIT, BYTES_TO_GET, MT_DATA);
			((struct mbuf *)xdrs->x_base)->m_next = m;
			xdrs->x_base = (caddr_t)m;
			xdrs->x_private = mtod(m, caddr_t);
			xdrs->x_handy = m->m_len - sizeof(int);
		} else {
			return (FALSE);
		}
	}
	*(int *)xdrs->x_private = htonl(*ip);
	xdrs->x_private += sizeof(int);
	return (TRUE);
}
#endif	/* _MIPS_SZLONG != _MIPS_SZINT */

bool_t
xdrmbuf_getbytes(xdrs, addr, len)
	register XDR	*xdrs;
	caddr_t		 addr;
	register u_int	 len;
{
	while ((xdrs->x_handy -= len) < 0) {
		if ((xdrs->x_handy += len) > 0) {
			bcopy(xdrs->x_private, addr, (u_int)xdrs->x_handy);
			addr += xdrs->x_handy;
			len -= xdrs->x_handy;
		}
		xdrmbuf_pullup(xdrs);
	}
	bcopy(xdrs->x_private, addr, (u_int)len);
	xdrs->x_private += len;
	return (TRUE);
}

/*
 * If the remainder of the bytes can be described as a chain of
 * NBPP-sized pages (ie. pfd_t's), chain them via pf_pchain and
 * return a pointer to the head of the chain. If we can't do this,
 * just return NULL.
 */
bool_t
xdrmbuf_getpfdchain(xdrs, voidp)
	register XDR	*xdrs;
	register void	*voidp;
{
	register pfd_t	**pfdp = (pfd_t **)voidp;
	register struct mbuf *m;
	pfd_t *pfd, *nextpfd;
	u_int len, curlen;

	if (xdrs->x_handy != sizeof(int) ||
	    XDR_GETINT(xdrs, (int *)&len) == FALSE)
	{
		return (FALSE);
	}
	if (len % NBPP != 0)
		goto exit_undo_getint; /* return (FALSE) */

	if (len == 0) {
		*pfdp = NULL;
		return (TRUE);
	}
	m = ((struct mbuf *)xdrs->x_base)->m_next;
	curlen = 0;
	while (m && curlen < len) {
		if (m->m_len != NBPP || m->m_p == NULL ||
		    mtod(m, caddr_t) != m->m_p || m->m_refp != &m->m_ref ||
		    *m->m_refp != 1 || m->m_freefunc)
		{
			goto exit_undo_getint; /* return (FALSE) */
		}
		curlen += NBPP;
		m = m->m_next;
	}
	if (curlen != len)
		goto exit_undo_getint; /* return (FALSE) */

	/* We can do it */

	m = ((struct mbuf *)xdrs->x_base)->m_next;
	pfd = kvatopfdat(m->m_p);
	*pfdp = pfd;
	while (len) {
		len -= NBPP;
		nextpfd = (len > 0) ? kvatopfdat(m->m_next->m_p) : NULL;

		if (m_stealcl(m) == -1)
			panic("xdrmbuf_getpfdchain unable to steal page from mbuf");

		pfd->pf_pchain = nextpfd;
		pfd_setflags(pfd, P_DONE);
		pfd = nextpfd;
		m = m->m_next;
	}
	xdrs->x_base = (caddr_t)m;
	if (m) {
		xdrs->x_private = mtod(m, caddr_t);
		xdrs->x_handy = m->m_len;
	} else {
		xdrs->x_private = NULL;
		xdrs->x_handy = 0;
	}
	return (TRUE);

exit_undo_getint:
	xdrs->x_private -= sizeof(int);
	xdrs->x_handy = sizeof(int);
	return (FALSE);
}

/*
 * Sort of like getbytes except that instead of getting
 * bytes we return the mbuf that contains all the rest
 * of the bytes.
 */
bool_t
xdrmbuf_getmbuf(xdrs, mm, lenp)
	register XDR	*xdrs;
	register struct mbuf **mm;
	register u_int *lenp;
{
	register struct mbuf *m;
	register int len, used;

	if (! xdr_u_int(xdrs, lenp)) {
		return (FALSE);
	}
	m = (struct mbuf *)xdrs->x_base;
	used = m->m_len - xdrs->x_handy;
	m->m_off += used;
	m->m_len -= used;
	*mm = m;
	/*
	 * Consistency check.
	 */
	len = 0;
	while (m) {
		len += m->m_len;
		m = m->m_next;
	}
	if (len < *lenp) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdrmbuf_putbytes(xdrs, addr, len)
	register XDR	*xdrs;
	caddr_t		 addr;
	register u_int		 len;
{

	while ((xdrs->x_handy -= len) < 0) {
		/*
		 * The bytes do NOT fit in the current mbuf, 
		 * copy the part that does fit, then allocate a new one.
		 */
		if ((xdrs->x_handy += len) > 0) {
			bcopy(addr, xdrs->x_private, (u_int)xdrs->x_handy);
			addr += xdrs->x_handy;
			len -= xdrs->x_handy;
		}
		if (xdrs->x_base) {
			register struct mbuf *m;

			m = m_vget(M_WAIT, BYTES_TO_GET, MT_DATA);
			((struct mbuf *)xdrs->x_base)->m_next = m;
			xdrs->x_base = (caddr_t)m;
			xdrs->x_private = mtod(m, caddr_t);
			xdrs->x_handy = m->m_len;
		} else {
			return (FALSE);
		}
	}
	bcopy(addr, xdrs->x_private, len);
	xdrs->x_private += len;
	return (TRUE);
}

/*
 * Called from m_copy to add a reference to an mbuf created with
 * xdrmbuf_putbuf below. We can't use pfd_useinc() because it
 * ASSERTs that P_HASH is set. Set m_farg to the pfdat for the
 * page. If we use page_mapin, set m_darg to the address rbrele()
 * needs to hand to page_mapout; this is done so that we never
 * rely on a virt mapping that we didn't create.
 */
void
rbhold(struct mbuf *m)
{
	caddr_t addr = mtod(m, caddr_t);
	pfd_t   *pfd = kvatopfdat(addr);
	int s;

	/* bump pf_use */

	pfd = kvatopfdat(addr);
	s = pfdat_lock(pfd);
	ASSERT((pfd)->pf_use >= 1);
	ASSERT(((pfd)->pf_flags &
		(P_QUEUE|P_SQUEUE|P_RECYCLE)) == 0);
	pfdat_inc_usecnt(pfd);
	pfdat_unlock(pfd, s);

	/* if the addr is kvirt, use page_mapin to get a local mapping */

	if (IS_KSEG2(addr)) {
		caddr_t newaddr;
		
		newaddr = page_mapin(pfd, VM_DMA_WR, 0);
		m->m_darg = (long)newaddr;
		newaddr += poff(addr);
		m->m_off += newaddr - addr;
	} else
		m->m_darg = 0;
	m->m_farg = (long)pfd;
}

/*
 * Called from m_free to drop a reference to an mbuf created with
 * xdrmbuf_putbuf below. If m_darg, we free the virtual mapping.
 */
void
rbrele(struct mbuf *m)
{
	if (m->m_darg)
		page_mapout((caddr_t)m->m_darg);
	pagefree((pfd_t *)m->m_farg);
}

/*
 * Like putbytes, only we avoid the copy by pointing cluster mbufs
 * at the buffer. Only supports addr's in kernel address space.
 * The resulting mbufs are not dependent on the original virtual
 * memory mapping [addr, addr+len), as rbhold uses page_mapin()
 * to get a local mapping. No user addresses allowed. We bump the
 * pf_use on each pfdat in rbhold, and pagefree each pfdat in rbrele.
 *
 * If putbuf returns false, the caller should fallback to using
 * xdrmbuf_putbytes, which will do a more straightforward bcopy.
 *
 * This code requires that addr point to a region of memory for
 * which kvatopfdat() will return a valid pfdat. This is always true
 * today, and it must remain so. It tries to check against IS_KSEG0/1/2,
 * but that isn't a guarantee. CKPHYSPNUM would be better yet, but is
 * still not a guarantee.
 */

int ethan_do_new_putbuf = 1;

bool_t
xdrmbuf_putbuf(xdrs, addr, len)
	register XDR	*xdrs;
	caddr_t		 addr;
	u_int		 len;
{
	struct mbuf *m = NULL;
	int curpoff;
	register struct mbuf **next_m = &m, *last_m;
	register u_int overflow_len, resid;
	register struct mbuf *overflow = NULL;
	register int curlen;

	if (ethan_do_new_putbuf == 0)
		return (FALSE);
	if (len == 0)
		return (TRUE);
	if (xdrs->x_handy >= len && len < 1024)
		return (FALSE);	/* just put it in the mbuf with bcopy */
	if (IS_KSEG2(addr)) {
		cache_operation(addr, len,
			  CACH_DCACHE|CACH_WBACK|CACH_IO_COHERENCY);
	} else if (!IS_KSEG0(addr) && !IS_KSEG1(addr)) {
		ASSERT(0);
		return (FALSE);
	}
	/*
	 * Now get the cluster mbufs.
	 * Use cluster_len for the data size.
	 */
	resid = len;
	curpoff = poff(addr);
	do {
		curlen = MIN(resid, NBPP - curpoff); 
		*next_m = mclgetx(rbhold, (long)0, rbrele, (long)0,
				  addr, curlen, M_WAIT);
		if (*next_m == NULL) {
			if (m)
				m_freem(m);
			return(FALSE);
		}
		rbhold(*next_m);
		
		curpoff      = 0;
		resid	    -= curlen;
		addr        += curlen;
		if (resid == 0)
			last_m = *next_m;
		next_m       = &(*next_m)->m_next;
	} while (resid);

	/*
	 * If len is not a multiple of the basic XDR size (4 bytes)
	 * then we create an mbuf to hold the round-up overflow and
	 * tack it on to the end of the new mbuf chain.
	 */
	if (overflow_len = RNDUP(len) - len) {
		MGET(overflow, M_WAIT, MT_DATA);
		if (!overflow) {
			m_freem(m);
			return (FALSE);
		}
		overflow->m_len = overflow_len;
		overflow->m_next = NULL;
		bzero(mtod(overflow, caddr_t), overflow_len);
		last_m = *next_m = overflow;
	}
	/*
	 * Encode the number of valid bytes.  This may be less than the
	 * actual number transferred as the length was rounded up above
	 * The contents of the bytes beyond this encoded length are
	 * undefined (i.e., junk).
	 */
	if (! xdrmbuf_putint(xdrs, (int *)&len)) {
		m_freem(m);
		if (overflow)
			m_free(overflow);
		return(FALSE);
	}
	((struct mbuf *)xdrs->x_base)->m_len -= xdrs->x_handy;
	((struct mbuf *)xdrs->x_base)->m_next = m;
	/*
	 *  getpos must still work (returns length in last mbuf)
	 */
	xdrs->x_base = (caddr_t)last_m;
	xdrs->x_private = mtod(last_m, caddr_t) + last_m->m_len;

	if (overflow) {
		xdrs->x_handy = MLEN - overflow->m_len;
		overflow->m_len = MLEN;
	} else
		xdrs->x_handy = 0;

	return (TRUE);
}

u_int
xdrmbuf_getpos(xdrs)
	register XDR	*xdrs;
{
	return ((__psunsigned_t)xdrs->x_private -
		mtod(((struct mbuf *)xdrs->x_base), __psunsigned_t));
}

bool_t
xdrmbuf_setpos(xdrs, pos)
	register XDR	*xdrs;
	u_int		 pos;
{
	register caddr_t	newaddr =
	    mtod(((struct mbuf *)xdrs->x_base), caddr_t) + pos;
	register caddr_t	lastaddr =
	    xdrs->x_private + xdrs->x_handy;

	if ((__psint_t)newaddr > (__psint_t)lastaddr)
		return (FALSE);
	xdrs->x_private = newaddr;
	xdrs->x_handy = (__psint_t)lastaddr - (__psint_t)newaddr;
	return (TRUE);
}

long *
xdrmbuf_inline(xdrs, len)
	register XDR	*xdrs;
	int		 len;
{
	long *buf = 0;

	if ((xdrs->x_handy >= len) && !XDR_MISALIGNED(xdrs->x_private)) {
		xdrs->x_handy -= len;
		buf = (long *) xdrs->x_private;
		xdrs->x_private += len;
	}
	return (buf);
}
