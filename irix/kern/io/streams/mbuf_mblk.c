/*
 * Copyright (c) 1997 Silicon Graphics, Inc.
 *
 * mbuf/mblk conversion routines
 */
#ident "$Revision: 1.21 $"

#include "sys/types.h"
#include "sys/param.h"
#include "sys/stream.h"
#include "sys/strsubr.h"
#include "sys/stropts.h"
#include "sys/strstat.h"
#include "sys/strmp.h"
#include "sys/systm.h"
#include "sys/atomic_ops.h"

#include "bsd/sys/mbuf.h"
#include "sys/errno.h"
#include "sys/debug.h"

#define CONST /* esballoc has bad prototype, needs const */
/*
 * forward function declarations
 */
struct mbuf *mblk_to_mbuf(mblk_t *, int, int, int);

mblk_t *mbuf_to_mblk(struct mbuf *, int);
void mbuf_to_mblk_free(struct mbuf *m);

/*
 * External function declarations
 */
extern void freeb(mblk_t *);

#ifdef DEBUG
/* some statistics stuff */
struct str_mblkstats str_mblkstats;

#define MBLK_STATS(field, c) atomicAddUint(&(str_mblkstats.field), c);
#else
#define MBLK_STATS(field, c)
#endif /* DEBUG */

#ifdef DEBUG
/*ARGSUSED*/
int
mblk_to_mbuf_check1(struct mbuf *m, mblk_t *bp)
{
	return 1;
}

/*ARGSUSED*/
int
mblk_to_mbuf_check2(struct mbuf *m, mblk_t *bp)
{
	return 2;
}

/*ARGSUSED*/
int
mblk_to_mbuf_check3(struct mbuf *m, mblk_t *bp)
{
	return 3;
}

/*ARGSUSED*/
int
mblk_to_mbuf_check4(struct mbuf *m, mblk_t *bp)
{
	return 4;
}

/*ARGSUSED*/
int
mblk_to_mbuf_check5(struct mbuf *m, mblk_t *bp)
{
	return 5;
}

/*ARGSUSED*/
int
mblk_to_mbuf_check6(struct mbuf *m, mblk_t *bp)
{
	return 6;
}

/*ARGSUSED*/
int
mblk_to_mbuf_check7(struct mbuf *m, mblk_t *bp)
{
	return 7;
}

/*ARGSUSED*/
int
mblk_to_mbuf_check8(struct mbuf *m, mblk_t *bp)
{
	return 8;
}

/* --------------------------------------------- */
/*ARGSUSED*/
int
mbuf_to_mblk_check1(struct mbuf *m, mblk_t *bp)
{
	return 1;
}

/*ARGSUSED*/
int
mbuf_to_mblk_check2(struct mbuf *m, mblk_t *bp)
{
	return 2;
}

/*ARGSUSED*/
int
mbuf_to_mblk_check3(struct mbuf *m, mblk_t *bp)
{
	return 3;
}

/*ARGSUSED*/
int
mbuf_to_mblk_check4(struct mbuf *m, mblk_t *bp)
{
	return 4;
}
#endif /* DEBUG */

/*
 * Free the mblk chain supplying buffers and linked off the mbuf entry.
 */
/*ARGSUSED*/
void
mblk_to_mbuf_free(struct mbuf *m)
{
	mblk_t *bp = (mblk_t *)m->m_farg;

#ifdef DEBUG
	if (m->m_type == MT_FREE) {
		(void)mblk_to_mbuf_check1(m, bp);
		MBLK_STATS(mblk2mbuf_free_bogus, 1);
		return;
	}
	if (atomicAddInt(&(bp->b_mbuf_refct), -1) <= 0) { /* last reference */
		MBLK_STATS(mblk2mbuf_free_freeb, 1);
		bp->b_mopsflag |= STR_MOPS_MBLK_TO_MBUF_FREE;

		if (bp->b_datap->db_ref > 1) { /* NOT last reference so look*/
			(void)mblk_to_mbuf_check2(m, bp);
		}
		freeb(bp);
	} else {
		MBLK_STATS(mblk2mbuf_free_RefGTone, 1);

		if (bp->b_datap->db_ref > 1) { /* last reference so look */
			(void)mblk_to_mbuf_check3(m, bp);
		} else {
			(void)mblk_to_mbuf_check4(m, bp);
		}
	}
#else
	if (atomicAddInt(&(bp->b_mbuf_refct), -1) <= 0) { /* last reference */

		bp->b_mopsflag |= STR_MOPS_MBLK_TO_MBUF_FREE;
		freeb(bp);
	}
#endif /* DEBUG */
	return;
}

