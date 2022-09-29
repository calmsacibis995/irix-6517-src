/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.295 $"

#include <limits.h>
#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/immu.h>
#include <sys/kmem.h>
#include <sys/kthread.h>
#include <sys/param.h>
#include <sys/pfdat.h>
#include <sys/page.h>
#include <sys/proc.h>
#include <sys/sbd.h>
#include <sys/sema.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/tuneable.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <sys/vnode_private.h>
#include <sys/ktrace.h>
#include <sys/numa.h>
#include <ksys/vhost.h>
#include <ksys/vm_pool.h>
#include <sys/nodepda.h>

#if CELL_IRIX 			/* TEMP til page cache distributed */
#include <fs/cfs/dvn.h>
#endif

struct cred;
static void	delalloc_insert(buf_t **, buf_t	*);
static void	delalloc_delete(buf_t **, buf_t	*);
static buf_t *	delalloc_lock_one(buf_t	**, unsigned int);
static void	delalloc_reserve(void);
static void	delalloc_found(buf_t *, uint);
#ifdef DEBUG_BUFTRACE
static void	delalloc_check(buf_t *, unsigned int);
#else
#define	delalloc_check(b,d)
#endif

static void	chunkreada(vnode_t *, struct bmapval *, struct cred *);
static buf_t	*fetch_chunk(vnode_t *, struct bmapval *, struct cred *);
static buf_t	*gather_chunk(vnode_t *, struct bmapval *, int *,
			      size_t *, size_t *);
static buf_t	*findchunk(vnode_t *, off_t, int);
static void	chunkunhash(buf_t *);
static void	notavailchunk(buf_t *);
static void	chunk_patch(buf_t *);
static void	cread(buf_t *, struct bmapval *);
static int	pdcluster(buf_t *, uint);
static int	chunk_pshake(int);
static int	chunk_vshake(int);
static void	do_pdflush(vnode_t *, int, uint64_t);

void	chunkhold(buf_t *);
void	chunkrelse(buf_t *);
void	chunkinactivate(buf_t *);
void	chunkcommit(buf_t *, int);
void    chunkfree(int);

static struct buf *bphand;	/* buffer pointer for chunk_pshake */
static struct buf *bpfhand;	/* buffer pointer for free_buffer_mem */
static struct buf *bvhand;	/* buffer pointer for chunk_vflush */
static struct buf *bvshand;	/* buffer pointer for chunk_vshake */
static struct buf **brhand;     /* buffer pointer for chunkfree */
static struct buf *bplast;	/* buffer pointer to the last buffer */ 
static mutex_t	chunk_vshake_lock;	/* chunk_vshake semaphore */

#define del_wait(s)	sv_wait(&delalloc_wait, PRIBIO, &delalloc_wlock, s)
#define del_wake()	sv_signal(&delalloc_wait)

/*
 * The following three variables are allocated together because bdflush
 * examines each of them, consecutively, every second -- might as well
 * get some cache hits.  Same for dchunkbufs and chunk_{ebb,neap}tide.
 */
int		bmappedcnt;	/* # of kernel virtual pages K2SEG mapped */
int		pdcount;	/* count of pdinserted pages */
int		pdflag;		/* set non-zero when adding page to delayed write queue */

static lock_t	chunk_plock;	/* lock for synchronizing between chunk_patch
				 * and patch_asyncrelse */
#if MP
#pragma fill_symbol (chunk_plock, 128)
#endif

static lock_t	delalloc_clock; /* lock on the delalloc_clean list */
#if MP
#pragma fill_symbol (delalloc_clock, 128)
#endif

static lock_t	delalloc_dlock;	/* lock on the delalloc_dirty list */
#if MP
#pragma fill_symbol (delalloc_dlock, 128)
#endif

static lock_t	delalloc_wlock;	/* lock protecting delalloc_waiters */
#if MP
#pragma fill_symbol (delalloc_wlock, 128)
#endif

int		chunkpages;	/* total # of pages mapped to chunk buffers */
int		dchunkpages;	/* total # of delwri pages */
int		dchunkbufs;	/* total # of delwri chunk buffers */

#define CHUNK_AVG_SAMPLES	64	/* # of chunk size samples */
int	chunk_avg_page_size_sum; /* calculated avg buffer chunk size */
#define chunk_avg_page_size()	(chunk_avg_page_size_sum / CHUNK_AVG_SAMPLES)

int	chunk_ebbtide;	/* low-water mark for decommissioning buffers */
int	chunk_neaptide;	/* hi-water mark for decommissioning buffers */

extern int	min_file_pages;	/* min memory to keep when shaking */
extern int	min_free_pages;	/* min free memory to attain when shaking */
extern int	cwcluster;	/* max number of pgs to cluster in each push */
extern int	nbuf;		/* number of buffers configured */
extern int	min_bufmem;	/* min metadata memory to keep when shaking */

hotIntCounter_t	delallocleft;	/* total # of delalloc chunk buffers */
				/* remaining to be allocated */
/*
 * delallocleft is an extremely hot counter - put it in a cacheline
 * by itself.
 */
#if MP
#pragma fill_symbol (delallocleft, L2cacheline)
#endif

volatile int	b_unstablecnt;	/* # of buffers that cannot be freed until
				 * another action (eg. commit) is performed */

#ifdef DEBUG_BUFTRACE 
int	delalloc_dbufs;
int	delalloc_cbufs;
#endif

int	delalloc_waiters; /* num procs waiting for delalloc bufs */
static buf_t	*delalloc_clean; /* list of clean, free, delalloc bufs */
static buf_t	*delalloc_dirty; /* list of dirty, free, delalloc bufs */
static sv_t	delalloc_wait;	/* sv_t to wait on for delalloc bufs */

#define CERT(EX)

#define AVL_BACK	1
#define AVL_BALANCE	0
#define AVL_FORW	2

/*
 * Flags for delalloc_lock_one().
 */
#define	DELALLOC_DELETE	0x1

/*
 * Flags for pdcluster().
 */
#define	PDC_EOF		0x1

/*
 * Flags returned by gather_chunk().
 */
#define	GC_PDCLUSTER		0x01

/*
 * Flags used internally by gather_chunk() but possibly in the
 * same flags word as the return flags.
 */
#define	GC_SOMEDONE		0x02
#define	GC_SOMEUNDONE		0x04
#define	GC_DELWRI		0x08
#define	GC_LPAGE_COLLISION	0x10
#define GC_FIRST_PAGEFREED	0x20
#define	GC_LAST_PAGEFREED	0x40
	
#ifdef DEBUG_BUFTRACE
ktrace_t	*buf_trace_buf;
#endif

#define	LPAGE_ALIGN(off, page_size) \
		((off_t)(off) & ~(off_t)((page_size) - 1))
#define	LPAGE_ALIGN_UP(off, page_size) \
		((off_t)((off) + ((page_size) - 1)) & \
		 ~(off_t)((page_size) - 1))
#define	LPAGE_OFFSET(off, page_size) \
		((off_t)(off) & (off_t)((page_size - 1)))

