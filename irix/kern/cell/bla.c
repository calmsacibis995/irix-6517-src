#ident "$Id: bla.c,v 1.8 1997/06/23 20:27:29 sp Exp $"

/*
 * This file contains code to resolve potential deadlocks that may occur if a 
 * thread goes into a long term interruptible sleep while holding a behavior
 * lock.
 *
 * If you compile with -DBLALOG, additional code is enabled that will
 * log all behavior locking chains & check the locking hierarchy for
 * potential deadlocks. Use the idbg "blalog" command to print the log.
 */

#include <sys/types.h>
#include <sys/kthread.h>
#include <ksys/behavior.h>


/* ------ BEGIN DEBUG  STUFF ZZZ -----------*/

int 		zzzranfail=0;

/* ------ END DEBUG  STUFF ZZZ -----------*/



#define MAX_STARVE_LOCKS	16		/* number of locks for 
						   starvation prevention */


/*
 * The following macros and structures are used for the starvation prevention
 * algorithms. (If we make behavior locks barrier locks, this can go 
 * away (maybe).
 * 
 * A thread that is about to sleep trying to acquire a behavior lock head for
 * update mode will:
 *	- search a small array of the bla_starve_t structures and acquire one
 *	  of the locks for update mode.
 *		- if all locks in the array are in use, no lock (obviously)
 *		  can be acquired. The threads proceed as described below but
 *		  interrupted threads will not sleep on any lock. The array
 *		  should be large enough that this seldom happens.
 *	- wake up all threads that are in interruptible sleeps that hold the 
 *	  lock being waited on. A handle to the bla_starve_t lock & it's current
 *	  <count> is passed to the thread being woken.
 *	- when the thread finally gets the lock for update, it will increment
 *	  the bla_starve_t <count> and release lock.
 *	- threads that were awaken from interruptible sleeps will unwind from 
 *	  the sleep & release the behavior head locks. After all locks are 
 *	  released, the interrupted thread will sleep til the <count> value
 *	  in the bla_starve_t structure changes. (sleeps with mraccess on the 
 *	  bla_starve_t lock).
 */

typedef struct {
	mrlock_t	lock;		/* lock set while slot is in use */
	uint_t		count;		/* count of number of times slot has 
					   been used */
} bla_starve_t;

#define STARVENULL		0
#define STARVEISNULL(arg)	((arg) == STARVENULL)
#define STARVEARG(i)		(((i) >= MAX_STARVE_LOCKS) ? STARVENULL : \
				   (((uint_t)(i)+128)<<24) | 		  \
				     (bla_starve[(i)].count&0xffffff))
#define STARVEINDEX(arg)	(((arg)>>24)&127)
#define STARVEISBUSY(arg)	((bla_starve[STARVEINDEX(arg)].count&0xffffff) \
					== ((arg)&0xffffff))



/*
 * Static variables - private to this file & not exported anywhere else
 */
STATIC lock_t		bla_gbla_mutex;		/* protects the GBLA list */
STATIC struct kthread	*bla_gbla_head;		/* head of BLA list */
STATIC bla_starve_t	bla_starve[MAX_STARVE_LOCKS];


/*
 * if BLALOG is enabled, the following structure is used to keep statistics.
 */

#ifdef BLALOG
struct {
	uint_t		push;
	uint_t		push_intr;
	uint_t		pop;
	uint_t		pop_unseq;
	uint_t		isleep;
	uint_t		isleep_locks_held;
	uint_t		isleep_lock_waiters;
	uint_t		isleep_fake_fail;
	uint_t		gbla_queue;
	uint_t		gbla_dequeue;
	uint_t		mrwait;
	uint_t		mrwait_wakeup;
	uint_t		mrwait_wakeup_skipped;
	uint_t		mrgot;
	uint_t		mrgotnull;
	uint_t		mrnostarve;
	uint_t		mrnostarve_no_backup_pc;
	uint_t		mrnostarvenull;
	uint_t		mrnostarve_sleep;
} bla_statistics;


#define STAT(x)			bla_statistics.x++
#define RWMASK			1		/* mask for read/write in rwi */
#define INTRMASK		2		/* mask for interruptin rwi */

#else /* BLALOG */
#define STAT(x)			
#endif /* BLALOG */









/*
 * Initialize the bla_starve structure. Called once early in boot.
 */
