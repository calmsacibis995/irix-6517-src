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
#ident	"$Revision: 3.250 $"

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/bitmasks.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/hwgraph.h>
#include <sys/time.h>
#include <sys/immu.h>
#include <sys/kmem.h>
#include <sys/kthread.h>
#include <sys/kopt.h>
#include <sys/map.h>
#include <sys/par.h>
#include <sys/param.h>
#include <sys/pfdat.h>
#include <sys/sema.h>
#include <sys/schedctl.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/tuneable.h>
#include <sys/timers.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/ktrace.h>
#include <sys/uuid.h>
#include <sys/grio.h>
#include <ksys/sthread.h>
#include <ksys/vhost.h>
#ifdef _SHAREII
#include <sys/shareIIstubs.h>
#endif
#include <sys/nodepda.h>

#ifdef BDEBUG
int Ddev = 0xdeadbabe;		/* allow buffer tracing on a dev */
int Ndev = 0xdeadbabe;		/* disable buffers for dev Ndev */
int bdebug = 0;
static void overlapsearch(dev_t, daddr_t, int);
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

buf_t	*bfreelist;
int	bfreelistmask;
#ifdef DEBUG_BUFTRACE
static	int bfreelist_check_enable = 0;
#endif

/*
 * locks & semas for bufs and pbufs
 */
lock_t		bfraglock;	/* lock on buffer frag list */
int		bufmem;
extern int 	min_bufmem;	/* min metadata memory that is cached */
int		bemptycnt;	/* # of empty buffers on free list */
buf_t		binactivelist;	/* place to park inactive buffers */
extern int	bmappedcnt;	/* # of kernel virtual pages K2SEG mapped */
extern int	valid_dcache_reasons;
static struct zone *buf_zone;	/* buffer heap zone */
dhbuf_t		nodev_chain;

lock_t		bpinlock;
bwait_pin_t	bwait_pin[NUM_BWAIT_PIN];

extern volatile int b_unstablecnt;
/*
 * This is the number of buffers for which we will respect the b_ref
 * field before simply grabbing one and reclaiming it.
 */
#define	BUF_REF_MAX_LOOK	16

/*
 * clamp the exponential delay to 4 secs
 */
#define BACKOFF_CAP	4

/*
 * Unlink a buffer from the available list and mark it busy.
 * notavailspl assumes that the free list is locked.
 * Modified to minimize memory references.
 */
#define	notavailspl(bp) { \
	buf_t *forw = bp->av_forw; \
	buf_t *back = bp->av_back; \
	back->av_forw = forw; \
	forw->av_back = back; \
	bp->b_flags |= B_BUSY; \
	ASSERT(spinlock_islocked((lock_t*)&bfreelist[bp->b_listid].b_private)); \
	bfreelist[bp->b_listid].b_bcount--; \
	bfreelist_check(&bfreelist[bp->b_listid]); \
}

static void bio_realloc(buf_t *bp);
void bio_freemem(buf_t *bp);
static int  bp_match(buf_t *bp, int field, void *value);
void dchunkflush(time_t);
extern void chunkcommit(buf_t *, int);

/*
 * Data structure to manage partial-page (fragment) buffers.
 * An array of frag_t-s are allocated as headers, and the first
 * sizeof(frag_t) bytes of each frag is overlaid with this
 * structure.
 */
typedef struct fraglist {
	struct fraglist	*frag;	/* ptr to memory -- MUST BE FIRST */
	short	bbsize;		/* size of this frag pool, in BBs */
	short	dirty;		/* is particular frag cache-dirty? */
#ifdef DEBUG
	short	count;
	short	hiwater;
#endif
} frag_t;

frag_t fraglist[BPCSHIFT-BBSHIFT+1];

/*
 * These macros are used to determine the current freelist to use.
 */
#define	FREELIST()	(private.p_hand & bfreelistmask)
#define	FREELIST_INC()	(private.p_hand++ & bfreelistmask)

int	_bhash_shift;	/* shift count for buffer hashing, see v_hbuf */

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
	mask = (1 << shift) - 1;	/* this is v.v_hmask */
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
	buf_t		*bp;
	buf_t		*dp;
	hbuf_t		*hbp;
	dhbuf_t		*dbp;
	int		i;
	int		bbsize;
	bwait_pin_t	*bwp;
	int		nlists;
	int		listid;
	void		breservemem(pgno_t);

	if (buf_zone) {
		return;
	}

	bp = &binactivelist;
	bp->b_forw = bp->b_back = bp;
	bp->av_forw = bp->av_back = bp;
	bp->b_flags = B_INACTIVE;
	spinlock_init((lock_t *)&bp->b_start, "binactive");

	nlists = MIN(numcpus, 8);
	/*
	 * Round down to the nearest power of 2.
	 */
	while (nlists & (nlists - 1)) {
		nlists--;
	}
	bfreelistmask = nlists - 1;
	
	/*
	 * We use a single buf_t for each free list.
	 * b_bcount is the number of buffers in the list.
	 * b_blkno is the number of processes waiting for a
	 * buffer to be freed into the list (get it, 'block'no).
	 * b_private is cast to a lock_t and used as the spinlock
	 * guarding the free list.
	 * b_fsprivate is cast to a lock_t and used as the spinlock
	 * guarding the bfreelist[0] dirty buffer chain (b_dforw/b_dback).
	 * b_iodonesema is used as the free buffer wait semaphore.
	 */
	bfreelist = (buf_t *)kmem_zalloc((nlists * sizeof(buf_t)),
					 KM_SLEEP);
	for (i = 0; i < nlists; i++) {
		dp = &bfreelist[i];
		dp->b_forw = dp->b_back = dp;
		dp->av_forw = dp->av_back = dp;
		spinlock_init((lock_t*)&(dp->b_private), "bfreelk");
		init_sema(&(dp->b_iodonesema), 0, "bfreeslp", i);
		spinlock_init((lock_t*)&(dp->b_fsprivate), "bfree_dirty_lk");
	}
	spinlock_init(&bpinlock,  "bpinlck");
	spinlock_init(&bfraglock, "bfraglk");

	listid = 0;
	for (i = 0, bp = global_buf_table; i < v.v_buf; i++, bp++) {
		bp->b_edev = NODEV;
		bp->bd_forw = bp->bd_back = bp;
		bp->b_forw = bp->b_back = bp;
		bp->b_dforw = bp->b_dback = bp;
		bp->bd_hash = NULL;
		bp->b_vp = NULL;
		bp->b_flags = B_BUSY;
		bp->b_bvtype = 0;
		bp->b_bcount = 0;
		bp->b_relse = NULL;
		bp->b_iodone = NULL;
		bp->b_bdstrat = NULL;
		bp->b_target = NULL;
		
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
		/*
		 * Simply round robin the buffers into the different
		 * free lists.  brelse() will place the buffer in the
		 * proper list.
		 */
		bp->b_listid = listid++;
		if (listid == nlists) {
			listid = 0;
		}
		brelse(bp);
	}

	/* 
	 * Enable bfreelist checking (in DEBUG kernels).  We don't
	 * really want to enable it until the initial set up has
	 * completed because it can take a really long time to boot
	 * on a large system, and it doesn't really buy us anything
	 * unless something is very very wrong.
	 */
#ifdef DEBUG_BUFTRACE
	bfreelist_check_enable = 1;
#endif

	for (bbsize = 1, i = 0; i <= BPCSHIFT-BBSHIFT; i++) {
		fraglist[i].bbsize = bbsize;
		bbsize = bbsize << 1;
	}

	/* initialize hash buffer headers */
	for (i = 0, hbp = global_buf_hash; i < v.v_hbuf; i++, hbp++) {
		init_sema(&hbp->b_lock, 1, "hbf", i);
		hbp->b_forw = hbp->b_back = (buf_t *)hbp;
	}

	/* initialize hash dev headers */
	for (i = 0, dbp = global_dev_hash; i < v.v_hbuf; i++, dbp++) {
		spinlock_init(&dbp->bd_lock,  "hdbf");
		dbp->bd_forw = dbp->bd_back = (buf_t *)dbp;
	}

	spinlock_init(&nodev_chain.bd_lock,  "hdbf");
	nodev_chain.bd_forw = nodev_chain.bd_back = (buf_t *)&nodev_chain;

	/* initialize bwait_pin structures. */
	for (i = 0, bwp = bwait_pin; i < NUM_BWAIT_PIN; i++, bwp++) {
		init_sv(&bwp->bwp_wait, SV_DEFAULT, "bwp", i);
	}

	if (showconfig) {
		printf("%d buffers\n", v.v_buf);
	}

	buf_zone = kmem_zone_init(sizeof(buf_t), "rbufs");

	/*
	 * Set up the minimum number of pages we'll cache for
	 * metadata buffers in the buffer cache
	 */
	if (min_bufmem == 0)
		min_bufmem = physmem / 50; 
	breservemem((pgno_t)min_bufmem);
	b_unstablecnt = 0;
}

/*
 * Place buffer on device chain, potentially removing from an
 * old chain first. Called with buffer locked.
 */
static void
dev_chain(buf_t *bp)
{
	dhbuf_t	*bdp;
	int	s;

	buftrace("DEVCHAIN", bp);

	if (bp->bd_hash) {
		/* Buffer already on device hash, remove if on a different
		 * chain, if on same hash just return.
		 */
		bdp = bp->bd_hash;
		if (bdhash(bp->b_edev) != bdp) {
			s = mutex_spinlock(&bdp->bd_lock);
			bp->bd_forw->bd_back = bp->bd_back;
			bp->bd_back->bd_forw = bp->bd_forw;
			mutex_spinunlock(&bdp->bd_lock, s);
		} else {
			return;
		}
	} else {
		ASSERT((bp->bd_back == bp) && (bp->bd_forw == bp));
	}

	if (bp->b_edev != (dev_t)NODEV) {
		bdp = bdhash(bp->b_edev);
	} else {
		bdp = &nodev_chain;
	}

	s = mutex_spinlock(&bdp->bd_lock);
	bp->bd_forw = bdp->bd_forw;
	bp->bd_back = (buf_t *)bdp;
	bdp->bd_forw->bd_back = bp;
	bdp->bd_forw = bp;
	bp->bd_hash = bdp;
	mutex_spinunlock(&bdp->bd_lock, s);
}

/*
 * Remove buffer from device chain, called with buffer locked
 */
void
dev_unchain(buf_t *bp)
{
	int	s;
	dhbuf_t	*bdp;

	bdp = bp->bd_hash;
	if (!bdp) {
		ASSERT((bp->bd_back == bp) && (bp->bd_forw == bp));
		buftrace("DEVUNCHAIN NOCHAIN", bp);
		return;
	}

	buftrace("DEVUNCHAIN", bp);

	s = mutex_spinlock(&bdp->bd_lock);
	bp->bd_forw->bd_back = bp->bd_back;
	bp->bd_back->bd_forw = bp->bd_forw;
	bp->bd_forw = bp->bd_back = bp;
	bp->bd_hash = NULL;
	mutex_spinunlock(&bdp->bd_lock, s);
}
		

/*
 * Remove the given buffer from the buffer free list.
 */
void
notavail(buf_t *bp)
{
	int	s;
	int	listid;

	listid = bp->b_listid;
	s = bfree_lock(listid);
	notavailspl(bp);	/* sets B_BUSY */
	bfree_unlock(listid, s);
}

#ifdef DEBUG_BUFTRACE
void
bfreelist_check(
	buf_t	*freelist)
{
	buf_t	*bp;
	int	count;

	if (!bfreelist_check_enable)
		return;

	ASSERT(spinlock_islocked((lock_t*)&(freelist->b_private)));
	count = 0;
	bp = freelist->av_forw;
	while (bp != freelist) {
		count++;
		bp = bp->av_forw;
	}
	ASSERT(count == freelist->b_bcount);
}
#endif

/*
 * This is just a wrapper for read_buf().  It calls read_buf()
 * with a flags parameter value indicating that we want
 * to sleep on the buffer lock if it is already in-core.
 */
buf_t *
bread(
	dev_t dev,
	daddr_t blkno,
	int	len)
{
	return read_buf(dev, blkno, len, 0);
}

/*
 * Read in (if necessary) the block and return a buffer pointer.
 * The flags parameter allows the caller to specify (with BUF_TRYLOCK)
 * that we should not sleep on the buffer lock if the requested buffer is
 * already in-core and is locked.  It also allows the caller to specify
 * (with BUF_BUSY) that we should not push out delayed write buffers
 * in this call.
 */
