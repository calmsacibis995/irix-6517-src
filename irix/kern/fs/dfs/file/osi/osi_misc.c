/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: osi_misc.c,v 65.12 1999/08/24 20:05:41 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_misc.c,v $
 * Revision 65.12  1999/08/24 20:05:41  gwehrman
 * Created osi_cv2string() function to convert an unsigned long value into
 * a hexidecimal character representation.  Fix for rfe 694510.
 *
 * Revision 65.11  1998/04/09 21:59:51  lmc
 * Put #defines around two unreachable break statements.  Put a
 * return statement back in and add the NOTREACHED to prevent
 * it from complaining.  If the return is removed, then it complains
 * that there is no return.  (These are actually turned into errors.)
 *
 * Revision 65.10  1998/04/01  14:16:05  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Removed unreachable statements.  Filled in an empty osi_strtol()
 * 	prototype.
 *
 * Revision 65.9  1998/03/24 17:20:26  lmc
 * Changed an osi_mutex_t to a mutex_t and changed osi_mutex_enter() to
 * mutex_lock and osi_mutex_exit to mutex_unlock.
 * Added a name parameter to osi_mutex_init() for debugging purposes.
 *
 * Revision 65.8  1998/03/23  16:26:21  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.7  1998/03/19 23:47:26  bdr
 * Misc 64 bit file changes and/or code cleanup for compiler warnings/remarks.
 *
 * Revision 65.6  1998/03/09  19:06:57  gwehrman
 * Removed definitions for osi_iodoneLock and osi_biowaitCond.  These are defined in SGIMIPS/osi_lock_mach.c.
 *
 * Revision 65.5  1998/02/02 22:05:14  lmc
 * Used time_t for SGI for osi_Time().
 *
 * Revision 65.2  1997/11/06  19:58:18  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:17:43  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.192.1  1996/10/02  18:11:32  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:45:02  damon]
 *
 * $EndLog$
*/
/* Copyright (C) 1996, 1989 Transarc Corporation - All rights reserved */

#ifndef KERNEL
#include <stdlib.h>
#include <values.h>
#endif

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/osi.h>
#include <dcedfs/osi_net.h>
#include <dcedfs/osi_intr.h>
#include <dcedfs/osi_buf.h>
#include <dcedfs/lock.h>
#include <dcedfs/common_data.h>

#ifdef AFS_AIX31_ENV
#include <sys/vattr.h>
#ifdef KERNEL
#include <sys/malloc.h>
#define AFS_PIN	1			/* must pin I/O buffers */
#endif
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/osi/RCS/osi_misc.c,v 65.12 1999/08/24 20:05:41 gwehrman Exp $")

#ifndef SGIMIPS
/*
 * XXX These don't really belong here.  Note that we rely on the
 * (currently true) assumption that zero-filled mutexes and cond.
 * vars. are properly initialized.  We will need to provide an interface
 * for initializing these objects explicitly if this assumption ever
 * becomes false.
 */
osi_mutex_t osi_iodoneLock;
osi_cv_t osi_biowaitCond;
#endif

/* This should really be elsewhere */
int osi_nfs_gateway_loaded = 0;
void (*osi_nfs_gateway_gc_p)() = NULL;

static struct memUsage memUsage= {0};

static struct osi_buffer *osi_freeBufferList = 0;
static osi_mutex_t osi_alloc_mutex;

static char memZero;			/* returned from osi_Alloc(0) */

#ifndef AFS_PIN
#define MAKE_BUF_OBJ(space, cast, size, flags) \
    ((space) = (cast)osi_Alloc_r(size))
#else
/* XXX -- preemption? */
#define MAKE_BUF_OBJ(space, cast, size, flags) \
    ((space) = (cast) xmalloc((size), 0, pinned_heap))
#endif /* !AFS_PIN */

#ifndef KERNEL
void
osi_Pause(long seconds)
{
    struct timespec tm;
    tm.tv_sec = seconds;
    tm.tv_nsec = 0;
    (void) pthread_delay_np(&tm);
}

long
osi_ThreadUnique(void)
{
    pthread_t self = pthread_self();
    return (long)pthread_getunique_np(&self);
}

/*
 * Initialize buf bp for disk I/O.
 */