void
bla_init(void)
{
	int	i;

	private.p_blaptr = curthreadp->k_bla.kb_lockp;
	for (i=0; i<MAX_STARVE_LOCKS; i++)
		mrinit(&bla_starve[i].lock, "blastarve");
}



/*
 * INTERNAL - Add my thread's BLA to the GBLA.
 */
STATIC void
bla_gba_queue(kthread_t *kt)
{
	int	rv;

	ASSERT(!kt->k_bla.kb_in_gbla);
	rv = mutex_spinlock(&bla_gbla_mutex);
	kt->k_bla.kb_in_gbla = 1;
	STAT(gbla_queue);
	kt->k_bla.kb_blink = NULL;
	kt->k_bla.kb_flink = bla_gbla_head;
	if (kt->k_bla.kb_flink)
		kt->k_bla.kb_flink->k_bla.kb_blink = kt;
	bla_gbla_head = kt;
	mutex_spinunlock(&bla_gbla_mutex, rv);
}



/*
 * INTERNAL - Remove my thread's BLA from the GBLA.
 */
STATIC void
bla_gba_dequeue(kthread_t *kt)
{
	int	rv;

	ASSERT(kt->k_bla.kb_in_gbla);
	rv = mutex_spinlock(&bla_gbla_mutex);
	kt->k_bla.kb_in_gbla = 0;
	STAT(gbla_dequeue);

	if (kt->k_bla.kb_blink)
		kt->k_bla.kb_blink->k_bla.kb_flink = kt->k_bla.kb_flink;
	else
		bla_gbla_head = kt->k_bla.kb_flink;

	if (kt->k_bla.kb_flink)
		kt->k_bla.kb_flink->k_bla.kb_blink = kt->k_bla.kb_blink;

	mutex_spinunlock(&bla_gbla_mutex, rv);

	kt->k_bla.kb_flink = NULL;
	kt->k_bla.kb_blink = NULL;
}



/*
 * Called when a thread is about to go into an interruptible sleep.
 * If any behavior read locks are held, the thread's BLA is added to the 
 * GBLA.
 */
void
bla_isleep(int *spl)
{
	kthread_t	*kt = curthreadp;
	mrlock_t	*mrp;
	int		i, nheld;

	STAT(isleep);
	ASSERT(!kt->k_bla.kb_in_gbla);

	/*
	 * If no behavior locks are held OR if there is an interrupt already
	 * pending, nothing needs to be done.
	 */
	nheld = bla_curlocksheld();
	ASSERT(nheld >= 0 && nheld <= KB_MAX_LOCKS);
	if (nheld == 0 || thread_interrupted(kt))
		return;

	STAT(isleep_locks_held);

#ifdef ZZZRANFAIL
	{
	uint_t		ts;
	ts = get_timestamp();
	if ( (ts%100) < zzzranfail) {
		kt->k_flags |= KT_BHVINTR;
		thread_interrupt(kt, spl);
		STAT(isleep_fake_fail);
		return;
	}
	}
#endif

	/*
	 * add my thread to the GBLA. 
	 */
	bla_gba_queue(kt);

	/*
	 * Now scan the list of behavior locks held by this thread. If any locks
	 * are already being waited for, we must interrupt the current sleep. 
	 * This check must be done after inserting the BLA into the GBLA to
	 * prevent race conditions.
	 */
	for (i=0; i < nheld; i++) {
		mrp = kt->k_bla.kb_lockp[i];
#ifdef BLALOG
		ASSERT(kt->k_bla.kb_rwi[i] == 0); /* dont sleep holding WRITE locks */
#endif
		if (mrgetnwaiters(mrp)) {
			STAT(isleep_lock_waiters);
			kt->k_flags |= KT_BHVINTR;
			thread_interrupt(kt, spl);
			return;
		}
	}
}



/*
 * Called when a thread comes out of an interruptible sleep.
 * If the thread's BLA is in the GBLA, it is removed from the GBLA.
 */
void
bla_iunsleep()
{
	kthread_t	*kt = curthreadp;

	ASSERT(bla_curlocksheld()>=0 && bla_curlocksheld() <= KB_MAX_LOCKS);
	ASSERT(kt->k_bla.kb_in_gbla || bla_curlocksheld() == 0 ||
		thread_interrupted(kt));
	if (kt->k_bla.kb_in_gbla)
		bla_gba_dequeue(curthreadp);
}