buf_t *
read_buf(
	dev_t		dev,
	daddr_t		blkno,
	int		len,
	uint		flags)
{
	buf_t *bp;
	struct bdevsw *my_bdevsw;

#ifdef BDEBUG
	if (dev == Ddev)
		cmn_err(CE_CONT, "!bread:blk:%d len:%d\n", blkno, len);
#endif
	ASSERT((len > 0) && (len <= BIO_MAXBBS));
	SYSINFO.lread += len;
	bp = get_buf(dev, blkno, len, flags);
	if (bp == NULL) {
		return NULL;
	}
	bp->b_target = NULL;
	ASSERT(bp->b_blkno == blkno && bp->b_bcount == BBTOB(len));
	if (bp->b_flags & B_DONE) {
		return (bp);
	}

	bp->b_flags |= B_READ;
	my_bdevsw = get_bdevsw(dev);
	ASSERT(my_bdevsw != NULL);
	bdstrat(my_bdevsw, bp);
	KTOP_UPDATE_CURRENT_INBLOCK(1);
#ifdef _SHAREII
	/*
	 *	ShareII potentially charges for BIO activity.
	 */
	SHR_BIO(SH_BIO_BREAD);
#endif /* _SHAREII */	
	SYSINFO.bread += len;
	biowait(bp);
	return (bp);
}

/*
 * Version of the above using a buftarg_t instead of a dev_t to
 * specify the device.
 */
buf_t *
read_buf_targ(
	buftarg_t	*target,
	daddr_t		blkno,
	int		len,
	uint		flags)
{
	buf_t *bp;

#ifdef BDEBUG
	if (target->dev == Ddev)
		cmn_err(CE_CONT, "!bread:blk:%d len:%d\n", blkno, len);
#endif
	ASSERT(target);
	ASSERT((len > 0) && (len <= BIO_MAXBBS));
	SYSINFO.lread += len;
	bp = get_buf(target->dev, blkno, len, flags);
	if (bp == NULL) {
		return NULL;
	}
	bp->b_target = target;
	ASSERT(bp->b_blkno == blkno && bp->b_bcount == BBTOB(len));
	if (bp->b_flags & B_DONE) {
		return (bp);
	}

	bp->b_flags |= B_READ;
#ifdef CELL_IRIX
	{
		vnode_t *vp;
		vp = target->specvp;
		ASSERT(vp);
		VOP_STRATEGY(vp, bp);
	}
#else
	{
		struct bdevsw	*my_bdevsw;

		my_bdevsw = target->bdevsw;
		ASSERT(my_bdevsw != NULL);
		bdstrat(my_bdevsw, bp);	
	}
#endif
	KTOP_UPDATE_CURRENT_INBLOCK(1);
#ifdef _SHAREII
	/*
	 *	ShareII potentially charges for BIO activity.
	 */
	SHR_BIO(SH_BIO_BREAD);
#endif
	SYSINFO.bread += len;
	biowait(bp);
	return(bp);
}

/*
 * Read in the block, like bread, but also start I/O on the
 * read-ahead block (which is not allocated to the caller)
 */
buf_t *
breada(
	dev_t		dev,
	daddr_t	blkno,
	int		len,
	daddr_t	rablkno,
	int ralen)
{
	buf_t	*bp, *rabp;
	struct bdevsw		*my_bdevsw;

	ASSERT((len > 0) && (len <= BIO_MAXBBS));
	ASSERT((ralen > 0) ? ralen <= BIO_MAXBBS : 1);
#ifdef BDEBUG
	if (dev == Ddev)
		cmn_err(CE_CONT, "!breada:blk:%d len:%d rablk:%d ralen:%d\n",
				blkno, len, rablkno, ralen);
#endif

	bp = NULL;
	my_bdevsw = get_bdevsw(dev);
	ASSERT(my_bdevsw != NULL);
	if (!incore(dev, blkno, len, 0)) {
		SYSINFO.lread += len;
		bp = getblk(dev, blkno, len);
		ASSERT(!bp->b_vp);
		ASSERT(bp->b_blkno == blkno && bp->b_bcount == BBTOB(len));
		if ((bp->b_flags & B_DONE) == 0) {
			bp->b_flags |= B_READ;
			bdstrat(my_bdevsw, bp);
#ifdef _SHAREII
			/*
			 *	ShareII potentially charges for BIO activity.
			 */
			SHR_BIO(SH_BIO_BREAD);
#endif /* _SHAREII */			
			KTOP_UPDATE_CURRENT_INBLOCK(1);
			SYSINFO.bread += len;
		}
	}
	if (rablkno &&
	    (bfreelist[FREELIST()].b_bcount > 1) &&
	    !incore(dev, rablkno, ralen, 0)) {
		rabp = getblk(dev, rablkno, ralen);
		ASSERT(!rabp->b_vp);
		ASSERT(rabp->b_blkno == rablkno && \
		       rabp->b_bcount == BBTOB(ralen));
		if (rabp->b_flags & B_DONE)
			brelse(rabp);
		else {
#ifdef _SHAREII
			/*
			 *	ShareII potentially charges for BIO activity.
			 */
			SHR_BIO(SH_BIO_BREAD | SH_BIO_ASYNC);
#endif /* _SHAREII */			
			KTOP_UPDATE_CURRENT_INBLOCK(1);
			SYSINFO.bread += ralen;
			rabp->b_flags |= B_READ | B_ASYNC;
			bdstrat(my_bdevsw, rabp);
		}
	}
	if (bp == NULL)
		return (read_buf(dev, blkno, len, 0));

	biowait(bp);
	return (bp);
}

/*
 * Like breada, but only start I/O on the
 * read-ahead block (which is not allocated to the caller)
 */
void
baread(
	buftarg_t *target,
	daddr_t	rablkno,
	int	ralen)
{
	buf_t	*rabp;

	ASSERT((ralen > 0) && (ralen <= BIO_MAXBBS));
	ASSERT(rablkno);
#ifdef BDEBUG
	if (target->dev == Ddev)
		cmn_err(CE_CONT, "!baread: rablk:%d ralen:%d\n",
				rablkno, ralen);
#endif
	if ((bfreelist[FREELIST()].b_bcount > 1) &&
	    !incore(target->dev, rablkno, ralen, 0)) {
		rabp = get_buf(target->dev, rablkno, ralen, BUF_TRYLOCK);
		if (rabp == NULL) {
			return;
		}
		rabp->b_target = target;
		ASSERT(!rabp->b_vp);
		ASSERT(rabp->b_blkno == rablkno &&
		       rabp->b_bcount == BBTOB(ralen));
		if (rabp->b_flags & B_DONE) {
			brelse(rabp);
		} else {
#ifdef _SHAREII
			/*
			 *	ShareII potentially charges for BIO activity.
			 */
			SHR_BIO(SH_BIO_BREAD | SH_BIO_ASYNC);
#endif /* _SHAREII */			
			KTOP_UPDATE_CURRENT_INBLOCK(1);
			SYSINFO.bread += ralen;
			rabp->b_flags |= B_READ | B_ASYNC;
#ifdef CELL_IRIX
			{
				vnode_t *vp;

				vp = target->specvp;
				ASSERT(vp);
				VOP_STRATEGY(vp, rabp);
			}
#else
			{
				struct bdevsw	*my_bdevsw;

				my_bdevsw = target->bdevsw;
				ASSERT(my_bdevsw != NULL);
				bdstrat(my_bdevsw, rabp);
			}
#endif
		}
	}
}

/*
 * See if the block is associated with some buffer.
 * Return a pointer to the buffer (header) if it exists.
 * Lock the buffer first if requested to do so.  If the caller
 * requested a trylock, then do not sleep trying to find or lock
 * the buffer.
 */
