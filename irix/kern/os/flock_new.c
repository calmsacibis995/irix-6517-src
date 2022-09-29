/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)uts-comm:fs/flock.c	1.9"*/
#ident	"$Revision: 1.2 $"

/*
 * This file contains the new versions of the flock routines.  The old
 * versions are in flock.c.  This routine also contains stubs which
 * take you to the old or the new version.
 * 
 * All record lock lists (referenced by a pointer in the vnode) are
 * ordered by starting position relative to the beginning of the file.
 * 
 * In this file the name "l_end" is a macro and is used in place of
 * "l_len" because the end, not the length, of the record lock is
 * stored internally.  l_end designates the last byte of the locked
 * range, rather than the first byte beyond it.
 */

#include <sys/types.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <ksys/vfile.h>
#include <ksys/vproc.h>
#include <sys/flock.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/sysinfo.h>
#include <sys/uthread.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#include "flock_private.h"


/*
 * The following section of flock_new.c contains the stubs which select 
 * either the new implmentation (routines ending in _new, located in 
 * flock_new.c) or the old implementation (routines ending _old, in
 * flock.c).  At some point these stubs will go away and the _new 
 * routines will lose their suffix and become the only implementation.
 */

typedef struct {
       int flsel_new;
} flock_select_t;
#ifdef MP
#pragma set type attribute flock_select_t align=128
#endif

flock_select_t flock_select;
extern int use_old_flock;

#define FLOCK_NEWER (flock_select.flsel_new)

STATIC void     flckinit_new(void);
STATIC int	reclock_new(struct vnode *, struct flock *, int, int, off_t,
				struct cred *);
STATIC int	convoff_new(struct vnode *, struct flock *, int, off_t, off_t,
				struct cred *);
STATIC int	haslocks_new(struct vnode *, pid_t, sysid_t);
STATIC int	haslock_new(struct vnode *, struct flock *);
STATIC int	remotelocks_new(struct vnode *);
STATIC void	release_remote_locks_new(struct vnode *);
STATIC int	lock_pending_new(struct vnode *, struct flock *);
STATIC int	cancel_lock_new(struct vnode *, struct flock *);
STATIC void	cleanlocks_new(struct vnode *, pid_t, sysid_t);
STATIC void 	cancel_blocked_locks_new(__uint32_t);
#ifdef DEBUG
STATIC int	locks_pending_new(pid_t, __uint32_t);
STATIC void 	print_locks_new(struct vnode *, pid_t, __uint32_t);
#endif /* DEBUG */
 
void
flckinit(void)
{
  	if (use_old_flock == 0) {
	        flock_select.flsel_new = 1;
		flckinit_new();
	}
	else {
		flckinit_old();
	}
}


int
reclock(
	vnode_t *	vp,		/* The vnode on which the lock/unlock/
					   etc. is being done. */
	flock_t *	lckdat,		/* The request information. */
	int 		cmd,		/* Command flags. */
	int 		flag,		/* Open mode flags. */
	off_t 		offset,         /* Current file offset. */
	cred_t *	cr)		/* User credentials. */
{
        if (FLOCK_NEWER)
		return (reclock_new(vp, lckdat, cmd, flag, offset, cr));
	else
		return (reclock_old(vp, lckdat, cmd, flag, offset, cr));
}

int
convoff(struct vnode *vp,
	struct flock *lckdat,
	int whence,
	off_t offset,
	off_t limit,
	cred_t *cr)
{
        if (FLOCK_NEWER)
		return (convoff_new(vp, lckdat, whence, offset, limit, cr));
	else
		return (convoff_old(vp, lckdat, whence, offset, limit, cr));
}

int
haslocks(
	vnode_t *	vp,		/* The vnode in question. */
	pid_t 		pid, 		/* The pid to compare (or IGN_PID). */
	sysid_t 	sysid)          /* The sysid to compare. */
{
        if (FLOCK_NEWER)
		return (haslocks_new(vp, pid, sysid));
	else
		return (haslocks_old(vp, pid, sysid));
}

int
haslock(
	vnode_t *	vp,		/* The vnode in question. */
        flock_t *       ld)             /* The lock request to check. */
{
        if (FLOCK_NEWER)
		return (haslock_new(vp, ld));
	else
		return (haslock_old(vp, ld));
}

int
remotelocks(
	vnode_t *	vp)		/* The vnode in question. */
{
        if (FLOCK_NEWER)
		return (remotelocks_new(vp));
	else
		return (remotelocks_old(vp));
}

void
release_remote_locks(vnode_t *vp)
{
        if (FLOCK_NEWER)
		release_remote_locks_new(vp);
	else
		release_remote_locks_old(vp);
}

int
lock_pending(vnode_t *vp, flock_t *flp)
{
        if (FLOCK_NEWER)
		return (lock_pending_new(vp, flp));
	else
		return (lock_pending_old(vp, flp));
}



#ifdef DEBUG
int
locks_pending(
	pid_t 		pid, 		/* The pid we are to match on (or
					   IGN_PID). */
	__uint32_t 	sysid)		/* The sysid we are to match on. */
{
        if (FLOCK_NEWER)
		return (locks_pending_new(pid, sysid));
	else
		return (locks_pending_old(pid, sysid));
}

void
print_locks(
	vnode_t *	vp,		/* The vnode in question. */
	pid_t 		pid, 		/* The pid to compare (or IGN_PID). */
	__uint32_t 	sysid)          /* The sysid to compare. */
{
        if (FLOCK_NEWER)
		print_locks_new(vp, pid, sysid);
	else
		print_locks_old(vp, pid, sysid);
}

#endif /* DEBUG */

int
cancel_lock(
	vnode_t *	vp, 		/* The vnode which the specified lock
                                           must be for.  The old code does
					   not check that this is so, but 
					   does depend on this fact for its
					   locking.  Ugh! */
        flock_t *	flp)		/* The request we are trying to 
					   cancel. */
{
        if (FLOCK_NEWER)
		return (cancel_lock_new(vp, flp));
	else
		return (cancel_lock_old(vp, flp));
}

void
cancel_blocked_locks(
	__uint32_t 	sysid)		/* The sysid we need to match on. */
{
        if (FLOCK_NEWER)
		cancel_blocked_locks_new(sysid);
	else
		cancel_blocked_locks_old(sysid);
}

void
cleanlocks(
	vnode_t *	vp, 		/* Vnode for file being lcoked. */
	pid_t 		pid, 		/* The pid of the process. */
	sysid_t 	sysid)		/* The sysid for the process. */
{
        if (FLOCK_NEWER)
		cleanlocks_new(vp, pid, sysid);
	else
		cleanlocks_old(vp, pid, sysid);
}

/* ==================== End of the stub portion ==================== */




/* ================ Start of the new implementation ================ */


/*
 * Options for flock_new.c
 *
 * The following debugging options are available:
 *
 *      FL_DEBUG turns on assertions within flock.c.  Generic assertions
 *      that come in through defines in header files are not affected.
 *      Also, data structures are not affected so that debugging within
 *      flock_new.o can be turned on independently of the rest of the kernel.
 *      Note that when debug is on, the effect is the same as if FL_DEBUG
 *      has been explicitly turned on.
 *
 *      FL_CHECK turns on special checking for proper locks being held
 *      when using defines that go through various flLock and flWait
 *      lists.  This option is generally not needed but it is helpful
 *      when doing substantial developement.
 *
 *      FL_VERIFY allows special checking beyond that provided by normal
 *      assertions.  In particular, when getting and releasing a flock-
 *      related lock a comprehensive validation is done of all of the
 *      data structures governed by that lock.  This is very expensive
 *      in run-time but is useful when debugging changes to flock_new.c.
 *      The value of FL_VERIFY is used to initialize a run-time control
 *      value.  Defining FL_VERIFY as zero compiles in the checking code 
 *      but doesn't cause a big run-time expense until flVerifyOn is 
 *      set.
 *
 *      FL_DEBUGMAX is a convenience to set all debugging options
 *      to their maximum values.
 */
#if FL_DEBUGMAX
#define FL_DEBUG 1
#define FL_CHECK 1
#define FL_VERIFY 2
#endif

#if defined(DEBUG) || defined(FL_DEBUG) || defined(FL_VERIFY) || defined(FL_CHECK)
#define FL_IDBG 1
#endif

/*
 * Variables global to the file.  We put the flWaitLock into a special
 * structure for alignment purposes.
 */
typedef struct {
       mutex_t flwl_mutex;
} flwait_lock_t;
#ifdef MP
#pragma set type attribute flwait_lock_t align=128
#endif

STATIC flwait_lock_t	flWaitLockStruct;/* Governs all the flWait lists */
#define flWaitLock	flWaitLockStruct.flwl_mutex

STATIC flHash_t *	flHashTable;    /* Table of flHash's. */

STATIC int          	flHashMask;     /* Mask to select hash entry. */

STATIC zone_t *         flWaitZone;     /* Zone for flWait entries. */
STATIC zone_t *         flLockZone;     /* Zone for flLock entries. */

/*
 * Macros for basic debugging.
 *
 * We use flDebug/flAssert so that we can turn on assertions relevant to 
 * the flock mechanism without slowing the system excessively.
 */
#if defined(DEBUG) || defined(FL_DEBUG)
#define flDebug(x)	x
#define flAssert(x) 	ASSERT_ALWAYS(x)
#define flInline 
#else /* DEBUG || FL_DEBUG */
#define flDebug(x)
#define flAssert(x) 	((void) 0)
#define flInline __inline
#endif /* DEBUG || FL_DEBUG */

/*
 * Macros for structure verification
 *
 * Things under FL_VERIFY do extensive structure and list verification at
 * the drop of hat.  This is *very* expensive and is not intended for normal
 * debugging.
 */
#ifdef FL_VERIFY 

int flVerifyOn = FL_VERIFY;

#define flVerify(x)   if (flVerifyOn) { ASSERT_ALWAYS(x); }
#define flValidate(x) if (flVerifyOn) { x; }

#else /* FL_VERIFY */

#define flVerify(x)   
#define flValidate(x) 

#endif /* FL_VERIFY */

/*
 * Macros for lock checking
 *
 * Things under FL_CHECK do extensive checking of for list scanning macros
 * and other similar defines to make sure that the appropriate locks are
 * always held.  This is *very* expensive and is not intended for normal
 * debugging.
 */
#ifdef FL_CHECK

#define flCheck(a, b)	(ASSERT_ALWAYS(a), (b))

#else /* FL_CHECK */

#define flCheck(a, b)	(b)

#endif /* FL_CHECK */

/*
 * Macros for dealing with pid/sysid identifiers
 */
#define identHashPair(p, s)  		/* Computes hash of pid/sysid. */\
        (flAssert((p) != IGN_PID), (((p) ^ (s)) & flHashMask))
#define identHashFlock(f) 		/* Compute hash of pid/sysid within */\
                                        /* a flock. */\
	identHashPair((f)->l_pid, (f)->l_sysid)
#define identMatchPair(f, p, s) 	/* Checks whether a pid/sysid pair */\
                                        /* matches a flock (checks for */\
                                        /* IGN_PID). */\
        ((f)->l_sysid == (s) && \
         ((p) == IGN_PID || (f)->l_pid == (p)))
#define identMatch(ld1, ld2)            /* Checks whether the owners for */\
                                        /* two flock's match, taking into */\
	                                /* account IGN_PID for the second. */\
        (identMatchPair(ld1, (ld2)->l_pid, (ld2)->l_sysid))
#define identMatchLock(flp, ld)		/* Checks whether the owner for a */\
                                        /* flock matches that in an flLock. */\
	(identMatch(&((flp)->fll_set), (ld)) && \
	 (((flp)->fll_flags & FLL_TEMP) == 0))
#define identMatchWait(wp, ld)		/* Checks whether the owner for a */\
                                        /* flock matches that in an flWait. */\
	(identMatch(&((wp)->flw_req), (ld)))
#define identSame(ld1, ld2)		/* Checks whether the owners for */\
                                        /* two flock's are the same. */\
	((ld1)->l_pid == (ld2)->l_pid && \
	 (ld1)->l_sysid == (ld2)->l_sysid)
#define identSameLock(flp, ld)		/* Checks whether the owner for a */\
                                        /* a flock is the same as that in */\
	                                /* an flLock. */\
	identSame(&((flp)->fll_set), (ld))
#define identSameWait(wp, ld)		/* Checks whether the owner for a */\
                                        /* a flock is the same as that in */\
	                                /* an flWait. */\
	identSame(&((wp)->flw_req), (ld))

/*
 * The central lock for flock handling is the v_filocksem in the associated
 * vnodes.  This is referenced by a number of files that deal with network
 * locking, as well as os/flock_new.c.  The following encapsulation macros 
 * are only used here.
 */
#define vnodeflLock(vp)                 /* Gets flock lock for a vnode. */\
        { \
        	mutex_lock(&((vp)->v_filocksem), PZERO); \
		flValidate(validateVnode(vp)); \
	}
#define vnodeflUnlock(vp)               /* Drops flock lock for a vnode. */\
        { \
		flValidate(validateVnode(vp)); \
        	mutex_unlock(&((vp)->v_filocksem)); \
	}
#define vnodeflHeld(vp)                 /* Checks that we have the flock */\
                                        /* lock for a vnode. */\
        mutex_mine(&((vp)->v_filocksem))

