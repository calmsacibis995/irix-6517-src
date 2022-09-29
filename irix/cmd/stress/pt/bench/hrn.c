/*
 * Purpose: provide a harness
 */

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#ifdef sgi
#include <sys/syssgi.h>
#endif

#include <Tst.h>
#include "trc.h"
#include "hrn.h"
#include "sthr.h"

static int 	iters = 1;	/* number of repetitions of this test */
static ulong_t	lib = 0;	/* the threads library we are using */

static int parse_env(hrn_args_t* args);
static int parse_args(hrn_args_t* args, int argc, char** argv);
static void print_short_usage(int fd, hrn_option_t* opts);
static void print_long_usage(int fd, hrn_option_t* opt);
static hrn_option_t* find_option(int c, hrn_option_t* opts);
static void concat_optstr(char* buf, hrn_option_t* opts);
static void print_usage(int fd, 
			void (*pfunc)(int fd, hrn_option_t* opts),
			char* argv0);
static int p_doc(char* arg, hrn_args_t* args);

/* environment variables for dynamic pthreads linkage... */
#ifdef sgi
/* Note that _RLD_LIST is honored by all abis */
#define LIBLIST_ENV "_RLD_LIST"
#define PTHREAD_ENV "_RLD_LIST=libsthrpthread.so:libc.so:libpthread.so"
#define PTHREAD_ENVAL "libsthrpthread.so:libc.so:libpthread.so"
#else	/* does everyone else use these? linux and solaris do ... */
#define LIBLIST_ENV "LD_PRELOAD"
#define PTHREAD_ENV "LD_PRELOAD=libsthrpthread.so"
#define PTHREAD_ENVAL "libsthrpthread.so"
#endif

/*
 * All benchmarks share this main. We look for meaningful arguments and 
 * environment variables, and pass control to the test.
 */
int 
main(int argc, char** argv)
{
	int ii, retval;
	hrn_args_t args;
	args.a_argc = argc;
	args.a_argv = argv;

	if (parse_env(&args)) {
		print_usage(STDERR_FILENO, print_short_usage, argv[0]);
		return 1;
	}

	if (parse_args(&args, argc, argv)) {
		print_usage(STDERR_FILENO, print_short_usage, argv[0]);
		return 1;
	}

	retval = hrn_main(&args);

	for (ii = 1; ii < iters; ii++) {
		trc_result("------- iter %d\n", ii);
		retval = hrn_main(&args);
	}

	return retval;
}

/* 
 * Environment variable parsing
 */
static int exec_pthreads(hrn_args_t* args);
static int get_current_lib(void);

