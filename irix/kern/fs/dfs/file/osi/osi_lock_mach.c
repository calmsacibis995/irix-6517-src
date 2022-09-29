/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: osi_lock_mach.c,v 65.4 1998/03/23 16:26:31 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * HISTORY
 * $Log: osi_lock_mach.c,v $
 * Revision 65.4  1998/03/23 16:26:31  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1998/01/07 15:46:46  lmc
 * Added some prototypes and added (void) for some, which is required to
 * recognize a function without any parameters as prototyped.  Changed
 * hashVal to long everywhere.  Removed the use of PNOSTOP flag.  Fixed
 * a mutex_spinunlock to pass in the value from mutex_spinlock.  I don't
 * believe this particular chunk of code is ever used, but its correct
 * now.
 *
 * Revision 65.2  1997/11/06  19:58:22  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/24  14:29:50  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:43  jdoak
 * Initial revision
 *
 * Revision 64.8  1997/08/26  22:00:46  bdr
 * High load average fix.  Make sure we are setting the PLTWAIT flag when ever
 * we call things like sv_wait and sv_wait_sig to prevent us from getting counted
 * in the nrun load average when we are sleeping.
 *
 * Revision 64.6  1997/06/24  18:01:36  lmc
 * Changes for memory mapped files and the double locking problem which
 * is seen when dirty pages are hung off the vnode for memory mapped files.
 * The scp and dcp locks are dropped in cm_read.  Locks that are held
 * by a thread as a write lock are allowed to be recursively locked by
 * that same thread as a write lock.  Memory mapped files changed fsync
 * to add a flag telling whether or not it came from an msync.  Various
 * other changes to flushing and tossing pages.
 *
 * Revision 64.4  1997/03/14  19:25:48  lord
 * Missing mutex_spinunlock in case where locks are gained uncontested
 *
 * Revision 64.1  1997/02/14  19:46:50  dce
 * *** empty log message ***
 *
 * Revision 1.8  1996/12/14  00:15:49  vrk
 * Removed lock_PackageInit() and CHECKINIT() definition and calls. Now,
 * we  initialize all osi related locks thru a osi_lock_init() from
 * afs_init().
 *
 * Also added debug info for lock_data structure - we store
 * the write locker in a debug field "lock_holder".
 *
 *
 */

/*
 * Kernel level locking based on the pthread implementation.
 */

#if defined(KERNEL)

/* VRK */
#define DFS_LOCK_DEBUG

#include <dcedfs/param.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <dcedfs/osi.h>
#include <osi_lock_mach.h>
#include <dcedfs/lock.h>

/*
 * XXX These don't really belong here.  Note that we rely on the
 * (currently true) assumption that zero-filled mutexes and cond.
 * vars. are properly initialized.  We will need to provide an interface
 * for initializing these objects explicitly if this assumption ever
 * becomes false.
 */
osi_mutex_t osi_iodoneLock;
osi_cv_t osi_biowaitCond;

/* Made the following non-static - so that we can view these variables */
sv_t lock_cond[LOCK_HASH_CONDS];
lock_t lock_mutex[LOCK_HASH_CONDS];

sv_t sleep_cond[LOCK_HASH_CONDS];
lock_t sleep_mutex[LOCK_HASH_CONDS];

lock_t osi_preemptLock;
sv_t osi_preemptFree;
int osi_preemptLevel = 0;
long osi_preemptheld;

static int release_preemption(void);
static int acquire_preemption(int val);

void
osi_lock_init(void)
{
    int i;

    /* called from afs_init during system startup */
    for (i = 0; i < LOCK_HASH_CONDS; i++) {
	spinlock_init(&lock_mutex[i], NULL);
	sv_init(&lock_cond[i], SV_DEFAULT, NULL);
	spinlock_init(&sleep_mutex[i], NULL);
	sv_init(&sleep_cond[i], SV_DEFAULT, NULL);
    }

    spinlock_init(&osi_preemptLock, "osi_preemptLock");
    sv_init(&osi_preemptFree, SV_DEFAULT, "osi_preemptFree");
}


