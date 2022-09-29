/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Revision: 1.42 $"

#ifdef	XFSDEBUG
#define	BDEBUG
#endif

#define _KERNEL 1
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/buf.h>
#include <sys/sysmacros.h>
#include <sys/time.h>
#include <sys/uuid.h>
#include <sys/grio.h>
#include <sys/debug.h>
#include <sys/atomic_ops.h>
#undef _KERNEL
#include <sys/immu.h>
#include <sys/pfdat.h>
#include <sys/vnode.h>
#include <sys/var.h>
#include <sys/sysinfo.h>
#include <sys/ksa.h>
#include <sys/kmem.h>
#ifdef BDEBUG
#include <sys/cmn_err.h>
#endif
#ifdef DEBUG_BUFTRACE
#include <sys/ktrace.h>
#endif
#include <errno.h>
#include <stdio.h>
#include <malloc.h>
#include <bstring.h>
#include "sim.h"

STATIC int	bp_match(register struct buf *, int, void *);

#ifdef BDEBUG
int Ddev = 0xdeadbabe;		/* allow buffer tracing on a dev */
int Ndev = 0xdeadbabe;		/* disable buffers for dev Ndev */
int bdebug = 0;
#endif

/*
 * The following several routines allocate and free
 * buffers with various side effects.  In general the
 * arguments to an allocate routine are a device and
 * a block number, and the value is a pointer to
 * the buffer header; the buffer is marked "busy"
 * so that no one else can touch it.  If the block was
 * already in core, no I/O need be done; if it is
 * already busy, the process waits until it becomes free.
 * The following routines allocate a buffer:
 *	getblk
 *	bread
 *	breada
 * Eventually the buffer must be released, possibly with the
 * side effect of writing it out, by using one of
 *	bwrite
 *	bdwrite
 *	bawrite
 *	brelse
 *
 * It is OK to read buffer fields for buffers that are on the freelist
 * without having the bp locked (though the freelist must be locked)
 * The bp MUST be locked before modifying any fields however (and either
 * be OFF the freelist, OR still have it locked)
 *
 * B_DELWRI & B_STALE are mutually exclusive, in order to allow checking
 * b_flags without always locking the buffer.  B_DELWRI should be cleared
 * BEFORE B_STALE is set.
 */

/*
 * locks & semas for bufs and pbufs
 */
int		bfslpcnt;	/* # procs waiting for free buffers */
sema_t		bfreeslp;	/* sleep semaphore for free list */
lock_t		bfreelock;	/* lock on buffer free list */
int		bufmem;
static struct zone *buf_zone;	/* buffer heap zone */

lock_t		bpinlock;
bwait_pin_t	bwait_pin[NUM_BWAIT_PIN];

/*
 * Unlink a buffer from the available list and mark it busy.
 * notavailspl assumes that the free list is locked.
 */
#define	notavailspl(bp) { \
	register struct buf *forw = bp->av_forw; \
	register struct buf *back = bp->av_back; \
	back->av_forw = forw; \
	forw->av_back = back; \
	bp->b_flags |= B_BUSY; \
	bfreelist->b_bcount--; \
}

static void bio_realloc(register struct buf *bp);
static void bio_freemem(register struct buf *bp);
static int bp_match(register struct buf *bp, int field, void *value);
void dchunkflush(time_t);
#ifdef BDEBUG
static void overlapsearch(dev_t, daddr_t, int);
#endif

/*
 * This is the static array of buf structures used by
 * the buffer cache.
 */
buf_t	*global_buf_table;
hbuf_t	*global_buf_hash;

buf_t	*bfreelist;

int     _bhash_shift;   /* shift count for buffer hashing, see v_hbuf */

/*
 * dev + daddr buffer hash calculation -- see bhash macro.
 */
int
_bhash(dev_t d, daddr_t b)
{
        int bit, hval, shift, mask;

        /*
         * dev_t is 32 bits, daddr_t is always 64 bits N32/N64
         */
        b ^= (unsigned)d;
        shift = _bhash_shift;
        mask = (1 << shift) - 1;        /* this is v.v_hmask */
        for (bit = hval = 0; b != 0 && bit < sizeof(b) * NBBY; bit += shift) {
                hval ^= (int)b & mask;
                b >>= shift;
        }
        return hval;
}

/*
 * Initialize the buffer I/O system by freeing all buffers
 * and setting all device hash buffer lists to empty.
 * Setup the list of swap buffers also.
 */
void
binit(void)
{
	register struct buf *bp, *dp;
	register struct hbuf *hbp;
	register int i;
	bwait_pin_t *bwp;

	if (buf_zone)
		return;

	bfreelist = (buf_t *)kmem_zalloc(sizeof(buf_t), KM_SLEEP);
	dp = bfreelist;
	dp->b_forw = dp; dp->b_back = dp;
	dp->av_forw = dp; dp->av_back = dp;
	bfslpcnt = 0;
	spinlock_init(&bpinlock, "bpinlck");
	spinlock_init(&bfreelock, "bfreelk");
	initnsema(&bfreeslp, 0, "bfreeslp");
	_bhash_shift = HSHIFT;
	global_buf_table = (buf_t *)kmem_zalloc(sizeof(buf_t) * NBUF, 0);
	for (i = 0, bp = global_buf_table; i < v.v_buf; i++, bp++) {
		bp->b_edev = NODEV;
		bp->b_forw = bp->b_back = bp;
		bp->b_dforw = bp->b_dback = bp;
		bp->b_vp = NULL;
		bp->b_flags = B_BUSY;
		bp->b_bcount = 0;
		bp->b_relse = NULL;
		bp->b_iodone = NULL;
		init_sema(&bp->b_lock, 1, "buf", i);
		psema(&bp->b_lock, PRIBIO);	/* so brelse works */
		init_sema(&bp->b_iodonesema, 0, "bio", i);
		bp->b_pincount = 0;
		bp->b_fsprivate = NULL;
		bp->b_fsprivate2 = NULL;
		bp->b_fsprivate3 = NULL;
#ifdef DEBUG_BUFTRACE
		bp->b_trace = ktrace_alloc(BUF_TRACE_SIZE, KM_SLEEP);
#endif
		brelse(bp);
	}

	/* initialize hash buffer headers */
	global_buf_hash = (hbuf_t*)kmem_zalloc(sizeof(hbuf_t) * HBUF, 0);
	for (i = 0, hbp = global_buf_hash; i < v.v_hbuf; i++, hbp++) {
		init_sema(&hbp->b_lock, 1, "hbf", i);
		hbp->b_forw = hbp->b_back = (struct buf *)hbp;
	}

	/* initialize bwait_pin structures. */
	for (i = 0, bwp = bwait_pin; i < NUM_BWAIT_PIN; i++, bwp++) {
		init_sv(&bwp->bwp_wait, SV_DEFAULT, "bwp", i);
	}

	buf_zone = kmem_zone_init(sizeof(buf_t), "rbufs");

#ifdef DEBUG_BUFTRACE
	/*
	 * Initialize the chunk trace buffer to 256 entries.
	 */
	buf_trace_buf = ktrace_alloc(256, KM_SLEEP);
#endif
}