/*
 * The list pointer within the vnode remains as a filock_t *, so some
 * defines are also necessary to deal with that. 
 */
#define vnodeflIfAny(vp)		/* A racey test for presence */\
                                        /* of any locks. */\
	if ((vp)->v_filocks != NULL)
#define vnodeflFirst(vp)		/* From vnode to first flLock. */\
         flCheck(vnodeflHeld(vp), ((flLock_t *) ((vp)->v_filocks)))
#define vnodeflHead(vp)                 /* From vnode to flLock head word. */\
         flCheck(vnodeflHeld(vp), ((flLock_t **) (&((vp)->v_filocks))))

/*
 * Locking macros for the global wait lock.
 */
#define wlockLock() \
        { \
		mutex_lock(&flWaitLock, PZERO); \
		flValidate(validateWlock()); \
	}
#define wlockUnlock() \
        { \
		flValidate(validateWlock()); \
		mutex_unlock(&flWaitLock); \
	}
#define wlockHeld()     mutex_mine(&flWaitLock)

/*
 * Macros for the queue of flWait's with the same hash.
 */
#define whqBase(qp) 			/* From queue element to flWait. */\
	baseof(flWait_t, flw_hashq, (qp))
#define whqAnchor(hp)                   /* Dummy wp address for end of list */\
	whqBase(kqueue_end(&(hp)->flh_wlist))
#define whqFirst(hp)			/* From flHash to first flWait. */\
	flCheck(wlockHeld(), whqBase(kqueue_first(&(hp)->flh_wlist)))
#define whqNext(wp)                     /* From flWait to next flWait */\
                                        /* with same flHash. */\
	flCheck(wlockHeld(), whqBase(kqueue_next(&(wp)->flw_hashq)))
#define whqPrev(wp)                     /* From flWait to previous flWait */\
                                        /* with same flHash. */\
	flCheck(wlockHeld(), whqBase(kqueue_prev(&(wp)->flw_hashq)))
#define whqDone(wp, hp)                 /* Sees if we are done with list. */\
	flCheck(wlockHeld(), (wp) == whqAnchor(hp))

/*
 * Macros for the queue of flWait's with the same flLock being awaited
 */
#define wlqBase(qp) 			/* From queue element to flWait. */\
	baseof(flWait_t, flw_lockq, (qp))
#define wlqAnchor(lp)			/* Dummy wp address for end of list */\
        wlqBase(kqueue_end(&(lp)->fll_waitq))
#define wlqFirst(lp)			/* From flHash to first flWait. */\
	flCheck(vnodeflHeld((lp)->fll_vnode), \
                wlqBase(kqueue_first(&(lp)->fll_waitq)))
#define wlqNext(wp)                     /* From flWait to next flWait */\
                                        /* with same flHash. */\
	flCheck(vnodeflHeld((wp)->flw_waitee->fll_vnode), \
                wlqBase(kqueue_next(&(wp)->flw_lockq)))
#define wlqPrev(wp)                     /* From flWait to previous flWait */\
                                        /* with same flHash. */\
	flCheck(vnodeflHeld((wp)->flw_waitee->fll_vnode), \
                wlqBase(kqueue_prev(&(wp)->flw_lockq)))
#define wlqDone(wp, lp)                 /* Sees if we are done with list. */\
	flCheck(vnodeflHeld((lp)->fll_vnode), \
                (wp) == wlqAnchor(lp))

/* region types */
#define	S_BEFORE	010
#define	S_START		020
#define	S_MIDDLE	030
#define	S_END		040
#define	S_AFTER		050
#define	E_BEFORE	001
#define	E_START		002
#define	E_MIDDLE	003
#define	E_END		004
#define	E_AFTER		005


STATIC int regionAlign			/* Determines region overlap pattern */
        (flock_t * ld, 			/* between an flLock and a lock */
	 flLock_t * flp);		/* specification. */
STATIC int regionOverlaps               /* Determines whether two regions */
	(flock_t * r1,                  /* have any overlap. */
         flock_t * r2);                 
STATIC int regionMatches                /* Determines whether two regions */
	(flock_t * r1,                  /* have an identical range. */
         flock_t * r2);                 
STATIC int regionConflicts              /* Determines whether two regions */
	(flock_t * r1,                  /* have any conflict. */
         flock_t * r2);                 
STATIC int regionBlocks                 /* Determines whether an flLock */
        (flLock_t * flp,                /* prevents satisfaction of a */
         flock_t * ld);                 /* a new lock request. */

STATIC void llistRemove 		/* Deletes an flLock from a lock */
	(flList_t * lock_list,          /* list. */
         flLock_t * to_remove);
STATIC void llistPrepend		/* Puts an flLock at the head of a */
	(flList_t * lock_list,          /* lock list. */
         flLock_t * to_add);
#ifdef FL_VERIFY
STATIC int llistPresent                 /* Makes sure a lock is on the given */
        (flList_t * list,               /* list. */
         flLock_t * lock);
#endif /* FL_VERIFY */

STATIC void whashInsert			/* Adds an flWait to the appropriate */
        (flWait_t * wp);                /* hash list. */
STATIC void whashRemove			/* Deletes an flWait from its hash */
        (flWait_t * wp);                /* list. */
STATIC flWait_t * whashFind             /* Find a match flWait bef using the */
        (flock_t * req);                /* hash list. */
STATIC int whashScanStart               /* Find the starting (or only) hash */
        (pid_t pid,                     /* to match a given pid/sysid pair */
	 sysid_t sysid);		/* (the pid may be IGN_PID). */

STATIC void wqueueTransfer              /* Transfer queue of flWaits from a */
	(kqueuehead_t * waitq,          /* flLock_t to an accumulating list. */
         flLock_t * waitee);
STATIC void wqueueRemove                /* Remove an flWait from the list of */
        (flWait_t * waiter,        	/* those waiting for a lock. */
         flLock_t * waitee);
STATIC void wqueueAppend		/* Adds an flWait to the list of */
        (flWait_t * waiter,        	/* those waiting for a lock. */
         flLock_t * waitee);
STATIC void wqueueShift                 /* Shifts an flWait's list from one */
        (flLock_t * dest,               /* flLock to another. */
         flLock_t * src);

STATIC int waitDeadlock                 /* Determines whether a waiting */
	(flLock_t * flp, 		/* request will deadlock. */
         flock_t * lckdat);
STATIC flWait_t * waitPrepare           /* Sets up to wait for a given lock */
        (flock_t * lckdat,              /* request to be granted or otherwise */
         flLock_t * waitee,	        /* dealt with. */
         int cmd);
STATIC int waitInterrupt                /* Deals with wait request when we */
        (flWait_t * wp,                 /* have been interrupted. */
         vnode_t * vp);
STATIC void waitCancel                  /* Cancels a waiting request. */
        (flWait_t * wp);
STATIC void waitDone                    /* Completes a waiting request. */
        (flWait_t * wp,
         vnode_t * vp,  
         int errno);
STATIC void waitFree                    /* Free the structure used for doing */
        (flWait_t * wp);                /* doing a wait. */
STATIC void waitExpunge                 /* Get rid of a waiting request when */
        (flWait_t * wp,                 /* we don't have a lock or reference */
         vnode_t * vp,                  /* to the vnode in question. */
         int hnum,                      
         vmap_t * map,
         flock_t * req);

STATIC int lockGift                     /* Disposes of a lock and tries to */
        (flLock_t * giver);             /* directly  give it to one of those */
                                        /* waiting. */
STATIC void lockDelete                  /* Gets rid of an existing lock. */
	(flLock_t * lock,
         vnode_t * vp);
STATIC void lockFree                    /* Gets rid of an flLock. */
        (flLock_t * to_free);


STATIC flLock_t * vnodeflBlocked        /* Checks a lock list to see if some */
	(vnode_t * vp,                  /* lock within a vnode's list would */
	 flock_t * req,                 /* vlock a specified request. */ 
	 flLock_t ** insertp,
	 int * flagsp);
STATIC flLock_t * vnodeflInsert		/* Create flLock and insert it in */
	(vnode_t * vp,                  /* the specified vnode's list at a */
	 flock_t * lockdat,  		/* given place. */
	 flLock_t * after);
STATIC int vnodeflAdjust                /* Adjust a vnode's lock list to */
	(vnode_t * vp,                  /* reflect a locking request. */
	 flLock_t * start, 
	 flock_t * req,
	 int flags);
STATIC int vnodeflUpdate                /* Adjust a vnode's lock list to */
	(vnode_t * vp,                  /* reflect a locking request while */
	 flLock_t * start,              /* allowing the caller to deal with */
	 flock_t * req,                 /* deletion of flLocks and reapply- */
         flList_t * dlist,              /* flWait's. */
         kqueuehead_t * wqp,
	 flLock_t ** outp);
STATIC void vnodeflDelete               /* Deletes an element from a vnode's */
	(vnode_t * vp,                  /* lock list. */
         flLock_t * flp,
         kqueuehead_t * wqp,
         flList_t * dlist);
STATIC void vnodeflReapply              /* Deal with a queue of flWait's */
	(vnode_t * vp,                  /* from deleted flLock's. */
         kqueuehead_t * wqp,
         flList_t * dlist);
STATIC void vnodeflCoalesce             /* Deal with coalesing matching */
        (vnode_t * vp,                  /* and abutting flLock's. */
         flLock_t * new); 

STATIC int reclockGet                   /* Subroutine to handle getting a */
	(vnode_t * vp,                  /* lock. */
         flock_t * lckdat,
         int cmd,
         int * were_held); 


#ifdef FL_VERIFY
STATIC void validateVnode               /* Validates the data structures */
	(vnode_t * vp);			/* associated with the vnodeflLock. */
STATIC void validateWlock(void);        /* Validates the data structures */
                                        /* associated with the wait lock. */
#endif /* FL_VERIFY */

static flid_t __sys_flid;
flid_t *sys_flid;

#ifdef FL_IDBG
void
idbg_flWait(flWait_t *wp)
{
	qprintf("flWait @ 0x%llx\n", wp);
	qprintf("lockq.next 0x%llx lockq.prev 0x%llx\n", wp->flw_lockq.kq_next,
	wp->flw_lockq.kq_prev);
	qprintf("hashq.next 0x%llx hashq.prev 0x%llx\n", wp->flw_hashq.kq_next,
		wp->flw_hashq.kq_prev);

	qprintf("flock:\ttype %d whence %d start %lld len %lld sysid %d pid %d\n",
		wp->flw_req.l_type, wp->flw_req.l_whence, wp->flw_req.l_start,
		wp->flw_req.l_len, wp->flw_req.l_sysid, wp->flw_req.l_pid);

	qprintf("\twaitee 0x%llx templock 0x%llx\n",
		wp->flw_waitee, wp->flw_tlock);
}

void
idbg_flLock(flLock_t *lp)
{
	char *lck;

	qprintf("flLock @ 0x%llx\n", lp);
	qprintf("filock:\t\n");
	qprintf("\t\tflock:\n");
	switch (lp->fll_filock.set.l_type) {
		case F_RDLCK: lck = "F_RDLCK"; break;
		case F_WRLCK: lck = "F_WRLCK"; break;
		case F_UNLCK: lck = "F_UNLCK"; break;
	}
	qprintf("\t\t\ttype %s whence %d start %lld len %lld sysid %d pid %d\n",
		lck, lp->fll_filock.set.l_whence, lp->fll_filock.set.l_start,
		lp->fll_filock.set.l_len, lp->fll_filock.set.l_sysid,
		lp->fll_filock.set.l_pid);
	qprintf("\t\tnext %llx prev %llx\n\n",
		lp->fll_filock.next, lp->fll_filock.prev);

	qprintf("waitq.next 0x%llx waitq.prev 0x%llx\n",
		lp->fll_waitq.kq_next, lp->fll_waitq.kq_prev);
	qprintf("vnode 0x%llx flags %x\n", lp->fll_vnode, lp->fll_flags);
}
#endif /* FL_IDBG */

/*
 * Initialize file locking data structures.
 */
void
flckinit_new(void)
{
        int i;

	/*
         * Size, allocate and set up flHashTable.  Because of a bug, this has
	 * never been tested with multiple hash entries.  Until it is fully
	 * tested, the better part of valor is to use only one entry.  This
	 * is not a big deal since the the old flock code only used a single
	 * entry and referred to the list *far* more often
	 */
#ifdef TestedWithMultipleHashEntries
        i = numcpus >> 1;
	while (i & (i-1))
		i--;
	if (i == 0)
		i = 1;
#else
	i = 1;
#endif
	flHashMask = i-1;
	flHashTable = (flHash_t *) kmem_zalloc(i * sizeof(flHash_t), KM_SLEEP);
	for (i = 0; i <= flHashMask; i++) {
		kqueue_init(&(flHashTable[i].flh_wlist));
	}

	/*
	 * Set up our zones.
	 */
	flWaitZone = kmem_zone_init(sizeof(flWait_t), "flWait");
	flLockZone = kmem_zone_init(sizeof(flLock_t), "flLock");

	/*
         * Initialize other global data
	 */
	mutex_init(&flWaitLock, MUTEX_DEFAULT, "flWaitLock");
	sys_flid = &__sys_flid;
	__sys_flid.fl_pid = 0;
	__sys_flid.fl_sysid = 0L;

#ifdef FL_IDBG
	idbg_addfunc("flWait", idbg_flWait);
	idbg_addfunc("flLock", idbg_flLock);
#endif
}

