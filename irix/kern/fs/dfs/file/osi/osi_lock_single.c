/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: osi_lock_single.c,v 65.3 1998/03/23 16:26:18 gwehrman Exp $";
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
 * $Log: osi_lock_single.c,v $
 * Revision 65.3  1998/03/23 16:26:18  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 19:58:17  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:17:43  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.130.1  1996/10/02  18:10:58  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:45:01  damon]
 *
 * Revision 1.1.125.2  1994/06/09  14:17:04  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:29:15  annie]
 * 
 * Revision 1.1.125.1  1994/02/04  20:26:36  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:16:26  devsrc]
 * 
 * Revision 1.1.123.1  1993/12/07  17:31:13  jaffe
 * 	New File from Transarc 1.0.3a
 * 	[1993/12/02  21:32:27  jaffe]
 * 
 * $EndLog$
 */

/* Copyright (C) 1993, 1990, 1989 Transarc Corporation - All rights reserved */

#define DEBUG_THIS_FILE OSI_DEBUG_LOCK

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/debug.h>
#include <dcedfs/osi.h>
#include <dcedfs/lock.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/osi/RCS/osi_lock_single.c,v 65.3 1998/03/23 16:26:18 gwehrman Exp $")

/*
 * Lock package implementation for single-threaded kernels, using
 * sleep and wakeup for synchronization.
 */
#if defined(KERNEL) && OSI_SINGLE_THREADED
void
lock_Init(struct lock_data *lock)
{
    lock->readers_reading = 0;
    lock->excl_locked = 0;
    lock->wait_states = 0;
    lock->num_waiting = 0;
}

void
lock_Destroy(struct lock_data *lock)
{
    afsl_PAssert(
	lock->readers_reading == 0 &&
	lock->excl_locked == 0 && lock->num_waiting == 0,
	("lock_Destroy(%x): lock contents (%x, %x, %x, %x)\n",
	    lock, lock->wait_states, lock->excl_locked,
	    lock->readers_reading, lock->num_waiting));
}

void
Lock_Obtain(struct lock_data *lock, int how)
{
    osi_SleepPrint (("Obtain: waiting for %s lock, %d already waiting",
		     (how == READ_LOCK ? "read" :
		      (how == WRITE_LOCK ? "write" :
		       (how == SHARED_LOCK ? "shared" :
			(how == BOOSTED_LOCK ? "boosted" :
			 "UNKNOWN TYPE")))), lock->num_waiting));
    switch (how) {
	case READ_LOCK:
            lock->num_waiting++;
	    do {
	      lock->wait_states |= READ_LOCK;
	      osi_Sleep(&lock->readers_reading);
	    } while (lock->excl_locked & WRITE_LOCK);
	    lock->num_waiting--;
	    lock->readers_reading++;
	    break;

	case WRITE_LOCK:
	    lock->num_waiting++;
	    do {
	      lock->wait_states |= WRITE_LOCK;
	      osi_Sleep(&lock->excl_locked);
	    } while (lock->excl_locked || lock->readers_reading);
	    lock->num_waiting--;
	    lock->excl_locked = WRITE_LOCK;
	    break;

	case SHARED_LOCK:
	    lock->num_waiting++;
	    do {
	      lock->wait_states |= SHARED_LOCK;
	      osi_Sleep(&lock->excl_locked);
	    } while (lock->excl_locked);
	    lock->num_waiting--;
	    lock->excl_locked = SHARED_LOCK;
	    break;

	case BOOSTED_LOCK:
	    lock->num_waiting++;
	    do {
	      lock->wait_states |= WRITE_LOCK;
	      osi_Sleep(&lock->excl_locked);
	    } while (lock->readers_reading);
	    lock->num_waiting--;
	    lock->excl_locked = WRITE_LOCK;
	    break;

	default:
	    panic("afs locktype");
    }
}

/*
 * release a lock, giving preference to new readers
 */
void
Lock_ReleaseR(struct lock_data *lock)
{
    if (lock->wait_states & READ_LOCK) {
	lock->wait_states &= ~READ_LOCK;
	osi_Wakeup(&lock->readers_reading);
    } else {
	lock->wait_states &= ~EXCL_LOCKS;
	osi_Wakeup(&lock->excl_locked);
    }
}

/*
 * release a lock, giving preference to new writers
 */
void
Lock_ReleaseW(struct lock_data *lock)
{
    if (lock->wait_states & EXCL_LOCKS) {
	lock->wait_states &= ~EXCL_LOCKS;
	osi_Wakeup(&lock->excl_locked);
    } else {
	lock->wait_states &= ~READ_LOCK;
	osi_Wakeup(&lock->readers_reading);
    }
}

/*
 * These next guys exist to provide an interface to drop a lock atomically
 * with blocking.  They're trivial to do in a non-preemptive Kernel
 * environment.
 */

/*
 * release a read lock and sleep on an address, atomically
 */
void
osi_SleepR(opaque addr, struct lock_data *alock)
{
    lock_ReleaseRead(alock);
    osi_Sleep(addr);
}

/*
 * release a write lock and sleep on an address, atomically
 */
void
osi_SleepW(opaque addr, struct lock_data *alock)
{
    lock_ReleaseWrite(alock);
    osi_Sleep(addr);
}


/*
 * release a shared lock and sleep on an address, atomically
 */
void
osi_SleepS(opaque addr, struct lock_data *alock)
{
    lock_ReleaseShared(alock);
    osi_Sleep(addr);
}

/*
 * release a generic lock and sleep on an address, atomically
 */
void
osi_Sleep2(opaque addr, struct lock_data *alock, int atype)
{
    lock_Release(alock, atype);
    osi_Sleep(addr);
}


/*
 * release a read lock and sleep interruptibly on an address, atomically
 */
int
osi_SleepRI(opaque addr, struct lock_data *alock)
{
    lock_ReleaseRead(alock);
    return osi_SleepInterruptably(addr);
}


/*
 * release a write lock and sleep interruptibly on an address, atomically
 */
int
osi_SleepWI(opaque addr, struct lock_data *alock)
{
    lock_ReleaseWrite(alock);
    return osi_SleepInterruptably(addr);
}


/*
 * release a shared lock and sleep interruptibly on an address, atomically
 */
int
osi_SleepSI(opaque addr, struct lock_data *alock)
{
    lock_ReleaseShared(alock);
    return osi_SleepInterruptably(addr);
}

/*
 * Release the mutex, sleep on cvp, and then reacquire the mutex, atomically.
 */
void
osi_cv_wait_single(osi_cv_t *cvp, osi_mutex_t *mp)
{
    osi_mutex_exit(mp);
    osi_Sleep(cvp);
    osi_mutex_enter(mp);
}

/*
 * The same, interruptibly.
 */
void
osi_cv_wait_intr_single(osi_cv_t *cvp, osi_mutex_t *mp)
{
    osi_mutex_exit(mp);
    osi_SleepInterruptably(cvp);
    osi_mutex_enter(mp);
}
#endif /* KERNEL && OSI_SINGLE_THREADED */