/*
 * Called by mrpsema just before a thread waits for a mrlock in a 
 * behavior head. Called ONLY if the lock is being acquired for UPDATE.
 *
 * Searches the GBLA to see if any thread that is in a long term interruptible
 * sleep holds the lock. Any thread found holding the lock will be interrupted.
 *
 */
uint_t
bla_wait_for_mrlock(mrlock_t *mrp)
{
	kthread_t	*kt;
	mrlock_t	*mrp2;
	int		i, rv, s;
	uint_t		starvearg;
	int		nheld;

	STAT(mrwait);

	/*
	 * Try to acquire a bla_starve_t lock. If all are busy, (should not
	 * happen much), a NULL starevearg is generated. No wait will happen
	 * in interrupted threads but everything should work ok. If the 
	 * interrupt happens again, maybe some locks will be available then.
	 */
	for (i=0; i<MAX_STARVE_LOCKS; i++)
		if (mrtryupdate(&bla_starve[i].lock))
			break;
	starvearg = STARVEARG(i);


	/*
	 * Now interrupt any threads that hold the lock we are about to wait 
	 * for. Note that interrupted threads are in long-term interruptible 
	 * sleeps holding the lock for ACCESS & we are trying to get it 
	 * for UPDATE.
	 */
	rv = mutex_spinlock(&bla_gbla_mutex);
	for (kt = bla_gbla_head; kt; kt=kt->k_bla.kb_flink) {
		nheld = bla_klocksheld(kt);
		ASSERT(nheld >= 0 && nheld <= KB_MAX_LOCKS);
		for (i=0; i < nheld; i++) {
			mrp2 = kt->k_bla.kb_lockp[i];
			if (mrp2 == mrp) {
				s = kt_lock(kt);
				kt->k_bla.kb_starvearg = starvearg;
				if (thread_interruptible(kt)) {
					kt->k_flags |= KT_BHVINTR;
					thread_interrupt(kt, &s);
					kt_unlock(kt, s);
					STAT(mrwait_wakeup);
				} else {
					kt_unlock(kt, s);
					STAT(mrwait_wakeup_skipped);
				}
			}
		}

	}
	mutex_spinunlock(&bla_gbla_mutex, rv);

	/*
	 * Return starvearg to the caller. It will be passed back when the
	 * behavior lock is finally obtained.
	 */
	return(starvearg);
}




/*
 * Called by mrpsema after it acquires a behavior lock that it had
 * to wait for.
 */
void
bla_got_mrlock(uint_t arg)
{
	int	i;

	/*
	 * If a bla_starve_t lock was obtained, advance its count & release
	 * the lock. This allows any interrupted threads to proceed from the
	 * signal handler & reacquire the behavior locks they held in the 
	 * long-term sleep.
	 */
	STAT(mrgot);
	if (! STARVEISNULL(arg)) {
		i = STARVEINDEX(arg);
		ASSERT(i <= MAX_STARVE_LOCKS);
		bla_starve[i].count++;
		mrunlock(&bla_starve[i].lock);
	} else {
		STAT(mrgotnull);
	}

}



/*
 * Called by psig when it detects that the current thread was woken from 
 * an interruptible sleep because it held a BHV lock that someone 
 * wanted for update mode.
 * This function will delay the current thread until the thread attempting
 * to acquire the behavior lock for update actually gets it.
 */
void
bla_prevent_starvation(int backup_pc) /* backup pc for debug only */
{
	kthread_t	*kt = curthreadp;
	int		i;
	uint_t		arg; 

	STAT(mrnostarve);
	if (!backup_pc)
		STAT(mrnostarve_no_backup_pc);

	/*
	 * If a bla_starve_t lock was assigned, acquire the bla_starve_t lock
	 * for ACCESS. The thread that woke up up acquired the bla_starve_t for
	 * UPDATE before waking us up. When it gets the behavior lock that we
	 * use to hold, it will release the bla_starve_t lock & allow us to 
	 * proceed.  If the bla_starve_t lock has already been released &
	 * is being used for an unrelated behavior lock, the STARVEISBUSY 
	 * check will usually prevent us from waiting on the wrong event.
	 */
	arg = kt->k_bla.kb_starvearg;
	if (! STARVEISNULL(arg)) {
		i = STARVEINDEX(arg);
		ASSERT(i <= MAX_STARVE_LOCKS);
		while (STARVEISBUSY(arg)) {
			STAT(mrnostarve_sleep);
			mraccess(&bla_starve[i].lock);
			mrunlock(&bla_starve[i].lock);
		}
	} else {
		STAT(mrnostarvenull);
	}
	kt->k_bla.kb_starvearg = STARVENULL;
}