#ifdef DEBUG_BUFTRACE
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
	 * If the bp's trace buffer isn't initialize, then
	 * other things may not be either.  In that case
	 * just don't do anything.
	 */
	if (bp->b_trace == NULL) {
		return;
	}

	ktrace_enter(buf_trace_buf,
		     (void *)id,
		     (void *)(__psint_t)cpuid(),
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
		     (void *)(__psint_t)cpuid(),
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

#endif /* DEBUG_BUFTRACE */
	   
/*
 * Delwri buffers associated with a vnode are linked onto the first
 * bfreelist's b_dforw/b_dback list, in time order, for easy access when
 * the # of delwri-s grows too large.  A vnode-assoicated buffer's b_dforw
 * pointer always points to itself if it not on the list.
 * Dirty delalloc buffers are linked in the delalloc_dirty list.  We
 * handle that here as well.
 */
void
dchunkunchain(buf_t *bp)
{
	register int i = btoct(bp->b_bufsize);
	int s;
	buf_t *forw;
	buf_t *back;

	ASSERT(bp->b_flags & B_DACCT);
	buftrace("DCHUNKUNCHAIN", bp);

	if (bp->b_flags & B_DELALLOC) {
		s = mutex_spinlock(&delalloc_dlock);
		delalloc_check(bp, B_DACCT);
		delalloc_delete(&delalloc_dirty, bp);
		mutex_spinunlock(&delalloc_dlock, s);
#ifdef DEBUG_BUFTRACE
		atomicAddInt(&delalloc_dbufs, -1);
#endif
	} else {
		s = mutex_spinlock((lock_t *)&bfreelist[0].b_fsprivate);
		forw = bp->b_dforw;
		back = bp->b_dback;
		ASSERT(forw != bp);
		forw->b_dback = back;
		back->b_dforw = forw;
		mutex_spinunlock((lock_t *)&bfreelist[0].b_fsprivate, s);
	}
	atomicAddInt(&dchunkbufs, -1);
	atomicAddInt(&dchunkpages, -i);
	ASSERT(bp->b_flags & B_VDBUF);

	ASSERT(dchunkbufs >= 0 && dchunkpages >= 0 && bp->b_vp->v_dbuf >= 0);
	ASSERT(valusema(&bp->b_lock) <= 0);

	bp->b_dforw = bp->b_dback = bp;
	bp->b_flags &= ~B_DACCT;
}

/*
 * Release a clustered buffer.
 * The constituent buffers are linked to the uberbuffer
 * through the b_dforw pointers (the constituent buffers
 * have first been locked and removed from the dirty-vnode/delalloc list
 * so it's safe to use these pointers).
 */
void
decluster(buf_t *bp)
{
	buf_t *chunkbp;

	buftrace("DECLUSTER HEAD", bp);

	while (chunkbp = bp->b_dforw) {
		buftrace("DECLUSTER LOOP", chunkbp);
		bp->b_dforw = chunkbp->b_dforw;
		chunkbp->b_dforw = chunkbp;
		/*
		 * Pass the error up so that we can recover from it
		 */
		if (bp->b_flags & B_ERROR) {
			chunkbp->b_flags |= 
					(bp->b_flags & (B_ERROR|B_STALE));
			chunkbp->b_error = bp->b_error;
		}
		(*chunkbp->b_relse)(chunkbp);
	}

	if ((bp->b_flags & B_MAPPED) && IS_KSEG2(bp->b_un.b_addr)) {
		atomicAddInt(&bmappedcnt, -btoc(bp->b_bufsize));
		kvfree(bp->b_un.b_addr, btoc(bp->b_bufsize));
	}

	/* vsema(&bp->b_lock); unnecessary */
	freerbuf(bp);
}

#ifdef DEBUG_BUFTRACE
void
cluster_check(buf_t *bp)
{
	buf_t	*nbp;
	buf_t	*lbp;
	int	count;

	count = 0;
	nbp = bp->b_dforw;
	lbp = bp;
	while (nbp != NULL) {
		ASSERT((lbp->b_offset + BTOBB(lbp->b_bcount)) == nbp->b_offset);
		if (lbp->b_blkno >= 0) {
			ASSERT((lbp->b_blkno + BTOBB(lbp->b_bcount)) == nbp->b_blkno);
		} else {
			ASSERT(nbp->b_blkno == -1);
		}
		lbp = nbp;
		nbp = nbp->b_dforw;
		ASSERT(count <= tune.t_dwcluster);
		count++;
	}
}
#endif

/*
 * Code to cluster logically- and physically-contiguous buffers
 * for faster write.
 */
void
clusterwrite(buf_t *bp, clock_t start)
{
	buf_t		*clusterbp = NULL;
	buf_t		*nbp;
	int		n;
	off_t		offset;
	vnode_t		*vp;
	pfd_t		*pfd;
	uint		clcount;
	int		basync;
	uint		maxiosize;

	ASSERT(!(bp->b_flags & B_DACCT));
	/*
	 * If being pushed by bdflush, and not because buffer
	 * has floated to the top of the free list, give it
	 * another chance in life.
	 */
	if (!(bp->b_flags & B_AGE))
		bp->b_relse = chunkhold;

	maxiosize = ctob(tune.t_dwcluster);

	/*
	 * A non-page-aligned buffer must be mapped to virtual memory.
	 * This is all too much trouble, so punt.
	 * For now, skip any XFS uninitialized buffers, also.
	 */
	buftrace("CLUSTERWRITE START", bp);
	if (bp->b_flags & B_MAPPED && poff(bp->b_un.b_addr) ||
	    bp->b_flags & B_UNINITIAL)
		goto unclustered;
	
	vp = bp->b_vp;
	clcount = 0;
	BUFINFO.clusters++;

	while (clcount < maxiosize) {
		n = BTOBBT(bp->b_bcount);
		offset = bp->b_offset + n;

		/*
		 * If bp has a superior child, its left-most descendent
		 * will always be the buffer most closely superior to bp.
		 * If there is no superior child, the closest superior,
		 * if it exists, will be on the direct parent chain.
		 */
		VN_BUF_LOCK(vp, s);
		nbp = bp->b_forw;
		if (nbp) {
			while (nbp->b_back)
				nbp = nbp->b_back;
		} else {
			nbp = bp->b_parent;
			while (nbp && nbp->b_offset < offset)
				nbp = nbp->b_parent;
		}
		if (!nbp || nbp->b_offset != offset) {
			VN_BUF_UNLOCK(vp, s);
			break;
		}

		/*
		 * If it is physically contiguous and is dirty,
		 * and is not an XFS uninitialized buffer,
		 * cluster it.  Otherwise, stop searching.
		 * Also allow clustering of delayed allocation
		 * buffers.  They can be identified by their
		 * negative block numbers. Thus, only skip nbp
		 * if the buffers are not physically contiguous
		 * and they are not both delayed alloc.
		 * We cannot pass a buffer with bcount > ctob(maxdmasz)
		 * since xlv will not pass it to the disk driver we
		 * could get file corruption (pv 608092). 
		 */
		ASSERT(nbp->b_offset == offset);
		if (clcount + nbp->b_bcount > maxiosize ||
		    ((nbp->b_flags & (B_DELWRI|B_DONE)) != (B_DELWRI|B_DONE)) ||
		    ((nbp->b_blkno != bp->b_blkno + n) &&
		     !((nbp->b_blkno < 0) && (bp->b_blkno < 0))) ||
		    ((nbp->b_flags & B_UNINITIAL) != 0) ||
		    start && nbp->b_start >= start ||
		    !cpsema(&nbp->b_lock)) {
			VN_BUF_UNLOCK(vp, s);
			break;
		}

		/*
		 * Would it be better to VN_BUF_UNLOCK before cpsema, then
		 * check blkno, offset, vp again?
		 */
		VN_BUF_UNLOCK(vp, s);
		ASSERT((nbp->b_offset == offset) &&
		       (nbp->b_vp == vp) &&
		       ((nbp->b_blkno < 0) ||
			(nbp->b_blkno == bp->b_blkno + n)));
		ASSERT((nbp->b_blkno >= 0) || (nbp->b_flags & B_DELALLOC));

		if (((nbp->b_flags & (B_STALE|B_DELWRI|B_DONE)) !=
					     (B_DELWRI|B_DONE)) ||
		      clcount + nbp->b_bcount > maxiosize) {
			vsema(&nbp->b_lock);
			goto unclustered;
		}

		if (!clusterbp) {
			clusterbp = getrbuf(KM_NOSLEEP);
			if (!clusterbp) {
				vsema(&nbp->b_lock);
				goto unclustered;
			}

			buftrace("CLUSTERWRITE !CBP", nbp);
			notavailchunk(nbp);
			ASSERT(nbp->b_flags & B_DACCT);
			dchunkunchain(nbp);

			ASSERT(clusterbp->b_flags == B_BUSY);
			clusterbp->b_flags =
				B_BUSY | B_PAGEIO |
				(bp->b_flags & (B_ASYNC|B_UNCACHED|B_BDFLUSH));
			clusterbp->b_relse = decluster;
			clusterbp->b_dforw = bp;

			clusterbp->b_vp = vp;
			clusterbp->b_edev = bp->b_edev;

			clusterbp->b_blkno = bp->b_blkno;
			clusterbp->b_offset = bp->b_offset;
			clusterbp->b_bcount = bp->b_bcount;

			clusterbp->b_remain = 0;
			pfd = bp->b_pages;
			clusterbp->b_pages = pfd;
			clcount = bp->b_bcount;
			n = btoct(bp->b_bufsize);
			while (--n > 0)
				pfd = pfd->pf_pchain;

			bp->b_flags &= ~(B_READ|B_ERROR|B_DELWRI);
		} else {
			buftrace("CLUSTERWRITE CBP", nbp);
			notavailchunk(nbp);
			ASSERT(nbp->b_flags & B_DACCT);
			dchunkunchain(nbp);
		}

		clusterbp->b_bcount += nbp->b_bcount;
		BUFINFO.clustered++;

		/*
		 * If current buffer ends within a
		 * page boundary, the next buffer
		 * will start on the same page.
		 */
		if (pfd != nbp->b_pages)
			pfd->pf_pchain = nbp->b_pages;

		nbp->b_relse = bp->b_relse;
		nbp->b_flags &= ~(B_READ|B_ERROR|B_DELWRI);

		bp->b_dforw = nbp;
		nbp->b_dforw = (buf_t *)NULL;
		bp = nbp;
			
		clcount += nbp->b_bcount; 
		pfd = nbp->b_pages;
		n = btoct(nbp->b_bufsize);
		while (--n > 0)
			pfd = pfd->pf_pchain;
	}

unclustered:

	if (!clusterbp) {
		clusterbp = bp;
		clusterbp->b_flags &= ~(B_READ | B_DONE | B_ERROR | B_DELWRI);
	} else {
		clusterbp->b_bufsize = ctob(btoc(clusterbp->b_bcount));
#ifdef DEBUG_BUFTRACE
		cluster_check(bp);
#endif
	}
	basync = clusterbp->b_flags & B_ASYNC;

	ASSERT(clusterbp->b_bcount <= maxiosize);
	VOP_STRATEGY(clusterbp->b_vp, clusterbp);

	KTOP_UPDATE_CURRENT_OUBLOCK(1);
	SYSINFO.bwrite += BTOBBT(clusterbp->b_bcount);

	if (!basync) {
		biowait(clusterbp);
		brelse(clusterbp);
	}
}


/*
 * Insert the given buffer into the list pointed to by the given
 * head.
 */
static void
delalloc_insert(buf_t	**headp,
		buf_t	*bp)
{
	buf_t	*head;

	head = *headp;
	if (head == NULL) {
		bp->b_dforw = bp;
		bp->b_dback = bp;
		*headp = bp;
		return;
	}

	/*
	 * Insert the buffer as the last elmt in the list.
	 */
	bp->b_dback = head->b_dback;
	head->b_dback->b_dforw = bp;
	head->b_dback = bp;
	bp->b_dforw = head;
}

/*
 * Delete the given buffer from the list pointed to by the given head.
 */
static void
delalloc_delete(buf_t	**headp,
		buf_t	*bp)
{
	buf_t	*forw;
	buf_t	*back;
	buf_t	*head;

	forw = bp->b_dforw;
	back = bp->b_dback;
	forw->b_dback = back;
	back->b_dforw = forw;
	head = forw;
	if (*headp == bp) {
		if (head == bp) { 
			head = NULL;
		}
		*headp = head;
	}
	bp->b_dforw = bp;
	bp->b_dback = bp;
}

/*
 * delalloc_check() is a debug function used to check that
 * the given buffer is in the proper list.  The dirty_flags
 * parameter allows the caller to specify what flags indicate
 * that the buffer should be in the dirty list.
 */
#ifdef DEBUG_BUFTRACE
static void
delalloc_check(buf_t		*bp,
	       unsigned int	dirty_flags)
{
	buf_t	*head;
	buf_t	*nbp;

	if ((bp->b_flags & dirty_flags) == dirty_flags) {
		head = delalloc_dirty;
		ASSERT(spinlock_islocked(&delalloc_dlock));
	} else {
		head = delalloc_clean;
		ASSERT(spinlock_islocked(&delalloc_clock));
	}

	nbp = head;
	ASSERT(nbp != NULL);
	do {
		if (nbp == bp) {
			return;
		}
		nbp = nbp->b_dforw;
	} while (nbp != head);
	ASSERT(0);
}
#endif /* DEBUG_BUFTRACE */
		
/*
 * This is called to lock a buffer from the given list.  The buffer
 * is only removed from the list if DELALLOC_DELETE is in the flags.
 * It cannot sleep on the buffer locks, however, so if the list is
 * empty or none of the buffers on the list can be locked then it
 * will return NULL.
 *
 * The caller must be holding the lock which guards the given list.
 */
static buf_t *
delalloc_lock_one(buf_t		**headp,
		  unsigned int	flags)
{
	buf_t	*bp;
	buf_t	*head;

	bp = *headp;
	if (bp == NULL) {
		return NULL;
	}

	head = bp;
	do {
		if (cpsema(&bp->b_lock)) {
			if (flags & DELALLOC_DELETE) {
				delalloc_delete(headp, bp);
			}
			ASSERT(bp->b_flags & B_DONE);
			return bp;
		}

		bp = bp->b_dforw;
	} while (bp != head);

	return NULL;
}

/*
 * delalloc_reserve() is called to make room for a new delayed alloc
 * buffer.  It pushes out delayed allocation buffers until there
 * is room for a new one, and then it decreases the
 * delallocleft counter to indicate how many can still be reserved.
 */
static void
delalloc_reserve(void)
{
	buf_t	*bp;
	uint64_t flags;
	int	s;
	int	loops;

	ASSERT(fetchIntHot(&delallocleft) >= 0);
	BUFINFO.delrsv++;
	if (compare_and_dec_int_gt_hot(&delallocleft, 0)) {
		BUFINFO.delrsvfree++;
		return;
	}
retry:
	loops = 0;
	while (fetchIntHot(&delallocleft) == 0) {
		loops++;
		s = mutex_spinlock(&delalloc_clock);
		bp = delalloc_lock_one(&delalloc_clean, DELALLOC_DELETE);
		mutex_spinunlock(&delalloc_clock, s);
		if (bp != NULL) {
			/*
			 * We've pulled a clean delalloc buffer off
			 * of the list.  Now decomission it.
			 * Clear B_DELALLOC before calling chunkrelse
			 * so it won't mess with the delalloc lists
			 * or counters. This way we claim the buffer's
			 * place among the allowed delalloc bufs.
			 */
#ifdef DEBUG_BUFTRACE
			atomicAddInt(&delalloc_cbufs, -1);
#endif
			buftrace("DELALLOC_RESERVE CLEAN", bp);
			notavailchunk(bp);
			bp->b_flags &= ~B_DELALLOC;
			ASSERT(!(bp->b_flags & B_DELWRI));
			if (bp->b_flags & B_NFS_UNSTABLE)
				chunkcommit(bp, 0);
			else
				chunkrelse(bp);
			BUFINFO.delrsvclean++;
			return;
		}
		/*
		 * There were no clean buffers available, so try to
		 * push out a dirty one and start over.  We're only
		 * willing to do that so many times, however.
		 */
		if (loops < 2) {
			s = mutex_spinlock(&delalloc_dlock);
			bp = delalloc_lock_one(&delalloc_dirty,
					       DELALLOC_DELETE);
			mutex_spinunlock(&delalloc_dlock,s);
			if (bp != NULL) {
				/*
				 * Before we push the buffer do the
				 * equivalent of a call to dchunkunchain().
				 */
				ASSERT((bp->b_flags &
					(B_DACCT | B_DELALLOC | B_DELWRI)) ==
				       (B_DACCT | B_DELALLOC | B_DELWRI));
				ASSERT(bp->b_flags & B_VDBUF);
				atomicAddInt(&dchunkbufs, -1);
				atomicAddInt(&dchunkpages, 
						-(btoct(bp->b_bufsize)));
#ifdef DEBUG_BUFTRACE
				atomicAddInt(&delalloc_dbufs, -1);
#endif
				buftrace("DELALLOC_RESERVE DIRTY", bp);
				notavailchunk(bp);
				flags = bp->b_flags | B_ASYNC | B_BDFLUSH;
				flags &= ~B_DACCT;
				bp->b_flags = flags;
				clusterwrite(bp, 0);
				BUFINFO.delrsvdirty++;
				continue;
			}
		}
		/*
		 * We can't or we refuse to push more buffers, so
		 * go to sleep waiting for a delalloc buffer to be
		 * released.  This coordinates with delalloc_free()
		 * to get this right.  delalloc_waiters will be
		 * decremented by the process that wakes us up.
		 */
		s=mutex_spinlock(&delalloc_wlock);
		delalloc_waiters++;
		BUFINFO.delrsvwait++;
		del_wait(s);
		loops = 0;
	}
	ASSERT(fetchIntHot(&delallocleft) >= 0);
	if (!compare_and_dec_int_gt_hot(&delallocleft, 0))
		goto retry;
}

/*
 * delalloc_found() is called after a call to delalloc_reserve() and
 * a successful call to fetch_chunk().  It either marks the buffer
 * as delayed allocate if it is not already so or releases the reservation
 * taken in delalloc_reserve() if the buffer was already there and
 * marked delayed allocated.  For newly allocated delayed allocation buffers
 * that sit over holes in the file, call VOP_PAGES_SETHOLE() to set the P_HOLE
 * bit in the pages attached to the buffer.  If the fetch_chunk() failed,
 * then we will be called with a NULL buf pointer and we just undo the
 * reservation.
 */
static void
delalloc_found(
	buf_t	*bp,
	uint	flags)
{
	int	pg_cnt;
#ifdef DEBUG_BUFTRACE
	int	s;
	lock_t	*lck;
#endif
	

	if (bp == NULL) {
		atomicAddIntHot(&delallocleft, 1);
	} else if (bp->b_flags & B_DELALLOC) {
		buftrace("DELALLOC_FOUND SET", bp);
		ASSERT((bp->b_flags & (B_DELWRI | B_DACCT)) != B_DACCT);
#ifdef DEBUG_BUFTRACE
		if ((bp->b_flags & B_DACCT) == B_DACCT)
			lck = &delalloc_dlock;
		else 
			lck = &delalloc_clock;
		s = mutex_spinlock(lck);
		delalloc_check(bp, B_DACCT);
		mutex_spinunlock(lck, s);
#endif
		atomicAddIntHot(&delallocleft, 1);
	} else if ((bp->b_blkno != -1)  && !(flags & BMAP_UNWRITTEN)) {
		/*
		 * Someone beat our caller to the buffer and made it a
		 * real buffer rather than a delalloc one.  Release
		 * our delalloc reservation and use the buffer.
		 */
		buftrace("DELALLOC_FOUND !-1", bp);
		atomicAddIntHot(&delallocleft, 1);
	} else {
		buftrace("DELALLOC_FOUND NOT SET", bp);
		bp->b_flags |= B_DELALLOC;
		if (flags & BMAP_HOLE) {
			/*
			 * This buffer sits over a hole in the file, so
			 * set the P_HOLE bit in each of its pages.  This
			 * is used in pfault() to ensure that space is
			 * allocated to pages over holes when they are
			 * stored to via a mapped file.
			 */
			pg_cnt = btoct(bp->b_bufsize);
			VOP_PAGES_SETHOLE(bp->b_pages->pf_vp, bp->b_pages, pg_cnt, 0, 0);
		}
	}
}

/*
 * delalloc_hold() keeps delayed allocation buffers on the proper
 * delalloc lists.  It also maintains the dirty buffer accounting for
 * dirty delalloc buffers.   Finally, it will wake up a process if there
 * is one waiting for a delayed allocation buffer to be unlocked.
 */
static void
delalloc_hold(buf_t *bp)
{
	uint64_t flags;
	int	s;
	int	wakeup;

	ASSERT(bp->b_flags & B_BUSY);
	ASSERT(bp->b_flags & B_DELALLOC);
	ASSERT(bp->b_flags & B_DONE);

	wakeup = 0;
	flags = bp->b_flags;
	if (flags & B_DELWRI) {
		buftrace("DELALLOC_HOLD DELWRI", bp);
		if (!(flags & B_DACCT)) {
			/*
			 * This is a newly dirtied buffer which has not
			 * yet been accounted for.  Account for it in the
			 * dirty buffer counters and move it from the
			 * clean delalloc list to the dirty delalloc list.
			 */
			atomicAddInt(&dchunkbufs, 1);
			atomicAddInt(&dchunkpages, btoct(bp->b_bufsize));
			s = mutex_spinlock(&delalloc_clock);
			if ((bp->b_dforw != bp) || (delalloc_clean == bp)) {
				delalloc_check(bp, B_DACCT);
				delalloc_delete(&delalloc_clean, bp);
#ifdef DEBUG_BUFTRACE
				atomicAddInt(&delalloc_cbufs, -1);
#endif
			}
			mutex_spinunlock(&delalloc_clock, s);
			s = mutex_spinlock(&delalloc_dlock);
			delalloc_insert(&delalloc_dirty, bp);
			mutex_spinunlock(&delalloc_dlock, s);
#ifdef DEBUG_BUFTRACE
			atomicAddInt(&delalloc_dbufs, 1);
#endif
			bp->b_flags |= B_DACCT;
		} 
#ifdef DEBUG_BUFTRACE
		s = mutex_spinlock(&delalloc_dlock);
		delalloc_check(bp, B_DACCT);
		mutex_spinunlock(&delalloc_dlock, s);
#endif
		if (!(bp->b_flags & B_VDBUF)) {
			bp->b_flags |= B_VDBUF;
			atomicAddInt(&(bp->b_vp->v_dbuf), 1);
		}
	} else if (flags & B_DACCT) {
		/*
		 * Decrement the dirty buffer and dirty buffer
		 * pages counters since the buffer is now clean.
		 * Also move it from the dirty list to the clean
		 * list.
		 */
		buftrace("DELALLOC_HOLD DACCT", bp);
		atomicAddInt(&dchunkbufs, -1);
		atomicAddInt(&dchunkpages, -(btoct(bp->b_bufsize)));
		s = mutex_spinlock(&delalloc_dlock);
		delalloc_check(bp, B_DACCT);
		bp->b_flags &= ~B_DACCT;
		delalloc_delete(&delalloc_dirty, bp);
		mutex_spinunlock(&delalloc_dlock, s);
#ifdef DEBUG_BUFTRACE
		atomicAddInt(&delalloc_dbufs, -1);
#endif
		s = mutex_spinlock(&delalloc_clock);
		delalloc_insert(&delalloc_clean, bp);
		mutex_spinunlock(&delalloc_clock, s);
#ifdef DEBUG_BUFTRACE
		atomicAddInt(&delalloc_cbufs, 1);
#endif
		ASSERT((bp->b_flags & B_VDBUF) && (bp->b_vp->v_dbuf > 0));
		bp->b_flags &= ~B_VDBUF;
		atomicAddInt(&(bp->b_vp->v_dbuf), -1);
	} else {
		/*
		 * It's just a clean buffer.  If it's not yet in the
		 * delalloc_clean list, then add it.
		 */

		if (bp->b_flags & B_VDBUF) {
			bp->b_flags &= ~B_VDBUF;
			ASSERT(bp->b_vp->v_dbuf > 0);
			atomicAddInt(&(bp->b_vp->v_dbuf), -1);
		}
		buftrace("DELALLOC_HOLD CLEAN", bp);
		s = mutex_spinlock(&delalloc_clock);
		if ((bp->b_dforw == bp) && (delalloc_clean != bp)) {
			delalloc_insert(&delalloc_clean, bp);
#ifdef DEBUG_BUFTRACE
			atomicAddInt(&delalloc_cbufs, 1);
#endif
		}
		delalloc_check(bp, B_DACCT);
		mutex_spinunlock(&delalloc_clock, s);
	}

	/*
	 * Check if anyone needs to be awakened.
	 */
	if (delalloc_waiters > 0) {
		s = mutex_spinlock(&delalloc_wlock);
		if (delalloc_waiters > 0) {
			delalloc_waiters--;
			wakeup = 1;
		}
		mutex_spinunlock(&delalloc_wlock, s);
	}

	if (wakeup) {
		del_wake();
	}
}

/*
 * delalloc_free() is called when a delayed allocation buffer is being
 * destroyed.  It must pull the buffer from the appropriate list,
 * decrement the count of delalloc buffers, and wake up a process waiting
 * for a delalloc buffer to be released if there is one.
 * It is possible that the buffer was never handled by chunkhold(), in
 * which case it won't be on any lists.
 */
void
delalloc_free(buf_t *bp)
{
	uint64_t flags;
	int	s;
	int	wakeup;

	ASSERT(bp->b_flags & B_BUSY);
	ASSERT(bp->b_flags & B_DELALLOC);

	wakeup = 0;
	flags = bp->b_flags & ~B_DELALLOC;
	atomicAddIntHot(&delallocleft, 1);

	if (flags & B_DACCT) {
		buftrace("DELALLOC_FREE DACCT", bp);
		s = mutex_spinlock(&delalloc_dlock);
		delalloc_check(bp, B_DACCT);
		delalloc_delete(&delalloc_dirty, bp);
		mutex_spinunlock(&delalloc_dlock, s);
#ifdef DEBUG_BUFTRACE
		atomicAddInt(&delalloc_dbufs, -1);
#endif
		atomicAddInt(&dchunkbufs, -1);
		atomicAddInt(&dchunkpages, -(btoct(bp->b_bufsize)));
		flags &= ~B_DACCT;
	} else {
		/*
		 * The buffer is either on the delalloc_clean list or
		 * it was never handled by chunkhold() at all.
		 */
		buftrace("DELALLOC_FREE CLEAN", bp);
		s = mutex_spinlock(&delalloc_clock);
		if ((bp->b_dforw != bp) || (delalloc_clean == bp)) {
			delalloc_check(bp, B_DACCT);
			delalloc_delete(&delalloc_clean, bp);
#ifdef DEBUG_BUFTRACE
			atomicAddInt(&delalloc_cbufs, -1);
#endif
		}
		mutex_spinunlock(&delalloc_clock, s);
	}

	if (delalloc_waiters > 0) {
		s = mutex_spinlock(&delalloc_wlock);
		if (delalloc_waiters > 0) {
			delalloc_waiters--;
			wakeup = 1;
		}
		mutex_spinunlock(&delalloc_wlock, s);
	}

	if (wakeup) {
		del_wake();
	}

	bp->b_flags = flags;
}
/*
 * Toss a delwri vnode-associated buffer.
 */
#ifndef DCHUNKTRIES
#define DCHUNKTRIES 25
#endif
static void
dchunkpush(void)
{
	register buf_t	*bp;
	register int	tries = DCHUNKTRIES;
	register buf_t	*back;
	register buf_t	*forw;
	uint64_t	flags;
	int		s;

	s = mutex_spinlock((lock_t *)&bfreelist[0].b_fsprivate);
	for (bp = bfreelist[0].b_dforw; bp != &bfreelist[0];
	     bp = bp->b_dforw) {

		if (!cpsema(&bp->b_lock)) {
			if (--tries <= 0)
				break;
			continue;
		}

		if ((bp->b_vp) && (bp->b_vp->v_flag & VDFSCONVERTED)) {
			buftrace("DCHUNKPUSH DFS", bp);
			vsema(&bp->b_lock);
			continue;
		}

		buftrace("DCHUNKPUSH NORM", bp);
		forw = bp->b_dforw;
		back = bp->b_dback;

		ASSERT(bp->b_vp);
		ASSERT(forw != bp);

		forw->b_dback = back;
		back->b_dforw = forw;
		mutex_spinunlock((lock_t *)&bfreelist[0].b_fsprivate, s);
		atomicAddInt(&dchunkbufs, -1);
		atomicAddInt(&dchunkpages, -(btoct(bp->b_bufsize)));
		ASSERT(dchunkbufs >= 0 &&
		       dchunkpages >= 0 &&
		       bp->b_vp->v_dbuf >= 0);

		bp->b_dforw = bp->b_dback = bp;

		ASSERT((bp->b_flags & (B_DACCT|B_DELALLOC)) == B_DACCT);
		ASSERT(bp->b_flags & B_VDBUF);
		bp->b_flags &= ~B_DACCT;

		if (bp->b_flags & B_DELWRI) {
			notavailchunk(bp);
			bp->b_flags |= B_ASYNC|B_BDFLUSH;
			clusterwrite(bp, 0);
		} else {
			vsema(&bp->b_lock);
		}
		return;
	}
	mutex_spinunlock((lock_t *)&bfreelist[0].b_fsprivate, s);

	s = mutex_spinlock(&delalloc_dlock);
	bp = delalloc_lock_one(&delalloc_dirty, DELALLOC_DELETE);
	mutex_spinunlock(&delalloc_dlock, s);
	if (bp != NULL) {
		/*
		 * Before we push the buffer do the
		 * equivalent of a call to dchunkunchain().
		 */
#ifdef DEBUG_BUFTRACE
		atomicAddInt(&delalloc_dbufs, -1);
#endif
		buftrace("DCHUNKPUSH DELALLOC", bp);
		ASSERT((bp->b_flags &
			(B_DACCT | B_DELALLOC | B_DELWRI)) ==
		       (B_DACCT | B_DELALLOC | B_DELWRI));
		ASSERT(bp->b_flags & B_VDBUF);
		atomicAddInt(&dchunkbufs, -1);
		atomicAddInt(&dchunkpages, -(btoct(bp->b_bufsize)));
		notavailchunk(bp);
		flags = bp->b_flags | B_ASYNC|B_BDFLUSH;
		flags &= ~B_DACCT;
		bp->b_flags = flags;
		clusterwrite(bp, 0);
		return;
	}
}


/*
 * vhand is running and needs memory.
 */
/* ARGSUSED */
static int
chunk_pshake(int resource)
{
	buf_t	*bp;
	int	count;
	time_t	age = lbolt - tune.t_autoup*HZ/2;  /* emergency */
	int 	error;
	buf_t	*ohash;
	extern	int bufmem;
	extern	void bio_freemem(buf_t *);
	int64_t nbytes;
	int	ignorefilebufs = 0, ignoremetabufs = 0;
	pgno_t	npages;

	npages = min_free_pages - GLOBAL_FREEMEM();	/* snapshot */
	if (npages <= 0)
		return 0;

	nbytes = ctob(npages);

	/*
	 * Release clean vnode-associated buffers -- it is a lot cheaper
	 * to reattach pages to buffers than it is to page user data
	 * back in from secondary storage.
	 * Release memory attached to metadata buffers too. 
	 */
	count = v.v_buf / 8;

	for (bp = bphand; --count >= 0; bp++) {
		/*
		 * Make sure we do not go below the min_file_pages or
		 * min_bufmem
		 */
		if (chunkpages <= min_file_pages) {
			if (bufmem <= min_bufmem)
				break;
			ignorefilebufs = 1;
		} else if (bufmem <= min_bufmem) {
			ignoremetabufs = 1; 
		} 
	
		if (bp >= bplast) {
			bp = &global_buf_table[0];
		}

		if (!ignorefilebufs && bp->b_vp && 
		    (!(bp->b_flags & B_DELWRI) || bp->b_start < age) 
		    && cpsema(&bp->b_lock)) {

			if (!bp->b_vp) {
				if (!ignoremetabufs)
					goto metadata;
				vsema(&bp->b_lock);
				continue;
			}

			notavailchunk(bp);
			buftrace("CHUNK_PSHAKE", bp);
			ASSERT(!(bp->b_flags & B_STALE));

			if (bp->b_flags & B_DELWRI) {
				dchunkunchain(bp);
				bp->b_flags |= B_ASYNC|B_BDFLUSH;
				bwrite(bp);
				continue;
			}

			nbytes -= bp->b_bufsize;

			ASSERT(bp->b_relse);
			if (bp->b_flags & B_NFS_UNSTABLE) {
				bp->b_flags |= B_ASYNC;
				error = 0;
				VOP_COMMIT(bp->b_vp, bp, error);
				if (!error)
					BUFINFO.commits++;
			} else
				(*bp->b_relse)(bp);
		} else if (!ignoremetabufs && !bp->b_vp && bp->b_bufsize 
		    && cpsema(&bp->b_lock)) {
			
metadata:
                        if (bp->b_vp || bp->b_pincount || 
			    bp->b_relse || !bp->b_bufsize || 
			    bp->b_start > age ||
			    (bp->b_flags & (B_DELWRI|B_NFS_UNSTABLE))) {
                                vsema(&bp->b_lock);
                                continue;
                        }

                        /*
                         * have to remove the buffer from the old hash
                         * list, if we can grab the old hash list semaphore
                         */
                        if (bp->b_edev != NODEV) {
                                ohash = bhash(bp->b_edev, bp->b_blkno);
                                if (!cpsema(&ohash->b_lock)) {
                                        vsema(&bp->b_lock);
                                        continue;
                                }
                                notavail(bp);

                                /*
                                 * Take it off the old hash list
                                 */
                                bp->b_back->b_forw = bp->b_forw;
                                bp->b_forw->b_back = bp->b_back;
                                vsema(&ohash->b_lock);

                        } else
                                notavail(bp);

			buftrace("CHUNK_PSHAKE", bp);
			nbytes -= bp->b_bufsize;
                        if (bp->b_bufsize)
                                bio_freemem(bp);
			bp->b_ref = 0;
                        putphysbuf(bp);
                }

		if (nbytes <= 0)
			break;

	}

	bphand = bp;
	return btoct(nbytes);
}

/*
 * Short of kernel VM.
 */
/* ARGSUSED */
static int
chunk_vshake(int resource)
{
	buf_t		*bp;
	int		count;
	static time_t	last_shaken;
	time_t		now = lbolt;
	int		ret, error;

	/*
	 * Don't let a mob of callers (e.g.: sptallocs)
	 * get too crazy -- ignore near-simultaneous requests.
	 */
	if (last_shaken <= (now - (HZ*16)))
		return 0;

	if (mutex_trylock(&chunk_vshake_lock) == 0)
		return 0;

	last_shaken = now;
	ret = 0;

	count = nbuf / 8;

	for (bp = bvshand; --count >= 0; bp++) {

		if (bp >= bplast)
			bp = &global_buf_table[0];

		if (IS_KSEG2(bp->b_un.b_addr)
		 && bp->b_vp
		 && !(bp->b_flags & B_DELWRI)
		 && cpsema(&bp->b_lock)) {

			if (!IS_KSEG2(bp->b_un.b_addr)
			 || !bp->b_vp
			 || bp->b_flags & B_DELWRI) {
				vsema(&bp->b_lock);
				continue;
			}
			ASSERT(BP_ISMAPPED(bp));
			ASSERT(!(bp->b_flags & B_STALE));

			buftrace("CHUNK_VSHAKE", bp);

			ret += BTOBBT(bp->b_bufsize);	/* aproximately... */

			if (poff(bp->b_un.b_addr)) {
				notavailchunk(bp);
				if (bp->b_flags & B_NFS_UNSTABLE) {
					bp->b_flags |= B_ASYNC;
					error = 0;
					VOP_COMMIT(bp->b_vp, bp, error);
					if (!error)
						BUFINFO.commits++;
				} else
					chunkrelse(bp);
			} else {
				bp_mapout(bp);
				vsema(&bp->b_lock);
			}
		}
	}

	bvshand = bp;
	mutex_unlock(&chunk_vshake_lock);
	return dtopt(ret);
}

/*
 * Routine (only) called from bdflush when # of
 * K2SEG mapped buffer cache pages gets too high.
 */
void
chunk_vflush(void)
{
	buf_t		*bp;
	int		count;
	extern int	sptsz;

	if (mutex_trylock(&chunk_vshake_lock) == 0)
		return;

	count = nbuf / 8;

	for (bp = bvhand; --count >= 0; bp++) {

		if (bmappedcnt < sptsz/2)
			break;

		if (bp >= bplast)
			bp = &global_buf_table[0];

		if (IS_KSEG2(bp->b_un.b_addr)
		 && bp->b_vp
		 && !(bp->b_flags & B_DELWRI)
		 && cpsema(&bp->b_lock)) {

			if (!IS_KSEG2(bp->b_un.b_addr)
			 || !bp->b_vp
			 || bp->b_flags & B_DELWRI) {
				vsema(&bp->b_lock);
				continue;
			}
			ASSERT(BP_ISMAPPED(bp));
			ASSERT(!(bp->b_flags & B_STALE));

			buftrace("CHUNK_VFLUSH", bp);

			if (poff(bp->b_un.b_addr)) {
				notavailchunk(bp);
				if (bp->b_flags & B_NFS_UNSTABLE)
					chunkcommit(bp, 0);
				else
					chunkrelse(bp);
			} else {
				bp_mapout(bp);
				vsema(&bp->b_lock);
			}
		}
	}

	bvhand = bp;
	mutex_unlock(&chunk_vshake_lock);
}

void
mem_adjust(void)
{
	pgno_t npages;
	pgno_t over;
	pgno_t cp = chunkpages;			/* convert to pgno_t */
	pgno_t gf;				/* local copy */
	pgno_t min_file = min_file_pages;	/* convert to pgno_t */
	pgno_t min_free = min_free_pages;	/* convert to pgno_t */
	pgno_t min_buf = min_bufmem;		/* convert to pgno_t */
	pgno_t rmem;				/* local copy */
	pgno_t smem;				/* local copy */
	pgno_t j;
	extern void free_buffer_mem(pgno_t);
	extern int bufmem;
	pgno_t mp = bufmem;			/* convert to pgno_t */ 
	pgno_t cache;				/* bufmem plus chunkpages */

	/*
	 * If the file cache has grown beyond min_file_pages
	 * or the metadata cache has grown beyond min_bufmem,
	 * and is either threatening the free page list or
	 * has caused an overcommittment of avail{rs}mem, trim them.
	 */
	over = cp - min_file;
	if (over < 0)
		over = 0;
	if ((mp - min_buf) > 0) {
		over += mp - min_buf;
	}

	gf = GLOBAL_FREEMEM();
	rmem = GLOBAL_AVAILRMEM();
	smem = GLOBAL_AVAILSMEM();
	cache = cp + mp;

	if (over > 0) {
		/*
		 * Is min_free_pages less than target??
		 */
		npages = min_free - gf;
		if (npages < 0)
			npages = 0;

		/*
		 * Now check whether the combined chunkpages and
	         * bufmem has grown over availrmem or availsmem.  
		 * If so, trim to the smaller of the two, so we 
		 * don't get a memory deadlock.
		 */
		j = MIN(rmem,smem);
		if (cache > j) {
			j = cache - j;
			if (j > npages) {
				npages = j;
			}
		}

		if (npages > 0) {
			if (over < npages)
				npages = over;
			free_buffer_mem(npages);
		}
	}

}

/*
 * This routine is called to flush the buffer cache because
 * it has grown too large (threatening freemem or avail{rs}mem).
 * minpages is the minimum number of pages to steal -- typically,
 * the number to get freemem up to the target range.
 * Changed to flush only clean pages, since bdflush flushes the delwri
 * buffers before getting here.
 */
void
free_buffer_mem(pgno_t minpages)
{
        register time_t date, age;
        register struct buf *bp, *ohash;
        int j;
	int64_t	nbytes = ctob(minpages);
	short	ignorefilebufs, ignoremetabufs;
	extern int bufmem;
	extern int min_bufmem;
	extern	void bio_freemem(buf_t *);

        age = tune.t_autoup * HZ * 2;

	for ( ; ; ) {

		/*
		 * Reset date with respect to 'right now', so we don't
		 * get further and further back when searching really
		 * large buffer sets.
		 */
		date = lbolt - age;
		ignorefilebufs = ignoremetabufs = 0;

		for (j = nbuf, bp = bpfhand; --j >= 0; bp++) {

			if (chunkpages <= min_file_pages) {
				if (bufmem <= min_bufmem) {
					bpfhand = bp;
					return;
				}
				ignorefilebufs = 1;
			} else if (bufmem <= min_bufmem) {
				ignoremetabufs = 1;
			}

			if (bp >= bplast)
				bp = &global_buf_table[0];

			if (!ignorefilebufs && bp->b_vp && 
	                    !(bp->b_flags & B_DELWRI) && (bp->b_start <= date) 
			    && cpsema(&bp->b_lock)) {

				if (!bp->b_vp || (bp->b_flags & B_DELWRI)) {
					vsema(&bp->b_lock);
					continue;
				}
				notavail(bp);

				buftrace("FREE_BUFFER_MEM", bp);
				if (bp->b_flags & B_NFS_UNSTABLE) {
					ASSERT(bp->b_relse == chunkrelse);
					chunkcommit(bp, 0);
					continue;
				}

				/*
				 * We want to know the average buffer size so
				 * we can figure out, typically, how much
				 * 'memory damage' we'll do by activating
				 * another buffer header later.
				 * We don't need a lock here -- only
				 * mem_adjust and its subroutines
				 * fiddle with chunk_avg_page_size, and
				 * mem_adjust is only called by bdflush.
				 */

				nbytes -= bp->b_bufsize;

				chunk_avg_page_size_sum -=
						chunk_avg_page_size();
				chunk_avg_page_size_sum += btoct(bp->b_bufsize);

				chunkinactivate(bp);

			} else if (!ignoremetabufs && !bp->b_vp && bp->b_bufsize
			   && (bp->b_start <= date) && cpsema(&bp->b_lock)) {

				if (bp->b_vp) {
					vsema(&bp->b_lock);
					continue;
				}

				if (bp->b_ref)
					--bp->b_ref;

				if (bp->b_ref || bp->b_pincount ||
                                    bp->b_relse || (bp->b_flags & B_DELWRI) 
				    || !bp->b_bufsize) {
                                        vsema(&bp->b_lock);
                                        continue;
                                }

				buftrace("FREE_BUFFER_MEM", bp);
				/*
                                 * have to remove the buffer from the old hash
                                 * list, if we can grab the old hash list
                                 * semaphore
                                 */
                                if (bp->b_edev != NODEV) {
                                        ohash = bhash(bp->b_edev, bp->b_blkno);
                                        if (!cpsema(&ohash->b_lock)) {
                                                vsema(&bp->b_lock);
                                                continue;
                                        }
                                        notavail(bp);

                                        /*
                                         * Take it off the old hash list
                                         */
                                        bp->b_back->b_forw = bp->b_forw;
                                        bp->b_forw->b_back = bp->b_back;
                                        vsema(&ohash->b_lock);

                                } else
                                        notavail(bp);

                                if (bp->b_bufsize) {
                                        nbytes -= bp->b_bufsize;
                                        bio_freemem(bp);
                                }
                                putphysbuf(bp);
 
			}

			if (nbytes <= 0) {
				bpfhand = bp;
				return;
			}

		}

		age -= HZ;
		if (age < 0) {
			bpfhand = bp;
			return;
		}
	}
}

void
activate_bufs(void)
{

	pgno_t npages, j;
	extern void bpactivate(pgno_t);
	extern buf_t binactivelist;
	extern int bemptycnt;

        /*
         * If so, determine a memory target, figure out how many
         * buffers it would take to get to that target, and
         * divide by a ramp-up factor.
         *
         * The memory target is to the smaller of avail{rs}mem,
         * as long as it will not affect min_free_pages.
         */
        if (binactivelist.b_bcount > bemptycnt) {
                /*
                 * npages here will be the target number of chunkpages
                 * in the cache.  It should be no larger than avail{rs}mem.
                 */
                npages = MIN(GLOBAL_AVAILRMEM(), GLOBAL_AVAILSMEM());

                /*
                 * It can also grow such that there are still min_free_pages
                 * on the free list.  We figure out how many pages to
                 * grow the chunk cache to reach this limit.
                 * Note that we have to divide by two when computing how
                 * many pages to add to chunkpages because each page we
                 * add to a buffer will likely be taken from the free list.
                 */
                j = ((GLOBAL_FREEMEM() - min_free_pages) / 2) + chunkpages;

                if (j < npages)
                        npages = j;

                /*
                 * If we're below min_free_pages already, we certainly
                 * have the right to grow to there.  It seems a little
                 * silly to add chunkpages to the above calculation and
                 * then subtract it below, but the min_file_pages
                 * minimum takes precedence over all.
                 */
                if (npages < min_file_pages)
                        npages = min_file_pages;

                /*
                 * Now make npages the number of pages we'd like to grow
                 * the chunk cache.
                 */
                npages -= chunkpages;

                if (npages > 0) {
                        /*
                         * After we figure the target number of pages
                         * we'd like to grow the buffer cache, we apply
                         * a damping factor -- we want to avoid whip-sawing
                         * around our targets.
                         * chunk_avg_page_size() is the amount of memory a
                         * buffer will likely map; we'll grow a quarter of
                         * the way to our target this iteration.
                         */
                        npages = (npages + 3) / 4;
                        j = npages / chunk_avg_page_size();

                        j -= bemptycnt;

                        if (j > 0) {
                                bpactivate(j);
			}
                }
	}
}

/*
 * Release function that's called by free_buffer_mem.  It acts just
 * like chunkrelse, except that it inactivates the buffers instead
 * of releasing them to the general pool, and it keeps average-
 * buffer-size statistics (to be used to reactivate them later).
 */
void
chunkinactivate(register buf_t *bp)
{
	uint64_t flags = bp->b_flags;
	int	i;
	extern void bpinactivate(buf_t *);
	extern void dev_unchain(buf_t *);
	extern int bemptycnt;

	ASSERT((flags & (B_DONE|B_BUSY|B_PAGEIO|B_DELWRI|B_DACCT)) == \
			(B_DONE|B_BUSY|B_PAGEIO) && \
		(valusema(&bp->b_lock) <= 0));
	ASSERT(!(bp->b_flags & (B_VDBUF|B_NFS_UNSTABLE)));

	buftrace("CHUNKINACTIVATE", bp);

	/*
	 * If there are delallocleft and the bfreelist is empty then do
	 * not inactivate this buffer since we could deadlock.
	 */
	if (fetchIntHot(&delallocleft) && !bemptycnt) {
		chunkrelse(bp);
		return;
	}

	/*
	 * If it's a delalloc buffer then clean up its delalloc state.
	 */
	if (flags & B_DELALLOC) {
		delalloc_free(bp);
	}

	i = btoct(bp->b_bufsize);
	ASSERT(i == btoc(bp->b_bufsize));

	atomicAddInt(&chunkpages, -i);

	pagesrelease(bp->b_pages, i, flags);

	chunkunhash(bp);
	bp->b_forw = bp->b_back = bp;
	bp->b_pages = (struct pfdat *)0;

	/*
	 * Can't call putphysbuf -- it sets b_relse to NULL.
	 */
	if ((flags & B_MAPPED) && IS_KSEG2(bp->b_un.b_addr)) {
		kvfree(bp->b_un.b_addr, i);
		atomicAddInt(&bmappedcnt, -i);
	}

	bp->b_un.b_addr = (caddr_t)0;

	dev_unchain(bp);

	bp->b_bcount = bp->b_bufsize = 0;
	bp->b_relse = 0;
	bp->b_error = 0;
	bp->b_edev = NODEV;
	bp->b_vp = NULL;
	bp->b_offset = 0;
	bp->b_parent = NULL;

	bpinactivate(bp);	/* b_flags set in bpinactivate */
}


/*
 * Release function that returns all memory to the page pool.
 * Usually called when free memory is getting tight or the number
 * of vnode-associated buffers is growing disproportionately large.
 */
void
chunkrelse(buf_t *bp)
{
	uint64_t flags = bp->b_flags;
	int	i;
#ifdef DEBUG
	extern struct vnodeops xfs_vnodeops;
	extern int xfs_isshutdown(bhv_desc_t *);
	bhv_desc_t *bhv;
#endif

	ASSERT((bp->b_flags & (B_DONE|B_BUSY|B_PAGEIO)) == \
				(B_DONE|B_BUSY|B_PAGEIO) && \
		(valusema(&bp->b_lock) <= 0));
#ifdef DEBUG
	if (!((bhv = vn_bhv_lookup_unlocked(VN_BHV_HEAD(bp->b_vp),
			&xfs_vnodeops)) && xfs_isshutdown(bhv))) {
		ASSERT((flags & (B_STALE|B_DELWRI)) != (B_STALE|B_DELWRI));
	}
#endif

	buftrace("CHUNKRELSE", bp);
	if ((flags & (B_STALE|B_DELWRI|B_ERROR)) == B_DELWRI && bp->b_pages) {
		chunkhold(bp);
		return;
	}

	/*
	 * Once the buffer has been written to the server cache,
	 * don't reclaim it until we are sure that the data got to
	 * disk on server side. Retry only once then toss the 
	 * buffer. The retry will be done synchronously in the 
	 * context of the biods.
	 */
	if (bp->b_flags & B_NFS_ASYNC) { 
		if (bp->b_flags & B_ERROR) {
			if (!(bp->b_flags & B_NFS_RETRY)) {
				bp->b_flags &= ~(B_ERROR|B_ASYNC|B_NFS_ASYNC);
				bp->b_flags |= B_NFS_RETRY;
				bp->b_error = 0;
				bwrite(bp);
				atomicAddInt(&b_unstablecnt, -1);
				return;
			} 
		} else {
			bp->b_flags &= ~(B_NFS_ASYNC|B_NFS_RETRY);
			bp->b_flags |= B_NFS_UNSTABLE; 
			if (!(bp->b_flags & B_STALE))
				chunkhold(bp);
			else 
				chunkcommit(bp, 1);
			return;
		}
	}

	/*
	 * Want to catch VOP_COMMIT errors so as to resend the buffers to
	 * the crashed server.
	 */
	if ((bp->b_flags & (B_NFS_UNSTABLE|B_ERROR)) ==
					(B_NFS_UNSTABLE|B_ERROR)) {
		bp->b_flags &= ~(B_ERROR|B_ASYNC|B_NFS_UNSTABLE);
		bp->b_error = 0;
		bp->b_start = lbolt;
		bwrite(bp);
		atomicAddInt(&b_unstablecnt, -1);
		return;
	}

	/*
	 * If it's a delalloc buffer then clean up its delalloc state.
	 * Otherwise, if it is on the delayed write list pull it off.
	 */
	if (bp->b_flags & B_DELALLOC) {
		delalloc_free(bp);
	} else if (bp->b_flags & B_DACCT) {
		ASSERT(bp->b_dforw != bp);
		dchunkunchain(bp);
	}

	if (bp->b_flags & B_VDBUF) {
		ASSERT(bp->b_vp->v_dbuf > 0);
		bp->b_flags &= ~B_VDBUF;
		atomicAddInt(&bp->b_vp->v_dbuf, -1);
	}

	i = btoct(bp->b_bufsize);
	ASSERT(i == btoc(bp->b_bufsize));

	ASSERT(flags & B_PAGEIO);

	if (bp->b_pages) {
		atomicAddInt(&chunkpages, -i);
		pagesrelease(bp->b_pages, i, flags);
	}
	chunkunhash(bp);
	bp->b_pages = (struct pfdat *)0;

	/*
	 * kvmem freed in putphysbuf...
	 */
	putphysbuf(bp);
}


/*
 * chunkhold -- chunk release function in which buffer is kept.
 * Manages delwri buffer list and marks pages done if necessary.
 */
void
chunkhold(
	buf_t	*bp)
{
	uint64_t flags = bp->b_flags;
	int	s;
	int	listid;
	buf_t	*freelist;

	ASSERT((bp->b_flags & (B_DONE|B_BUSY|B_PAGEIO)) ==
	       (B_DONE|B_BUSY|B_PAGEIO) &&
	       (valusema(&bp->b_lock) <= 0));

	buftrace("CHUNKHOLD", bp);
	if ((flags & (B_ERROR|B_STALE)) || bp->b_pages == NULL) {
		chunkrelse(bp);
		return;
	}

	/*
	 * B_WAKE is set when any of the pages weren't done at the
	 * time the buffer was commissioned.
	 */
	if (flags & B_WAKE) {
		int	i = btoct(bp->b_bufsize);
		pfd_t	*pfd;

		ASSERT(i == btoc(bp->b_bufsize));

		for (pfd = bp->b_pages; --i >= 0; pfd = pfd->pf_pchain) {
			pagedone(pfd);
		}

		bp->b_flags &= ~B_WAKE;
	}
#ifdef PGCACHEDEBUG
	else {
		int	i = btoct(bp->b_bufsize);
		pfd_t	*pfd;

		ASSERT(i == btoc(bp->b_bufsize));

		for (pfd = bp->b_pages; --i >= 0; pfd = pfd->pf_pchain) {
			ASSERT(pfd->pf_flags & P_DONE);
			ASSERT(pfd->pf_use > 0);
		}
	}
#endif

	bp->b_relse = chunkrelse;

	if (flags & B_DELALLOC) {
		delalloc_hold(bp);
	} else if (flags & B_DELWRI) {
		if (!(flags & B_DACCT)) {
			/*
			 * This is a newly dirtied buffer which has
			 * not yet been accounted for.  Account for
			 * it in the dirty buffer counters and add
			 * it to the dirty buffer list.
			 */
			int	i = btoct(bp->b_bufsize);
			s = mutex_spinlock((lock_t *)&bfreelist[0].b_fsprivate);
			ASSERT(bp->b_dforw == bp);
			bp->b_dforw = &bfreelist[0];
			bp->b_dback = bfreelist[0].b_dback;
			bp->b_dback->b_dforw = bp;
			bfreelist[0].b_dback = bp;
			mutex_spinunlock((lock_t *)&bfreelist[0].b_fsprivate, s);
			atomicAddInt(&dchunkbufs, 1);
			atomicAddInt(&dchunkpages, i);
			if (!(bp->b_flags & B_VDBUF)) {
				bp->b_flags |= B_VDBUF;
				atomicAddInt(&(bp->b_vp->v_dbuf), 1);
			}
			bp->b_flags |= B_DACCT;
		} else if (!(bp->b_flags & B_VDBUF)) {
			bp->b_flags |= B_VDBUF;
			atomicAddInt(&bp->b_vp->v_dbuf, 1);
		}
	} else if (bp->b_flags & B_DACCT) {
		/*
		 * This was a dirty buffer but it is now clean.  Decrement
		 * the dirty buffer counters and pull it from the dirty
		 * buffer list.
		 */
		ASSERT(bp->b_dforw != bp);
		dchunkunchain(bp);
		if (bp->b_flags & B_VDBUF) {
			bp->b_flags &= ~B_VDBUF;
			ASSERT(bp->b_vp->v_dbuf > 0);
			atomicAddInt(&bp->b_vp->v_dbuf, -1);
		}
	} else if (bp->b_flags & B_VDBUF) {
		bp->b_flags &= ~B_VDBUF;
		ASSERT(bp->b_vp->v_dbuf > 0);
		atomicAddInt(&bp->b_vp->v_dbuf, -1);
	}

	/*
	 * Place the buffer at the end of its free list.
	 */
	listid = bp->b_listid;
	freelist = &bfreelist[listid];
	s = bfree_lock(listid);
	bp->av_back = freelist->av_back;
	bp->av_back->av_forw = bp;
	freelist->av_back = bp;
	bp->av_forw = freelist;
	freelist->b_bcount++;		/* just for fun */

	/*
	 * If someone waiting for a free buffer - get them going.
	 */
	ASSERT(freelist->b_blkno >= 0);
	if (freelist->b_blkno) {
		freelist->b_blkno--;
		bfree_unlock(listid, s);
		vsema(&(freelist->b_iodonesema));
	} else {
		bfree_unlock(listid, s);
	}

	bp->b_flags &= ~(B_BUSY|B_ASYNC|B_AGE|B_BDFLUSH);

	if ((bp->b_flags & B_NFS_ASYNC) && !(bp->b_flags & B_DELWRI)) {
		bp->b_flags &= ~(B_NFS_ASYNC|B_NFS_RETRY);
		bp->b_flags |= B_NFS_UNSTABLE;
	}

	vsema(&bp->b_lock);

	/*
	 * There _could_ be delwri buffers released from interrupt
	 * handlers (e.g., released from patch_release _reading_
	 * asynchronously a buffer with some dirty pages).  Must
	 * avoid dchunkpush-ing since strategy dumps statistics
	 * into the uarea.
	 */
	if ((flags & (B_DELWRI|B_ASYNC)) == B_DELWRI &&
	    (dchunkbufs >= chunk_ebbtide ||
	     dchunkpages >= GLOBAL_FREEMEM())) {
		dchunkpush();
	}


	if ((flags & (B_READ|B_ASYNC)) == B_READ && 
	    (GLOBAL_FREEMEM() <= min_free_pages) &&
            (chunkpages > min_file_pages))
		chunkfree(nbuf/4096);
}

/*
 * chunk_decommission()
 *
 * This routine is called to decommission a buffer which is about
 * to be overlapped by another being created in the chunk cache.
 * If the buffer is not a delayed write buffer, then we call
 * chunkrelse() to decommission the buffer.  If the buffer is a
 * delayed write buffer, then we need to move its pages to the
 * vnode's dirty pages list before decommissioning the buffer with
 * a call to chunkrelse().
 */
static void
chunk_decommission(
	buf_t	*bp,
	uint	flags)		   
{
	int	bp_page_count;
	pfd_t	*pfdp;
	vnode_t	*vp;
	int	s;
	int	dirty_page_count;

	ASSERT(bp->b_flags & B_DONE);

	BUFINFO.decomms++;
	buftrace("CHUNK_DECOMMISSION", bp);
	/*
	 * If the buffer is not dirty, just decommission it.
	 */
	if (!(bp->b_flags & B_DELWRI)) {
		chunkrelse(bp);
		return;
	}

	/*
	 * If the caller specified the BMAP_FLUSH_OVERLAPS flag,
	 * then write the buffer out rather than tearing it appart.
	 */
	if (flags & CD_FLUSH) {
		BUFINFO.flush_decomms++;
		ASSERT(bp->b_flags & B_STALE);
		bp->b_flags &=
			~(B_DELWRI | B_READ | B_ASYNC | B_DONE | B_ERROR);
		VOP_STRATEGY(bp->b_vp, bp);
		(void) biowait(bp);
		chunkrelse(bp);
		return;
	}

	/*
	 * Move all of the dirty buffer's pages to the vnode's
	 * dirty pages list.
	 */
	vp = bp->b_vp;
	bp_page_count = btoc(bp->b_bufsize);
	ASSERT(bp_page_count > 0);
	pfdp = bp->b_pages;
	s = VN_LOCK(vp);
	dirty_page_count = 0;
	while (bp_page_count > 0) {
		ASSERT(pfdp != NULL);
		pageflags(pfdp, P_DIRTY, 1);
		if (!(pfdp->pf_flags & P_DQUEUE)) {
			pfdp->pf_vchain = vp->v_dpages;
			vp->v_dpages = pfdp;
			pageflagsinc(pfdp, P_DQUEUE, 1);
			dirty_page_count++;
		}
		pfdp = pfdp->pf_pchain;
		bp_page_count--;
	}
	if (dirty_page_count) {
		atomicAddInt(&vp->v_vfsp->vfs_dcount, dirty_page_count);
		ADD_PDCOUNT(dirty_page_count);
	}
	vp->v_dpages_gen++;
	VN_UNLOCK(vp, s);

	/*
	 * Toss the buffer.
	 */
	bp->b_flags &= ~B_DELWRI;
	if (bp->b_flags & B_NFS_ASYNC) {
		bp->b_flags &= ~(B_NFS_ASYNC|B_NFS_RETRY);
		atomicAddInt(&b_unstablecnt, -1);
	}
	chunkrelse(bp);
}

/*	Return buffer pointing to data at [offset,offset+count).
**	Start up readahead on a portion of unfulfilled request.
**	Upon return ALL pages are read and done.
**	If BMAP_HOLE | BMAP_DELAY | BMAP_UNWRITTEN is set in 
**	bmap->eof, then the retrieved buffers will be delayed 
**	allocation buffers.
*/
buf_t *
chunkread(
	vnode_t		*vp,
	bmapval_t	*bmap,
	int		nmaps,
	struct cred	*cred)
{
	buf_t	*bp;
	uint64_t bflags;
	uint	delalloc;

	ASSERT(!(nmaps > 1 && (bmap->eof & BMAP_EOF)));

	delalloc = bmap->eof & (BMAP_HOLE | BMAP_DELAY | BMAP_UNWRITTEN);
	if (delalloc) {
		delalloc_reserve();
	}

	bp = fetch_chunk(vp, bmap, cred);

	if (delalloc) {
		delalloc_found(bp, delalloc);
	}
	buftrace("CHUNKREAD", bp);

	bflags = bp->b_flags;
	if (bflags & B_ERROR) {
		return bp;
	}

	SYSINFO.lread += BTOBB(bmap->pbsize);

	if (bflags & B_DONE) {
		while (--nmaps > 0)
			chunkreada(vp, ++bmap, cred);
		return bp;
	}

	ASSERT(!(bp->b_flags & B_ASYNC));

	if (bflags & B_PARTIAL) {
		/*
		 * XXX Should be a way to start up bp's sub-buffers,
		 * XXX and wait for them at this level...
		 * XXX  B_WAIT -- last buffer's interrupt does iodone
		 * XXX	of bp... we biowait here...
		 */
		chunk_patch(bp);
		while (--nmaps > 0)
			chunkreada(vp, ++bmap, cred);
		return bp;
	}

	bp->b_flags |= B_READ;
	VOP_STRATEGY(vp, bp);

	KTOP_UPDATE_CURRENT_INBLOCK(1);
	SYSINFO.bread += BTOBBT(bp->b_bcount);

	while (--nmaps > 0)
		chunkreada(vp, ++bmap, cred);

	(void) biowait(bp);

	ASSERT((bp->b_error && (bp->b_flags & B_ERROR)) ||
	       (!bp->b_error && !(bp->b_flags & B_ERROR)));
	ASSERT(bp->b_pages || (bp->b_flags & B_ERROR));

	return bp;
}


/*
 * Chunk readahead routine.
 * If the given bmap has the BMAP_HOLE or BMAP_DELAY flag set, then
 * make sure to do the proper delayed allocation buffer
 * processing to limit the total number of delayed allocation
 * buffers.
 */
static void
chunkreada(
	vnode_t		*vp,
	bmapval_t	*bmap,
	cred_t		*cred)
{
	buf_t 	*bp;
	uint	delalloc;

	/*
	 * Don't deal with buffers which don't align both front and back
	 * on page boundaries for read ahead.  This prevents read ahead
	 * buffers from getting caught up in the ugliness in fetch_chunk().
	 * This is important for avoiding deadlock when the system is
	 * out of memory, because we already have a buffer locked
	 * (in chunkread) and we need to be able to progress without
	 * tripping over that locked buffer. Also, there is no reason
	 * to readahead an unwritten area. For read ahead we simply
	 * don't deal and just continue with the important buffer that
	 * we already have locked in chunkread().
	 */
	if (dpoff(bmap->offset) || dpoff(bmap->offset + bmap->length) ||
	    (bmap->eof & BMAP_UNWRITTEN) || findchunk(vp, bmap->offset, 0))
		return;

	delalloc = bmap->eof & (BMAP_HOLE | BMAP_DELAY);
	if (delalloc) {
		delalloc_reserve();
	}

	/*
	 * Use the BMAP_READAHEAD flag to inform fetch_chunk() that it is
	 * OK to fail if we are running out of memory.
	 */
	ASSERT(!(bmap->eof & BMAP_READAHEAD));
	bmap->eof |= BMAP_READAHEAD;
	bp = fetch_chunk(vp, bmap, cred);
	bmap->eof &= ~BMAP_READAHEAD;

	if (delalloc) {
		delalloc_found(bp, delalloc);
	}

	/*
	 * If the fetch_chunk() failed because we are running out of
	 * memory then just forget about the read ahead.
	 */
	if (bp == NULL) {
		return;
	}

	buftrace("CHUNKREADA", bp);

	if (bp->b_flags & B_DONE) {
		chunkhold(bp);
	} else {
		bp->b_flags |= B_ASYNC | B_READ;

		if (bp->b_flags & B_PARTIAL) {
			chunk_patch(bp);
		} else {
			VOP_STRATEGY(vp, bp);
			KTOP_UPDATE_CURRENT_INBLOCK(1);
			SYSINFO.bread += BTOBB(bp->b_bcount);
		}
	}
}

/*
 * This is for use by a caller that acquired a buffer via getchunk()
 * but later realize that the buffer needs to be read from the file
 * system.  Those callers use this interface rather than having
 * knowledge of chunk_patch().
 */
buf_t *
chunkreread(buf_t *bp)
{

	ASSERT(bp->b_flags & B_BUSY);
	ASSERT(bp->b_flags & B_PAGEIO);

	if (bp->b_flags & B_DONE) {
		return bp;
	}

	SYSINFO.lread += BTOBB(bp->b_bcount);
	ASSERT(!(bp->b_flags & B_ASYNC));

	if (bp->b_flags & B_PARTIAL) {
		chunk_patch(bp);
		return bp;
	}

	bp->b_flags |= B_READ;
	VOP_STRATEGY(bp->b_vp, bp);

	KTOP_UPDATE_CURRENT_INBLOCK(1);
	SYSINFO.bread += BTOBBT(bp->b_bcount);

	(void) biowait(bp);

	ASSERT((bp->b_error && (bp->b_flags & B_ERROR)) ||
	       (!bp->b_error && !(bp->b_flags & B_ERROR)));

	return bp;
}

/*
 * Read in a chunk buffer.  Discern whether buffer needs to be patched.
 */
static void
cread(buf_t *bp, struct bmapval *bmap)
{
	uint64_t flags = bp->b_flags;

	ASSERT((flags & (B_DONE|B_ASYNC)) == 0);
	BUFINFO.getfrag++;

	/*
	 *	B_UNINITIAL flags the buffer as covering XFS allocated
	 *	but uninitialized data.
	 */
	if (bmap->eof & BMAP_UNWRITTEN)
		bp->b_flags |= B_UNINITIAL;

	/*
	 *	B_DELWRI && !B_DONE implies that there are some
	 *	P_DIRTY pages amongst uninitialized ones.
	 *	Patch up the buffer by reading in just the
	 *	ininitialized pages.
	 */
	if (flags & B_PARTIAL) {
		if (flags & B_WAIT)
			bp->b_flags &= ~B_WAIT;
		chunk_patch(bp);
		return;
	}

	bp->b_flags |= B_READ;
	VOP_STRATEGY(bp->b_vp, bp);

	KTOP_UPDATE_CURRENT_INBLOCK(1);
	SYSINFO.bread += BTOBBT(bp->b_bcount);

	/*
	 * B_WAIT implies that caller will do the waiting -- caller may
	 * want to have several pseudo-synchronous i/o requests outstanding.
	 */
	if (!(flags & B_WAIT)) {
		(void) biowait(bp);

		ASSERT((bp->b_error && (bp->b_flags & B_ERROR)) ||
		       (!bp->b_error && !(bp->b_flags & B_ERROR)));
	}
}

/*
 * Buffers from bhead to btail have been locked while
 * collecting an equivalence class of fragmented buffers.
 * Release the buffers after, possibly, biowait-ing for them.
 */
static void
release(
	buf_t *bhead,
	buf_t *btail,
	buf_t *bp)
{
	buf_t	*bnext;
	vnode_t	*vp;

	vp = bhead->b_vp;
	while (bhead) {
		/*
		 * Find next buffer in the chain and save it for
		 * the next loop.  Hold the vnode spin lock while
		 * we walk around the tree.
		 */
		if (bhead != btail) {
#ifdef DEBUG
			off_t nextoff = bhead->b_offset +
						BTOBBT(bhead->b_bcount);
#endif
			VN_BUF_LOCK(vp, s);
			bnext = bhead->b_forw;
			if (bnext) {
				while (bnext->b_back)
					bnext = bnext->b_back;
			} else {
				bnext = bhead->b_parent;
				ASSERT(bnext);
				while (bnext->b_offset < bhead->b_offset) {
					bnext = bnext->b_parent;
					ASSERT(bnext);
				}
				ASSERT(bnext->b_offset == nextoff);
			}
			VN_BUF_UNLOCK(vp, s);
		} else {
			bnext = NULL;
		}

		ASSERT(bhead->b_offset <= btail->b_offset);
		ASSERT(valusema(&bhead->b_lock) <= 0);

		buftrace("RELEASE", bhead);
		if (bhead->b_flags & B_WAIT) {
			ASSERT(valusema(&bhead->b_iodonesema) <= 1);
			biowait(bhead);
			/*
			 * We can't clear B_WAIT until after the iodone()
			 * has woken us up from biowait(), because there
			 * is no locking around the b_flags field which is
			 * modified both here and in iodone().
			 */
			bhead->b_flags &= ~B_WAIT;
		}
		ASSERT(bhead->b_flags & B_DONE);

		/*
		 * bp is the target buffer that started all the trouble --
		 * don't release it.
		 */
		if (bhead != bp)
			chunkhold(bhead);

		bhead = bnext;
	}
}

/*
 * Get buffer that bmap describes, with no i/o implied.
 * If the buffer will exist over a hole, then do the
 * proper delayed allocation processing to limit the
 * number of delalloc buffers.
 */
buf_t *
getchunk(
	vnode_t		*vp,
	bmapval_t	*bmap,
	cred_t		*cred)
{
	buf_t	*bp;
	uint	delalloc;

	/*
	 * If we're acquiring a delayed alloc buffer, first make
	 * sure there is room for it. This will reserve us a space
	 * among the delayed alloc buffers.
	 */
	delalloc = bmap->eof & (BMAP_HOLE | BMAP_DELAY | BMAP_UNWRITTEN);
	if (delalloc) {
		delalloc_reserve();
	}

	bp = fetch_chunk(vp, bmap, cred);

	if (delalloc) {
		delalloc_found(bp, delalloc);
	}

	buftrace("GETCHUNK", bp);

	return bp;
}

/*
 * This subroutine of fetch_chunk() determines whether it is worth
 * trying to allocate large pages for the buffer mapped by bmap.
 * Currently it makes its decision based on whether there are already
 * pages in the range mapped by the bmap (extended out to the large
 * page boundaries) or not.
 */
size_t
fetch_chunk_lpage_scan(
	vnode_t		*vp,
	bmapval_t	*bmap,
	size_t		pm_page_size)
{
	pfd_t	*pfd;
	off_t	lpn;
	off_t	end_lpn;

	/*
	 * Convert the start offset (in 512 blocks) to the lpn of
	 * the start of the large page.
	 */
	lpn = offtoct(LPAGE_ALIGN(BBTOOFF(bmap->offset), pm_page_size));

	/*
	 * Convert the end of the buffer to the lpn of the base page
	 * after the large page encompassing this buffer.
	 */
	end_lpn = offtoct(LPAGE_ALIGN_UP(BBTOOFF(bmap->offset + bmap->length),
					 pm_page_size));
	
	/*
	 * If there are already any pages in the range, return a
	 * page size of NBPP.  Otherwise return the input pm_page_size
	 * so that we'll try to allocate the large pages.
	 */
	ASSERT(lpn < end_lpn);
	while (lpn < end_lpn) {
		pfd = vnode_pfind(vp, lpn, 0);
		if (pfd != NULL) {
			return NBPP;
		}
		lpn++;
	}
	return pm_page_size;
}


/*
 * This is simply a subroutine for fetch_chunk().  It is
 * used to abort the creation of a chunk buffer.
 */
void
fetch_chunk_destroy(
	buf_t	*bp,
	pfd_t	*pfdp)
{
	pfd_t	*tpfd;
	int	i;

	buftrace("FETCH_CHUNK DESTROY", bp);
	i = btoct(bp->b_bufsize);
	atomicAddInt(&chunkpages, -i);

	while (--i >= 0) {
		ASSERT(pfdp);
		ASSERT(!(pfdp->pf_flags & P_DIRTY) ||
		       (pfdp->pf_use > 1));
		tpfd = pfdp->pf_pchain;
		pagefree(pfdp);
		pfdp = tpfd;
	}

	chunkunhash(bp);
	bp->b_pages = (struct pfdat *)0;
	putphysbuf(bp);
}

/*
 * This subroutine of fetch_chunk() is called to free the references
 * taken by gather_chunk() on pages outside the range of the buffer
 * requested when it allocates large pages.
 */
void
fetch_chunk_lpage_freerefs(
	vnode_t		*vp,
	bmapval_t	*bmap,
	size_t		start_page_size,
	size_t		end_page_size,
	int		gc_flags)
{
	off_t		stop_pg;
	off_t		offset_b;
	off_t		lpn;
	pfd_t		*pfd;
	off_t		last_bb;

	/*
	 * If we have not already freed the pages in gather_chunk_lpage_insert,
	 * first free all the base pages from the start of the starting
	 * large page to the first base page in the buffer.
	 *
	 * We don't have to attach to the pages in our calls to vnode_pfind()
	 * in either of the loops below because we know that we already
	 * have references to the pages for which we're searching.
	 */
	if (!(gc_flags & GC_FIRST_PAGEFREED)) {
		offset_b = BBTOOFF(bmap->offset);
		lpn = offtoct(LPAGE_ALIGN(offset_b, start_page_size));
		stop_pg = dtopt(bmap->offset);
		while (lpn < stop_pg) {
			pfd = vnode_pfind(vp, lpn, 0);
			ASSERT(pfd->pf_use > 0);
			pagefree(pfd);
			lpn++;
		}
	}

	/*
	 * If we have not already freed the pages in gather_chunk_lpage_insert,
	 * free all of the base pages from the one following the last base
	 * page of the buffer to the end of the ending large page.
	 */
	if (!(gc_flags & GC_LAST_PAGEFREED)) {
		last_bb = bmap->offset + bmap->length;
		lpn = dtop(last_bb);
		offset_b = BBTOOFF(last_bb);
		stop_pg = offtoct(LPAGE_ALIGN_UP(offset_b, end_page_size));
		while (lpn < stop_pg) {
			pfd = vnode_pfind(vp, lpn, 0);
			ASSERT(pfd->pf_use > 0);
			pagefree(pfd);
			lpn++;
		}
	}
}


/*
 * Get the buffer that bmap describes, with no i/o implied.
 * This routine's primary responsibility is to check
 * for and manage buffers which are not aligned on page
 * boundaries.  These can be normal or large page boundaries.
 *
 * The primary issue with unaligned buffers is that we cannot
 * release the buffer and thus mark its constituent pages
 * P_DONE until the entire range of the pages is initialized.
 * Fetch_chunk() builds up the buffers needed to entirely map
 * the pages mapped by the given bmap so that the pages can
 * be initialized completely.
 */
buf_t *
fetch_chunk(
	vnode_t		*vp,
	bmapval_t	*bmap,
	cred_t		*cred)
{
	buf_t		*bp;
	buf_t		*lbp;
	buf_t		*bhead;
	buf_t		*btail;
	pfd_t		*pfd;
	int		i;
	bmapval_t	*lmap;
	off_t		offset_b;
	off_t		offset_bb;
	off_t		stop_offset_b;
	off_t		stop_offset_bb;
	int		error;
	int		nmaps;
	uint		delalloc;
	off_t		last_offset_bb;
	int		last_length_bb;
	int		diff_b;
	int		diff_bb;
	int		gc_flags;
	int		inner_gc_flags;
	int		first_time;
	size_t		start_page_size;
	size_t		end_page_size;
	size_t		first_start_page_size;
	size_t		first_end_page_size;
	int		first_gc_flags;
	size_t		inner_page_size;
	struct pm	*pmp;
#define NGETMAPS 2
	bmapval_t	lbmaps[NGETMAPS];
	int		cleared_dontalloc = 0;

	ASSERT(vp->v_type == VREG);
	pmp = bmap->pmp;
	first_time = 1;
	
startover:
	/*
	 * On the first time we do this, check to see if we should do
	 * large page allocations.  On subsequent loops we never use
	 * large pages, because we should have allocated any that we
	 * want the first time around.
	 *
	 * We use start_page_size as both an input and output parameter
	 * of gather_chunk().
	 */
	if ((pmp != NULL) && (first_time)) {
		start_page_size = PM_GET_PAGESIZE(pmp);
		if (start_page_size != NBPP) {
			ASSERT(start_page_size > NBPP);
			start_page_size = fetch_chunk_lpage_scan(vp, bmap,
						 start_page_size);
		}
	} else {
		start_page_size = NBPP;
	}
	/*
	 * Ensure that BMAP_DONTALLOC only gets passed down iff we'll
	 * be taking the short path through fetch_chunk() and we won't
	 * need to reference bp->b_pages.
	 */
	if ((bmap->eof & BMAP_DONTALLOC) &&
	    (pmp || LPAGE_OFFSET(BBTOOFF(bmap->offset), NBPP) ||
	     ((LPAGE_OFFSET(BBTOOFF(bmap->offset) + BBTOOFF(bmap->length), NBPP)) &&
	      (bmap->eof & BMAP_EOF) == 0)))
	{
		cleared_dontalloc = 1;
		bmap->eof ^= BMAP_DONTALLOC;
	}

	bp = gather_chunk(vp, bmap, &gc_flags,
			  &start_page_size, &end_page_size);

	ASSERT(bp->b_pages || (gc_flags & GC_PDCLUSTER) == 0);

	if (cleared_dontalloc)
		bmap->eof |= BMAP_DONTALLOC;

	if (first_time) {
		first_start_page_size = start_page_size;
		first_end_page_size = end_page_size;
		first_gc_flags = gc_flags;
		first_time = 0;
	}
	if (bp->b_flags & (B_ERROR|B_DONE)) {
		ASSERT((start_page_size == NBPP) && (end_page_size == NBPP));
		goto cluster_return;
	}

	/*
	 * If the buffer doesn't begin on a page boundary,
	 * either 1) make sure that the page has already
	 * been read in, or 2) get it in.  Once the page
	 * has been read in, it can't go away since bp holds
	 * a reference to it.
	 *
	 * The problem is, of course, that the page(s) mapped
	 * by bp are marked P_DONE when the i/o is complete,
	 * and a partially-read page can't be P_DONE'd.
	 */

	offset_bb = bmap->offset;
	offset_b = BBTOOFF(offset_bb);
	pfd = bp->b_pages;
	if ((LPAGE_OFFSET(offset_b, start_page_size) != 0) &&
	    (!(pfd->pf_flags & P_DONE) || (start_page_size != NBPP))) {
		/*
		 * First, get rid of the buffer.  Another process
		 * could be trying to get the same set of fragmented
		 * buffers read in, and may want to acquire bp.
		 */
		fetch_chunk_destroy(bp, pfd);
		bp = NULL;

		/*
		 * Now simply round down to a page boundary and
		 * use that as the starting point for creating
		 * buffers to initialize the pages we need for
		 * the requested buffer.  Set the end point to
		 * the start of the first page not mapped by
		 * the buffer.
		 */
		stop_offset_b =
			LPAGE_ALIGN_UP((offset_b + BBTOOFF(bmap->length)),
				       end_page_size);
		stop_offset_bb = OFFTOBBT(stop_offset_b);
		offset_b = LPAGE_ALIGN(offset_b, start_page_size);
		offset_bb = OFFTOBBT(offset_b);
		bhead = NULL;
		btail = NULL;
	} else {
		/*
		 * We were OK on the front end of the buffer,
		 * so check for end-of-mapping page fragments.
		 * If we're not properly aligned at the end,
		 * then we'll use the loop below to map forward
		 * to the end of the last page in our buffer.
		 */
		offset_bb += bmap->length;
		offset_b += BBTOOFF(bmap->length);

		/*
		 * If we're page aligned or this buffer overlaps
		 * the end of the file, then we're OK.
		 */
		if ((LPAGE_OFFSET(offset_b, end_page_size) == 0) ||
		    (bmap->eof & BMAP_EOF)) {
			goto cluster_return;
		}
	
		/*
		 * If the last page in the buffer is already initialized
		 * and there aren't any pages after the last one in the
		 * buffer that we need to worry about, then we're OK.
		 * If the buffer ends in the last base page of a large
		 * page then end_page_size will be set to NBPP (see
		 * gather_chunk_lpage_insert()) so we'll handle that
		 * case as well.
		 */
		if (end_page_size == NBPP) {
			i = btoct(bp->b_bufsize);
			while (--i > 0) {
				pfd = pfd->pf_pchain;
			}
			if (pfd->pf_flags & P_DONE) {
				goto cluster_return;
			}
		}

		/*
		 * We need to initialize the entire last page and we
		 * only cover part of it, so start reading our part
		 * asynchronously and let the loop below read in the
		 * rest of it.  Set stop_offset so that we only read
		 * in the remainder of this last page.
		 */
		ASSERT(!(bp->b_flags & (B_DONE|B_WAIT)));
		bp->b_flags |= B_WAIT;
		cread(bp, bmap);

		stop_offset_b = LPAGE_ALIGN_UP(offset_b, end_page_size);
		stop_offset_bb = OFFTOBBT(stop_offset_b);
		bhead = bp;
		btail = bp;
		buftrace("FETCH_CHUNK MIDDLE", bp);
	}
	/*
	 * At this point, offset is set to the point at which
	 * we should start mapping forward until stop_offset.
	 * Set last_offset and last_length such that if
	 * the file system gives back a mapping that extends
	 * back below offset we trim it to the offset boundary.
	 */
	last_offset_bb = offset_bb;
	last_length_bb = 0;

	/*
	 * Put together buffers mapping from the current value
	 * of offset until we reach stop_offset.  Trim the
	 * buffer mappings so that we don't extend outside the
	 * offset, stop_offset range.  We acquire the buffers
	 * from low to high offsets so that we don't deadlock
	 * with anyone else in this code.  All of the buffers
	 * are read in asynchronously, and we wait for them in
	 * the call to release() after the loop.  The bhead and
	 * btail pointers keep track of the range of buffers to
	 * wait for.
	 *
	 * All of the calls to gather_chunk() set the page sizes
	 * to NBPP.  This is because we expect to be doing no
	 * page allocations in this path so we want to stay in the
	 * simple code paths.
	 */
	inner_page_size = NBPP;
	for ( ; ; ) {
		/*
		 * We pass in NGETMAPS at a time with the hope
		 * that this will lower the number of VOP_BMAP
		 * calls that we make.
		 */
		nmaps = NGETMAPS;
		lmap = &lbmaps[0];
		VOP_BMAP(vp, BBTOOFF(offset_bb), 1,
			      B_READ, cred, lmap, &nmaps, error);

		if (error) {
			goto out;
		}

		do {
			if (lmap->length == 0) {
				goto out;
			}
			/*
			 * Make sure that the buffer we're going to
			 * grab doesn't overlap any of the previous
			 * buffers we've grabbed and kept locked.
			 * If it does overlap, then trim the mapping
			 * so that it does not.
			 */
			if ((last_offset_bb + last_length_bb) > lmap->offset) {
				diff_bb = (last_offset_bb + last_length_bb) -
					  lmap->offset;
				lmap->offset += diff_bb;
				lmap->length -= diff_bb;
				ASSERT(lmap->length > 0);
				diff_b = BBTOB(diff_bb);
				lmap->bsize -= diff_b;
				ASSERT(lmap->bsize > 0);
#ifdef DEBUG
				if (lmap == &lbmaps[0]) {
					lmap->pboff -= diff_b;
					ASSERT((lmap->pboff + lmap->pbsize) <=
					       lmap->bsize);
				}
#endif
				if (lmap->bn != (daddr_t)-1) {
					lmap->bn += diff_bb;
				}
			}
			/*
			 * Now make sure that the buffer doesn't
			 * extend beyond stop_offset.  Trim back
			 * the mapping if it does.
			 */
			if ((lmap->offset + lmap->length) > stop_offset_bb) {
				diff_bb = (lmap->offset + lmap->length) -
					  stop_offset_bb;
				lmap->length -= diff_bb;
				ASSERT(lmap->length > 0);
				diff_b = BBTOB(diff_bb);
				lmap->bsize -= diff_b;
				ASSERT(lmap->bsize > 0);
#ifdef DEBUG
				if (lmap == &lbmaps[0]) {
					if (lmap->pboff + lmap->pbsize >
					    lmap->bsize) {
						lmap->pbsize =
							lmap->bsize -
							lmap->pboff;
						ASSERT(lmap->pbsize > 0);
					}
				}
#endif
				/*
				 * Since we've trimmed back the buffer
				 * we can't be sure that we're covering
				 * the end of the file anymore, so clear
				 * that bit if it is set.
				 */
				lmap->eof &= ~BMAP_EOF;
			}

			/*
			 * Save the offset and length for the buffer
			 * we're going to grab for use the next time
			 * around.
			 */
			last_offset_bb = lmap->offset;
			last_length_bb = lmap->length;

			/*
			 * Use the Policy Module we were given for the
			 * buffers we create mappings for ourselves.
			 */
			lmap->pmp = pmp;

			/*
			 * We have to do the delalloc thing since
			 * we're creating buffers.  Only skip it if
			 * the the mapping is the same as
			 * that requested by the caller.  We skip it
			 * because the caller must have already called
			 * delalloc_reserve() for that buffer and will
			 * call delalloc_found() when we're done.
			 */
			delalloc = lmap->eof & 
                       (BMAP_HOLE | BMAP_DELAY | BMAP_UNWRITTEN);
			if (delalloc &&
			    (!((bmap->offset == lmap->offset) &&
			       (bmap->length == lmap->length)))) {
					delalloc_reserve();
					lbp = gather_chunk(vp, lmap,
						&inner_gc_flags,
						&inner_page_size,
						&inner_page_size);
					delalloc_found(lbp, delalloc);
			} else {
				lbp = gather_chunk(vp, lmap,
						   &inner_gc_flags,
						   &inner_page_size,
						   &inner_page_size);
			}
			buftrace("FETCH_CHUNK GATHERED", lbp);

			/*
			 * If the buffer had pages on the vnode's dirty
			 * pages list, then pull them off now.
			 */
			if (inner_gc_flags & GC_PDCLUSTER) {
				(void)pdcluster(lbp, ((lmap->eof & BMAP_EOF) ?
						      PDC_EOF : 0));
			}

			/*
			 * If lbp is not done yet, then start reading
			 * it in.
			 */
			if (!(lbp->b_flags & B_DONE)) {
				ASSERT(!(lbp->b_flags & (B_WAIT|B_ASYNC)));
				lbp->b_flags |= B_WAIT;
				cread(lbp, lmap);
			}

			/*
			 * We can't release lbp until all of the buffers
			 * we're pulling in are completely read in.
			 * This is so that none of the buffers are marked
			 * done and made accessible until all of the
			 * pages are fully initialized.
			 *
			 * If lbp matches the request made by our caller
			 * we save it for later in 'bp'.  Otherwise
			 * 'bp' will be null and we'll start over to
			 * fetch what was wanted at the end of this routine.
			 */
			if ((bp == NULL) &&
			    (offset_bb == bmap->offset) &&
			    (lmap->length == bmap->length)) {
				bp = lbp;
			}

			/*
			 * Update the range of buffers we're pulling in.
			 */
			if (bhead == NULL) {
				bhead = lbp;
			}
			btail = lbp;
			ASSERT((lbp->b_flags & (B_WAIT | B_DONE)) != 0);
			ASSERT((bhead->b_flags & (B_WAIT | B_DONE)) != 0);
			ASSERT(valusema(&lbp->b_iodonesema) <= 1);

			/*
			 * If this buffer maps the end of the file or
			 * we've made it to stop_offset, then we're
			 * done.
			 */
			if (lmap->eof & BMAP_EOF) {
				goto out;
			}
			offset_bb += lmap->length;
			if (offset_bb >= stop_offset_bb) {
				ASSERT(offset_bb == stop_offset_bb);
				goto out;
			}

			lmap++;

		} while (--nmaps > 0);
	}
out:
	/*
	 * Now call release() to unlock the accumulated buffers.
	 * This does a biowait() on each buffer for which B_WAIT is set.
	 * This waits for the completion of the asynchronously issued,
	 * but not B_ASYNC, buffers from above (see cread() calls).
	 */
	if (bhead != NULL) {
		ASSERT(btail != NULL);
		ASSERT((bp == NULL) || (bp->b_offset >= bhead->b_offset));
		ASSERT((bp == NULL) || (bp->b_offset <= btail->b_offset));
		release(bhead, btail, bp);
	}

	if (error) {
		/*
		 * We've encountered an error, so retrieve the
		 * buffer we started out looking for (if we
		 * don't already have it) and mark it in error.
		 * If it happens to be marked done when we retrieve
		 * it, then someone else managed to get it in
		 * and we can just return it.
		 */
		if (bp == NULL) {
			bp = gather_chunk(vp, bmap, &inner_gc_flags,
					  &inner_page_size, &inner_page_size);
			if (!(bp->b_flags & B_DONE)) {
				bp->b_flags |= B_ERROR;
				bp->b_error = error;
			}
		} else {
			bp->b_flags |= B_ERROR;
			bp->b_error = error;
		}
	} else if (bp == NULL) {
		/*
		 * If we never did re-encounter that buffer we
		 * decommissioned (because the file's mappings
		 * must have changed out from under us) then
		 * just start all over again.
		 */
		goto startover;
	}

 cluster_return:
	if ((first_start_page_size != NBPP) ||
	    (first_end_page_size != NBPP)) {
		fetch_chunk_lpage_freerefs(vp, bmap,
					   first_start_page_size,
					   first_end_page_size,
					   first_gc_flags);
	}
	/*
	 * If gather_chunk() suggested that we call pdcluster() for
	 * the buffer it returned, then do so now.  We may not have been
	 * able to do it at first, because pulling the pages from the
	 * dirty queue could make us the last reference to the pages so
	 * we wouldn't be able to decommission our buffer.
	 */
	if (gc_flags & GC_PDCLUSTER) {
		(void) pdcluster(bp, ((bmap->eof & BMAP_EOF) ? PDC_EOF : 0));
	}

	return bp;
}

/*
 * Lock free list and remove bp.
 */
static void
notavailchunk(
	buf_t	*bp)
{
	buf_t	*freelist;
	buf_t	*forw;
	buf_t	*back;
	int	s;	
	int	listid;

	listid = bp->b_listid;
	freelist = &bfreelist[listid];

	s = bfree_lock(listid);
	forw = bp->av_forw;
	back = bp->av_back;
	forw->av_back = back;
	back->av_forw = forw;

	ASSERT(!(bp->b_flags & B_BUSY));
	ASSERT(freelist->b_bcount > 0);
	bp->b_flags |= B_BUSY;
	ASSERT(spinlock_islocked((lock_t*)&bfreelist[listid].b_private));
	freelist->b_bcount--;
	bfreelist_check(freelist);
	bfree_unlock(listid, s);
}

#ifndef AVLDEBUG
#define chunk_check(V)
#else
void
chunk_certify(buf_t *bp)
{
	buf_t *back = bp->b_back;
	buf_t *forw = bp->b_forw;
	int bal = bp->b_balance;

	ASSERT(bal != AVL_BALANCE || (!back && !forw) || (back && forw));
	ASSERT(bal != AVL_FORW || forw);
	ASSERT(bal != AVL_BACK || back);

	if (forw) {
		ASSERT(bp->b_offset < forw->b_offset);
		ASSERT(bp->b_forw->b_parent == bp);
		ASSERT(back || bal == AVL_FORW);
	} else {
		ASSERT(bal != AVL_FORW);
		ASSERT(bal == AVL_BALANCE || back);
		ASSERT(bal == AVL_BACK || !back);
	}

	if (back) {
		ASSERT(bp->b_offset > back->b_offset);
		ASSERT(bp->b_back->b_parent == bp);
		ASSERT(forw || bal == AVL_BACK);
	} else {
		ASSERT(bal != AVL_BACK);
		ASSERT(bal == AVL_BALANCE || forw);
		ASSERT(bal == AVL_FORW || !forw);
	}
}

void
chunk_check(vnode_t *vp)
{
	buf_t *blast, *bnext, *bp;
	off_t offset = 0;
	off_t end;

	blast = bnext = vp->v_buf;

	ASSERT(!bnext || bnext->b_parent == NULL);

	while (bnext) {

		chunk_certify(bnext);
		end = bnext->b_offset + BTOBBT(bnext->b_bcount);

		if (end <= offset) {
			if ((bp = bnext->b_forw) && bp != blast) {
				blast = bnext;
				bnext = bp;
			} else {
				blast = bnext;
				bnext = bnext->b_parent;
			}
			continue;
		}

		blast = bnext;
		if (bp = bnext->b_back) {
			if (bp->b_offset + BTOBBT(bp->b_bcount) > offset) {
				bnext = bp;
				continue;
			}
		}

		bp = bnext;
		bnext = bnext->b_forw;
		if (!bnext)
			bnext = bp->b_parent;

		offset = end;
	}
}
#endif

/*
 * Reset balance for bp up through tree.
 * ``direction'' is the way that bp's balance
 * is headed after the deletion of one of its children --
 * e.g., deleting a b_forw child sends b_balance toward AVL_BACK.
 * Called only when unhashing a buffer from the tree.
 */
static void
retreat(buf_t *bp, int direction)
{
	buf_t *parent;
	buf_t *child;
	buf_t *tmp;
	int	bal;

	do {
		ASSERT(direction == AVL_BACK || direction == AVL_FORW);

		if (bp->b_balance == AVL_BALANCE) {
			bp->b_balance = direction;
			return;
		}

		parent = bp->b_parent;

		/*
		 * If balance is being restored, no local node
		 * reorganization is necessary, but may be at
		 * a higher node.  Reset direction and continue.
		 */
		if (direction != bp->b_balance) {
			bp->b_balance = AVL_BALANCE;
			if (parent) {
				if (parent->b_forw == bp)
					direction = AVL_BACK;
				else
					direction = AVL_FORW;

				bp = parent;
				continue;
			}
			return;
		}

		/*
		 * Imbalance.  If a b_forw node was removed, direction
		 * (and, by reduction, bp->b_balance) is/was AVL_BACK.
		 */
		BUFINFO.drotates++;
		if (bp->b_balance == AVL_BACK) {

			ASSERT(direction == AVL_BACK);
			child = bp->b_back;
			bal = child->b_balance;

			if (bal != AVL_FORW) /* single LL */ {
				/*
				 * bp gets pushed down to lesser child's
				 * b_forw branch.
				 *
				 *  bp->    -D 		    +B
				 *	    / \		    / \
				 * child-> B   deleted	   A  -D
				 *	  / \		      /
				 *	 A   C		     C
				cmn_err(CE_CONT, "!LL delete b 0x%x c 0x%x\n",
					bp, child);
				 */

				bp->b_back = child->b_forw;
				if (child->b_forw)
					child->b_forw->b_parent = bp;
				child->b_forw = bp;

				if (parent) {
					if (parent->b_forw == bp) {
						parent->b_forw = child;
						direction = AVL_BACK;
					} else {
						ASSERT(parent->b_back == bp);
						parent->b_back = child;
						direction = AVL_FORW;
					}
				} else {
					ASSERT(bp->b_vp->v_buf == bp);
					bp->b_vp->v_buf = child;
				}
				bp->b_parent = child;
				child->b_parent = parent;

				if (bal == AVL_BALANCE) {
					bp->b_balance = AVL_BACK;
					child->b_balance = AVL_FORW;
					return;
				} else {
					bp->b_balance = AVL_BALANCE;
					child->b_balance = AVL_BALANCE;
					bp = parent;
					chunk_check(child->b_vp);
					continue;
				}
			}

			/* child->b_balance == AVL_FORW  double LR rotation
			 *
			 * child's b_forw node gets promoted up, along with
			 * its b_forw subtree
			 *
			 *  bp->     -G 		  C
			 *	     / \		 / \
			 * child-> +B   H	       -B   G
			 *	   / \   \	       /   / \
			 *	  A  +C   deleted     A   D   H
			 *	       \
			 *	        D
			cmn_err(CE_CONT, "!LR delete b 0x%x c 0x%x t 0x%x\n",
				bp, child, child->b_forw);
			 */

			tmp = child->b_forw;
			bal = tmp->b_balance;

			child->b_forw = tmp->b_back;
			if (tmp->b_back)
				tmp->b_back->b_parent = child;

			tmp->b_back = child;
			child->b_parent = tmp;

			bp->b_back = tmp->b_forw;
			if (tmp->b_forw)
				tmp->b_forw->b_parent = bp;
			tmp->b_forw = bp;

			if (bal == AVL_FORW)
				child->b_balance = AVL_BACK;
			else
				child->b_balance = AVL_BALANCE;

			if (bal == AVL_BACK)
				bp->b_balance = AVL_FORW;
			else
				bp->b_balance = AVL_BALANCE;

			goto next;
		}

		ASSERT(bp->b_balance == AVL_FORW && direction == AVL_FORW);

		child = bp->b_forw;
		bal = child->b_balance;

		if (bal != AVL_BACK) /* single RR */ {
			/*
			 * bp gets pushed down to greater child's
			 * b_back branch.
			 *
			 *  bp->    +B 		     -D
			 *	    / \		     / \
			 *   deleted   D <-child   +B   E
			 *	      / \	     \
			 *	     C   E	      C
			cmn_err(CE_CONT, "!RR delete b 0x%x c 0x%x\n",
				bp, child);
			 */

			bp->b_forw = child->b_back;
			if (child->b_back)
				child->b_back->b_parent = bp;
			child->b_back = bp;

			if (parent) {
				if (parent->b_forw == bp) {
					parent->b_forw = child;
					direction = AVL_BACK;
				} else {
					ASSERT(parent->b_back == bp);
					parent->b_back = child;
					direction = AVL_FORW;
				}
			} else {
				ASSERT(bp->b_vp->v_buf == bp);
				bp->b_vp->v_buf = child;
			}
			bp->b_parent = child;
			child->b_parent = parent;

			if (bal == AVL_BALANCE) {
				bp->b_balance = AVL_FORW;
				child->b_balance = AVL_BACK;
				return;
			} else {
				bp->b_balance = AVL_BALANCE;
				child->b_balance = AVL_BALANCE;
				bp = parent;
				chunk_check(child->b_vp);
				continue;
			}
		}

		/* child->b_balance == AVL_BACK  double RL rotation
		cmn_err(CE_CONT, "!RL delete b 0x%x c 0x%x t 0x%x\n",
			bp, child, child->b_back);
		*/

		tmp = child->b_back;
		bal = tmp->b_balance;

		child->b_back = tmp->b_forw;
		if (tmp->b_forw)
			tmp->b_forw->b_parent = child;

		tmp->b_forw = child;
		child->b_parent = tmp;

		bp->b_forw = tmp->b_back;
		if (tmp->b_back)
			tmp->b_back->b_parent = bp;
		tmp->b_back = bp;

		if (bal == AVL_BACK)
			child->b_balance = AVL_FORW;
		else
			child->b_balance = AVL_BALANCE;

		if (bal == AVL_FORW)
			bp->b_balance = AVL_BACK;
		else
			bp->b_balance = AVL_BALANCE;
next:
		bp->b_parent = tmp;
		tmp->b_balance = AVL_BALANCE;
		tmp->b_parent = parent;

		if (parent) {
			if (parent->b_forw == bp) {
				parent->b_forw = tmp;
				direction = AVL_BACK;
			} else {
				ASSERT(parent->b_back == bp);
				parent->b_back = tmp;
				direction = AVL_FORW;
			}
		} else {
			ASSERT(bp->b_vp->v_buf == bp);
			bp->b_vp->v_buf = tmp;
			return;
		}

		bp = parent;
		chunk_check(tmp->b_vp);
	} while (bp);
}

/*
 * Unhash buffer from its vnode/buffer tree.
 * chunkunhash does the local tree manipulations,
 * calls retreat() to rebalance tree up to its root.
 *
 * Clear bp->b_vp before dropping the vnode lock.
 * This prevents races between here and nextchunk(),
 * since it notifies nextchunk() that this buffer
 * is no longer in the AVL tree.
 */
static void
chunkunhash(buf_t *bp)
{
	vnode_t *vp = bp->b_vp;
	buf_t *forw;
	buf_t *back;
	buf_t *parent;

	VN_BUF_LOCK(vp, s);
	forw = bp->b_forw;
	back = bp->b_back;
	parent = bp->b_parent;

	/*
	 * bufgen must be incremented on both insertions and deletions
	 * because bp_insert() keeps a pointer to the potential parent
	 * of the buffer to be inserted after releasing the vnode lock
	 * to allocate the actual buffer.  With a deletion, the parent
	 * buffer could go away, or (more difficult to detect) the
	 * logical parent of the buffer to be inserted could change.
	 * So any change in the vnode/buffer tree will force bp_insert()
	 * to retraverse the tree after allocating the to-be-inserted buffer.
	 */
	vp->v_bufgen++;

	chunk_check(vp);

	/*
	 * Degenerate cases...
	 */
	if (forw == NULL) {
		forw = back;
		goto attach;
	}

	if (back == NULL) {
attach:
		if (forw)
			forw->b_parent = parent;
		if (parent) {
			if (parent->b_forw == bp) {
				parent->b_forw = forw;
				retreat(parent, AVL_BACK);
			} else {
				ASSERT(parent->b_back == bp);
				parent->b_back = forw;
				retreat(parent, AVL_FORW);
			}
		} else {
			/*
			 * This _could_ be an orphan buffer created to
			 * pass an error condition up to the caller,
			 * so check that v_buf really points to bp
			 * before assigning it to bp's child.
			 */
			if (vp->v_buf == bp)
				vp->v_buf = forw;
			else
				ASSERT(bp->b_error);
		}
		chunk_check(vp);
		bp->b_vp = NULL;
		VN_BUF_UNLOCK(vp, s);
		return;
	}

	/*
	 * Harder case: children on both sides.
	 * If back's b_forw pointer is null, just have back
	 * inherit bp's b_forw tree, remove bp from the tree
	 * and adjust balance counters starting at back.
	 *
	 * bp->	    xI		    xH	(befor retreat())
	 *	    / \		    / \
	 * back->  H   J	   G   J
	 *	  /   / \             / \
	 *       G   ?   ?           ?   ?
	 *      / \
	 *     ?   ?
	 */
	if ((forw = back->b_forw) == NULL) {
		/*
		 * AVL_FORW retreat below will set back's
		 * balance to AVL_BACK.
		 */
		back->b_balance = bp->b_balance;
		back->b_forw = forw = bp->b_forw;
		forw->b_parent = back;
		back->b_parent = parent;
		
		if (parent) {
			if (parent->b_forw == bp)
				parent->b_forw = back;
			else {
				ASSERT(parent->b_back == bp);
				parent->b_back = back;
			}
		} else {
			ASSERT(vp->v_buf == bp);
			vp->v_buf = back;
		}

		/*
		 * back is taking bp's place in the tree, and
		 * has therefore lost a b_back node (itself).
		 */
		retreat(back, AVL_FORW);
		chunk_check(vp);
		bp->b_vp = NULL;
		VN_BUF_UNLOCK(vp, s);
		return;
	}

	/*
	 * Hardest case: children on both sides, and back's
	 * b_forw pointer isn't null.  Find the immediately
	 * inferior buffer by following back's b_forw line
	 * to the end, then have it inherit bp's b_forw tree.
	 *
	 * bp->	    xI			      xH
	 *	    / \			      / \
	 *         G   J	     back->  G   J   (before retreat())
	 *	  / \			    / \
	 *       F   ?...  		   F   ?1
	 *      /     \
	 *     ?       H  <-forw
	 *	      /
	 *	     ?1
	 */
	while (back = forw->b_forw)
		forw = back;

	/*
	 * Will be adjusted by retreat() below.
	 */
	forw->b_balance = bp->b_balance;
	
	/*
	 * forw inherits bp's b_forw...
	 */
	forw->b_forw = bp->b_forw;
	bp->b_forw->b_parent = forw;

	/*
	 * ... forw's parent gets forw's b_back...
	 */
	back = forw->b_parent;
	back->b_forw = forw->b_back;
	if (forw->b_back)
		forw->b_back->b_parent = back;

	/*
	 * ... forw gets bp's b_back...
	 */
	forw->b_back = bp->b_back;
	bp->b_back->b_parent = forw;

	/*
	 * ... and forw gets bp's parent.
	 */
	forw->b_parent = parent;

	if (parent) {
		if (parent->b_forw == bp)
			parent->b_forw = forw;
		else
			parent->b_back = forw;
	} else {
		ASSERT(vp->v_buf == bp);
		vp->v_buf = forw;
	}

	/*
	 * What used to be forw's parent is the starting
	 * point for rebalancing.  It has lost a b_forw node.
	 */
	retreat(back, AVL_BACK);
	chunk_check(vp);
	bp->b_vp = NULL;
	VN_BUF_UNLOCK(vp, s);

	BUFINFO.deletes++;
}

/*
 * If len is 0, this returns a pointer to buffer starting at ``offset'';
 * only finds exact match, and doesn't lock the buffer.
 * Useful only for optimization probes.
 *
 * If len is non-zero, this returns the first buffer it finds that
 * overlaps the given range.  It does not lock the buffer.
 */
static buf_t *
findchunk(
	vnode_t	*vp,
	off_t	offset,
	int	len)
{
	buf_t	*bp;
	off_t	boff;
	int	blen;

	ASSERT(len >= 0);

	VN_BUF_LOCK(vp, s);
	bp = vp->v_buf;

	while (bp) {

		boff = bp->b_offset;
		if (len == 0) {
			/*
			 * Len is zero, so we're looking for
			 * an exact match on offset.
			 */
			if (boff < offset) {
				bp = bp->b_forw;
				continue;
			}
			if (boff > offset) {
				bp = bp->b_back;
				continue;
			}
		} else {
			/*
			 * Len is non-zero, so we're looking
			 * for any buffer overlapping the range
			 * specified by offset and len.
			 */
			blen = BTOBB(bp->b_bcount);
			if ((boff + blen) <= offset) {
				bp = bp->b_forw;
				continue;
			}
			if (boff >= (offset + len)) {
				bp = bp->b_back;
				continue;
			}
		}


		VN_BUF_UNLOCK(vp, s);
		buftrace("FINDCHUNK FOUND", bp);

		return bp;
	}

	VN_BUF_UNLOCK(vp, s);
	return NULL;
}

static void
patch_asyncrelse(buf_t *bp)
{
	int s;
	buf_t *forw;
	buf_t *back;

	ASSERT(bp->b_flags & B_DONE);

	s = mutex_spinlock(&chunk_plock);	
	forw = bp->b_dforw;
	back = bp->b_dback;

	ASSERT(forw != bp && back != bp);

	forw->b_dback = back;
	back->b_dforw = forw;

	if (forw == back && !(forw->b_flags & B_PARTIAL)) {
		/*
		 * last buffer out, and parent buffer
		 * hasn't any more dependent buffers
		 */
		buftrace("PATCH_ASYNC LAST", bp);
		if (bp->b_error) {
			forw->b_error = bp->b_error;
			forw->b_flags |= B_ERROR;
		}
		forw->b_flags |= B_DONE;
		ASSERT(forw->b_flags & B_LEADER);
		forw->b_flags &= ~B_LEADER;
		mutex_spinunlock(&chunk_plock, s);	

		ASSERT(forw->b_relse);
		(*forw->b_relse)(forw);
	} else {
		/*
		 * Set b_error, if necessary, while originating buffer
		 * is still being held captive (orig buffer will be
		 * released by last buffer off the list).
		 */
		buftrace("PATCH_ASYNC NOT LAST", bp);
		if (bp->b_error) {
			while (!(forw->b_flags & B_LEADER)) {
				forw = forw->b_dforw;
				ASSERT(forw != bp);
			}
			forw->b_error = bp->b_error;
			forw->b_flags |= B_ERROR;
		}
		mutex_spinunlock(&chunk_plock, s);	
	}

	bp->b_dforw = bp->b_dback = bp;
	putphysbuf(bp);
}


#ifdef NOTYET
/*
 * This routine optimizes the path in chunk_patch() where there
 * are multiple ranges of pages in the buffer that need to be
 * read in from disk.  Doing a single I/O of the entire buffer
 * rather than two small I/Os is almost always more efficient.
 * This can only be done if the buffer is the only thing referencing
 * the pages (no mapped file references), the buffer is not dirty,
 * and none of the pages are dirty.
 *
 * In re-reading pages we could potentially have problems on non I/O
 * coherent machines.  However, since the pages are clean they must
 * have exactly the same contents on disk so we don't need to bother
 * flushing the cache in this case.
 *
 * This routine returns 1 if it decided to initialize the buffer itself
 * and 0 otherwise.
 */
static int
chunk_patch_multiple_holes(
	buf_t	*bp)
{			   
	int		pgcnt;
	int		holecnt;
	int		c;
	pfd_t		*curpfd;
	uint64_t	flags;

	ASSERT(!(bp->b_flags & B_DELWRI));
	pgcnt = btoct(bp->b_bufsize);
	holecnt = 0;
	c = pgcnt;
	curpfd = bp->b_pages;
	/*
	 * Check to see if there is more than one range of
	 * uninitialized pages.   If so then it will be
	 * to our advantage to read in the entire buffer if
	 * we can.
	 */	
	while (c > 0) {
		if (curpfd->pf_flags & P_DONE) {
			curpfd = curpfd->pf_pchain;
			c--;
			continue;
		}
		holecnt++;
		if (holecnt > 1) {
			break;
		}
		/*
		 * Find the start of the next initialized section
		 * or the end of the buffer.
		 */	
		curpfd = curpfd->pf_pchain;
		c--;
		while ((c > 0) && !(curpfd->pf_flags & P_DONE)) {
			curpfd = curpfd->pf_pchain;
			c--;
		}
	}
				
	/*
	 * If we can clear the P_DONE flag in all of the
	 * already initialized pages and none of them are
	 * being referenced by anyone else, then we can just
	 * read in the entire thing at once.  This keeps
	 * us from slowing way down in the code below by 
	 * issuing lots of very small I/Os.
	 * We only do this if there is more than one uninitialized
	 * range in the buffer.
	 */
	if ((holecnt > 1) && pagespatch(bp->b_pages, pgcnt)) {
		BUFINFO.getpatch++;
		buftrace("PATCH_ENTIRE", bp);
		flags = bp->b_flags;
		flags &= ~B_PARTIAL;
		
		ASSERT((flags & B_WAKE) &&
		       (bp->b_relse == chunkhold));
		flags |= B_READ;
		bp->b_flags = flags;
		VOP_STRATEGY(bp->b_vp, bp);

		KTOP_UPDATE_CURRENT_INBLOCK(1);
		SYSINFO.bread += BTOBBT(bp->b_bcount);

		if (!(flags & B_ASYNC)) {
			biowait(bp);
		}
		return 1;
	}
	return 0;
}
#endif /* NOTYET */

/*
 * A buffer which has some dirty pages and some initialized pages
 * cannot just be read in wholesale.  chunk_patch reads in just
 * those parts of the buffer that are uninitilized.
 */
static void
chunk_patch(buf_t *pbp)
{
	register buf_t 	*bp;
	register pfd_t 	*pfd = pbp->b_pages;
	register int	bbs = BTOBBT(pbp->b_bcount);
	register uint64_t flags;
	int		pgbboff = dpoff(pbp->b_offset);
	daddr_t		blkno = pbp->b_blkno;
	int		set_lead = 0;
	int		s;
	off_t	offset = pbp->b_offset;

	ASSERT((pbp->b_flags & (B_DONE|B_WAKE|B_PARTIAL)) == (B_WAKE|B_PARTIAL));
	ASSERT(pbp->b_dback == pbp && pbp->b_dforw == pbp);

#ifdef NOTYET
	if (!(pbp->b_flags & B_DELWRI)) {
		if (chunk_patch_multiple_holes(pbp)) {
			return;
		}
	}
#endif

	buftrace("CHUNK_PATCH", pbp);
	while (bbs > 0) {
		int nblks = NDPP - pgbboff;

		nblks = MIN(nblks, bbs);

		ASSERT((pfd->pf_flags & (P_DONE|P_DIRTY)) != P_DIRTY);

		if (!(pfd->pf_flags & P_DONE)) {
			BUFINFO.getpatch++;
			bp = ngeteblkdev(pbp->b_edev, 0);
			bp->b_vp = pbp->b_vp;
			bp->b_blkno = blkno;
			bp->b_offset = offset;
			bp->b_bufsize = NBPP;
			bp->b_remain = 0;
			bp->b_pages = pfd;
			flags = B_READ | B_PAGEIO;
			flags |= pbp->b_flags &
			         (B_UNCACHED|B_ASYNC|B_UNINITIAL);
			bp->b_flags |= flags;

			/*
			 * These are anonymous buffers, so they will never
			 * be using the dirty-vnode buffers list pointers
			 * (b_dback, b_dforw); pbp isn't on dirty chain
			 * yet, either, so it isn't using the pointers.
			 */
			if (flags & B_ASYNC) {
				buf_t *forw;

				s = mutex_spinlock(&chunk_plock);
				forw = pbp->b_dforw;
				pbp->b_dforw = bp;
				forw->b_dback = bp;
				bp->b_dforw = forw;
				bp->b_dback = pbp;
				mutex_spinunlock(&chunk_plock, s);

				bp->b_relse = patch_asyncrelse;
				/*
				 * Mark pbp as the lead buffer to
				 * distinguish it for patch_asyncrelse().
				 */
				if (!set_lead) {
					set_lead = 1;
					pbp->b_flags |= B_LEADER;
				}
			} else {
				bp->b_dback = pbp->b_dback;
				pbp->b_dback = bp;
			}

			pfd = pfd->pf_pchain;
			while (bbs > nblks) {
				if (pfd->pf_flags & P_DONE)
					break;
				ASSERT(!(pfd->pf_flags & P_DIRTY));
				bp->b_bufsize += NBPP;
				nblks += NDPP;
				nblks = MIN(nblks, bbs);
				pfd = pfd->pf_pchain;
			}

			bp->b_bcount = BBTOB(nblks);
			ASSERT((bp->b_offset + BTOBBT(bp->b_bcount)) <=
			       (pbp->b_offset + BTOBBT(pbp->b_bcount)));

			if (pgbboff)
				(void)bp_mapin(bp);

			VOP_STRATEGY(bp->b_vp, bp);

			KTOP_UPDATE_CURRENT_INBLOCK(1);
			SYSINFO.bread += nblks;

		} else {
			pfd = pfd->pf_pchain;
		}

		pgbboff = 0;
		/*
		 * If this buffer is B_DELALLOC or covers a hole,
		 * then blkno may be < 0.  If so, just keep it
		 * that way.
		 */
		if (blkno >= 0) {
			blkno += nblks;
		}
		offset += nblks;
		bbs -= nblks;
	}

	if (pbp->b_flags & B_ASYNC) {
		s = mutex_spinlock(&chunk_plock);
		ASSERT((pbp->b_flags & (B_DONE|B_PARTIAL)) == B_PARTIAL);
		ASSERT(pbp->b_flags & B_LEADER);

		if (pbp->b_dforw == pbp) {
			ASSERT(pbp->b_dback == pbp);
			/*
			 * This process could have gotten an interrupt
			 * so that the dependent buffers have already
			 * completed.  Check, and if so, clean up.
			 */
			flags = pbp->b_flags | B_DONE;
			pbp->b_flags = flags & ~(B_PARTIAL | B_LEADER);
			mutex_spinunlock(&chunk_plock, s);

			ASSERT(pbp->b_relse);
			(*pbp->b_relse)(pbp);
			return;
		}

		pbp->b_flags &= ~B_PARTIAL;

		mutex_spinunlock(&chunk_plock, s);
		return;
	}

	/*
	 * These are anonymous buffers, so they will never be using
	 * the dirty-vnode buffers list pointers (b_dback, b_dforw).
	 */
	while ((bp = pbp->b_dback) != pbp) {
		pbp->b_dback = bp->b_dback;
		bp->b_dback = bp;

		biowait(bp);
		ASSERT((bp->b_error && (bp->b_flags & B_ERROR)) ||
		       (!bp->b_error && !(bp->b_flags & B_ERROR)));

		if (bp->b_error) {
			pbp->b_error = bp->b_error;
			pbp->b_flags |= B_ERROR;
		}
		putphysbuf(bp);
	}

	ASSERT(pbp->b_dback == pbp);

	flags = pbp->b_flags | B_DONE;
	pbp->b_flags = flags & ~B_PARTIAL;
}

/*
 * Balance buffer AVL tree after attaching a child buffer to bp.
 * Called only by bp_insert.
 */
void
bp_balance(
	struct vnode *vp,
	buf_t *bp,
	int growth)
{
	/*
	 * At this point, bp points to the buffer to which
	 * buf has been attached.  All that remains is to
	 * propagate b_balance up the tree.
	 */
	for ( ; ; ) {
		buf_t *parent = bp->b_parent;
		buf_t *child;

		CERT(growth == AVL_BACK || growth == AVL_FORW);

		/*
		 * If the buffer was already balanced, set b_balance
		 * to the new direction.  Continue if there is a
		 * parent after setting growth to reflect bp's
		 * relation to its parent.
		 */
		if (bp->b_balance == AVL_BALANCE) {
			bp->b_balance = growth;
			if (parent) {
				if (parent->b_forw == bp)
					growth = AVL_FORW;
				else {
					ASSERT(parent->b_back == bp);
					growth = AVL_BACK;
				}

				bp = parent;
				continue;
			}
			break;
		}

		if (growth != bp->b_balance) {
			/*
			 * Subtree is now balanced -- no net effect
			 * in the size of the subtree, so leave.
			 */
			bp->b_balance = AVL_BALANCE;
			break;
		}

		BUFINFO.irotates++;

		if (growth == AVL_BACK) {

			child = bp->b_back;
			CERT(bp->b_balance == AVL_BACK && child);

			if (child->b_balance == AVL_BACK) { /* single LL */
				/*
				 * ``A'' just got inserted;
				 * bp points to ``E'', child to ``C'',
				 * and it is already AVL_BACK --
				 * child will get promoted to top of subtree.

				bp->	     -E			C
					     / \	       / \
				child->	   -C   F	     -B   E
					   / \		     /   / \
					 -B   D		    A   D   F
					 /
					A

					Note that child->b_parent and
					b_balance get set in common code.
				 */
				bp->b_parent = child;
				bp->b_balance = AVL_BALANCE;
				bp->b_back = child->b_forw;
				if (child->b_forw)
					child->b_forw->b_parent = bp;
				child->b_forw = bp;
			} else {
				/*
				 * double LR
				 *
				 * child's b_forw node gets promoted to
				 * the top of the subtree.

				bp->	     -E		      C
					     / \	     / \
				child->	   +B   F	   -B   E
					   / \		   /   / \
					  A  +C 	  A   D   F
					       \
						D

				 */
				buf_t *tmp = child->b_forw;

				CERT(child->b_balance == AVL_FORW && tmp);

				child->b_forw = tmp->b_back;
				if (tmp->b_back)
					tmp->b_back->b_parent = child;

				tmp->b_back = child;
				child->b_parent = tmp;

				bp->b_back = tmp->b_forw;
				if (tmp->b_forw)
					tmp->b_forw->b_parent = bp;

				tmp->b_forw = bp;
				bp->b_parent = tmp;

				if (tmp->b_balance == AVL_BACK)
					bp->b_balance = AVL_FORW;
				else
					bp->b_balance = AVL_BALANCE;

				if (tmp->b_balance == AVL_FORW)
					child->b_balance = AVL_BACK;
				else
					child->b_balance = AVL_BALANCE;

				/*
				 * Set child to point to tmp since it is
				 * now the top of the subtree, and will
				 * get attached to the subtree parent in
				 * the common code below.
				 */
				child = tmp;
			}

		} else /* growth == AVL_BACK */ {

			/*
			 * This code is the mirror image of AVL_FORW above.
			 */

			child = bp->b_forw;
			CERT(bp->b_balance == AVL_FORW && child);

			if (child->b_balance == AVL_FORW) { /* single RR */
				bp->b_parent = child;
				bp->b_balance = AVL_BALANCE;
				bp->b_forw = child->b_back;
				if (child->b_back)
					child->b_back->b_parent = bp;
				child->b_back = bp;
			} else {
				/*
				 * double RL
				 */
				buf_t *tmp = child->b_back;

				ASSERT(child->b_balance == AVL_BACK && tmp);

				child->b_back = tmp->b_forw;
				if (tmp->b_forw)
					tmp->b_forw->b_parent = child;

				tmp->b_forw = child;
				child->b_parent = tmp;

				bp->b_forw = tmp->b_back;
				if (tmp->b_back)
					tmp->b_back->b_parent = bp;

				tmp->b_back = bp;
				bp->b_parent = tmp;

				if (tmp->b_balance == AVL_FORW)
					bp->b_balance = AVL_BACK;
				else
					bp->b_balance = AVL_BALANCE;

				if (tmp->b_balance == AVL_BACK)
					child->b_balance = AVL_FORW;
				else
					child->b_balance = AVL_BALANCE;

				child = tmp;
			}
		}

		child->b_parent = parent;
		child->b_balance = AVL_BALANCE;

		if (parent) {
			if (parent->b_back == bp)
				parent->b_back = child;
			else
				parent->b_forw = child;
		} else {
			ASSERT(vp->v_buf == bp);
			vp->v_buf = child;
		}

		break;
	}
}

/*
 * Find the buffer mapped by *bmap, or insert it into the vnode hash tree.
 * A found buffer will always be B_DONE; a newly inserted buffer won't.
 */
static buf_t *
bp_insert(
	vnode_t *vp,
	struct bmapval *bmap)
{
	buf_t *bp;
	off_t start = bmap->offset;
	off_t end = start + bmap->length;
	buf_t *buf = NULL;
	int growth;
	unsigned int gen;

	BUFINFO.getblks++;

	/*
	 * Negative start is an error.  Protect from idiot file systems.
	 */
	if (start < 0) {
		buf = ngeteblkdev(bmap->pbdev, 0);
		buf->b_vp = vp;
		buf->b_offset = start;
		buf->b_forw = buf->b_back = buf->b_parent = (buf_t *)NULL;
		buf->b_relse = chunkrelse;
		buf->b_flags |= B_DONE|B_ERROR;
		buf->b_error = EINVAL;
		return buf;
	}

again:
	VN_BUF_LOCK(vp, s);
changed:
	if ((bp = vp->v_buf) == NULL) { /* degenerate case... */
		if (buf == NULL) {
			VN_BUF_UNLOCK(vp, s);

			buf = ngeteblkdev(bmap->pbdev, 0);
			buf->b_vp = vp;
			buf->b_offset = start;
			buf->b_forw = buf->b_back =
					buf->b_parent = (buf_t *)NULL;
			buf->b_relse = chunkhold;
			buf->b_bcount = bmap->bsize;
			buf->b_balance = AVL_BALANCE;

			goto again;
		}
		vp->v_buf = buf;
		VN_BUF_UNLOCK(vp, s);
		return buf;
	}

	for ( ; ; ) {
		CERT(bp->b_parent || vp->v_buf == bp);
		CERT(!bp->b_parent || vp->v_buf != bp);
		CERT(!(bp->b_back) || bp->b_back->b_parent == bp);
		CERT(!(bp->b_forw) || bp->b_forw->b_parent == bp);
		CERT(bp->b_balance != AVL_FORW || bp->b_forw);
		CERT(bp->b_balance != AVL_BACK || bp->b_back);
		CERT(bp->b_balance != AVL_BALANCE ||
		     bp->b_back == NULL || bp->b_forw);
		CERT(bp->b_balance != AVL_BALANCE ||
		     bp->b_forw == NULL || bp->b_back);

		ASSERT(bp->b_bcount != 0);
		if (bp->b_offset >= end) {
			if (bp->b_back) {
				bp = bp->b_back;
				continue;
			}
			growth = AVL_BACK;
			break;
		}

		if (bp->b_offset + BTOBBT(bp->b_bcount) <= start) {
			if (bp->b_forw) {
				bp = bp->b_forw;
				continue;
			}
			growth = AVL_FORW;
			break;
		}

		VN_BUF_UNLOCK(vp, s);

		if (!cpsema(&bp->b_lock)) {
			BUFINFO.getblockmiss++;
			ADD_SYSWAIT(iowait);

			psema(&bp->b_lock, PRIBIO);

			SUB_SYSWAIT(iowait);
		}

		if (bp->b_vp != vp ||
		    bp->b_offset >= end ||
		    bp->b_offset + BTOBBT(bp->b_bcount) <= start) {
			/*
			 * Buffer has changed and no longer
			 * overlaps *bmap.  Start over.
			 */
			vsema(&bp->b_lock);
			BUFINFO.getbchg++;
			goto again;
		}

		/*
		 * Since pages associated with chunk buffers are always
		 * linked through their pf_pchain pointers, it is not
		 * sufficient to B_STALE a buffer when the file is being
		 * truncated since some of the pages may still be valid.
		 * The assert !B_STALE checks this.
		 */
		ASSERT((bp->b_flags & (B_DONE|B_STALE)) == B_DONE);

		notavailchunk(bp);

		if (bp->b_offset != start || bp->b_bcount != bmap->bsize) {
			BUFINFO.getoverlap++;
			if (bp->b_flags & B_NFS_UNSTABLE) {
				BUFINFO.sync_commits++;
				chunkcommit(bp, 1);	
				goto again;
			}
			bp->b_flags |= B_STALE;
			if (bmap->eof & BMAP_FLUSH_OVERLAPS) {
				chunk_decommission(bp, CD_FLUSH);
			} else {
				chunk_decommission(bp, 0);
			}
			goto again;
		}

		if (buf)
			putphysbuf(buf);

#ifdef PGCACHEDEBUG
                {
                int j;
                pfd_t *bpfd;
                for (bpfd = bp->b_pages, j = btoct(bp->b_bufsize);
		     j > 0;
		     bpfd = bpfd->pf_pchain, j--) {
                        if (bpfd->pf_flags & P_BAD) {
                                cmn_err(CE_CONT,
					"pfd 0x%x BAD but on buf 0x%x\n",
                                        bpfd, bp);
                                debug("");
                        }
                }
                }
#endif
		BUFINFO.getfound++;
		bp->b_flags |= B_FOUND;
		bp->b_relse = chunkhold;

		return(bp);
	}

	if (!buf) {
		gen = vp->v_bufgen;
		VN_BUF_UNLOCK(vp, s);

		buf = ngeteblkdev(bmap->pbdev, 0);
		buf->b_vp = vp;
		buf->b_offset = start;
		buf->b_forw = buf->b_back = buf->b_parent = (buf_t *)NULL;
		buf->b_relse = chunkhold;
		buf->b_bcount = bmap->bsize;
		buf->b_balance = AVL_BALANCE;

		/*
		 * Reacquire vnode lock and check that state hasn't changed.
		 */
		VN_BUF_LOCK(vp, s);
		if (gen != vp->v_bufgen) {
			/* BUFINFO.ichange++; */
			goto changed;
		}
	}

	if (growth == AVL_BACK)
		bp->b_back = buf;
	else
		bp->b_forw = buf;

	buf->b_parent = bp;
	CERT(bp->b_forw == buf || bp->b_back == buf);

	vp->v_bufgen++;
	BUFINFO.inserts++;

	bp_balance(vp, bp, growth);

	chunk_check(vp);
	VN_BUF_UNLOCK(vp, s);

	return buf;
}


/*
 * This routine is called by gather_chunk() when a pagealloc()
 * call fails.  It decommissions the current buffer and calls
 * setsxbrk().
 */
void
gather_chunk_low_mem(
	vnode_t		*vp,
	buf_t		*bp,
	off_t		lpn)
{
	pfd_t	*tpfd;
	pfd_t	*pfd;
	int	s;
	int	set_flag;

	/*
	 * If we're about to go sxbrk, get rid of the
	 * buffer.  We just want to reduce our memory
	 * usage as much as possible while we wait.
	 */
	tpfd = bp->b_pages;
	s = lpn - dtopt(bp->b_offset);
	while (--s >= 0) {
		pfd = tpfd->pf_pchain;
#ifdef CHUNKDEBUG
		if (tpfd->pf_flags & P_DIRTY)
			cmn_err(CE_NOTE,
				"_dirty lpn %x",
				tpfd->pf_pageno);
#endif
		pagefree(tpfd);
		tpfd = pfd;
	}

	chunkunhash(bp);
	ASSERT(bp->b_un.b_addr == NULL);
	putphysbuf(bp);

	/*
	 * Try to flush any dirty pages associated
	 * with this vnode before going sxbrk.  This
	 * is necessary because we are holding the
	 * vnode rwlock at this point and that will
	 * lock out anyone else trying to flush
	 * out the pages.
	 *
	 * Use the B_SWAP flag to indicate that we
	 * are low on memory so pdflush should
	 * push the pages one at a time rather than
	 * clustering them.  This keeps us from
	 * getting stuck needing additional memory
	 * to build a cluster when trying to push
	 * out a page in do_pdflush().
	 *
	 * The B_SWAP flag also protects us from
	 * deadlocking on buffers that we already
	 * have locked.
	 */
	if (vp->v_dpages) {
		if (!(vp->v_flag & VFLUSH)) {
			VN_FLAGSET(vp, VFLUSH);
			set_flag = 1;
		} else {
			set_flag = 0;
		}
		do_pdflush(vp, INT_MAX, B_ASYNC | B_SWAP);
		if (set_flag) {
			VN_FLAGCLR(vp, VFLUSH);
		}
	}

	setsxbrk();

	return;
}

/*
 * Decide whether it is OK to allocate a page of the given size at
 * the given offset.  We don't want the large page to extend past
 * the base page encompassing the end of the file.
 */
size_t
gather_chunk_lpage_check(
	vnode_t	*vp,
	off_t	lpn,
	size_t	pm_page_size)
{
	vattr_t	va;
	int	error;
	off_t	end_page_offset;

	va.va_mask = AT_SIZE;
	VOP_GETATTR(vp, &va, 0, curuthread ? get_current_cred() : sys_cred, error);
	if (error) {
		return NBPP;
	}
	end_page_offset = LPAGE_ALIGN(ctooff(lpn), pm_page_size) + pm_page_size;
	if (end_page_offset > va.va_size) {
		return NBPP;
	}
	return pm_page_size;
}

/*
 * This is a subroutine for some common code shared by gather_chunk()
 * and gather_chunk_lpage_insert().  It waits for the completion of
 * page migration on the page if necessary and then collects some
 * state based on the state of the page.
 */
/* ARGSUSED */
int
gather_chunk_process_page(
	pfd_t	*pfd,
	buf_t	*bp)
{
	int	flags;
	int	gc_flags;

	/*
	 * P_DONE is also used to prevent threads from using
	 * a pfdat that is currently being migrated.
	 * When releasing the memory object cache lock
	 * in the migration engine code, we allow for the
	 * possibility of pfind to succeed and return the newly
	 * inserted pfdat. We have to prevent this case by waiting
	 * for migrating pages to be DONE before proceeding.
	 */
	if (pfdat_ismigrating(pfd)) {
		pagewait(pfd);
	}

	flags = pfd->pf_flags;
	gc_flags = 0;
	if (flags & P_DONE) {
		if (flags & P_DIRTY) {
			if (flags & P_DQUEUE) {
				gc_flags |= GC_PDCLUSTER;
				ASSERT(pfd->pf_use > 1);
			}
			gc_flags |= GC_DELWRI;
		}
		gc_flags |= GC_SOMEDONE;
	} else {
		ASSERT(!(pfd->pf_flags & P_DIRTY));
#ifdef _VCE_AVOIDANCE
		if (vce_avoidance && ! (bp->b_flags & B_UNCACHED))
		    (void) color_validation(
		    	pfd,
			vcache_color(pfd->pf_pageno),
			0,0);
#endif /* _VCE_AVOIDANCE */
		gc_flags |= GC_SOMEUNDONE;
	}

	return gc_flags;
}

/*
 * To be used on a buffer allocated BMAP_DONTALLOC and for which
 * gather_chunk() actually decided not to allocate pages. Alloc them
 * now. Also works for a buffer that has some pages in b_pages, but
 * more need to be alloced.
 */
void
gather_chunk_alloc_pages(
	buf_t	*bp)
{
	pfd_t	**pfdp;
	pfd_t	*pfd;
	vnode_t	*vp = bp->b_vp;
	pgno_t	lpn;
	uint	pagekey;
	int	pages;
	int	gc_flags;

	gc_flags = 0;
loop:
	lpn = dtopt(bp->b_offset);
	pages = btoc(bp->b_bufsize);
	pfdp = &bp->b_pages;
	
	while (*pfdp) {
		pages--;
		lpn++;
		pfdp = &(*pfdp)->pf_pchain;
	}
	while (pages-- > 0) {
		pagekey = VKEY(vp) + lpn;
		pfd = pagealloc_rr(pagekey, VM_MVOK);
		if (pfd	== NULL) {
			/*
			 * free memory and retry
			 */
			pfd = bp->b_pages;
			bp->b_pages = NULL;
			while (pfd) {
				pfd_t *nextpfd;

				nextpfd = pfd->pf_pchain;
				premove(pfd);
				pagefree(pfd);
				pfd = nextpfd;
			}
			setsxbrk();
			goto loop;
		}
		pinsert(pfd, (void *)vp, lpn, 0);
		*pfdp = pfd;
		pfdp = &pfd->pf_pchain;

		gc_flags |= gather_chunk_process_page(pfd, bp);
		lpn++;
	}

	/*
	 * Wait to update chunk stats until _after_ all the pages
	 * have been collected -- the pages will be freed and the
	 * buffer decommissioned before going sxbrk above...
	 */
	atomicAddInt(&chunkpages, btoc(bp->b_bufsize));
	if (gc_flags & GC_DELWRI)
		bp->b_flags |= B_DELWRI;
	if (gc_flags & GC_PDCLUSTER)
		pdcluster(bp, 0);	/* end of buf is aligned */
	(void)bp_mapin(bp);
}

/*
 * To be used on a buffer allocated BMAP_DONTALLOC with b_pages set
 * to NULL. Takes a chain of pfdats (use cnt has already been bumped
 * for us), initializes the fields, and inserts into buf_t.
 */
void
gather_chunk_insert_pages(
	buf_t	*bp,
	pfd_t	*pfd)
{
	vnode_t	*vp = bp->b_vp;
	pgno_t	lpn;
	int	gc_flags;
	int	pages;

	ASSERT(bp->b_pages == NULL);

	gc_flags = 0;
	lpn = dtopt(bp->b_offset);
	pages = btoc(bp->b_bufsize);
	bp->b_pages = pfd;
	
	while (pfd) {
		if ((pfd->pf_flags & P_DONE) == 0)
			if (pfd->pf_use == 1)
				pfd->pf_flags |= P_DONE;
			else
				pfd_setflags(pfd, P_DONE);
		pinsert(pfd, (void *)vp, lpn++, 0);
		gc_flags |= gather_chunk_process_page(pfd, bp);
		pfd = pfd->pf_pchain;
		pages--;
	}
	if (pages) {
		/* g_c_alloc_pages() adjusts chunkpages for entire buffer */
		gather_chunk_alloc_pages(bp);
		bp->b_flags |= B_WAKE;
		if (pages != btoc(bp->b_bufsize))
			bp->b_flags |= B_PARTIAL;
	} else {
		atomicAddInt(&chunkpages, btoc(bp->b_bufsize));
		bp->b_flags &= ~B_WAKE;	/* B_WAKE set previously */
	}
	if (gc_flags & GC_DELWRI)
		bp->b_flags |= B_DELWRI;
	if (gc_flags & GC_PDCLUSTER)
		pdcluster(bp, 0);	/* end of buf is aligned */
	(void)bp_mapin(bp);
}

/*
 * This subroutine for gather_chunk() handles the insertion of large
 * pages into the page cache.  All of the subpages of the large page
 * are inserted into the page cache.  If there are pages before or
 * after the range of the buffer we're creating then we set appropriate
 * flags to tell fetch_chunk() about them.
 *
 * The number of parameters here is ridiculous, but they are necessary
 * in order to keep this code from polluting the main line code in
 * gather_chunk().
 */
pfd_t **
gather_chunk_lpage_insert(
	vnode_t		*vp,
	pfd_t		*pfd,
	pfd_t		**pfdp,
	size_t		page_size,
	pgno_t		*lpnp,
	int		*bmap_lengthp,
	int		pgbboff,
	int		*gc_flagsp,
	size_t		*start_page_sizep,
	size_t		*end_page_sizep,
	buf_t		*bp)
{
	pfd_t	*tpfd;
	pgno_t	lpn;
	pgno_t	last_lpn;
	pgno_t	first_buf_lpn;
	int	lpn_lpage_offset;
	int	bmap_length;
	int	gc_flags;
	size_t	start_page_size;
	size_t	end_page_size;

	lpn = *lpnp;
	bmap_length = *bmap_lengthp;
	start_page_size = NBPP;
	end_page_size = NBPP;
	gc_flags = 0;

	ASSERT(page_size > NBPP);
	lpn_lpage_offset = offtoct(ctooff(lpn) % page_size);
	ASSERT(lpn_lpage_offset < offtoct(page_size));
	first_buf_lpn = lpn;
	lpn -= lpn_lpage_offset;
	last_lpn = lpn + offtoct(page_size);
	while (lpn < last_lpn) {
		tpfd = pinsert_try(pfd, (void *)vp, lpn);
		if (tpfd != pfd) {
			gc_flags |= GC_LPAGE_COLLISION;
		}
		/*
		 * If the subpage is not within the bounds of the buffer
		 * we're creating, then set the appropriate return parameter
		 * so that fetch_chunk will know which pages to free later.
		 * If the buffer overlaps the first base page of the large
		 * page or the last base page of the large page, then the
		 * corresponding page size return parameter will be NBPP.
		 * Otherwise it will be the size of the large page.
		 */
		if (lpn < first_buf_lpn) {
			start_page_size = page_size;
			ASSERT(tpfd->pf_use > 0);
			pagefree(tpfd);
			gc_flags |= GC_FIRST_PAGEFREED;
		} else if (bmap_length <= 0) {
			end_page_size = page_size;
			ASSERT(tpfd->pf_use > 0);
			pagefree(tpfd);
			gc_flags |= GC_LAST_PAGEFREED;
		} else {
			gc_flags |= gather_chunk_process_page(tpfd, bp);

			bmap_length -= NDPP - pgbboff;
			pgbboff = 0;
			*pfdp = tpfd;
			pfdp = &tpfd->pf_pchain;
		}
		lpn++;
		pfd++;
	}

	*lpnp = lpn;
	*bmap_lengthp = bmap_length;
	*gc_flagsp |= gc_flags;
	*start_page_sizep = start_page_size;
	*end_page_sizep = end_page_size;
	return pfdp;
}

/*
 * Internal routine to fetch a buffer mapped by bmap,
 * with no i/o or policy implied.  The outflagsp
 * parameter tells the caller whether or not pdcluster()
 * should be called on the returned buffer.  It only
 * returns with GC_PDCLUSTER set if there are pages in the
 * buffer which are on the vnode's dirty pages list.
 *
 * The page size parameters indicate fetch_chunk's desired
 * page size for the buffer and the page sizes actually used.
 * The start_page_sizep parameter indicates the desired size,
 * and each returns the size of the page overlapping either
 * the beginning or end of the buffer.
 */
static buf_t *
gather_chunk(
	vnode_t		*vp,
	bmapval_t	*bmap,
	int		*outflagsp,
	size_t		*start_page_sizep,
	size_t		*end_page_sizep)
{
	buf_t		*bp;
	pfd_t		*pfd;
	pfd_t		**pfdp;
	pgno_t		lpn;
	int		bmap_length;
	int		pgbboff;
	int		gc_flags;
	size_t		page_size;
	size_t		pm_page_size;
	uint		pagekey;
	struct pm	*pmp;

	gc_flags = 0;
loop:
	/*
	 * Find buffer mapped by bmap, or insert one.
	 */
	bp = bp_insert(vp, bmap);
	ASSERT(bp->b_bcount != 0 || bp->b_offset < 0);

	/*
	 * The only buffers that bp_insert can return that aren't
	 * done are those it put on the vnode tree itself.  Undone
	 * buffers at this point have no pages associated with them.
	 */
	if (bp->b_flags & B_DONE) {
		*outflagsp = 0;
		*start_page_sizep = NBPP;
		*end_page_sizep = NBPP;
		return bp;
	}

	ASSERT(bp->b_un.b_addr == NULL);

	bp->b_dforw = bp->b_dback = bp;
	bp->b_blkno = bmap->bn;
	bp->b_remain = 0;
	bp->b_pages = (struct pfdat *)NULL;

	ASSERT(bp->b_flags == (B_BUSY|B_AGE));
	bp->b_flags = B_BUSY|B_PAGEIO;

	/*
	 * Default page release function is now chunkhold.
	 */
	bp->b_relse = chunkhold;

	bmap_length = bmap->length;
	lpn = dtopt(bmap->offset);
	pgbboff = dpoff(bmap->offset);
	bp->b_bufsize = ctob(dtop(pgbboff + bmap_length));

	if ((bmap->eof & BMAP_DONTALLOC) && dpoff(bmap->offset) == 0 &&
	    *start_page_sizep == NBPP && bmap->pmp == NULL) {
		int length = bmap_length;
		pgno_t curlpn = lpn;

		while (length > 0) {
			/*
			 * If any pages were in the cache, we go the hard way
			 * rather than hope they're not in use and remove them.
			 * Hopefully all this buf's pages are in the cache.
			 */
			if (vnode_plookup((void *)vp, curlpn))
				goto bmap_do_alloc;
			length -= NDPP;
			curlpn++;
		}
		*end_page_sizep = NBPP;
		*outflagsp = 0;
		bp->b_flags |= B_WAKE;
		goto done;
	}
bmap_do_alloc:
	/*
	 * Collect all pages to be mapped by this buffer.
	 */
	pfdp = &bp->b_pages;
	pmp = bmap->pmp;

	/*
	 * Use the page size decided upon by fetch_chunk().
	 */
	pm_page_size = *start_page_sizep;

	/*
	 * Set these here for the no PM case.
	 */
	page_size = NBPP;
	*start_page_sizep = NBPP;
	*end_page_sizep = NBPP;

	do {
		if (!(pfd = (*pfdp = pfind((void *)vp, lpn, VM_ATTACH)))) {
			int pagekey_base;

#if VCE_AVOIDANCE
			
			if (vce_avoidance)
				pagekey_base = 0;
			else
				pagekey_base = VKEY(vp);
#else
			pagekey_base = VKEY(vp);
#endif
			/*
			 * Allocate a page since it's not already there.
			 */
			pagekey = pagekey_base + lpn;
			if (pmp != NULL) {
				if (pm_page_size != NBPP) {
					page_size =
						gather_chunk_lpage_check(vp,
							lpn, pm_page_size);
					/*
					 * Recalculate the page key using the
					 * base page of the large page we're
					 * going to try to allocate.
					 */
					pagekey = LPAGE_ALIGN(ctooff(lpn),
							      page_size);
					pagekey = pagekey_base + offtoct(pagekey);
				} else {
					page_size = pm_page_size;
				}

				pfd = PM_PAGEALLOC(pmp, pagekey,
						   VM_MVOK, &page_size, (caddr_t)lpn);
				/*
				 * If for whatever reason we don't allocate
				 * the page size we want, then don't waste
				 * time trying again.
				 */
				if (page_size != pm_page_size) {
					pm_page_size = NBPP;
				}
			} else {
                                pfd = pagealloc_rr(pagekey, VM_MVOK);
			}
			if (pfd	== NULL) {
				/*
				 * Deal with the fact that we're running
				 * out of memory and then start over.
				 */
				gather_chunk_low_mem(vp, bp, lpn);
				
				/* 
				 * We need to save the GC_FIRST_PAGEFREED
				 * & GC_LAST_PAGEFREED flag if appropriate, 
				 * since gc_flags will be reinitialized
				 */ 
				gc_flags &= GC_FIRST_PAGEFREED | GC_LAST_PAGEFREED;
				goto loop;
			}

			if (page_size != NBPP) {
				/*
				 * We've allocated a large page.
				 * Insert all of the subpages of the large
				 * page we just allocated.  Do the work of
				 * linking the pages within the bounds of
				 * this buffer together here rather than
				 * calling pfind for each.  Set the start
				 * and end page size parameters as appropriate
				 * so that fetch_chunk() can do the right
				 * thing with subpages outside the range
				 * of the buffer we're creating.
				 */
				pfdp = gather_chunk_lpage_insert(vp, pfd,
						pfdp, page_size, &lpn,
						&bmap_length, pgbboff,
						&gc_flags,
						start_page_sizep,
						end_page_sizep, bp);
				/*
				 * If we run into other pages while trying
				 * to insert our large page, then give up
				 * on the large page thing.
				 */
				if (gc_flags & GC_LPAGE_COLLISION) {
					pm_page_size = NBPP;
				}
				pgbboff = 0;
				continue;
			}
			pfd = pinsert_try(pfd, (void *)vp, lpn);
			*pfdp = pfd;
		}

		pfdp = &pfd->pf_pchain;

		gc_flags |= gather_chunk_process_page(pfd, bp);

		bmap_length -= NDPP - pgbboff;
		pgbboff = 0;
		lpn++;

	} while (bmap_length > 0);

	/*
	 * Wait to update chunk stats until _after_ all the pages
	 * have been collected -- the pages will be freed and the
	 * buffer decommissioned before going sxbrk above...
	 */
	atomicAddInt(&chunkpages, btoct(bp->b_bufsize));

	/*
	 * Strict pageio buffer implies no page offset.
	 * Map the buffer and set offset if this isn't
	 * a page-aligned chunk.
	 */
	if (dpoff(bmap->offset)) {
		(void)bp_mapin(bp);
	}

	ASSERT(gc_flags & (GC_SOMEDONE | GC_SOMEUNDONE));
	if (gc_flags & GC_SOMEDONE) {
		if (gc_flags & GC_SOMEUNDONE) {
			bp->b_flags |= B_WAKE|B_PARTIAL;
		} else {
			bp->b_flags |= B_DONE;
		}
	} else {
		bp->b_flags |= B_WAKE;
	}
	if (gc_flags & GC_DELWRI) {
		bp->b_flags |= B_DELWRI;
	}

	/*
	 * Set the appropriate return parameters.
	 */
	*outflagsp = 0;
	if (gc_flags & GC_PDCLUSTER) 
		*outflagsp |= GC_PDCLUSTER;
	if (gc_flags & GC_FIRST_PAGEFREED) 
		*outflagsp |= GC_FIRST_PAGEFREED;
	if (gc_flags & GC_LAST_PAGEFREED) 
		*outflagsp |= GC_LAST_PAGEFREED;


done:
	/*
	 * If the given mapping says that the buffer is over
	 * a hole in the file, then the buffer cannot be dirty.
	 * Thus, don't allow the B_DELWRI flag to be set in the
	 * buffer.  This prevents buffers over holes in files
	 * from being written out and confusing the file systems.
	 */
	if (bmap->eof & BMAP_HOLE) {
		bp->b_flags &= ~B_DELWRI;
	} else if (bmap->eof & BMAP_UNWRITTEN) {
		bp->b_flags |= B_UNINITIAL;
	}

	return bp;
}

void
pdinsert(pfd_t *pfd)
{
	vnode_t *vp = pfd->pf_vp;
	int s;

	ASSERT(pfd->pf_flags & P_DIRTY);
	/*
	 * Page could have transitioned from P_HASH to P_BAD
	 * (because of a truncate/ptoss, for example) between
	 * the time vhand looked and it called pdinsert, so
	 * strictly asserting P_HASH is incorrect.
	 */
	ASSERT(pfd->pf_flags & (P_BAD|P_HASH));
	ASSERT(pfd->pf_use > 0);

	s = VN_LOCK(vp);
	if (!(pfd->pf_flags & P_DQUEUE)) {
		pfd->pf_vchain = vp->v_dpages;
		vp->v_dpages = pfd;
		pageflagsinc(pfd, P_DQUEUE, 1);
		vp->v_dpages_gen++;
		VN_UNLOCK(vp, s);
		atomicAddInt(&vp->v_vfsp->vfs_dcount, 1);
		ADD_PDCOUNT(1);
	} else {
#ifdef PGCACHEDEBUG
		int found = 0;
		pfd_t *npfd;

		for (npfd = vp->v_dpages; npfd; npfd = npfd->pf_vchain) {
			ASSERT(npfd->pf_vp == vp && npfd->pf_flags & P_DQUEUE);
			if (npfd == pfd) {
				found = 1;
				break;
			}
		}
		ASSERT(found);
#endif
		VN_UNLOCK(vp, s);
	}
}

/*
 * pdcluster - find any dirty pages that are wholly mapped via bp
 * and remove them from the PDQUEUE list - since bp is about to be written
 * they will be written as well
 * Note that partially mapped pages are NOT taken.  The only exception to
 * this is that the last page in the range will be taken if the PDC_EOF
 * flag is specified.  This is to pages which are only partially mapped
 * at the EOF.
 */
static int
pdcluster(
	buf_t	*bp,
	uint	flags)
{
	pgno_t	start;
	pgno_t	end;
	vnode_t	*vp;
	int	npgs;
	int	j;
	pfd_t	*pfd;
	pgno_t	pageno;
	int	tries;
	pfd_t	*pfd_freed;
	pfd_t	*prev_pfd;
	pfd_t	*next_pfd;
	int	ret;
	int	s;
	int	nfreed;
	unsigned int gen;

	start = dtop(bp->b_offset);
	end = dtopt(bp->b_offset + BTOBBT(bp->b_bcount));
	/*
	 * If the end of the buffer is page aligned, then there is
	 * no need for the EOF flag.  Leaving it set will cause us
	 * to pull the page after the the buffer from the vnode's
	 * dirty pages list.  This is because the end variable comes
	 * out the same whether b_offset + b_bcount is at the start
	 * of the page or in the middle.  The PDC_EOF flag is only
	 * to be used when it comes out in the middle.
	 */
	if (dpoff(bp->b_offset + BTOBBT(bp->b_bcount)) == 0) {
		flags &= ~PDC_EOF;
	}
	vp = bp->b_vp;
	ret = 0;

	buftrace("PDCLUSTER", bp);
	if ((start > end) || ((start == end) && !(flags & PDC_EOF))) {
		return 0;
	}

	/* figure out max # of pages to search for */
	for (pfd = bp->b_pages, npgs = 0, j = btoct(bp->b_bufsize);
					j > 0; pfd = pfd->pf_pchain, j--) {
		if (pfd->pf_flags & P_DQUEUE)
			npgs++;
	}

	if (npgs == 0)
		return 0;

	/*
	 * We continue here after preemption.  Instead of searching the
	 * entire list (again), we start where we left off.
	 */
	tries = DCHUNKTRIES;
	pfd_freed = NULL;
	prev_pfd = NULL;
again:
	s = VN_LOCK(vp);
	if (prev_pfd != NULL) {
		/*
		 * If the generation number changed, then we can no longer 
		 * trust prev_pfd, just get out. 
		 */
		if (gen != vp->v_dpages_gen)
			goto out;
	}
	if (prev_pfd != NULL) {
		pfd = prev_pfd->pf_vchain;
	} else {
		pfd = vp->v_dpages;
	}
	while (pfd && (npgs > 0)) {
		ASSERT(pfd->pf_vp == vp);
		ASSERT(pfd->pf_flags & (P_DIRTY|P_DQUEUE));
		ASSERT((pfd->pf_flags & P_QUEUE) == 0);

		if (--tries <= 0) {
			gen = ++vp->v_dpages_gen;
			VN_UNLOCK(vp, s);
			tries = DCHUNKTRIES;
			goto again;
		}

		pageno = pfd->pf_pageno;
		if ((pfd->pf_flags & P_BAD) ||
		    ((pageno >= start) && (pageno < end)) ||
		    ((pageno == end) && (flags & PDC_EOF))) {

			if ((pfd->pf_flags & P_BAD) == 0) {
				ret++;
			}
			npgs--;

			/*
			 * Pull the pfdat from the v_dpages list and
			 * put it in the pfd_freed list.  Do not clear
			 * the P_DQUEUE flag so that noone tries to
			 * add it back to the list.  We'll free the
			 * pfdats once we've pulled them all from the
			 * list.  This way we don't have to drop the
			 * vnode lock.
			 */
			next_pfd = pfd->pf_vchain;
			if (prev_pfd != NULL) {
				prev_pfd->pf_vchain = next_pfd;
			} else {
				vp->v_dpages = next_pfd;
			}
			pfd->pf_vchain = pfd_freed;
			pfd_freed = pfd;
#ifdef CHUNKDEBUG
			if ((pfd->pf_flags & P_BAD) == 0) {
				pfd_t *bpfd = bp->b_pages;
				for (j = btoct(bp->b_bufsize);
				     j > 0;
				     bpfd = bpfd->pf_pchain, j--) {
					if (bpfd == pfd)
						break;
				}
				if (bpfd == NULL || j <= 0) {
					cmn_err(CE_CONT,
						"pfd 0x%x not on buf 0x%x\n",
						pfd, bp);
					ASSERT(0);
				}
			}
#endif
			pfd = next_pfd;
		} else {
			prev_pfd = pfd;
			pfd = pfd->pf_vchain;
		}
	}

	vp->v_dpages_gen++;
out:
	VN_UNLOCK(vp, s);
	if (pfd_freed != NULL) {
		/*
		 * Now free all the pages pulled from the v_dpages list.
		 * We don't hold the vnode lock here.  We can do this
		 * because all the places that mainpulate the pfdats on
		 * the list either find the pfdat on the list or they
		 * look at P_DQUEUE to see whether the pfdat should be
		 * added to the list.  Since P_DQUEUE will still be set
		 * and the pfdat will not be on the list, none of those
		 * places should do anything with the pfdats on our private
		 * list.
		 *
		 * When page replication is enabled it may be the case that
		 * the page we're removing is not the same pfdat for
		 * a given file offset as the one in the buffer.  This
		 * is because the buffer points to a replica and the list
		 * contains the dirty original and our search above does
		 * not notice the difference since it is based on offsets.
		 * This is OK, though, as all replicas are guaranteed to
		 * contain the same data so as long as the buffer will
		 * write out some copy we're safe.
		 */
		nfreed = 0;
		while (pfd_freed != NULL) {
			pfd = pfd_freed;
			pfd_freed = pfd->pf_vchain;
			pfd->pf_vchain = NULL;
			nfreed++;
			ASSERT(pfd->pf_flags & P_DQUEUE);
			pageflags(pfd, P_DQUEUE, 0);
			pagefree(pfd);
		}
		/*
		 * Buffer holds reference to vnode, so no need
		 * to hold vnode locked here.
		 */
		atomicAddInt(&(vp->v_vfsp->vfs_dcount), -(nfreed));
		ADD_PDCOUNT(-(nfreed));
	}
	return ret;
}

/*
 * remove P_BAD pages from a vp, freeing the page and removing them from the
 * dpages queue - this occurs usually when a mmaped file is truncated
 */
#define PDREMOVETRIES 25

static void
pdremove(vnode_t *vp)
{
	pfd_t	*pfd;
	int	tries;
	int	s;
	int	unchained;
	pfd_t	*prev_pfd;
	unsigned int gen = 0;

	/*
	 * Assumes VOP_RWLOCK held on entry.
	 */
	unchained = 0;
	prev_pfd = NULL;
	tries = PDREMOVETRIES;
again:
	s = VN_LOCK(vp);
	if (prev_pfd != NULL) {
		/*
		 * If the generation number has changed, then we can no
		 * longer trust prev_pfd since another thread could be
		 * manipulating the v_dpages chain and for all we know
		 * prev_pfd may no longer be on the v_dpages chain.
		 * Instead of starting over and possibly getting stuck
		 * here forever, we just get out.  The pages for this
		 * buffer will be pulled from the list eventually.
		 */
		if (gen != vp->v_dpages_gen)
			goto out;
	}
	if (prev_pfd != NULL) {
		pfd = prev_pfd->pf_vchain;
	} else {
		pfd = vp->v_dpages;
	}

	while (pfd) {
		if (--tries <= 0) {
			gen = ++vp->v_dpages_gen;
			VN_UNLOCK(vp, s);
			tries = PDREMOVETRIES;
			goto again;
		}
		if ((pfd->pf_flags & P_BAD) == 0) {
			prev_pfd = pfd;
			pfd = pfd->pf_vchain;
			continue;
		}
		if (prev_pfd != NULL) {
			prev_pfd->pf_vchain = pfd->pf_vchain;
		} else {
			vp->v_dpages = pfd->pf_vchain;
		}

		gen = ++vp->v_dpages_gen;
		pageflags(pfd, P_DQUEUE, 0);
		pfd->pf_vchain = NULL;
		VN_UNLOCK(vp, s);

		ASSERT(pfd->pf_vp == vp);
		ASSERT(pfd->pf_flags & P_DIRTY);

		pagefree(pfd);
		unchained++;
		goto again;
	}
out:
	VN_UNLOCK(vp, s);
	if (unchained) {
		atomicAddInt(&vp->v_vfsp->vfs_dcount, -unchained);
		ADD_PDCOUNT(-unchained);
	}
}


/*
 * This is a a helper routine for do_pdflush().  It checks to see if
 * there are any locked buffers in the range given.  It uses findchunk()
 * to find the buffers, and it returns 1 if there are any busy buffers
 * in the given range and 0 otherwise.
 */
STATIC int
chunk_range_is_busy(
	vnode_t		*vp,
	off_t		offset,
	int		len)
{
	buf_t		*bp;
	off_t		end;

	end = offset + len;
	do {
		bp = findchunk(vp, offset, len);
		if (bp != NULL) {
			/*
			 * If we find a locked buffer in the range
			 * we're checking, return 1.  The B_BUSY bit
			 * is as reliable as the lock since we're not
			 * going to do anything but check it.
			 */
			if (bp->b_flags & B_BUSY) {
				return 1;
			}
			/*
			 * If the buffer we found is not locked, skip
			 * over it.  If this takes us beyond the end
			 * of the range we're searching, then we're done
			 * and can return that the range is not locked.
			 */
			offset = bp->b_offset + BTOBB(bp->b_bcount);
			if (offset >= end) {
				return 0;
			}
		}
	} while (bp != NULL);

	return 0;
}


/*
 * This is a utility routine for do_pdflush() used to pull
 * a bmapval to within the confines of a given range.  The file
 * system may have suggested that the data in the range
 * be clustered with data outside the range, but this routine is
 * used by do_pdflush() to refuse the suggested clustering.
 */
STATIC void
chunk_trim_to_range(
	off_t		offset_bb,
	int		len_bb,
	struct bmapval	*bmvp)
{
	int		offset_diff;
	int		length_diff;

	ASSERT(bmvp->offset <= (offset_bb + len_bb));
	ASSERT(bmvp->offset + bmvp->length > offset_bb);

	/*
	 * First trim off any of the mapping that
	 * comes before the start of the range.
	 */
	offset_diff = offset_bb - bmvp->offset;
	if (offset_diff > 0) {
		if (bmvp->bn != (daddr_t)-1) {
			bmvp->bn += offset_diff;
		}
		bmvp->offset += offset_diff;
		bmvp->length -= offset_diff;
		bmvp->bsize -= BBTOB(offset_diff);
		ASSERT(bmvp->pboff >= BBTOB(offset_diff));
		bmvp->pboff -= BBTOB(offset_diff);
		ASSERT(bmvp->pbsize + bmvp->pboff <= bmvp->bsize);
	}
	/*
	 * Now make sure that the mapping doesn't
	 * extend beyond the end of the range.
	 */
	length_diff = bmvp->offset + bmvp->length - offset_bb - len_bb;
	if (length_diff > 0) {
		bmvp->length -= length_diff;
		bmvp->bsize -= BBTOB(length_diff);
		ASSERT(bmvp->pbsize + bmvp->pboff <= bmvp->bsize);
	}
}

extern struct vnodeops *nfs3_getvnodeops(void);

/*
 * do_pdflush - flush the specified pages from a vnode. 
 *	do_pdflush() is a general purpose page flush routine, and
 *	has a number of modes under which it operates. do_pdflush
 *	flushes delayed write pages queued to the vnode on the
 *	v_dpages list, which is an unordered list of linked pfdat
 *	entries. Routines like pdinsert and chunk_decommission add
 *	pages to v_dpages.
 *
 *	The flags argument B_SWAP specifes that do_pdflush was called
 *	for a low memory situation, from which no additional memory 
 *	allocations should be made. All possible pages are flushed in 
 *	this condition.
 *
 *	The flags argument B_BUSY specifies that the caller may have some
 *	buffers locked, which will be bypassed to prevent a deadlock
 *	situation.
 *
 *	count specifies the number of pages to flush. To flush all attached
 *	pages within the declared range, specify INT_MAX.
 */

static void
do_pdflush(vnode_t *vp, int count, uint64_t flags)
{
	pfd_t		*pfd;
	buf_t		*bp;
	off_t		offset;
	int		resid;
	int		n;
	bmapval_t	bmv;
	int		nmaps;
	int		s;
	pfd_t		*pfd_save = NULL;
	int		memory_is_low;
	int		nfs3vp;
	int		dirty_page_count;

#if CELL_IRIX
	/*
	 * XXX - should do_pdflush be a VOP??
	 * defer until we understand how XFS will be distributed.
	 *
	 * If a CFS file, we are pushing pages of
	 * a mmaped CFS file back to the server.
	 */
	
	if (VNODE_TO_DCVN(vp)) {
		n = dcvn_update_page_flags (VNODE_TO_DCVN(vp), count, flags);
		if (n) {
			atomicAddInt(&vp->v_vfsp->vfs_dcount, -n);
			ADD_PDCOUNT(-n);
		}
		return;
	}
#endif /* CELL_IRIX  || UNIKERNEL */

	/*
	 * The B_SWAP flag indicates that we should create buffers
	 * that cover only the page we're trying to push out.  This
	 * is only passed in in low memory situations and it is to
	 * keep us from requiring the chunk cache to allocate memory
	 * when we're out of it.
	 */
	memory_is_low = (flags & B_SWAP);
	flags &= ~B_SWAP;
	nfs3vp = 0;

	/*
	 * Assumes VOP_RWLOCK held on entry.
	 */
	s = VN_LOCK(vp);
	if (vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), nfs3_getvnodeops()))
		nfs3vp = 1;

	dirty_page_count = 0;