int
lock_Init(osi_dlock_t *ap)
{
    ap->wait_states = 0;
    ap->excl_locked = 0;
    ap->readers_reading = 0;
    ap->recursive_locks = 0;
#ifdef DFS_LOCK_DEBUG
    ap->lock_holder = 0;
#endif /* DFS_LOCK_DEBUG */
    return 0;
}

void lock_Destroy(osi_dlock_t *ap)
{
    (void) lock_Init(ap);
}

static long
lock_HashAddr(long a)
{
    long tval = ((a >> 8) + (a >> 16)) & (LOCK_HASH_CONDS - 1);
    return tval;
}

void
lock_ObtainWrite(osi_dlock_t *alock)
{
    long hashVal = lock_HashAddr((long)alock);
    int preempting = 0;
    int ospl;


    while (1) {
	ospl = mutex_spinlock(&lock_mutex[hashVal]);
	if (alock->excl_locked == 0 && alock->readers_reading == 0) {
	    alock->excl_locked = WRITE_LOCK;
#ifdef DFS_LOCK_DEBUG
	    alock->lock_holder = osi_ThreadUnique();
#endif /* DFS_LOCK_DEBUG */
	    mutex_spinunlock(&lock_mutex[hashVal], ospl);
	    break;
	}

	/* if we're the one holding the lock, bump the recursive_lock 
	 * counter and proceed.
	 */
	if (alock->readers_reading == 0 && alock->lock_holder == osi_ThreadUnique()) {
		alock->recursive_locks++;
		mutex_spinunlock(&lock_mutex[hashVal], ospl);
		break;
	}
	/* otherwise, someone's got the lock right now, so we're going
	 * to sleep for a bit.
	 */
	alock->wait_states |= WRITE_LOCK;
#ifdef DFS_LOCK_DEBUG
        if ((alock->excl_locked == WRITE_LOCK) &&
	    (alock->lock_holder == osi_ThreadUnique()) ) {
                printf("lock_ObtainWrite: lock excl held by itself. proc=%0X\n",
                        osi_ThreadUnique());
                panic("lock_ObtainWrite: lock excl held by itself");
        }
#endif /* DFS_LOCK_DEBUG */

	if((osi_preemptLevel) && (osi_preemptheld == osi_ThreadUnique()))
	    preempting = release_preemption();
	/* sleep, and atomically drop the lock */
	sv_wait(&lock_cond[hashVal], PZERO|PLTWAIT, &lock_mutex[hashVal], ospl);
    }

    if(preempting)
	acquire_preemption(preempting);
}

long
lock_GetOwner(osi_dlock_t *alock)
{
	return(alock->lock_holder);
}

int
lock_ObtainWriteNoBlock(osi_dlock_t *alock)
{
    int	rtnVal = 0;
    long hashVal = lock_HashAddr((long)alock);
    int ospl;


    ospl = mutex_spinlock(&lock_mutex[hashVal]);

    if (alock->excl_locked == 0 && alock->readers_reading == 0) {
	alock->excl_locked = WRITE_LOCK;
	rtnVal = WRITE_LOCK;
#ifdef DFS_LOCK_DEBUG
	alock->lock_holder = osi_ThreadUnique();
#endif /* DFS_LOCK_DEBUG */
    } else {
	if (alock->excl_locked != 0 && alock->readers_reading == 0 &&
		alock->lock_holder == osi_ThreadUnique()) {
		alock->recursive_locks++;
		rtnVal = WRITE_LOCK;
    	}
    }

    mutex_spinunlock(&lock_mutex[hashVal], ospl);
    return rtnVal;
}

void
lock_ObtainRead(osi_dlock_t *alock)
{
    long hashVal = lock_HashAddr((long)alock);
    int preempting = 0;
    int ospl;
    
    while (1) {
	ospl = mutex_spinlock(&lock_mutex[hashVal]);
	if ((alock->excl_locked & WRITE_LOCK) == 0) {
	    alock->readers_reading++;
#ifdef DFS_LOCK_DEBUG
	    alock->lock_holder = osi_ThreadUnique();
#endif /* DFS_LOCK_DEBUG */
	    mutex_spinunlock(&lock_mutex[hashVal], ospl);
	    break;
	}
	/* otherwise, we're going to sleep */
	alock->wait_states |= READ_LOCK;

	if((osi_preemptLevel) && (osi_preemptheld == osi_ThreadUnique()))
	    preempting = release_preemption();
	/* sleep, and atomically drop the lock */
	sv_wait(&lock_cond[hashVal], PZERO|PLTWAIT, &lock_mutex[hashVal], ospl);
    }

    if(preempting)
	acquire_preemption(preempting);
}

