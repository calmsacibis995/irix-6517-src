/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_lock_mach.h,v $
 * Revision 65.4  1998/03/24 17:20:24  lmc
 * Changed an osi_mutex_t to a mutex_t and changed osi_mutex_enter() to
 * mutex_lock and osi_mutex_exit to mutex_unlock.
 * Added a name parameter to osi_mutex_init() for debugging purposes.
 *
 * Revision 65.3  1998/03/06  19:45:07  gwehrman
 * Added definitions for osi_mutex_init(), osi_mutex_enter_no_wait(), osi_mutex_destroy(), osi_mutex_held().  They are all defined as no ops, as they are not really used at this time.
 *
 * Revision 65.2  1998/03/04 21:10:37  lmc
 * Add #define for lock_xxxxx_r to be lock_xxxx.
 *
 * Revision 65.1  1997/10/24  14:29:51  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:43  jdoak
 * Initial revision
 *
 * Revision 64.4  1997/08/14  20:18:24  bdr
 * Add PLTWAIT flag to prevent DCE threads for getting counted in load average
 * (nrun).
 *
 * Revision 64.3  1997/06/24  18:01:38  lmc
 * Changes for memory mapped files and the double locking problem which
 * is seen when dirty pages are hung off the vnode for memory mapped files.
 * The scp and dcp locks are dropped in cm_read.  Locks that are held
 * by a thread as a write lock are allowed to be recursively locked by
 * that same thread as a write lock.  Memory mapped files changed fsync
 * to add a flag telling whether or not it came from an msync.  Various
 * other changes to flushing and tossing pages.
 *
 * Revision 64.2  1997/03/05  19:18:19  lord
 * Changed how we identify threads in the kernel so that kthreads can
 * enter dfs code without panicing.
 *
 * Revision 64.1  1997/02/14  19:46:51  dce
 * *** empty log message ***
 *
 * Revision 1.5  1996/12/14  00:20:06  vrk
 * a debug field "lock_holder". Added debug info for lock_data structure - we store
 * the write locker in a debug field "lock_holder"
 *
 * Revision 1.4  1996/06/15  02:19:59  bhim
 * Fixed the implementation of the preemption lock.
 *
 * Revision 1.3  1996/04/10  18:41:44  bhim
 * Added prototype for osi_Sleep2.
 *
 * Revision 1.2  1996/04/06  00:17:42  bhim
 * No Message Supplied
 *
 * Revision 1.1.106.2  1994/06/09  14:15:09  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:27:41  annie]
 *
 * Revision 1.1.106.1  1994/02/04  20:24:08  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:15:28  devsrc]
 * 
 * Revision 1.1.104.1  1993/12/07  17:29:34  jaffe
 * 	New File from Transarc 1.0.3a
 * 	[1993/12/02  21:30:52  jaffe]
 * 
 * $EndLog$
 */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/osi/RCS/osi_lock_mach.h,v 65.4 1998/03/24 17:20:24 lmc Exp $ */
/* Copyright (C) 1993 Transarc Corporation - All rights reserved */

/*
 * Machine-specific locking defintions.
 */

#ifndef TRANSARC_OSI_LOCK_MACH_H
#define	TRANSARC_OSI_LOCK_MACH_H	    1

#ifdef _KERNEL

#include <dcedfs/param.h>
#include <dcedfs/stds.h>

#define LOCK_HASH_CONDS 64	/* must be power of 2 */

/* 
 * Standard "data lock". All locks wait on excl_locked except for READ_LOCK, 
 * which waits on readers_reading 
 */
#define DFS_LOCK_DEBUG

struct lock_data {
    unsigned char wait_states; 		/* type of lockers waiting */
    unsigned char excl_locked;		/* boosted, shared or write lock? */
    unsigned short readers_reading;	/* readers actually with read locks */
    unsigned short recursive_locks;	/* Owner of lock has locked it more than once */
#ifdef DFS_LOCK_DEBUG
    /* will indicate the write or shared lock holder or the last read lock
     * holder. Valid only if excl_locked field is set. Otherwise, will 
     * denote the last lock releaser (basically, we won't reset lock_holder
     * field during lock release).  
     */
    long lock_holder; 		
#endif /* DFS_LOCK_DEBUG */
};

#define lock_ObtainWrite_r(x) 	lock_ObtainWrite(x)
#define lock_ReleaseWrite_r(x) 	lock_ReleaseWrite(x)
#define lock_ObtainRead_r(x) 	lock_ObtainRead(x)
#define lock_ReleaseRead_r(x) 	lock_ReleaseRead(x)


typedef struct lock_data osi_rwlock_t;
typedef struct lock_data osi_dlock_t;
typedef opaque osi_cv_t;

/*
 * Operations on mutexes; we implement the mutex as a writer lock.
 */
typedef mutex_t			osi_mutex_t;
#define osi_mutex_enter(mp)	mutex_lock(mp, PZERO)
#define osi_mutex_exit(mp)	mutex_unlock(mp)