/*
 * regionAlign -- Determine relation between two regions
 *
 * regionAlign detrmine the type of span of a given request relative to the 
 * region specified in an flLock.

 * There are five regions:
 *
 *  S_BEFORE        S_START         S_MIDDLE         S_END          S_AFTER
 *     010            020             030             040             050
 *  E_BEFORE        E_START         E_MIDDLE         E_END          E_AFTER
 *      01             02              03              04              05
 * 			|-------------------------------|
 *
 * relative to the flLock's section.  The type is two octal digits,
 * the 8's digit is the start type and the 1's digit is the end type.
 *
 * The value returned is the two-octal-digit code.
 */
int
regionAlign(
	flock_t *	ld, 		/* Specification of area we want 
					   to know about. */
	flLock_t *	flp)            /* flLock we are using as comparand. */
{
	int 	regntype;		/* Return value we are building. */

	if (ld->l_start > flp->fll_set.l_start) {
		if (ld->l_start-1 == flp->fll_set.l_end)
			return S_END|E_AFTER;
		if (ld->l_start > flp->fll_set.l_end)
			return S_AFTER|E_AFTER;
		regntype = S_MIDDLE;
	} else if (ld->l_start == flp->fll_set.l_start)
		regntype = S_START;
	else
		regntype = S_BEFORE;

	if (ld->l_end < flp->fll_set.l_end) {
		if (ld->l_end == flp->fll_set.l_start-1)
			regntype |= E_START;
		else if (ld->l_end < flp->fll_set.l_start)
			regntype |= E_BEFORE;
		else
			regntype |= E_MIDDLE;
	} else if (ld->l_end == flp->fll_set.l_end)
		regntype |= E_END;
	else
		regntype |= E_AFTER;

	return  regntype;
}

/*
 * regionOverlaps -- Determine if there is any overlap between two regions
 *
 * This routine is used to see if two flock's represent regions that have
 * any overlap.
 *
 * The implementation of this primitive which appears below is due to 
 * Casey Leedom.  For those who, like me, do not find the correctness of
 * this to be obvious, I offer following proof:
 *
 * Let,
 *    R[12] denote the set of offset values designated by the corresponding
 *    region.
 *    R[12][SE] denote the starting and ending (l_start and l_end) for
 *    the corresponding region.
 *    R[12]S+ denote the set of offsets greater than or equal to the 
 *    corresponding R[12]S.
 *    R[12]E- denote the set of offsets less than or equal to the
 *    corresponding R[12]E.
 *
 * We need to test that: R1 intersect R2 != Empty
 * 
 * But we can express R1 as R1S+ intersect R1E- and R2 as R2S+ intersect R2E-
 *
 * Rewriting and rearanging terms, we get:
 *
 *    ((R1S+ intersect R2E-) intersect (R2S+ intersect R1E-)) != Empty
 *
 * But for this to be true, it must be that:
 *
 *    R1S+ intersect R2E- != Empty   and
 *    R2S+ intersect R1E- != Empty   and
 *    The above two sets are not disjoint
 *
 * But the first statements are exactly what is tested by the routine below
 * and so we have to show that they can't both be non-empty and still 
 * disjoint.  But for that to be the case, either we would have
 * 
 *    R1S <= R2E < R2S <= R1E   or
 *    R2S <= R1E < R1S <= R2E
 *
 * And both of these are impossible since R1S <= R1E and R2S <= R2E 
 *
 * The value returned is non-zero iff there is an overlap.
 */
__inline int
regionOverlaps(
	flock_t *	r1,		/* First region for check. */
        flock_t *       r2)             /* Second region for check. */
{
	return (r1->l_start <= r2->l_end && r1->l_end >= r2->l_start);
}

/*
 * regionMatches -- Determine if two regions have identical ranges
 *
 * This routine is used to see if two flock's represent regions that have
 * identical ranges.  The type is ignored.
 *
 * The value returned is non-zero iff there is a match.
 */
__inline int
regionMatches(
	flock_t *	r1,		/* First region for check. */
        flock_t *       r2)             /* Second region for check. */
{
	return ((r1->l_start == r2->l_start && r1->l_end == r2->l_end));
}

/*
 * regionConflicts -- Determine if there is any confict between two regions
 *
 * This routine is used to see if two flock's represent regions that have
 * any conflict.
 *
 * The value returned is non-zero iff there is a conflict.
 */
__inline int
regionConflicts(
	flock_t *	r1,		/* First region for check. */
        flock_t *       r2)             /* Second region for check. */
{
        flAssert(r1->l_type == F_RDLCK || r1->l_type == F_WRLCK);
        flAssert(r2->l_type == F_RDLCK || r2->l_type == F_WRLCK);

        return(regionOverlaps(r1,r2) &&
	       (r1->l_type == F_WRLCK || r2->l_type == F_WRLCK));
}

/*
 * regionBlocks -- Determine if an flLock prevents a request from being granted
 *
 * This routine is used to determine whether a specified flLock will block
 * a given request from being granted.  vnodeflBlocked does not use this
 * routine because it is able to take advantage of the ordering of the
 * flLock's in the vnode list.  The decision made by this routine and
 * that made by vnodeflBlocked are intended to be compatible, however.
 *
 * The value returned is non-zero if the specified flLock would prevent
 *      new request from being granted.
 */
flInline int
regionBlocks(
	flLock_t *	flp,		/* The flLock we are checking 
					   against. */
	flock_t *	req)		/* The new request. */
{
	flAssert(flp->fll_set.l_pid != IGN_PID &&
		 req->l_pid != IGN_PID);

	if (flp->fll_set.l_pid == req->l_pid &&
	    flp->fll_set.l_sysid == req->l_sysid)
		return (0);

	return (regionConflicts(&flp->fll_set, req));
}

/*
 * llistPrepend -- Put an flLock and head of a lock list
 *
 * This routine puts a specified flLock into alock list as the first element.
 * It is the responsibility of the caller to provide any necessary locking.
 */
flInline void
llistPrepend(
	flList_t * 	list,		/* The list to which we are adding. */
        flLock_t * 	lock)           /* The lock to be added. */
{
        flLock_t *      next;           /* The head of the list on entry. */

	flAssert(flLockNext(lock) == lock);

	next = *list;
        lock->fll_next = flLockFilock(next);
	lock->fll_prev = NULL;
	if (next != NULL)
	        next->fll_prev = flLockFilock(lock);
	*list = lock;
}

/*
 * llistRemove -- Take an flLock out of a lock list
 *
 * This routine removes an flLock from a given lock list.  It is the 
 * responsibility of the caller to provide any necessary locking.
 */
flInline void 
llistRemove(
	flList_t * 	list,		/* The list from which the flLock is
                                           to be extracted. */
        flLock_t * 	lock)           /* The lock to be removed. */
{
        flLock_t *	next;           /* The lock after the one we're 
                                          removing from the list. */
        flLock_t *	prev;           /* The lock beforethe one we're 
                                          removing from the list. */

	flAssert(flLockNext(lock) != lock);
	flVerify(llistPresent(list, lock));

	next = flLockNext(lock);
	prev = flLockPrev(lock);

	if (prev != NULL)
		prev->fll_next = flLockFilock(next);
	else
		*list = next;
	if (next != NULL)
		next->fll_prev = flLockFilock(prev);

	flDebug(lock->fll_next = flLockFilock(lock));
}

/*
 * llistPresent -- Make sure an flLock is on a specified flList
 *
 * This routine is used for debugging.  It checks that given flLock is on
 * a specified list.
 *
 * The value returned is non-zero if the lock is present on the list.
 */
#ifdef FL_VERIFY
int 
llistPresent(
	flList_t *	list,		/* The list to check. */
	flLock_t *      lock)           /* The flLock which should be on it. */
{
	flLock_t *      cur;            /* The item we are currently looking
                                           at. */
	for (cur = *list; cur != NULL; cur = flLockNext(cur)) {
		if (cur == lock)
			return 1;
	}
	return 0;
}
#endif /* FL_VERIFY */


/*
 * whashInsert -- Add an flWait to its hash list
 *
 * This routine puts an flWait onto the appropriate hash list based on its
 * pid/sysid.
 */
flInline void
whashInsert(
	flWait_t *	wp)		/* The flWait to be inserted. */
{
	flHash_t *	hp;		/* The proper hash entry. */

	flAssert(wlockHeld());
	flAssert(kqueue_isnull(&wp->flw_hashq));
	flVerify(whashFind(&wp->flw_req) == NULL);

	hp = &flHashTable[identHashFlock(&wp->flw_req)];
	kqueue_enter(&hp->flh_wlist, &wp->flw_hashq);
}
 
/* 
 * whashRemove -- Remove an flWait from its hash list
 *
 * This routine deletes an flWait from its hash list.
 */
flInline void
whashRemove(
	flWait_t *	wp)		/* The flWait to be removed. */
{
	flAssert(wlockHeld());
	flAssert(!kqueue_isnull(&wp->flw_hashq));
	flVerify(whashFind(&wp->flw_req) == wp);

	kqueue_remove(&wp->flw_hashq);
	flDebug(kqueue_null(&wp->flw_hashq));
}

/* 
 * whashFind -- Find an flWait whose identity matches a specified request
 *
 * This routine searches the appropriate hash list to find an flWait whose
 * pid/sysid matches the request passed in.  The pid may not be IGN_PID.
 *
 * The value returned is the address of the first flWait found or NULL
 * if none is found.
 */
flWait_t *
whashFind(
	flock_t *	req)		/* The request whose pid/sysid is 
					   to be matched. */
{
	flHash_t *	hp;		/* The proper hash entry. */
	flWait_t *	wp;             /* The current flWait. */

	flAssert(wlockHeld());

	hp = &flHashTable[identHashFlock(req)];
	for (wp = whqFirst(hp); !whqDone(wp, hp); wp = whqNext(wp)) {
		if (wp->flw_req.l_pid == req->l_pid &&
		    wp->flw_req.l_sysid == req->l_sysid)
			return (wp);
	}
	return (NULL);
}

/*
 * whashScanStart -- Find the proper starting flHash index
 *
 * This routine find the proper starting index for a scan of the flHashTable
 * based on a pid/sysid pair.  
 *
 * There are two basic cases:
 *
 *    o If a real pid is given, this will be the only entry that needs to be 
 *      scanned.  The corresponding index is returned.  The caller will 
 *      normally test for a real pid at the bottom of his for-loop and 
 *      break out of the loop.
 *
 *    o When IGN_PID is passed, zero will be returned and all entries 
 *      should be scanned by the caller's for-loop
 *
 * The value returned is the starting (or only) index for the flHashTable 
 * scan.
 */
int
whashScanStart(
	pid_t		pid,		/* Pid value we are try to match. */
	sysid_t		sysid)		/* Sysid value we are trying to 
					   match. */
{
	if (pid == IGN_PID)
		return (0);
	else
	  	return (identHashPair(pid, sysid));
}

/*
 * wqueueRemove -- Remove flWait from flLock list
 *
 * This routine removes an flWait from the list of all flWaits waiting for
 * a given flLock.
 */
/* ARGSUSED */
flInline void
wqueueRemove(
	flWait_t *      wp,             /* The flWait we are removing. */
	flLock_t *	flp)		/* The flLock containing the queue. */
{
	flAssert(wp->flw_waitee == flp);
	flAssert(vnodeflHeld(flp->fll_vnode) && wlockHeld());
	flAssert(!kqueue_isnull(&wp->flw_lockq));

	wp->flw_waitee = NULL;
	kqueue_remove(&wp->flw_lockq);
	flDebug(kqueue_null(&wp->flw_lockq));
}

/*
 * wqueueAppend -- Add flWait to flLock list
 *
 * This routine adds an flWait to the end of the list of all flWaits waiting
 * for a given flLock.
 */
flInline void
wqueueAppend(
	flWait_t *      wp,             /* The flWait we are adding. */
	flLock_t *	flp)		/* The flLock containing the queue. */
{
	flAssert(vnodeflHeld(flp->fll_vnode) && wlockHeld());
	flAssert(kqueue_isnull(&wp->flw_lockq));
	flAssert(wp->flw_waitee == NULL);

	wp->flw_waitee = flp;
	kqueue_enter_last(&flp->fll_waitq, &wp->flw_lockq);
}

/* 
 * wqueueTransfer -- Transfer list of those waiting for a lock to another list
 *
 * This routine moves the list of those waiting for an flLock to specified
 * queue which may be used to accumulate flWait's from a number of flLock's.
 */
STATIC void 
wqueueTransfer(
        kqueuehead_t *  qhp,            /* The queue head to receive the 
					   list. */
	flLock_t * 	flp)		/* The flLock whose flWait's are to
					   be transferred. */
{
	kqueue_t *	dtail;		/* The tail of the destination list. */
	kqueue_t *	shead;		/* The head of the source list. */
	kqueue_t *	stail;		/* The tail of the source list. */

	flAssert(vnodeflHeld(flp->fll_vnode));

	/*
	 * If there is nothing to transfer, we can get out now.
	 */
	if (kqueue_isempty(&flp->fll_waitq))
		return;

	/*
	 * Get the head and tail pointers to do the transfer.
	 */
	shead = kqueue_first(&flp->fll_waitq);
	stail = kqueue_last(&flp->fll_waitq);
	dtail = kqueue_last(qhp);

	/*
	 * Append to the existing list.  This will work even the existing
	 * list is empty.
	 */
	stail->kq_next = qhp;
	qhp->kq_prev = stail;
	shead->kq_prev = dtail;
	dtail->kq_next = shead;

	/*
	 * Reset the source to empty.
	 */
	kqueue_init(&flp->fll_waitq);
}