again:
	while (pfd = vp->v_dpages) {
		vp->v_dpages_gen++;
		vp->v_dpages = pfd->pf_vchain;
		pageflags(pfd, P_DQUEUE, 0);
		VN_UNLOCK(vp, s);

		ASSERT(pfd->pf_vp == vp);
		ASSERT(pfd->pf_flags & P_DIRTY);
		dirty_page_count++;

		if (pfd->pf_flags & P_BAD) {
			pagefree(pfd);
			goto eoloop;
		}


		if (memory_is_low) {
			/*
			 * If memory is low, it is possible that we
			 * are already holding some buffers locked.
			 * We're going to push the pages out 1 at a
			 * time in individual buffers, so just check
			 * to see if there are any locked buffers
			 * overlapping the range we're interested in.
			 * If so we'll just skip this page.
			 *
			 * This is not quite the same as the B_BUSY
			 * case below, because we can't count on the
			 * consistency of the mappings provided by
			 * the file system since we're trimming them
			 * down to single page mappings (see below).
			 */
			if (chunk_range_is_busy(vp,
						(off_t)ptod(pfd->pf_pageno),
						ptod(1))) {
				s = VN_LOCK(vp);
                                if (!(pfd->pf_flags & P_DQUEUE)) {
                                        pageflags(pfd, P_DQUEUE, 1);
                                        pfd->pf_vchain = pfd_save;
                                        pfd_save = pfd;
				} else {
					ASSERT(pfd->pf_use > 1);
					pagefree(pfd);
                                }
				/*
				 * We continue to hold the lock for
				 * the top of the loop.
				 */
				continue;
			}
		}
		offset = ctob(pfd->pf_pageno);
		resid = NBPP;
		do {
			int error;

			nmaps = 1;
			VOP_BMAP(vp, offset, resid, B_READ, 
				      get_current_cred(), &bmv, &nmaps, error);
			if (error)
				break;

			if ((n = bmv.pbsize) == 0)
				break;

			/*
			 * If the mapping is for a hole in the file,
			 * then just skip it and don't write it out.
			 */
			if (bmv.eof & BMAP_HOLE) {
				resid -= n;
				offset += n;
				continue;
			}

			/*
			 * If called with B_BUSY flag, don't try to attach
			 * to a busy buffer -- caller has some buffers locked,
			 * and we could deadlock.
			 */
                        if (flags & B_BUSY) {
                                if ((bp = findchunk(vp, bmv.offset, 0))
                                  && cpsema(&bp->b_lock)) {
                                        if (bp->b_vp == vp &&
                                             bp->b_offset == bmv.offset) {
                                                notavailchunk(bp);
                                                goto put;
                                        }
                                        vsema(&bp->b_lock);
                                }
                                s = VN_LOCK(vp);
                                if (!(pfd->pf_flags & P_DQUEUE)) {
                                        pageflags(pfd, P_DQUEUE, 1);
                                        pfd->pf_vchain = pfd_save;
                                        pfd_save = pfd;
				} else {
					ASSERT(pfd->pf_use > 1);
					pagefree(pfd);
                                }
                                goto again;
                        }

			/*
			 * Make sure that chunkread()
			 * does not decommission overlapping dirty buffers.
			 * Doing so would put more pages on the dirty list
			 * and can get us stuck in this loop forever.
			 */
			bmv.eof |= BMAP_FLUSH_OVERLAPS;

			/*
			 * We don't have a good context from which to
			 * retrieve an appropriate Policy Module here,
			 * so just set it to NULL.
			 */
			bmv.pmp = NULL;

			/*
			 * Trim the suggested buffer back to our page
			 * boundaries so that we don't force the buffer
			 * cache to allocate more memory to create a
			 * buffer for us.
			 */
			if (memory_is_low) {
				chunk_trim_to_range(ptod(pfd->pf_pageno),
						    ptod(1), &bmv);
			}

			bp = chunkread(vp, &bmv, 1, get_current_cred());
			ASSERT(!memory_is_low ||
			       ((btoct(bp->b_bufsize) == 1) &&
				(bp->b_offset >= ptod(pfd->pf_pageno)) &&
				(bp->b_offset + BTOBBT(bp->b_bcount) <=
				 ptod(pfd->pf_pageno + 1))));
			buftrace("DO_PDFLUSH", bp);

			if (bp->b_flags & B_ERROR) {
				bp->b_flags |= B_DONE|B_STALE;
				if (nfs3vp)
					cmn_err_tag(321,CE_WARN, "do_pdflush: error bp->b_vp 0x%x buffer");
				chunkrelse(bp);
				break;
			}
		put:
			ASSERT(pfd->pf_flags & P_DIRTY);

			if (vp->v_dpages && !memory_is_low) {
				if (bmv.eof & BMAP_EOF) {
					count -= pdcluster(bp, PDC_EOF);
				} else {
					count -= pdcluster(bp, 0);
				}
			}
			if (flags & B_WAIT) {
				bp->b_flags |= B_WAIT;
				bp->b_relse = chunkrelse;
			}
			if (nfs3vp) {
				if ((bp->b_flags & (B_NFS_ASYNC|B_NFS_UNSTABLE)) == 0)
					atomicAddInt(&b_unstablecnt, 1);
				bp->b_flags |= B_NFS_ASYNC;
				bp->b_flags &= ~B_NFS_UNSTABLE;
			}
			if (flags & B_DELWRI) {
				bdwrite(bp);
			} else {
				if (flags & B_ASYNC)
					bp->b_flags |= B_ASYNC;
				else {
					if (bp->b_flags & B_NFS_ASYNC)
						atomicAddInt(&b_unstablecnt, -1);
					bp->b_flags &= ~(B_NFS_ASYNC|B_NFS_RETRY);
				}
				bwrite(bp);
			}

			resid -= n;
			offset += n;

		} while (resid > 0);

		if (resid == NBPP && (pfd->pf_flags & P_BAD) == 0)
			pagebad(pfd);
		else
			--count;
		pagefree(pfd);
	eoloop:
		s = VN_LOCK(vp);

		if (count <= 0)
			break;
	}

	if (dirty_page_count) {
		atomicAddInt(&vp->v_vfsp->vfs_dcount, -dirty_page_count);
		ADD_PDCOUNT(-dirty_page_count);
	}

	/*
	 * Put back any stashed pdelwri pages.
	 */
	if (pfd_save) {
		n = 0;
		while (pfd = pfd_save) {
			ASSERT(pfd->pf_flags & P_DQUEUE);
			pfd_save = pfd->pf_vchain;
			pfd->pf_vchain = vp->v_dpages;
			vp->v_dpages = pfd;
			n++;
		}
		vp->v_dpages_gen++;
		VN_UNLOCK(vp, s);
		atomicAddInt(&vp->v_vfsp->vfs_dcount, n);
		ADD_PDCOUNT(n);
	} else {
		VN_UNLOCK(vp, s);
	}
}

