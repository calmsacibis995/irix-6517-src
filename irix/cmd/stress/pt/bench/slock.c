/*
 * Purpose: correctness/performance testing for simple locks. We cheat
 * and use some of the symbols that libpthread exports to do locking on 
 * reentrant routines.
 * 
 * Basically, we start a low priority thread, and several high-priority
 * threads. The high-priority threads hit a barrier immediately; the 
 * low-prio ones first acquire a simple lock, then hit the barrier. If
 * slocks are functioning properly, the low-priority thread should not
 * be completely starved out by the busy-waiting hi-prio threads.
 * 
 * This test needs to be run as root.
 * 
 * This was originally written as a test case for pv 621089.
 */

#include <mutex.h>
#include <sched.h>
#include <sys/mman.h>
#include <assert.h>
#include <unistd.h>
#include "sthr.h"
#include "hrn.h"

HRN_DECLARE_LIBCAP(HRN_LIBCAP_ALL);

/* command-line tweakables */
static int nrealtime = 4;
static int thr_scope = PTHREAD_SCOPE_SYSTEM;

/* global counter */
static int ctr = 0;

static void setup_rt(void);
static void* hipri_main(void*);
static void* lopri_main(void*);

/* 
 * We're hijacking the simple locks out of libc's rand; N.B. that we can't
 * use rand in this test.
 */
extern void __libc_lockrand(void);
extern void __libc_unlockrand(void);
#define SLOCK_ACQUIRE __libc_lockrand
#define SLOCK_RELEASE __libc_unlockrand

/*
 * Start our threads at high priority.
 */
int hrn_main(hrn_args_t* args)
{
	int 		ii;
	sthr_barrier_t 	bar;
	struct sched_param sched;
	int 		minprio;
	pthread_attr_t	attr;
	pthread_t* 	threads = NEWV(pthread_t, nrealtime+1);

	/* initialize real-time  */
	setup_rt();
	ChkInt((minprio = sched_get_priority_min(SCHED_FIFO)), >=0);
	sched.sched_priority = minprio + 2;

	/* set up thread attributes */
	ChkInt(pthread_attr_init(&attr), ==0);
	ChkInt(pthread_attr_setscope(&attr, thr_scope), ==0);	
	ChkInt(pthread_attr_setschedpolicy(&attr, SCHED_FIFO), ==0);

	sthr_barrier_init(&bar, nrealtime+1);

	/* make the lopri thread */
	ChkInt(pthread_attr_setschedparam(&attr, &sched), ==0);
	trc_info("creating lopri\n");
	ChkInt(pthread_create(&threads[0], &attr, lopri_main, &bar), ==0);

	/* make the hipri threads */
	for (ii = 1; ii < nrealtime+1; ii++) {
		trc_info("creating hipri %d\n", ii-1);
		ChkInt(pthread_create(threads+ii, &attr, hipri_main, &bar),==0);
	}

	for (ii=0; ii < nrealtime+1; ii++) {
		trc_info("joining %d'th thread\n", ii);
		ChkInt(pthread_join(threads[ii], 0), ==0);
	}

	pthread_attr_destroy(&attr);
	sthr_barrier_destroy(&bar);
	return 0;
}


/*
 * Increment a shared counter for a while. This should be done with the lock
 * held, to provide correctness testing.
 */
static void bump_ctr(void)
{
	int 	ii; 
	int 	cpy = ctr;

	for (ii=0; ii < 1000; ii++) {
		sched_yield();
		ctr++;
	}

	if (ctr != cpy + 1000) {
		trc_print(STDERR_FILENO, "ctr should be %d, was %d!\n",
			  cpy + 1000, ctr);
		abort();
	}
}

/*
 * lopri_main: acquire the lock, then lower our own priority before releasing
 * the lock. A stupid thing to do in practice, but slocks should still do the 
 * right thing: hi-pri threads attempting to obtain the lock should not
 * starve us out while they busy-wait for the lock.
 */
static void* lopri_main(void* arg)
{
	sthr_barrier_t* bar = (sthr_barrier_t*) arg;
	struct 		timespec msec = { 0, 1000*1000 };
	int 		minprio = sched_get_priority_min(SCHED_FIFO);
	struct 		sched_param sched;
	int 		garbage;

	pthread_getschedparam(pthread_self(), &garbage, &sched);

	START_TIMER;
	SLOCK_ACQUIRE();
	sthr_barrier_join(bar);

	sched.sched_priority --;
	pthread_setschedparam(pthread_self(), SCHED_FIFO, &sched);
	sginap(200);
	bump_ctr();

	SLOCK_RELEASE();
	END_TIMER("lopri");
	return 0;
}

/*
 * hipri_main: wait for everyone to hit the barrier, then try to acquire
 * the slock.
 */
static void* hipri_main(void* arg)
{
	sthr_barrier_t*	bar = (sthr_barrier_t*) arg;

	sthr_barrier_join(bar);
	START_TIMER;

	SLOCK_ACQUIRE();
	bump_ctr();
	SLOCK_RELEASE();

	END_TIMER("critical section");
	return 0;
}

/*
 * Initialize realtime 
 */
static void setup_rt(void)
{
	int 			minprio;
	struct sched_param 	scpm;

	ChkInt ((minprio = sched_get_priority_min(SCHED_FIFO)), == 0);

	scpm.sched_priority = minprio + 2;
	ChkInt (sched_setscheduler(0, SCHED_FIFO, &scpm),  != -1);
	thr_scope = PTHREAD_SCOPE_SYSTEM;
}

/*
 * command-line options
 */
DEF_PARSER(p_nrealtime, nrealtime = atoi(arg))
hrn_option_t hrn_options[]= {
	{ "P:", "Set number of high-priority threads\n",
		"",
		p_nrealtime },
	{ 0, 0, 0, 0 }
};

hrn_doc_t hrn_doc = {
"slock",

"Simple-lock benchmark.",

"hipri_threads:\n"
"	barrier_join(&global_bar);\n"
"	slock_acquire(&global_slock);\n"
"	slock_release(&global_slock);\n"
"\n"
"lopri_thread:\n"
"	slock_acquire(&global_slock);\n"
"	barrier_join(&global_bar);\n"
"	lower priority;\n"
"	slock_release(&global_slock);\n"
"\n",

"Time required for each thread to complete\n"
};

