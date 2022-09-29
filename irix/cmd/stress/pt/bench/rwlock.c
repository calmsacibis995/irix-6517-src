/*
 * Purpose: test read-write lock performance
 */

#include <mutex.h>
#include <stdlib.h>
#include <pthread.h>
#include "hrn.h"
#include "sthr.h"

HRN_DECLARE_LIBCAP(HRN_LIBCAP_PTHREAD);

#define NITERS 10000

/* command-line parameters */
static int nreaders = 2;
static int nwriters = 1;
static double readcont = 100000;
static double writecont = 100000;

pthread_rwlock_t rwl;

static void* readmain(void* arg);
static void* writemain(void* arg);

/*
 * Create our read and write threads. Join them all.
 */
int hrn_main(hrn_args_t* args)
{
	int 		ii, jj;
	sthr_barrier_t 	bar;
	pthread_t* 	threads = NEWV(pthread_t, nreaders + nwriters);

	pthread_rwlock_init(&rwl, 0);
	sthr_barrier_init(&bar, nreaders + nwriters + 1);
	for (ii = 0; ii < nreaders; ii++) {
		trc_info("creating %d'th reader\n", ii);
		ChkInt(pthread_create(threads+ii, 0,readmain, &bar), ==0);	
	}

	for (jj=0; jj < nwriters; jj++, ii++) {
		trc_info("creating %d'th reader\n", ii);
		ChkInt(pthread_create(threads+ii, 0, writemain, &bar), ==0);		}

	sthr_barrier_join(&bar);
	for (ii = 0; ii < nwriters + nreaders; ii++) {
		trc_info("joining %d'th thread\n", ii);
		ChkInt(pthread_join(threads[ii], 0), ==0);
	}

	pthread_rwlock_destroy(&rwl);
	sthr_barrier_destroy(&bar);
	return 0;
}

static void waste_time(void* arg);
static void do_read(void* arg);
static void do_write(void* arg);

/*
 * In both threads we time the completion of a bunch of operations executed
 * some ratio of the time.
 */
#define MAIN_BODY(x) \
	sthr_barrier_t* bar = (sthr_barrier_t*) arg; \
	sthr_barrier_join(bar); \
\
	trc_vrb("%s start\n", #x); \
	START_TIMER;	\
	hrn_ratio_execute(x ## cont, NITERS, do_ ## x, 0, waste_time, 0); \
	END_TIMER(#x); \
	return 0;

static void* readmain(void* arg)
{ 
	MAIN_BODY(read) 
}

static void* writemain(void* arg)
{ 
	MAIN_BODY(write) 
}

static void waste_time(void* arg)
{
	int ii;
	volatile double snork;

	for (ii=0; ii < NITERS; ii++) {
		snork = ii / 7;
	}
}

#define LOCK_BODY(x) \
	pthread_rwlock_ ## x (&rwl); \
	waste_time(0); \
	pthread_rwlock_unlock(&rwl);

static void do_read(void* arg)
{ 
	LOCK_BODY(rdlock) 
}

static void do_write(void* arg)
{ 
	LOCK_BODY(wrlock) 
}


DEF_PARSER(p_nreaders, nreaders = atoi(arg))
DEF_PARSER(p_nwriters, nwriters = atoi(arg))
DEF_PARSER(p_readcont, readcont = strtod(arg, 0))
DEF_PARSER(p_writecont, writecont = strtod(arg, 0))

hrn_option_t hrn_options[]= {
	{ "W:", "Set the number of writing threads\n",
		"",
		p_nwriters },	
	{ "R:", "Set the number of reading threads\n",
		"",
		p_nreaders },	
	{ "X:", "Set the write contention\n",
		"",
		p_writecont },
	{ "S:", "Set the read contention\n",
		"",
		p_readcont },
	{ 0, 0, 0, 0 }
};

hrn_doc_t hrn_doc = {
"RWLock",

"Measure performance of rwlock's\n",

"write thread:\n"
"	while (++cntr < BIGNUM)\n"
"		pthread_rwlock_wrlock(&global_lock);\n"
"\n"
"read thread:\n"
"	while (++cntr < BIGNUM)\n"
"		pthread_rwlock_rdlock(&global_lock);\n",

"Total time for reads; total time for writes\n"
};