int
lock_ObtainReadNoBlock(osi_dlock_t *alock)
{
    int	rtnVal = 0;
    long hashVal = lock_HashAddr((long)alock);
    int ospl;

    ospl = mutex_spinlock(&lock_mutex[hashVal]);

    if ((alock->excl_locked & WRITE_LOCK) == 0) {
	rtnVal = ++alock->readers_reading;
#ifdef DFS_LOCK_DEBUG
	alock->lock_holder = osi_ThreadUnique();
#endif /* DFS_LOCK_DEBUG */
    }

    mutex_spinunlock(&lock_mutex[hashVal], ospl);
    return rtnVal;
}

void
lock_ReleaseWrite(osi_dlock_t *alock)
{
    long hashVal = lock_HashAddr((long)alock);
    int ospl;

    ospl = mutex_spinlock(&lock_mutex[hashVal]);

    if (!(alock->excl_locked & (WRITE_LOCK | SHARED_LOCK)))
	panic("lock_ReleaseWrite: lock is not shared- or write-locked");
    /* clear both bits so that we can use this to release an unboosted
     * shared lock, too.
     */
    if (alock->recursive_locks > 0) {
	if (!(alock->recursive_locks < 3 && alock->recursive_locks > 0)) 
		panic("Lock structure toasted..0x%x\n");
	alock->recursive_locks--;
    } else {
        alock->excl_locked &= ~(WRITE_LOCK | SHARED_LOCK);
        if (alock->wait_states) {
	    alock->wait_states = 0;
	    /* wakeup anyone who might have been waiting for this */
	    cv_broadcast(&lock_cond[hashVal]);
        }
    }
    mutex_spinunlock(&lock_mutex[hashVal], ospl);
}

void
lock_ReleaseRead(osi_dlock_t *alock)
{
    long hashVal = lock_HashAddr((long)alock);
    int ospl;

    ospl = mutex_spinlock(&lock_mutex[hashVal]);

    /* clear both bits so that we can use this to release an unboosted
     * shared lock, too.
     */
    if (alock->readers_reading <= 0)
	panic("lock_ReleaseRead: lock is not read-locked");
    if ((--alock->readers_reading) == 0 && alock->wait_states != 0) {
	alock->wait_states = 0;
	/* wakeup anyone who might have been waiting for this */
	cv_broadcast(&lock_cond[hashVal]);
    }
    mutex_spinunlock(&lock_mutex[hashVal], ospl);
}

void
lock_ObtainShared(osi_dlock_t *alock)
{
    long hashVal = lock_HashAddr((long)alock);
    int preempting = 0;
    int ospl;

    while (1) {
	ospl = mutex_spinlock(&lock_mutex[hashVal]);
	if (alock->excl_locked == 0) {	/* compatible with readers */
	    alock->excl_locked = SHARED_LOCK;
#ifdef DFS_LOCK_DEBUG
	    alock->lock_holder = osi_ThreadUnique();
#endif /* DFS_LOCK_DEBUG */
	    mutex_spinunlock(&lock_mutex[hashVal], ospl);
	    break;
	}
	/* otherwise, someone's got the lock right now, so we're going
	 * to sleep for a bit.
	 */
	alock->wait_states |= SHARED_LOCK;

	if((osi_preemptLevel) && (osi_preemptheld == osi_ThreadUnique()))
	    preempting = release_preemption();
	/* sleep, and atomically drop the lock */
	sv_wait(&lock_cond[hashVal], PZERO|PLTWAIT, &lock_mutex[hashVal], ospl);
    }

    if(preempting)
	acquire_preemption(preempting);
}