void
pdflushdaemon(void)
{
	extern sema_t pdwakeup;

	/*
	 * This should NOT be vhosted - pdcount is a cell private variable.
	 */

	for ( ; ; ) {
		if (pdflag > 0) {
			pdflag = 0;
			vfs_syncall(SYNC_ATTR|SYNC_PDFLUSH|SYNC_NOWAIT);
		}

		psema(&pdwakeup, PZERO);
	}
}

void
pdflush(vnode_t *vp, uint64_t flags)
{
	/*
	 * VFLUSH flag tested and set because pdflush can be called
	 * via getchunk and gather_chunk, and could double-trip as
	 * do_pdflush calls chunkread, which calls getchunk...
	 */

	if (vp->v_flag & VFLUSH)
		return;

	VN_FLAGSET(vp, VFLUSH);

	do {
		do_pdflush(vp, 8, flags);
	} while (vp->v_dpages && dchunkbufs < chunk_ebbtide);

	VN_FLAGCLR(vp, VFLUSH);
}

void
cinit(void)
{
	extern pfn_t	physmem;
	int		i;
	extern void breservemem(pgno_t);

	spinlock_init(&chunk_plock, "chunk_plock");
	spinlock_init(&delalloc_clock, "delalloc_clock");
	spinlock_init(&delalloc_dlock, "delalloc_dlock");
	spinlock_init(&delalloc_wlock, "delalloc_wlock");

	/*
	 * set highwater marks used to determine when to push delwri
	 * buffers and when to decommission buffers.
	 */
	chunk_ebbtide = v.v_buf / 2;
	chunk_neaptide = v.v_buf - v.v_buf / 8;

	/*
	 * Guess average chunk size.  This will get adjusted
	 * by chunkinactivate over time, and used by bpactivate
	 * to return buffers to the active list.
	 * We multiply the chunk size by the (logical) number of
	 * samples we'll be keeping.
	 */
	chunk_avg_page_size_sum = btoct(CHUNK_AVG_SAMPLES * 64 * 1024);

	/*
	 * Set the minimum number of pages we'll cache in the
	 * chunk cache.  chunk_pshake() will stop shaking once
	 * we reach this level.  It can be set manually as a
	 * tunable or we'll default it to 3% of the system's memory.
	 */
	if (min_file_pages == 0) {
		min_file_pages = physmem / 33;
	}

	breservemem((pgno_t)min_file_pages);

	if (min_free_pages == 0) {
		ASSERT(tune.t_gpgshi);
                if (tune.t_gpgshi >= (btoc(50*1024*1024)))
			min_free_pages = tune.t_gpgshi * 4;
                else
			min_free_pages = tune.t_gpgshi * 2;
	}

	/*
	 * Set the hard limit on the number of delalloc buffers
	 * allowed to exist simultaneously and initialize the
	 * semaphore used to wait for them to be freed.
	 */
	if (v.v_buf > 200)
		setIntHot(&delallocleft, v.v_buf - (100 + (v.v_buf / 10)));
	else
		setIntHot(&delallocleft, v.v_buf / 2);
	sv_init(&delalloc_wait, SV_DEFAULT, "delalloc");

	/*
	 * prep delwri and dirty chunk buffers list headers
	 */
	bfreelist[0].b_dforw = bfreelist[0].b_dback = &bfreelist[0];

#ifdef DEBUG_BUFTRACE
	/*
	 * Initialize the chunk trace buffer to 256 entries.
	 */
	buf_trace_buf = ktrace_alloc(256, KM_SLEEP);
#endif

	/*
	 * Register shake routines, and initialize associated baggage.
	 */
	shake_register(SHAKEMGR_MEMORY, chunk_pshake);
	shake_register(SHAKEMGR_VM, chunk_vshake);
	mutex_init(&chunk_vshake_lock, MUTEX_DEFAULT, "bvshake");
	bvhand = bphand = bvshand = bpfhand = &global_buf_table[0];
	bplast = &global_buf_table[nbuf];

        /*
         * Initialize the per cpu clean buffer push hand
         */
        brhand = kmem_alloc((maxcpus * sizeof(buf_t *)), KM_SLEEP);
        for (i = 0; i < maxcpus; i++)
                brhand[i] = &global_buf_table[0];
}

