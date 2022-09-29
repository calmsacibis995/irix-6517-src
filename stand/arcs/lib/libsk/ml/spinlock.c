
#ident	"lib/libsk/ml/spinlock.c:  $Revision: 1.19 $"

#include <sys/types.h>
#include <sys/systm.h>
#include <libsc.h>
#include <libsk.h>

#if EVEREST
#include <sys/EVEREST/everest.h>
#ifndef LARGE_CPU_COUNT_EVEREST
#define MAXCPU EV_MAX_CPUS
#endif
#define MULTIPROCESSOR 1
#endif /* EVEREST */

#if SN0


#ifndef MAXCPU
#define MAXCPU MAXCPUS
#endif

#define MULTIPROCESSOR 1
#endif /* SN0 */

#if IP30
#include <sys/RACER/IP30.h>
#define	MULTIPROCESSOR	1
#endif	/* IP30 */

extern void spunlockspl(lock_t, int);

/*
 * All-software version of spin locks.  On Everest, there's
 * no special hardware support for locks.  The R4000 ll/sc
 * instructions only work cached, so they don't help programs
 * that may run uncached.  This pure software version of
 * spin locks should work under any circumstances.
 *
 * NOTE that these algorithms will NOT work to synchronize with
 * interrupt code.  It's assumed that interrupts are all blocked
 * when in standalone code.
 *
 * We preserve the notion of a lock_t as a single integer in
 * order to avoid confusion with kernel code and headers.
 *
 * These interfaces might belong as pvector entry points!
 */

#if MULTIPROCESSOR
typedef enum {S_FREE, S_WANTED, S_TAKEN} lock_state_t;

typedef struct salock {
	lock_state_t	l_lock_state[MAXCPU];	/* state of lock */
	int		l_turn;			/* who gets it next */
	int		l_flags;
#define SL_ALLOCATED	0x01
} salock_t;

/* 
 * We only really need a handful of locks for standalone, so
 * we'll just statically allocate them here.
 */
#define MAX_SALOCK 50 
salock_t salocks[MAX_SALOCK];

lock_t monitor_lock;

/*
 * Initialize spin lock subsystem.
 */
void
initsplocks(void)
{
	int i;
	salock_t	*samonitor_lock = &salocks[0];

	bzero(salocks, sizeof(salock_t) * MAX_SALOCK);

	for (i=0; i<MAXCPU; i++)
		samonitor_lock->l_lock_state[i] = S_FREE;
	samonitor_lock->l_turn = 0;
	samonitor_lock->l_flags = SL_ALLOCATED;
	monitor_lock = (lock_t)0;
	wbflush();
}

/*
 * Initialize a lock.
 */
void
initlock(lock_t *lock)
{
	int i;
	int ospl; 
	salock_t *salock;

	ospl = splockspl(monitor_lock, splhi);

	for (i=0; i<MAX_SALOCK; i++)
		if (salocks[i].l_flags != SL_ALLOCATED)
			break;

	if (i == MAX_SALOCK)
		panic("out of standalone spin locks");

	salock = &salocks[i];
	*lock = (lock_t)i;

	for (i=0; i<MAXCPU; i++)
		salock->l_lock_state[i] = S_FREE;
	salock->l_turn = 0;
	salock->l_flags |= SL_ALLOCATED;

	spunlockspl(monitor_lock, ospl);
}
/*
 * Free a spinlock 
 */
void
freesplock(lock_t *lock)
{
	int		ospl;
	salock_t	*salock;

	ospl = splockspl(monitor_lock, splhi);

	salock = &salocks[(int)*lock];

	salock->l_flags &= ~SL_ALLOCATED;
	salock->l_turn   = 0;

	spunlockspl(monitor_lock, ospl);
	*lock = (lock_t) MAX_SALOCK;

	return;
}

/*
 * For compatibility with kernel -- initialize a lock with a name
 */
/* ARGSUSED */
void
initnlock(lock_t *lock, char *name)
{
	initlock(lock);
}

/*
 * Lock a spin lock.
 *
 * Interrupts are all blocked when in standalone code, but we
 * preserve the kernel interface by passing the splr parameter.
 */
/* ARGSUSED */
int
splockspl(lock_t lock, int (*splr)(void))
{
	int id = cpuid();
	salock_t *salock = &salocks[(int)lock];
	int i;

tryagain:
	salock->l_lock_state[id] = S_WANTED;
	wbflush();

	while (salock->l_turn != id) {
		if (salock->l_lock_state[salock->l_turn] == S_FREE) {
			salock->l_turn = id;
			wbflush();
		}
	}

	salock->l_lock_state[id] = S_TAKEN;
	wbflush();

	for (i=0; i<MAXCPU; i++) {
		if ((i != id) && (salock->l_lock_state[i] == S_TAKEN))
			goto tryagain;
	}

	/* We have the lock */
	return(0);
}

/*
 * Unlock a spin lock
 *
 * Interrupts are all blocked when in standalone code, but we
 * preserve the kernel interface by passing the ospl parameter.
 */
/* ARGSUSED */
void
spunlockspl(lock_t lock, int ospl)
{
	int id = cpuid();
	salock_t *salock = &salocks[(int)lock];

	salock->l_lock_state[id] = S_FREE;
	wbflush();
}

/* Steal a spin lock
 *
 * Intended primarily for use by symmon after receiving an NMI interrupt.
 * Used in the case where one CPU has the UI locked and it does not respond
 * to an NMI.  Other cpus may timeout and attempt to steal the UI, but
 * for this to work we need to steal the console duart lock if it is held.
 *
 * NOTE: owner_cpuid is specified as an additional safeguard (we could just
 *	 imply it from salock->l_turn) and is the CPU which we intend to
 *	 steal the lock from.
 *
 * return 0 if lock was not held by the indicated cpu.
 */
int
spsteallock(lock_t lock, int owner_cpuid)
{
	salock_t *salock = &salocks[(int)lock];

	if (salock->l_lock_state[owner_cpuid] == S_TAKEN) {

		salock->l_lock_state[owner_cpuid] = S_FREE;
		wbflush();
		return 1;
	} else
		return 0;
}
#else /* !MULTIPROCESSOR */
/*
 * Uniprocessor code won't count on spin locks for any synchronization,
 * but we'll go ahead and do the spl since these routines might get called
 * to do funny things to the status register.
 */
/* ARGSUSED */
void
initlock(lock_t *lock)
{
}

/* ARGSUSED */
void
initnlock(lock_t *lock, char *name)
{
}

/* ARGSUSED */
void
freesplock(lock_t *lock)
{
}

/* ARGSUSED */
int
splockspl(lock_t lock, int (*splr)(void))
{
	int ospl;

	ospl = splr();

	return(ospl);
}

/* ARGSUSED */
void
spunlockspl(lock_t lock, int ospl)
{
	splx(ospl);
}
#endif /* !MULTIPROCESSOR */