buf_t *
incore(
	dev_t		dev,
	daddr_t	blkno,
	int len,
	int lockit)
{
	buf_t *bp;
	buf_t *dp;
	int bytelen = BBTOB(len);

 	ASSERT(len > 0);

	dp = bhash(dev, blkno);
startover:
	if (lockit == INCORE_TRYLOCK) {
		if (!cpsema(&dp->b_lock)) {
			return NULL;
		}
	} else {
		psema(&dp->b_lock, PRIBIO);
	}

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
				if (!cpsema(&bp->b_lock)) {
					return NULL;
				}
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
static int
bp_match(buf_t *bp, int field, void *value)
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
 * Go through all incore buffers, and release buffers
 * if they belong to the given device. This is used in
 * filesystem error handling to preserve the consistency
 * of its metadata.
 */
int
incore_relse(
	dev_t	dev,
	int	delwri_only,
	int	wait)
{
	buf_t	*bp;
	int	count, j;

	count = 0;
	for (bp = &global_buf_table[0], j = 0; j < v.v_buf; bp++, j++) {
		if (bp->b_edev != dev ||
		    (delwri_only && (bp->b_flags & B_DELWRI) == 0))
			continue;
		if (wait)
			psema(&bp->b_lock, PRIBIO);
		else if (!cpsema(&bp->b_lock))
			continue;
		if (bp->b_edev != dev ||
		    (delwri_only && (bp->b_flags & B_DELWRI) == 0) ||
		    (!wait && bp->b_pincount > 0)) {
			vsema(&bp->b_lock);
			continue;
		}
		if (bp->b_pincount > 0)
			bwait_unpin(bp);
		if (bp->b_flags & B_DELWRI) {
			bp->b_flags &= ~(B_DELWRI|B_ASYNC|B_READ);
			if (bp->b_iodone) {
				notavail(bp);
				bp->b_bdstrat = NULL;
				bp->b_target = NULL;
				bp->b_flags &= ~(B_DONE);
				bp->b_flags |= B_STALE|B_ERROR|B_XFS_SHUT;
				bp->b_error = EIO;
				biodone(bp);
			} else {
				ASSERT(bp->b_fsprivate2 == NULL);
				notavail(bp);
				bp->b_flags |= B_STALE;
				brelse(bp);
			}
			count++;
		} else if (!delwri_only) {
			ASSERT(bp->b_fsprivate2 == NULL);
			notavail(bp);
			bp->b_flags |= B_STALE;
			brelse(bp);
		}
	}
	return count;
}		
	
/*
 * Write the buffer, waiting for completion.
 * Then release the buffer.
 */
int
bwrite(buf_t *bp)
{
	uint64_t flag;
	int error;
	struct bdevsw *my_bdevsw;
	extern xfs_inobp_bwcheck(buf_t *);
#ifdef CELL_IRIX
	vnode_t *vp;
#endif

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
	if (!(flag & B_DELWRI))
		SYSINFO.lwrite += BTOBBT(bp->b_bcount);

	bp->b_flags &= ~(B_READ | B_DONE | B_ERROR | B_DELWRI);

	/*
	 * Wait for the buffer to be unpinned if it is currently pinned.
	 */
	if (bp->b_pincount > 0) {
		bwait_unpin(bp);
	}

#ifdef _SHAREII
	/*
	 *	ShareII potentially charges for BIO activity.
	 */
	SHR_BIO(SH_BIO_BWRITE | (((flag & B_ASYNC) == 0) ? 0 : SH_BIO_ASYNC));
#endif /* _SHAREII */
	KTOP_UPDATE_CURRENT_OUBLOCK(1);
	SYSINFO.bwrite += BTOBBT(bp->b_bcount);

	/*
	 * Consistency check XFS inode buffers
	 */
	if (bp->b_bvtype == B_FS_INO)  {
		xfs_inobp_bwcheck(bp);
	}

	if (bp->b_vp) {
		VOP_STRATEGY(bp->b_vp, bp);
	} else if (bp->b_bdstrat) {
		(*bp->b_bdstrat)(bp);
	} else {
		if (bp->b_target) {
#ifdef CELL_IRIX
			vp = bp->b_target->specvp;
			ASSERT(vp);
			VOP_STRATEGY(vp, bp);
#else
			my_bdevsw = bp->b_target->bdevsw;
			ASSERT(my_bdevsw != NULL);
			bdstrat(my_bdevsw, bp);
#endif /* CELL_IRIX */
		} else {
			my_bdevsw = get_bdevsw(bp->b_edev);
			ASSERT(my_bdevsw != NULL);
			bdstrat(my_bdevsw, bp);
		}
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
bdwrite(buf_t *bp)
{
	ASSERT((bp->b_flags & B_BUSY) && (valusema(&bp->b_lock) <= 0));
	ASSERT(bp->b_bufsize > 0 && bp->b_bcount <= bp->b_bufsize);

	SYSINFO.lwrite += BTOBBT(bp->b_bcount);
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
bawrite(buf_t *bp)
{
#ifdef BDEBUG
	if (bp->b_edev == Ndev) {
		bwrite(bp);
		return;
	}
#endif
	if (bfreelist[FREELIST()].b_bcount > 4) {
		bp->b_flags |= B_ASYNC;
	}
	bwrite(bp);
}

/*
 * Increment the pin count of the given buffer.
 * This value is protected by the bpinlock spinlock.
 */
void
bpin(buf_t *bp)
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
bunpin(buf_t *bp)
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
		/*
		 * The count may be 0 if someone else doing a bunpin
		 * just woke everyone on this wait structure up but
		 * the guy we'd wake up hasn't yet gone back to sleep.
		 * He'll see that the b_pincount has gone to 0 and
		 * not bother to sleep again.
		 */
		ASSERT(bwp->bwp_count >= 0);
		count = bwp->bwp_count;
		if (count != 0) {
			bwp->bwp_count = 0;
		}
	}
	mutex_spinunlock(&bpinlock, s);
	if (count > 0) {
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
bwait_unpin(
	buf_t	*bp) 
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

#define bpinact_lock()	mutex_spinlock((lock_t *)&binactivelist.b_start)
#define bpinact_unlock(s)	\
			mutex_spinunlock((lock_t *)&binactivelist.b_start, s)
/*
 * Buffer cache is straining memory limits.  Inactive this buffer
 * for a while.  bdflush will occasionally call bpactivate (indirectly?).
 */
void
bpinactivate(
	buf_t *bp)
{
	buf_t *temp;
	int s;

#ifdef notdef
	cmn_err(CE_CONT, "!bp I 0x%x [%d]\n", bp, binactivelist.b_bcount);
#endif
	ASSERT(bp->b_relse == 0);
	ASSERT((bp->b_flags & B_BUSY) && (valusema(&bp->b_lock) <= 0));
	ASSERT((bp->b_flags & (B_NFS_UNSTABLE|B_NFS_ASYNC)) == 0);

	bp->b_flags = B_INACTIVE;

	s = bpinact_lock();
	temp = binactivelist.av_forw;
	temp->av_back = bp;
	bp->av_forw = temp;
	bp->av_back = &binactivelist;
	binactivelist.av_forw = bp;
	binactivelist.b_bcount++;
	bpinact_unlock(s);

	vsema(&bp->b_lock);

	BUFINFO.inactive++;
}

void
bpactivate(
	pgno_t howmany)
{
	struct buf *forw;
	struct buf *back;
	struct buf *bp;

	while (howmany > 0) {
		int s = bpinact_lock();

		bp = binactivelist.av_forw;
		while (bp != &binactivelist && !cpsema(&bp->b_lock)) {
			bp = bp->av_forw;
		}

		/*
		 * If we can't get a buffer header on the inactive list
		 * without sleeping, bail.
		 */
		if (bp == &binactivelist) {
			bpinact_unlock(s);
			return;
		}

		bp->b_flags = B_BUSY|B_STALE;

		forw = bp->av_forw;
		back = bp->av_back;
		back->av_forw = forw;
		forw->av_back = back;

		if (--binactivelist.b_bcount == 0)
			howmany = 0;
		else
			howmany--;
		bpinact_unlock(s);

#ifdef notdef
		cmn_err(CE_CONT, "!bp A 0x%x [%d]\n",
			bp, binactivelist.b_bcount);
#endif

		brelse(bp);	/* into free list */
		BUFINFO.active++;
	}
}


/*
 * Activate one buffer from the binactivelist.
 */
struct buf *
bpactivate_one(void)
{
        struct buf *forw;
        struct buf *back;
        struct buf *bp;
        int s;

        s = bpinact_lock();

        bp = binactivelist.av_forw;
        while (bp != &binactivelist && !cpsema(&bp->b_lock)) {
                bp = bp->av_forw;
        }

        /*
         * If we can't get a buffer header on the inactive list
         * without sleeping, bail.
         */
        if (bp == &binactivelist) {
                bpinact_unlock(s);
                return NULL;
        }

        bp->b_flags = B_BUSY;

        forw = bp->av_forw;
        back = bp->av_back;
        back->av_forw = forw;
        forw->av_back = back;

        --binactivelist.b_bcount;
        bpinact_unlock(s);

        BUFINFO.active++;
	return bp;
}

/*
 * Release the buffer, with no I/O implied.
 * If there is a 'special action' function specified in the b_relse field
 * of the buffer, this takes precedence.
 */
void
brelse(
	buf_t	*bp)
{
	buf_t	*temp;
	buf_t	*freelist;
	uint64_t flags = bp->b_flags;
	int	s;
	int	listid;

	bp->b_start = lbolt;

#ifdef BDEBUG
	if (bp->b_edev == Ddev) {
		cmn_err(CE_CONT, "!rel:blk:%d len:%d\n",
			bp->b_blkno, bp->b_bufsize);
	}
#endif
	/*
	 * This is an oft-called routine, and it is worth it to
	 * avoid gratuitous memory fetches.
	 */
	if (bp->b_relse && !(flags & B_RELSE)) {
		buftrace("BRELSE SPECIAL", bp);
		(*bp->b_relse)(bp);
		return;
	}
	buftrace("BRELSE", bp);

	ASSERT((bp->b_flags & B_BUSY) && (valusema(&bp->b_lock) <= 0));

	if (flags & B_ERROR) {
		flags &= ~(B_ERROR|B_DELWRI);
		flags |= B_STALE|B_AGE;
		bp->b_error = 0;
	}

	ASSERT((flags & (B_STALE|B_DELWRI)) != (B_STALE|B_DELWRI));
	ASSERT(bp->b_iodone == 0 || (flags & B_DELWRI));

	/*
	 * Place the buffer on the appropriate place in the free list.
	 */
	listid = bp->b_listid;
	freelist = &bfreelist[listid];
	s = bfree_lock(listid);
	if (flags & (B_AGE|B_STALE)) {
		temp = freelist->av_forw;
		temp->av_back = bp;
		bp->av_forw = temp;
		freelist->av_forw = bp;
		bp->av_back = freelist;
	} else {
		temp = freelist->av_back;
		temp->av_forw = bp;
		bp->av_back = temp;
		freelist->av_back = bp;
		bp->av_forw = freelist;
	}
	freelist->b_bcount++;		/* just for fun */
	if (bp->b_bcount == 0) {
		atomicAddInt(&bemptycnt, 1);
		ASSERT(bemptycnt <= v.v_buf);
	}

	ASSERT(spinlock_islocked((lock_t*)&(freelist->b_private)));
	bfreelist_check(freelist);

	/*
	 * If someone waiting for a free buffer, get them going.
	 */
	ASSERT(freelist->b_blkno >= 0);
	if (freelist->b_blkno) {
		freelist->b_blkno--;
		bfree_unlock(listid, s);
		vsema(&(freelist->b_iodonesema));
	} else {
		bfree_unlock(listid, s);
	}

	/*
	 * b_flags finally get local 'flags' bits.
	 */
	bp->b_flags = flags & ~(B_BUSY|B_ASYNC|B_AGE|B_BDFLUSH|B_RELSE|B_HOLD);

	vsema(&bp->b_lock);
}

/*
 * This is just a wrapper for the get_buf() routine.  It is
 * simply a call to get_buf() which will wait on locked buffers.
 */
buf_t *
getblk(
	dev_t	dev,
	daddr_t	blkno,
	int	len)
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
buf_t *
get_buf(
	dev_t		dev,
	daddr_t		blkno,
	int		len,
	uint		flags)
{
	buf_t		*bp;
	buf_t		*dp;	/* hash chain buf will/be on */
	buf_t		*op;	/* hash chain buf WAS on */
	int		bytelen = BBTOB(len);
	int		innerloopcount;
	int		s;
	int		locked;
	int		refed;
	int		backoff = 1;	/* exponential to avoid thrashing */
	int		wait_count = 0;
	int		listid;
	int		first_listid;
	int		next_listid;
	buf_t		*freelist;
	dev_t		pinned;

	ASSERT((len > 0) && (len <= BIO_MAXBBS));
#ifdef BDEBUG
	if (dev == Ddev) {
		cmn_err(CE_CONT, "!gb:blk:%x len:%x\n", blkno, len);
	}
#endif

	refed = 0;
	/* Compute hash bucket address. */
	dp = bhash(dev, blkno);
	BUFINFO.getblks++;
loop:
	BUFINFO.getloops++;
	/* lock hash list */
	psema(&dp->b_lock, PRIBIO);
#ifdef BDEBUG
	if (dev == Ndev) {
		goto loop2;
	}
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
		/*
		 * Got it!
		 * Wait for any i/o in progress.
		 * If the buffer is busy and BUF_TRYLOCK is not set then
		 * release the hash lock, wait for the buffer,
		 * and reacquire the hash lock.
		 * If the buffer is busy and BUF_TRYLOCK is set, then
		 * release the hash lock and return NULL.
		 */
		locked = cpsema(&bp->b_lock);
		if (!locked && !(flags & BUF_TRYLOCK)) {
			BUFINFO.getblockmiss++;
			vsema(&dp->b_lock);
			ADD_SYSWAIT(iowait);
			psema(&bp->b_lock, PRIBIO|PLTWAIT);
			SUB_SYSWAIT(iowait);
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
			BUFINFO.getbchg++;
			goto loop;
		}

		if (bp->b_flags & B_STALE) {
			/* notavailspl below will set B_BUSY */
			if (bp->b_flags & (B_NFS_UNSTABLE|B_NFS_ASYNC))
				atomicAddInt(&b_unstablecnt, -1);
			bp->b_flags = bp->b_bvtype = 0;
			/*
			 * Since we are returning this buffer with the
			 * B_DONE flag cleared, it is very likely that
			 * the caller will read it in from disk.  We need
			 * to flush the contents of the buffer from the
			 * CPU cache on non I/O coherent machines so that
			 * the CPU will see only the data read in from disk.
			 * Normally this is taken care of in bio_realloc(),
			 * but we're not going through there in this path.
			 */
			if (valid_dcache_reasons & CACH_IO_COHERENCY) {
				ASSERT((bp->b_un.b_addr != NULL) &&
				       (bp->b_bufsize > 0) &&
				       (bp->b_bcount > 0));
				data_cache_wbinval(bp->b_un.b_addr,
						   bp->b_bcount,
						   CACH_IO_COHERENCY);
			}
		} else {
			BUFINFO.getfound++;
			bp->b_flags |= B_FOUND;
			ASSERT(bp->b_flags & B_DONE);
		}

		ASSERT(!bp->b_vp);

		/* Lock free list and remove bp. */
		notavail(bp);

		return bp;
	}

#ifdef BDEBUG
	if (bdebug) {
		overlapsearch(dev, blkno, len);
	}
#endif

	/*
	 * The blocks requested are not in the cache.  Allocate a new
	 * buffer header, then perform the random bookkeeping needed to
	 * make the blocks accessible, making sure to use the base hash
	 * header already computed for the blkno being cached.
	 *
	 * We have primary hash locked and freelist locked.  We must hold
	 * the hash locked until the buffer is safely on the hash list.
	 *
	 * We choose a somewhat random free list to search on each call.
	 * This should balance the lock traffic of the cpus on the free
	 * lists.  It should also ensure that the lists are evenly used.
	 */
	BUFINFO.getfree++;
#ifdef BDEBUG
loop2:
#endif
	listid = FREELIST_INC();
	first_listid = listid;
	pinned = NODEV;

nextlistloop:
	freelist = &bfreelist[listid];

samelistloop:
	s = bfree_lock(listid);
	ASSERT_MP(valusema(&dp->b_lock) <= 0);

	if ((bp = freelist->av_forw) == freelist) {
		ASSERT(freelist->b_bcount == 0);
		BUFINFO.getfreeempty++;
		/*
		 * There's nothing left in this list.  If this isn't
		 * the list where we started, then circulate through
		 * the others looking for free buffers.
		 */
		if (listid == bfreelistmask) {
			next_listid = 0;
		} else {
			next_listid = listid + 1;
		}
		if (next_listid == first_listid) {
			/*
			 * Nothing on the freelists. Let's check the
			 * binactivelist.
			 */
			if (binactivelist.b_bcount) {
				bfree_unlock(listid, s);
				bp = bpactivate_one();
				if (bp == NULL) 
					goto samelistloop;
				goto exitloop;
			}

			/*
			 * We've looked through all the free lists and
			 * there is nothing available.  Sleep on the
			 * current list for something to be freed.
			 */
			freelist->b_blkno++;
			bfree_unlock(listid, s);
			vsema(&dp->b_lock);
			psema(&(freelist->b_iodonesema), PRIBIO);
			goto loop;  /* see if the buffer is now in */
		} else {
			bfree_unlock(listid, s);
			listid = next_listid;
			goto nextlistloop;
		}
	}

	innerloopcount = 0;

	/*
	 * Look for one we can get locked.
	 * We don't have to worry about finding a lot of buffers
	 * locked on the free list (and holding off interrupts with
	 * bfreelock held) because very few routines lock buffers
	 * on the free list -- bdflush, bflush, binval -- and they
	 * will each only lock one at a time.
	 */
	while ((bp != freelist) && !cpsema(&bp->b_lock)) {
innerloop:
		bp = bp->av_forw;
	}

	/*
	 * If we can't get a block on the free list without sleeping,
	 * we'll have to start all over again (sigh!).  Delay to
	 * give other processes a chance to unlock their buffers.
	 * Also, potentially try another free list in looking for buffers.
	 */
	if (bp == freelist) {
		BUFINFO.getfreealllck++;
		if (binactivelist.b_bcount) {
			bfree_unlock(listid, s);
			bp = bpactivate_one();
			if (bp == NULL)
				goto pinloop;
			goto exitloop;
		}

startover:
		bfree_unlock(listid, s);
pinloop:
		vsema(&dp->b_lock);
		++wait_count;
		if ((pinned != NODEV) && ((wait_count % 4) == 0)) {
			/* if pinned buffers found, shake 'em loose */
			BUFINFO.force++;
			(void)vfs_force_pinned(pinned);
		} else { 
			delay(backoff);
			backoff *= 2;
			if (backoff > BACKOFF_CAP)
				backoff = 1;
		}
		goto loop;
	}

	/*
	 * If the buffer is still too sticky, put it on the end of
	 * the free list and try another.
	 */
	if (bp->b_ref > 0) {
		if (refed++ > BUF_REF_MAX_LOOK) {
			bp->b_ref = 0;
		} else {
			buf_t *forw = bp->av_forw;
			buf_t *back = bp->av_back;

			back->av_forw = forw;
			forw->av_back = back;

			freelist->av_back->av_forw = bp;
			bp->av_back = freelist->av_back;
			freelist->av_back = bp;
			bp->av_forw = freelist;
			bfree_unlock(listid, s);

			--bp->b_ref;
			vsema(&bp->b_lock);
			BUFINFO.getfreeref++;

			goto samelistloop;
		}
	}

	/*
	 * If the buffer is pinned, behave as if we couldn't lock
	 * it.  We don't want to sleep waiting for it to be unpinned,
	 * because that could be a long time.
	 */
	if (bp->b_pincount > 0) {
		pinned = bp->b_edev;	/* save for possible log force */
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

                if ((bp->b_vp) && (bp->b_vp->v_flag & VDFSCONVERTED)) {
                        buftrace("GETBUF DFS", bp);
			vsema(&bp->b_lock);
                        goto innerloop;
                }
		notavailspl(bp);
		bfree_unlock(listid, s);
		buftrace("GETBUF DELWRI", bp);
		vsema(&dp->b_lock);	/* unlock hash */
		BUFINFO.getfreedelwri++;

		/*
		 * B_AGE ensures that buffer goes to the head of the freelist
		 * on completion so that it doesn't a free ride through the
		 * freelist again; B_ASYNC|B_BDFLUSH ensures that the
		 * transaction will really be asynchronous -- strategy
		 * routines must always queue up a buffer with these
		 * flags set if the routine might have to wait for a resource.
		 */
		bp->b_flags |= B_ASYNC | B_BDFLUSH | B_AGE;
		if (bp->b_vp) {
			ASSERT(bp->b_flags & B_DACCT);
			dchunkunchain(bp);
			clusterwrite(bp, 0);
		} else {
			bwrite(bp); /* eventually freeing bp internally .. */
		}
		goto loop;
	}

	/*
	 * This only applies to NFS V3 client buffers
	 */

	if (bp->b_flags & B_NFS_UNSTABLE) {
		notavailspl(bp);
		bfree_unlock(listid, s);
		buftrace("GETBUF NFS UNSTABLE", bp);
		vsema(&dp->b_lock);     /* unlock hash */

		/*
		 * Call chunkcommit to cluster the UNSTABLE buffers so that
		 * we make fewer trips to the NFS server to do an fsync
		 */
		chunkcommit(bp, 0);	
		BUFINFO.getfreecommit++;
		goto loop;
	}
	ASSERT((bp->b_flags & (B_NFS_UNSTABLE|B_NFS_ASYNC)) == 0);
		
	if (bp->b_relse) {
		notavailspl(bp);
		bfree_unlock(listid, s);
		buftrace("GETBUF RELSE", bp);
		vsema(&dp->b_lock);	/* unlock hash */
		(*bp->b_relse)(bp);
		BUFINFO.getfreerelse++;
		goto loop;
	}

	/*
	 * Find the old hash bucket.  Note that iff bp->b_edev == NODEV,
	 * bp must be hashed on bfreelist and, thus, bhash will
	 * point to an innocent hash buffer (always the next to last
	 * hash header with the current definition of bhash).
	 */
	if (bp->b_edev == (dev_t) NODEV) {
		op = NULL;
	} else {
		op = bhash(bp->b_edev, bp->b_blkno);

		/*
		 * Make sure we can lock old hash list.
		 */
		if (dp != op && !cpsema(&op->b_lock)) {
			/*
			 * Couldn't get old hash.
			 */
			BUFINFO.getfreehmiss++;
			vsema(&bp->b_lock);

			/*
			 * Traversed too many buffers with spinlock held?
			 * Release lock so interrupts can get in,
			 * and start over.
			 */
			if (innerloopcount++ > 8) {
				BUFINFO.getfreehmissx++;
				goto startover;
			}

			/* Otherwise, try the next available buffer. */
			goto innerloop;
		}
	}

	/*
	 * Got one. (Whew!)
	 * Now take it off the freelist.
	 */
	notavailspl(bp);

	ASSERT((bp->b_flags & B_VDBUF) == 0);
	bp->b_flags = B_BUSY;	/* reset all flags (AFTER we can't fail!)  */

	ASSERT(bp->b_relse == 0);
	ASSERT(bp->b_iodone == 0);
	ASSERT(op || bp->b_forw == bp);

	bfree_unlock(listid, s);

	if (op) {
		/* Take off old hash queue. */
		bp->b_back->b_forw = bp->b_forw;
		bp->b_forw->b_back = bp->b_back;
		if (op != dp) {
			vsema(&op->b_lock);
		}
	} else {
		ASSERT(bp->b_forw == bp && bp->b_back == bp);
	}

	/*
	 * N.B.: this code _has_ to be in front of exitloop so we
	 * don't confuse an empty buffer from one being reactivated.
	 */
	if (bp->b_bufsize == 0) {
		/*
		 * This was an empty buffer -- update bemptycnt.
		 */
		ASSERT(bp->b_bcount == 0);
		ASSERT(bemptycnt > 0);
		atomicAddInt(&bemptycnt, -1);
	}

exitloop:
	/* Put on new hash. */
	bp->b_forw = dp->b_forw;
	bp->b_back = dp;
	dp->b_forw->b_back = bp;
	dp->b_forw = bp;

	/*
	 * clear out b_fsprivate3 field
	 */
	bp->b_fsprivate3 = NULL;

	bp->b_edev = dev;
	bp->b_bdstrat = NULL;
	bp->b_target = NULL;
	bp->b_blkno = blkno;
	bp->b_bvtype = 0;
	bp->b_bcount = bytelen;		/* MUST be set up before */
					/* unlocking hash list lock. */
	vsema(&dp->b_lock);		/* Release new hash. */

	dev_chain(bp);

	bio_realloc(bp);

	return bp;
}

/*
 * Get an block, assigned to a particular device.
 */
buf_t *
ngeteblkdev(
	dev_t	dev,
	size_t	len)
{
	buf_t	*bp;
	buf_t	*op;
	int	s;
	int	innerloopcount;
	int	refed;
	int	backoff = 1;		/* exponential to avoid thrashing */
	int	wait_count = 0;
	int	listid;
	int	first_listid;
	int	next_listid;
	buf_t	*freelist;
	dev_t	pinned;

	BUFINFO.getfree++;
	refed = 0;

anylistloop:
	/*
	 * Choose a sort of random free list on which to look for buffers.
	 */
	listid = FREELIST_INC();
	first_listid = listid;
	pinned = NODEV;

nextlistloop:
	freelist = &bfreelist[listid];

samelistloop:
	s = bfree_lock(listid);
	if ((bp = freelist->av_forw) == freelist) {
		ASSERT(freelist->b_bcount == 0);
		BUFINFO.getfreeempty++;
		/*
		 * There's nothing left in this list.  If this isn't
		 * the list where we started, then circulate through
		 * the others looking for free buffers.
		 */
		if (listid == bfreelistmask) {
			next_listid = 0;
		} else {
			next_listid = listid + 1;
		}
		if (next_listid == first_listid) {
			/*
			 * Check the binactivelist for buffers
			 */
			if (binactivelist.b_bcount) {
				bfree_unlock(listid, s);
				bp = bpactivate_one();
				if (bp == NULL)
					goto samelistloop;
				bp->b_flags |= B_AGE;
				goto exitloop;
			}

			/*
			 * We've looked through all the free lists and
			 * there is nothing available.  Sleep on the
			 * current list for something to be freed.
			 */
			freelist->b_blkno++;
			bfree_unlock(listid, s);
			psema(&(freelist->b_iodonesema), PRIBIO);
			goto anylistloop;
		} else {
			bfree_unlock(listid, s);
			listid = next_listid;
			goto nextlistloop;
		}
	}

	innerloopcount = 0;

	/* look for one we can get locked. */
	while ((bp != freelist) && !cpsema(&bp->b_lock)) {
innerloop:
		bp = bp->av_forw;
	}

	/*
	 * Avoid spinning if there is no buffer readily available.
	 * Potentially change lists after we wake up.
	 */
	if (bp == freelist) {
		BUFINFO.getfreealllck++;
		if (binactivelist.b_bcount) {
                        bfree_unlock(listid, s);
			bp = bpactivate_one();
                        if (bp == NULL) 
                                goto pinloop;
			bp->b_flags |= B_AGE;
                        goto exitloop;
                }

startover:
		bfree_unlock(listid, s);
pinloop:
		++wait_count;
		if ((pinned != NODEV) && ((wait_count % 4)) == 0) {
			/* if pinned buffers found, shake 'em loose */
			BUFINFO.force++;
			(void)vfs_force_pinned(pinned);
		} else {
			delay(backoff);
			backoff *= 2;
			if (backoff > BACKOFF_CAP)
				backoff = 1;
		}
		goto anylistloop;
	}

	/*
	 * If the buffer is still too sticky, put it on the end of
	 * the free list and try another.
	 */
	if (bp->b_ref > 0) {
		if (refed++ > BUF_REF_MAX_LOOK) {
			bp->b_ref = 0;
		} else {
			buf_t *forw = bp->av_forw;
			buf_t *back = bp->av_back;

			back->av_forw = forw;
			forw->av_back = back;

			freelist->av_back->av_forw = bp;
			bp->av_back = freelist->av_back;
			freelist->av_back = bp;
			bp->av_forw = freelist;
			bfree_unlock(listid, s);

			--bp->b_ref;
			vsema(&bp->b_lock);

			BUFINFO.getfreeref++;
			goto samelistloop;
		}
	}

	/*
	 * If the buffer is pinned, behave as if we couldn't lock
	 * it.  We don't want to sleep waiting for it to be unpinned,
	 * because that could be a long time.
	 */
	if (bp->b_pincount > 0) {
		pinned = bp->b_edev;	/* save for possible log force */
		vsema(&bp->b_lock);
		goto innerloop;
	}

	/*
	 * Can't unlock freelist until buffer is off/on hash chain.
	 */
	if (bp->b_flags & B_DELWRI) {
                if ((bp->b_vp) && (bp->b_vp->v_flag & VDFSCONVERTED)) {
                        buftrace("NGETEBLKDEV DFS", bp);
			vsema(&bp->b_lock);
			goto innerloop;
                }
		notavailspl(bp);
		bfree_unlock(listid, s);
		BUFINFO.getfreedelwri++;
		/*
		 * B_AGE ensures that the buffer goes to the head
		 * of the freelist upon completion, and doesn't get
		 * a free ride through the freelist; B_ASYNC|B_BDFLUSH
		 * ensures that the transaction will be strictly async.
		 */
		buftrace("NGETEBLKDEV DELWRI", bp);
		bp->b_flags |= B_ASYNC | B_BDFLUSH | B_AGE;
		if (bp->b_vp) {
			ASSERT(bp->b_flags & B_DACCT);
			dchunkunchain(bp);
			clusterwrite(bp, 0);
		} else {
			bwrite(bp);
		}
		goto samelistloop;
	}

	if (bp->b_flags & B_NFS_UNSTABLE) {
		notavailspl(bp);
		bfree_unlock(listid, s);
		buftrace("NGETBLKDEV B_NFS_UNSTABLE", bp);
		chunkcommit(bp, 0);
		BUFINFO.getfreecommit++;
		goto samelistloop;
	}
		
	if (bp->b_relse) {
		notavailspl(bp);
		bfree_unlock(listid, s);
		buftrace("NGETEBLKDEV RELSE", bp);
		/* BUFINFO.getbrelse++; */
		(*bp->b_relse)(bp);
		BUFINFO.getfreerelse++;
		goto samelistloop;
	}

	/*
	 * Take off hash queue if hash lock is available.
	 * If not, continue on.  bp isn't removed from the
	 * free list until after hash lock is acquired so
	 * we don't have to place buffer back on if cpsema fails.
	 */
	if (bp->b_edev == (dev_t)NODEV) {
		op = (buf_t *)0;
	} else {
		op = bhash(bp->b_edev, bp->b_blkno);
		if (!cpsema(&op->b_lock)) {
			BUFINFO.getfreehmiss++;
			vsema(&bp->b_lock);

			/* Traversed too many buffers with spinlock held?
			 * Release lock so interrupts can get in,
			 * and start over.
			 */
			if (innerloopcount++ > 8) {
				BUFINFO.getfreehmissx++;
				goto startover;
			}
			goto innerloop;
		}
	}

	/* Got one. (Whew!) */
	notavailspl(bp);

	if (bp->b_flags & (B_NFS_UNSTABLE|B_NFS_ASYNC))
		atomicAddInt(&b_unstablecnt, -1);

	bp->b_flags = B_BUSY|B_AGE;

	ASSERT(bp->b_relse == 0);
	ASSERT(bp->b_iodone == 0);
	ASSERT(bp->b_error == 0);

	bfree_unlock(listid, s);

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

	/*
	 * N.B.: this code _has_ to be in front of exitloop so we
	 * don't confuse an empty buffer from one being reactivated.
	 */
	if (bp->b_bufsize == 0) {
		/*
		 * This was an empty buffer -- update bemptycnt.
		 */
		ASSERT(bp->b_bcount == 0);
		ASSERT(bemptycnt > 0);
		atomicAddInt(&bemptycnt, -1);
	}

exitloop:
	ASSERT(bp->b_dforw == bp);
	ASSERT(bp->b_offset == 0);
	bp->b_blkno = -1;
	bp->b_bcount = len;
	bp->b_edev = dev;
	bp->b_bdstrat = NULL;
	bp->b_target = NULL;
	bp->b_ref = 0;
	bp->b_bvtype = 0;
	bp->b_resid = 0;
	bp->b_remain = 0;
	bp->b_fsprivate3 = NULL;

	dev_chain(bp);

	bio_realloc(bp);

	return bp;
}

/*
 * Get a block, not assigned to any particular device
 */
buf_t *
ngeteblk(
	size_t	len)
{
	return(ngeteblkdev((dev_t)NODEV, len));
}


buf_t *
getphysbuf(
	dev_t	dev)
{
	return ngeteblkdev(dev, 0);
}

/*
 * Return bp to phys buffer pool.
 */
void
putphysbuf(
	buf_t	*bp)
{
	uint64_t flags = bp->b_flags;

	ASSERT(bp->b_grio_private == NULL);

	if ((flags & B_MAPPED) && IS_KSEG2(bp->b_un.b_addr)) {
		int npgs = btoct(bp->b_bufsize);
		kvfree(bp->b_un.b_addr, npgs);
		atomicAddInt(&bmappedcnt, -npgs);
		bp->b_un.b_addr = (caddr_t)0;
	}

	if (bp->b_flags & (B_NFS_UNSTABLE|B_NFS_ASYNC))
		atomicAddInt(&b_unstablecnt, -1);

	bp->b_flags = B_STALE | B_AGE | B_BUSY;

	dev_unchain(bp);

	bp->b_fsprivate3 = NULL;
	bp->b_bcount = bp->b_bufsize = 0;
	bp->b_relse = 0;
	bp->b_error = 0;
	bp->b_edev = NODEV;
	bp->b_bdstrat = NULL;
	bp->b_target = NULL;
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
biowait(
	buf_t	*bp)
{
	buftrace("IOWAIT", bp);
	ADD_SYSWAIT(iowait);
	ASSERT(bp->b_flags & B_BUSY);
	/*
	 * Only bio buffer headers must be locked.
	 */
	ASSERT(bp < &global_buf_table[0] ||
	       &global_buf_table[v.v_buf] <= bp ||
	       valusema(&bp->b_lock) <= 0);
	ASSERT(valusema(&bp->b_iodonesema) <= 1);

	psema(&bp->b_iodonesema, PRIBIO|TIMER_SHIFT(AS_BIO_WAIT));

	SUB_SYSWAIT(iowait);
	return geterror(bp);
}

/*
 * Mark I/O complete on a buffer, release it if I/O is asynchronous,
 * and wake up anyone waiting for it.
 */
void
biodone(
	buf_t	*bp)
{
	/*
	 * Call guaranteed rate I/O completion routine.
	 */
	if (bp->b_flags & B_GR_ISD) {
		buftrace("IODONE GRIO", bp);
		grio_iodone(bp);
		return;
	}

	/*
	 * If biodone() special handling is asked for,
	 * indirect through the function pointer.
	 */
	if (bp->b_iodone) {
		buftrace("IODONE SPECIAL", bp);
		(*bp->b_iodone)(bp);
		return;
	}

	buftrace("IODONE", bp);
	ASSERT(bp->b_flags & B_BUSY);
	/* only bio buffer headers must be locked */
	ASSERT((bp < &global_buf_table[0] ||
		bp >= &global_buf_table[v.v_buf]) ||\
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
clrbuf(
	buf_t	*bp)
{
	ASSERT(bp->b_un.b_addr && bp->b_bcount);
	bzero(bp->b_un.b_addr, bp->b_bcount);
	bp->b_resid = 0;
}

/*
 * Indiscriminate version of buffer flushing - look for all devices
 */

void
bflushall(
	void)
{
	buf_t	*bp;
	int	j;

	for (bp = &global_buf_table[0], j = v.v_buf; --j >= 0; bp++) {
		if ((bp->b_flags & (B_STALE|B_DELWRI)) == B_DELWRI) {
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

			buftrace("BFLUSH", bp);
			if ((bp->b_flags & (B_STALE|B_DELWRI)) == B_DELWRI) {
				notavail(bp);
				bp->b_flags |= (B_ASYNC|B_BFLUSH);
				bwrite(bp);
			} else {
				vsema(&bp->b_lock);
			}
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

/*
 * Flush buffers on the passed in device hash chain.
 */
void
bflush_hash(
	dhbuf_t	*bdp,
	dev_t	dev)
{
	buf_t	*bp;
	int	s;

	s = mutex_spinlock(&bdp->bd_lock);

restart:

	for (bp = bdp->bd_forw; bp != (buf_t *) bdp; bp = bp->bd_forw) {

		if ((bp->b_edev == dev) &&
		    ((bp->b_flags & (B_STALE|B_DELWRI|B_BFLUSH)) == B_DELWRI)) {

			mutex_spinunlock(&bdp->bd_lock, s);
			psema(&bp->b_lock, PRIBIO);

			if (bp->b_pincount > 0) {
				/*
				 * Don't mess with pinned buffers.  Trying
				 * to flush them here can interfere with
				 * file system locking conventions.
				 */
				vsema(&bp->b_lock);
				s = mutex_spinlock(&bdp->bd_lock);
				if (bp->bd_hash != bdp) goto restart;
				continue;
			}

			buftrace("BFLUSH", bp);
			if ((bp->b_edev == dev) &&
			    ((bp->b_flags & (B_STALE|B_DELWRI)) == B_DELWRI)) {
				notavail(bp);
				bp->b_flags |= (B_ASYNC|B_BFLUSH);
				bwrite(bp);
			} else {
				vsema(&bp->b_lock);
			}
			s = mutex_spinlock(&bdp->bd_lock);
			if (bp->bd_hash != bdp) goto restart;
		}
	}

restart2:

	for (bp = bdp->bd_forw; bp != (buf_t *) bdp; bp = bp->bd_forw) {

		/*
		 * Grab the buffer to ensure that any writes in
		 * progress complete.  Only acquire lock for buffers
		 * marked and pushed out above; this prevents
		 * raw device buffers from being acquired here.
		 */
		if (bp->b_flags & B_BFLUSH) {
			mutex_spinunlock(&bdp->bd_lock, s);
			psema(&bp->b_lock, PRIBIO);
			bp->b_flags &= ~B_BFLUSH;
			vsema(&bp->b_lock);
			s = mutex_spinlock(&bdp->bd_lock);
			if (bp->bd_hash != bdp) goto restart2;
		}
	}

	mutex_spinunlock(&bdp->bd_lock, s);

#ifdef DEBUG
	for (bp = &global_buf_table[0], s = v.v_buf; --s >= 0; bp++) {
		if ((bp->b_edev == dev) && (bp->bd_hash != &nodev_chain)) {
			if (bp->bd_hash != bdhash(dev)) {
				printf("buf %x not on correct chain\n", bp);
			}
			ASSERT(bp->bd_hash == bdhash(dev));
		}
	}
#endif
}


/*
 * Make sure all write-behind blocks on dev
 * (NODEV implies all) are flushed out.
 */
void
bflush(
	dev_t	dev)
{
	dhbuf_t	*bdp;

	BUFINFO.flush++;

	if (dev == NODEV) {
		bflushall();
		return;
	}

	bdp = bdhash(dev);

	bflush_hash(bdp, dev);
	bflush_hash(&nodev_chain, dev);
}


#ifdef DEBUG
/*
 * Assert that there are no delayed write buffered left around for
 * the given device.
 */
void
bflushed(
	dev_t	dev)
{
	buf_t	*bp;
	int	j;

	for (bp = &global_buf_table[0], j = v.v_buf; --j >= 0; bp++) {
		if ((bp->b_edev == dev) || (dev == NODEV)) {
			ASSERT(!(bp->b_flags & B_DELWRI));
			/*
			 * Assert that no buffers are in the process
			 * of being written by making sure that none
			 * of them are locked. However, the problem is that
			 * someone can race and change b_edev, because 
			 * we didn't take the bp lock. If the buffer _is_
			 * locked, then edev must have changed.
			 */
			if (valusema(&(bp->b_lock)) < 1) {
				psema(&bp->b_lock, PRIBIO);
				ASSERT(bp->b_edev != dev);
				vsema(&bp->b_lock);
			}
		}
	}
}
#endif

extern int bdflush_interval;  /* interval at which bdflush runs (10ms ticks) */
timespec_t bdflush_holdoff;   /* time for next vsema(&bdwakeup) if a holdoff exists */
lock_t bdflush_holdoff_lock;  /* protects bdflush_holdoff */
toid_t bdflush_toid;	/* timeout id for bdflush */
sema_t bdwakeup;	/* bdflush waits on this */
sema_t vfswakeup;	/* vfs_sync waits on this */
sema_t pdwakeup;	/* pdflush daemon waits on this */
static void bdflush(void);
void bdflush_wake_proc(void *);
static void vfs_sync(void);
extern void pdflushdaemon(void);

void
bdflushinit(void)
{
	extern int pdflush_pri;
	extern int vfs_sync_pri;
	extern int bdflush_pri;

	/* All these have larger than usual stacks - pdflush in particular
	 * has been seen to get stack overflows, but all of these,
	 * along with xfsd, have a history of stack overflows. vfs_sync
	 * has overflowed from NFS V3.
	 */
	sthread_create("pdflush", 0, 2 * KTHREAD_DEF_STACKSZ, 0, pdflush_pri,
				KT_PS,
				(st_func_t *)pdflushdaemon, 0, 0, 0, 0);
	sthread_create("vfs_sync", 0, 2 * KTHREAD_DEF_STACKSZ, 0, vfs_sync_pri,
				KT_PS,
				(st_func_t *)vfs_sync, 0, 0, 0, 0);
	sthread_create("bdflush", 0, 2 * KTHREAD_DEF_STACKSZ, 0, bdflush_pri,
				KT_PS,
				(st_func_t *)bdflush, 0, 0, 0, 0);

	/* schedule the initial timeout to get bdflush started */
	bdflush_toid = timeout(bdflush_wake_proc, (void *)0, 
				(long)bdflush_interval);
}

void
bdflushearlyinit(void)
{
	/* init bdflush semas */
	initnsema(&bdwakeup, 0, "bdwake");
	initnsema(&pdwakeup, 0, "pdwake");
	initnsema(&vfswakeup, 0, "vfssync");
	spinlock_init(&bdflush_holdoff_lock, "bdflush_holdoff_lock");
	bdflush_holdoff.tv_sec = bdflush_holdoff.tv_nsec = 0;
}

/* ARGSUSED */
void
bdflush_wake_proc(void *arg)
{
	timespec_t thisrun;
	int start_bdflush = 0;
	int s;

	/*
	 * bdflush_wake_proc is called at bdflush_interval.
	 * We want to wake bdflush unless a holdoff has been
	 * set by syssgi(SGI_BDFLUSHCNT).
	 * 
	 * We can't just use the timeout interface to schedule
	 * the vsema(&bdwakeup) due to race conditions with 
	 * the SGI_BDFLUSHCNT code.
	 * 
	 */ 

	nanotime(&thisrun);
	s = mutex_spinlock(&bdflush_holdoff_lock);
	if ((thisrun.tv_sec > bdflush_holdoff.tv_sec) ||
	    ((thisrun.tv_sec == bdflush_holdoff.tv_sec) &&
		(thisrun.tv_nsec >= bdflush_holdoff.tv_nsec))) {
		start_bdflush= 1;
	}
	mutex_spinunlock(&bdflush_holdoff_lock, s);

	if (start_bdflush) {
		/*
		 * If bdflush is busy and is still running when it's time for 
		 * it to run again, we increment the sema so bdflush runs again
		 * right away.  
		 *
		 * However, on a heavily loaded system it is possible for the 
		 * vsema calls to get so far ahead of the psema calls that the 
		 * sema counter wraps. So don't let it grow too large.
		 *
		 * We could use cvsema to avoid this problem, but then bdflush 
		 * would miss getting scheduled for an entire bdflush_holdoff 
		 * interval.   If it's running longer than its interval, it's 
		 * already behind in its work, so use vsema with a limit.
		 *
		 * The value of MAX_PENDING_BD_VSEMA doesn't matter too much,
		 * as long as it's low enough to avoid a wrap, and high enough
		 * that we don't miss one due to a race on valusema().
		 */ 
#define MAX_PENDING_BD_VSEMA	3
		if (valusema(&bdwakeup) < MAX_PENDING_BD_VSEMA) {
			vsema(&bdwakeup);
		}
	}

	bdflush_toid = timeout(bdflush_wake_proc, (void *)0, 
				(long)bdflush_interval);
}
		
/*
 * bdflush -
 *	1) flush delayed writes after time delay
 *
 * XXX Improvements:
 *	1) use a resource sema to permit round-robin of all processes
 *		attempting to help flush pages (init to #processors)
 *		and rather then just cvsema, keep a count and
 *		keep doing round-obin at each age break.
 *	2) use hi/lo water marks so don't thrash ..
 */
static void
bdflush(
	void)
{
	buf_t		*bp;
	buf_t		*bplast;
	static buf_t	*bpstart;
	time_t		age;
	int		j;
	int		delta;
	extern int	bdflushr, nbuf;
	extern int	pdflag;
	extern int	dchunkbufs;
	extern int	chunkpages;
	extern int	dchunkpages;
	extern int	min_file_pages;
	extern int	min_free_pages;
	extern int	chunk_ebbtide;
	extern int	chunk_neaptide;
	extern void	chunk_vflush(void);
	extern void	mem_adjust(void);
	extern void	activate_bufs(void);
	extern int	sptsz;

	bpstart = &global_buf_table[0];
	bplast = &global_buf_table[nbuf];

	for ( ; ; ) {

		BUFINFO.flush++;
		/*
		 * bdflush is now called at (bdflush_interval * 10ms)
		 * intervals, and will look at only nbuf/bdflushr buffers
		 * at a time.
		 * 
		 * Periodically we're updating the amount of global system
		 * free memory.
		 */
		GLOBAL_FREEMEM_UPDATE();

		delta = tune.t_autoup * HZ;
		if (dchunkbufs >= chunk_ebbtide) {
			delta >>= 1;
		}
		if (dchunkbufs >= chunk_neaptide) {
			delta >>= 1;
		}
		age = lbolt - delta;

		for (bp = bpstart, j = nbuf/bdflushr; --j >= 0; bp++) {
			if (bp >= bplast) {
				bp = &global_buf_table[0];
			}

			/*
			 * If buffer is dirty, and it's legit, and its time
			 * has come, push it to disk.
			 *
			 * If the buffer is pinned, bdflush'd sleep trying
			 * to write it out, so punt.
			 *
			 * If can't get lock, ignore.
			 */
			if ((bp->b_flags & B_DELWRI) &&
			    (bp->b_start < age) &&
			     !(bp->b_pincount > 0) &&
			     cpsema(&bp->b_lock)) {
				if (!(bp->b_flags & B_DELWRI) ||
				    bp->b_pincount > 0) {
					vsema(&bp->b_lock);
					continue;
				}

				ASSERT(!(bp->b_flags & (B_BUSY|B_STALE)));
				buftrace("BDFLUSH", bp);
				notavail(bp);
				bp->b_flags |= B_ASYNC|B_BDFLUSH;

				if (bp->b_vp) {
					dchunkunchain(bp);
					clusterwrite(bp, age);
				} else
					bwrite(bp);
			}
		}

		bpstart = bp;

		if (pdflag) {
			cvsema(&pdwakeup);
		}

		if (bmappedcnt > sptsz/2) {
			chunk_vflush();
		}

		if ((chunkpages > min_file_pages || bufmem > min_bufmem)
		    && GLOBAL_FREEMEM() <= min_free_pages)  
			mem_adjust();
		else if (binactivelist.b_bcount)
			activate_bufs();

		psema(&bdwakeup, PZERO);
	}
}

/*
 * Flush superblocks, dirty fs-nodes, every vfs_syncr seconds.
 * Wakup driven from clock.
 */
static void
vfs_sync(
	void)
{
	for ( ; ; ) {

		VHOST_SYNC(SYNC_FSDATA|SYNC_ATTR|SYNC_BDFLUSH|SYNC_NOWAIT);
		psema(&vfswakeup, PZERO);
	}
}

#ifndef DEBUG_BUFTRACE
#define do_binval_hash(bdp, dev)	\
	binval_hash(bdp, dev)
#else
#define do_binval_hash(bdp, dev)	\
	binval_hash(bdp, dev, (inst_t *)__return_address)
#endif /* DEBUG_BUFTRACE */

/*
 * Write all delayed-write buffers and stales all buffers for ``dev''.
 * binval guarantees only that any buffers associated with ``dev'' AT
 * THE TIME BINVAL IS CALLED will be written and removed.
 * The caller must ensure that no activity on that device occurs if it needs
 * to guarantee that no buffers whatsoever exist upon return from binvalfree.
 */
void
binval_hash(
	dhbuf_t	*bdp,
	dev_t	dev
#ifdef DEBUG_BUFTRACE
	, inst_t *ra
#endif /* DEBUG_BUFTRACE */
	)
{
	buf_t	*bp;
	int	s;

	s = mutex_spinlock(&bdp->bd_lock);

restart:

	for (bp = bdp->bd_forw; bp != (buf_t *) bdp; bp = bp->bd_forw) {
#ifdef BINVAL_DEBUG
		/* If someone overwrites b_edev this could happen,
		 * we should dequeue when they do this.
		 */
		if ((bdp != nodev_chain) && (bdhash(bp->b_edev) != bdp)) {
			printf("buffer %x(%d) not in hash %x(%d)\n",
				bp, bp->b_edev,
				bdp, dev);
			debug("binval");
		}
#endif

		if ((bp->b_edev == dev) && !(bp->b_flags & B_STALE)) {
			mutex_spinunlock(&bdp->bd_lock, s);

			psema(&bp->b_lock, PRIBIO);
			if (bp->b_edev != dev) {
				vsema(&bp->b_lock);
				s = mutex_spinlock(&bdp->bd_lock);
				if (bp->bd_hash != bdp) goto restart;
				continue;
			}
			/*
			 * Waiting for pinned buffers could cause us
			 * to hang, so just ignore them.
			 */
			if (bp->b_pincount > 0) {
				vsema(&bp->b_lock);
				s = mutex_spinlock(&bdp->bd_lock);
				if (bp->bd_hash != bdp) goto restart;
				continue;
			}
#ifdef DEBUG_BUFTRACE
			buftrace_enter("BINVAL", bp,
					(inst_t *)ra);
#endif /* DEBUG_BUFTRACE */
			if (bp->b_flags & B_DELWRI) {
				notavail(bp);
				bp->b_flags &= ~(B_DELWRI|B_ASYNC);
				/*
				 * Stale buffer now so we won't have
				 * to reacquire buffer after bwrite
				 * just to stale it.
				 */
				bp->b_flags |= B_STALE;
				ASSERT(bp->b_pincount == 0);
				bwrite(bp);
			} else if (!(bp->b_flags & B_STALE)) {
				bp->b_flags |= B_STALE;
				ASSERT(!(bp->b_flags & B_ASYNC));
				if (bp->b_relse) {
					notavail(bp);
					brelse(bp);
				} else {
					vsema(&bp->b_lock);
				}
			} else {
				vsema(&bp->b_lock);
			}
			s = mutex_spinlock(&bdp->bd_lock);
			if (bp->bd_hash != bdp) goto restart;
		}
	}

	mutex_spinunlock(&bdp->bd_lock, s);
}

void
binval(
	dev_t	dev)
{
	dhbuf_t	*bdp;

	bdp = bdhash(dev);
	do_binval_hash(bdp, dev);	   /* Invalidate hashed buffers */
	do_binval_hash(&nodev_chain, dev); /* Invalidate unhashed buffers */
}

/*
 * Bio buffer memory allocation routines.
 *
 * Frag headers manage power-of-two free-memory buckets, arranged
 * in increasing order of size.  First frag bucket maps only BBSIZE
 * buckets, second bucket 2*BBSIZE, etc, through a page-sized bucket.
 * The last bucket is only a place-holder.  Any time a full page is
 * returned/coalesced, it is returned to the kernel page pool.
 *
 * Individual frags on each frag list are also sorted by memory address
 * (highest address first), both for faster sorting, and to increase the
 * probability that we can coalesce frags.
 */

/*
 * Free up power-of-two-sized chunk of bb's into free frag pool.
 * Coalese with power-of-two buddies, if possible.
 */
static void
_fragfree(
	char	*bp,		/* memory-buffer pointer */
	frag_t	*fragp,		/* frag pool into which to free bp */
	int	dirty)		/* is memory (possibly) cache-dirty? */
{
	frag_t	*fp = (frag_t *)bp;
	frag_t	*buddy;		/* with which to coalesce */
	frag_t	*tfp, **fpp;	/* temp frag pointers */
	int	fragsize = BBTOB(fragp->bbsize);
	int	s;

	ASSERT(((size_t)bp & (fragsize-1)) == 0);

	s = mutex_spinlock(&bfraglock);
	for ( ; ; ) {
		buddy = (frag_t *)((size_t)fp ^ fragsize);
		for (fpp = &fragp->frag ; ; fpp = &tfp->frag) {

			tfp = *fpp;

			if (tfp == buddy) {
				*fpp = tfp->frag;
				dirty |= buddy->dirty;
#ifdef DEBUG
				ASSERT(fragp->count > 0);
				fragp->count--;
				if (fragp->count > fragp->hiwater)
					fragp->hiwater = fragp->count;
#endif
				break;
			}

			/*
			 * Use unsigned comparisons --
			 * want tfp==0 to always compare < fp.
			 */
			if ((size_t)tfp < (size_t)fp) {
				fp->frag = tfp;
				fp->dirty = dirty;
				*fpp = fp;
#ifdef DEBUG
				ASSERT(fragp->count >= 0);
				fragp->count++;
				if (fragp->count > fragp->hiwater)
					fragp->hiwater = fragp->count;
#endif
				mutex_spinunlock(&bfraglock, s);
				return;
			}
		}

		fp = (frag_t *)((size_t)fp & ~fragsize);
		fragp++;

		if (fragp->bbsize == NDPP) {
			mutex_spinunlock(&bfraglock, s);

			kvpffree(fp, 1, 0);
			if (IS_KSEG2(fp))
				atomicAddInt(&bmappedcnt, -1);
			(void)atomicAddInt(&bufmem, -1);
			return;
		}

		fragsize <<= 1;
		ASSERT(((size_t)fp & (fragsize-1)) == 0);
	}
}

static void
fragfree(
	char	*ptr,
	int	tsize,
	int	dirty)
{
	frag_t	*fragp = fraglist;

	ASSERT(tsize > 0 && tsize < NDPP);

	while (tsize) {
		ASSERT(fragp->bbsize < NDPP);
		if (tsize & 1) {
			_fragfree(ptr, fragp, dirty);
			ptr += BBTOB(fragp->bbsize);
		}
		tsize >>= 1;
		fragp++;
	}
}

/*
 * Allocate len bb's worth of physical memory for the given buffer.
 * The buffer region is locked on entry.
 * The size of the currently-allocated buffer (in bytes) is bp->b_bufsize.
 * The size to the allocated (in bytes) is bp->b_bcount.
 */
static void
bio_realloc(
	buf_t	*bp)
{
	int	newlen;
	void	*ptr;
	frag_t	*fragp;
	int	s;

	ASSERT((bp->b_flags & B_BUSY) && (valusema(&bp->b_lock) <= 0));

	newlen = BTOBB(bp->b_bcount);
	if (bp->b_bufsize == bp->b_bcount) {
		if (newlen == 0)
			bp->b_un.b_addr = 0;
		else
		if (valid_dcache_reasons & CACH_IO_COHERENCY)
			data_cache_wbinval(bp->b_un.b_addr,
					   bp->b_bcount,
					   CACH_IO_COHERENCY);
		return;
	}

	if (bp->b_bufsize) {
		bio_freemem(bp);
	}

	if ((bp->b_bufsize = BBTOB(newlen)) == 0) {
		bp->b_un.b_addr = 0;
		return;
	}

	if (newlen > NDPP/2
#ifdef MH_R10000_SPECULATION_WAR
	    ||
	    IS_R10000()
#endif /* MH_R10000_SPECULATION_WAR */
	    ) {
		/*
		 * XXX	Should really set b_bufsize to actual length
		 * XXX	allocated, and check whether caller matches
		 * XXX	page-wise (for those requesting > NDPP/2)
		 * XXX	before calling bio_freemem.
		 */
		newlen = dtop(newlen);
		bp->b_un.b_addr = kvpalloc(newlen, VM_BULKDATA, 0);
		ASSERT(bp->b_un.b_addr);
		if (IS_KSEG2(bp->b_un.b_addr))
			atomicAddInt(&bmappedcnt, newlen);
		(void)atomicAddInt(&bufmem, newlen);
		return;
	}

	s = mutex_spinlock(&bfraglock);
	for (fragp = fraglist; ; fragp++) {
		if (fragp->bbsize < newlen)
			continue;
		if (fragp->bbsize == NDPP)
			break;
		if ((ptr = (void *)fragp->frag) == NULL)
			continue;

		fragp->frag = ((frag_t *)ptr)->frag;
#ifdef DEBUG
		ASSERT(fragp->count > 0);
		fragp->count--;
#endif
		mutex_spinunlock(&bfraglock, s);
		bp->b_un.b_addr = (char *)ptr;
		if ((s = fragp->bbsize - newlen) > 0)
			fragfree((char *)ptr + BBTOB(newlen), s,
				 ((frag_t *)ptr)->dirty);

		if (valid_dcache_reasons & CACH_IO_COHERENCY) {
			data_cache_wbinval(ptr, ((frag_t *)ptr)->dirty ?
					bp->b_bcount : sizeof(frag_t),
					CACH_IO_COHERENCY);
		}
		return;
	}

	mutex_spinunlock(&bfraglock, s);

	(void)atomicAddInt(&bufmem, 1);
	/*
	 * No frag of ''newlen'' size available on the free frag list.
	 * Allocate a new page.
	 */
	bp->b_un.b_addr = ptr = kvpalloc(1, VM_BULKDATA, 0);
	ASSERT(bp->b_un.b_addr);
	if (IS_KSEG2(ptr)) {
		atomicAddInt(&bmappedcnt, 1);
	}

	fragfree((char *)ptr + BBTOB(newlen), NDPP - newlen, 0);
}

/*
 * Free up the memory space consumed by the given buffer
 * Assumes bp is locked (BUSY) on entry
 */
void
bio_freemem(
	buf_t	*bp)
{
	int	olen = BTOBBT(bp->b_bufsize);
	char	*ptr = bp->b_un.b_addr;
	frag_t	*fragp;

	ASSERT((bp->b_flags & B_BUSY) && (valusema(&bp->b_lock) <= 0));
	ASSERT(olen == BTOBB(bp->b_bufsize));
	ASSERT(ptr);

	bp->b_bufsize = 0;

	if (olen > NDPP/2
#ifdef MH_R10000_SPECULATION_WAR
	    ||
	    IS_R10000()
#endif /* MH_R10000_SPECULATION_WAR */
	    ) {
		ASSERT(((__psunsigned_t)ptr & (NBPP-1)) == 0);
		olen = dtop(olen);
		kvpffree(ptr, olen, 0);
		if (IS_KSEG2(ptr))
			atomicAddInt(&bmappedcnt, -olen);
		ASSERT(bufmem > 0);
		(void)atomicAddInt(&bufmem, -olen);
		return;
	}

	ptr += BBTOB(olen);
	fragp = fraglist;

	while (olen) {
		ASSERT(fragp->bbsize < NDPP);
		if (olen & 1) {
			ptr -= BBTOB(fragp->bbsize);
			_fragfree(ptr, fragp, 1);
		}
		olen >>= 1;
		fragp++;
	}
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
overlapsearch(
	dev_t	dev,
	daddr_t	blkno,
	int	len)
{
	buf_t	*bp;
	buf_t	*tp;
	daddr_t endblkno;
	int	s;
	int	listid;
	int	i;

	endblkno = blkno + len;

	/*
	 * Acquire all the free list locks to freeze up the creation
	 * and deletion of buffers.  We can't use the hash list locks
	 * because we may already be holding one.
	 */
	for (listid = 0; listid < (bfreelistmask + 1); listid++) {
		if (listid == 0) {
			s = bfree_lock(listid);
		} else {
			nested_bfree_lock(listid);
		}
	}

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
			cmn_err_tag(145,CE_PANIC, "buffer 0x%x overlaps dev 0x%x blkno 0x%x len 0x%x global_buf_hash[0x%x]", bp, dev, blkno, len, i);
		}
	}
	for (listid = bfreelistmask; listid >= 0; listid--) {
		if (listid == 0) {
			bfree_unlock(listid, s);
		} else {
			nested_bfree_unlock(listid);
		}
	}
}
#endif

/*
 * bioerror
 *  Set or clear the error flag and error field in a buffer header.
 */
void
bioerror(
	buf_t	*bp,
	int	errno)
{
	bp->b_error = errno;
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
getrbuf(
	int	sleep)
{
	buf_t	*bp = kmem_zone_zalloc(buf_zone, sleep);

        if (bp == NULL) {
                return (NULL);
        }
        initnsema(&bp->b_lock, 1, "b_lock");
	psema(&bp->b_lock, PRIBIO);
        initnsema(&bp->b_iodonesema, 0, "b_iodonesema");
	bp->b_edev = NODEV; 
#ifdef didn_t_use_zalloc
	bp->b_target = NULL;
        bp->b_un.b_addr = NULL; 
        bp->b_bcount    = 0; 
	bp->b_relse = NULL;
	bp->b_iodone = NULL;
	bp->b_fsprivate3 = NULL;
#endif
	bp->av_forw = bp->av_back = bp;
	bp->b_forw = bp->b_back = bp;
        bp->b_flags = B_BUSY;
#ifdef DEBUG_BUFTRACE
	bp->b_trace = ktrace_alloc(BUF_TRACE_SIZE, sleep);
#endif
	return bp;
}

/*
 * Allocate a buffer header with the specified amount
 * of attached memory.  The units of the len parameter
 * are bytes.
 *
 * This routine may sleep allocating memory.
 */
buf_t *
ngetrbuf(
	size_t	len)
{
	buf_t	*bp;

	bp = getrbuf(0);
	ASSERT(bp != NULL);
	bp->b_bcount = len;
	bio_realloc(bp);
	return bp;
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
freerbuf(
	buf_t	*bp)
{
	freesema(&bp->b_iodonesema);
	freesema(&bp->b_lock);
#ifdef DEBUG_BUFTRACE
	ktrace_free(bp->b_trace);
#endif
	kmem_zone_free(buf_zone, bp);
}

/*
 * Free up space allocated by ngetrbuf().
 */
void
nfreerbuf(
	buf_t	*bp)
{
	freesema(&bp->b_iodonesema);
	bio_freemem(bp);
	freesema(&bp->b_lock);
#ifdef DEBUG_BUFTRACE
	ktrace_free(bp->b_trace);
#endif
        kmem_zone_free(buf_zone, bp);
}

void
reinitrbuf(
	buf_t	*bp)
{
        if (bp == NULL) {
		return;
	}

	freesema(&bp->b_iodonesema);
	freesema(&bp->b_lock);
#ifdef DEBUG_BUFTRACE
	ktrace_free(bp->b_trace);
#endif
	bzero(bp, sizeof(*bp));
        initnsema(&bp->b_lock, 1, "b_lock");
	psema(&bp->b_lock, PRIBIO);
        initnsema(&bp->b_iodonesema, 0, "b_iodonesema");
	bp->b_edev = NODEV; 
#ifdef didn_t_bzero
	bp->b_target = NULL;
        bp->b_un.b_addr = NULL; 
        bp->b_bcount    = 0; 
	bp->b_relse = NULL;
	bp->b_iodone = NULL;
	bp->b_fsprivate3 = NULL;
#endif
	bp->av_forw = bp->av_back = bp;
	bp->b_forw = bp->b_back = bp;
        bp->b_flags = B_BUSY;
#ifdef DEBUG_BUFTRACE
	bp->b_trace = ktrace_alloc(BUF_TRACE_SIZE, 0);
#endif
}

void
iodone(
	buf_t	*bp)
{
	biodone(bp);
}

int
iowait(
	buf_t	*bp)
{
	return biowait(bp);
}


/*
 * Pick up the device's error number and pass it to the user;
 * if there is an error but the number is 0 set a generalized code
 */
int
geterror(
	buf_t	*bp)
{
        int     error;

	error = 0;
	if (bp->b_flags & B_ERROR) {
		error = bp->b_error;
		if (!error) {
			error = EIO;
		}
	}
	return error;
}
/*
 * Get an empty 1024 byte block, not assigned to any particular device.
 */
buf_t *
geteblk(
	void)
{
	return ngeteblk(1024);
}


/*
 * DDI/DKI buffer mapping routine.
 *
 * Map physical memory associated with buffer header
 * (that is linked through b_pages list) to kernel virtual space.
 */
#ifdef _VCE_AVOIDANCE
extern int bp_color_validation(pfd_t *);
#endif /* _VCE_AVOIDANCE */

char *
bp_mapin(
	buf_t	*bp)
{
	pfd_t	*pfd;
	pde_t	*pde;
	int	npgs;
	pgi_t	pdebits;
#ifdef _VCE_AVOIDANCE
	int	color;
	int 	color_flags = 0;
#endif /* _VCE_AVOIDANCE */


	if (BP_ISMAPPED(bp)) {
		buftrace("BP_MAPIN MAPPED", bp);
		return bp->b_un.b_addr;
	}

	bp->b_flags |= B_MAPPED;

	pfd = bp->b_pages;
	ASSERT(pfd);

	npgs = btoct(bp->b_bufsize);

#ifdef _VCE_AVOIDANCE
	if (vce_avoidance) {
		if ((bp->b_flags & B_SWAP) == 0)
			color = vcache_color(dtopt(bp->b_offset));
		else
			/* swap to/from file, use current page vcolor */
			color = pfd_to_vcolor(pfd);
		color_flags = VM_VACOLOR;
	} else
		color = 0;
#endif /* _VCE_AVOIDANCE */
#ifdef MH_R10000_SPECULATION_WAR
	if (IS_R10000())
		color_flags |= VM_DMA_RW;
#endif /* MH_R10000_SPECULATION_WAR */

	if (npgs == 1) {
		buftrace("BP_MAPIN 1PAGE", bp);
#ifdef _VCE_AVOIDANCE
		if (vce_avoidance && ! (bp->b_flags & B_UNCACHED))
			bp_color_validation(pfd);
#endif /* _VCE_AVOIDANCE */
		bp->b_un.b_addr = page_mapin(pfd,
					     ((bp->b_flags & B_UNCACHED)
						   ? VM_UNCACHED : 0)
#ifdef _VCE_AVOIDANCE
					     | color_flags, color
#else /* _VCE_AVOIDANCE */
					     , 0
#endif /* _VCE_AVOIDANCE */
					     );
		if (IS_KSEG2(bp->b_un.b_addr))
			atomicAddInt(&bmappedcnt, 1);
	} else {
#ifdef _VCE_AVOIDANCE
		if (vce_avoidance) {
			bp->b_un.b_addr = kvalloc(npgs, 
						  color_flags,
						  color);
		} else
#endif /* _VCE_AVOIDANCE */
		bp->b_un.b_addr = kvalloc(npgs, 0, 0);
		pde = kvtokptbl(bp->b_un.b_addr);
		atomicAddInt(&bmappedcnt, npgs);

		if (bp->b_flags & B_UNCACHED)
			pdebits = PG_VR|PG_G|PG_M|PG_NDREF|PG_SV|PG_N;
		else {
			pdebits =
				PG_VR|PG_G|PG_M|PG_NDREF|PG_SV|pte_cachebits();
#ifdef MH_R10000_SPECULATION_WAR
			if (IS_R10000())
				pdebits &= ~PG_VR;
#endif /* MH_R10000_SPECULATION_WAR */
		}
		while (--npgs >= 0) {
#ifdef _VCE_AVOIDANCE
			if (vce_avoidance && !(bp->b_flags & B_UNCACHED))
				bp_color_validation(pfd);
#endif /* _VCE_AVOIDANCE */
			pg_setpgi(pde, mkpde(pdebits,pfdattopfn(pfd)));
#ifdef MH_R10000_SPECULATION_WAR
			if (! pg_isnoncache(pde) &&
			    IS_R10000())
				krpf_add_reference(pfd,
						   kvirptetovfn(pde));
#endif /* MH_R10000_SPECULATION_WAR */
			pfd = pfd->pf_pchain;
			pde++;
		}
	}

	if (npgs = dpoff(bp->b_offset))
		bp->b_un.b_addr += BBTOB(npgs);

	buftrace("BP_MAPIN NPAGES", bp);
	return(bp->b_un.b_addr);
}

/*
 * Release kernel virtual memory used to map
 * physical memory to buffer header; physical
 * memory remains associated with buffer,
 * linked through b_pages list.
 */
void
bp_mapout(
	buf_t	*bp)
{
	if (bp->b_flags & B_MAPPED) {
		buftrace("BP_MAPOUT UNMAPPED", bp);
		if (IS_KSEG2(bp->b_un.b_addr)) {
			int npgs = btoct(bp->b_bufsize);
			atomicAddInt(&bmappedcnt, -npgs);
			kvfree(bp->b_un.b_addr, npgs);
		}
		bp->b_un.b_addr = (caddr_t)0;
		bp->b_flags &= ~B_MAPPED;
	} else {
		buftrace("BP_MAPOUT UNNEEDED", bp);
	}
}

/*
 * B_PAGEIO routine to return next page of buffer.
 */
pfd_t *
getnextpg(
	buf_t	*bp,
	pfd_t	*pfd)
{
	if (pfd) {
		return pfd->pf_pchain;
	} else {
		return bp->b_pages;
	}
}

#include <sys/dnlc.h>

typedef struct bp_info {
	struct bp_info *	bi_prev;
	struct bp_info *	bi_next;
	vnode_t	*		bi_vp;
	vnumber_t		bi_vnumber;
	short			bi_fstype;
	char			bi_name[NC_NAMLEN+1];
} bpinfo_t;

/*
 * Free bip structs.
 * The first bip struct in each page allocated is used to link the
 * pages of bip structures, via bi_prev.
 */
void
put_bips(bpinfo_t **biphead)
{
	bpinfo_t *bip = *biphead;
	bpinfo_t *tbip;

	while (bip) {
		tbip = bip->bi_prev;
		kvpffree(bip, 1, 0);
		bip = tbip;
	}
}

/*
 * Get a bip structure.
 * These structures get allocated a page at a time, then freed when
 * the call to getbuf_stats finishes.
 * The first entry on each page is kept around to link the pages, via bi_prev.
 * Allocatable bips are linked through bi_next.
 */
bpinfo_t *
get_bip(bpinfo_t **biphead)
{
	bpinfo_t *bip, *tbip;

	if (!(tbip = *biphead) || !(bip = tbip->bi_next)) {
		tbip = kvpalloc(1, KM_SLEEP, 0);
		bip = tbip + 1;
		tbip->bi_next = tbip + 2;
		ASSERT(pbase(tbip) == pbase(tbip+2));
		tbip->bi_prev = *biphead;
		*biphead = tbip;
	} else {
		/*
		 * We use 'bip+2' because we need to know that _all_
		 * of bip+1 fits in this page.
		 */
		if (pbase(bip+2) == pbase(tbip))
			tbip->bi_next = bip+1;
		else
			tbip->bi_next = NULL;
	}

	return bip;
}

void
get_vpinfo(struct vnode *vp, bpinfo_t *bip)
{
	extern void dnlc_vname(vnode_t *, char *);

	dnlc_vname(vp, bip->bi_name);
	bip->bi_vnumber = vp->v_number;
	bip->bi_fstype = vp->v_vfsp ? vp->v_vfsp->vfs_fstype : 0;
}

/*
 * We'll allocate about a quarter page of binfos on the stack.
 */
#define NKBINFOS	((NBPP/4)/sizeof(struct binfo))

/*
 * Gather buffer stats for syssgi(SGI_BUFINFO) call.
 *
 * first_buf:	index into buffer array
 * n:		number of buffers to report
 * ubase:	user binfos
 * howmany:	[indirectly] return # of binfos
 */
int
getbuf_stats(int first_buf, int n, caddr_t ubase, __int64_t *howmany)
{
	binfo_t binfos[NKBINFOS];
	bpinfo_t *bi_head = NULL;	/* name tree */
	bpinfo_t *bi_pages = NULL;	/* pages allocated for name tree */
	buf_t *bp;
	extern int nbuf;

	if ((first_buf < 0) || (first_buf >= nbuf) || (n < 1))
		return EINVAL;
	if (first_buf + n > nbuf)
		n = nbuf - first_buf;

	*howmany = n;

	bp = global_buf_table + first_buf;
	while (n) {
		int i = MIN(n, NKBINFOS);
		volatile binfo_t *binfop = &binfos[0];
		vnode_t *vp;
		bpinfo_t *bip, **bipp;
		bpinfo_t *tbip = NULL;
		buf_t *is_locked;

		/*
		 * We allocate kernel binfos on the stack.
		 * Iterate through till we've pushed out
		 * all the binfos requested.
		 */
		while (i--) {
			is_locked = NULL;
	retry:
			binfop->binfo_size = bp->b_bcount;
			binfop->binfo_flags = bp->b_flags;
			binfop->binfo_bvtype = bp->b_bvtype;
			binfop->binfo_fpass = bp->b_ref;
			binfop->binfo_dev = bp->b_edev;
			binfop->binfo_start = bp->b_start;

			/*
			 * If the buffer maps file data, we'll want to
			 * find the vp's vnumber, and the corresponding
			 * file system name via the dnlc.  In order to
			 * avoid locking the same vp again and again,
			 * we'll build a tree of the vnodes already
			 * interrogated and cache the info.  The program(s)
			 * that calls this routine usually requests info
			 * about _all_ the buffers, so this seems like a
			 * useful strategy.
			 */
			if ((bp->b_flags & (B_PAGEIO|B_STALE)) == B_PAGEIO) {

				binfop->binfo_offset = bp->b_offset;

				/*
				 * Find the vnumber of bp's vnode, and its
				 * name.  We keep a cache here of all vnodes
				 * we've seen in the scan, so we don't have
				 * to lock the buffer or go traipsing through
				 * the dnlc for every buffer that maps data.
				 */
				if (!(vp = bp->b_vp))
					goto retry;

				bipp = &bi_head;	/* head of bip tree */
				bip = bi_head;
		next:
				/*
				 * No match?
				 * Get bp_info struct and fill it in.
				 */
				if (!bip) {
					/*
					 * Get a bip entry.
					 */
					if (tbip) {
						bip = tbip;
						tbip = NULL;
					} else
						bip = get_bip(&bi_pages);

					bip->bi_next =
					bip->bi_prev = NULL;
					bip->bi_vp = vp;

					/*
					 * Ok -- we've got a candidate.  Fill
					 * in the bip and hang it on our tree.
					 * If we can't lock the bp, we'll
					 * fill in bip with whatever we have,
					 * and touch it up late.
					 */
					if (!is_locked && cpsema(&bp->b_lock))
						is_locked = bp;

					if (is_locked) {
						if (vp != bp->b_vp) {
							tbip = bip;
							goto retry;
						}
						get_vpinfo(vp, bip);
					} else {
						/*
						 * If we can't lock this buffer
						 * without sleeping, fake up
						 * the name and vnumber and
						 * press on.
						 */
						strcpy(bip->bi_name, "??");
						bip->bi_vnumber = 0;
						bip->bi_fstype = 0;
					}

					*bipp = bip;

					goto fill_it_in;
				}

				/*
				 * Traverse tree.
				 */
				if (bip->bi_vp > vp) {
					bipp = &bip->bi_prev;
					bip = bip->bi_prev;
					goto next;
				}
				if (bip->bi_vp < vp) {
					bipp = &bip->bi_next;
					bip = bip->bi_next;
					goto next;
				}

				/*
				 * Found a match -- copy cached info.
				 */
		fill_it_in:
				/*
				 * If we couldn't lock the buffer when
				 * bip was created, its v_number is 0 --
				 * try to get its real name and vnumber now.
				 */
				if ((bip->bi_vnumber == 0) &&
				    cpsema(&bp->b_lock)) {
					ASSERT(is_locked == NULL);
					is_locked = bp;
					if (vp != bp->b_vp)
						goto retry;
					get_vpinfo(vp, bip);
				}

				binfop->binfo_vnumber = bip->bi_vnumber;
				binfop->binfo_vp = (uint64_t)bip->bi_vp;
				binfop->binfo_fstype = bip->bi_fstype;
				strcpy((char *)binfop->binfo_name,
					bip->bi_name);

			} else /* !B_PAGEIO */ {
				if (binfop->binfo_flags & B_PAGEIO)
					binfop->binfo_flags &= ~B_PAGEIO;
				binfop->binfo_blkno = bp->b_blkno;
			}

			/*
			 * Check that buffer data didn't change mid-stream.
			 */
			if (binfop->binfo_flags != bp->b_flags ||
			    binfop->binfo_dev != bp->b_edev ||
			    binfop->binfo_size != bp->b_bcount)
				goto retry;

			if (is_locked) {
				ASSERT(is_locked == bp);
				vsema(&is_locked->b_lock);
			}

			bp++;
			binfop++;
		}

		i = MIN(n, NKBINFOS);

		if (copyout(binfos, ubase, i * sizeof(binfo_t))) {
			put_bips(&bi_pages);
			return EFAULT;
		}

		n -= i;
		ubase += i * sizeof(binfo_t);
	}

	/*
	 * Deallocate bip structures.
	 */
	put_bips(&bi_pages);
	return 0;
}