/* 
 * wqueueShift -- Transfer list of flWait's between flLock's
 *
 * This routine is used to move the list of those waiting for an flLock to 
 * another specified flLock.  If the destination already has flWait's, the
 * the transferred ones are appended to the queue.
 *
 * This routine differs from wqueueTransfer in that the destination queue
 * is part of an active flLock and that flw_waitee is updated for all 
 * flWait's moved.  For the latter reason, the wait lock must be held.
 */
STATIC void 
wqueueShift(
	flLock_t *	dest,           /* The flLock which will receive the 
					   flWait's. */
	flLock_t *      src)            /* The flLock from which the flWait's
					   will be taken. */
{
	kqueue_t *	otqp;		/* The tail of the old list. */
	kqueue_t *	nhqp;		/* The head of the list being moved. */
	kqueue_t *	ntqp;		/* The tail of the list being moved. */
	flWait_t *      wp;             /* The current flWait. */

	flAssert(src->fll_vnode == dest->fll_vnode);
	flAssert(vnodeflHeld(src->fll_vnode) && wlockHeld());

	/*
	 * If there is nothing to transfer, we can get out now.
	 */
	if (kqueue_isempty(&src->fll_waitq))
		return;

	/*
	 * Go thorugh the list of flWait's.
	 */
	for (wp = wlqFirst(src); !wlqDone(wp, src); wp = wlqNext(wp)) {
		flAssert(wp->flw_waitee == src);
		wp->flw_waitee = dest;
	}

	/*
	 * Get the head and tail of what we are going to transfer.
	 */
	nhqp = kqueue_first(&src->fll_waitq);
	ntqp = kqueue_last(&src->fll_waitq);

	/*
	 * If the destination is empty, we can simply set his head and
	 * tail and adjust our head's previous and our tail's next to 
	 * point to the new list-head.
	 */
	if (kqueue_isempty(&dest->fll_waitq)) {
		dest->fll_waitq.kq_next = nhqp;
		dest->fll_waitq.kq_prev = ntqp;
		ntqp->kq_next = &dest->fll_waitq;
		nhqp->kq_prev = &dest->fll_waitq;
	}

	/*
	 * Otherwise, we append to the existing list.
	 */
	else {
		otqp = kqueue_last(&dest->fll_waitq);
		otqp->kq_next = nhqp;
		nhqp->kq_prev = otqp;
		ntqp->kq_next = &dest->fll_waitq;
		dest->fll_waitq.kq_prev = ntqp;
	}

	/*
	 * Reset the source to empty.
	 */
	kqueue_init(&src->fll_waitq);
}

/*
 * waitDeadlock -- Determine if request would cause deadlock
 *
 * This routine determines whether waiting for a specified request would
 * result in deadlock.  A deadlock might arise, for example, when process
 * X has lock A and process Y has lock B and each of those processes
 * tries to get the lock held by the other.  In such a situation, we
 * want to make sure that one of the processes gets EDEADLCK returned
 * to avoid a situation in which both processes would wait forever.
 *
 * This routine operates with the wait lock held to prevent racing
 * processes (such as X and Y above) from either not detecting
 * a deadlock or having both processes get EDEADLCK.  When considering
 * switching from a single waitlock to a lock per hash list (probably
 * simple everywhere else), the implications for deadlock detection
 * need to be considered carefully.
 *
 * The value returned is non-zero iff there would be a deadlock.
 */
int
waitDeadlock(
	flLock_t *	flp,		/* The lock we are waiting for. */
	flock_t *	lckdat)         /* Description of our request. */
{
	pid_t 		blckpid;	/* The current waiting pid. */
	sysid_t 	blcksysid;      /* The current waiting sysid. */
	flHash_t *      hp;             /* The current hash entry. */
        flWait_t *      wp;             /* The current wait entry. */

	flAssert(wlockHeld());

	/*
	 * Find out who holds the lock we are waiting for.
	 */
	blckpid = flp->fll_set.l_pid;
	blcksysid = flp->fll_set.l_sysid;

	/*
	 * We are looking below for a wait cycle.  We proceed for each
	 * waiting pid/sysid to try to find a lock that it is waiting 
	 * for.  We then switch to looking for the pid/sysid which is
	 * holding the lock being awaited.
	 */
	while (1) {
	        flAssert(blckpid != IGN_PID);

		/*
		 * If have gotten back to the original pid/sysid, we have
		 * a wait cycle and thus need to reurn EDEADLCK.
		 */
		if (blckpid == lckdat->l_pid &&
		    blcksysid == lckdat->l_sysid)
			return 1;

		/*
		 * If the blocking process is sleeping on a locked region,
		 * change the blocked lock to this one.
		 */
		hp = &flHashTable[identHashPair(blckpid, blcksysid)];
		for (wp = whqFirst(hp); !whqDone(wp, hp); wp = whqNext(wp)) {
			if (blckpid == wp->flw_req.l_pid
			  && blcksysid == wp->flw_req.l_sysid) {
				blckpid = wp->flw_waitee->fll_set.l_pid;
				blcksysid = wp->flw_waitee->fll_set.l_sysid;
				break;
			}
		}

		/*
		 * If, thhis guy wasn't waiting for anything, then there is 
		 * no wait cycle.
		 */
		if (whqDone(wp, hp))
			return (0);
	};
}

/*
 * waitPrepare -- Prepare to await a lock
 *
 * This routine allocates an flWait and does the setup in order for a process
 * to wait for a specified lock.
 *
 * The value returned is the address of the flWait.
 */
flWait_t *
waitPrepare(
	flock_t *	req,		/* The request we are waiting to
					   satisfy. */
	flLock_t *      waitee,         /* The conflicting lock we are 
					   waiting for. */
	int             cmd)            /* Lock command flags. */
{
	flWait_t *	wp;		/* Our flWait. */

	flAssert(wlockHeld() && vnodeflHeld(waitee->fll_vnode));

	/*
	 * Allocate an flWait
	 */
	wp = (flWait_t *) kmem_zone_alloc(flWaitZone, KM_SLEEP);
	if (wp == NULL)
		return (NULL);

	/*
	 * Initialize the flWait.
	 */
	wp->flw_req = *req;
	wp->flw_err = -1;
	sv_init(&wp->flw_sv, SV_DEFAULT, "frlock");
	wp->flw_tflag = ((cmd & (SETFLCK | SETBSDFLCK)) == 0);
	wp->flw_tlock = NULL;
	flDebug(kqueue_null(&wp->flw_lockq));
	flDebug(kqueue_null(&wp->flw_hashq));
	flDebug(wp->flw_waitee = NULL);

	/*
	 * Insert the flWait in the appropriate queues.
	 */
	wqueueAppend(wp, waitee);
	whashInsert(wp);
	return (wp);
}

/*
 * waitInterrupt -- Deal with wait when interrupted
 *
 * This routine deals with the situation in which our wait for a given lock
 * has been interrupted.  We have to be very careful since the interrupt and
 * the completion of the wait may be racing.
 *
 * The value returned is an the error code for the request.
 */
STATIC int
waitInterrupt(
	flWait_t *	wp, 		/* The flWait for the request. */
	vnode_t *       vp)             /* The vnode on which we were trying
					   to do a lock. */
{
        int             retval;         /* The error code to return. */

	/*
	 * Get the proper locks.
	 */
	vnodeflLock(vp);
	wlockLock();

	/*
	 * Get the return value.  If none has been stored, we have to 
	 * remove the flWait from the queues.
	 */
	retval = wp->flw_err;
	if (retval == -1) {
	        retval = EINTR;
		flDebug(wp->flw_err = EINTR);
		wqueueRemove(wp, wp->flw_waitee);
		whashRemove(wp);
	}

	/*
	 * Release locks and return.
	 */
	wlockUnlock();
	vnodeflUnlock(vp);
	return (retval);
}

/*
 * WaitDone -- Mark wait completed
 *
 * This routine indicates to the origianl issuer that the request for 
 * which he is waiting has been completed.  When it has been completed 
 * successfully (errno == 0), he may assume that he has the requested 
 * lock.
 */
/* ARGSUSED */
flInline void
waitDone(
	flWait_t *	wp,		/* The flWait to be freed. */
	vnode_t *       vp,             /* The vnode we were waiting to lock. */
	int		errno)		/* The rror code to return. */
{
        flAssert(vnodeflHeld(vp));
	flAssert(kqueue_isnull(&wp->flw_lockq));
	flAssert(kqueue_isnull(&wp->flw_hashq));
	flAssert(wp->flw_waitee == NULL);
	flAssert(wp->flw_err == -1);

	wp->flw_err = errno;
	sv_broadcast(&wp->flw_sv);
}
  	
/*
 * waitCancel -- Cancel an existing wait request
 *
 * This routine is used to abort a waiting lock request.
 */
void
waitCancel(
	flWait_t *	wp)		/* The flWait for the request to
					   be aborted. */
{
        vnode_t *       vp;             /* The vnode for which a lock 
					   is being attempted. */
	
	vp = wp->flw_waitee->fll_vnode;
	flAssert(wlockHeld() && vnodeflHeld(vp));
	
	wqueueRemove(wp, wp->flw_waitee);
	whashRemove(wp);
	waitDone(wp, vp, EINTR);
}

/*
 * waitFree -- Get rid of flWait
 *
 * This routine is responsible for deallocating an flWait structure.  This
 * routine should only be called by the process that is waiting.
 */
flInline void
waitFree(
	flWait_t *	wp)		/* The flWait to be freed. */
{
	flAssert(kqueue_isnull(&wp->flw_lockq));
	flAssert(kqueue_isnull(&wp->flw_hashq));
	flAssert(wp->flw_waitee == NULL);
	flAssert(wp->flw_err != -1);
	
	sv_destroy(&wp->flw_sv);
	kmem_zone_free(flWaitZone, wp);
}

/*
 * waitExpunge -- Cancel a wait when we don't have the vnode
 *
 * This routine is used to cancel a pending wait in situations in which
 * we are scanning through a hash list without any lock on the target
 * vnode or even a reference to it.  We want to make sure that the 
 * wait is gone if the completion should race ahead of us while we are
 * getting access to the vnode.
 */
void
waitExpunge(
	flWait_t *	wp,		/* The flWait we are trying to get
					   rid of. */
	vnode_t *       vp,             /* Vnode pointer gotten with the
					   wait lock held. */
	int             hx,             /* The hash index for the list on
					   which we found the flWait. */
	vmap_t *	map,            /* Vnode identification gotten with
					   the wait lock held. */
	flock_t *       reqp)           /* Copy of the request gotten with
					   the wait lock held. */
{
        flHash_t *      hp;             /* FlHash we will scan. */
	flWait_t *      cur;            /* The current flWait for the scan. */

	/*
	 * Get the vnode.  If we can't get it, we have "lost" the race
	 * and have nothing to do.
	 */
	vp = vn_get(vp, map, 0);
	if (vp == NULL)
		return;

	/*
	 * Now that we have the vnode, we can get the locks in the 
	 * proper order.
	 */
	vnodeflLock(vp);
	wlockLock();

	/*
	 * Now scan down the list for the flWait we want to expunge.
	 * Since we didn't have the wait lock for a while, it may not 
	 * be present or even may have been recycled to another use.
	 */
	hp = &flHashTable[hx];
	for (cur = whqFirst(hp); !whqDone(cur, hp); cur = whqNext(cur)) {
	        if (cur != wp)
			continue;

		/*
		 * Now that we have found what we are looking for, make sure
		 * it is still the same request (i.e. that the flWait has
		 * not been re-used for a different request).  We have
		 * to be careful that we don't have a lock request that
		 * looks exactly like the old one but is for a different
		 * vnode.
		 */
		if (bcmp(&wp->flw_req, reqp, sizeof(flock_t)) == 0 &&
		    cur->flw_waitee->fll_vnode == vp) {
			wqueueRemove(wp, wp->flw_waitee);
			whashRemove(wp);
			waitDone(wp, vp, EINTR);
		}
		break;
	}

	/*
	 * Release the locks and the reference.
	 */
	wlockUnlock();
	vnodeflUnlock(vp);
	VN_RELE(vp);
}

/*
 * lockFree -- Get rid of an flLock
 *
 * Used to deallocate an flLock structure.  The caller is responsible for
 * removing the fl_lock from any lists in which it had previously been 
 * stored.
 */
flInline void
lockFree(
	flLock_t * 	lock)		/* The flLock to be freed. */
{
        flAssert(kqueue_isempty(&lock->fll_waitq));
	kmem_zone_free(flLockZone, lock);
	SYSINFO.reccnt--;
}

/*
 * lockDelete -- Get rid of a lock
 *
 * This routine gets rid of a specified lock and makes sure that anybody 
 * waiting for it is started.
 */