void
notavail(struct buf *bp)
{
	register int	s = bfree_lock(0);

	notavailspl(bp);	/* sets B_BUSY */
	bfree_unlock(0,s);
}

/*
 * This is just a wrapper for read_buf().  It calls read_buf()
 * with a lock_flags parameter value indicating that we want
 * to sleep on the buffer lock if it is already in-core.
 */
struct buf *
bread(
	register dev_t dev,
	register daddr_t blkno,
	register int	len)
{
	return read_buf(dev, blkno, len, 0);
}

/*
 * Read in (if necessary) the block and return a buffer pointer.
 * The lock_flags parameter allows the caller to specify (with BUF_TRYLOCK)
 * that we should not sleep on the buffer lock if the requested buffer is
 * already in-core and is locked.  It also allows the caller to specify
 * (with BUF_BUSY) that we should not push out delayed write buffers
 * in this call.
 */
struct buf *
read_buf(
	register dev_t		dev,
	register daddr_t	blkno,
	register int		len,
	register uint		flags)
{
	register struct buf *bp;
	struct bdevsw *my_bdevsw;

#ifdef BDEBUG
	if (dev == Ddev)
		cmn_err(CE_CONT, "!bread:blk:%d len:%d\n", blkno, len);
#endif
	ASSERT(len > 0);
	bp = get_buf(dev, blkno, len, flags);
	if (bp == NULL) {
		return NULL;
	}
	ASSERT(bp->b_blkno == blkno && bp->b_bcount == BBTOB(len));
	if (bp->b_flags & B_DONE)
		return (bp);

	bp->b_flags |= B_READ;
	my_bdevsw = get_bdevsw(dev);
	bdstrat(my_bdevsw, bp);
	biowait(bp);
	return (bp);
}

/*
 * See if the block is associated with some buffer.
 * Return a pointer to the buffer (header) if it exists.
 * Lock the buffer first if requested to do so.  If the caller
 * requested a trylock, then do not sleep trying to find or lock
 * the buffer.
 */
struct buf *
incore(
	register dev_t		dev,
	register daddr_t	blkno,
	int len,
	int lockit)
{
	register struct buf *bp;
	register struct buf *dp;
	register int bytelen = BBTOB(len);

 	ASSERT(len > 0);

	dp = bhash(dev, blkno);
startover:
	if (lockit == INCORE_TRYLOCK) {
		if (!cpsema(&dp->b_lock))
			return NULL;
	} else
		psema(&dp->b_lock, PRIBIO);

	for (bp = dp->b_forw; bp != dp; bp = bp->b_forw) {
		if ((bp->b_edev == dev) && !(bp->b_flags & B_STALE) &&
		    (bp->b_blkno == blkno) && (bp->b_bcount == bytelen)) {
			vsema(&dp->b_lock);
			/*
			 * If caller wants buffer locked, do so.
			 * Must lock buffer semaphore, take off
			 * free list, and mark B_BUSY.
			 */
			if (lockit == INCORE_LOCK) {
				psema(&bp->b_lock, PRIBIO);
				if ((bp->b_flags & B_STALE) ||
				    (bp->b_blkno != blkno) ||
				    (bp->b_bcount != bytelen) ||
				    (bp->b_edev != dev)) {
					vsema(&bp->b_lock);
					goto startover;
				}
				notavail(bp);	/* sets B_BUSY */
			} else if (lockit == INCORE_TRYLOCK) {
				if (!cpsema(&bp->b_lock))
					return NULL;
				if ((bp->b_flags & B_STALE) ||
				    (bp->b_blkno != blkno) ||
				    (bp->b_bcount != bytelen) ||
				    (bp->b_edev != dev)) {
					vsema(&bp->b_lock);
					goto startover;
				}
				notavail(bp);	/* sets B_BUSY */
			}
			return (bp);
		}
	}
	vsema(&dp->b_lock);

#ifdef BDEBUG
	if (bdebug)
		overlapsearch(dev, blkno, len);
#endif

	return (0);
}



/*
 * This is called to find the specified buffer in the cache
 * with the given value in the specified field.  The values
 * allowed are specified in bp_match() below.  The buffer is
 * never locked by this call.  It assumes that if there is a
 * match the caller has prevented any races about the value
 * changing (usually by already having the buffer locked).
 * If the value does not match there are no races at all.
 *
 * Note that this routine does not recognize the B_STALE flag
 * in the buffer.  It is up to the caller to look for that if
 * it is interested.
 */
buf_t *
incore_match(
	dev_t			dev,
	daddr_t			blkno,
	int 			len,
	int			field,
	void			*value)
{
	buf_t *bp;
	buf_t *dp;
	int bytelen = BBTOB(len);

 	ASSERT(len > 0);

	dp = bhash(dev, blkno);
	psema(&dp->b_lock, PRIBIO);

	for (bp = dp->b_forw; bp != dp; bp = bp->b_forw) {
		if ((bp->b_edev == dev) && (bp->b_blkno == blkno) &&
		    (bp->b_bcount == bytelen)) {
			vsema(&dp->b_lock);
			if (bp_match(bp, field, value)) {
				return (bp);
			} else {
				return (NULL);
			}
		}
	}
	vsema(&dp->b_lock);

#ifdef BDEBUG
	if (bdebug)
		overlapsearch(dev, blkno, len);
#endif

	return (0);
}

/*
 * Compare value to the field indicated in the given buffer.
 * If it matches return 1, else return 0.
 */
STATIC int
bp_match(register struct buf *bp, int field, void *value)
{
	switch (field) {
	case BUF_FSPRIV:
		return (bp->b_fsprivate == value);
	case BUF_FSPRIV2:
		return (bp->b_fsprivate2 == value);
	default:
		ASSERT(0);
		return (0);
	}
}