int
lock_ObtainSharedNoBlock(osi_dlock_t *alock)
{
    int	rtnVal = 0;
    long hashVal = lock_HashAddr((long)alock);
    int ospl;

    ospl = mutex_spinlock(&lock_mutex[hashVal]);

    if (alock->excl_locked == 0) {	/* compatible with readers */
	alock->excl_locked = SHARED_LOCK;
	rtnVal = SHARED_LOCK;
#ifdef DFS_LOCK_DEBUG
	alock->lock_holder = osi_ThreadUnique();
#endif /* DFS_LOCK_DEBUG */
    }

    mutex_spinunlock(&lock_mutex[hashVal], ospl);
    return rtnVal;
}

/* lock_ReleaseShared is a macro that simply calls lock_ReleaseWrite */

/* this call can block waiting for all of the readers to clear out */
void
lock_UpgradeSToW(osi_dlock_t *alock)
{
    long hashVal = lock_HashAddr((long)alock);
    int preempting = 0;
    int ospl;

    while (1) {
	ospl = mutex_spinlock(&lock_mutex[hashVal]);
	/* don't forget that we already have a SHARED lock, so we know
	 * that there aren't any other compatible exclusive-style locks.
	 * Thus, all we have to do is wait for the readers to vanish; no
	 * one else can get going during this time.
	 *
	 * Even though we've released a SHARED lock, a write lock is
	 * incompatible with all the locks that a SHARED lock is, so
	 * there should't be anyone new who should be woken up now.
	 */
	if (!(alock->excl_locked & SHARED_LOCK))
	    panic("lock_UpgradeSToW: lock is not shared-locked");
	if (alock->readers_reading == 0) {
	    /* we can do it right now */
	    alock->excl_locked &= ~SHARED_LOCK;
	    alock->excl_locked |= WRITE_LOCK;
#ifdef DFS_LOCK_DEBUG
	    alock->lock_holder = osi_ThreadUnique();
#endif /* DFS_LOCK_DEBUG */
	    mutex_spinunlock(&lock_mutex[hashVal], ospl);
	    break;
	}
	/* otherwise, we indicate that someone's waiting for a write lock,
	 * and sleep until the readers disappear.
	 */
	alock->wait_states |= WRITE_LOCK;

	if((osi_preemptLevel) && (osi_preemptheld == osi_ThreadUnique()))
	    preempting = release_preemption();
	/* sleep, and atomically drop the lock */
	sv_wait(&lock_cond[hashVal], PZERO|PLTWAIT, &lock_mutex[hashVal], ospl);
    }

    if(preempting)
	acquire_preemption(preempting);
}

int
lock_UpgradeSToWNoBlock(osi_dlock_t *alock)
{
  int	rtnVal = 0;
  long	hashVal = lock_HashAddr((long)alock);
  int ospl;

  ospl = mutex_spinlock(&lock_mutex[hashVal]);

  /* don't forget that we already have a SHARED lock, so we know
   * that there aren't any other compatible exclusive-style locks.
   * Thus, all we have to do is wait for the readers to vanish; no
   * one else can get going during this time.
   *
   * Even though we've released a SHARED lock, a write lock is
   * incompatible with all the locks that a SHARED lock is, so
   * there should't be anyone new who should be woken up now.
   */
  if (!(alock->excl_locked & SHARED_LOCK))
      panic("lock_UpgradeSToWNoBlock: lock is not shared-locked");
  if (alock->readers_reading == 0) {
    /* we can do it right now */
    alock->excl_locked &= ~SHARED_LOCK;
    alock->excl_locked |= WRITE_LOCK;

    rtnVal = WRITE_LOCK;
#ifdef DFS_LOCK_DEBUG
    alock->lock_holder = osi_ThreadUnique();
#endif /* DFS_LOCK_DEBUG */
  }

  mutex_spinunlock(&lock_mutex[hashVal], ospl);
  return rtnVal;
}

/* simply convert a write lock down to a shared lock */
void
lock_ConvertWToS(osi_dlock_t *alock)
{
    long hashVal = lock_HashAddr((long)alock);
    int ospl;

    ospl = mutex_spinlock(&lock_mutex[hashVal]);
    if (!(alock->excl_locked & WRITE_LOCK))
	panic("lock_ConvertWToS: lock is not write-locked");
    alock->excl_locked |= SHARED_LOCK;
    alock->excl_locked &= ~WRITE_LOCK;
#ifdef DFS_LOCK_DEBUG
    alock->lock_holder = osi_ThreadUnique();
#endif /* DFS_LOCK_DEBUG */
    if (alock->wait_states) {
	alock->wait_states = 0;
	/* wake up anyone who might have been waiting for this */
	cv_broadcast(&lock_cond[hashVal]);
    }
    mutex_spinunlock(&lock_mutex[hashVal], ospl);
}