void
osi_BufInit(
  struct buf *bp,
  int flags,			/* buf flags */
  caddr_t addr,			/* in-core data area */
  size_t length,		/* transfer length */
  dev_t device,			/* disk device */
  daddr_t dblkno,		/* disk address */
  struct vnode *vp,		/* vnode for I/O */
  osi_iodone_t (*iodone)(struct buf *)) /* iodone function */
{
    bp->b_un.b_addr = addr;
    bp->b_bcount = length;
    bp->b_blkno = dblkno;
    osi_set_bdev(bp, device);
    bp->b_vp = vp;
    bp->b_resid = bp->b_error = 0;
    osi_set_iodone(bp, iodone);

    bp->b_flags |= (B_BUSY | flags);
    bp->b_flags &= ~B_DONE;
}
#endif /* !KERNEL */

#ifdef KERNEL
/*
 * Procedure for making our processes as invisible as we can
 */
void
osi_Invisible(void)
{
#if	!defined(AFS_AIX31_ENV) && !defined(AFS_HPUX_ENV) && !defined(AFS_SUNOS5_ENV) && !defined(AFS_IRIX_ENV)
    /*
     * called once per "kernel" lwp to make it invisible
     */
    osi_curproc()->p_flag |= SSYS;
#endif
}
#endif	/* KERNEL */

#ifdef KERNEL
/*
 * Call procedure aproc with arock as an argument, in ams milliseconds
 */
int
osi_CallProc_r(
    osi_timeout_t (*aproc)(),
    char *arock,
    long ams)
{
    int code;
    /*
     * hz is in cycles/second, and timeout's 3rd parm is in cycles(barf!)
     */
#if OSI_BSD_TIMEOUT
    timeout(aproc, arock, (ams * osi_hz) / 1000 + 1);
    code = 0;
#else
    code = timeout(aproc, arock, (ams * osi_hz) / 1000 + 1);
#endif
    return code;
}

int
osi_CallProc(
    osi_timeout_t (*aproc)(),
    char *arock,
    long ams)
{
    int code;
    osi_MakePreemptionRight();
    code = osi_CallProc_r(aproc, arock, ams);
    osi_UnmakePreemptionRight();
    return code;
}
#endif	/* KERNEL */

#ifdef KERNEL
/*
 * Cancel a timeout, whether or not it has already occurred
 */
/*ARGSUSED*/
void
osi_CancelProc_r(
    osi_timeout_t (*aproc)(),
    char *arock,
    int cookie)
{
#if OSI_BSD_TIMEOUT
    untimeout(aproc, arock);
#else
    (void) untimeout(cookie);
#endif
}

void
osi_CancelProc(
    osi_timeout_t (*aproc)(),
    char *arock,
    int cookie)
{
    osi_MakePreemptionRight();
    osi_CancelProc_r(aproc, arock, cookie);
    osi_UnmakePreemptionRight();
}

/*
 * Get the time in seconds
 */
#ifdef SGIMIPS
time_t
#else
long
#endif
osi_Time(void)
{
    struct timeval tv;

    osi_GetTimeForSeconds(&tv);
    return tv.tv_sec;
}
#endif /* KERNEL */

#ifdef AFSDEBMEM
/*
 * osi_Alloc debugging info.
 */
static struct osimem *osi_memhash[OSI_MEMHASHSIZE];

#define OSI_MEMHASH(p)	((u_long)p % OSI_MEMHASHSIZE)

/* So we can maintain 8 byte alignment round up size of overhead block */
#define OSI_ALLOC_OVERHEAD ((sizeof(struct osimem) + 7) & ~7)

static struct osi_allocrec osi_allocrec_head = {
    &osi_allocrec_head, &osi_allocrec_head, 0
};

static void
update_allocrec(caddr_t caller, size_t size, int is_alloc, int type)
{
    struct osi_allocrec *rec = osi_allocrec_head.next;

    while (rec != &osi_allocrec_head && rec->caller != caller)
	rec = rec->next;

    if (rec == &osi_allocrec_head) {
	rec = osi_kalloc_r(sizeof (*rec));
	bzero((caddr_t)rec, sizeof (*rec));
	rec->caller = caller;
     } else {
	rec->next->prev = rec->prev;
	rec->prev->next = rec->next;
     }
     rec->next = osi_allocrec_head.next;
     rec->prev = &osi_allocrec_head;
     osi_allocrec_head.next->prev = rec;
     osi_allocrec_head.next = rec;
     if (is_alloc) {

	rec->total += size;
	rec->held += size;
        if (rec->held > rec->bytes_hiwat)
            rec->bytes_hiwat += size;

	rec->allocs++;
        rec->blocks++;
        if (rec->blocks > rec->allocs_hiwat)
            rec->allocs_hiwat++;

	rec->last = size;
        rec->type = type;

     } else {
	rec->held -= size;
	rec->blocks--;
    }
}
#endif /* AFSDEBMEM */

