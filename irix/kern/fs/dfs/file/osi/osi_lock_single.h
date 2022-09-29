/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_lock_single.h,v $
 * Revision 65.1  1997/10/20 19:17:43  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.131.1  1996/10/02  18:11:01  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:45:01  damon]
 *
 * Revision 1.1.126.2  1994/06/09  14:17:04  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:29:16  annie]
 * 
 * Revision 1.1.126.1  1994/02/04  20:26:38  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:16:27  devsrc]
 * 
 * Revision 1.1.124.1  1993/12/07  17:31:14  jaffe
 * 	New File from Transarc 1.0.3a
 * 	[1993/12/02  21:32:39  jaffe]
 * 
 * $EndLog$
 */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/osi/RCS/osi_lock_single.h,v 65.1 1997/10/20 19:17:43 jdoak Exp $ */
/* Copyright (C) 1994, 1993 Transarc Corporation - All rights reserved */
/*
 * Locking definitions for single-threaded kernels.
 */
#ifndef _TRANSARC_OSI_LOCK_SINGLE_H
#define _TRANSARC_OSI_LOCK_SINGLE_H

#if defined(KERNEL) && OSI_SINGLE_THREADED
/*
 * Standard "data lock". All locks wait on excl_locked except for READ_LOCK,
 * which waits on readers_reading
 */
struct lock_data {
    unsigned char wait_states;		/* type of lockers waiting */
    unsigned char excl_locked;		/* boosted, shared or write lock? */
    unsigned char readers_reading;	/* readers actually with read locks */
    unsigned char num_waiting;		/* probably need this soon */
};

/*
 * Basic lock types.  We implement everything in terms of the
 * "data lock" above.
 */
typedef struct lock_data osi_mutex_t;
typedef struct lock_data osi_rwlock_t;
typedef struct lock_data osi_dlock_t;
typedef opaque osi_cv_t;

#define LOCK_DATA_INITIALIZER {0, 0, 0, 0}

/*
 * Operations on mutexes; we implement the mutex as a writer lock.
 */
#define osi_mutex_init(mp)	lock_Init(mp)
#define osi_mutex_destroy(mp)	lock_Destroy(mp)
#define osi_mutex_enter(mp)	lock_ObtainWrite(mp)
#define osi_mutex_enter_no_wait(mp) lock_ObtainWriteNoBlock(mp)
#define osi_mutex_exit(mp)	lock_ReleaseWrite(mp)
#define osi_mutex_held(mp)	lock_WriteLocked(mp)
/* See SUNOS5 version for explanation */
#define osi_mutex_hasowner(mp)	osi_mutex_held(mp)

/*
 * Operations on R/W locks:
 */
#define osi_rwlock_init(rwp)		lock_Init(rwp)
#define osi_rwlock_destroy(rwp)		lock_Destroy(rwp)
#define osi_rwlock_reader(rwp)		lock_ObtainRead(rwp)
#define osi_rwlock_writer(rwp)		lock_ObtainWrite(rwp)
#define osi_rwlock_reader_no_wait(rwp)	lock_ObtainReadNoBlock(rwp)
#define osi_rwlock_writer_no_wait(rwp)	lock_ObtainWriteNoBlock(rwp)
#define osi_rwlock_read_unlock(rwp)	lock_ReleaseRead(rwp)
#define osi_rwlock_write_unlock(rwp)	lock_ReleaseWrite(rwp)
#define osi_rwlock_read_held(rwp)	(lock_Check(rwp) > 0)
#define osi_rwlock_write_held(rwp)	lock_WriteLocked(rwp)
#define osi_rwlock_downgrade(rwp)	lock_ConvertWtoR(rwp)
#define osi_rwlock_upgrade(rwp)		lock_ConvertRtoW(rwp)

/*
 * Operations on condition variables:
 */
#define osi_cv_init(cvp)
#define osi_cv_destroy(cvp)
extern void osi_cv_wait_single(osi_cv_t *cvp, osi_mutex_t *mp);
extern void osi_cv_wait_intr_single(osi_cv_t *cvp, osi_mutex_t *mp);
#define osi_cv_wait(cvp, mp)		osi_cv_wait_single(cvp, mp)
#define osi_cv_wait_intr(cvp, mp)	osi_cv_wait_intr_single(cvp, mp)
#define osi_cv_signal(cvp)		osi_Wakeup(cvp)
#define osi_cv_broadcast(cvp)		osi_Wakeup(cvp)

#define lock_HasOwner(lock)	lock_Check(lock)

#define lock_ObtainRead(lock) \
    if (!((lock)->excl_locked & WRITE_LOCK)) \
	(lock)->readers_reading++; \
    else \
	Lock_Obtain(lock, READ_LOCK)

#define lock_ObtainWrite(lock) \
    if (!(lock)->excl_locked && !(lock)->readers_reading) \
	(lock)->excl_locked = WRITE_LOCK; \
    else \
	Lock_Obtain(lock, WRITE_LOCK)

#define lock_ObtainShared(lock) \
    if (!(lock)->excl_locked) \
	(lock)->excl_locked = SHARED_LOCK; \
    else \
	Lock_Obtain(lock, SHARED_LOCK)

#define lock_UpgradeSToW(lock) \
    if (!(lock)->readers_reading) { \
	osi_assert((lock)->excl_locked == SHARED_LOCK); \
	(lock)->excl_locked = WRITE_LOCK; \
    } else \
	Lock_Obtain(lock, BOOSTED_LOCK)

