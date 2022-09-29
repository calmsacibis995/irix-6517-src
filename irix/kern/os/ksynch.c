
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

#ident	"$Revision: 1.17 $"

#include <sys/types.h>
#include <sys/splock.h>
#include <sys/ksynch.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/ddi.h>

/* ARGSUSED */
lock_t *
lock_alloc(uchar_t hierarchy, pl_t min_ipl, lkinfo_t *lkinfop, int flag)
{
	lock_t *lockp = (lock_t *)kmem_zalloc(sizeof(*lockp), flag);

	if (lockp)
		spinlock_init(lockp, 0);
	return(lockp);
}

/* 
 * Initializes the given spin lock.
 */
/* ARGSUSED */
void
lock_init(lock_t *lockp,
	uchar_t hierarchy,
	pl_t min_ipl,
	lkinfo_t *lkinfop)
{
	if (lockp)
		spinlock_init(lockp, 0);
}

/* 
 * Frees the given spin lock.
 */
void
lock_dealloc(lock_t *lockp)
{
	spinlock_destroy(lockp);
	kmem_free((void *)lockp, sizeof(*lockp));
}

/* 
 * Initializes the given spin lock.
 */
void
lock_destroy(lock_t *lockp)
{
	if (lockp)
		spinlock_destroy(lockp);
}

/*
 * Acquire a lock.
 */
/*ARGSUSED*/
int 
ddi_lock(lock_t *lockp, pl_t ipl)
{
	return(mutex_spinlock_spl(lockp, splhi));
}

/*
 * Unlocks a lock.
 */
void
ddi_unlock(lock_t *lockp, int ipl)
{
	mutex_io_spinunlock(lockp, ipl);
}

/*
 * Try to acquire a basic lock
 */
/* ARGSUSED */
int
trylock(lock_t *lockp, pl_t ipl)
{
	int rv;

	if ((rv = mutex_spintrylock_spl(lockp, splhi)) == 0)
		return(INVPL);
	else
		return(rv);
}

/*
 * Allocates and initializes a sleep lock.
 */
/* ARGSUSED */
sleep_t *
sleep_alloc(int arg, lkinfo_t *lkinfop, int km_flags)
{
	return sema_alloc(km_flags, 1, "ddi/dki");
}

/* ARGSUSED */
void
sleep_init(sleep_t *sp, int arg, lkinfo_t *lkinfop)
{
	initnsema(sp, 1, "ddi/dki");
}

/* 
 * Frees the given sleep lock
 */
void
sleep_dealloc(sleep_t *lockp)
{
	sema_dealloc(lockp);
}

/*
 * acquire a sleep lock
 * the caller will not be interrupted by signals while sleeping
 * inside SLEEP_LOCK.
 */
/* ARGSUSED */
void
sleep_lock(sleep_t *lockp, int priority)
{
	psema(lockp, PZERO);
}

/*
 * Acquires a lock.  Returns when signaled.
 */
boolean_t
sleep_lock_sig(sleep_t *lockp, int priority)
{
	int rv;

	priority &= PMASK;
	
	if (priority <= PZERO)
		priority = PZERO + 1;
	rv = psema(lockp, priority);
	if (rv == -1)
		return(B_FALSE);
	else
		return(B_TRUE);
}

/*
 *  Attempts to acquire the lock without sleeping.
 */
boolean_t
sleep_trylock(sleep_t *lockp)
{
	return((boolean_t)cpsema(lockp));

}

/*
 * Releases the sleep lock. If there are waiting processes,
 * the first process on the semaphore is dequeued and 
 * scheduled to run.
 */
void
sleep_unlock(sleep_t *lockp)
{
	vsema(lockp);
}

/* 
 * See if a lock is available.
 * 
 * Returns: TRUE if the lock is available, FALSE otherwise.
 */
boolean_t
sleep_lockavail(sleep_t *lockp)
{
	if (valusema(lockp) > 0)
		return(B_TRUE);
	return(B_FALSE);
}

/* ARGSUSED */
void
ddi_mutex_lock(mutex_t *lockp, int priority)
{
	mutex_lock(lockp, PZERO);
}

mutex_t *
ddi_mutex_alloc(int type, int flags, char *name)
{
	return mutex_alloc(type, flags, name, METER_NO_SEQ);
}

void
ddi_sv_wait(sv_t *svp, void *lkp, int rv)
{
	sv_wait(svp, 0, lkp, rv);
}

int
ddi_sv_wait_sig(sv_t *svp, void *lkp, int s)
{
	/*
	 * If signal woke caller, but signal was cancelled,
	 * must have been job control signal?
	 * If so, fake normal wakeup.
	 */
	if (sv_wait_sig(svp, 0, lkp, s) == -1)
		return(B_FALSE);

	return(B_TRUE);
}

rwlock_t *
rw_alloc(char *name, int sleepflag)
{
	register rwlock_t *rwp = kmem_alloc(sizeof(rwlock_t), sleepflag);
	if (rwp)
		mrinit(rwp, name);
	return rwp;
}

void
rw_dealloc(rwlock_t *rwp)
{
	mrfree(rwp);
	kmem_free(rwp, sizeof(rwlock_t));
}

void
rw_rdlock(rwlock_t *rwp)
{
	mraccess(rwp);
}

void
rw_wrlock(rwlock_t *rwp)
{
	mrupdate(rwp);
}

void
rw_tryrdlock(rwlock_t *rwp)
{
	mrtryaccess(rwp);
}

void
rw_trywrlock(rwlock_t *rwp)
{
	mrtryupdate(rwp);
}