/*
 * Write the buffer, waiting for completion.
 * Then release the buffer.
 */
int
bwrite(register struct buf *bp)
{
	register uint64_t flag;
	int error;
	struct bdevsw *my_bdevsw;

	ASSERT((bp->b_flags & B_BUSY) && (valusema(&bp->b_lock) <= 0));
	ASSERT(bp->b_bufsize > 0 && bp->b_bcount <= bp->b_bufsize);

#ifdef BDEBUG
	if (bp->b_edev == Ddev)
		cmn_err(CE_CONT, "!bwrite:blk:%d len:%d\n", bp->b_blkno, bp->b_bufsize);
#endif
	/*
	 * Bwrite can be called either directly by some FS routine, or by
	 * the buffer cache when writing out a delayed buffer.  We don't
	 * wish to count the logical write twice (after all the data was
	 * "logically" written only once), so don't add more if this
	 * is a delayed buffer we are pushing out.
	 */
	flag = bp->b_flags;
	bp->b_flags &= ~(B_READ | B_DONE | B_ERROR | B_DELWRI);

	/*
	 * Wait for the buffer to be unpinned if it is currently pinned.
	 */
	if (bp->b_pincount > 0) {
		bwait_unpin(bp);
	}

	if (bp->b_vp) {
		VOP_STRATEGY(bp->b_vp, bp);
	} else {
		my_bdevsw = get_bdevsw(bp->b_edev);
		bdstrat(my_bdevsw, bp);
	}

	if (flag & B_ASYNC) {
		/*
		 * This test can return up-front errors that are caught
		 * by the strategy routine before it queues the request
		 * for completion.
		 */
		if (!(flag & B_DELWRI)) {
			error = geterror(bp);
		} else {
			error = 0;
		}
	} else {
		error = biowait(bp);
		if (!(bp->b_flags & B_HOLD)) {
			brelse(bp);
		}
	}
	return error;
}

/*
 * Release the buffer, marking it so that if it is grabbed
 * for another purpose it will be written out before being
 * given up (e.g. when writing a partial block where it is
 * assumed that another write for the same block will soon follow).
 * This can't be done for magtape, since writes must be done
 * in the same order as requested.
 */
void
bdwrite(register struct buf *bp)
{
	ASSERT((bp->b_flags & B_BUSY) && (valusema(&bp->b_lock) <= 0));
	ASSERT(bp->b_bufsize > 0 && bp->b_bcount <= bp->b_bufsize);

#ifdef BDEBUG
	if (bp->b_edev == Ddev)
		cmn_err(CE_CONT,
			"!bdwrite:blk:%d len:%d\n", bp->b_blkno, bp->b_bufsize);
	if (bp->b_edev == Ndev) {
		bwrite(bp);
		return;
	}
#endif
	if (!(bp->b_flags & B_DELWRI))
		bp->b_start = lbolt;
	bp->b_flags |= B_DELWRI | B_DONE;
	brelse(bp);
}

/*
 * Release the buffer, start I/O on it, but don't wait for completion.
 */
void
bawrite(register struct buf *bp)
{
#ifdef BDEBUG
	if (bp->b_edev == Ndev) {
		bwrite(bp);
		return;
	}
#endif
	if (bfreelist->b_bcount > 4)
		bp->b_flags |= B_ASYNC;
	bwrite(bp);
}

/*
 * Increment the pin count of the given buffer.
 * This value is protected by the bpinlock spinlock.
 */
void
bpin(register struct buf *bp)
{
	int	s;

	ASSERT((bp->b_flags & B_BUSY) && (valusema(&bp->b_lock) <= 0));

	s = mutex_spinlock(&bpinlock);
	bp->b_pincount++;
	mutex_spinunlock(&bpinlock, s);
}

/*
 * Decrement the pin count of the given buffer, and wake up anyone
 * in bwait_unpin() if the pin count goes to 0.  The buffer must
 * have been previously pinned with a call to bpin().  This routine
 * is not forgiving about being unnecessarily called.
 */
void
bunpin(register struct buf *bp)
{
	int		s;
	int		count;
	bwait_pin_t	*bwp;

	ASSERT(bp->b_pincount > 0);

	count = 0;
	s = mutex_spinlock(&bpinlock);
	bp->b_pincount--;
	if ((bp->b_pincount == 0) && (bp->b_pin_waiter > 0)){
		bwp = BPTOBWP(bp);
		ASSERT(bwp->bwp_count >= 0);
		count = bwp->bwp_count;
		if (count != 0) {
			bwp->bwp_count = 0;
		}
	}
	mutex_spinunlock(&bpinlock, s);
	while (count > 0) {
		sv_broadcast(&bwp->bwp_wait);
		count = 0;
	}
}

/*
 * This is called to wait for the given buffer to be unpinned.
 * It will sleep until this happens.  The caller must have the
 * buffer locked so that the buffer cannot be subsequently pinned
 * once someone is waiting for it to be unpinned.
 *
 * The bpinlock is used to guard the pincount values in all buffers.
 * The bwait_pin is used to sleep until the buffer is unpinned.
 */
void
bwait_unpin(register struct buf *bp) 
{
	int		s;
	bwait_pin_t	*bwp;

	ASSERT((bp->b_flags & B_BUSY) && (valusema(&bp->b_lock) <= 0));

	if (bp->b_pincount == 0) {
		return;
	}

	s = mutex_spinlock(&bpinlock);
	if (bp->b_pincount == 0) {
		mutex_spinunlock(&bpinlock, s);
		return;
	}
	ASSERT(bp->b_pin_waiter == 0);
	bp->b_pin_waiter = 1;
	while (bp->b_pincount > 0) {
		bwp = BPTOBWP(bp);
		bwp->bwp_count++;
		sv_wait(&(bwp->bwp_wait), PRIBIO, &bpinlock, s);
		s = mutex_spinlock(&bpinlock);
	}
	mutex_spinunlock(&bpinlock, s);
	bp->b_pin_waiter = 0;
}

/*
 * Release the buffer, with no I/O implied.
 * If there is a 'special action' function specified in the b_relse field
 * of the buffer, this takes precedence.
 */