void
chunkinvalfree(
	vnode_t	*vp)
{
        buf_t	*bp;
	buf_t	*fp;
        int	retry = 0;
	int	s;
	int	listid;

        BUFINFO.flush++;

	if (vp->v_dpages) {
		do_pdflush(vp, INT_MAX, B_BUSY);
	}

	/*
	 * We traverse one free list at a time looking for buffers
	 * associated with the given vnode.
	 */
	for (listid = 0; listid <= bfreelistmask; listid++) {
		retry = 0;
 loop:
		BUFINFO.flushloops++;
		fp = &bfreelist[listid];
		s = bfree_lock(listid);

		for (bp = fp->av_forw; bp != fp; bp = bp->av_forw) {
			if (bp->b_vp == vp) {
				if (!cpsema(&bp->b_lock)) {
					if (retry >= 0) {
						retry = 1;
						continue;
					}
					bfree_unlock(listid, s);
					psema(&bp->b_lock, PRIBIO);
					/*
					 * The bfreelock has been released.
					 * If the buffer has changed then
					 * we must start over.
					 */
					if (bp->b_vp != vp) {
						vsema(&bp->b_lock);
						goto loop;
					}
					s = bfree_lock(listid);
				} else {
					if (bp->b_vp != vp) {
						vsema(&bp->b_lock);
						continue;
					}
				}
				buftrace("CHUNKINVALFREE", bp);
				ASSERT(!(bp->b_flags & (B_STALE|B_BUSY)));
				bp->av_back->av_forw = bp->av_forw;
				bp->av_forw->av_back = bp->av_back;
				bp->b_flags |= B_BUSY;
				fp->b_bcount--;
				bfree_unlock(listid, s);

				bp->b_flags |= B_STALE;
				if (bp->b_flags & B_DELWRI) {
					bp->b_flags |= B_ASYNC;
					bp->b_relse = chunkrelse;
					bwrite(bp);
				} else {
					ASSERT(bp->b_relse);
					if (bp->b_flags & B_NFS_UNSTABLE){
						chunkcommit(bp, 0);
					} else 
						(*bp->b_relse)(bp);
				}
				goto loop;
			}
		}
		bfree_unlock(listid, s);

		if (retry > 0) {
			retry = -1;
			goto loop;
		}
	}
}