void
lockDelete(
	flLock_t * 	lock,		/* The flLock to be deleted. */
	vnode_t * 	vp)		/* The vnode the lock applies to. */
{
	kqueuehead_t	wlist;          /* Used to hold flWait list. */
	flList_t        dlist = NULL;   /* Deletion list. */

	/*
	 * Try the simple path first.
	 */
	if (lockGift(lock))
		return;

	/*
         * Otherwise, we will do this like a trivial version of 
	 * vnodeflAdjust.
	 */
	kqueue_init(&wlist);
	vnodeflDelete(vp, lock, &wlist, &dlist);
	vnodeflReapply(vp, &wlist, &dlist);
}

/*
 * lockGift -- Try to move deleted lock directly to a waiter
 *
 * This routine tries to implement a fast path for the deletion of a 
 * lock, transferring the lock to a single waiting process without
 * deallocating and reallocating an flLock.
 *
 * The value returned is non-zero if we succeeded in doing the transfer.
 */
int
lockGift(
	flLock_t *	flp)		/* The lock being deleted. */
{
	flWait_t *      wp;             /* The candidate for the lock. */
	vnode_t *       vp;             /* The vnode which is being unlocked
					   and locked. */
	int 		temp;           /* Whether we created a temporary
					   lock. */

	vp = flp->fll_vnode;
	flAssert(vnodeflHeld(vp));

	/*
         * If it's not a write lock, or it is potentially shared, it would 
	 * be too hard to transfer.
	 */
	if (flp->fll_set.l_type != F_WRLCK ||
	    (flp->fll_flags & FLL_SHARE))
		return (0);

	/*
         * See if someone is waiting.  If not, do a null transfer and 
	 * return an indication that the caller does not need to do 
	 * anything.  Note that since the wait lock and the vnodefl 
         * lock are required to change the wait queues, the fact that 
	 * we hold the vnodefl lock allows us to inspect them with the 
	 * assurance that they won't change.
	 */
	wp = wlqFirst(flp);
	if (wlqDone(wp, flp)) {
	        llistRemove(vnodeflHead(vp), flp);
		lockFree(flp);
		return (1);
	}

	/*
         * If the first waiter is not an exact match, no transfer.
	 */
	if (wp->flw_req.l_type != F_WRLCK ||
	    wp->flw_req.l_start != flp->fll_set.l_start ||
	    wp->flw_req.l_end != flp->fll_set.l_end)
		return (0);

	/*
	 * Now do the transfer.  The reason that we don't have to check 
	 * for a deadlock at this point is that everyone who is waiting
	 * is waiting for a process which cannot itself be waiting.  We 
	 * know this because a process may have only one wait at a time
	 * and that one wait that it had has just been satisfied.  Since
	 * we have the wait lock, there can be no unsatisfied waits so 
	 * there can be no deadlock.
	 */
	wlockLock();
	flp->fll_set.l_pid = wp->flw_req.l_pid;
	flp->fll_set.l_sysid = wp->flw_req.l_sysid;
	flp->fll_flags &= ~FLL_TEMP;
	temp = wp->flw_tflag;
	if (temp) {
		wp->flw_tlock = flp;
		flp->fll_flags |= FLL_TEMP;
	}
	wqueueRemove(wp, flp);
	whashRemove(wp);
	waitDone(wp, vp, 0);
	if (temp == 0)
		vnodeflCoalesce(vp, flp);
	wlockUnlock();
	return (1);
}

/*
 * vnodeflInsert -- create and insert an flLock in a vnode lcok list
 *
 * Insert lock (lckdat) after given lock (fl); If fl is NULL place the
 * new lock at the beginning of the vnode's list.
 *
 * The value returned is the address of flLock created.  A tradition
 * from the old insflck is that people check for NULL but no effort 
 * ever was made to deal cleanly with this situation.
 */
flLock_t *
vnodeflInsert(
	vnode_t *	vp, 		/* The vnode in whose lock list the
					   flLock is to be placed. */
        flock_t *	lckdat,         /* Description of the request. */
	flLock_t *	fl)		/* The insertion point. */
{
	flLock_t *      new;		/* The flLock we are building and
					   inserting. */
	flLock_t *      next;		/* The flLock which is to follow
					   the new one in the list. */

	flAssert(vnodeflHeld(vp));
	flVerify(fl == NULL || llistPresent(vnodeflHead(vp), fl));

	/*
	 * Allocate an flLock.
	 */
	new = (flLock_t *) kmem_zone_alloc(flLockZone, KM_SLEEP);
	flAssert(new != NULL);
	++SYSINFO.reccnt;

	/*
	 * Initialize the flLock.
	 */
	new->fll_set = *lckdat;
	new->fll_vnode = vp;
	new->fll_flags = 0;
	kqueue_init(&new->fll_waitq);

	/*
	 * Put the flLock in the vnode lock list.
	 */
	if (fl == NULL) {
		flDebug(new->fll_next = flLockFilock(new));
		llistPrepend(vnodeflHead(vp), new);
	} 
	else {
		next = flLockNext(fl);

		new->fll_next = flLockFilock(next);
		if (next != NULL)
			next->fll_prev = flLockFilock(new);
		fl->fll_next = flLockFilock(new);
		new->fll_prev = flLockFilock(fl);
	}

	return new;
}

/*
 * vnodeflDelete -- Delete flLock from a vnode's lock list
 *
 * Deletes a specified flLock from a vnode lock list.  Any waiting flWait's 
 * are transferred to a specified list of such for later disposition.
 */
void
vnodeflDelete(
	vnode_t *	vp,		/* The vnode from which the flLock
                                           is to be deleted. */
        flLock_t *      flp,            /* The lock being deleted. */
        kqueuehead_t *  wqp,            /* The wait queue to receive the
					   flWait list. */
	flList_t *      dlist)          /* The deletion list for later
					   freeing of the locks. */
{
	flAssert(vnodeflHeld(vp));

	llistRemove(vnodeflHead(vp), flp);
	wqueueTransfer(wqp, flp);
	llistPrepend(dlist, flp);
}

/*
 * vnodeflReapply -- Reapply waiting request to new state of locks
 *
 * This routine is used by vnodeflAdjust to apply flWaits for locks that
 * were deleted or otherwise modified so that they can either be granted
 * or properly queued without the expense of waking up a thundering herd
 * of waiters.
 */
void
vnodeflReapply(
	vnode_t *	vp,		/* The vnode whoase locks are being
					   modified. */
        kqueuehead_t *  wqp,            /* List of flWait's that need to
					   be dealt with. */
        flList_t *      dlist)          /* List of flLock's that can be 
					   deallocated once the flWait's
					   (which used to be waiting for 
					   them) are dealt with. */
{
  	flWait_t *	wp;		/* Current flWait. */
  	flLock_t * 	flp = NULL;	/* Lock resulting from an flWait 
					   which succeeded. */
	flLock_t *      found;          /* Lock found which blocks the 
					   current flWait from succeeding. */
        flLock_t *      insert;         /* Insertion point for current
					   lock. */
	int             flags;          /* Flags for vnodeflBlocked. */

	flAssert(vnodeflHeld(vp));

	/*
	 * Check for the case in which there is no wait queue so we avoid
	 * waiting for a lock which we don't need.  I don't consider this
	 * this goto harmful.  I'd code an if the indent were four, however.
	 */
	if (kqueue_isempty(wqp))
		goto free_locks;

	/*
	 * Go through the flWait's and try to do something with each one.
	 * This may means satisfying or aborting the request or queueing
	 * it waiting for different waitee.
	 */
	wlockLock();
	while (!kqueue_isempty(wqp)) {
	        kqueue_t *	qp;

		/*
		 * Get the first flWait and remove it from the list.
		 */
		qp = kqueue_first(wqp);
		wp = wlqBase(qp);
		kqueue_remove(qp);
		flDebug(kqueue_null(qp));
		flDebug(wp->flw_waitee = NULL);

		/*
		 * Get the flags set up.
		 */
		flags = 0;
		if (wp->flw_tflag)
			flags = FLL_TEMP;

		/*
		 * If we have satisfied a waiting request, see if this
		 * request is blocked by that one.  If so, we can just
		 * have the current request wait on that one.
		 * This allows us to avoid deadlock detection since
		 * we are waiting for an flLock and we know that 
		 * there cannot be a waiting request with the same
		 * identity since there can only be one such for any
		 * identity (pid/sysid pair) and that one was just
		 * converted to non-waiting status and the wait lock
		 * has not been given up since.
		 */
		if (flp && regionBlocks(flp, &wp->flw_req)) {
			wqueueAppend(wp, flp);
			continue;
		}


		/*
		 * If we would block on some other request, then
		 * we'll wait for that one.  First, we check for 
		 * a deadlock and abort the request if we find one.
		 */
		found = vnodeflBlocked(vp, &wp->flw_req, &insert, &flags);
		if (found != NULL) {
		        if (waitDeadlock(found, &wp->flw_req)) {
				whashRemove(wp);
				waitDone(wp, vp, EDEADLK);
				continue;
			}
			wqueueAppend(wp, found);
			continue;
		}

		/*
		 * Otherwise we've won.  Insert our new lock and
		 * and mark the wait as completed.  In the case of
		 * a non-temporary request, we need to call 
		 * vnodeflUpdate since the newly-satisfiable request
		 * may cause non-trivial revision the process's
		 * lock structure (replace read locks by write, 
		 * splitting locked regions, etc.).
		 */
		whashRemove(wp);
		if (flags & FLL_TEMP) {
			flp = vnodeflInsert(vp, &wp->flw_req, insert);
			if (flp == NULL) {
			        waitDone(wp, vp, ENOLCK);
				continue;
			}
			flp->fll_flags |= flags;
			wp->flw_tlock = flp;
			waitDone(wp, vp, 0);
		}
		else {
		        int	retv;

		        retv = vnodeflUpdate(vp, insert, &wp->flw_req, 
					     dlist, wqp, &flp);
			if (flp != NULL && (flags & FLL_SHARE)) 
				flp->fll_flags |= FLL_SHARE;
			waitDone(wp, vp, retv);
		}
	}
	wlockUnlock();

	/*
	 * Now that we are done with the flWait's we can get rid of 
	 * the flLock's that they may have been pointing to.
	 */
free_locks:
	while ((flp = *dlist) != NULL) {
		llistRemove(dlist, flp);
		lockFree(flp);
	}
}

/*
 * vnodeflAdjust -- Adjust a vnode's locking list
 *
 * This routine is a wrapper around vnodeflUpdate in order to easily handle
 * the normal non-waiting case.
 *
 * The value returned is an error code.
 */ 
int
vnodeflAdjust(
	vnode_t *	vp,		/* The vnode being locked/unlocked. */
	flLock_t *	insrtp,		/* The initial guess at the insertion
					   point. */
	flock_t *	ld,             /* The lock/unlock request. */
	int             flags)          /* The fll_flags to be used. */
{
        kqueuehead_t    wlist;		/* Queue of waiting request to be 
					   dealt with. */
	flList_t        dlist = NULL;   /* List of flLock's to be deleted. */
	flLock_t *      flp;		/* Lock created by vnodflUpdate. */
	int             retval;		/* Value returned by vnodeflUpdate. */

	kqueue_init(&wlist);
	retval = vnodeflUpdate(vp, insrtp, ld, &dlist, &wlist, &flp);
	if (flp && flags)
		flp->fll_flags |= flags;
	vnodeflReapply(vp, &wlist, &dlist);
	return (retval);
}

/*
 * vnodeflUpdate -- Update a vnode's locking list
 *
 * This routines adjusts file locks for a region specified by 'ld', in 
 * the specified vnode's lock list.  To optimize the search, updates
 * start at the lock given by 'insrtp'.  It is assumed the list is 
 * ordered on starting position, relative to the beginning of the 
 * file, and no updating is required on any locks in the list previous 
 * to the one pointed to by insrtp.  Insrtp is a result from the 
 * routine vnodeflBlocked..  vnodeflAdjust scans the list looking for 
 * locks owned by the process requesting the new (un)lock :
 *
 * 	- If the new record (un)lock overlays an existing lock of
 * 	  a different type, the region overlaid is released.
 *
 * 	- If the new record (un)lock overlays or adjoins an exist-
 * 	  ing lock of the same type, the existing lock is deleted
 * 	  and its region is coalesced into the new (un)lock.
 *
 * When the list is sufficiently scanned and the new lock is not 
 * an unlock, the new lock is inserted into the appropriate
 * position in the list.
 *
 * In order handle the transfer of locks to waiting processes and
 * their proper queueing, a queue of flWait's which need to be
 * dealt with is maintained and disposed of after all deletes and
 * inserts are finished.  Also, when a write locked region is being 
 * unlocked, we provided for its immeditae transfer to a process
 * waiting for the exact same region.
 *
 * The value returned is an error code (0 or ENOLCK).
 */