/* osi_AllocCaller_r -- is the internal version of the memory allocator.  It is
 *     public because, in addition to being called by the various interfaces in
 *     this file, it is also called from the krpch kernel module to provide
 *     memory plumbing for the kernel RPC.  (Perhaps the name of this function
 *     could be improved.)
 *
 * PARAMETERS -- Unlike the various higher level interfaces it takes all its
 *     arguments explicitly.  The "caller" argument is the return address of
 *     the allocation routine that returns this memory.  All memory allocated
 *     with the same "caller" value will be treated as a group for record
 *     keeping purposes.  A function's return address can be obtained via
 *     osi_caller().  Legal "type"s are defined in osi_alloc.h (e.g.
 *     M_UNKNOWN).  These types are used irregularly.
 *
 * LOCKS USED -- It assumes that the preemption lock is *not* held. */

opaque osi_AllocCaller_r (size_t asize, caddr_t caller, int type)
{
#ifdef	AFSDEBMEM
    struct osimem *tm;
#ifdef SGIMIPS
    size_t hash;
#else
    int hash;
#endif
#endif
#ifdef SGIMIPS
    static int inited = 0;
#else
    static inited = 0;
#endif
    opaque p;
    size_t size;

    /*
     * 0-length allocs may return NULL ptr from osi_kalloc, so we special-case
     * things so that NULL returned iff an error occurred
     */
    if (asize == 0)
	return &memZero;

    if (!inited) {
	inited = 1;
	osi_InitGeneric();
    }

    size = asize;
#ifdef AFSDEBMEM
    size += OSI_ALLOC_OVERHEAD;
#endif

    p = osi_kalloc_r(size);

#ifdef	KERNEL
    afsl_PAssert(p != NULL, ("osi_kalloc(%d) returned NULL", size));
#endif

#ifdef	AFSDEBMEM
    tm = (struct osimem *) p;
#ifdef SGIMIPS
    tm->size = (long)asize;
#else
    tm->size = asize;
#endif
    tm->caller = caller;
    hash = OSI_MEMHASH(p);
#endif

    osi_mutex_enter(&osi_alloc_mutex);
    memUsage.mu_curr_bytes += size;
    memUsage.mu_curr_allocs++;
    if (memUsage.mu_curr_bytes > memUsage.mu_hiwat_bytes)
        memUsage.mu_hiwat_bytes = memUsage.mu_curr_bytes;
    if (memUsage.mu_curr_allocs > memUsage.mu_hiwat_allocs)
        memUsage.mu_hiwat_allocs = memUsage.mu_curr_allocs;

#ifdef AFSDEBMEM
    tm->next = osi_memhash[hash];
    osi_memhash[hash] = tm;
    update_allocrec(tm->caller, tm->size, 1, type);
    p = ((caddr_t)p + OSI_ALLOC_OVERHEAD);
#endif

    osi_mutex_exit(&osi_alloc_mutex);

    return p;
}

#ifdef NEW_MALLOC
opaque
osi_Malloc(size_t asize, int type)
{
    opaque p;

    osi_MakePreemptionRight();
    p = osi_AllocCaller_r (asize, osi_caller(), type);
    osi_UnmakePreemptionRight();
    return p;
}
#endif /* NEW_MALLOC */

/*
 * Generic allocation routine
 */
opaque
osi_Alloc(size_t asize)
{
    opaque p;

    osi_MakePreemptionRight();
    p = osi_AllocCaller_r (asize, osi_caller(), M_UNKNOWN);
    osi_UnmakePreemptionRight();
    return p;
}

/* osi_Alloc_r -- the preempt-safe version of alloc. */

opaque
osi_Alloc_r(size_t asize)
{
    opaque p;
    p = osi_AllocCaller_r (asize, osi_caller(), M_UNKNOWN);
    return p;
}