void
brelse(register struct buf *bp)
{
	register struct buf *temp;
	register uint64_t flags = bp->b_flags;
	int s;

#ifdef BDEBUG
	if (bp->b_edev == Ddev)
	   cmn_err(CE_CONT, "!rel:blk:%d len:%d\n", bp->b_blkno, bp->b_bufsize);
#endif
	if (bp->b_relse && !(flags & B_RELSE)) {
		(*bp->b_relse)(bp);
		return;
	}

	ASSERT((bp->b_flags & B_BUSY) && (valusema(&bp->b_lock) <= 0));

	if (flags & B_ERROR) {
		flags &= ~(B_ERROR|B_DELWRI);
		flags |= B_STALE|B_AGE;
		bp->b_error = 0;
	}

	ASSERT((flags & (B_STALE|B_DELWRI)) != (B_STALE|B_DELWRI));

	/*
	 * Place the buffer on the appropriate place in the free list.
	 */
	s = bfree_lock(0);
	if (flags & (B_AGE|B_STALE)) {
		temp = bfreelist->av_forw;
		temp->av_back = bp;
		bp->av_forw = temp;
		bfreelist->av_forw = bp;
		bp->av_back = bfreelist;
	} else {
		temp = bfreelist->av_back;
		temp->av_forw = bp;
		bp->av_back = temp;
		bfreelist->av_back = bp;
		bp->av_forw = bfreelist;
	}
	bfreelist->b_bcount++;		/* just for fun */

	/*
	 * If someone waiting for a free buffer - get them going.
	 */
	ASSERT(bfslpcnt >= 0);
	if (bfslpcnt) {
		bfslpcnt--;
		bfree_unlock(0,s);
		vsema(&bfreeslp);
	} else
		bfree_unlock(0,s);

	/*
	 * b_flags finally get local 'flags' bits.
	 * B_LAST is turned on in efs_i_update
	 */
	bp->b_flags = flags & ~(B_BUSY|B_ASYNC|B_AGE|B_BDFLUSH|B_RELSE|B_HOLD);

	vsema(&bp->b_lock);
}

/*
 * This is just a wrapper for the get_buf() routine.  It is
 * simply a call to get_buf() which will wait on locked buffers.
 */
struct buf *
getblk(
	register dev_t dev,
	register daddr_t blkno,
	int len)
{
	return get_buf(dev, blkno, len, 0);
}


/*
 * Assign a buffer for the given block.  If the appropriate block is
 * already associated, return it; otherwise search for the oldest
 * non-busy buffer and reassign it.
 *
 * The BUF_TRYLOCK flag determines what to do when the requested
 * buffer is found already in the hash table.  If the flag is not set, then
 * we wait for the buffer to be unlocked before returning it.  If the
 * flag is set, then we try to lock the buffer without sleeping
 * and return the buffer if we succeed or NULL if we fail.
 *
 * The BUF_BUSY flag can be specified to indicate that delayed write
 * buffers should not be pushed out by this call.
 * Doing so could cause recursive calls into the buffer cache leading
 * to deadlock or stack overflow.
 *
 * get_buf is the keeper of all the buffer cache consistency.
 * It is important that once the upper portion of get_buf finds that
 * a buffer is not in the cache until the lower portion grabs a buffer
 * locks it and puts in on the correct hash chain that no-one be able
 * to get into the top hash chain search loop.  Holding the hash chain
 * lock throughout ensures this.
 *
 * Buffers may vary in size, but creating buffers that overlap is NOT allowed.
 * It is up to the buffer cache clients to ensure that this never happens.
 *
 * Hash chain rules - may only be modified by holding BOTH hash lock and
 *	freelist lock; may be scanned by holding either.
 *
 * Stats:
 *	getblks - getloops == getfreeempty + getfreedelwri + getbchg + getfreealllck
 */
