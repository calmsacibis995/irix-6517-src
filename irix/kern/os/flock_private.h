/**************************************************************************
 *									  *
 * 		 Copyright (C) 1998 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef	_OS_FLOCK_PRIVATE_H_
#define	_OS_FLOCK_PRIVATE_H_ 1
#ident "$Id: flock_private.h,v 1.1 1998/11/19 19:55:36 dnoveck Exp $"

/*
 * Structure definitions used only to implement flock-type locking.  There
 * should be no need to include them outside of os/flock.c, with the
 * exception of debugging support such as icrash.
 */

#include <sys/flock.h>
#include <sys/vnode.h>
#include <ksys/kqueue.h>

/*
 * We specify Null structure definitions and typedefs in advance for all
 * all of our types so we can use the typedef's uniformly below,  
 *
 *     flLock -- Records a single active lock range.
 *     flList -- Work containing the head of a list of flLock's.
 *     flWait -- Used to bind a waiting request to the lock it is
 *               waiting for.
 *     flHash -- Keeps all information for given hash of actor identity.
 */
struct flLock;
typedef struct flLock flLock_t;
typedef struct flLock * flList_t;
struct flWait;
typedef struct flWait flWait_t;
struct flHash;
typedef struct flHash flHash_t;

/*
 * FlLock -- Records a single active or waiting lock range
 *
 * The flLock is used to keep track of actual granted locks and locks 
 * which are waiting to be granted.  Locks that have been granted are
 * organized in order of the starting offset of the byte range locked 
 * in a chain which starts at v_filocks.  These flLock's are subject
 * to be coalesced and split in response to subsequent lock and 
 * unlock request.  Waiting flLock's are not themselves kept in a
 * list but are found from the list of flWait structures hanging 
 * off the flLock which is currently blocking the granting of the
 * waiting request.
 *
 * In order to maintain binary compatibility in the face of any third-
 * party code which inspects the vnode's v_filocks list, we embed
 * the old structure filock structure at the front of an flLock.
 * The result is that there is fair amount of useless space in the
 * flLock which could be removed in 6.6.  Note also that we make no
 * attempt to simulate the use of blocked filock fields since there 
 * is no conceivable way that third-party code could access such
 * filocks.
 *
 * All fields within the flLock are governed by the v_filocksem of the
 * associated vnode.  
 */
struct flLock {
        filock_t        fll_filock;     /* Embed old structure in new. */
        kqueuehead_t    fll_waitq;      /* List of those waiting. */
        vnode_t *       fll_vnode;      /* Address of associated vnode. */
        int             fll_flags;      /* Some type and state flags. */
};					

/*
 * Values for fll_flags
 */
#define FLL_TEMP	0x0001		/* A temporary lock for support of */
                                        /* fs_checklock in the new structure. */
                                        /* Temporary locks may not be */
                                        /* coalesced upgraded, downgraded, */
                                        /* or unlocked by subsequent requests */
                                        /* for the same pid/sysid. */
#define FLL_SHARE       0x0002          /* A possibly shared lock,  When you */
                                        /* a full or partial overlap between */
                                        /* a normal and a temporary lock with */
                                        /* the same pid/sysid, the removal of */
                                        /* a write lock need not imply the */
                                        /* availability of that region, since */
                                        /* the region mey be shared among a */
                                        /* normal lock and one or more tempo- */
                                        /* rary locks. */

/*
 * In order to use the old filock structure for possible external users,
 * while doing as much work as possible in the new frameowrk, we use
 * defines for fields that we have to refer to within the embedded
 * flock.
 */
#define fll_set	        fll_filock.set  /* Specification of the lock. */
#define fll_next	fll_filock.next /* Pointer to next lock for file. */
#define fll_prev	fll_filock.prev /* Pointer to previous lock for file. */

/*
 * Because the old forward and back pointers are specified as of type
 * filock_t *, we need some defines to do the appropriate type conversions.
 */
#define flLockFilock(f)                 /* From new to old pointer. */\
	(&((f)->fll_filock))
#define flLockNext(f)                   /* Fetch next flLock. */\
        ((flLock_t *) ((f)->fll_next))
#define flLockPrev(f)                   /* Fetch previous flLock. */\
        ((flLock_t *) ((f)->fll_prev))