/* simply convert a write lock down to a shared lock */
void
lock_ConvertWToR(osi_dlock_t *alock)
{
    long hashVal = lock_HashAddr((long)alock);
    int ospl;
    int debug_var;

    ospl = mutex_spinlock(&lock_mutex[hashVal]);
    if (!(alock->excl_locked & WRITE_LOCK))
	panic("lock_ConvertWToR: lock is not write-locked");
    debug_var = alock->recursive_locks;
    alock->readers_reading++;
    alock->excl_locked &= ~WRITE_LOCK;
#ifdef DFS_LOCK_DEBUG
    alock->lock_holder = osi_ThreadUnique();
#endif /* DFS_LOCK_DEBUG */
    if (alock->wait_states) {
	alock->wait_states = 0;
	/* wakeup anyone who might have been waiting for this */
	sv_broadcast(&lock_cond[hashVal]);
    }
    mutex_spinunlock(&lock_mutex[hashVal], ospl);
    if (debug_var)
	printf("lock_ConvertWToR: with recursive locks held\n");
}

/* simply convert a write lock down to a shared lock */
void
lock_ConvertSToR(osi_dlock_t *alock)
{
    long hashVal = lock_HashAddr((long)alock);
    int ospl;

    ospl = mutex_spinlock(&lock_mutex[hashVal]);
    if (!(alock->excl_locked & SHARED_LOCK))
	panic("lock_ConvertSToR: lock is not shared-locked");
    alock->readers_reading++;
    alock->excl_locked &= ~SHARED_LOCK;
#ifdef DFS_LOCK_DEBUG
    alock->lock_holder = osi_ThreadUnique();
#endif /* DFS_LOCK_DEBUG */
    if (alock->wait_states) {
	alock->wait_states = 0;
	/* wakeup anyone who might have been waiting for this */
	sv_broadcast(&lock_cond[hashVal]);
    }
    mutex_spinunlock(&lock_mutex[hashVal], ospl);
}

void
osi_SleepR(opaque addr, osi_dlock_t *alock)
{
    long hashVal = lock_HashAddr((long)addr);
    int preempting = 0;
    int ospl;

    /* this function must work atomically w.r.t. sleep_Wakeups directed
     * to the same address.  That is, after the lock is released,
     * we must assume that a wakeup could be issued, and if we lose
     * the wakeup, we're violating a guarantee that we're trying to make.
     *
     * During the period between the lock_ReleaseRead and the time we
     * actually get queued for sleeping, we hold the mutex sleep_mutex[hashVal],
     * which prevents a wakeup from being issued until we're asleep.
     */

    ospl = mutex_spinlock(&sleep_mutex[hashVal]);
    lock_ReleaseRead(alock);
    if((osi_preemptLevel) && (osi_preemptheld == osi_ThreadUnique()))
	preempting = release_preemption();
    /* sleep, and atomically drop the lock */
    sv_wait(&sleep_cond[hashVal], PZERO|PLTWAIT, &sleep_mutex[hashVal], ospl);

    if(preempting)
	acquire_preemption(preempting);
}

void
osi_SleepS(opaque addr, osi_dlock_t *alock)
{
    long hashVal = lock_HashAddr((long)addr);
    int preempting = 0;
    int ospl;

    /* this function must work atomically w.r.t. lock_Wakeups directed
     * to the same address.  That is, after the lock is released,
     * we must assume that a wakeup could be issued, and if we lose
     * the wakeup, we're violating a guarantee that we're trying to make.
     *
     * During the period between the lock_ReleaseShared and the time we
     * actually get queued for sleeping, we hold the mutex lock_mutex[hashVal],
     * which prevents a wakeup from being issued until we're asleep.
     */

    ospl = mutex_spinlock(&sleep_mutex[hashVal]);
    lock_ReleaseShared(alock);
    if((osi_preemptLevel) && (osi_preemptheld == osi_ThreadUnique()))
        preempting = release_preemption();
    /* sleep, and atomically drop the lock */
    sv_wait(&sleep_cond[hashVal], PZERO|PLTWAIT, &sleep_mutex[hashVal], ospl);

    if(preempting)
        acquire_preemption(preempting);
}