/*
 * Routine to do ordered tree walk of a vnode's buffer.
 * Passed bp is starting point of tree.  If a matching
 * buffer is found, it is returned, and if there is a superior
 * (object-address-wise) buffer, it gets returned indirectly,
 * to be used as starting point of next search.
 *
 * nextchunk() returns buffers in the range [*startp, stop)
 */
static buf_t *
nextchunk(
	vnode_t *vp,
	buf_t *buf,	/* IN */
	off_t *startp,	/* IN/OUT - starting offset / next starting offset */
	off_t stop,
	uint64_t flags,	/* IN - if non-zero, find bufs with these flags set */
	int skip,	/* IN - non-zero == skip locked buffers */
	buf_t **breturn,	/* OUT */
	int *skipped)	/* In/OUT - skipped a buffer instead of sleeping */
{
	buf_t *bp = buf;
	buf_t *blast, *bnext;
	off_t begin, end;
	off_t start = *startp;
	int nosleep;

	if (!(flags & B_BUSY) && skip == 0)
		nosleep = 0;
	else if (skip) {
		nosleep = 1;
	} else if (flags & B_BUSY) {
		/*
		 * preserve semantics of flags == B_BUSY as passed in
		 * from #ifdef TILE_DATA chunk cache code.
		 */
		nosleep = 1;
		flags &= ~B_BUSY;
	}
again:
	VN_BUF_LOCK(vp, s);
	if (bp == NULL || bp->b_vp != vp || bp->b_offset != start)
		bp = vp->v_buf;

	blast = bp;
	while (bp) {

		ASSERT(bp->b_bcount != 0);
		begin = bp->b_offset;
		end = begin + BTOBBT(bp->b_bcount);

		if (end <= start) {
			if ((bnext = bp->b_forw) && bnext != blast) {
				blast = bp;
				bp = bnext;
			} else {
				blast = bp;
				bp = bp->b_parent;
			}
			continue;
		}

		if ((bnext = bp->b_back) && bnext != blast) {
			blast = bp;
			bp = bnext;
			continue;
		}

		if (begin >= stop) {
			VN_BUF_UNLOCK(vp, s);
			return NULL;
		}

		if (flags == 0 || bp->b_flags & flags) {
			/*
			 * We've found the first buffer in the address
			 * range [start,stop) that has matching flags.
			 * Now find the next buffer in address order,
			 * flags notwithstanding.  It will be used as
			 * the starting point for the next search.
			 */
			if (bnext = bp->b_forw) {
				while (blast = bnext->b_back)
					bnext = blast;
			} else {
				/*
				 * There was no superior (b_forw) buffer.
				 * The next buffer in address order must
				 * be a parent -- stop at the first parent
				 * that is superior to bp.
				 */
				bnext = bp->b_parent;
				while (bnext) {
					if (bnext->b_offset >= end)
						break;
					bnext = bnext->b_parent;
				}
			}
			*breturn = bnext;
			if (bnext)
				*startp = bnext->b_offset;

			VN_BUF_UNLOCK(vp, s);
			if (!nosleep) {
				psema(&bp->b_lock, PRIBIO);
			} else {
				if (!cpsema(&bp->b_lock)) {
					if (skip) {
						start = end;
						*skipped = 1;
						goto again;
					} else {
						/*
						 * XXX Should indicate this
						 * happened rather than just
						 * return NULL.
						 */
						return NULL;
					}
				}
			}
		} else {
			if ((start = end) >= stop)
				break;
			continue;
		}

		/*
		 * If the buffer has changed at all, back off
		 * and start anew.
		 */
		if (bp->b_vp != vp ||
		    bp->b_offset != begin ||
		    begin + BTOBBT(bp->b_bcount) != end ||
		    flags && !(bp->b_flags & flags)) {
			vsema(&bp->b_lock);
			bp = NULL;
			goto again;
		}

		notavailchunk(bp);

		ASSERT(!(bp->b_flags & B_STALE));
		ASSERT(bp->b_flags & B_DONE);

		return bp;
	}

	VN_BUF_UNLOCK(vp, s);
	return NULL;
}