int 
vnodeflUpdate(
	vnode_t *	vp,		/* The vnode whose lock list is to 
					   be updated. */
	flLock_t *	insrtp,		/* Lock at which the update is to
					   start or NULL in which case the 
					   scan starts at the beginning of 
					   the list. */
	flock_t *	ld,		/* Description of the new lock or
					   unlock. */
	flList_t *      dlist,          /* List to which flLock's what we
					   deleted are to be prepended. */
	kqueuehead_t *  qhp,		/* Queue of waiting requests to
					   which requests displaced are
					   to be added. */
	flLock_t **     outlock)        /* Place to put address of flLock
					   created for lock request. */
{
	flLock_t *	flp;		/* Lock currently being looked at. */
	flLock_t *	nflp;		/* Lock following the one currently 
					   being looked at. */
	int 		regtyp;		/* Relative alignment of current
					   flLock and the area of the new
					   request. */
	int		retval = 0;	/* Value to be returned. */
	int             flags = 0;      /* Flags to be applied to the new
					   flLock. */

	flAssert(vnodeflHeld(vp));
	flVerify(insrtp == NULL || llistPresent(vnodeflHead(vp), insrtp));

	nflp = (insrtp == NULL) ? vnodeflFirst(vp): insrtp;
	*outlock = NULL;

	while (flp = nflp) {
		nflp = flLockNext(flp);
		if ( !identMatchLock(flp, ld) ) 
			continue;

		/* Release already locked region if necessary */

		switch (regtyp = regionAlign(ld, flp)) {
		case S_BEFORE|E_BEFORE:
		        nflp = NULL;
			break;
		case S_BEFORE|E_START:
			if (ld->l_type == flp->fll_set.l_type) {
			        ld->l_end = flp->fll_set.l_end;
				flags |= (flp->fll_flags & FLL_SHARE);
				vnodeflDelete(vp, flp, qhp, dlist);
			}
			nflp = NULL;
			break;
		case S_START|E_END:
			/*
			 * If this is null operation, setting a region
			 * to the same thing as it is now, get out
			 * quickly.
			 */
		        if (ld->l_type == flp->fll_set.l_type) {
			        flAssert(*dlist == NULL &&
					 kqueue_isempty(qhp));
				return (0);
			}
			
			/*
			 * If this is an unlock that precisely matches
			 * an existing region, try to quickly transfer
			 * it to a waiter, if any.  We take advantage
			 * of the fact that there should have been no 
			 * other regions that we did anything about so
			 * we can exit immediately.
			 */
			if (ld->l_type == F_UNLCK && lockGift(flp)) {
			        flAssert(*dlist == NULL &&
					 kqueue_isempty(qhp));
				return (0);
			}
		case S_START|E_AFTER:
			insrtp = flLockPrev(flp);
			vnodeflDelete(vp, flp, qhp, dlist);
			break;
		case S_BEFORE|E_END:
			if (ld->l_type == flp->fll_set.l_type)
			        nflp = NULL;
		case S_BEFORE|E_AFTER:
			vnodeflDelete(vp, flp, qhp, dlist);
			break;
		case S_START|E_MIDDLE:
			insrtp = flLockPrev(flp);
		case S_MIDDLE|E_MIDDLE:
			/*
			 * Don't bother if this is in the middle of
			 * an already similarly set section.
			 */
			if (ld->l_type == flp->fll_set.l_type)
			        return (0);
		case S_BEFORE|E_MIDDLE:
		        if (ld->l_type == flp->fll_set.l_type) {
			        ld->l_end = flp->fll_set.l_end;
				flags |= (flp->fll_flags & FLL_SHARE);
			}
			else {
			        /* setup piece after end of (un)lock */
			        flLock_t *	tdi;
			        flLock_t *	tdp;
				flock_t 	td;
				flLock_t *      new;

				td = flp->fll_set;
				td.l_start = ld->l_end + 1;
				tdp = tdi = flp;
				do {
				        if (tdp->fll_set.l_start < td.l_start)
					        tdi = tdp;
					else
					        break;
				} while (tdp = flLockNext(tdp));
				new = vnodeflInsert(vp, &td, tdi);
				if (new == NULL) 
				        return (ENOLCK);
				new->fll_flags |= (flp->fll_flags & FLL_SHARE);
				
			}
			if (regtyp == (S_MIDDLE|E_MIDDLE)) {
			        /* setup piece before (un)lock */
			        flp->fll_set.l_end = ld->l_start - 1;
				wqueueTransfer(qhp, flp);
				insrtp = flp;
			} 
			else
				vnodeflDelete(vp, flp, qhp, dlist);
			nflp = NULL;
			break;
		case S_MIDDLE|E_END:
			/*
			 * Don't bother if this is in the middle of
			 * an already similarly set section.
			 */
			if (ld->l_type == flp->fll_set.l_type)
			        return (0);
			flp->fll_set.l_end = ld->l_start - 1;
			wqueueTransfer(qhp, flp);
			insrtp = flp;
			break;
		case S_MIDDLE|E_AFTER:
			if (ld->l_type == flp->fll_set.l_type) {
			        ld->l_start = flp->fll_set.l_start;
				flags |= (flp->fll_flags & FLL_SHARE);
				insrtp = flLockPrev(flp);
				vnodeflDelete(vp, flp, qhp, dlist);
			} 
			else {
			        flp->fll_set.l_end = ld->l_start - 1;
				wqueueTransfer(qhp, flp);
				insrtp = flp;
			}
			break;
		case S_END|E_AFTER:
			if (ld->l_type == flp->fll_set.l_type) {
			        ld->l_start = flp->fll_set.l_start;
				flags |= (flp->fll_flags & FLL_SHARE);
				insrtp = flLockPrev(flp);
				vnodeflDelete(vp, flp, qhp, dlist);
			}
			break;
		case S_AFTER|E_AFTER:
			insrtp = flp;
			break;
		}
	}

	if (ld->l_type != F_UNLCK) {
	        flLock_t *	new;

		if (flp = insrtp) {
			do {
				if (flp->fll_set.l_start < ld->l_start)
					insrtp = flp;
				else
					break;
			} while (flp = flLockNext(flp));
		}
		new = vnodeflInsert(vp, ld, insrtp);
		if (new == NULL)
			retval = ENOLCK;
		else {
			*outlock = new;
			new->fll_flags |= flags;
		}
	}

	return (retval);
}

/*
 * vnodeflBlocked -- Find an flLock which would block a new request
 *
 * This routine checks whether a new lock request would be blocked by a 
 * previously set lock owned by another process.  In order optimize the
 * call to vnodeflAdjust when vnodeflBlocked returns NULL, insrt is set 
 * to point to the lock where lock list updating should begin to place 
 * the new lock.
 *
 * The value returned is the addresd of the conflicting flLock or NULL
 *      if there is no such.
 */ 
flLock_t *
vnodeflBlocked(
	vnode_t *	vp,		/* The vnode for which we are doing
					   locking. */
	flock_t  *	lckdat,		/* The new request. */
        flLock_t **	insrt,          /* Work to update with insertion 
					   point. */
	int *           flagsp)         /* flLock flags (inout). */
{
  	flLock_t *	f;		/* The flLock we are currently 
					   looking at. */

	flAssert(vnodeflHeld(vp));
	flAssert(lckdat->l_pid != IGN_PID);

	/*
	 * Scan for locks which whose start is at least as great as
	 * our request's is.
	 */
	*insrt = NULL;
	for (f = vnodeflFirst(vp); f != NULL; f = flLockNext(f)) {
		if (f->fll_set.l_start >= lckdat->l_start)
			break;
		*insrt = f;
		if( identSameLock(f, lckdat) ) {
		        if ((lckdat->l_start-1) <= f->fll_set.l_end) 
				break;
		} 
		else if (lckdat->l_start <= f->fll_set.l_end &&
			 (f->fll_set.l_type == F_WRLCK ||
			  (f->fll_set.l_type == F_RDLCK && 
			   lckdat->l_type == F_WRLCK))) {
		        flVerify(regionBlocks(f, lckdat));
			return f;
		}
		flVerify(!regionBlocks(f, lckdat));
	}

	/*
	 * Now continue the scan until we reach locks which start beyond
	 * the end of our request.  If we see any that overlap our
         * request, we do the following:
         *
         *      o For ordinary requests which match as far as identity,
         *        we ignore them.
         *      o When one of the requests (our or the one in the list)
         *        is temporary, we mark the request in the list as shared
	 *        and also set the shared flag to be used for the new
	 *        request.
         *      o When the requests don't match as far as identity we
	 *        check for a conflict based on lock type.
	 */
	for (; f != NULL; f = flLockNext(f)) {
	        if (lckdat->l_end < f->fll_set.l_start) {
		        flVerify(!regionBlocks(f, lckdat));
			break;
		}
		if (lckdat->l_start <= f->fll_set.l_end) {
		        if (identSameLock(f, lckdat)) {
			        if ((f->fll_flags | *flagsp) & FLL_TEMP) {
					f->fll_flags |= FLL_SHARE;
					*flagsp |= FLL_SHARE;
				}
			}
			else if (f->fll_set.l_type == F_WRLCK ||
				 lckdat->l_type == F_WRLCK) {
			        flVerify(regionBlocks(f, lckdat));
				return f;
			}
		}
		flVerify(!regionBlocks(f, lckdat));
	}

	return NULL;
}

/*
 * vnodeflCoalesce -- Combine a new non-temporary lock with matching flLock's
 *
 * This routine is used to combine a newly inserted or transferred flLock
 * (by lockGift) with any abutting matching flLock's.
 */
void
vnodeflCoalesce(
	vnode_t *	vp,		/* The vnode for our list. */
	flLock_t *	new)            /* The new or transferred lock. */
{
        pid_t		pid;            /* The pid to match on. */
	sysid_t         sysid;          /* The sysid to match on. */
	flLock_t *      cur;            /* The flLock being examined. */

	flAssert(vnodeflHeld(vp) && wlockHeld());
	flAssert((new->fll_flags & FLL_TEMP) == 0);
	flVerify(llistPresent(vnodeflHead(vp), new));

	/*
	 * Get the pid/sysid to compare.
	 */
	pid = new->fll_set.l_pid;
	sysid = new->fll_set.l_sysid;

	/*
         * Scan looking for subsequent locks which match ours as
	 * identity and type and which abut ours.
	 */
	for (cur = flLockNext(new); cur != NULL; cur = flLockNext(cur)) {
		if (cur->fll_set.l_start > new->fll_set.l_end + 1)
			break;
		if (cur->fll_set.l_pid != pid ||
		    cur->fll_set.l_sysid != sysid ||
		    (cur->fll_flags & FLL_TEMP))
			continue;
		flAssert(cur->fll_set.l_start > new->fll_set.l_end);
		if (new->fll_set.l_type == cur->fll_set.l_type &&
		    cur->fll_set.l_start == new->fll_set.l_end + 1) {
		        new->fll_flags |= (cur->fll_flags & FLL_SHARE);
		        new->fll_set.l_end =  cur->fll_set.l_end;
			llistRemove(vnodeflHead(vp), cur);
			wqueueShift(new, cur);
			lockFree(cur);
		}
		break;
	}

	/*
         * Scan looking for previous locks which match ours as
	 * identity and type and which abut ours.
	 */
	for (cur = flLockPrev(new); cur != NULL; cur = flLockPrev(cur)) {
		if (cur->fll_set.l_pid != pid ||
		    cur->fll_set.l_sysid != sysid ||
		    (cur->fll_flags & FLL_TEMP))
			continue;
		if (new->fll_set.l_type == cur->fll_set.l_type &&
		    new->fll_set.l_start == cur->fll_set.l_end + 1) {
		        cur->fll_set.l_end =  new->fll_set.l_end;
		        cur->fll_flags |= (new->fll_flags & FLL_SHARE);
			llistRemove(vnodeflHead(vp), new);
			wqueueShift(cur, new);
			lockFree(new);
		}
		break;
	}

}

/*
 * reclockGet -- Get (or try to get) a file lock
 *
 * This subroutine deals with the case of getting as opposed to releasing 
 * file locks.
 *
 * Particularly worthy of note is our handling of F_CHKLKW requests, used
 * for mandatory file locking.  In the old structure, the handling was to 
 * check for the specified lock being available without adding any locks,
 * taking advantage of the fact that we held the file read-write lock
 * throughout the test and the subsequent io so that we could be assured
 * that no conflicting lock could be granted without actually holding
 * an flock-style file lock during the pendedncy of IO and therefore
 * allowing us to simply give up the file read-write lock at the end of
 * the end of the IO, implicitly giving up what one might think of as a
 * virtual file lock on the region in question.  In the new structure,
 * in which waiting for a lock implies that one gets that lock before
 * the wait is completed, we have to do something different.  When we 
 * get the lock on behalf of the waiter, we do not have the file read-
 * write lock and even if we did, we could not very easily transfer 
 * ownership of it atomically to the waiter.  Instead, we simply get 
 * the lock as a special temporary lock.  This lock is not subject to
 * the normal rules of coalescing of abutting requests and the subsumption
 * of one locked region by a subsequent lock request for a different 
 * lock type (or unlock) when done by the same process.  This implies
 * that it will remain until release expllcitly by the waiter, which
 * he does after getting back the read-write lock for the file.  Since
 * the process in question has the read-write and the flock-style temporary
 * lock at the time the waiter releases the temproary lock, he has the 
 * same assurance that the old code used to have and by giving up the 
 * temporary lock, he leaves the filesystem's IO code in the same position
 * when it gives up its read-write lock.
 *
 * The value returned is the error code.
 */