/*
 * Generic external mbuf duplication procedure, called when the mbuf is
 * duplicated and it's associated reference count is incremented.
 */
/*ARGSUSED*/
void
mblk_to_mbuf_dup(struct mbuf *m)
{
	mblk_t *bp;

	MBLK_STATS(mblk2mbuf_dupfun, 1);
	bp = (mblk_t *)m->m_farg;
#ifdef DEBUG
	if ((bp->b_mopsflag & STR_MOPS_MBLK_TO_MBUF) &&
	    (bp->b_mopsflag & STR_MOPS_MBLK_TO_MBUF_DUP)) {
		(void)mblk_to_mbuf_check5(m, bp);
	}
	if (bp->b_mopsflag & STR_MOPS_MBLK_TO_MBUF_DUP) { /* second dup */

		SAVE_MBUFDUP_ADDRESS(bp, m);

		(void)mblk_to_mbuf_check6(m, bp);
	} else { /* first dup */

		bp->b_mopsflag |= STR_MOPS_MBLK_TO_MBUF_DUP;
		(void)mblk_to_mbuf_check7(m, bp);
	}
#else
	bp->b_mopsflag |= STR_MOPS_MBLK_TO_MBUF_DUP;
#endif /* DEBUG */
	atomicAddInt(&(bp->b_mbuf_refct), 1);

	SAVE_DUPB_ADDRESS(bp, __return_address);
	return;
}

/*
 * Convert mblk chain to mbuf chain
 *
 * All mbufs in the chain are marked with mbuf_type.
 * NOTES:
 * 1. The reference count on each dblk in the mblk chain is NOT
 * incremented, since we assume that it's already >= 1.
 * 2. We assume freeb() will be called for each mblk in the mbuf chain when
 * it is freed. So any dup'ing of the mblk chain REQUIRES that the caller
 * call freeb() on that mblk to decrement and possibly free the storage.
 * 3. Failure to create entire mbuf chain results in NO mblk chain changes.
 *
 * Check and fixup the case when this mblk points to a mbuf which was
 * attached via an mblk_to_mbuf() conversion operation.
 * This handles the case where:
 * 1. a msgb was created first, say by the stream-head code
 * 2. a mblk_to_mbuf() operation was done returning an mbuf chain.
 * 3. a mbuf_to_mblk() operation was done subsequently, say for the
 *    loopback TLI cases, which returns the "original" mblk chain.
 * 4. The freeb operation was done on the "original" mblk BUT we must
 *    handle releasing the mbuf header's storage.
 *
 * The converse case is where a mbuf is created first, say from data
 * received via a network device driver, and then converted via
 * mbuf_to_mblk() to mblk/dblk chain for a Streams application read.
 * In this case there's NO requirement for a double transformation
 * of the data back from an mblk chain into an mbuf chain, so we
 * don't implement "adjusting" the mblk free_fun.
 */