/*
 * Push buffers mapping [first,last].
 * (ie. to push 1000 bytes you'd set first=0, last=999)
 * Optionally stale the buffers.
 */

int
chunkpush(
	vnode_t *vp,
	off_t first,
	off_t last,
	uint64_t flags)
{
	off_t start;
	off_t stop;
	int wait;
	int ret = 0;
	uint64_t lflags;
	int skipped = 0;
	buf_t *bp;
	buf_t *bnext;
	int commitwait = 0;

	/*
	 * If B_ASYNC is specified, then we don't want to wait for
	 * any of the buffers to complete below.
	 * If it's not set, though, we'll set it so we can wait for
	 * them after all pages/buffers have been pushed.
	 */
	wait = !(flags & B_ASYNC);
	flags |= B_ASYNC;

	/*
	 * do_pdflush forces any pages on the v_dpages (pages for 
	 * delayed write) to be written. An msync() across a range
	 * is accomplished by tossmpages, which was called by msync1,
	 * to move the specified range to the v_dpages list.
	 */ 
	if (vp->v_dpages && !(flags & B_BDFLUSH)) {
		do_pdflush(vp, INT_MAX, flags);
	}

	/*
	 * If callers wants buffers invalidated and (optionally) pushed,
	 * grab every buffer.  But if caller only wants to push dirty
	 * buffers, ask just for delwri buffers.
	 */
	if (flags & B_STALE)
		lflags = 0;
	else
		lflags = B_DELWRI;
	/*
	 * Must push inclusively...  If caller specifies STALE,
	 * some extra buffers may get invalidated, but so what?
	 * We must convert to nextchunk()'s [start, stop) semantics.
	 */
	start = OFFTOBBT(first);
	stop = OFFTOBB(last);
	if ((last & BBMASK) == 0)
		stop++;	/* nextchunk [start, stop) semantics */
startover:
	for (bp = nextchunk(vp, (buf_t *)NULL, &start, stop,
			    lflags, 0, &bnext, &skipped);
	     bp;
	     bp = nextchunk(vp, bnext, &start, stop,
			    lflags, 0, &bnext, &skipped)) {

		ASSERT(!(bp->b_flags & B_STALE));
		buftrace("CHUNKPUSH", bp);

		if (bp->b_flags & B_DELWRI) {
			/*
			 * This buffer will never be found again on
			 * the vnode queue, but we'll still have to
			 * look (wait) again later if !B_ASYNC to
			 * ensure that this B_ASYNC write completed.
			 */
			if (flags & B_STALE)
				bp->b_flags |= B_STALE;
			/*
			 * All buffers go out asynchronously.  We wait for
			 * them below if the caller did not set B_ASYNC.
			 */
			bp->b_flags |= B_ASYNC;
			dchunkunchain(bp);
			(void)clusterwrite(bp, 0);
		} else {
			if (flags & B_STALE)
				bp->b_flags |= B_STALE;
			else {
				if (bp->b_error)
					ret = bp->b_error;
			}
			/*
			 * For non-NFS buffers, relse them.
			 * Otherwise commit the B_NFS_UNSTABLE buffers.
			 */
			if (bp->b_flags & B_NFS_UNSTABLE)
				chunkcommit(bp, commitwait);
			else 
				chunkrelse(bp);
		}

		if (start >= stop || !bnext)
			break;
	}

	/*
	 * If we're supposed to wait for all buffers in the given range
	 * to be written synchronously, then we now have to lock every
	 * buffer in the given range.  We cannot tag the ones we sent out
	 * and only wait for them, because others may have been pushed out
	 * by someone else and we must wait for those to complete as well.
	 * Locking each of the buffers simulates a biowait() on each.
	 */
	if (wait) {
		wait = 0;
		lflags = flags = 0;
		start = OFFTOBBT(first);
		stop = OFFTOBB(last);
		if ((last & BBMASK) == 0)
			stop++;	/* nextchunk [start, stop) semantics */
		if (vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), nfs3_getvnodeops()))
			commitwait = 1;
		goto startover;
	}

	/*
	 * One more pass for B_NFS_UNSTABLE buffers
	 */
	if (commitwait) {
		wait = 0;
                commitwait = 0;
                lflags = flags = 0;
                start = OFFTOBBT(first);
		stop = OFFTOBB(last);
		if ((last & BBMASK) == 0)
			stop++;	/* nextchunk [start, stop) semantics */
                goto startover;
        }

	return ret;
}

