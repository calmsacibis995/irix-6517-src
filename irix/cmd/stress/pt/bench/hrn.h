/*
 * Harness interface for pthread benchmarks.
 */

#ifndef HRN_H_
#define HRN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "trc.h"
#include <mutex.h>
#include <Tst.h>

#include "sthr.h"

/* 
 * A hrn_args_t is passed to the test's main body; this contains arguments
 * that various tests might be interested in.
 */
typedef struct {
	int		a_argc;
	char**		a_argv;
	/* 
	 * nthreads; this option is parsed by the harness, but each test does
	 * something slightly different with it.
	 */
	int		a_nthreads;
} hrn_args_t;

/* 
 * convenience macros for timing. notice that START_TIMER has an opening brace
 * that is closed in END_TIMER. caveat emptor: don't try to use these across
 * scopes. 
 */
#define START_TIMER \
{ \
	BnchDecl(start); BnchDecl(stop); \
	BnchStamp(&start); BnchStamp(&stop); /* take startup costs upfront */ \
	BnchStamp(&start);

#define END_TIMER(str) \
	BnchStamp(&stop); \
	BnchDelta(&stop, &start, &stop); \
	BnchPrint(str, &stop); \
}

/* 
 * Capability bitmasks. Some tests/options are applicable only to a particular
 * thread library; 
 */
#define HRN_LIBCAP_PTHREAD		1
#define HRN_LIBCAP_SPROC		2
#define HRN_LIBCAP_ALL		(~0UL)

extern ulong_t	hrn_libcap;		/* tests this thread can run */

#define HRN_DECLARE_LIBCAP(x) \
ulong_t hrn_libcap = x

typedef struct {
	char*	o_str;			/* option string */
	char*	o_sdesc;		/* short description */
	char*	o_ldesc;		/* long description */
	int	(*o_parsehook)(char* arg, hrn_args_t* hargs);	/* callback */
} hrn_option_t;

/* documentation hooks */
typedef struct hrn_doc {
	char*	d_name;		/* "Mutex" */
	char*	d_purpose;	/* "Measuring mutex performance" */
	char*	d_algo;		/* description of test's operation */
	char*	d_results;	/* description of test's output */
} hrn_doc_t;


/* convenience macro for defining trivial parser functions */
#define DEF_PARSER(name, body) \
static int name (char* arg, hrn_args_t* args) { body; return 0; }


/* environment variables and values we look for */
#define HRN_ENV_SPROC 		"HRN_SPROC"
#define HRN_ENV_VRB 		"HRN_VRB"
#define HRN_ENVAL_RESULT		"result"
#define HRN_ENVAL_INFO			"info"
#define HRN_ENVAL_VRB			"vrb"

/*
 * From here on, helper functions.
 * 
 * purple prose follows.
 */
/* 
 * Use this to repeatedly perform two operations with a given relative 
 * frequency.
 */
typedef void (*hrn_ratio_func_t)(void* arg);
extern void 
hrn_ratio_execute(const double ratio, const int niters, 
		  hrn_ratio_func_t func1, void* arg1, 
		  hrn_ratio_func_t func2, void* arg2);

/* 
 * convenience routine for starting threads on a barrier
 */
int hrn_barrier_start (sthr_barrier_t* bar, sthr_t* retval, sthr_attr_t* opts,
		       sthr_func_t func, void* arg);

/* XXX: why are these here? */
#define NEW(x) (x*) malloc(sizeof(x))
#define NEWV(x,sz) (x*) malloc(sizeof(x) * (sz))

/*
 * Below are symbols that the test module must provide 
 */

/* 
 * The main of the benchmark 
 */
int hrn_main(hrn_args_t* args);

/* 
 * A table of option_t's, null-terminated, to allow the harness to parse
 * command-line options.
 */
extern hrn_option_t hrn_options[];

/* docs */
extern hrn_doc_t hrn_doc;

#ifdef __cplusplus
}
#endif
#endif

