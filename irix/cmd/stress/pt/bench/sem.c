/*
 * Purpose: measure semaphore performance
 */

#include <mutex.h>
#include "sthr.h"
#include "hrn.h"

HRN_DECLARE_LIBCAP(HRN_LIBCAP_ALL);

static sthr_sem_t 	sem;

/* command-line parameters */
static int 		nwaiters = 1;
static int 		nposters = 1;

static int 		nwaits = 500;
static sthr_barrier_t	bar;

static void waitmain(void* arg);
static void postmain(void* arg);

int hrn_main(hrn_args_t* args)
{
	int 		ii, jj;
	int 		total = nwaiters + nposters;
	sthr_t*		threads = NEWV(sthr_t, total);

	/* 
	 * initialize nwaits: it's important that nwaits/nposters is a
	 * whole number, so, if nwaits isn't divisible by nposters, we 
	 * bump it up to the next largest multiple of nposters.
	 */
	if (nwaits % nposters) {
		int old_nwaits = nwaits;
		nwaits = (((int)nwaits/nposters)+1) * nposters; 
		trc_info("bumping nwaits from %d to %d\n", old_nwaits, nwaits);
	}

	sthr_barrier_init(&bar, total+1);
	sthr_sem_init(&sem, 1);

	for (ii=0; ii < nwaiters; ii++) {
		trc_info("creating %d'th waiter\n", ii);
		ChkInt(hrn_barrier_start(&bar, threads+ii, 0, waitmain, 0), 
		       == 0);	
	}

	for (jj = 0; jj < nposters; jj++, ii++) {
		trc_info("creating %d'th poster\n", jj);
		ChkInt(hrn_barrier_start(&bar, threads+ii, 0, postmain, 0),
		       == 0);
	}

	sthr_barrier_join(&bar);

	for (ii = 0; ii < total; ii++) {
		trc_info("joining %d'th thread\n", ii);
		ChkInt(sthr_join(threads+ii), ==0);
	}

	sthr_sem_destroy(&sem);
	sthr_barrier_destroy(&bar);
	return 0;
}

/*
 * Do nwaits waits.
 */
static void waitmain(void* arg)
{
	int ii;
	long val;

	trc_info("wait: will do %d waits\n", nwaits);
	START_TIMER;
	for (ii = 0; ii < nwaits; ii++)  {
		sthr_sem_wait(&sem);
	}

	END_TIMER("wait");

	trc_info("waitmain returning\n");
}

/*
 * Do an appropriate number of posts
 */
static void postmain(void* arg)
{
	int ii;
	long val;
	int total = (nwaits*nwaiters) / nposters;
	START_TIMER;
	
	trc_info("post: will do %d posts\n", total);
	for (ii=0; ii < total; ii++) {
		sthr_sem_post(&sem);
	}
	END_TIMER("post");
	trc_info("postmain returning\n");
}

DEF_PARSER(p_posters, nposters = atoi(arg))
DEF_PARSER(p_waiters, nwaiters = atoi(arg))
DEF_PARSER(p_nwaits, nwaits = atoi(arg))
hrn_option_t hrn_options[]= {
	{ "P:", "Set number of posters\n",
		"", 
		p_posters },
	{ "W:", "Set number of waiters\n",
		"", 
		p_waiters },
	{ "I:", "Set the number of wait iterations for each thread\n",
		"",
		p_nwaits },
	{ 0, 0, 0, 0 }
};

hrn_doc_t hrn_doc = {
"Sem",

"Measure performance of semaphore operations",

"wait_thread:\n"
"	for (ii=0; ii < nwaits; ii++)\n"
"		sem_wait(global_sem);\n"
"\n"
"post_thread:\n"
"	for (ii=0; ii < nwaiters*nwaits/nposters; ii++)\n"
"		sem_post(global_sem);\n"
"\n",

"Total time for each thread to complete.\n"
};