struct buf *
get_buf(
	register dev_t		dev,
	register daddr_t	blkno,
	int			len,
	uint			flags)
{
	register struct buf *bp;
	register struct buf *dp;	/* hash chain buf will/be on */
	register struct buf *op;	/* hash chain buf WAS on */
	register int	bytelen = BBTOB(len);
	int		innerloopcount;
	int		s;
	int		locked;

	ASSERT(len > 0);
#ifdef BDEBUG
	if (dev == Ddev)
	cmn_err(CE_CONT, "!gb:blk:%x len:%x\n", blkno, len);
#endif

	/* Compute hash bucket address. */
	dp = bhash(dev, blkno);
loop:
	/* lock hash list */
	psema(&dp->b_lock, PRIBIO);
#ifdef BDEBUG
	if (dev == Ndev)
		goto loop2;
#endif
	/*
	 * First check if in cache - buffer must fit exactly.
	 */
	for (bp = dp->b_forw; bp != dp; bp = bp->b_forw) {
		ASSERT(bp);
		/*
		 * See if its valid and matches exactly.
		 * Must attempt to acquire a B_STALE buffer if it
		 * otherwise matches, because we write-and-stale in
		 * one operation as a performance hack (see binval()).
		 */
		if ((bp->b_edev != dev) ||
		    (bp->b_blkno != blkno) ||
		    (bp->b_bcount != bytelen)) {
			continue;
		}
		/* Got it!
		 * Wait for any i/o in progress.
		 * If the buffer is busy and BUF_TRYLOCK is not set then
		 * release hash lock, wait for the buffer,
		 * and reacquire the hash lock.
		 * If the buffer is busy and BUF_TRYLOCK is set, then
		 * release the hash lock and return NULL.
		 */
		locked = cpsema(&bp->b_lock);
		if (!locked && !(flags & BUF_TRYLOCK)) {
			vsema(&dp->b_lock);
			ASSERT(syswait.iowait >= 0);
			atomicAddInt(&syswait.iowait, 1);

			psema(&bp->b_lock, PRIBIO);

			ASSERT(syswait.iowait > 0);
			atomicAddInt(&syswait.iowait, -1);
		} else if (!locked) {
			vsema(&dp->b_lock);
			return NULL;
		} else {
			vsema(&dp->b_lock);
		}
		/*
		 * Buffer could have changed since hash lock does not
		 * guarantee that buffer won't change.
		 */
		if ((bp->b_edev != dev) ||
		    (bp->b_blkno != blkno) ||
		    (bp->b_bcount != bytelen)) {
			/*
			 * Since the buffer is no good, the chain hung
			 * off it may be no good either - in fact, in may
			 * not even be on the hash chain! Forget about it.
			 */
			vsema(&bp->b_lock);
			goto loop;
		}

		if (bp->b_flags & B_STALE) {
			/* notavailspl below will set B_BUSY */
			bp->b_flags = 0;
		} else {
			bp->b_flags |= B_FOUND;
			ASSERT(bp->b_flags & B_DONE);
		}

		ASSERT(!bp->b_vp);

		/* Lock free list and remove bp. */
		notavail(bp);

		return(bp);
	}

#ifdef BDEBUG
	if (bdebug)
		overlapsearch(dev, blkno, len);
#endif

	/* The blocks requested are not in the cache.  Allocate a new
	 * buffer header, then perform the random bookkeeping needed to
	 * make the blocks accessible, making sure to use the base hash
	 * header already computed for the blkno being cached.
	 *
	 * We have primary hash locked and freelist locked.  We must hold
	 * the hash locked until the buffer is salely on the hash list.
	 */
loop2:
	s = bfree_lock(0);
	if ((bp = bfreelist->av_forw) == bfreelist) {
		ASSERT(bfreelist->b_bcount == 0);
		/* Nothing free - release locks and wait. */
		bfslpcnt++;
		bfree_unlock(0,s);
		vsema(&dp->b_lock);
		psema(&bfreeslp, PRIBIO);
		goto loop;	/* see if buffer now in */
	}

	innerloopcount = 0;

	/* Look for one we can get locked.
	 * We don't have to worry about finding a lot of buffers
	 * locked on the free list (and holding off interrupts with
	 * bfreelock held) because very few routines lock buffers
	 * on the free list -- bdflush, bflush, binval -- and they
	 * will each only lock one at a time.
	 */
	while (bp != bfreelist && !cpsema(&bp->b_lock)) {
innerloop:
		bp = bp->av_forw;
	}

	/* If we can't get a block on the free list withouth sleeping,
	 * we'll have to start all over again (sigh!).  delay to
	 * give other processes a chance to unlock their buffers.
	 */
	if (bp == bfreelist) {
startover:
		bfree_unlock(0,s);
		vsema(&dp->b_lock);
		delay(1);
		goto loop;
	}

	/*
	 * If the buffer is still too sticky, put it on the end of
	 * the free list and try another.
	 */
	if (bp->b_ref > 0) {
		register struct buf *forw = bp->av_forw;
		register struct buf *back = bp->av_back;

		back->av_forw = forw;
		forw->av_back = back;

		bfreelist->av_back->av_forw = bp;
		bp->av_back = bfreelist->av_back;
		bfreelist->av_back = bp;
		bp->av_forw = bfreelist;
		bfree_unlock(0,s);

		--bp->b_ref;
		vsema(&bp->b_lock);

		goto loop2;
	}

	/*
	 * If the buffer is pinned, behave as if we couldn't lock
	 * it.  We don't want to sleep waiting for it to be unpinned,
	 * because that could be a long time.
	 */
	if (bp->b_pincount > 0) {
		vsema(&bp->b_lock);
		goto innerloop;
	}

	if (bp->b_flags & B_DELWRI) {
		/*
		 * If the caller has indicated that they do not want
		 * to push delayed write buffers, then don't do so.
		 * If everything is taken over by
		 * such buffers, then we'll just have to depend on
		 * bdflush() to clean some of them up for us eventually.
		 */
		if (flags & BUF_BUSY) {
			vsema(&bp->b_lock);
			goto innerloop;
		}

		notavailspl(bp);
		bfree_unlock(0,s);
		vsema(&dp->b_lock);	/* unlock hash */

		/*
		 * B_AGE ensures that buffer goes to the head of the freeli
		 * on completion so that it doesn't a free ride through the
		 * freelist again; B_ASYNC|B_BDFLUSH ensures that the
		 * transaction will really be asynchronous -- strategy
		 * routines must always queue up a buffer with these
		 * flags set if the routine might have to wait for a resource.
		 */
		bp->b_flags |= B_ASYNC | B_BDFLUSH | B_AGE;
		bwrite(bp);	/* eventually freeing bp internally ... */
		goto loop;
	}

	if (bp->b_relse) {
		notavailspl(bp);
		bfree_unlock(0,s);
		vsema(&dp->b_lock);	/* unlock hash */
		(*bp->b_relse)(bp);
		goto loop;
	}

	/* Find old hash bucket.  Note that iff bp->b_edev == NODEV,
	 * bp must be hashed on bfreelist and, thus, bhash will
	 * point to an innocent hash buffer (always the next to last
	 * hash header with the current definition of bhash).
	 */
	if (bp->b_edev == (dev_t) NODEV)
		op = NULL;
	else {
		op = bhash(bp->b_edev, bp->b_blkno);

		/*
		 * Make sure we can lock old hash list.
		 */
		if (dp != op && !cpsema(&op->b_lock)) {
			/*
			 * Couldn't get old hash.
			 */
			vsema(&bp->b_lock);

			/* Traversed too many buffers with spinlock held?
			 * Release lock so interrupts can get in,
			 * and start over.
			 */
			if (innerloopcount++ > 8) {
				goto startover;
			}

			/* Otherwise, try the next available buffer. */
			goto innerloop;
		}
	}

	/* Got one. (Whew!)
	 * Now take it off the freelist.
	 */
	notavailspl(bp);

	bp->b_flags = B_BUSY;	/* reset all flags (AFTER we can't fail!)  */

	ASSERT(bp->b_relse == 0);
	ASSERT(bp->b_iodone == 0);
	ASSERT(op || bp->b_forw == bp);

	bfree_unlock(0,s);

	if (op) {
		/* Take off old hash queue. */
		bp->b_back->b_forw = bp->b_forw;
		bp->b_forw->b_back = bp->b_back;
		if (op != dp)
			vsema(&op->b_lock);
	} else {
		ASSERT(bp->b_forw == bp && bp->b_back == bp);
	}

	/* Put on new hash. */
	bp->b_forw = dp->b_forw;
	bp->b_back = dp;
	dp->b_forw->b_back = bp;
	dp->b_forw = bp;

	bp->b_edev = dev;
	bp->b_blkno = blkno;
	bp->b_bcount = bytelen;		/* MUST be set up before */
					/* unlocking hash list lock. */
	vsema(&dp->b_lock);		/* Release new hash. */

	bio_realloc(bp);

	return (bp);
}

/*
 * Get an empty block, not assigned to any particular device.
 */