opaque
osi_Zalloc(size_t asize)
{
    opaque p;

    p = osi_Alloc(asize);
    if (p != NULL) 
	bzero((caddr_t) p, asize);
    return p;
}

opaque
osi_Zalloc_r(size_t asize)
{
    opaque p;

    p = osi_Alloc_r(asize);
    if (p != NULL)
	bzero((caddr_t) p, asize);
    return p;
}

/*
 * Generic memory deallocation routine
 */
void
osi_Free(opaque p, size_t asize)
{
    osi_MakePreemptionRight();
    osi_Free_r(p,asize);
    osi_UnmakePreemptionRight();
}

/*
 * Generic memory deallocation routine
 */
void osi_Free_r(opaque p, size_t asize)
{
#ifdef AFSDEBMEM
    struct osimem *tm, **lm, *um;
#endif /* AFSDEBMEM */
    size_t size;

    if (p == &memZero) {
	afsl_Assert(asize == 0);
	return;
    }

    size = asize;
#ifdef	AFSDEBMEM
    size += OSI_ALLOC_OVERHEAD;
    tm = (struct osimem *)((caddr_t)p - OSI_ALLOC_OVERHEAD);
    lm = &osi_memhash[OSI_MEMHASH(tm)];
#endif

    osi_mutex_enter(&osi_alloc_mutex);
    memUsage.mu_curr_allocs--;
    memUsage.mu_curr_bytes -= size;

#ifdef AFSDEBMEM
    um = *lm;
    while (um != NULL && um != tm) {
	lm = &um->next;
	um = *lm;
    }
    afsl_Assert(um);			/* Not found! */
    afsl_PAssert(um->size == asize,
	("osi_Free(%x, %d): allocation size %d, from %x",
	 p, asize, um->size, um->caller));
    *lm = tm->next;
    update_allocrec(tm->caller, tm->size, 0, M_UNKNOWN);
#endif

    osi_mutex_exit(&osi_alloc_mutex);

#ifdef AFSDEBMEM
    p = (opaque)tm;
    osi_Memset((void *)p, -1, size);
#endif

    osi_kfree_r(p, size);
}

#if 0
/*
 * Dump allocated memory: for debugging purposes only
 */
void osi_Dump(void)
{
#ifdef AFSDEBMEM
    struct osimem *tm;
    long *val;
    int i, j, n;

    for (i = 0; i != OSI_MEMHASHSIZE; i++) {
	for (tm = osi_memhash[i]; tm; tm = tm->next) {
	    val = (long *) ((caddr_t)tm + OSI_ALLOC_OVERHEAD);
	    printf("block at %x size %d addr %x: ",
		   tm, tm->size, tm->caller);
	    n = MIN(5, tm->size / sizeof (*val));
	    for (j = 0; j != n; j++)
		printf("%x ", val[j]);
	    printf("\n");
	}
    }
#endif /* AFSDEBMEM */
}
#endif /* 0 */