void
osi_SleepW(opaque addr, osi_dlock_t *alock)
{
    long hashVal = lock_HashAddr((long)addr);
    int preempting = 0;
    int ospl;

    /* this function must work atomically w.r.t. lock_Wakeups directed
     * to the same address.  That is, after the lock is released,
     * we must assume that a wakeup could be issued, and if we lose
     * the wakeup, we're violating a guarantee that we're trying to make.
     *
     * During the period between the lock_ReleaseWrite and the time we
     * actually get queued for sleeping, we hold the mutex lock_mutex[hashVal],
     * which prevents a wakeup from being issued until we're asleep.
     */

    ospl = mutex_spinlock(&sleep_mutex[hashVal]);
    lock_ReleaseWrite(alock);
    if((osi_preemptLevel) && (osi_preemptheld == osi_ThreadUnique()))
        preempting = release_preemption();
    /* sleep, and atomically drop the lock */
    sv_wait(&sleep_cond[hashVal], PZERO|PLTWAIT, &sleep_mutex[hashVal], ospl);

    if(preempting)
        acquire_preemption(preempting);
}

/* 
 * release a generic lock and sleep on an address 
 */
void
osi_Sleep2(opaque addr, osi_dlock_t *alock, int atype)
{
    long hashVal = lock_HashAddr((long)addr);
    int preempting = 0;
    int ospl;

    /* this function must work atomically w.r.t. lock_Wakeups directed
     * to the same address.  That is, after the lock is released,
     * we must assume that a wakeup could be issued, and if we lose
     * the wakeup, we're violating a guarantee that we're trying to make.
     *
     * During the period between the lock_ReleaseShared and the time we
     * actually get queued for sleeping, we hold the mutex lock_mutex[hashVal],
     * which prevents a wakeup from being issued until we're asleep.
     */

    ospl = mutex_spinlock(&sleep_mutex[hashVal]);

    switch(atype) {
      case READ_LOCK:
        lock_ReleaseRead(alock);
        break;

      case WRITE_LOCK:
        lock_ReleaseWrite(alock);
        break;

      case SHARED_LOCK:
        lock_ReleaseShared(alock);
        break;

      default:
        panic("osi_Sleep2 locktype");
    }

    if((osi_preemptLevel) && (osi_preemptheld == osi_ThreadUnique()))
        preempting = release_preemption();
    /* sleep, and atomically drop the lock */
    sv_wait(&sleep_cond[hashVal], PZERO|PLTWAIT, &sleep_mutex[hashVal], ospl);

    if(preempting)
        acquire_preemption(preempting);
}

int
osi_SleepRI(opaque addr, osi_dlock_t *alock)
{
    long hashVal = lock_HashAddr((long)addr);
    int retval, ospl;
    int preempting = 0;


    /* this function must work atomically w.r.t. sleep_Wakeups directed
     * to the same address.  That is, after the lock is released,
     * we must assume that a wakeup could be issued, and if we lose
     * the wakeup, we're violating a guarantee that we're trying to make.
     *
     * During the period between the lock_ReleaseRead and the time we
     * actually get queued for sleeping, we hold the mutex sleep_mutex[hashVal],
     * which prevents a wakeup from being issued until we're asleep.
     */

    ospl = mutex_spinlock(&sleep_mutex[hashVal]);
    lock_ReleaseRead(alock);
    if((osi_preemptLevel) && (osi_preemptheld == osi_ThreadUnique()))    
        preempting = release_preemption();
    /* sleep, and atomically drop the lock */
    retval = sv_wait_sig(&sleep_cond[hashVal], PZERO|PLTWAIT, &sleep_mutex[hashVal], ospl);

    if(preempting)
        acquire_preemption(preempting);

    return(retval);
}