/*ARGSUSED*/
struct mbuf *
mblk_to_mbuf(mblk_t *mp, int alloc_flags, int mbuf_type, int mbuf_flags)
{
	struct mbuf *m, *n;
	mblk_t *bp;
	int len;

	ASSERT(mbuf_type == MT_DATA);
	MBLK_STATS(mblk2mbuf_calls, 1);

	if (mp == NULL) { /* no-op case */
		MBLK_STATS(mblk2mbuf_null, 1);
		return NULL;
	}
	m = NULL;
	for (bp = mp; bp; bp = bp->b_cont) {
		/*
		 * Create new mbuf to hang off this msg.
		 * and set m_off to correspond current stream read pointer.
		 */
		len = bp->b_wptr - bp->b_rptr;
		n = mclgetx(mblk_to_mbuf_dup, (long)bp,
			    mblk_to_mbuf_free, (long)bp,
			    (caddr_t)bp->b_rptr, len, alloc_flags);

		if (n == NULL) {
			MBLK_STATS(mblk2mbuf_mclx_fail, 1);
			if (m) {
				m_freem(m);
			}
			return NULL;
		}
		SAVE_MBUF_ADDRESS(bp, n);
		MBLK_STATS(mblk2mbuf_mclx_ok, 1);
		bp->b_mopsflag |= STR_MOPS_MBLK_TO_MBUF;

		/* update number of mbufs pointing to this mblk/dblk/buffer */
		atomicAddInt(&(bp->b_mbuf_refct), 1);
		if (m) {
			m_cat(m, n);
		} else {
			m = n;
		}
	}
	m->m_flags |= (mbuf_flags & M_COPYFLAGS);
	return m;
}

/*
 * Convert mbuf chain to a corresponding mblk chain
 * NOTE: Failure to create the mblk chain results in an unchanged mbuf chain
 */
mblk_t *
mbuf_to_mblk(struct mbuf *m0, int pri)
{
 	mblk_t *mp, *bp, **nbp;
	struct mbuf *m;
	struct free_rtn *frp;
	CONST struct free_rtn free_dummy = {NULL, NULL};

	MBLK_STATS(mbuf2mblk_calls, 1);
	/*
	 * First pass we allocate external storage mblks. Free_dummy
	 * procedure is a placeholder to allow us to back out with
	 * freemsg()
	 */
	mp = NULL;
	nbp = &mp;

	for (m = m0; m; m = m->m_next) {
		/* skip empty mbufs */
		if (m->m_len == 0)
			continue;
		/*
		 * Create a new mblk and attach it to this message
		 */
		if ((bp = esballoc(mtod(m, u_char *), m->m_len, pri,
			&free_dummy)) == NULL) {
			/*
			 * opps, problem allocating a mblk, toss everything
			 * and report a failure.
			 */
			if (mp) freemsg(mp);

			MBLK_STATS(mbuf2mblk_esballoc_fail, 1);
			return NULL;
		}
		MBLK_STATS(mbuf2mblk_esballoc_ok, 1);
		*nbp = bp;
		nbp = &bp->b_cont;
		bp->b_wptr = bp->b_datap->db_lim;

	}

	/*
	 * zero-length message, return single mblk/dblk pair with no
	 * buffer
	 */
	if (mp == NULL) {
		if (mp = allocb(-1, pri)) { /* ok */
			MBLK_STATS(mbuf2mblk_allocb_ok, 1);
			m_freem(m0);
		} else {
			MBLK_STATS(mbuf2mblk_allocb_fail, 1);
		}
		return mp;
	}

	/*
	 * walk down the mbuf list and put in the real free information
	 */
	bp = mp;
	m = m0;
	while ( m ) {

		if (m->m_len == 0) {
			/*
			 * free the zero length mbuf and advance
			 * to the next one.
			 */
			MFREE(m, m);
			continue;
		}

		frp = &bp->b_datap->db_frtn;
		frp->free_func = (void (*)())mbuf_to_mblk_free;
		frp->free_arg = (char *)m; /* mbuf pointer */

		bp->b_datap->db_frtnp = frp;
		bp->b_mopsflag |= STR_MOPS_MBUF_TO_MBLK;

		bp = bp->b_cont;

		m = m->m_next;
	}

	return (mp);
}

/*
 * Free the mbuf chain supplying buffers and linked off the mblk entry.
 */
void
mbuf_to_mblk_free(struct mbuf *m)
{
#ifdef DEBUG
	MBLK_STATS(mbuf2mblk_free, 1);
	(void)mbuf_to_mblk_check4(m, NULL);
#endif /* DEBUG */

	(void)m_free(m);
	return;
}