int
reclockGet(
	vnode_t *	vp,		/* The file whose lock is to be 
                                           gotten. */
        flock_t *       lckdat,         /* The lock request. */
        int             cmd,            /* The lock command. */
        int *           were_held)      /* To update locks_were_held. */
{
        flLock_t *	found;		/* Conflicting lock found by 
					   vnodeflBlocked. */
        flLock_t *	insrt;          /* Insertion point for lock. */
	int 		retval = 0;	/* Return value. */
        int             flags = 0;      /* Value for fll_flags. */

	/*
	 * See if we can get the lock.
	 */
	found = vnodeflBlocked(vp, lckdat, &insrt, &flags);
	if (found == NULL) {
	        /*
		 * Can get it.
		 */
	        if (cmd & (SETFLCK | SETBSDFLCK)) {
		        retval = vnodeflAdjust(vp, insrt, lckdat, flags);
		} 

		/*
		 * Otherwise, just return an indication that the lock isn't 
		 * held.
		 */
		else {
		        lckdat->l_type = F_UNLCK;
		}
	}
	else if (cmd & SLPFLCK) {
		flWait_t *      waitp;
		flLock_t *	temp;

		/*
		 * Can't get it and need to sleep.  If our process identity
		 * pid/sysid is already waiting, then return EBUSY.
		 */
		wlockLock();
		if (whashFind(lckdat) != NULL) {
		        wlockUnlock();
			return(EBUSY);
		}

		/* 
		 * Do initial deadlock detection here.  Deadlocks can develop
		 * while we are waitng, requiring us to check as the thing
		 * we are waiting for changes.
		 */
		if (waitDeadlock(found, lckdat)) {
		        wlockUnlock();
			return(EDEADLK);
		}

		/*
		 * Now get ready to wait.
		 */
		waitp = waitPrepare(lckdat, found, cmd);
		wlockUnlock();
		if (waitp == NULL) {
		        return(ENOLCK);
		}

		/*
		 * Give up any file read-write lock, wait, and then
		 * see what happened to terminate the wait.
		 */
		if (cmd & INOFLCK) {
		        VOP_RWUNLOCK(vp, 
				     (cmd & INOFLCK_READ) ?
				     VRWLOCK_READ : VRWLOCK_WRITE);
		}
		if (sv_wait_sig(&waitp->flw_sv, PZERO+1, 
				&vp->v_filocksem, 0)) {
		        retval = waitInterrupt(waitp, vp);
		}
		else {    
		        retval = waitp->flw_err;
		        flAssert(retval >= 0);
		}

		/*
		 * Relock now what we're awake.
		 */
		if (cmd & INOFLCK) {
		        VOP_RWLOCK(vp, 
				   (cmd & INOFLCK_READ) ?
				   VRWLOCK_READ : VRWLOCK_WRITE);
		}
		vnodeflLock(vp);
		*were_held = (vnodeflFirst(vp) != NULL);

		/*
		 * Now that we have the vnodeflLock, we can free the
		 * flWait since we know the sv_broadcast in waitDone
		 * is done.
		 */
		temp = waitp->flw_tlock;
		waitFree(waitp);

		/*
		 * Now give up our temprorary lock, if we were only doing
		 * an F_CHKLW.
		 */
		if (temp != NULL) {
			flAssert((cmd & (SETFLCK | SETBSDFLCK)) == 0);
			flVerify(llistPresent(vnodeflHead(vp), temp));
			lockDelete(temp, vp);
			lckdat->l_type = F_UNLCK;
		}
	} 

	/*
	 * If we were not prepared to wait and had to wait, return an
	 * error.  Support network locking by checking for deadlock.
	 */
	else if (cmd & (SETFLCK | SETBSDFLCK)) {
	        retval = EAGAIN;
		if (cmd & RCMDLCK) {
		        wlockLock();
			if (waitDeadlock(found, lckdat))
			        retval = EDEADLK;
		        wlockUnlock();
		}
	} 

	/*
	 * If we are just checking for a conflicting lock, return
	 * the information.
	 */
	else {
	        *lckdat = found->fll_set;
	}
	return (retval);
}

/*
 * get and set file/record locks
 *
 * cmd & SETFLCK indicates setting a lock.
 * cmd & SLPFLCK indicates waiting if there is a blocking lock.
 */
int
reclock_new(
	vnode_t *	vp,		/* The vnode on which the lock/unlock/
					   etc. is being done. */
	flock_t *	lckdat,		/* The request information. */
	int 		cmd,		/* Command flags. */
	int 		flag,		/* Open mode flags. */
	off_t 		offset,         /* Current file offset. */
	cred_t *	cr)		/* User credentials. */
{
	int 		retval;         /* Value to be returned. */
	int 		locks_were_held;/* Whether there were any locks when
					   we last grabbed the vnodefl lock. */

	/* check access permissions */
	if ((cmd & SETFLCK) 
	  && ((lckdat->l_type == F_RDLCK && (flag & FREAD) == 0)
	    || (lckdat->l_type == F_WRLCK && (flag & FWRITE) == 0)))
		return EBADF;
	
	/* Convert start to be relative to beginning of file */
	/* whence = lckdat->l_whence; XXX see below */
	if (retval = convoff_new(vp, lckdat, 0, offset, SEEKLIMIT, cr))
		return retval;

	/* Convert l_len to be the end of the rec lock l_end */
	if (lckdat->l_len < 0)
		return EINVAL;
	if (lckdat->l_len == 0)
		lckdat->l_end = MAXEND;
	else
		lckdat->l_end += (lckdat->l_start - 1);

	/* check for arithmetic overflow */
	if (lckdat->l_start > lckdat->l_end)
		return EINVAL;

        vnodeflLock(vp);
	locks_were_held = (vnodeflFirst(vp) != NULL);

	switch (lckdat->l_type) {
	case F_RDLCK:
	case F_WRLCK:
	        retval = reclockGet(vp, lckdat, cmd, &locks_were_held);
		break;
	case F_UNLCK:
		/* removing a file record lock */
		if (cmd & (SETFLCK | SETBSDFLCK))
		        retval = vnodeflAdjust(vp, NULL, lckdat, 0);

		break;
	default:
		/* invalid lock type */
		retval = EINVAL;
		break;
        }

	/*
	 * Call VOP_VNODE_CHANGE if the vnode has transitioned from either:
	 * - having no locks to having at least one lock
	 * - having at least one lock to having no locks
	 */
	if (locks_were_held) {
		if (vp->v_filocks == NULL)
			VOP_VNODE_CHANGE(vp, VCHANGE_FLAGS_FRLOCKS, 0);
	} else {
		if (vp->v_filocks != NULL)
			VOP_VNODE_CHANGE(vp, VCHANGE_FLAGS_FRLOCKS, 1);
	}
		
	vnodeflUnlock(vp);

	/* Restore l_len */
	if (lckdat->l_end == MAXEND)
		lckdat->l_len = 0;
	else
		lckdat->l_len -= (lckdat->l_start-1);
	/*
	 * POSIX compatibility requires that we disable this perfectly
	 * nice behavior.  The returned lock description will be normalized
	 * relative to SEEK_SET (0) instead of being converted back to the
	 * format that was passed in.
	 *
	 * (void) convoff_new(vp, lckdat, whence, offset, SEEKLIMIT);
	 */

	return retval;
}

/*
 * Given and process ID (possibly IGN_PID) and a sysid, determine whether
 * or not any locks are held on the given vnode.  Returns 1 (locks held)
 * or 0 (no locks held).
 */
int
haslocks_new(
	vnode_t *	vp,		/* The vnode in question. */
	pid_t 		pid, 		/* The pid to compare (or IGN_PID). */
	sysid_t 	sysid)          /* The sysid to compare. */
{
	flLock_t *      cur;            /* The current lock in list. */
	int 		locks_held = 0; /* For return value. */

	/*
	 * If there are no locks at all, we can skip this since asking whether
	 * you have a lock held is an inherently racy action ...
	 */
	vnodeflIfAny(vp) {
		vnodeflLock(vp);
		for (cur = vnodeflFirst(vp); 
		     cur != NULL; 
		     cur = flLockNext(cur)) {
			if ((sysid == cur->fll_set.l_sysid) &&
			    ((pid == IGN_PID) || (pid == cur->fll_set.l_pid))) {
					locks_held = 1;
					break;
			}
		}
		vnodeflUnlock(vp);
	}
	return(locks_held);
}

/*
 * This function answers the question "Is there a lock with the
 * same owner as the lock described by ld which spans the lock
 * described by ld?"
 */
int
haslock_new(
	vnode_t *	vp,		/* The vnode in question. */
        flock_t *       ld)             /* The lock request to check. */
{
	flLock_t *      cur;		/* The current flLock to look at. */
	int 		lock_held = 0;  /* For return value. */
	int 		reg;		/* Relation between current range and
                                           lcok sought. */

	/* Convert l_len to be the end of the rec lock l_end */
	if (ld->l_len == 0)
		ld->l_end = MAXEND;
	else
		ld->l_end += (ld->l_start - 1);

	/*
	 * If there are no locks at all, we can skip this since asking whether
	 * you have a lock held is an inherently racy action ...
	 */
	vnodeflIfAny(vp) {
		vnodeflLock(vp);
		for (cur = vnodeflFirst(vp); 
		     cur != NULL && !lock_held; 
		     cur = flLockNext(cur)) {
			if (identMatchLock(cur, ld) && 
			    (ld->l_type == cur->fll_set.l_type)) {
				reg = regionAlign(ld, cur);
                                switch (reg) {
                                        case S_MIDDLE|E_MIDDLE:
                                        case S_START|E_END:
                                        case S_START|E_MIDDLE:
                                        case S_MIDDLE|E_END:
                                        	lock_held = 1;
                                                break;
                                        default:
                                                break;
				}
			}
		}
		vnodeflUnlock(vp);
	}
	/* Restore l_len */
	if (ld->l_end == MAXEND)
		ld->l_len = 0;
	else
		ld->l_len -= (ld->l_start-1);
	return(lock_held);
}

#ifdef DEBUG
void
print_locks_new(
	vnode_t *	vp,		/* The vnode in question. */
	pid_t 		pid, 		/* The pid to compare (or IGN_PID). */
	__uint32_t 	sysid)          /* The sysid to compare. */
{
	flLock_t *      cur;		/* The current flLock to look at. */

	vnodeflIfAny(vp) {
		vnodeflLock(vp);
		for (cur = vnodeflFirst(vp); 
		     cur != NULL; 
		     cur = flLockNext(cur)) {
			if ((sysid == cur->fll_set.l_sysid) &&
			    ((pid == IGN_PID) || (pid == cur->fll_set.l_pid))) {
			        printf("\tpid %d: %lld, %lld, type %d\n", 
				       cur->fll_set.l_pid, cur->fll_set.l_start, 
				       cur->fll_set.l_end, cur->fll_set.l_type);
			}
		}	
		vnodeflUnlock(vp);
	}
}

#endif /* DEBUG */

/*
 * Determine whether or not any remote locks are held on the given vnode.
 * Returns 1 (locks held) or 0 (no locks held).
 */
int
remotelocks_new(
	vnode_t *	vp)		/* The vnode in question. */
{
	flLock_t *      cur;		/* The current flLock to look at. */
	int 		locks_held = 0; /* For return value. */

	flAssert(vnodeflHeld(vp));
	for (cur = vnodeflFirst(vp); cur != NULL; cur = flLockNext(cur)) {
		/*
		 * Local locks will have a sysid of 0, remote ones will be
		 * non-zero, so return 1 when the first lock with a non-zero
		 * sysid is found.
		 */
		if (cur->fll_set.l_sysid != 0) {
			locks_held = 1;
			break;
		}
	}
	return(locks_held);
}

void
release_remote_locks_new(vnode_t *vp)
{
	flLock_t *      cur;		/* The current flLock to look at. */
	flLock_t *      next;		/* The bext flLock to look at. */

	/*
	 * If there are no locks at all, we can skip this ...
	 */
	vnodeflIfAny(vp) {
	        int	locks_were_held;	/* Set if we had locks when we
						   started. */

		vnodeflLock(vp);
	        locks_were_held = (vnodeflFirst(vp) != NULL);
		for (cur = vnodeflFirst(vp); cur != NULL; cur = next) {
		        next = flLockNext(cur);
			if (cur->fll_set.l_sysid != 0) {
		        	lockDelete(cur, vp);
			} 		
		}

		/*
		 * Call VOP_VNODE_CHANGE if the vnode has transitioned from
		 * having at least one lock to having no locks
		 */
		if (locks_were_held && vnodeflFirst(vp) == NULL)
			VOP_VNODE_CHANGE(vp, VCHANGE_FLAGS_FRLOCKS, 0);
		vnodeflUnlock(vp);
	}
}

/*
 * lock_pending -- Look for a pending (blocked) lock request
 *
 * This routine used to be used internally in flock_new.c but is not 
 * used in IRIX code anymore.  Since it appears in sys/flock.h, we
 * need to keep the function.
 *
 * It was documented as follows:
 *
 *      Determine whether or not there is a pending (blocked) lock 
 *      request matching the given vnode and lock description.
 *
 * In fact, it did not use the vnode at all and only matched on the
 * lock description.  We will implement the actual behavior in
 * 6.5 rather than the dubious doucmentation in the comments. 
 */
/* ARGSUSED */
int
lock_pending_new(vnode_t *vp, flock_t *flp)
{
        int 		hn;             /* Current hash chain number. */
        flHash_t *      hp;             /* Current flHash. */
        flWait_t *      wp;             /* Current flWait. */
	int 		found = 0;      /* Count of those which match. */

	wlockLock();
	for (hn = whashScanStart(flp->l_pid, flp->l_sysid);
	     hn <= flHashMask;
	     hn++) {
	        hp = &flHashTable[hn];
		for (wp = whqFirst(hp); 
		     !whqDone(wp, hp); 
		     wp = whqNext(wp)) {
			if (identMatchWait(wp, flp)) {
				found++;
#ifndef DEBUG
				break;
#endif /* DEBUG */
			}
		}
#ifndef DEBUG
		if (found)
			break;
#endif /* DEBUG */
		if (flp->l_pid != IGN_PID)
			break;
	}
	wlockUnlock();
	ASSERT((found == 0) || (found == 1));
	return(found);
}