int 
parse_env(hrn_args_t* args)
{
	char* c;
	int retval = 0;

	/* the verbosity level */
	if (c = getenv(HRN_ENV_VRB)) {

#define ENVCASE(x) \
		if (!strcmp(c,HRN_ENVAL_ ## x)) \
			trc_set_verbosity(TRC_ ## x); \
		else

		ENVCASE(RESULT)
		ENVCASE(INFO)
		ENVCASE(VRB)
#undef ENVCASE
		return 0;
	}	

	/* the thread library; by default, use pthreads */
	if (!(c = getenv(HRN_ENV_SPROC)))
		retval += exec_pthreads(args);
	else
		lib = get_current_lib();


	return retval;
}

/* 
 * Determine the current thread model. If we are linked against something
 * defining pthread_create, we assume we are a pthread program.
 */
static int 
get_current_lib(void)
{
#ifdef sgi
	if (! dlsym(0, "_SGIPT_init1")) {
		return HRN_LIBCAP_SPROC;
	}
#else
#error fix me now.
#endif

	return HRN_LIBCAP_PTHREAD;
}

/*
 * Switching to pthreads is non-obvious. if we link against libpthread, sproc
 * will fail; at present we can't just dlopen libpthread because of pv 607716.
 * So, we set some environment variables to ask rld to load the pthread library,
 * and then re-exec ourselves.
 * 
 * Fail if the current test can't run with pthreads.
 */
static int
exec_pthreads(hrn_args_t* args)
{
	char* c;
	if ( !(hrn_libcap & HRN_LIBCAP_PTHREAD) ) 
		return -1;

	/* if liblist isn't set, or it was set to something other than what
	 * we expected, re-exec ourselves
	 */
	if (!(c = getenv(LIBLIST_ENV)) 
	    || (strcmp(c, PTHREAD_ENVAL)) ) {
		trc(TRC_INFO, "switching to pthreads\n");
		putenv(PTHREAD_ENV);
		execvp(args->a_argv[0], args->a_argv);
		perror("exec");
		return -1;
	}

	/* 
	 * the environment variable was already set; ergo, we're the
	 * pthread-running exec'ed child.
	 */
	lib = HRN_LIBCAP_PTHREAD;

	return 0;
}

/*
 * Argument parsing. We have an internal table of hrn_option_t's, and each test
 * defines its own additional table.
 */

DEF_PARSER(p_nthreads, args->a_nthreads = atoi(arg))
DEF_PARSER(p_conc, pthread_setconcurrency(atoi(arg)))
DEF_PARSER(p_iters, iters=atoi(arg))
DEF_PARSER(p_output, TstSetOutput("IAT"))
DEF_PARSER(p_shortusage, 
	   print_usage(STDOUT_FILENO, print_short_usage, args->a_argv[0]); 
	   exit(0))
DEF_PARSER(p_longusage, 
	   print_usage(STDOUT_FILENO, print_long_usage, args->a_argv[0]); 
	   exit(0))


static hrn_option_t 
internal_options[] = {
	{ "t:", "use # threads\n", 
		"This adjusts the number of threads used in the test.\n",
		p_nthreads },
	{ "c:", "use concurrency level #\n",
		"By default, the thread library chooses the number of kernel\n"
		"execution vehicles on which pthreads are multiplexed. The\n"
		"user can change this number via pthread_setconcurrency(3P).\n"
		"This option causes the concurrency level to be set to the\n"
		"given option\n",
		p_conc },
	{ "i:", "iterate test # times\n",
		"This option causes the test to be repeated the specified \n"
		"number of times",
		p_iters },
	{ "d", "print documentation\n",
		"This causes the test to print out some documentation about\n"
		"its purpose, use, and actions.\n",
		p_doc },
	{ "u", "print usage\n",
		"This causes the test to output the short usage message.\n",
		p_shortusage },
	{ "h", "print verbose help\n",
		"Causes the test to print this longer help message.\n",
       		p_longusage },
	
	{ 0, 0, 0, 0 }
};


/*
 * p_doc
 * 
 * Print out the current test's documentation.
 */
static int
p_doc(char* arg, hrn_args_t* args)
{
	char* fmt;

	trc_print(STDOUT_FILENO, 
		  "-------------------------------------------------------\n");
	trc_print(STDOUT_FILENO, "Name:\t\t%s\n", hrn_doc.d_name);
	trc_print(STDOUT_FILENO, "Pthreads?:\t%s\n", 
		  (hrn_libcap & HRN_LIBCAP_PTHREAD)? "yes": "no");
	trc_print(STDOUT_FILENO, "Sprocs?:\t%s\n",
		  (hrn_libcap & HRN_LIBCAP_SPROC)? "yes": "no");
	trc_print(STDOUT_FILENO, "Purpose:\t%s\n", hrn_doc.d_purpose);
	trc_print(STDOUT_FILENO, "Algorithm:\n%s\n", hrn_doc.d_algo);
	trc_print(STDOUT_FILENO, "Results:\t%s\n", hrn_doc.d_results);

	exit(0);
	return -1;
}

/* 
 * parse_args
 * 
 * Search our table and the user's table, make sure option is relevant,
 * and call the hook.
 */
static int 
parse_args(hrn_args_t* args, int argc, char** argv)
{
	int c;
	char optbuf[256];

	memset(optbuf, 0, sizeof(optbuf));
	concat_optstr(optbuf, hrn_options);
	concat_optstr(optbuf, internal_options);

	args->a_nthreads = 1;
	while ((c = getopt(argc, argv, optbuf)) > 0) {
		hrn_option_t* opt;
		/* allow user's options to override harness options */
		if (!(opt = find_option(c, hrn_options))
		    && !(opt = find_option(c, internal_options)))
			return -1;
		if (opt->o_parsehook(optarg, args))
			return -1;
	}
	return 0;
}

/* 
 * convert an option-string to a readable pre-hyphenated description 
 */
static void 
optstr_to_desc(char* dest, char* src)
{
	*dest++ = '-';
	*dest++ = *src;
	if (src[1] == ':') {
		*dest++ = ' ';
		*dest++ = '#';
	}
	*dest++ = 0;
}

/*
 * find an option matching c in the given list of options
 */
static hrn_option_t* 
find_option(int c, hrn_option_t* opts)
{
	int ii;
	for (ii = 0; opts[ii].o_str; ii++) {
		if (*(opts[ii].o_str) == c)
			return opts+ii;
	}

	return 0;
}


/*
 * append the options in opts to buf, for use as an arg to getopt
 */
static void 
concat_optstr(char* buf, hrn_option_t* opts)
{
	int ii;
	for (ii = 0; opts[ii].o_str; ii++) {
		strcat(buf, opts[ii].o_str);
	}
}

/*
 * usage/documentation routines
 */
static void 
print_usage(int fd, void (*pfunc)(int fd, hrn_option_t* opts), char* argv0)
{
	trc_print(fd, "usage: %s <options>\n", argv0);
	pfunc(fd, hrn_options);
	pfunc(fd, internal_options);
}

static void 
print_short_usage(int fd, hrn_option_t* opts)
{
	int ii;

	for (ii=0; opts[ii].o_str; ii++) {
		char optstr[256];
		optstr_to_desc(optstr, opts[ii].o_str);
		trc_print(fd, "\t%s\t%s", optstr, opts[ii].o_sdesc);
	}
}

static void 
print_long_usage(int fd, hrn_option_t* opts)
{
	int ii;

	for (ii=0; opts[ii].o_str; ii++) {
		char optstr[256];
		optstr_to_desc(optstr, opts[ii].o_str);
		trc_print(fd, "\t%s\n\n", optstr);
		trc_print(fd, "%s\n", opts[ii].o_ldesc);
	}
}


/* 
 * Barriers
 * 
 * we need a general purpose barrier facility; we usually want the threads
 * created in a benchmark to start running at approximately the same time.
 */
typedef struct barargs {
	sthr_func_t	b_func;	
	void*		b_arg;
	sthr_barrier_t* b_bar;
} barargs_t;


/*
 * hrn_barrier_start
 * 
 * create a thread that will synchronize on bar before executing func.
 */
static void barmain(void* arg);

int 
hrn_barrier_start(sthr_barrier_t* bar, sthr_t* retval, sthr_attr_t* opts,
		      sthr_func_t func, void* arg)
{
	barargs_t *b = NEW(barargs_t);
	
	b->b_func = func;
	b->b_arg = arg;
	b->b_bar = bar;
	return sthr_create(retval, opts, barmain, b);
}

static void
barmain(void* arg)
{
	barargs_t *b = (barargs_t*) arg;
	sthr_barrier_join(b->b_bar);

	b->b_func(b->b_arg);
	free(arg);
}

/*
 * hrn_ratio_execute
 * 
 * execute func1 or func2 a total of niters times, so that
 * 	nuber of calls of func1 / number of calls of func2 ~= ratio
 */
void
hrn_ratio_execute(const double ratio, const int niters, 
	     hrn_ratio_func_t func1, void* arg1, 
	     hrn_ratio_func_t func2, void* arg2)
{
	int	ii;
	double	nf1, nf2;

	nf1 = nf2 = 1;
	for (ii=0; ii < niters; ii++) {
		if (nf1 / nf2 <= ratio) {
			nf1++;
			func1(arg1);
		}

		if (nf1 / nf2 > ratio) {
			nf2 ++;
			func2(arg2);
		}
	}
}