#ifdef BLALOG

/*
 * The following is used if BLALOG is enabled to collect statistics about
 * behavior lock usage & to provide additional debugging help.
 *
 * The code also creates a global log of unique behavior lock chains. 
 * This log can be printed using idbg commands. There are also debugging 
 * routines to scan the log for potential deadlocks in the order in which 
 * behavior locks are acquired.
 */


#include <sys/debug.h>
#include <sys/idbgentry.h>

#define BHV_NAME_SIZE		16		/* max size of BHV head name */
#define BHV_MAX_NAMES		20		/* max unique BHV names */
#define MAX_LOG_ENTRIES		400		/* max log entries to keep */
#define MAX_RECURSION		100		/* max recursion in deadlock 
						   detection algorithm */

#define MATCH(idp1,idp2)	((idp1)->lid_name == (idp2)->lid_name &&	\
				 (idp1)->lid_rwi == (idp2)->lid_rwi &&		\
				 (idp1)->lid_recursive == (idp2)->lid_recursive)

#define CONFLICT(idp1,idp2)	((idp1)->lid_name == (idp2)->lid_name &&	\
				 (((idp1)->lid_rwi|(idp2)->lid_rwi)&RWMASK))

#define LOWTOUP(c)		((char) ((int)(c) - (int)'a' + (int)'A'))
#define ISLOWER(c)		((c) >= 'a' && (c) <= 'z')

typedef char bla_name_t[BHV_NAME_SIZE];

typedef struct {
	uchar_t		lid_rwi;
	uchar_t		lid_recursive;
	uchar_t		lid_name;
	void		*lid_caller;
} bla_lock_id;

typedef struct {
	uint_t		lchecksum;
	uint_t		lcount;
	uchar_t		lsubset;
	bla_lock_id	lockid[KB_MAX_LOCKS+1];
} bla_log_entry_t;




/*
 * Forward/external procedure declarations
 */
extern void	_prsymoff(void *, char *, char *);
STATIC void	bla_log_entry(kthread_t *kt);
STATIC int	bla_check_for_entry(bla_log_entry_t *lp);
STATIC int	bla_check_for_subset(int, bla_log_entry_t *lp);
STATIC int	bla_check_deadlock (int index);
void		bla_deadlock(void);
STATIC int	bla_check_deadlock2(int nesting, bla_lock_id *starting_idp[], 
			bla_lock_id *id);
STATIC void	bla_print_log_entry (int index, int syms, 
			bla_lock_id *highlite1, bla_lock_id *highlite2);




int			bla_debugnow=0;		/* set to 1 to breakpoint on
						   deadlocks when they occur */
lock_t			bla_log_mutex;		/* Mutex to protect log*/
int			bla_log_next_index=0;	/* Number of entries in log */

bla_name_t		bla_name[BHV_MAX_NAMES];/* Array of BHV lock names */
int			bla_namex=1;		/* index to next entry in bla_name 
						   (entry 0 is used for NULL */

bla_log_entry_t		bla_log[MAX_LOG_ENTRIES];/* Log of behavior log 
						  locking chains */

extern mrlock_t		*dummy_bla[];


/*
 * Called by BHV_READ_LOCK, BHV_WRITE_LOCK  & BHV_WRITE_TO_READ after obtaining
 * a behavior lock.  Additional info about the lock is saved. Also provides a
 * lot of ASSERTs to make sure everything is working ok.
 *
 * If the lock was obtained for update, add an entry to the list of behavior
 * locks currently held by the current thread. (lock routines only log 
 * read-locks).
 */