#ifdef KERNEL
/*ARGSUSED*/
int
afscall_plumber(
  int  *cookiep,
  int size,
  int *countp,
  char *bufferp,
  int	cmd,
  int *retvalP)
{
    int error;
    int copies = 0;
    int count;
    int cookie;

    *retvalP = 0;    /* retvalP is ignored by our caller (afs_syscall()) */

    switch (cmd) {

	case PLUMBER_GET_MEM_USAGE: {
	    /*
	     * Retrieve the global counters.  Technically, we should take
             * all three mutexes which protect the fields of the memUsage
             * structure, but we have to be careful of deadlock (on a
             * multiprocessor).
	     */
            error = osi_copyout((caddr_t)&memUsage, bufferp, sizeof(memUsage));
	    return error;
#ifndef SGIMIPS
	    break;
#endif
	}

#ifdef AFSDEBMEM

	case PLUMBER_GET_MEM_RECS: {
	    /*
	     * Retrieve the allocation headers, plus some more bytes
	     * from the allocation, if desired.  The cookie value is
	     * the hash chain index.   Copy out a complete number of
             * hash chains, but not more than 'count' records.
	     */

	    struct osimem *top;
	    int i;

	    error = osi_copyin((caddr_t)countp, (caddr_t)&count, sizeof(int));
	    if (error != 0)
		return (error);

	    error = osi_copyin((caddr_t)cookiep, (caddr_t)&cookie,
			       sizeof(int));
	    if (error != 0)
		return (error);

	    if (cookie < 0 || cookie >= OSI_MEMHASHSIZE)
		return EINVAL;

	    osi_MakePreemptionRight();
            osi_mutex_enter (&osi_alloc_mutex);

            while (cookie < OSI_MEMHASHSIZE) {
		i = 0;
		for (top = osi_memhash[cookie]; top != NULL; top = top->next) {
		    if (copies == count)
			break;
		    error = copyout((caddr_t)top, bufferp, size);
		    if (error != 0)
			break;
		    copies++;
		    i++;
		    bufferp += size;
		}
		if (error)
		    break;
		/*
		 * If there is more left on this chain, pretend that none
		 * of the chain was copied out.
		 */
                if (copies == count) {
                    if (top != NULL)
			copies -= i;
                    break;
		}
	        cookie++;
	    }

            osi_mutex_exit(&osi_alloc_mutex);
	    osi_UnmakePreemptionRight();

	    if (error)
		return error;

	    error = osi_copyout((caddr_t)&cookie, (caddr_t)cookiep,
				sizeof(int));
            if (error != 0)
                return error;
            return (osi_copyout((caddr_t)&copies, (caddr_t)countp,
				sizeof(int)));
#ifndef SGIMIPS
	    break;
#endif
	}

	case PLUMBER_GET_ALLOC_RECS: {
	    /*
	     * Retrieve the per-program counter allocation records.  This
	     * list should be read all at once for consistency.  Fortunately,
	     * the list is MRU, so the most useful information is at the
	     * front.
	     */

	    struct osi_allocrec *rec = osi_allocrec_head.next;

	    error = osi_copyin((caddr_t)countp, (caddr_t)&count, sizeof(int));
	    if (error != 0)
		return (error);

	    osi_MakePreemptionRight();
            osi_mutex_enter(&osi_alloc_mutex);

	    while (rec != &osi_allocrec_head) {
		if (copies++ == count)
		    break;
		error = copyout((caddr_t)rec, bufferp, sizeof(*rec));
		if (error != 0)
		    break;
		bufferp += sizeof(*rec);
                rec = rec->next;
	    }

            osi_mutex_exit(&osi_alloc_mutex);
	    osi_UnmakePreemptionRight();

	    if (error)
		return error;
	    return (osi_copyout((caddr_t)&copies, (caddr_t)countp,
				sizeof(int)));
#ifndef SGIMIPS
	    break;
#endif
	  }

#endif /* AFSDEBMEM */

        default: {
            return EINVAL;
#ifndef SGIMIPS
            break;
#endif
        }
    }
#ifdef SGIMIPS
    /* NOTREACHED */
#endif
    return 0;
}
#endif /* KERNEL */

static struct buf *osi_bufhead;
static osi_dlock_t osi_buflock;

/*
 * osi_GetBuf -- return a struct buf for I/O.  Fixed a bug
 * here -- you better not bzero something unless you first
 * check that its not NULL.
 */
struct buf *
osi_GetBuf_r(void)
{
    struct buf *bp;

    lock_ObtainWrite_r(&osi_buflock);
    memUsage.mu_used_IO_buffers++;
    if (osi_bufhead == NULL) {
        memUsage.mu_alloc_IO_buffers++;
	lock_ReleaseWrite_r(&osi_buflock);
        MAKE_BUF_OBJ(bp, struct buf *, sizeof(struct buf), 0);
    } else {
	bp = osi_bufhead;
	osi_bufhead = bp->av_forw;
	lock_ReleaseWrite_r(&osi_buflock);
    }
    bzero((caddr_t)bp, sizeof (*bp));
    osi_bio_init(bp);
    return (bp);
}

struct buf *
osi_GetBuf(void)
{
    struct buf *bp;

    osi_MakePreemptionRight();
    bp = osi_GetBuf_r();
    osi_UnmakePreemptionRight();
    return bp;
}

/*
 * osi_ReleaseBuf -- release a buf once I/O is complete.
 */