/*
 * this must only be called with a WRITE or boosted SHARED lock!
 */
#define lock_ConvertWToS(lock) \
    MACRO_BEGIN \
	osi_assert((lock)->excl_locked == WRITE_LOCK); \
	(lock)->excl_locked = SHARED_LOCK; \
	if((lock)->wait_states) \
	    Lock_ReleaseR(lock); \
    MACRO_END

#define lock_ConvertWToR(lock) \
     MACRO_BEGIN \
	osi_assert(((lock)->excl_locked & (SHARED_LOCK|WRITE_LOCK))); \
	(lock)->excl_locked &= ~(SHARED_LOCK | WRITE_LOCK); \
	(lock)->readers_reading++; \
	Lock_ReleaseR(lock); \
    MACRO_END

#define lock_ConvertSToR(lock) \
    MACRO_BEGIN \
	osi_assert(((lock)->excl_locked & (SHARED_LOCK|WRITE_LOCK))); \
	(lock)->excl_locked &= ~(SHARED_LOCK | WRITE_LOCK); \
	(lock)->readers_reading++; \
	Lock_ReleaseR(lock); \
    MACRO_END

/*
 * Upgrade from reader to writer if no contention for the lock.
 */
#define lock_ConvertRtoW(lock) \
    (lock_WriteLocked(lock) || lock_Check(lock) > 1 ? 0 : \
     (--(lock)->readers_reading, (lock)->excl_locked = WRITE_LOCK, 1))

#define lock_ReleaseRead(lock) \
    MACRO_BEGIN \
	osi_assert((lock)->readers_reading > 0); \
	if (!--(lock)->readers_reading && (lock)->wait_states) \
	    Lock_ReleaseW(lock) ; \
    MACRO_END

#define lock_ReleaseWrite(lock) \
    MACRO_BEGIN \
	osi_assert((lock)->excl_locked == WRITE_LOCK); \
	(lock)->excl_locked &= ~WRITE_LOCK; \
	if ((lock)->wait_states) Lock_ReleaseR(lock); \
    MACRO_END

/*
 * can be used on shared or boosted (write) locks
 */
#define lock_ReleaseShared(lock) \
    MACRO_BEGIN \
	osi_assert(((lock)->excl_locked & (SHARED_LOCK|WRITE_LOCK))); \
	(lock)->excl_locked &= ~(SHARED_LOCK | WRITE_LOCK); \
	if ((lock)->wait_states) Lock_ReleaseR(lock); \
    MACRO_END

#define lock_Check(lock) \
    ((lock)->excl_locked ? (int) -1 : (int) (lock)->readers_reading)

#define lock_WriteLocked(lock) \
    ((lock)->excl_locked != 0)

/* routines that only try once to obtain a lock and return right
 * away if they do not succeed
 */
#define lock_ObtainReadNoBlock(lock) \
    ((!((lock)->excl_locked & WRITE_LOCK)) ? \
	++(lock)->readers_reading : LOCK_NOLOCK)

#define lock_ObtainWriteNoBlock(lock) \
    ((!(lock)->excl_locked && !(lock)->readers_reading) ? \
	(lock)->excl_locked = WRITE_LOCK : LOCK_NOLOCK)

#define lock_ObtainSharedNoBlock(lock) \
    ((!(lock)->excl_locked) ? \
	(lock)->excl_locked = SHARED_LOCK : LOCK_NOLOCK)

#define lock_UpgradeSToWNoBlock(lock) \
    ((!(lock)->readers_reading) ? \
	(lock)->excl_locked = WRITE_LOCK : LOCK_NOLOCK)

#define lock_InitDesc(lock,desc) lock_Init(lock)

/*
 * OSI_SINGLE_THREADED routines exported by lock.c:
 */
extern void lock_Init(osi_dlock_t *lock);
extern void lock_Destroy(osi_dlock_t *lock);
extern void Lock_Obtain(osi_dlock_t *lock, int how);
extern void lock_Release(osi_dlock_t *lockp, int type);
extern void Lock_ReleaseR(osi_dlock_t *lock);
extern void Lock_ReleaseW(osi_dlock_t *lock);
extern void osi_SleepR(opaque addr, osi_dlock_t *alock);
extern void osi_SleepW(opaque addr, osi_dlock_t *alock);
extern void osi_SleepS(opaque addr, osi_dlock_t *alock);
extern void osi_Sleep2(opaque addr, osi_dlock_t *alock, int alockType);
extern int osi_SleepRI(opaque addr, osi_dlock_t *alock);
extern int osi_SleepWI(opaque addr, osi_dlock_t *alock);
extern int osi_SleepSI(opaque addr, osi_dlock_t *alock);

/* Preemption friendly versions (no real distinction on these platforms). */

#define lock_ObtainRead_r(l) lock_ObtainRead(l)
#define lock_ObtainWrite_r(l) lock_ObtainWrite(l)
#define lock_ReleaseRead_r(l) lock_ReleaseRead(l)
#define lock_ReleaseWrite_r(l) lock_ReleaseWrite(l)
#define osi_SleepW_r(a,l) osi_SleepW(a,l)
#define osi_SleepWI_r(a,l) osi_SleepWI(a,l)

#endif /* KERNEL && OSI_SINGLE_THREADED */
#endif /* _TRANSARC_OSI_LOCK_SINGLE_H */