/*
 * flWait -- Wait queueing element for waiting locks
 *
 * An flWait is used to connect a waiting request to the lock for which it
 * is waiting.  In the new organization of the flock code, threads are not
 * waken up ay every conceivable point at which their request might be 
 * satisfied.  Instead, they are woken up only when their request actually
 * has been satisfied (or it is decided that it will never be satisfied).
 * This implies that the waiter cannot wait on a variable located in the
 * flLock for the lock being awaited, since the lock being awaited will
 * change during the pendency of the waiting request.  Also, the waiter
 * may not refer to the flLock for his own request after the wait completes,
 * since, in general, it is subject to deletion (because of merging with
 * subsequent requests and racing unlocks from other threads) once the
 * flLock is inserted in the vnode-based list and v_filocksem dropped.
 *
 * Manipulating all fields in the flWait requires the v_filocksem for
 * the associated vnode.  Manipulating flw_hashq and flw_waitee requires
 * that we also have the global flWaitLock.   Note also that flw_err may 
 * be referenced by the waiting thread (after being awakened) since at that
 * point all connection between other data structures and a given flWait 
 * has been broken and only the waiting thread may look at it (and then 
 * free it).  When a waiting thread is awoken, the only data structures
 * it may depend on is its flWait (and possibly the flLock pointed to
 * be flw_tlock).
 */
struct flWait {
        kqueue_t	flw_lockq;      /* List of everyone waiting for 
                                           the same thing (i.e having the
                                           same flw_waitee). */
        kqueue_t        flw_hashq;      /* List of some of those waiting with
					   the same hash chain (based on
					   identity).   This is used for
                                           deadlock detection and some
                                           remote locking functions.  Note that
                                           those in the list will have a non-
                                           NULL flw_waitee, and conversely. */
        flock_t         flw_req;        /* The request on whose behalf we
                                           are waiting. */
        flLock_t *      flw_waitee;     /* The request for which we are 
                                           waiting.  This may be any one of the
                                           locks which prevent our request 
                                           from being granted, in those cases
                                           in which there are multiple such. */
        sv_t            flw_sv;         /* Sync variable on which the actual
					   wait will be done. */
        int             flw_err;        /* Error to return to caller. */ 
        char            flw_tflag;      /* Set if this is a temporary request
                                           for a CHKLKW. */
        flLock_t *      flw_tlock;      /* Temporary flLock created for a
					   CHKLKW. */
};

/*
 * flHash -- All information for given hash of actor identity
 *
 * The flHash keeps track of a number of items that are organized by a hash
 * of the identity (flid_t) of the requestor.
 *
 * Currenly, the only item in the flHash in a queue of flWait's with the
 * given hash, although other items may be added in the future.
 *
 * For example, It is possible to re-organize deadlock detection so that
 * each of the wait lists has its own lock, but this is not being done now
 * sinceit adds to the risk of the new structure and is almost certainly 
 * not worth the effort at current levels of use.
 */
struct flHash {
	kqueuehead_t    flh_wlist;      /* List of flWait's with this hash. */
};

/*
 * Interfaces to _old flock routines.
 *
 * This seems as good a place as any for prototypes to the routine in flock.c
 * where the _old routines are located.
 */
extern void     flckinit_old(void);
extern int	reclock_old(struct vnode *, struct flock *, int, int, off_t,
				struct cred *);
extern int	convoff_old(struct vnode *, struct flock *, int, off_t, off_t,
				struct cred *);
extern int	haslocks_old(struct vnode *, pid_t, sysid_t);
extern int	haslock_old(struct vnode *, struct flock *);
extern int	remotelocks_old(struct vnode *);
extern void	release_remote_locks_old(struct vnode *);
extern int	lock_pending_old(struct vnode *, struct flock *);
extern int	cancel_lock_old(struct vnode *, struct flock *);
extern void	cleanlocks_old(struct vnode *, pid_t, sysid_t);
extern void 	cancel_blocked_locks_old(__uint32_t);
#ifdef DEBUG
extern int	locks_pending_old(pid_t, __uint32_t);
extern void 	print_locks_old(struct vnode *, pid_t, __uint32_t);
#endif /* DEBUG */


#endif	/* _OS_FLOCK_PRIVATE_H_ */