void
osi_ReleaseBuf_r(struct buf *bp)
{
    afsl_Assert((bp->b_flags & B_DONE) != 0);
    lock_ObtainWrite_r(&osi_buflock);
    bp->av_forw = osi_bufhead;
    osi_bufhead = bp;
    memUsage.mu_used_IO_buffers--;
    lock_ReleaseWrite_r(&osi_buflock);
}

void
osi_ReleaseBuf(struct buf *bp)
{
    osi_MakePreemptionRight();
    osi_ReleaseBuf_r(bp);
    osi_UnmakePreemptionRight();
}

static osi_dlock_t osi_bufferLock;

/*
 * Free space allocated by osi_AllocBufferSpace.
 */
void
osi_FreeBufferSpace_r(struct osi_buffer *bufp)
{
    lock_ObtainWrite_r(&osi_bufferLock);
    bufp->next = osi_freeBufferList;
    osi_freeBufferList = bufp;
    memUsage.mu_used_buffers--;
    lock_ReleaseWrite_r(&osi_bufferLock);
}

void
osi_FreeBufferSpace(struct osi_buffer *bufp)
{
    osi_MakePreemptionRight();
    osi_FreeBufferSpace_r(bufp);
    osi_UnmakePreemptionRight();
}

/*
 * Allocate some (i.e. osi_BUFFERSIZE) space.
 */
static opaque
AllocBufferSpace(caddr_t caller)
{
    struct osi_buffer *bufp;
    opaque space;

    lock_ObtainWrite_r(&osi_bufferLock);
    memUsage.mu_used_buffers++;
    if (!osi_freeBufferList) {
        memUsage.mu_alloc_buffers++;
	lock_ReleaseWrite_r(&osi_bufferLock);
	space = osi_AllocCaller_r (osi_BUFFERSIZE, caller, M_BUFFER);
        return space;
    }
    bufp = osi_freeBufferList;
    osi_freeBufferList = bufp->next;
    lock_ReleaseWrite_r(&osi_bufferLock);
    return (opaque) bufp;
}

opaque
osi_AllocBufferSpace_r(void)
{
    struct osi_buffer *bufp;

    bufp = AllocBufferSpace(osi_caller());
    return bufp;
}

opaque
osi_AllocBufferSpace(void)
{
    struct osi_buffer *bufp;

    osi_MakePreemptionRight();
    bufp = AllocBufferSpace(osi_caller());
    osi_UnmakePreemptionRight();
    return bufp;
}

#if !defined(KERNEL) || defined(AFS_AIX_ENV)
/*
 * Set vattr structure to a null value.
 */
void
osi_vattr_null(struct vattr *vap)
{
	int n;
	char *cp;

	n = sizeof(struct vattr);
	cp = (char *)vap;
	while (n--) {
		*cp++ = -1;
	}
}
#endif
/*
 * To convert a long integer to a string
 */
void
osi_cv2string(u_long aval, char *buffer)
{
    char *tp, c;
    int i, j, n = 0;

    tp = buffer;
    do {
#ifdef SGIMIPS
	i = (int)(aval % 10);
#else
	i = aval % 10;
#endif
	*tp++ = '0' + i;
	aval /= 10;
    } while (aval != 0);
    *tp = '\0';
#ifdef SGIMIPS
    n = (int)(tp - buffer);
#else
    n = tp - buffer;
#endif
    for (i = 0, j = n - 1; i < j; i++, j--) {
	c = buffer[i];
	buffer[i] = buffer[j];
	buffer[j] = c;
    }
}


#ifdef SGIMIPS
/*
 * To convert a long integer to a string, hex format
 */
void
osi_cv2hstring(u_long aval, char *buffer)
{
    char *tp;
    int i;

    tp = buffer;
    *tp++ = '0';
    *tp++ = 'x';
    tp += sizeof(u_long) * 2;
    *tp-- = '\0';
    for (i = 0; i < (sizeof(u_long) * 2); i++) {
	*tp-- = "0123456789abcdef"[aval&0x0f];
	aval >>= 4;
    }
}
#endif /* SGIMIPS */


/*
 * Convert the given IP address to a string in the user's buffer.
 * Return the address of the buffer, for convenience.
 */