void
bla_push(mrlock_t *mrp, int rwi)
{
	kthread_t	*kt = curthreadp;
	mrlock_t	**lockpp;
	int		nheld;

	/* 
	 * skip ASSERTs if idbg running - they'll fail 
	 */
	if (private.p_blaptr >= dummy_bla && 
			private.p_blaptr < &dummy_bla[KB_MAX_LOCKS])
		return;

	STAT(push);
	if (rwi) {
		lockpp = private.p_blaptr++;
		*lockpp = mrp;
	}

	nheld = bla_curlocksheld();
	ASSERT(nheld == 0 || kt->k_bla.kb_lockp[0] != NULL);
	ASSERT(nheld > 0 && nheld <= KB_MAX_LOCKS);
	ASSERT(kt->k_bla.kb_lockp[nheld-1] == mrp);
	kt->k_bla.kb_growing = 1;
	if (private.p_kstackflag == PDA_CURINTSTK) {
		rwi |= INTRMASK;
		STAT(push_intr);
	}
	kt->k_bla.kb_rwi[nheld-1] = rwi;
	kt->k_bla.kb_pc[nheld-1] = __return_address;

}



/*
 * Called by BHV_READ_UNLOCK & BHV_WRITE_UNLOCK before they release a behavior
 * lock.  
 *
 * If the lock was held for update, remove the entry from the list of behavior
 * locks held by the current thread. 
 *
 * If the chain of locks held is starting to shrink (ie., at a local max size),
 * log the behavior lock chain if its not already in the log.
 */
void
bla_pop(mrlock_t *mrp, int rwi)
{
	kthread_t	*kt = curthreadp;
        int		i, nheld;

	/* 
	 * skip ASSERTs if idbg running - they'll fail 
	 */
	if (private.p_blaptr >= dummy_bla && 
			private.p_blaptr < &dummy_bla[KB_MAX_LOCKS])
		return;

	STAT(pop);

	if (private.p_kstackflag == PDA_CURINTSTK)
		rwi |= INTRMASK;

	nheld = bla_curlocksheld();
	ASSERT(nheld >= 1 && nheld <= KB_MAX_LOCKS);
	ASSERT(nheld == 0 || kt->k_bla.kb_lockp[0] != NULL);

	for (i=nheld-1; i>=0; i--)
		if (kt->k_bla.kb_lockp[i] == mrp)
			break;

	if (i != nheld-1)
		STAT(pop_unseq);

	ASSERT(i>=0);
	ASSERT(kt->k_bla.kb_rwi[i] == rwi);

	if (kt->k_bla.kb_growing)
		bla_log_entry(kt);
	kt->k_bla.kb_growing = 0;

	for (; i<nheld-1; i++) {
		if (rwi&RWMASK)
			kt->k_bla.kb_lockp[i] = kt->k_bla.kb_lockp[i+1];
		kt->k_bla.kb_rwi[i] = kt->k_bla.kb_rwi[i+1];
		kt->k_bla.kb_pc[i] = kt->k_bla.kb_pc[i+1];

	}
	if (rwi&RWMASK)
		private.p_blaptr--;
	ASSERT(nheld == 0 || kt->k_bla.kb_lockp[0] != NULL);

}


/*
 * Convert a mrlock name into an index into the bla_name array.
 */
STATIC uchar_t
bla_findname (char *name)
{
	int	i;

	for (i=1; i< bla_namex; i++)
		if (strcmp(name, bla_name[i]) == 0)
			return(i);

	ASSERT(bla_namex < BHV_MAX_NAMES);

	strcpy(bla_name[bla_namex++], name);
	return(bla_namex);
}


/*
 * Add an entry to the global log of unique behavior lock chains if 
 * not already there.
 *
 * THIS IS FOR DEBUGGING ONLY 
 */

STATIC void
bla_log_entry(kthread_t *kt)
{
	bla_log_entry_t		ent;
	int			entries, index, new_index=-1, i, j, rv;
	uint64_t		checksum=0;
	mrlock_t		*mrp;

	if (bla_log_next_index >= MAX_LOG_ENTRIES)
		return;

	entries = bla_curlocksheld();
	if (entries < 2) 		/* ignore chains of length 1 */
		return;

	for (i=0; i < entries; i++) {
		mrp = kt->k_bla.kb_lockp[i];
		ent.lockid[i].lid_rwi = kt->k_bla.kb_rwi[i];
		ent.lockid[i].lid_caller = kt->k_bla.kb_pc[i];
		for (j=0; j<i; j++)
			if (kt->k_bla.kb_lockp[j] == 
					kt->k_bla.kb_lockp[i])
				break;
		ent.lockid[i].lid_recursive = (i && i != j);
		ent.lockid[i].lid_name = bla_findname(wchanname(mrp, KT_WMRLOCK));
		checksum = (checksum<<4) ^ ent.lockid[i].lid_name ^ ent.lockid[i].lid_rwi;
	}

	ent.lockid[i].lid_name = 0;
	ent.lchecksum = checksum;
	ent.lcount = 1;
	ent.lsubset = 0;
	if ( (index=bla_check_for_entry(&ent)) >= 0) {
		bla_log[index].lcount++;
		return;
	}

	rv = mutex_spinlock(&bla_log_mutex);
	if (bla_log_next_index < MAX_LOG_ENTRIES && bla_check_for_entry(&ent) < 0){
		bla_log[bla_log_next_index] = ent;
		new_index = bla_log_next_index++;
	}
	mutex_spinunlock(&bla_log_mutex, rv);
	
	if (bla_debugnow) {
		if (new_index >= 0 && bla_check_deadlock (new_index))
			bla_deadlock();
	}
}



