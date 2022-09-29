/*
 * Purpose: measure the performance of a mutex under varying contention.
 * All threads spend a varying amount of time attempting to lock a single
 * mutex. The contention ratio specifies how "hot" the mutex is.
 * 
 * Optionally, some of the threads can run at higher priorities than
 * others.  
 */

#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>

#include "trc.h"
#include "hrn.h"
#include "sthr.h"

HRN_DECLARE_LIBCAP(HRN_LIBCAP_PTHREAD);

pthread_t *threads;
pthread_mutex_t mtx;

static void* 	spin (void* arg);

/* command-line tweakables */
static double 	ratio = 1.0;
static int 	niters = 100;
static int	trylock;
static int	nrealtime;		/* number of real-time threads */
static int	rtprio = 2;		/* real-time priority */
static int	spincnt = 10000;	/* spinning done in critical section */
static int	record_latency;		/* do latency measurements */

/* mutex attributes */
static int 	mtx_protocol = PTHREAD_PRIO_NONE;
static int	mtx_type = PTHREAD_MUTEX_DEFAULT;
static int	mtx_pshared = PTHREAD_PROCESS_PRIVATE; 

/* thread attributes */
static int	thr_scope = PTHREAD_SCOPE_PROCESS;

static void spend_time(void);
static void* thread_body (void* arg);
static void setup_rt(void);

/*
 * We create several threads, each of which bangs on a global mutex.
 */
int hrn_main(hrn_args_t* args)
{
	int 			ii, jj;
	pthread_mutexattr_t 	mtx_attr;
	pthread_attr_t		attr;
	struct sched_param 	scpm;
	int 			minprio;

	threads = NEWV(pthread_t, args->a_nthreads + nrealtime);

	if (nrealtime)
		setup_rt();

	/* initialize mutex */
	ChkInt(pthread_mutexattr_init(&mtx_attr), == 0);
	ChkInt(pthread_mutexattr_setpshared(&mtx_attr, mtx_pshared), == 0);
	ChkInt(pthread_mutexattr_setprotocol(&mtx_attr, mtx_protocol), == 0);
	ChkInt(pthread_mutexattr_settype(&mtx_attr, mtx_type), == 0);
	ChkInt(pthread_mutex_init(&mtx, &mtx_attr), == 0);

	/* initialize thread attributes */
	ChkInt(pthread_attr_init(&attr), == 0);
	ChkInt(pthread_attr_setscope(&attr, thr_scope), == 0);
	if (nrealtime) {
		setup_rt();
		ChkInt((minprio = sched_get_priority_min(SCHED_FIFO)), >=0);
		/* rtprio-1 for low priority threads */
		scpm.sched_priority = minprio + rtprio-1;
		ChkInt(pthread_attr_setschedparam(&attr, &scpm), ==0);
		ChkInt(pthread_attr_setschedpolicy(&attr, SCHED_FIFO), ==0);
	}

	START_TIMER;
	/* start lo-pri threads first */
	for (ii = 0; ii < args->a_nthreads; ii++) {
		trc_info("creating %d'th thread...\n", ii);
		ChkInt(pthread_create(&threads[ii], &attr, thread_body, args), 
	       	       ==0);
		trc_info("created.\n");
	}


	/* now start the high-priority threads */
	if (nrealtime) {
		scpm.sched_priority = minprio + rtprio;
		ChkInt(pthread_attr_setschedparam(&attr, &scpm), ==0);
	}
	for (jj = 0; jj < nrealtime; jj++, ii++) {
		trc_info("creating %d'th real-time thread...\n", jj);
		ChkInt(pthread_create(&threads[ii], &attr, thread_body, args), 
	       	       ==0);
		trc_info("created.\n");
	}

	for (ii = 0; ii < args->a_nthreads + nrealtime; ii++) {
		trc_info("joining %d'th thread...\n", ii);
		ChkInt( pthread_join(threads[ii], 0), == 0);
		trc_info("join'ed.\n");
	}
	END_TIMER("total");

	pthread_mutexattr_destroy(&mtx_attr);
	pthread_attr_destroy(&attr);
	pthread_mutex_destroy(&mtx);
	free(threads);
	return 0;
}

/*
 * Let some busy time pass while holding the mutex
 */
static void contended(void* vmtx)
{
	register pthread_mutex_t* mtx = (pthread_mutex_t*) vmtx;
	trc_vrb("contended\n");

	if (record_latency) {
		START_TIMER;
		pthread_mutex_lock(mtx);
		END_TIMER("mutex_latency");
	} else {
		pthread_mutex_lock(mtx);
	}

	spend_time();
	pthread_mutex_unlock(mtx);
}