char *
osi_inet_ntoa_buf(struct in_addr inaddr, char *buf)
{
    register u_long ip_addr = ntohl(inaddr.s_addr);
    register char *p;

    osi_cv2string(ip_addr >> 24, buf);
    p = &buf[strlen(buf)];
    *p++ = '.';
    osi_cv2string((ip_addr >> 16) & 0xff, p);
    p = &buf[strlen(buf)];
    *p++ = '.';
    osi_cv2string((ip_addr >> 8) & 0xff, p);
    p = &buf[strlen(buf)];
    *p++ = '.';
    osi_cv2string(ip_addr & 0xff, p);
    return (buf);
}


/*
 * osi_inet_aton() -- convert a string representing an IP address to
 *     a scalar in network byte order.
 * 
 * RETURN CODES:  0 - successful,  1 - conversion failed
 */

int
osi_inet_aton(char *buf, struct in_addr *inaddr)
{
    char *scanp;
#ifdef SGIMIPS
    long i, octetval, osi_strtol(char *str, char **ptr);
#else
    long i, octetval, osi_strtol();
#endif
    unsigned long addrval;

    addrval = 0;

    for (i = 3; i >= 0; i--) {
	scanp = buf;
	octetval = osi_strtol(buf, &scanp);

	if ((scanp == buf) || (i > 0 && *scanp != '.')) {
	    return (1);
	} else {
	    addrval |= ((unsigned long)octetval) << (i << 3);
	    buf = scanp + 1;
	}
    }

#ifdef SGIMIPS
    inaddr->s_addr = (unsigned int)htonl(addrval);
#else
    inaddr->s_addr = htonl(addrval);
#endif
    return (0);
}


/*
 * Print the IP address along with some text
 */
void
osi_printIPaddr(char *preamble, u_long address, char *postamble)
{
    address = ntohl(address);
    printf("%s%d.%d.%d.%d%s", preamble,
	(int)(address >> 24), (int)(address >> 16) & 0xff,
	(int)(address >> 8) & 0xff, (int)(address) & 0xff, postamble);
#ifdef KERNEL
#ifndef SGIMIPS
    /* In Irix, uprintf is #defined to printf. Hence, dropping duplicate
     * printf.
     */
    uprintf("%s%d.%d.%d.%d%s", preamble,
	(int)(address >> 24), (int)(address >> 16) & 0xff,
	(int)(address >> 8) & 0xff, (int)(address) & 0xff, postamble);
#endif /* ! SGIMIPS */
#endif /* KERNEL */
}

/*
 * Given a power of two, return its binary logarithm.
 */
u_int
osi_log2(u_int n)
{
    u_int i = 0, m = 1;
    osi_assert((n & (n - 1)) == 0);
    while (m != n) {
	i += 1;
	m <<= 1;
    }
    return i;
}


#ifndef KERNEL
/*
 * Convert a string to an int. If string is valid it returns
 * value of int and  sets err = 1. If string is not a valid integer
 * or > MAXINT or, returns err = -1 and sets the return value to be -1.
 * Error conditions should be checked by checking the value of err, not
 * the return value, as the return value can be -1 legitimately.
 */

int
osi_atoi(char *str, int *err)
{
    long l;
    double f;
    char *ptr;

    *err = 1;

    l = strtol(str, &ptr, 0);

    if (ptr == str) {
	/* Couldn't convert str into an integer*/
	*err = -1;
	return (-1);
    }

    f = strtod (str, &ptr);

    if (ptr == str) {
	/* Couldn't convert str into a float. This should never happen */
	*err = -1;
	return (-1);
    }

    /* Since some strtol() functions return garbage for larger numbers,
    check here to see if value > MAXINT */

    if (f > MAXINT) {
	*err = -1;
	return (-1);
    }

    return (l);
}
#endif


/*
 * osi_strtol() -- a weaker version of the strtol() facility for kernel use.
 * 
 * ASSUMPTIONS: string represents a base-10 number that will fit in a long
 *
 */
long
osi_strtol(char *str, char **ptr)
{
    char *strcur;
    long strval, place;

    strval = 0;
    place  = 1;

    /* skip leading white space */
    while (*str == ' '  || *str == '\n' || *str == '\t' ||
	   *str == '\r' || *str == '\v' || *str == '\f')
	str++;

    for (strcur = str; *strcur >= '0' && *strcur <= '9'; strcur++);

    if (ptr) {
	*ptr = strcur;
    }

    for (strcur--; strcur >= str; strcur--) {
	strval += ((*strcur - '0') * place);
	place *= 10;
    }

    return strval;
}