/*
 * Remove B_NFS_UNSTABLE from bufs mapping [first,last).
 * (ie. to push 1000 bytes you'd set first=0, last=999)
 */
void
chunkstabilize(
	vnode_t *vp,
	off_t first,
	off_t last)
{
	off_t start;
	off_t stop;
	int skipped = 0;
	buf_t *bp;
	buf_t *bnext;
	int count = 0;


	start = OFFTOBBT(first);
	stop = OFFTOBB(last);
	if ((last & BBMASK) == 0)
		stop++;	/* nextchunk [start, stop) semantics */

	for (bp = nextchunk(vp, (buf_t *)NULL, &start, stop,
			    B_NFS_UNSTABLE, 1, &bnext, &skipped);
	     bp;
	     bp = nextchunk(vp, bnext, &start, stop,
			    B_NFS_UNSTABLE, 1, &bnext, &skipped)) {

		buftrace("CHUNKSTABILIZE", bp);
		bp->b_flags &= ~B_NFS_UNSTABLE;
		brelse(bp);
		count--;
		
		if (start >= stop || !bnext)
			break;
	}
	atomicAddInt(&b_unstablecnt, count);
	return;
}


/*
 * bp_trim()
 *
 * This is a utility routine called by _chunkpush().  It is used to
 * reduce the length of a buffer so that it does not extend beyond
 * the given start (given as a basic block offset in the file).
 *
 * This routine assumes that all of the pages attached to the buffer
 * beyond start have already been marked P_BAD by the caller of chunktoss()
 * and that they have been removed from the vnode's v_dpages list by
 * a call to pdremove().
 *
 * ETHAN: these requirements seem to neither be met, nor required.
 * pdremove() can fail and exit early without returning an error.
 * I can't see any reason for the P_BAD requirement, either, and it
 * causes a problem if we need to trim a page from the middle of a
 * multi-page buffer. The code will currently call us without having
 * nuked the pages after the one actually being nuked.
 *
 */
static void
bp_trim(
	buf_t	*bp,
	off_t	start)
{
	pgno_t	start_page;
	pgno_t	first_buffer_page;
	int	buffer_pages;
	int	good_pages;
	int	bad_pages;
	int	pages_left;
	pfd_t	*pfdp;
#ifdef PGCACHEDEBUG
	pfd_t	*test_pfdp;
#endif

	ASSERT((bp->b_flags & (B_BUSY | B_DONE)) == (B_BUSY | B_DONE));
	buftrace("BP_TRIM", bp);

	/*
	 * If the buffer is already short enough, don't bother.
	 */
	if ((bp->b_offset + BTOBB(bp->b_bcount)) <= start) {
		return;
	}

	BUFINFO.trimmed++;

	/*
	 * Start invalidating pages from the page after the one
	 * containing the last byte we're interested in.  Just
	 * round up start to the next page boundary.
	 */
	start_page = dtop(start);
	first_buffer_page = dtopt(bp->b_offset);
	buffer_pages = btoc(bp->b_bufsize);
	good_pages = start_page - first_buffer_page;
	ASSERT((good_pages > 0) && (good_pages <= buffer_pages));
	bad_pages = buffer_pages - good_pages;
	ASSERT((bad_pages >= 0) && (bad_pages <= buffer_pages));

	/*
	 * Skip over the good pages and then free up the bad pages.
	 */
	pfdp = bp->b_pages;
	ASSERT(pfdp != NULL);
	pages_left = good_pages;
	while (pages_left > 0) {
		ASSERT(!(pfdp->pf_flags & P_BAD));
		pfdp = pfdp->pf_pchain;
		pages_left--;
	}
#if 0
#ifdef PGCACHEDEBUG
	pages_left = bad_pages;
	test_pfdp = pfdp;
	while (pages_left > 0) {
		ASSERT(test_pfdp->pf_flags & P_BAD);
		ASSERT(!(test_pfdp->pf_flags & P_DQUEUE));
		test_pfdp = test_pfdp->pf_pchain;
		pages_left--;
	}
#endif
#endif
	if (bad_pages > 0) {
		/*
		 * Free up any kernel virtual memory associated with
		 * pages being trimmed.
		 */
		if (bp->b_flags & B_MAPPED) {
			ASSERT(IS_KSEG2(bp->b_un.b_addr));
			/*
			 * Address passed to kvfree needn't be
			 * page-aligned.
			 */
			kvfree(bp->b_un.b_addr+ctob(good_pages), bad_pages);
			atomicAddInt(&bmappedcnt, -bad_pages);
		}
		pagesrelease(pfdp, bad_pages, 0);

		/*
		 * Do chunk accounting.
		 */
		if (bp->b_flags & B_DACCT) {
			ASSERT((bp->b_dforw != bp) ||
			       (bp->b_flags & B_DELALLOC));
			atomicAddInt(&dchunkpages, -bad_pages);
		}
		atomicAddInt(&chunkpages, -bad_pages);
	}

	bp->b_bufsize = ctob(good_pages);
	ASSERT(bp->b_bufsize > 0);
	bp->b_bcount = BBTOB(start - bp->b_offset);
	ASSERT((bp->b_bcount > 0) && (bp->b_bcount <= bp->b_bufsize));

	buftrace("BP_TRIM DONE", bp);
}


/*
 * Stale buffers mapping [first,last].
 */
void
chunktoss(
	vnode_t *vp,
	off_t first,
	off_t last)
{
	buf_t *bp;
	buf_t *bnext;
	off_t start;
	off_t stop;
	int skipped;
	int skip;

	/*
	 * Cull delwri page list of P_BAD pages...
	 */
	if (vp->v_dpages)
		pdremove(vp);

	/*
	 * Invalidate exclusively.
	 * Round up last to buffer boundary -- flush/toss
	 * up to but not including upper boundary.
	 */
	skip = 1;

	/*
	 * 2-pass toss algorithm.  First pass, skip locked up buffers.
	 * We want to toss everything we can without waiting.  Waiting
	 * gives bdflush and friends a chance to push more of these
	 * buffers out if they're delwri.  Second pass (if required),
	 * don't skip anything.  Toss them all.
	 */
	do {
		skipped = 0;
		start = OFFTOBBT(first);
		stop = OFFTOBB(last);
		if ((last & BBMASK) == 0)
			stop++;	/* nextchunk [start, stop) semantics */
		for (bp = nextchunk(vp, (buf_t *)NULL, &start, stop,
				    0, skip, &bnext, &skipped);
		     bp;
		     bp = nextchunk(vp, bnext, &start, stop,
				    0, skip, &bnext, &skipped)) {

			ASSERT(!(bp->b_flags & B_STALE));
			/*
			 * Buffers that overlap toss zone and are delwri must
			 * be trimmed so that they don't overlap the toss zone
			 * or tossed.
			 */
			buftrace("CHUNKTOSS", bp);
			ASSERT(bp->b_offset <= OFFTOBB(last));
			if (bp->b_flags & B_DELWRI) {
				if (bp->b_offset < OFFTOBB(first)) {
					bp_trim(bp, (off_t)OFFTOBB(first));
					chunkhold(bp);
				} else {
					bp->b_flags |= B_STALE;
					bp->b_flags &= ~B_DELWRI;
					SYSINFO.wcancel += BTOBBT(bp->b_bcount);
					chunkrelse(bp);
				}
			} else {
				bp->b_flags |= B_STALE;				
				chunkrelse(bp);
			}
			
			if (start >= stop || !bnext)
				break;
		}
		skip = 0;
	} while (skipped);
}


#ifdef _VCE_AVOIDANCE
/*
 * chunk_vrelse
 *
 * used by preg_color_validation() to break kernel
 * mappings in the buffer cache for a page for which 
 * preg_color_validation() wants to change to color.
 *
 * Remove specified pages from the chunk cache, leaving them
 * in the page pool and possibly on the vnode delayed write chain.
 *
 * chunk_vrelse is called, when vce_avoidance is true, to remove kernel
 * references to a page for which the color must be changed to satisfy
 * process vfault.  Note that this routine must not be called from a 
 * context where the vp or any of the affected chunks could be locked,
 * since a deadlock could occur.  In particular, this routine must not
 * be called in a vfault triggered by a biomove(), if user address is
 * an mmap'ed page from the same file, as could happen if a user
 * mmap'ed a file, and then read part of the file into another (or
 * even the same) part of the file.
 */
int
chunk_vrelse(
	vnode_t *vp,
	off_t first,
	off_t last)
{
	buf_t	*bp;
	buf_t	*bnext;
	off_t	start;
	off_t	stop;
	int	cnt = 0;

	start = OFFTOBBT(first);
	stop = OFFTOBB(last);

	bnext = NULL;
	while (1) {
		bp = nextchunk(vp, bnext, &start, stop, 0, 0,
			       &bnext, NULL);
		if (bp == NULL)
			break;

		buftrace("CHUNK_VRELSE", bp);
		chunk_decommission(bp,0);
		cnt++;

		if (start >= stop || !bnext)
			break;
	}
	
	return(cnt);
}

#endif /* _VCE_AVOIDANCE */


#ifdef TILE_DATA

/*
 * Find all the buffers that map this page.
 * Return a pointer to the head of the buffer list
 * with all buffers locked in bplistp.
 */
int
chunkpfind(pfd_t *pfd, buf_t **bplistp)
{
	vnode_t *vp;
	buf_t *bp, *bplist;
	auto buf_t *bnext;
	off_t start, stop;
	int s, nrefs, skipped;
	pfd_t *pfd2;

	nrefs = skipped = 0;
	bplist = NULL;

	vp = pfd->pf_vp;
	if (vp) {
		start = OFFTOBB(ctob(pfd->pf_pageno));
		stop = start + ctob(1);

		for (bp = nextchunk(vp,  NULL, &start, stop,
				    B_BUSY, 0, &bnext, &skipped);
		     bp;
		     bp = nextchunk(vp, bnext, &start, stop,
				    B_BUSY, 0, &bnext, &skipped)) {
			int npages;

			/*
			 * See if this buffer really has this page.
			 */
			pfd2 = bp->b_pages;
			for (npages = btoct(bp->b_bufsize); npages; npages--) {
				if (pfd2 == pfd) {
					/*
					 * Found page.  Put bp on bplist.
					 */
					buftrace("CHUNKPFIND", bp);
					bp->av_forw = bplist;
					bplist = bp;
					nrefs++;
 					break;
				}
				pfd2 = pfd2->pf_pchain;
			}

			/*
			 * If we didn't put this bp on bplist then
			 * release this bp.
			 */
			if (bplist != bp)
				chunkhold(bp);
			
			if (start >= stop || !bnext)
				break;
		}

		/*
		 * If the page has P_DQUEUE set it might be on
		 * the vnode dirty queue.  It might not be here
		 * if pdcluster has the page on its local list of
		 * pages to be freed soon, so check to be sure.
		 */
		if (pfd->pf_flags & P_DQUEUE) {
			s = VN_LOCK(vp);
			for (pfd2 = vp->v_dpages;
			     pfd2;
			     pfd2 = pfd2->pf_vchain) {
				if (pfd2 == pfd) {
					nrefs++;
					break;
				}
			}
			VN_UNLOCK(vp, s);
		}
	}

	*bplistp = bplist;
	return nrefs;
}


/*
 * If a buffer was mapped when we replaced one of the pages,
 * update the page table entry for the replaced page.
 */
static void
chunkbpremap(buf_t *bp, pfd_t *opfd, pfd_t *npfd, int n)
{
	int poffset;

	if (IS_KSEG2(bp->b_un.b_addr)) {
		/*
		 * If a K2 address, just replace the entry in kptbl
		 * to point to the new page.
		 */
		pde_t *pde = kvtokptbl(bp->b_un.b_addr + ctob(n));
		ASSERT(pde->pte.pg_pfn == pfdattopfn(opfd));
#ifdef MH_R10000_SPECULATION_WAR
		if (IS_R10000()) {
			__psint_t vfn = vatovfn(bp->b_un.b_addr + ctob(n));

			if (pg_isvalid(pde))
				krpf_remove_reference(pdetopfdat(pde),vfn);
			if (! pg_isnoncache(pde))
				krpf_add_reference(npfd,vfn);
		}
#endif /* MH_R10000_SPECULATION_WAR */
		pde->pte.pg_pfn = pfdattopfn(npfd);
		/* This will NEVER work on MP w/ isolated procs */
		tlbsync(0LL, allclr_cpumask, STEAL_PHYSMEM);
	} else {
		/*
		 * Not a K2 address.  Must be a single page.
		 * Have page_mapin remap npfd.  This code is
		 * like bp_mapin when npgs == 1.
		 */
		ASSERT(btoct(bp->b_bufsize) == 1 && n == 0);
#ifdef _VCE_AVOIDANCE
		if (vce_avoidance)
			bp->b_un.b_addr =
			    page_mapin(npfd,
#ifdef MH_R10000_SPECULATION_WAR
				       (IS_R10000() ? VM_DMA_RW : 0) |
#endif /* MH_R10000_SPECULATION_WAR */
				       ((bp->b_flags & B_UNCACHED) ?
					VM_UNCACHED : 0) | VM_VACOLOR,
				       pfd_to_vcolor(npfd));
		else
#endif /* _VCE_AVOIDANCE */
		bp->b_un.b_addr =
		    page_mapin(npfd,
			       ((bp->b_flags & B_UNCACHED) ? VM_UNCACHED : 0),
			       0);

		if (poffset = dpoff(bp->b_offset))
			bp->b_un.b_addr += BBTOB(poffset);

		if (IS_KSEG2(bp->b_un.b_addr))
			atomicAddInt(&bmappedcnt, 1);
	}
}


/*
 * Walk through the list of buffers replacing occurrences
 * of opfd with npfd.
 */
int
chunkpreplace(buf_t *bplist, pfd_t *opfd, pfd_t *npfd)
{
	vnode_t *vp;
	buf_t *bp, *bnext;
	pfd_t *pfd, **pfdp;
	int s, nreplaced;

	nreplaced = 0;

	/*
	 * We brought this page into the cache when we
	 * made the copy.  Flush it out now.
	 */
	if (npfd)
		_bclean_caches(0, NBPP, pfdattopfn(npfd),
			       CACH_DCACHE | CACH_ICACHE | CACH_WBACK |
			       CACH_INVAL | CACH_FORCE | CACH_NOADDR);
	else
		_bclean_caches(0, NBPP, pfdattopfn(opfd),
			       CACH_DCACHE | CACH_ICACHE | CACH_WBACK |
			       CACH_INVAL | CACH_FORCE | CACH_NOADDR);

	/*
	 * Check the list of dirty pages on the vnode.
	 */
	vp = opfd->pf_vp;
	if ((opfd->pf_flags & P_DQUEUE) && npfd) {
		s = VN_LOCK(vp);
		for (pfdp = &vp->v_dpages; *pfdp; pfdp = &pfd->pf_vchain) {
			pfd = *pfdp;
			if (pfd == opfd) {
				npfd->pf_vchain = pfd->pf_vchain;
				*pfdp = npfd;
				pageflagsinc(npfd, P_DQUEUE, 1);
				pagefree(pfd); 
				pfd->pf_vchain = NULL;
				nreplaced++;
				break;
			}
		}
		VN_UNLOCK(vp, s);
		/* opfd had P_DQUEUE so we should find it */
		ASSERT(nreplaced);
	}

	for (bp = bplist; bp; bp = bnext) {
		int n, npages;

		ASSERT(bp->b_vp == vp);
		buftrace("CHUNKPREPLACE", bp);
		bnext = bp->av_forw;

		if (!npfd) {
			/* Put the buffer back into circulation. */
			chunkhold(bp);
			continue;
		}

		/*
		 * Walk through the pages on this buffer.
		 */
		pfdp = &bp->b_pages;
		npages = btoct(bp->b_bufsize);
		for (n = 0; n < npages; n++) {
			pfd = *pfdp;
			if (pfd == opfd) {
				npfd->pf_pchain = pfd->pf_pchain;
				*pfdp = npfd;
				pageuseinc(npfd);
				pagefree(pfd); 
				nreplaced++;

				if (BP_ISMAPPED(bp))
					chunkbpremap(bp, opfd, npfd, n);

				break;
			}
			pfdp = &pfd->pf_pchain;
		}

		/* Put the buffer back into circulation. */
		chunkhold(bp);
	}

	/*
	 * If we have a new page to replace with, then
	 * opfd has been unchained.
	 */
	if (npfd)
		opfd->pf_pchain = NULL;

	return nreplaced;
}

#endif /* TILE_DATA */

#ifdef  DEBUG_BUFTRACE

buf_t *
findbuf(
        vnode_t *vp,
        pgno_t pgno)
{

        off_t   offset = ptod(pgno);
        buf_t *bp;

	VN_BUF_LOCK(vp, s);
        bp = vp->v_buf;

        while (bp) {

                if ((bp->b_offset <= offset) &&
                        (offset < (bp->b_offset + BTOBB(bp->b_bufsize)))) {
                        VN_BUF_UNLOCK(vp, s);
			printf("Found buffer 0x%x for vp 0x%x pgno 0x%x\n",
				bp, vp, pgno);
                        return bp;
                }

                /*
                 * Offset is the only necessary match here.
                 * Clients are required to purge buffers if the
                 * starting offset tags are to change -- buffer
                 * cache will catch size changes.
                 */
                if (bp->b_offset < offset) {
                        bp = bp->b_forw;
                        continue;
                }
                if (bp->b_offset > offset) {
                        bp = bp->b_back;
                        continue;
                }

                VN_BUF_UNLOCK(vp, s);
                /* buftrace("FINDCHUNK FOUND", bp); */

		printf("Found buffer 0x%x for vp 0x%x pgno 0x%x\n",
				bp, vp, pgno);
                return bp;
        }

        VN_BUF_UNLOCK(vp, s);
        return NULL;
}
#endif


/*
 * Release the B_NFS_UNSTABLE buffers linked via the b_dforw chain.
 * Release function for chunkcommit buffers.
 */
void
commitrelse(buf_t *bp)
{
	buf_t *chunkbp;

	if (bp->b_flags & B_ERROR) {
		while (chunkbp = bp->b_dforw) {
			bp->b_dforw = chunkbp->b_dforw;
			chunkbp->b_dforw = chunkbp;
			chunkbp->b_error = bp->b_error;
			chunkbp->b_flags |= (bp->b_flags & (B_STALE|B_ERROR));
			(*chunkbp->b_relse)(chunkbp);
		}
	} else {
		while (chunkbp = bp->b_dforw) {
			buftrace("COMMITRELSE LOOP", chunkbp);
			bp->b_dforw = chunkbp->b_dforw;
			chunkbp->b_dforw = chunkbp;
			if (chunkbp->b_flags & B_NFS_UNSTABLE)
				atomicAddInt(&b_unstablecnt, -1);
			chunkbp->b_flags &= ~B_NFS_UNSTABLE;
			chunkbp->b_start = lbolt;
			ASSERT(chunkbp->b_relse == chunkrelse);
			(*chunkbp->b_relse)(chunkbp);
		}
	}

	if ((bp->b_flags & B_MAPPED) && IS_KSEG2(bp->b_un.b_addr)) {
                atomicAddInt(&bmappedcnt, -btoc(bp->b_bufsize));
                kvfree(bp->b_un.b_addr, btoc(bp->b_bufsize));
        }

	freerbuf(bp);
}

 
/*
 * Cluster B_NFS_UNSTABLE buffers for faster nfs commits.
 * Flag determines if the request is done synchronously or is queued
 * for the network daemons.
 */
void
chunkcommit(buf_t *bp, int flag)
{
	buf_t	*clusterbp = NULL;
	buf_t	*nbp;
	int	n;
	vnode_t	*vp;
	int	clcount;
	off_t	offset;
	int	error;
	
	ASSERT(bp->b_flags & B_NFS_UNSTABLE);
	ASSERT(!(bp->b_flags & B_DELWRI)); 
	vp = bp->b_vp;
	ASSERT(vp != NULL);
	clcount = 0;

	if (bp->b_flags & B_DACCT) 
		dchunkunchain(bp);

	while (clcount < cwcluster) {
		n = BTOBBT(bp->b_bcount);
		offset = bp->b_offset + n;

		VN_BUF_LOCK(vp, s);		
		nbp = bp->b_forw;
		if (nbp) {
			while (nbp->b_back)
				nbp = nbp->b_back;
		} else {
			nbp = bp->b_parent;
			while (nbp && nbp->b_offset < offset) 
				nbp = nbp->b_parent;
		}
		if (!nbp || nbp->b_offset != offset) { 
			VN_BUF_UNLOCK(vp,s);
			break;
		}

		if (!(nbp->b_flags & B_NFS_UNSTABLE) || 
		    !cpsema(&nbp->b_lock)) {
			VN_BUF_UNLOCK(vp,s);
			break;
		}
		
		VN_BUF_UNLOCK(vp, s);
		
		if (!(nbp->b_flags & B_NFS_UNSTABLE)) {
			vsema(&nbp->b_lock);
			break;
		}
		
		ASSERT(!(nbp->b_flags & B_DELWRI));
		if (!clusterbp) {
			clusterbp = getrbuf(KM_NOSLEEP);
			if (!clusterbp) {
				vsema(&nbp->b_lock);
				break;
			}

			buftrace("CHUNKCOMMIT !cbp", bp);
			buftrace("CHUNKCOMMIT !cbp", nbp);
			notavailchunk(nbp);
			if (nbp->b_flags & B_DACCT)
				dchunkunchain(nbp);

			clusterbp->b_flags = B_BUSY|B_NFS_UNSTABLE;
			clusterbp->b_relse = commitrelse;
			clusterbp->b_dforw = bp;

			clusterbp->b_vp = vp;
			clusterbp->b_edev = bp->b_edev;

			clusterbp->b_offset = bp->b_offset;
			clusterbp->b_bcount = bp->b_bcount;
			
			/*
			 * Not doing any actual I/O, therefore do not need
			 * to set the b_pages list
			 */
			clusterbp->b_pages = NULL;
			clcount = btoct(bp->b_bufsize);
		} else {
			buftrace("CLUSTERCOMMIT CBP", nbp);
			notavailchunk(nbp);
			if (nbp->b_flags & B_DACCT)
				dchunkunchain(nbp);
		}

		clusterbp->b_bcount += nbp->b_bcount;
		ASSERT(nbp->b_relse == chunkrelse);

		bp->b_dforw = nbp;
		nbp->b_dforw = NULL;
		bp = nbp;

		clcount += btoct(nbp->b_bufsize);
	
	}
			
	if (!clusterbp) {
		buftrace("CHUNKCOMMIT !CBP", bp);
		clusterbp = bp;
	} else
		clusterbp->b_bufsize = ctob(btoc(clusterbp->b_bcount));

	/*
	 * This indicates that the buffer should be committed
	 * synchronously
	 */
	if (flag)
		clusterbp->b_flags &= ~B_ASYNC;
	else	
		clusterbp->b_flags |= B_ASYNC;

	error = 0;
	VOP_COMMIT(vp, clusterbp, error);

        if (error == ENOSYS) {
                cmn_err_tag(322,CE_WARN,
                        "chunkcommit(): commit vop not supported - bp (0x%x)",
                        clusterbp);
                brelse(clusterbp);
		return;
        }

	/*
	 * DO NOT reference clusterbp since it has been freed up by the 
	 * release function (commitrelse or chunkrelse) at this point.
	 */
	if (!error) 
		BUFINFO.commits++;
}

/*
 * This routine is to be called in emergency situations when chunkhold
 * finds the freemem to be less than min_free_pages. Its purpose is to 
 * try and free some memory in case the buffer cache is being filled up
 * faster than the 1 sec interval that bdflush runs at.
 */
void
chunkfree(int bufs)
{
        int             count;
        buf_t           *bp;

        for (count = nbuf/12, bp = brhand[private.p_cpuid]; --count >= 0; bp++) {

                if (chunkpages <= min_file_pages)
                        break;

		if (GLOBAL_FREEMEM() > min_free_pages)
			break;

                if (bp >= bplast)
                        bp = &global_buf_table[0];

                if (bp->b_vp && 
                    !(bp->b_flags & (B_DELWRI|B_NFS_UNSTABLE)) &&
                    cpsema(&bp->b_lock)) {

                        if (!bp->b_vp ||
                            (bp->b_flags & (B_DELWRI|B_NFS_UNSTABLE))) {
                                vsema(&bp->b_lock);
                                continue;
                        }

                        notavailchunk(bp);
                        buftrace("CHUNKFREE", bp);
                        chunkrelse(bp);

                        if (--bufs <= 0)
                                break;
                }

        }

        brhand[private.p_cpuid] = bp;
}
