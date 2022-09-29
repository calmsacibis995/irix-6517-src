/*
 * Purpose: measure condition performance.
 * 
 * TODO: add some back-off for signal/broadcast threads via hrn_ratio_exec?
 */

#include <mutex.h>
#include "sthr.h"
#include "hrn.h"

HRN_DECLARE_LIBCAP(HRN_LIBCAP_ALL);

#define CNTR_INIT 1000

/* command-line parameters */
static int nwaiters = 1;
static int nsignallers = 1;
static int nbroadcasters = 0;

static ulong_t	cntr = CNTR_INIT;	/* threads count done this number */
static ulong_t	waiters_done;		/* waiters hit this counter when done */
static sthr_cond_t cond;

static void waitmain(void* arg);
static void signalmain(void* arg);
static void broadcastmain(void* arg);

/*
 * We create two different types of threads: one of them repeatedly
 * does waits; the other does signals.
 */
int hrn_main(hrn_args_t* args)
{
	int 		ii, jj;
	sthr_barrier_t	bar;
	register	total = nwaiters + nsignallers + nbroadcasters;
	sthr_t		*threads = NEWV(sthr_t, total);

	waiters_done = 0;
	sthr_cond_init(&cond);
	sthr_barrier_init(&bar, total+1);

	for (ii = 0; ii < nwaiters; ii++) {
		trc_info("starting waiter %d\n", ii);
		ChkInt(hrn_barrier_start(&bar, threads+ii, 0,
					 waitmain, 0), == 0);	
	}

	for (jj = 0; jj < nsignallers; jj++, ii++) {
		trc_info("starting signaller %d\n", ii);
		ChkInt(hrn_barrier_start(&bar, threads+ii, 0,
					 signalmain, 0), == 0);
	}

	for (jj = 0; jj < nbroadcasters; jj++, ii++) {
		trc_info("starting broadcaster %d\n", ii);
		ChkInt(hrn_barrier_start(&bar, threads+ii, 0,
					 broadcastmain, 0), == 0);
	}
	sthr_barrier_join(&bar);

	for (ii=0; ii < total; ii++) {
		trc_info("joining %d: 0x%08x\n", ii, threads[ii].s_pid);
		ChkInt(sthr_join(threads+ii), == 0);
	}

	sthr_cond_destroy(&cond);
	sthr_barrier_destroy(&bar);
	return 0;
}

/*
 * Both wake-up threads (signal and broadcast) do wake-ups until they see that
 * the waiters have counted cntr down to 0. Then, they begin doing broadcasts
 * until they see that there are no more waiters.
 */
#define WAKEUP_MAIN(op,args) \
static void op ## main(void* arg) \
{ \
	int ii; \
	long retval; \
	START_TIMER; \
	while ((retval = (long)test_then_add(&cntr, 0)) > 0) {\
		trc_vrb("%s: %d\n", #op, retval); \
		sthr_cond_ ## op args; \
	} \
	END_TIMER(#op); \
	while (waiters_done < nwaiters) {\
		trc_vrb("%s: broadcast %d\n", #op, waiters_done); \
		sthr_cond_broadcast(&cond); \
	} \
}

WAKEUP_MAIN(signal, (&cond))
WAKEUP_MAIN(broadcast, (&cond))

/*
 * Waiters each wait on a separate mutex, so as not to serialize the entire
 * test. Once we have waited the prescribed number of times, we exit and
 * bump waiters_done.
 */
static void waitmain(void* arg)
{
	long retval;
	sthr_mutex_t mtx;

	sthr_mutex_init(&mtx);
	sthr_mutex_lock(&mtx);
	START_TIMER;
	while ((retval = (long)test_then_add(&cntr,-1)) > 0) {
		trc_vrb("wait: %d\n", retval);
		sthr_cond_wait(&cond, &mtx);
	}
	sthr_mutex_unlock(&mtx);
	END_TIMER("wait");

	sthr_mutex_destroy(&mtx);
	test_then_add(&waiters_done, 1);
}

DEF_PARSER(p_sig, nsignallers = atoi(arg))
DEF_PARSER(p_wait, nwaiters = atoi(arg))
DEF_PARSER(p_broadcast, nbroadcasters = atoi(arg))
hrn_option_t hrn_options[]= {
	{ "S:", "Use # signalling threads\n",
		"",
		p_sig },
	{ "W:", "Use # waiting threads\n",
		"",
		p_wait },
	{ "B:", "Use # broadcasting threads\n",
		"",
		p_broadcast },
	{ 0, 0, 0, 0 }
};

hrn_doc_t hrn_doc = {
"Cond",

"Measure performance of condition operations",

"signal/broadcast threads:\n"
"	while there are living waiters\n"
"		signal/broadcast(&global_cond);\n"
"\n"
"wait threads:\n"
"	while (--nwaits) {\n"
"		mutex_lock(&priv_mtx);\n"
"		wait(&global_cond);\n"
"		mutex_unlock(&priv_mtx);\n"
"	}",

"Length of each operation",
};

