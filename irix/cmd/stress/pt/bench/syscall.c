/*
 * Purpose: test system call performance and scalability. 
 */

#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>

#include "hrn.h"
#include "sthr.h"

HRN_DECLARE_LIBCAP(HRN_LIBCAP_ALL);

static void do_syscall(void*);
static void do_close(void);
static void do_write(void);

/* cmd-line parameter */
static void (*syscallptr)() = do_close;
static int niters = 100000;

/* 
 * Create a number of threads, each of which will repeatedly perform a system
 * call.
 */
int hrn_main(hrn_args_t* args)
{
	int		ii;
	sthr_barrier_t	bar;
	sthr_t		*threads = NEWV(sthr_t, args->a_nthreads);
	int*		targs = NEWV(int, args->a_nthreads);

	sthr_barrier_init(&bar, args->a_nthreads+1);
	for (ii = 0; ii < args->a_nthreads; ii++) {
		targs[ii] = ii + 1;
		trc_info("creating %d'th thread\n", ii);
		ChkInt(hrn_barrier_start(&bar, threads+ii, 
					 0, do_syscall, targs+ii), == 0);
	}

	trc_info("joining barrier\n");
	sthr_barrier_join(&bar);

	for (ii = 0; ii < args->a_nthreads; ii++) {
		trc_info("joining %d'th thread\n", ii);
		ChkInt(sthr_join(threads+ii), == 0);
	}

	sthr_barrier_destroy(&bar);
	trc_info("all done\n");
	return 0;
}

static void do_syscall(void* arg)
{
	int ii;
	int cpu = *(int*) arg;

	START_TIMER;
	for (ii = 0; ii < niters; ii++) {
		syscallptr();
	}
	END_TIMER("total");

	free(arg);
}

#define DO_SYSCALL(call,args) \
static void do_ ## call (void) { call args; }

static void do_nop() { return; }

DO_SYSCALL(close, (-1))
DO_SYSCALL(write, (-1, 0, 0))
DO_SYSCALL(_syscall, (-1))
DO_SYSCALL(signal, (11, 0))
DO_SYSCALL(__write, (-1,0,0))
DO_SYSCALL(getpid, ())
DO_SYSCALL(syssgi, (-1))

int p_syscall(char* arg, hrn_args_t* args)
{
#define SYS_COND(x) \
	if (! strcmp(arg,(#x)) ) { \
		syscallptr = do_ ## x ; \
		return 0; \
	}

	SYS_COND(close)
	SYS_COND(write)
	SYS_COND(signal)
	SYS_COND(_syscall)
	SYS_COND(__write)
	SYS_COND(getpid)
	SYS_COND(syssgi)
	SYS_COND(nop)
	return -1;
}

DEF_PARSER(p_write, syscallptr = do_write)
DEF_PARSER(p_niters, niters = atoi(arg))
hrn_option_t hrn_options[] = {
	{ "S:", "change system call to test\n",
		"change system call to test\n",
		p_syscall },
	{ "I:", "set number of system call iterations\n",
		"The test will perform the given syscall this many times\n",
		p_niters },
	{ 0, 0, 0, 0 }
};

hrn_doc_t hrn_doc = {
"Syscall",

"Measure impact of thread library on system call performance",

"each thread {\n"
"	for i = 0 to a_big_number {\n"
"		do_syscall();\n"
"	}\n"
"}\n",

"time for each thread to complete\n"
};