struct buf *
ngeteblkdev(
	dev_t	dev,
	size_t len)
{
	register struct buf *bp;
	register struct buf *dp, *op;
	int s, innerloopcount;

	dp = bfreelist;
loop:
	s = bfree_lock(0);
	if ((bp = bfreelist->av_forw) == dp) {
		ASSERT(bfreelist->b_bcount == 0);
		/* Nothing free - release locks and wait. */
		bfslpcnt++;
		bfree_unlock(0,s);
		psema(&bfreeslp, PRIBIO);
		goto loop;
	}

	innerloopcount = 0;

	/* look for one we can get locked. */
	while (bp != dp && !cpsema(&bp->b_lock))
innerloop:
		bp = bp->av_forw;

	/*
	 * Avoid spinning if there is no buffer readily available.
	 */
	if (bp == dp) {
startover:
		bfree_unlock(0,s);
		delay(1);
		goto loop;
	}

	/*
	 * If the buffer is still too sticky, put it on the end of
	 * the free list and try another.
	 */
	if (bp->b_ref > 0) {
		register struct buf *forw = bp->av_forw;
		register struct buf *back = bp->av_back;

		back->av_forw = forw;
		forw->av_back = back;

		bfreelist->av_back->av_forw = bp;
		bp->av_back = bfreelist->av_back;
		bfreelist->av_back = bp;
		bp->av_forw = bfreelist;
		bfree_unlock(0,s);

		--bp->b_ref;
		vsema(&bp->b_lock);

		goto loop;
	}

	/*
	 * If the buffer is pinned, behave as if we couldn't lock
	 * it.  We don't want to sleep waiting for it to be unpinned,
	 * because that could be a long time.
	 */
	if (bp->b_pincount > 0) {
		vsema(&bp->b_lock);
		goto innerloop;
	}

	/*
	 * Can't unlock freelist until buffer is off/on hash chain.
	 */
	if (bp->b_flags & B_DELWRI) {
		notavailspl(bp);
		bfree_unlock(0,s);
		/*
		 * B_AGE ensures that the buffer goes to the head
		 * of the freelist upon completion, and doesn't get
		 * a free ride through the freelist; B_ASYNC|B_BDFLUSH
		 * ensures that the transaction will be strictly async.
		 */
		bp->b_flags |= B_ASYNC | B_BDFLUSH | B_AGE;
		bwrite(bp);
		goto loop;
	}

	if (bp->b_relse) {
		notavailspl(bp);
		bfree_unlock(0,s);
		(*bp->b_relse)(bp);
		goto loop;
	}

	/*
	 * Take off hash queue if hash lock is available.
	 * If not, continue on.  bp isn't removed from the
	 * free list until after hash lock is acquired so
	 * we don't have to place buffer back on if cpsema fails.
	 */
	if (bp->b_edev == (dev_t) NODEV)
		op = (struct buf *)0;
	else {
		op = bhash(bp->b_edev, bp->b_blkno);
		if (!cpsema(&op->b_lock)) {
			vsema(&bp->b_lock);

			/* Traversed too many buffers with spinlock held?
			 * Release lock so interrupts can get in,
			 * and start over.
			 */
			if (innerloopcount++ > 8) {
				goto startover;
			}
			goto innerloop;
		}
	}

	/* Got one. (Whew!) */
	notavailspl(bp);
	bp->b_flags = B_BUSY|B_AGE;
	ASSERT(bp->b_relse == 0);
	ASSERT(bp->b_iodone == 0);
	ASSERT(bp->b_error == 0);

	bfree_unlock(0,s);

	if (op) {
		/* Take off old hash queue. */
		bp->b_back->b_forw = bp->b_forw;
		bp->b_forw->b_back = bp->b_back;
		vsema(&op->b_lock);

		/*
		 * Buffers that are anonymous to the buffer cache proper
		 * aren't hashed to any buffer cache hash chain.  They
		 * are set to point to themselves, and must be returned
		 * that way.
		 */
		bp->b_forw = bp->b_back = bp;
	} else {
		ASSERT(bp->b_forw == bp && bp->b_back == bp);
	}

	ASSERT(bp->b_dforw == bp);
	ASSERT(bp->b_offset == 0);
	bp->b_blkno = -1;
	bp->b_bcount = (uint_t)len;
	bp->b_edev = dev;
	bp->b_ref = 0;
	bp->b_resid = 0;
	bp->b_remain = 0;

	bio_realloc(bp);

	return(bp);
}

/*
 * Get a block, not assigned to any particular device
 */
buf_t *
ngeteblk(
        size_t  len)
{
        return(ngeteblkdev((dev_t)NODEV, len));
}

/*
 * Allocate a phys buffer.
 */
struct buf *
getphysbuf(dev_t dev)
{
	return (ngeteblkdev(dev, 0));
}

/*
 * Return bp to phys buffer pool.
 */
void
putphysbuf(register struct buf *bp)
{
	bp->b_flags = B_STALE | B_AGE | B_BUSY;
	bp->b_bcount = bp->b_bufsize = 0;
	bp->b_relse = 0;
	bp->b_error = 0;
	bp->b_edev = NODEV;
	bp->b_vp = NULL;
	bp->b_offset = 0;
	bp->b_parent = NULL;
	bp->b_forw = bp->b_back = bp;
	brelse(bp);
}

/*
 * Wait for I/O completion on the buffer; return errors to the user.
 * WARNING - this routine and iodone may be called with 'alien' bp's
 * which are not using semaphores.
 */
int
biowait(register struct buf *bp)
{
	ASSERT(syswait.iowait >= 0);
	atomicAddInt(&syswait.iowait, 1);

	ASSERT(bp->b_flags & B_BUSY);
	/*
	 * Only bio buffer headers must be locked.
	 */
	ASSERT(bp < &global_buf_table[0] ||
	       &global_buf_table[v.v_buf] <= bp ||
	       valusema(&bp->b_lock) <= 0);
	ASSERT(valusema(&bp->b_iodonesema) <= 1);

	psema(&bp->b_iodonesema, PRIBIO);

	ASSERT(syswait.iowait > 0);
	atomicAddInt(&syswait.iowait, -1);
	return geterror(bp);
}

/*
 * Mark I/O complete on a buffer, release it if I/O is asynchronous,
 * and wake up anyone waiting for it.
 */