int
osi_SleepSI(opaque addr, osi_dlock_t *alock)
{
    long hashVal = lock_HashAddr((long)addr);
    int retval, ospl;
    int preempting = 0;

    /* this function must work atomically w.r.t. lock_Wakeups directed
     * to the same address.  That is, after the lock is released,
     * we must assume that a wakeup could be issued, and if we lose
     * the wakeup, we're violating a guarantee that we're trying to make.
     *
     * During the period between the lock_ReleaseShared and the time we
     * actually get queued for sleeping, we hold the mutex lock_mutex[hashVal],
     * which prevents a wakeup from being issued until we're asleep.
     */

    ospl = mutex_spinlock(&sleep_mutex[hashVal]);
    lock_ReleaseShared(alock);
    if((osi_preemptLevel) && (osi_preemptheld == osi_ThreadUnique()))
	preempting = release_preemption();
    /* sleep, and atomically drop the lock */
    retval = sv_wait_sig(&sleep_cond[hashVal], PZERO|PLTWAIT, &sleep_mutex[hashVal], ospl);

    if(preempting)
        acquire_preemption(preempting);

    return(retval);
}

int
osi_SleepWI(opaque addr, osi_dlock_t *alock)
{
    long hashVal = lock_HashAddr((long)addr);
    int retval, ospl;
    int preempting = 0;

    /* this function must work atomically w.r.t. lock_Wakeups directed
     * to the same address.  That is, after the lock is released,
     * we must assume that a wakeup could be issued, and if we lose
     * the wakeup, we're violating a guarantee that we're trying to make.
     *
     * During the period between the lock_ReleaseWrite and the time we
     * actually get queued for sleeping, we hold the mutex lock_mutex[hashVal],
     * which prevents a wakeup from being issued until we're asleep.
     */

    ospl = mutex_spinlock(&sleep_mutex[hashVal]);
    lock_ReleaseWrite(alock);
    if((osi_preemptLevel) && (osi_preemptheld == osi_ThreadUnique()))
	preempting = release_preemption();
    /* sleep, and atomically drop the lock */
    retval = sv_wait_sig(&sleep_cond[hashVal], PZERO|PLTWAIT, &sleep_mutex[hashVal], ospl);

    if(preempting)
        acquire_preemption(preempting);

    return(retval);
}

int 
osi_Sleep(opaque addr)
{
    long hashVal = lock_HashAddr((long) addr);
    int preempting = 0;
    int ospl;

    ospl = mutex_spinlock(&sleep_mutex[hashVal]);
    if((osi_preemptLevel) && (osi_preemptheld == osi_ThreadUnique()))
	preempting = release_preemption();
    sv_wait(&sleep_cond[hashVal], PZERO|PLTWAIT, &sleep_mutex[hashVal], ospl);

    if(preempting)
	acquire_preemption(preempting);

    return 0; /* normal wakeup */
}

int 
osi_SleepInterruptably(opaque addr)
{
    long hashVal = lock_HashAddr((long) addr);
    int retval, ospl;
    int preempting = 0;

    ospl = mutex_spinlock(&sleep_mutex[hashVal]);
    if((osi_preemptLevel) && (osi_preemptheld == osi_ThreadUnique()))
	preempting = release_preemption();
    retval = sv_wait_sig(&sleep_cond[hashVal], PZERO|PLTWAIT, &sleep_mutex[hashVal], ospl);

    if(preempting)
	acquire_preemption(preempting);

    return(retval);
}

int 
osi_Wakeup(opaque addr)
{
    long hashVal = lock_HashAddr((long) addr);

    sv_broadcast(&sleep_cond[hashVal]);
    return 0;
}



#if PREEMPT
static long caller_address;
static long prev_caller_address;

static int save_caller_address(inst_t *ra)
{
   prev_caller_address = caller_address;
   caller_address = (long) ra;
}

int
osi_PreemptionOff(void)
{
	int	ospl;

	ospl = mutex_spinlock(&osi_preemptLock);

	while (osi_preemptLevel) {

		/* Check recursive locking case */
		if (osi_preemptheld == osi_ThreadUnique()) {
			osi_preemptLevel++;
			save_caller_address(__return_address);
			mutex_spinunlock(&osi_preemptLock, ospl);
			return 0;
		}
                sv_wait(&osi_preemptFree, PZERO|PLTWAIT, &osi_preemptLock, ospl);
		mutex_spinlock(&osi_preemptLock);
	}

       	osi_preemptheld = osi_ThreadUnique();
        osi_preemptLevel++;
	save_caller_address(__return_address);

	mutex_spinunlock(&osi_preemptLock, ospl);

	return(osi_preemptLevel - 1);
}