#ifdef KERNEL
#define OSI_ZEROBUFLEN (64)
int
osi_ZeroUData_r(char *buf, long len)
{
    int code = 0;
    long zlen;
    static zeroBuf[OSI_ZEROBUFLEN];

    while (len > 0) {
	zlen = MIN(len, OSI_ZEROBUFLEN);
	code = osi_copyout_r((char *)zeroBuf, buf, zlen);
	len -= zlen;
	buf += zlen;
    }
    return (code == -1) ? EFAULT : code;
}
#endif

#ifdef OSI_TRACE_SYNC
#include <dcedfs/icl.h>

struct icl_set *osi_lockSet;
struct icl_set *osi_lockInitSet;
#endif

void osi_InitLockTrace_r()
{
#ifdef OSI_TRACE_SYNC
    struct icl_log *logp;
    long code;

#ifdef KERNEL
#define LOCK_SET_FLAGS (ICL_CRSET_FLAG_DEFAULT_ON)
#else
#define LOCK_SET_FLAGS (ICL_CRSET_FLAG_DEFAULT_ON | ICL_CRSET_FLAG_PERTHREAD)
#endif

    code = icl_CreateLogWithFlags_r("disk/lock", 0, 0, &logp);
    if (code == 0)
	code = icl_CreateSetWithFlags_r("osi/lock", logp, 0, LOCK_SET_FLAGS,
					&osi_lockSet);
    afsl_MBZ(code);
    icl_LogRele_r(logp);

    code = icl_CreateLogWithFlags_r("disk", 0, 0, &logp);
    if (code == 0)
	code = icl_CreateSetWithFlags_r("osi/lockInit", logp, 0,
					ICL_CRSET_FLAG_DEFAULT_ON,
					&osi_lockInitSet);
    afsl_MBZ(code);
    icl_LogRele_r(logp);
#endif
}

/* Instantiate these externs declared in osi.h */

afs_hyper_t osi_maxFileSizeDefault = AFS_HINIT(0,0x7fffffff);
afs_hyper_t osi_minMaxFileSizeSupported = AFS_HINIT(0,0x7fffffff);

afs_hyper_t osi_maxFileClient;		/* initialized below */
afs_hyper_t osi_maxFileServer;

afs_hyper_t osi_hZero = AFS_HINIT(0,0);
afs_hyper_t osi_hMaxint32 = AFS_HINIT(0,0x7fffffff);
afs_hyper_t osi_hMaxint64 = AFS_HINIT(0x7fffffff,-1);

/* osi_MaxFileParmToHyper -- converts the packed parameter value used with
 *     AFS_SetParams into a hyper.  The value stores two exponents that are
 *     used to determine the max file size.  If 'a' is the lowest-order byte
 *     and 'b' is the second-to-lowest-order byte (the other bytes are
 *     ignored), the max file size is (2^a - 2^b). */

afs_hyper_t osi_MaxFileParmToHyper(unsigned value)
{
    unsigned hi, lo;			/* two exponents from value */
    afs_hyper_t hih, loh;
    
    hi = OSI_MAX_FILE_GET_HIEND(value);
    lo = OSI_MAX_FILE_GET_LOEND(value);
    if (hi <= 64 && lo < hi) {

	/* Compute 2^hi - 2^lo, taking care that 1<<64 isn't defined. */

	AFS_hset64(hih, 0,1);
	loh = hih;
	if (hi < 64)
	    AFS_hleftshift(hih, hi);
	else
	    AFS_hzero(hih);
	AFS_hleftshift(loh, lo);
	AFS_hsub(hih, loh);
    } else {
	AFS_hzero(hih);
    }
    return hih;
}

/* Platform independent initialization. */

void osi_InitGeneric(void)
{
    osi_maxFileClient = osi_MaxFileParmToHyper(OSI_MAX_FILE_PARM_CLIENT);
    osi_maxFileServer = osi_MaxFileParmToHyper(OSI_MAX_FILE_PARM_SERVER);

#if defined(SGIMIPS) && defined(_KERNEL)
    osi_mutex_init (&osi_alloc_mutex, "osi_alloc");
#else
    osi_mutex_init (&osi_alloc_mutex);
#endif
}