void
biodone(register struct buf *bp)
{
        /*
         * Call guaranteed rate I/O completion routine.
         */
        if (bp->b_flags & B_GR_ISD)
                grio_iodone(bp);

	/*
	 * If biodone() special handling is asked for,
	 * indirect through the function pointer.
	 */
	if (bp->b_iodone) {
		(*bp->b_iodone)(bp);
		return;
	}

	ASSERT(bp->b_flags & B_BUSY);
	/* only bio buffer headers must be locked */
	ASSERT((bp < &global_buf_table[0] ||
		bp >= &global_buf_table[NBUF]) ||
	       valusema(&bp->b_lock) <= 0);
	ASSERT(!(bp->b_flags & B_DONE));

	bp->b_flags |= B_DONE;

	if (bp->b_flags & B_ASYNC) {
		brelse(bp);
	} else {
		/* Kludge to support un-converted drivers that use bufs.
		 * We really should get rid of this.
		 * Don't bother with our real buffers
		 */
		if (bp < &global_buf_table[0] ||
		    bp >= &global_buf_table[v.v_buf])
			wakeup((caddr_t)bp);
		bp->b_flags &= ~B_WANTED;

		vsema(&bp->b_iodonesema);	/* the real way */
	}
}

/*
 * Zero the core associated with a buffer.
 */
void
clrbuf(register struct buf *bp)
{
	ASSERT(bp->b_un.b_addr && bp->b_bcount);
	bzero(bp->b_un.b_addr, (int)bp->b_bcount);
	bp->b_resid = 0;
}

/*
 * Make sure all write-behind blocks on dev
 * (NODEV implies all) are flushed out.
 */
void
bflush(register dev_t dev)
{
	register struct buf *bp;
	register int j;

	for (bp = &global_buf_table[0], j = v.v_buf; --j >= 0; bp++) {
		if ((bp->b_edev == dev || dev == NODEV) &&
		    ((bp->b_flags & (B_STALE|B_DELWRI)) == B_DELWRI)) {
			psema(&bp->b_lock, PRIBIO);

			if (bp->b_pincount > 0) {
				/*
				 * Don't mess with pinned buffers.  Trying
				 * to flush them here can interfere with
				 * file system locking conventions.
				 */
				vsema(&bp->b_lock);
				continue;
			}

			if ((bp->b_edev == dev || dev == NODEV) &&
			    ((bp->b_flags & (B_STALE|B_DELWRI)) == B_DELWRI)) {

				notavail(bp);
				bp->b_flags |= (B_ASYNC|B_BFLUSH);
				bwrite(bp);
			} else
				vsema(&bp->b_lock);
		}
	}

	for (bp = &global_buf_table[0], j = v.v_buf; --j >= 0; bp++) {
		/*
		 * Grab the buffer to ensure that any writes in
		 * progress complete.  Only acquire lock for buffers
		 * marked and pushed out above; this prevents
		 * raw device buffers from being acquired here.
		 */
		if (bp->b_flags & B_BFLUSH) {
			psema(&bp->b_lock, PRIBIO);
			bp->b_flags &= ~B_BFLUSH;
			vsema(&bp->b_lock);
		}
	}
}

#ifdef DEBUG
/* ARGSUSED */
void
bflushed(dev_t dev)
{
	return;
}
#endif

sema_t bdwakeup;	/* bdflush waits on this */

void
bdflushinit(void)
{
	/* init bdflush semas */
	initnsema(&bdwakeup, 0, "bdwake");
}

/*
 * Write all delayed-write buffers and stales all buffers for ``dev''.
 * binval guarantees only that any buffers associated with ``dev'' AT
 * THE TIME BINVAL IS CALLED will be written and removed.
 * The caller must ensure that no activity on that device occurs if it needs
 * to guarantee that no buffers whatsoever exist upon return from binvalfree.
 */
void
binval(register dev_t dev)
{
	register struct buf *bp;
	register int j;

	bp = &global_buf_table[0];
	for (j = v.v_buf; j > 0; j--, bp++) {
		if (bp->b_edev == dev) {
			psema(&bp->b_lock, PRIBIO);
			if (bp->b_edev != dev) {
				vsema(&bp->b_lock);
				continue;
			}
			if (bp->b_flags & B_DELWRI) {
				notavail(bp);
				bp->b_flags &= ~(B_DELWRI|B_ASYNC);
				/*
				 * Stale buffer now so we won't have
				 * to reacquire buffer after bwrite
				 * just to stale it.
				 */
				bp->b_flags |= B_STALE;
				bwrite(bp);
			} else {
				bp->b_flags |= B_STALE;
				ASSERT(!(bp->b_flags & B_ASYNC));
				if (bp->b_relse) {
					notavail(bp);
					brelse(bp);
				} else
					vsema(&bp->b_lock);
			}
		}
	}
}

/*
 * Allocate len bb's worth of physical memory for the given buffer.
 * The buffer region is locked on entry.
 * The size of the currently-allocated buffer (in bytes) is bp->b_bufsize.
 * The size to be allocated (in bytes) is bp->b_bcount.
 */
static void
bio_realloc(register struct buf *bp)
{
	if (bp->b_un.b_addr != NULL) {
		free(bp->b_un.b_addr);
	}
	bp->b_bufsize = (uint_t)BBTOB(BTOBB(bp->b_bcount));
	if (bp->b_bufsize == 0) {
		bp->b_un.b_addr = NULL;
	} else {
		bp->b_un.b_addr = (caddr_t)malloc(bp->b_bufsize);
	}
}

/*
 * Free up the memory space consumed by the given buffer
 * Assumes bp is locked (BUSY) on entry
 */
static void
bio_freemem(register struct buf *bp)
{
	register int olen = (int)BTOBBT(bp->b_bufsize);

	ASSERT((bp->b_flags & B_BUSY) && (valusema(&bp->b_lock) <= 0));
	ASSERT(olen == BTOBB(bp->b_bufsize));

	if (olen == 0)
		return;

	ASSERT(bp->b_un.b_addr);

	bp->b_bufsize = 0;
	free(bp->b_un.b_addr);
}

#ifdef BDEBUG
/*
 * A buffer was not found in the cache.  This is a sanity check that
 * there are no overlapping buffers in the cache.  We single thread
 * through this code holding the freelist the entire time: thus we
 * need not lock any of the hash chains that we look through.
 * If we find an overlap, we make very loud noises.
 */