/*
 * Check to see if an entry is in the global log of unique behavior lock chains
 * Returns:
 *       -1    not in log
 *      >=0    index of log entry in log
 */
STATIC int
bla_check_for_entry(bla_log_entry_t *new_entry_p)
{
	bla_log_entry_t		*entry;
	bla_lock_id		*idp1, *idp2;
	int			i;

	for (i=0, entry=&bla_log[0]; i<bla_log_next_index; entry++, i++) {
		if (entry->lchecksum != new_entry_p->lchecksum)
			continue;
		for (idp1=entry->lockid, idp2=new_entry_p->lockid; 
				idp1->lid_name && idp2->lid_name;
				idp1++, idp2++)
			if (!MATCH(idp1, idp2))
				break;

		if (idp1->lid_name == 0 && idp2->lid_name == 0)
			return(i);

	}
	return(-1);
}


/*
 * Check to see if an entry is a subset of another entry in the
 * log. No need to check subset-entries to see if the are part 
 * of a deadlock chain.
 * Returns:
 *	0   not a subset
 *	1   subset of another log entry
 */
STATIC int
bla_check_for_subset(int xlogi, bla_log_entry_t *xep)
{
	bla_log_entry_t		*ep;
	bla_lock_id		*xidp, *idp;
	int			logi, ret=0;

	for (logi=0, ep=bla_log; logi<bla_log_next_index; ep++, logi++) {
		if (xep == ep)
			continue;

		for (xidp=xep->lockid, idp=ep->lockid; 
				xidp->lid_name && idp->lid_name; 
				idp++)
			if (MATCH(xidp, idp))
				xidp++;

		if (xidp->lid_name == 0) {
			if (!ret) {
				ret = 1;
				qprintf("  %d subset of ", xlogi);
			}
			qprintf(" %d", logi);
		}
		
	}
	if (ret)
		qprintf("\n");
	return(ret);
}


/*
 * Check a log entry to see if it is part of a potential deadlock chain.
 * Returns:
 *	 0 - no potential deadlocks detected
 *	>0 - one or more potential deadlocks detected
 */
STATIC int
bla_check_deadlock (int index)
{
	bla_lock_id		*idp;
	bla_lock_id		*term_idp[MAX_RECURSION+2];
	int			rv=0;
	
	for (idp=bla_log[index].lockid; idp->lid_name; idp++) {
		term_idp[0] = idp;
		term_idp[1] = NULL;
		if (!idp->lid_recursive)
			rv += bla_check_deadlock2 (0, term_idp, idp);
	}
	return (rv);
}



STATIC int 
bla_check_deadlock2 (int nesting, bla_lock_id *term_idp[], bla_lock_id *sidp)
{
	int			logi;
	bla_lock_id		*bidp, *idp, *idp2, **idpp;
	
	for (logi=0; logi<bla_log_next_index; logi++) {
		if (bla_log[logi].lsubset)
			continue;
		for (idp=bla_log[logi].lockid; idp->lid_name; idp++) {
			if (!idp->lid_recursive && idp != sidp && 
					CONFLICT(sidp, idp)) {
				idp2 = idp;
				bidp = bla_log[logi].lockid;
				while (idp != bidp) {
					idp--;					
					if (idp->lid_recursive)
						continue;	
					if (idp == term_idp[0]) {
						qprintf ("\nPotential behavior lock deadlock detected.......\n");
						bla_print_log_entry(logi, 0, idp, idp2);
						return(1);
					}
					for (idpp=&term_idp[1]; *idpp; idpp++)
						if (idp == *idpp)
							return(0);
					ASSERT(nesting < MAX_RECURSION);
					*idpp = idp;
					*(idpp+1) = NULL;
					if (bla_check_deadlock2 (nesting+1, term_idp, idp)) {
						bla_print_log_entry(logi, 0, idp, idp2);
						return(1);
					}
					*idpp = NULL;
				}
				break;
			}
		}
	}
	return(0);
}