/*
 * Let some busy time pass, presumably without holding the mutex.
 */
static void uncontended(void* mtx)
{
	spend_time();
}

static void* thread_body (void* varg)
{
	START_TIMER;
	hrn_ratio_execute(ratio, niters, contended, &mtx, uncontended, &mtx);
	END_TIMER("single");

	return 0;
}

/*
 * This function spends a "large" amount of time; hopefully enough to dwarf the
 * time required to acquire a free mutex.
 */
static void spend_time(void)
{
	int ii;

	trc_vrb("time passes...\n");

	/* compute */
	for (ii = 0; ii < spincnt; ii++) {
		if (! ( ii % 100) )
			trc_vrb("%d\n", ii);
	}
}

/*
 * Initialize realtime stuff
 */
static void setup_rt(void)
{
	int minprio;
	struct sched_param scpm;

	CHKInt (mlockall(MCL_CURRENT), == 0);
	CHKInt (mlockall(MCL_FUTURE), == 0);

	ChkInt ((minprio = sched_get_priority_min(SCHED_FIFO)), == 0);

	scpm.sched_priority = minprio + rtprio;
	ChkInt (sched_setscheduler(0, SCHED_FIFO, &scpm),  != -1);
	thr_scope = PTHREAD_SCOPE_SYSTEM;
}


/*
 * Command-line parsing
 */
DEF_PARSER(parse_contention,	ratio = strtod(arg, 0))
DEF_PARSER(parse_trylock,	trylock = 1)
DEF_PARSER(parse_recursive, 	mtx_type = PTHREAD_MUTEX_RECURSIVE)
DEF_PARSER(parse_inherit, 	mtx_protocol = PTHREAD_PRIO_INHERIT)
DEF_PARSER(parse_shared, 	mtx_pshared = PTHREAD_PROCESS_SHARED)
DEF_PARSER(parse_thrscope,	thr_scope = PTHREAD_SCOPE_SYSTEM)
DEF_PARSER(parse_realtime,	nrealtime = atoi(arg))
DEF_PARSER(parse_rtprio,	rtprio = atoi(arg))
DEF_PARSER(parse_niters,	niters = atoi(arg))
DEF_PARSER(parse_spincnt,	spincnt = atoi(arg))
DEF_PARSER(parse_latency,	record_latency = 1)

hrn_option_t hrn_options[] = {
	{ "C:", "set lock contention ratio\n",
		"The contention ratio is the time a thread spends holding\n"
		"the lock divided by the time the thread spends without\n"
		" attempting to acquire the lock. Just read the code.\n",
		parse_contention }, 
	{ "T", "use trylock instead of lock\n",
		"use trylock instead of lock\n",
		parse_trylock },
	{ "S:", "set the number of locks acquired by each thread\n",
		"By default each thread attempts to lock a mutex "
		"100 times.\n The -S flag allows you to change this number.",
		parse_niters },
	{ "B:", "set the amount of time spent holding the mutex\n",
		"",
		parse_spincnt },	
	{ "I", "use priority-inheriting mutexes\n",
		"use priority-inheriting mutexes\n",
		parse_inherit },
	{ "R", "use recursive mutexes\n",
		"use recursive mutexes\n",
		parse_recursive },
	{ "P:", "use # high priority threads.\n",
		"By default, all threads are created with default priority.\n"
		"If this option is set, it determines that a number of\nthreads"
		"should be created with real-time priority. You must be root\n"
		"to use this option.",
		parse_realtime },
	{ "U:", "set real-time thread priority\n",
		"",
		parse_rtprio },
	{ "L", "record lock latency.\n",
		"This option causes the test to record the time required for\n"
		"each lock to succeed. This incurs a substantial overhead.\n",
		parse_latency },
	{ "Y", "use system scope threads\n",
		"Use system scope threads; if any real-time threads are to be\n"
		"created this is implied.\n",
		parse_thrscope },
	{ 0, 0, 0, 0 }
};

/*
 * Documentation
 */

hrn_doc_t hrn_doc = {
	"Mutex",
	"Test performance of mutexes under varying degrees of contention",
	"each thread {\n"
	"	while ((nlocks / nwaits) < CONTENTION) {\n"
	"		mutex_lock(&global_mutex);\n"
	"		compute();\n"
	"		mutex_unlock(&global_mutex);\n"
	"		nlocks++;\n"
	"	}\n"
	"	while ((nlocks / nwaits) > CONTENTION) {\n"
	"		compute()\n"
	"		nwaits++;\n"
	"	}\n"
	"}\n",
	"returns total time spent for each thread\n"
};