void
overlapsearch(dev_t dev, daddr_t blkno, int len)
{
	register struct buf *bp;
	register struct buf *tp;	/* temp hash chain for bucket search */
	register daddr_t endblkno;
	int s, i;

	endblkno = blkno + len;

	s = bfree_lock(0);

	for (i = 0; i < v.v_hbuf; i++) {
		tp = (buf_t *)&global_buf_hash[i];
		for (bp = tp->b_forw; bp != tp; bp = bp->b_forw) {
			ASSERT(bp);
			/*
			 * Pass if it's stale or being tossed
			 * or not same device...
			 */
			if ((bp->b_flags & B_STALE) || (bp->b_edev != dev)) {
				continue;
			}
			/*
			 * or if our request completely misses the given
			 * buffer.
			 */
			if ((endblkno <= bp->b_blkno) ||
			    (blkno >= bp->b_blkno + BTOBBT(bp->b_bufsize))) {
				continue;
			}
			cmn_err(CE_PANIC, "buffer 0x%x overlaps dev 0x%x blkno 0x%x len 0x%x global_buf_hash[0x%x]", bp, dev, blkno, len, i);
		}
	}
	bfree_unlock(0,s);
}
#endif

/*
 * bioerror
 *  Set or clear the error flag and error field in a buffer header.
 */
void
bioerror(struct buf *bp, int errno)
{
	bp->b_error = (char)errno;
	if (errno) {
		bp->b_flags |= B_ERROR;
	} else {
		bp->b_flags &= ~B_ERROR;
	}
	return;
}

/*
 * Allocate buffer header.
 */
buf_t *
getrbuf(int sleep)
{
        buf_t *bp = kmem_zone_zalloc(buf_zone, sleep);
        if (bp == NULL) {
                return (NULL);
        }
        initsema(&bp->b_lock, 1);
	psema(&bp->b_lock, PRIBIO);
        initsema(&bp->b_iodonesema, 0);
	bp->b_edev = NODEV; 
	bp->av_forw = bp->av_back = bp;
	bp->b_forw = bp->b_back = bp;
        bp->b_flags |= B_BUSY;
	return(bp);
}

/*
 * Allocate a buffer header with the specified amount
 * of attached memory.  The units of the len parameter
 * are bytes.
 *
 * This routine may sleep allocating memory.
 */
buf_t *
ngetrbuf(size_t len)
{
	buf_t	*bp;

	bp = getrbuf(0);
	ASSERT(bp != NULL);
	bp->b_bcount = (uint_t)len;
	bio_realloc(bp);
	return (bp);
}

/*
 * void
 * freerbuf(buf_t *)
 *      free up space allocated by getrbuf()
 *
 * Calling/Exit State:
 *
 *      None
 *
 */
void
freerbuf(buf_t *bp)
{
	freesema(&bp->b_iodonesema);
	freesema(&bp->b_lock);
        kmem_zone_free(buf_zone, bp);
}

/*
 * Free up space allocated by ngetrbuf().
 */
void
nfreerbuf(buf_t *bp)
{
	freesema(&bp->b_iodonesema);
	bio_freemem(bp);
	freesema(&bp->b_lock);
        kmem_zone_free(buf_zone, bp);
}

void
reinitrbuf(buf_t *bp)
{
        if (bp == NULL) 
		return;
	freesema(&bp->b_iodonesema);
	freesema(&bp->b_lock);
	bzero(bp, sizeof(*bp));
        initsema(&bp->b_lock, 1);
	psema(&bp->b_lock, PRIBIO);
        initsema(&bp->b_iodonesema, 0);
	bp->b_edev = NODEV; 
	bp->av_forw = bp->av_back = bp;
	bp->b_forw = bp->b_back = bp;
        bp->b_flags |= B_BUSY;
}

void
iodone(struct buf *bp)
{
	biodone(bp);
}

int
iowait(struct buf *bp)
{
	return(biowait(bp));
}


/*
 * Pick up the device's error number and pass it to the user;
 * if there is an error but the number is 0 set a generalized code
 */
int
geterror(struct buf *bp)
{
        int     error;

	error = 0;
	if (bp->b_flags & B_ERROR) {
		error = bp->b_error;
		if (!error)
			error = EIO;
	}
	return(error);
}

/*
 * Get an empty 1024 byte block, not assigned to any particular device.
 */
buf_t *
geteblk(void)
{
	return(ngeteblk(1024));
}

/*
 * B_PAGEIO routine to return next page of buffer.
 */
pfd_t *
getnextpg(register buf_t *bp, register pfd_t *pfd)
{
	if (pfd)
		return(pfd->pf_pchain);
	else
		return(bp->b_pages);
}

/*
 * From chunkio.c in kern/sgi
 */
#ifdef DEBUG_BUFTRACE
ktrace_t	*buf_trace_buf;

/*
 * Log the given buffer with the given identifier string in the
 * chunk tracebuffer.
 */
void
buftrace_enter(
	char	*id,
	buf_t	*bp,
	inst_t	*ra)
{
	if (buf_trace_buf == NULL) {
		/*
		 * We're not initialized yet, so just get out.
		 */
		return;
	}

	/*
	 * If the bp's trace buffer isn't initialized, then
	 * other things may not be either.  In that case
	 * just don't do anything.
	 */
	if (bp->b_trace == NULL) {
		return;
	}

	ktrace_enter(buf_trace_buf,
		     (void *)id,
		     (void *)(__psint_t)0,		/* cpuid() */
		     (void *)bp,
		     (void *)(__psint_t)(bp->b_flags),
		     (void *)(bp->b_offset >> 32),
		     (void *)(bp->b_offset & 0xffffffff),
		     (void *)(__psint_t)(bp->b_bcount),
		     (void *)(__psint_t)(bp->b_bufsize),
		     (void *)(bp->b_blkno),
		     (void *)ra,
		     (void *)(bp->b_forw),
		     (void *)(bp->b_back),
		     (void *)(bp->b_vp),
		     (void *)(bp->b_dforw),
		     (void *)(bp->b_dback),
		     (void *)(__psint_t)(valusema(&(bp->b_lock))));

	ktrace_enter(bp->b_trace,
		     (void *)id,
		     (void *)(__psint_t)0,		/* cpuid() */
		     (void *)bp,
		     (void *)(__psint_t)(bp->b_flags),
		     (void *)(bp->b_offset >> 32),
		     (void *)(bp->b_offset & 0xffffffff),
		     (void *)(__psint_t)(bp->b_bcount),
		     (void *)(__psint_t)(bp->b_bufsize),
		     (void *)(bp->b_blkno),
		     (void *)ra,
		     (void *)(bp->b_forw),
		     (void *)(bp->b_back),
		     (void *)(bp->b_vp),
		     (void *)(bp->b_dforw),
		     (void *)(bp->b_dback),
		     (void *)(__psint_t)(valusema(&(bp->b_lock))));

}
#endif	/* DEBUG_BUFTRACE */
