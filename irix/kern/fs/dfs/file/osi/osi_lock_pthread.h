/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_lock_pthread.h,v $
 * Revision 65.2  1997/10/24 22:01:12  gwehrman
 * Changed include for pthread.h
 * to dce/pthread.h.
 *
 * Revision 65.1  1997/10/20  19:17:43  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.129.1  1996/10/02  18:10:55  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:45:00  damon]
 *
 * Revision 1.1.124.2  1994/06/09  14:17:03  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:29:14  annie]
 * 
 * Revision 1.1.124.1  1994/02/04  20:26:35  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:16:26  devsrc]
 * 
 * Revision 1.1.122.1  1993/12/07  17:31:09  jaffe
 * 	New File from Transarc 1.0.3a
 * 	[1993/12/02  21:32:22  jaffe]
 * 
 * $EndLog$
 */
/* Copyright (C) 1994 Transarc Corporation - All rights reserved. */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/osi/RCS/osi_lock_pthread.h,v 65.2 1997/10/24 22:01:12 gwehrman Exp $ */

/* basic defines for user-level lock package */

#ifndef _TRANSARC_OSI_LOCK_PTHREAD_H
#define _TRANSARC_OSI_LOCK_PTHREAD_H 1

#ifndef _KERNEL

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dce/pthread.h>

#define LOCK_HASH_CONDS 64	/* must be power of 2 */

/*
 * Standard "data lock". All locks wait on excl_locked except for READ_LOCK,
 * which waits on readers_reading
 */
struct lock_data {
    unsigned char wait_states;		/* type of lockers waiting */
    unsigned char excl_locked;		/* boosted, shared or write lock? */
    unsigned short readers_reading;	/* readers actually with read locks */
};

typedef pthread_mutex_t osi_mutex_t;
typedef struct lock_data osi_rwlock_t;
typedef struct lock_data osi_dlock_t;
typedef pthread_cond_t osi_cv_t;

/*
 * Operations on mutexes; we implement the mutex as a writer lock.
 */
#define osi_mutex_init(mp)    pthread_mutex_init(mp,pthread_mutexattr_default)
#define osi_mutex_destroy(mp) pthread_mutex_destroy(mp)
#define osi_mutex_enter(mp)   pthread_mutex_lock(mp)
#define osi_mutex_enter_no_wait(mp) pthread_mutex_trylock(mp)
#define osi_mutex_exit(mp)    pthread_mutex_unlock(mp)
#define osi_mutex_held(mp) \
    (pthread_mutex_trylock(mp) ? (pthread_mutex_unlock(mp), 0) : 1)
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
#define osi_cv_init(cvp)       pthread_cond_init(cvp, pthread_condattr_default)
#define osi_cv_destroy(cvp)    pthread_cond_destroy(cvp)
#define osi_cv_wait(cvp, mp)   pthread_cond_wait(cvp,mp)
#define osi_cv_wait_intr(cvp, mp) pthread_cond_wait(cvp,mp)
#define osi_cv_signal(cvp)     pthread_cond_signal(cvp)
#define osi_cv_broadcast(cvp)  pthread_cond_broadcast(cvp)

/*
 * Operations on data locks:
 */
#define lock_Check(lock) \
    ((lock)->excl_locked ? (int) -1 : (int) (lock)->readers_reading)

#define lock_WriteLocked(lock) \
    ((lock)->excl_locked != 0)

#define lock_HasOwner(lock)	lock_Check(lock)

int lock_Init(osi_dlock_t *ap);
int lock_InitDesc(osi_dlock_t *ap, const char *desc);
void lock_Destroy(osi_dlock_t *lock);

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
void osi_SleepR(opaque addr, osi_dlock_t *lockP);
void osi_SleepS(opaque addr, osi_dlock_t *lockP);
void osi_SleepW(opaque addr, osi_dlock_t *lockP);

int osi_SleepRI(opaque addr, osi_dlock_t *lockP);
int osi_SleepSI(opaque addr, osi_dlock_t *lockP);
int osi_SleepWI(opaque addr, osi_dlock_t *lockP);

/*
 * sleep with a timeout
 */
/* Comment these out until we can figure out what's going wrong *
void osi_TimedSleepR(opaque addr, osi_dlock_t *lockP,
		     struct timespec *absWaitTime, int *timedOut);
void osi_TimedSleepS(opaque addr, osi_dlock_t *lockP,
		     struct timespec *absWaitTime, int *timedOut);
void osi_TimedSleepW(opaque addr, osi_dlock_t *lockP,
		     struct timespec *absWaitTime, int *timedOut);
*/

/* Preemption friendly versions (no distinction in user space). */

#define lock_ObtainRead_r(l) lock_ObtainRead(l)
#define lock_ObtainWrite_r(l) lock_ObtainWrite(l)
#define lock_ReleaseRead_r(l) lock_ReleaseRead(l)
#define lock_ReleaseWrite_r(l) lock_ReleaseWrite(l)
#define osi_SleepW_r(a,l) osi_SleepW(a,l)
#define osi_SleepWI_r(a,l) osi_SleepWI(a,l)

#endif /* !KERNEL */
#endif /*  _TRANSARC_OSI_LOCK_PTHREAD_H */