/*
 * Print the entire behavior lock log. Called from idbg command (blalog).
 */
void
bla_print_log ()
{
	int		logi;
	
	for (logi=0; logi<bla_log_next_index; logi++)
		bla_print_log_entry(logi, 0, NULL, NULL);
		
	qprintf("\n");
	for (logi=0; logi<bla_log_next_index; logi++)
		bla_print_log_entry(logi, 1, NULL, NULL);
		
	qprintf("\n");
	for (logi=0; logi<bla_log_next_index; logi++)
		if (bla_check_for_subset(logi, &bla_log[logi]))
			bla_log[logi].lsubset = 1;	

	qprintf("\n");
	for (logi=0; logi<bla_log_next_index; logi++)
		if (!bla_log[logi].lsubset)
			bla_check_deadlock (logi);
		
	qprintf("\nBLA Statistics....\n");
	qprintf ("%10d push lock\n", bla_statistics.push);
	qprintf ("%10d   push intr\n", bla_statistics.push_intr);
	qprintf ("%10d pop lock\n", bla_statistics.pop);
	qprintf ("%10d   pop not LIFO\n", bla_statistics.pop_unseq);
	qprintf ("%10d interruptible sleep\n", bla_statistics.isleep);
	qprintf ("%10d   held BHV locks\n", bla_statistics.isleep_locks_held);
	qprintf ("%10d   waiters for BHV locks\n", bla_statistics.isleep_lock_waiters);
	qprintf ("%10d   faked waiters for BHV locks\n", bla_statistics.isleep_fake_fail);
	qprintf ("%10d mrwait called\n", bla_statistics.mrwait);
	qprintf ("%10d   woke up thread\n", bla_statistics.mrwait_wakeup);
	qprintf ("%10d   woke skipped - lost race\n", bla_statistics.mrwait_wakeup_skipped);
	qprintf ("%10d mrgot  called\n", bla_statistics.mrgot);
	qprintf ("%10d   null arg passed\n", bla_statistics.mrgotnull);
	qprintf ("%10d mrnostarve called\n", bla_statistics.mrnostarve);
	qprintf ("%10d   no backup pc\n", bla_statistics.mrnostarve_no_backup_pc);
	qprintf ("%10d   null arg passed \n", bla_statistics.mrnostarvenull);
	qprintf ("%10d   mrnostarve slept\n", bla_statistics.mrnostarve_sleep);
	qprintf ("%10d GBLA current size\n", bla_statistics.gbla_queue-
			bla_statistics.gbla_dequeue);
}



/*
 * Print one entry in the behavior lock log.
 */
STATIC void
bla_print_log_entry (int logi, int sym, bla_lock_id *highlite1, 
		bla_lock_id *highlite2)
{
	bla_lock_id	*idp;
	char		*p, name[BHV_NAME_SIZE];
	int		i;
	
	qprintf ("%3d: %7d  |", logi, bla_log[logi].lcount);
	for (i=0, idp = bla_log[logi].lockid; idp->lid_name; i++, idp++) {
		if (i)
			qprintf ("->");
		if ( (idp->lid_rwi&INTRMASK) && 
				(i==0 || !((idp-1)->lid_rwi&INTRMASK)))
			qprintf ("INTERRUPT->");
		if (idp->lid_recursive)
			qprintf ("(R)");
		if (idp == highlite1 || idp == highlite2)
			qprintf ("[");
		strcpy (name, bla_name[idp->lid_name]);
		if (idp->lid_rwi&RWMASK)
			for(p=name; *p; p++)
				*p = ISLOWER(*p) ? LOWTOUP(*p) : *p;
		if (sym)
			_prsymoff((void *)idp->lid_caller, NULL, NULL);
		else
			qprintf("%s", name);
		if (idp == highlite1 || idp == highlite2)
			qprintf ("]");
	}
	qprintf("\n");

}



/*
 * Set breakpoint here to see deadlocks as detected
 * 	must set bla_debugnow=1
 */
void
bla_deadlock() {
}

#endif /* BLALOG */
