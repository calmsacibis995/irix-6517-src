/*
 * Purpose: Measure creation/destruction costs for threads. This experiment can
 * be run in one of two modes; it defaults to serial mode.
 * 
 * In serial mode, it creates and joins, one after another, args->a_nthreads
 * threads, all running null_thread, which simply returns zero. In parallel
 * mode, it first creates args->a_nthreads threads, all of which then go on to
 * create and join a null thread.
 * 
 * The serial mode provides a straight number for the overhead of thread
 * creation and teardown. The parallel one measures the cost of contention for
 * thread library resources.
 */

#include <stdlib.h>
#include <pthread.h>

#include "hrn.h"
#include "sthr.h"

HRN_DECLARE_LIBCAP(HRN_LIBCAP_PTHREAD);

/* these parameters are tweakable from the command-line */
static int scope = PTHREAD_SCOPE_PROCESS;/* scope of created threads */
static int parallel=0;			/* if true, run tests in parallel */
static int comp=0;			/* number of loops in thread */

static sthr_barrier_t bar;		/* barrier for parallel test */

static void* par_nullthr_create(void* arg);
static int parmain(hrn_args_t* args, pthread_attr_t* attr);
static int sermain(hrn_args_t* args, pthread_attr_t* attr);
static void* par_nullthr_create(void* arg);
static void* nullthr(void*arg);

int hrn_main(hrn_args_t* args)
{
	pthread_attr_t attr;
	int retval;

	pthread_attr_init(&attr);
	pthread_attr_setscope(&attr, scope);

	if (parallel)
		retval = parmain(args, &attr);
	else 
		retval = sermain(args, &attr);

	pthread_attr_destroy(&attr);
	return retval;
}

/* 
 * The threads we are creating ultimately execute this body. By default, it
 * returns immediately; if comp has been set, it may spin a bit before
 * returning.
 */
static void* nullthr(void*arg) 
{ 
	int ii;
	for (ii=0; ii < 100*comp; ii++) ;
	return 0; 
}

/* 
 * Wait for the creation and completion of a thread that will execute
 * null_thread
 */
static int nullthr_create(pthread_attr_t* attr)
{
	pthread_t thr;
	int err = 0;
	err += pthread_create(&thr, attr, nullthr, 0);
	err += pthread_join(thr, 0);
	return err;
}

/* 
 * The serial test's body: create one thread after another 
 */
static int sermain(hrn_args_t* args, pthread_attr_t* attr)
{
	int ii;

	START_TIMER;
	for (ii = 0; ii < args->a_nthreads; ii++)
		nullthr_create(attr);
	END_TIMER("serial");
	return 0;
}

/*
 * The parallel test's body; create several threads, let them hit a barrier,
 * and wait for the threads to each create a thread, then return. This should
 * test the scalability of thread creation code.
 */
static int parmain(hrn_args_t* args, pthread_attr_t* attr)
{
	int ii;
	pthread_t 	*thrs;

	thrs = NEWV(pthread_t, args->a_nthreads);

	sthr_barrier_init(&bar, args->a_nthreads+1);
	for (ii = 0; ii < args->a_nthreads; ii++) {
		pthread_create(&thrs[ii], attr, par_nullthr_create, attr);
	}

	START_TIMER;
	sthr_barrier_join(&bar);
	for (ii = args->a_nthreads-1; ii >= 0; ii--) {
		pthread_join(thrs[ii], 0);
	}

	END_TIMER("parallel");
	
	sthr_barrier_destroy(&bar);
	return 0;
}


/* 
 * Wait on the barrier for the other threads to be ready, then create a thread.:
 */
static void* par_nullthr_create(void* arg)
{
	pthread_attr_t* attr = (pthread_attr_t*) arg;
	sthr_barrier_join(&bar);
	nullthr_create(attr);
	return 0;
}


/* 
 * Command-line parsing.
 */
DEF_PARSER(parse_par, parallel = 1)
DEF_PARSER(parse_scope, scope = PTHREAD_SCOPE_SYSTEM)
DEF_PARSER(parse_compute, comp = atoi(arg))

hrn_option_t hrn_options[] = {
	{"P", "create threads in parallel\n",
		"This will create several threads, all of which create \n"
		"threads in parallel.\n",
		parse_par },
	{ "S", "create threads at system scope\n",
		"Set the PTHREAD_SCOPE_SYSTEM pthread attribute for created\n"
		"threads. By default, threads are created with \n"
		"PTHREAD_SCOPE_PROCESS",
		parse_scope },
	{ "C:", "set computation time for threads\n",
		"This sets a loop index that created threads will spin in.\n",
		parse_compute },
	{ 0, 0, 0, 0 }
};

hrn_doc_t hrn_doc = {
"LifeCycle",

"Measure the cost of thread creation and destruction\n",

"In parallel mode:\n"
	"each thread {\n"
	"	for i = 0 to nthread {\n"
	"		thread_create();\n"
	"	}\n"
	"	for i = 0 to nthread {\n"
	"		thread_join(i);\n"
	"	}\n"
	"}\n"
	"\n"
	"In serial mode:\n"
	"each thread {\n"
	"	nop\n"
	"}\n",

"Elapsed thread creation/join times\n"
};