#ifdef DEBUG
int
locks_pending_new(
	pid_t 		pid, 		/* The pid we are to match on (or
					   IGN_PID). */
	__uint32_t 	sysid)		/* The sysid we are to match on. */
{
        int 		hn;             /* Current hash chain number. */
        flHash_t *      hp;             /* Current flHash. */
        flWait_t *      wp;             /* Current flWait. */
	int 		found = 0;      /* Count of those which match. */

	wlockLock();
	for (hn = whashScanStart(pid, sysid); hn <= flHashMask; hn++) {
	        hp = &flHashTable[hn];
		for (wp = whqFirst(hp); 
		     !whqDone(wp, hp); 
		     wp = whqNext(wp)) {
		        if (identMatchPair(&wp->flw_req, pid, sysid))
				found++;
		}
		if (pid != IGN_PID)
			break;
	}
	wlockUnlock();
	return(found);
}
#endif /* DEBUG */

/*
 * Cancel a pending (blocked) lock request.
 *
 */
int
cancel_lock_new(
	vnode_t *	vp, 		/* The vnode which the specified lock
                                           must be for.  The current code does
					   not check that this is so, but 
					   does depend on this fact for its
					   locking.  Ugh! */
        flock_t *	flp)		/* The request we are trying to 
					   cancel. */
{
        int 		hn;             /* Current hash chain number. */
        flHash_t *      hp;             /* Current flHash. */
        flWait_t *      cur;            /* Current flWait. */
        flWait_t *      next;           /* Current flWait in the list. */
        int             error = EAGAIN; /* Error to return.  Must map to
					    nlm_denied/NLM4_DENIED. */  
#ifdef DEBUG
	flock_t tfl;

	ASSERT(flp->l_whence == 0);
	tfl = *flp;
	if (tfl.l_len == 0)
		tfl.l_end = MAXEND;
	else
		tfl.l_end += (tfl.l_start - 1);
	ASSERT(tfl.l_start <= tfl.l_end);
#endif /* DEBUG */

	/*
         * In the interests of compatibility, handle the IGN_PID case by
	 * calling cancel_blocked_locks, even though no Irix code call
	 * cancel_lock with this specified and the code would not work
	 * correctly in the old implementation, in any case.  Note that 
	 * cancel_blocked_locks is not defined as making any distinction 
         * based on vnode, while this routine is defined as doing so, even 
	 * though it in fact does not.  In any case, cancel_blocked_locks
         * can deal with the case of encountering locks on miscellaneous
	 * vnodes without causing internal inconsistencies.
         *
         * Note that no effort is made to return a meaningful error code.
	 * Since the error return was only recently introduced (and 
         * specifically to handle problems in which we know l_pid was 
         * *not* IGN_PID), this should not be a problem.
	 */
	if (flp->l_pid == IGN_PID) {
		cancel_blocked_locks(flp->l_sysid);
		return (0);
	}

	vnodeflLock(vp);
	wlockLock();
	hn = identHashPair(flp->l_pid, flp->l_sysid); 
	hp = &flHashTable[hn];
	for (cur = whqFirst(hp); !whqDone(cur, hp); cur = next) {
	        next = whqNext(cur);
		if (identSameWait(cur, flp) && 
		    regionMatches(flp, &cur->flw_req)) {
			waitCancel(cur);
			error = 0;
		}
	}
	wlockUnlock();
	vnodeflUnlock(vp);
	return (error);
}

/*
 * Cancel all pending (blocked) lock requests for the given sysid.
 */
void
cancel_blocked_locks_new(
	__uint32_t 	sysid)		/* The sysid we need to match on. */
{
        int 		hn = 0;         /* Current hash chain number. */
        flHash_t *      hp;             /* Current flHash. */
        flWait_t *      cur;            /* Current flWait. */
        flWait_t *      next;           /* Current flWait in the list. */

	/*
	 * Scan each of the hash lists.
	 */
start:
	while (hn <= flHashMask) {
	        wlockLock();
	        hp = &flHashTable[hn];
		for (cur = whqFirst(hp); !whqDone(cur, hp); cur = next) {
		        next = whqNext(cur);
			if (cur->flw_req.l_sysid == sysid) {
				vmap_t		map;
				flock_t		req;
				vnode_t *       vp;

				/*
				 * If we have a match, then get a vmap for the
				 * vnode and also a copy of the request.  
				 * waitExpunge will use these to get the
				 * needed locks in the right order and to
				 * make sure that we are zapping the right
				 * request.  After the waitExpunge, we'll
				 * start again at the beginning of the current
				 * list.
				 */
				vp = cur->flw_waitee->fll_vnode;
				VMAP(vp, map);
				req = cur->flw_req;
				wlockUnlock();
				waitExpunge(cur, vp, hn, &map, &req);
				goto start;
			}
		}
	        wlockUnlock();
		hn++;
	}
}

/*
 * convoff - converts the given data (start, whence) to the
 * given whence.
 * The limit given as a parameter specifies the maximum offset the
 * caller is willing to allow.
 */
int
convoff_new(struct vnode *vp,
	struct flock *lckdat,
	int whence,
	off_t offset,
	off_t limit,
	cred_t *cr)
{
	int error;
	off_t llen;
	struct vattr vattr;

	vattr.va_mask = AT_SIZE;
	VOP_GETATTR(vp, &vattr, 0, cr, error);
	if (error)
		return error;
	if (lckdat->l_whence == 1)
		lckdat->l_start += offset;
	else if (lckdat->l_whence == 2)
		lckdat->l_start += vattr.va_size;
	else if (lckdat->l_whence != 0)
		return EINVAL;

	/*
	 * Bug/feature alert.  The following test allows locking
	 * starting at 'limit' with length of one to succeed
	 * even though there can't be any file space _beginning_
	 * at 'limit' in most cases.  But C-ISAM (and other products?)
	 * relies on the ability to lock here at the end of the world...
	 *
	 * Note that it is up to the calling file systems to enforce any
	 * 32 bit limits that they might have.
	 */
	llen = lckdat->l_len > 0 ? lckdat->l_len - 1 : lckdat->l_len;

	if ((lckdat->l_start < 0) ||
	    (lckdat->l_start > limit) ||
	    (lckdat->l_start + llen < 0) ||
	    (lckdat->l_start + llen > limit)) {
		return EINVAL;
	}

	if (whence == 1)
		lckdat->l_start -= offset;
	else if (whence == 2)
		lckdat->l_start -= vattr.va_size;
	else if (whence != 0)
		return EINVAL;
	lckdat->l_whence = (short)whence;
	return 0;
}

/*
 * Clean up record locks left around by process.
 */
void
cleanlocks_new(
	vnode_t *	vp, 		/* Vnode for file being lcoked. */
	pid_t 		pid, 		/* The pid of the process. */
	sysid_t 	sysid)		/* The sysid for the process. */
{
	flLock_t *      cur;		/* The current flLock to look at. */
	flLock_t *      next;		/* The bext flLock to look at. */

	/*
	 * If there are no locks at all, we can skip this ...
	 */
	vnodeflIfAny(vp) {
	        int	locks_were_held;	/* Set if we had locks when we
						   started. */

		vnodeflLock(vp);
	        locks_were_held = (vnodeflFirst(vp) != NULL);
		for (cur = vnodeflFirst(vp); cur != NULL; cur = next) {
		        next = flLockNext(cur);
		        if (identMatchPair(&cur->fll_set, pid, sysid))
			        lockDelete(cur, vp);
		}

		/*
		 * Call VOP_VNODE_CHANGE if the vnode has transitioned from
		 * having at least one lock to having no locks
		 */
		if (locks_were_held && vnodeflFirst(vp) == NULL)
			VOP_VNODE_CHANGE(vp, VCHANGE_FLAGS_FRLOCKS, 0);
		vnodeflUnlock(vp);
	}
}

#ifdef FL_VERIFY

/* 
 * validateVnode -- Check data sructures guarded by the vnodeflLock
 */
void
validateVnode(
	vnode_t *	vp)		/* The vnode in question. */
{
	flLock_t *	lp;             /* Current lock pointer. */
	flLock_t *      plp;            /* Previous lock point. */
	flLock_t *	lp2;            /* Current lock pointer for inner
					   loop. */
	flWait_t *      wp;             /* Current wait pointer. */
	flWait_t *      pwp;            /* Previous wait pointer. */
	off_t           pstart = 0;     /* Start from previous lock. */

	/*
	 * Make sure VFRLOCKS correctly indicates whether there are any 
	 * lock's.
	 */
	ASSERT_ALWAYS((vnodeflFirst(vp) != NULL) ==
		      	((vp->v_flag & VFRLOCKS) != 0));

	/*
	 * Now go through the list of locks and do extensive verification.
	 */
        for (plp = NULL, lp = vnodeflFirst(vp);
	     lp != NULL;
	     plp = lp, lp = flLockNext(lp)) {

	        /*
		 * Check the basic integrity of the list, that the
		 * starts are non-descending and that we don't have a
		 * rogue IGN_PID.
		 */
		ASSERT_ALWAYS(flLockPrev(lp) == plp);
		ASSERT_ALWAYS(lp->fll_set.l_start >= pstart);
		pstart = lp->fll_set.l_start;
		ASSERT_ALWAYS(lp->fll_vnode == vp);
		ASSERT_ALWAYS(lp->fll_set.l_pid != IGN_PID);

		/*
		 * Now check the list of those waiting and make sure
		 * that those that are waiting should be waiting.
		 */
		for (wp = wlqFirst(lp), pwp = wlqAnchor(lp);
		     !wlqDone(wp, lp);
		     pwp = wp, wp = wlqNext(wp)) {
		        ASSERT_ALWAYS(wlqPrev(wp) == pwp);
			ASSERT_ALWAYS(wp->flw_waitee == lp);
			ASSERT_ALWAYS(regionBlocks(lp, &wp->flw_req));
		}

		/*
		 * If flVerifyOn is not at least two, avoid the
		 * O(n**2) part of the checking.
		 */
		if (flVerifyOn <= 1) 
			continue;

		/*
                 * Now check each lock against our current lock.
		 */
		for (lp2 = vnodeflFirst(vp); 
		     lp2 != NULL; 
		     lp2 = flLockNext(lp2)) {

		        /*
			 * Don't check a lock against itself.
			 */
			if (lp == lp2)
				continue;
			
			/*
			 * If the identities don't match, then there
			 * should be no conflict between the two locks
			 * since they are both in effect.
			 */
			if (lp->fll_set.l_pid != lp2->fll_set.l_pid ||
			    lp->fll_set.l_sysid != lp2->fll_set.l_sysid) {
				ASSERT_ALWAYS(!regionConflicts(&lp->fll_set,
							       &lp2->fll_set));
			}

			/*
			 * If the identities match and one of the locks is 
			 * temporary, then if they overlap, they both should
			 * be marked shared.
			 */
			else if ((lp->fll_flags | lp2->fll_flags) & FLL_TEMP) {
			        ASSERT_ALWAYS((lp->fll_flags &
					       lp2->fll_flags & FLL_SHARE) ||
					      !regionOverlaps(&lp->fll_set,
							      &lp2->fll_set));
			}

			/*
			 * If they have the same identity and are not
			 * temporary, they should not overlap and regions
			 * of the same type should not abut.
			 */
			else {

				ASSERT_ALWAYS(!regionOverlaps(&lp->fll_set,
							      &lp2->fll_set));
				ASSERT_ALWAYS(lp->fll_set.l_type != 
					              lp2->fll_set.l_type ||
					      (lp->fll_set.l_start !=
					              lp2->fll_set.l_end + 1 &&
					       lp2->fll_set.l_start !=
				                      lp->fll_set.l_end + 1));
			}
		}

	}
}

/* 
 * validateWlock -- Check data sructures guarded by the wait lock
 */
void
validateWlock(void)
{
	int		hx;   		/* Current hash index. */
        flHash_t *      hp;             /* FlHash we are scanning. */
	flWait_t *      cur;            /* The current flWait for the scan. */
	flWait_t *      prev;           /* The current flWait for the scan. */

	/*
	 * Go through each list in turn.
	 */
	for (hx = 0; hx <= flHashMask; hx++) {
		hp = &flHashTable[hx];

		/*
		 * Check the current list, verifying:
		 *     o The integrity of the list
		 *     o That no wait is marked as having been completed
		 *     o That those marked waiting are validly waiting
		 *       for the lock in question.
		 */
		for (cur = whqFirst(hp), prev = whqAnchor(hp);
		     !whqDone(cur, hp); 
		     prev = cur, cur = whqNext(cur)) {
		        ASSERT_ALWAYS(whqPrev(cur) == prev);
			ASSERT_ALWAYS(cur->flw_err == -1);
			ASSERT_ALWAYS(regionBlocks(cur->flw_waitee, 
						   &cur->flw_req));

		}
	}
}

#endif /* FL_VERIFY */