#define osi_mutex_init(mp, name)	mutex_init(mp, MUTEX_DEFAULT, name)
#if 0
/*  These aren't used, and I don't want to #define them to nothing
 *	because if they are used we want to know it!
 */
#define osi_mutex_enter_no_wait(mp)
#define osi_mutex_destroy(mp)
#define osi_mutex_held(mp)		
#endif

/*
 * Operations on R/W locks:
 */

#define osi_rwlock_writer(rwp)		lock_ObtainWrite(rwp)
#define osi_rwlock_write_unlock(rwp)	lock_ReleaseWrite(rwp)

#if 0
#define osi_rwlock_init(rwp)		lock_Init(rwp)
#define osi_rwlock_destroy(rwp)		lock_Destroy(rwp)
#define osi_rwlock_reader(rwp)		lock_ObtainRead(rwp)
#define osi_rwlock_reader_no_wait(rwp)	lock_ObtainReadNoBlock(rwp)
#define osi_rwlock_writer_no_wait(rwp)	lock_ObtainWriteNoBlock(rwp)
#define osi_rwlock_read_unlock(rwp)	lock_ReleaseRead(rwp)
#define osi_rwlock_read_held(rwp)	(lock_Check(rwp) > 0)
#define osi_rwlock_write_held(rwp)	lock_WriteLocked(rwp)
#define osi_rwlock_downgrade(rwp)	lock_ConvertWtoR(rwp)
#define osi_rwlock_upgrade(rwp)		lock_ConvertRtoW(rwp)
#endif

/*
 * Operations on condition variables:
 */
#define osi_cv_init(cvp)
#define osi_cv_destroy(cvp)
#define osi_cv_wait(cvp, mp) \
    (osi_SleepW((opaque)cvp, (osi_dlock_t *)mp), \
     osi_mutex_enter(mp))
#define osi_cv_wait_intr(cvp, mp) \
    (osi_SleepWI((opaque)cvp, (osi_dlock_t *)mp), \
     osi_mutex_enter(mp))
#define osi_cv_signal(cvp)		osi_Wakeup(cvp)
#define osi_cv_broadcast(cvp)		osi_Wakeup(cvp)

/*
 * Operations on data locks:
 */
#define lock_Check(lock) \
    ((lock)->excl_locked ? (int) -1 : (int) (lock)->readers_reading)

#define lock_WriteLocked(lock) \
    ((lock)->excl_locked != 0)

		
int lock_Init(osi_dlock_t *ap);
void lock_Destroy(osi_dlock_t *lock);
void osi_lock_init(void);


/*
 * the (potentially) blocking versions of the lock routines
 */
void lock_ObtainWrite(osi_dlock_t *alock);
void lock_ObtainRead(osi_dlock_t *alock);
void lock_ObtainShared(osi_dlock_t *alock);
void lock_UpgradeSToW(osi_dlock_t *alock);

/*
 * the non-blocking versions
 */
int lock_ObtainWriteNoBlock(osi_dlock_t *alock);
int lock_ObtainReadNoBlock(osi_dlock_t *alock);
int lock_ObtainSharedNoBlock(osi_dlock_t *alock);
int lock_UpgradeSToWNoBlock(osi_dlock_t *alock);

/*
 * lock releasing routines
 */
void lock_ReleaseWrite(osi_dlock_t *alock);
void lock_ReleaseRead(osi_dlock_t *alock);
#define lock_ReleaseShared(x)	lock_ReleaseWrite(x)

/*
 * lock downgrading routines
 */
void lock_ConvertWToS(osi_dlock_t *alock);
void lock_ConvertWToR(osi_dlock_t *alock);
void lock_ConvertSToR(osi_dlock_t *alock);

/*
 * Release lock and sleep.
 */
void osi_SleepR(opaque addr, osi_dlock_t *alock);
void osi_SleepS(opaque addr, osi_dlock_t *alock);
void osi_SleepW(opaque addr, osi_dlock_t *alock);
void osi_Sleep2(opaque addr, osi_dlock_t *alock, int atype);
int osi_SleepRI(opaque addr, osi_dlock_t *alock);
int osi_SleepSI(opaque addr, osi_dlock_t *alock);
int osi_SleepWI(opaque addr, osi_dlock_t *alock);

/* osi_PreemptionOff/osi_RestorePreemption related macros */

#define SAFE_MUTEX_LOCK(lock, pri) \
	{\
		int preemptlevel = 0; \
		if((osi_preemptLevel) && (osi_preemptheld == osi_ThreadUnique())) \
			preemptlevel = release_preemption(); \
		mutex_lock(lock, (pri | PLTWAIT)); \
		if(preemptlevel) \
			acquire_preemption(preemptlevel); \
        }

#define SAFE_CV_WAIT(cond, lock) \
	{\
		int preemptlevel = 0; \
		if((osi_preemptLevel) && (osi_preemptheld == osi_ThreadUnique())) \
			preemptlevel = release_preemption(); \
		cv_wait(cond, lock); \
		if(preemptlevel) \
			acquire_preemption(preemptlevel); \
	}

#endif /* KERNEL */
#endif /* TRANSARC_OSI_LOCK_MACH_H */