void
osi_RestorePreemption(int val)
{
	int	ospl;

	ospl = mutex_spinlock(&osi_preemptLock);
	if(!osi_preemptLevel) {
		mutex_spinunlock(&osi_preemptLock, ospl);
		panic("Restoring a ghost lock?");
		return;
	}

	if(osi_preemptheld == osi_ThreadUnique()) 
			osi_preemptLevel--;
	else {
	     mutex_spinunlock(&osi_preemptLock, ospl);
	     panic("Cannot Restore lock held by someone else");
	     return;
	}
        save_caller_address(__return_address);

	sv_signal(&osi_preemptFree);
	mutex_spinunlock(&osi_preemptLock, ospl);
}

static int 
release_preemption(void)
{
	int retval, ospl;

	ospl = mutex_spinlock(&osi_preemptLock);
	retval = osi_preemptLevel;
	osi_preemptLevel = 0;
	sv_signal(&osi_preemptFree);
	mutex_spinunlock(&osi_preemptLock, ospl);
	return retval;
}

static int 
acquire_preemption(int val)
{
	int	ospl;

	spl = mutex_spinlock(&osi_preemptLock);
	/* Wait till lock is free for use */
	while (osi_preemptLevel) {
		sv_wait(&osi_preemptFree, PZERO|PLTWAIT, &osi_preemptLock, ospl);
		mutex_spinlock(&osi_preemptLock);
	}
	
	if(!osi_preemptLevel) {
		osi_preemptheld = osi_ThreadUnique();
		osi_preemptLevel = val;
	}
	mutex_spinunlock(&osi_preemptLock, ospl);
	return 0;
}

#else
int osi_PreemptionOff(void)
{
  return 0;
}

void osi_RestorePreemption(int val)
{
  return;
}

static int release_preemption(void)
{
  return 0;
}

static int acquire_preemption(int val)
{
  return 0;
}
#endif /* PREEMPT */
/* SGIMIPS specific osi_Wait */

#ifdef SGIMIPS
int osi_Wait(long ams, struct osi_WaitHandle *ahandle, int aintok)
{
    timespec_t atv;
    timespec_t remaining;
    int preempting = 0;
    int old_spl, ret = 0;

    /* First release the global lock if we had it */
    if((osi_preemptLevel) && (osi_preemptheld == osi_ThreadUnique()))
	preempting = release_preemption();

    if (ahandle) {
	old_spl = mutex_spinlock(&ahandle->mutex);
	/* If called with a wait handle then use sv_timedwait() call */
	ahandle->proc = (caddr_t) osi_ThreadUnique();
	ams = (ams * HZ) / 1000;
	tick_to_timespec((int)ams, &atv, NSEC_PER_TICK);
		
	sv_timedwait(&(ahandle->sv), PZERO|PLTWAIT, &(ahandle->mutex),
		     old_spl, 0, &atv, &remaining);
	if (ahandle->proc == (caddr_t) 0) {
	     ret = EINTR;
	}
    } else {
	/* Otherwise just sleep for a bit */
	delay((ams*HZ) / 1000);
    }

    /* Get back the global lock */
    if(preempting)
	acquire_preemption(preempting);

    return ret;
}

void osi_CancelWait( struct osi_WaitHandle *achandle)
{
    caddr_t proc;
    int ospl;

    ospl = mutex_spinlock(&achandle->mutex);
    proc = achandle->proc;
    if (proc == 0) {
	mutex_spinunlock(&achandle->mutex, ospl);
	return;
    }
    achandle->proc = (caddr_t) 0; /* so dude can figure out he was signalled */
    sv_broadcast(&achandle->sv);
    mutex_spinunlock(&achandle->mutex, ospl);
}

void osi_InitWaitHandle ( register struct osi_WaitHandle *achandle)
{
    spinlock_init(&achandle->mutex, "osi_WaitHandle");
    sv_init(&achandle->sv, SV_DEFAULT, "osi_WaitHandle");
    achandle->proc = (caddr_t) 0;
}

#endif
#endif /* KERNEL */
