#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/sysmp.h>
#include <sys/prctl.h>

#include <ulocks.h>
#include <invent.h>

#ifdef SN0
#include <sys/pmo.h>
#include <fetchop.h>
#endif /* SN0 */

#include "cycle.h"
#include "lat_op.h"

struct thread_info;

#define ALL            (-1)
#define ALL_8BIT       (ALL & 0xFF)

/*
 *	Operation definitions
 *	=====================
 */

typedef struct {
	const char *op_name;
	void (*run)(struct thread_info *this_thread);
	void (*init)(void);
} op_t;

#define decl_op_init(o) static void init_ ## o (void)
#define decl_op(o) static void use_ ## o (struct thread_info *this_thread)
#define ops_op(o) { #o, use_ ## o, init_ ## o }

decl_op_init(fetch_add);
decl_op(fetch_add);
decl_op_init(load);
decl_op(load);
decl_op_init(null);
decl_op(null);
decl_op_init(fail);
decl_op(fail);
decl_op_init(store);
decl_op(store);

#ifdef SN0
decl_op_init(fetchop_fetch_add);
decl_op(fetchop_fetch_add);
#endif /* SN0 */

op_t ops[] = {
	ops_op(fetch_add),
	ops_op(load),
	ops_op(null),
	ops_op(fail),
	ops_op(store),

#ifdef SN0
	ops_op(fetchop_fetch_add),
#endif /* SN0 */
};
const int nops = sizeof(ops) / sizeof(op_t);


/*
 *      Program state
 *      =============
 */

typedef struct {
    clock_t       clk;
    struct tms    tms;
    struct rusage ru;
} perfInfo;

typedef struct thread_info {
  char          pad1[128];      /* eliminate cache-line contention */

  int           num;            /* thread number */
  int           proc;           /* assigned processor; -1 for none */
  pid_t         pid;            /* sproc pid */
  perfInfo      pi;             /* performance for operations */
  int           op;             /* operation to run */
  uint64_t      nops;           /* number of completed operations */
  uint64_t      fops;           /* number of failed llscs */
  uint64_t	work_ticks;	/* parallel work (counter ticks) */
  uint64_t	*work_ticks_jittered; /* clock jitter samples */

  char          pad2[128];      /* eliminate cache-line contention */
} thread_info;

static volatile int nready;	/* barrier: number of procs ready */

static volatile uint64_t *sharedp = NULL; 	/* shared variable */

static thread_info *g_threads = NULL;
static volatile uint64_t *counter = NULL;
static uint64_t cycleval = 0;
static uint64_t *work_ticks_per_proc = NULL;

#define barrier_wait(b, n) {           \
            __add_and_fetch(&b, 1);    \
	    while (b != n)             \
               __synchronize(b);       \
        }

/*
 *	Argument declarations
 *	=====================
 */
static char *myname;			/* name we were invoked under */
static int mustrun = -1;                /* default initial cpu */
static int nsecs = 60;		        /* time until operation-count */
static double work_usecs = 0;		/* work to do between ops */
static int op = 0;                      /* default operation */
static int nthreads = 1;                /* default number of threads */
static double magic_ticks_per_usec = 1;	/* default ticks per usec */
					/*    -- machine specific */
static int calibrating_timer = 0;       /* calibrate timer only */

/*
 *	Local routine declarations
 *	==========================
 */

int main(int argc, char *argv[]);
static void parseArguments(int argc, char *argv[]);
static void usage(void);
static void run_once(void *arg);

static void startTiming(perfInfo *pi);
static void endTiming(perfInfo *pi);
static void printTiming(const char *name, perfInfo *pi, unsigned long units);
static void printThreadTimingHdr();
static void printThreadTiming(int threadn, perfInfo *pi,
                              uint64_t units, uint64_t failed_units,
			      double work_usecs);

static void init_clock_jitter(uint64_t work_ticks,
				uint64_t **work_ticks_jittered);
void calibrate_nop_counter(uint64_t nloops,
				uint64_t *work_ticks_jitter);

static void __inline
delay(uint64_t nticks)
{
	register uint64_t end = *counter + nticks;
	while (*counter < end)
		;
}

static void
try_bind(int proc)
{
  if (sysmp(MP_MUSTRUN, proc) < 0)
    {
      perror("sysmp");
      exit(1);
    }
}

/*
 *	Program code
 *	============
 */

#if defined(USE_COP0_COUNTER)
static uint64_t
ticks_per_usec(double usecs, double mhz)
{
	return (usecs * mhz);
}

static void
init_timing_unmapped(int nprocs)
{
	inventory_t *ent;
	int pp;

	setinvent();

	while ((ent = getinvent()) != NULL) {
		if (ent->inv_class == INV_PROCESSOR &&
		    ent->inv_type == INV_CPUBOARD) {
			uint64_t ticks;
			double mhz;

			mhz = ((double) ent->inv_controller) / 2;
			ticks = ticks_per_usec(work_usecs, mhz);

			if ((ent->inv_unit & ALL_8BIT) == ALL_8BIT) {
				/* all processors same speed */

				for (pp = 0; pp < nprocs; pp++) {
					work_ticks_per_proc[pp] = ticks;
				}
			}
			else {
				int pp = ent->inv_unit;

				work_ticks_per_proc[pp] = ticks;
			}
		}
	}

	endinvent();
}
#endif /* USE_COP0_COUNTER */

#if defined(USE_MAPPED_COUNTER)
static void
init_timing_mapped(int nprocs)
{
	int pp;
	uint64_t work_ticks;

	if (!cycle_init(&counter, &cycleval))
		exit(EXIT_FAILURE);

	work_ticks = cycle_usecs_to_ticks(cycleval, work_usecs);

	/* account for high-granularity timing on lego */
	work_usecs = cycle_ticks_to_usecs(cycleval, work_ticks);

	for (pp = 0; pp < nprocs; pp++) {
		work_ticks_per_proc[pp] = work_ticks;
	}
}
#endif /* USE_MAPPED_COUNTER */

static double
timeval_diff(struct timeval *start_clock, struct timeval *end_clock)
{
	double elapsed_s;

	elapsed_s = (end_clock->tv_sec - start_clock->tv_sec)
		+ (double) (end_clock->tv_usec - start_clock->tv_usec)
			/ 1.0e6;

	return elapsed_s;
}

#if defined(USE_NOP_COUNTER)
static void
init_timing_nops(int nprocs)
{
	int pp;
	uint64_t work_ticks;

	if (calibrating_timer) {
		struct tms dummy;
#if defined(CALIBRATE_WITH_GETTIMEOFDAY)
		struct timeval start_clock, end_clock;
#elif defined(CALIBRATE_WITH_TIMES)
		clock_t start_clock, end_clock;
#else
		struct timespec start_clock, end_clock;
#endif
		double elapsed_s, usec_per_loop;
		const uint64_t nloops = 10000;
		const uint64_t work = 400000;
		perfInfo timing;
		uint64_t *work_ticks_jitter;

		init_clock_jitter(work, &work_ticks_jitter);

#if defined(CALIBRATE_WITH_GETTIMEOFDAY)
		gettimeofday(&start_clock);
#elif defined(CALIBRATE_WITH_TIMES)
		start_clock = times(&dummy);
#else
		clock_gettime(CLOCK_SGI_CYCLE, &start_clock);
#endif

		calibrate_nop_counter(nloops, work_ticks_jitter);

#if defined(CALIBRATE_WITH_GETTIMEOFDAY)
		gettimeofday(&end_clock);
#elif defined(CALIBRATE_WITH_TIMES)
		end_clock = times(&dummy);
#else
		clock_gettime(CLOCK_SGI_CYCLE, &end_clock);
#endif

#if defined(CALIBRATE_WITH_GETTIMEOFDAY)
		elapsed_s = (end_clock.tv_sec - start_clock.tv_sec)
			+ (double) (end_clock.tv_usec - start_clock.tv_usec)
				/ 1.0e6;
#elif defined(CALIBRATE_WITH_TIMES)
		elapsed_s = ((double)(end_clock - start_clock)) / CLK_TCK;
#else
		elapsed_s = (double) (end_clock.tv_sec - start_clock.tv_sec)
			+ (double) (end_clock.tv_nsec -
				start_clock.tv_nsec) / 1.0e9;
#endif

		magic_ticks_per_usec = (nloops * work) / (elapsed_s * 1.0e6);

		printf("CALIBRATED: %.7f MHz "
			"(took %0.4fs, clk_tck %ld)\n",
			(double) magic_ticks_per_usec,
			(double) elapsed_s,
			(long) CLK_TCK);

		exit(EXIT_SUCCESS);
	}

	work_ticks = (double) magic_ticks_per_usec * work_usecs;

	/* account for rounding error */
	work_usecs = work_ticks / magic_ticks_per_usec;

	for (pp = 0; pp < nprocs; pp++) {
		work_ticks_per_proc[pp] = (uint64_t) work_ticks;
	}

}
#endif /* USE_NOP_COUNTER */

static void
init_timing(void)
{
	int nprocs;

	nprocs = sysmp(MP_NPROCS);
	work_ticks_per_proc = (uint64_t*) calloc(sizeof(uint64_t), nprocs);

#if defined(USE_MAPPED_COUNTER)

	init_timing_mapped(nprocs);

#elif defined(USE_COP0_COUNTER)

	init_timing_unmapped(nprocs);

#elif defined(USE_NOP_COUNTER)

	init_timing_nops(nprocs);

#else
#error No workload counter defined!
#endif
}

static void
init_clock_jitter(uint64_t work_ticks, uint64_t **work_ticks_jittered_ret)
{
	int ii;
	const double first_mult = 1 - CLOCK_JITTER_EPSILON;
	const double last_mult = 1 + CLOCK_JITTER_EPSILON;
	const double delta = (last_mult - first_mult) /
		(CLOCK_JITTER_NTICKS - 1);
	double mult = first_mult;
	uint64_t *work_ticks_jittered;

	work_ticks_jittered = *work_ticks_jittered_ret =
		malloc(CLOCK_JITTER_NTICKS * sizeof(uint64_t));

	/* initialize jitter samples s.t. they average to work_ticks
	*/
	for (ii = 0; ii < CLOCK_JITTER_NTICKS; ii++) {
		work_ticks_jittered[ii] = work_ticks * mult;
		mult += delta;
	}

	/* randomly shuffle jitter samples
	*/
	for (ii = 0; ii < CLOCK_JITTER_NTICKS; ii++) {
		long to;
		uint64_t save;

		to = lrand48() % CLOCK_JITTER_NTICKS;
		save = work_ticks_jittered[ii];
		work_ticks_jittered[ii] = work_ticks_jittered[to];
		work_ticks_jittered[to] = save;
	}
}

static thread_info *
init_threads(void)
{
  thread_info *threads;
  int threadn;

  threads = (thread_info*) malloc(sizeof(thread_info) * nthreads);
  if (threads == NULL)
    {
      perror("malloc");
      exit(1);
    }
  memset(threads, 0, sizeof(thread_info) * nthreads);

  for (threadn = 0; threadn < nthreads; threadn++)
    {
      threads[threadn].num = threadn;
      threads[threadn].op = op;
      threads[threadn].nops = 0;
      threads[threadn].fops = 0;

      if (mustrun >= 0)
        threads[threadn].proc = mustrun + threadn;
      else
        threads[threadn].proc = -1;

	if (threads[threadn].proc != -1)
		threads[threadn].work_ticks =
			work_ticks_per_proc[threads[threadn].proc];
	else
		threads[threadn].work_ticks = work_ticks_per_proc[0];

	init_clock_jitter(threads[threadn].work_ticks,
		&threads[threadn].work_ticks_jittered);
    }

  return threads;
}

static void
create_children(thread_info *threads, int nchildren)
{
  int childn;
  int threadn;

  /* SPECIAL CASE -- first thread */
  threads[0].pid = getpid();

  for (childn = 0; childn < nchildren; childn++)
    {
      threadn = childn + 1;

      if ((threads[threadn].pid =
           sproc(run_once, PR_SALL, &threads[threadn])) < 0)
        {
          perror("sproc");
          exit(1);
        }
    }
}

int
main(int argc, char *argv[])
{
  int childn;
  int nchildren;
  int threadn;

  srand48(time(NULL)|getpid());

  sharedp = (uint64_t*) valloc(getpagesize());
  sharedp = (uint64_t*) ((__psunsigned_t) sharedp + 128);

  *sharedp = 0;  /* place page on first processor */

  parseArguments(argc, argv);

  if (mlockall(MCL_CURRENT|MCL_FUTURE) < 0)
     perror("mlockall()");

  init_timing();

  /* to create more than 8 sprocs */
  usconfig(CONF_INITUSERS, (nthreads+8));

  nchildren = nthreads - 1;
  nready = 0;

  ops[op].init();

  g_threads = init_threads();
  create_children(g_threads, nchildren);

  run_once(&g_threads[0]);

  return 0;
}

static void
catch_alarm(int sign)
{
  int threadn;
  thread_info *this_thread;
  pid_t pid = getpid();

  for (threadn = 0; threadn < nthreads; threadn++)
    if (g_threads[threadn].pid == pid)
      {
        this_thread = &g_threads[threadn];
        break;
      }

  if (threadn == nthreads)
    {
      fprintf(stderr, "ERROR: Failed to find thread in threads list.\n");
      abort();
    }

  endTiming(&this_thread->pi);

  if (this_thread->num == 0)
    {
      while (wait(NULL) >= 0 || errno != ECHILD)
        continue;

      printThreadTimingHdr();
      for (threadn = 0; threadn < nthreads; threadn++)
        {
          printThreadTiming(g_threads[threadn].proc, &g_threads[threadn].pi,
                            g_threads[threadn].nops, g_threads[threadn].fops,
			    work_usecs);
        }

      if (g_threads != NULL)
        {
          free(g_threads);
          g_threads = NULL;
        }
    }

  exit(EXIT_SUCCESS);
}

static void
run_once(void *arg)
{
  thread_info *this_thread = (thread_info*) arg;

  if (this_thread->proc >= 0)
    try_bind(this_thread->proc);

  signal(SIGALRM, catch_alarm);

  barrier_wait(nready, nthreads);

  alarm(nsecs);
  startTiming(&this_thread->pi);

  ops[this_thread->op].run(this_thread);

  /* SPROC EXITS */
}


/*
 *	Command line argument processing
 *	================================
 */
static void
parseArguments(int argc, char *argv[])
{
    int          ch;
    extern char *optarg;
    extern int   optind;

    myname = strrchr(argv[0], '/');
    if (myname != NULL)
	myname++;
    else
	myname = argv[0];

    while ((ch = getopt(argc, argv, "chn:m:t:o:w:z:")) != -1)
	switch ((char)ch)
	{
	    default:
	    case '?':
		fprintf(stderr, "%s: unknown option -%c\n", myname, ch);
		/*FALLTHROUGH*/

	    case 'h':
		usage();
		/*NOTREACHED*/

	    case 'c':
		calibrating_timer = 1;
		break;

	    case 'n':
		nsecs = atoi(optarg);
		break;

	    case 'm':
		mustrun = atoi(optarg);
		break;

            case 't':
		nthreads = atoi(optarg);
		break;

	    case 'z':
		magic_ticks_per_usec = strtod(optarg, NULL);
		break;

	    case 'o':
		for (op = 0; op < nops; op++)
		  if (strcmp(ops[op].op_name, optarg) == 0)
		    break;

		if (op == nops) {
		  fprintf(stderr, "%s: unknown test type \"%s\"\n",
			  myname, optarg);
		  usage();
		  /*NOTREACHED*/
		}
		break;

	    case 'w':
		work_usecs = strtod(optarg, NULL);
		break;
	}
}

static void
usage(void)
{
    fprintf(stderr, "usage: %s\n"
	    "\t[-n numsecs] [-m mustrun] [-t nthreads]\n"
	    "\t[-o fetch_add|load|null]\n"
	    "\t[-w work_usecs] [-c] [-z ticks_per_usec]\n", myname);
    exit(EXIT_FAILURE);
}

/*
 *      Operation routines
 *      ==================
 */

void use_store_asm(volatile uint64_t *sharedp, uint64_t *failp,
		   uint64_t *sucp,
		   uint64_t *work_ticks, volatile uint64_t *counterp);

decl_op_init(store)
{
}

decl_op(store)
{
	use_store_asm(sharedp, &this_thread->fops, &this_thread->nops,
		this_thread->work_ticks_jittered, counter);
}

void use_fetch_and_add_asm(volatile unsigned long *sharedp, uint64_t *failp,
                           uint64_t *sucp,
			   uint64_t *work_ticks, volatile uint64_t *counterp);

decl_op_init(fetch_add)
{
}

decl_op(fetch_add)
{
	use_fetch_and_add_asm((unsigned long*) sharedp,
		&this_thread->fops, &this_thread->nops,
		this_thread->work_ticks_jittered, counter);
}

void use_load_asm(volatile uint64_t *sharedp, uint64_t *failp,
		   uint64_t *sucp,
		   uint64_t *work_ticks, volatile uint64_t *counterp);

decl_op_init(load)
{
}

decl_op(load)
{
	use_load_asm(sharedp, &this_thread->fops, &this_thread->nops,
		this_thread->work_ticks_jittered, counter);
}

void use_fail_asm(volatile uint64_t *sharedp, uint64_t *failp,
		   uint64_t *sucp,
		   uint64_t *work_ticks, volatile uint64_t *counterp);

decl_op_init(fail)
{
}

decl_op(fail)
{
  use_fail_asm(sharedp, &this_thread->fops, &this_thread->nops,
 	 this_thread->work_ticks_jittered, counter);
}

void use_null_asm(volatile uint64_t *sharedp, uint64_t *failp,
		   uint64_t *sucp,
		   uint64_t *work_ticks, volatile uint64_t *counterp);

decl_op_init(null)
{
}

decl_op(null)
{
  use_null_asm(sharedp, &this_thread->fops, &this_thread->nops,
 	 this_thread->work_ticks_jittered, counter);
}

#ifdef SN0
decl_op_init(fetchop_fetch_add)
{
  volatile fetchop_var_t *fetchop_var;
  fetchop_reservoir_t res;
  fetchop_var_t val_after, val_before;

  res = fetchop_init(USE_DEFAULT_PM, 1);
  fetchop_var = fetchop_alloc(res);

  /* fake out use_load_asm to use fetchop memory */
  sharedp = (volatile uint64_t*) ((__psunsigned_t) fetchop_var +
	(1L<<3)); /* fetchop atomic increment op */

  /* make sure we're really using fetchop memory */
  val_before = *sharedp;
  val_after = *fetchop_var;

  if (val_before >= val_after) {
	fprintf(stderr, "before = %lld, after = %lld\n",
		(long long) val_before, (long long) val_after);
	fprintf(stderr, "fetchop_init() gave non-fetchop memory!\n");
	exit(EXIT_FAILURE);
  }
}

decl_op(fetchop_fetch_add)
{
  use_load_asm(sharedp, &this_thread->fops, &this_thread->nops,
 	 this_thread->work_ticks_jittered, counter);
}
#endif /* SN0 */

/*
 *	Timing routines
 *	===============
 */
static void
startTiming(perfInfo *pi)
{
    pi->clk = times(&pi->tms);
    getrusage(RUSAGE_SELF, &pi->ru);
}


static void
endTiming(perfInfo *pi)
{
    perfInfo pN;

    pN.clk = times(&pN.tms);
    getrusage(RUSAGE_SELF, &pN.ru);

    pi->clk = pN.clk - pi->clk;
    pi->tms.tms_utime  = pN.tms.tms_utime  - pi->tms.tms_utime;
    pi->tms.tms_stime  = pN.tms.tms_stime  - pi->tms.tms_stime;
    pi->tms.tms_cutime = pN.tms.tms_cutime - pi->tms.tms_cutime;
    pi->tms.tms_cstime = pN.tms.tms_cstime - pi->tms.tms_cstime;

    pi->ru.ru_utime.tv_sec  = pN.ru.ru_utime.tv_sec - pi->ru.ru_utime.tv_sec;
    pi->ru.ru_utime.tv_usec = pN.ru.ru_utime.tv_usec - pi->ru.ru_utime.tv_usec;
    if (pi->ru.ru_utime.tv_usec < 0) {
	pi->ru.ru_utime.tv_usec += 1000000;
	pi->ru.ru_utime.tv_sec  -= 1;
    }
    pi->ru.ru_minflt   = pN.ru.ru_minflt - pi->ru.ru_minflt;
    pi->ru.ru_majflt   = pN.ru.ru_majflt - pi->ru.ru_majflt;
    pi->ru.ru_nswap    = pN.ru.ru_nswap - pi->ru.ru_nswap;
    pi->ru.ru_inblock  = pN.ru.ru_inblock - pi->ru.ru_inblock;
    pi->ru.ru_oublock  = pN.ru.ru_oublock - pi->ru.ru_oublock;
    pi->ru.ru_msgsnd   = pN.ru.ru_msgsnd - pi->ru.ru_msgsnd;
    pi->ru.ru_msgrcv   = pN.ru.ru_msgrcv - pi->ru.ru_msgrcv;
    pi->ru.ru_nsignals = pN.ru.ru_nsignals - pi->ru.ru_nsignals;
    pi->ru.ru_nvcsw    = pN.ru.ru_nvcsw - pi->ru.ru_nvcsw;
    pi->ru.ru_nivcsw   = pN.ru.ru_nivcsw - pi->ru.ru_nivcsw;
}


static void
printTiming(const char *name, perfInfo *pi, unsigned long units)
{
    long   clk_tck = sysconf(_SC_CLK_TCK);
    double wall    = (double)pi->clk / clk_tck;
    double usercpu = pi->ru.ru_utime.tv_sec +
      pi->ru.ru_utime.tv_usec/1000000.0;
    double syscpu  = pi->ru.ru_stime.tv_sec +
      pi->ru.ru_stime.tv_usec/1000000.0;

    printf("\n%s timing:\n"
	   "wall      (seconds)  = %.2f\n"
	   "user cpu  (seconds)  = %.2f\n"
	   "sys  cpu  (seconds)  = %.2f\n"
	   "\n"
	   "units (N)            = %lu\n"
	   "wall/units     (us)  = %.2f\n"
	   "user cpu/unit  (us)  = %.2f\n"
	   "sys  cpu/unit  (us)  = %.2f\n"
	   "\n"
	   "page reclaims        = %ld\n"
	   "page faults          = %ld\n"
	   "vol ctxswtchs        = %ld\n"
	   "invol ctxswtchs      = %ld\n",
           name,
	   wall, usercpu, syscpu,
           units,
	   wall/units*1000000, usercpu/units*1000000, syscpu/units*1000000,
	   pi->ru.ru_minflt, pi->ru.ru_majflt,
	   pi->ru.ru_nvcsw, pi->ru.ru_nivcsw);
}

static void
printThreadTimingHdr()
{
  printf("#%4s "
         "%10s %10s %10s "
         "%10s %10s "
         "%12s %12s %12s "
         "%10s %10s %10s %10s "
	 "%10s "
	 "\n",
         "proc",
         "wall (s)", "user (s)", "sys (s)",
         "nops", "fops",
         "wall/op (us)", "user/op (us)", "sys/op (us)",
         "pgrec", "pgflt", "vcsw", "ivcsw",
	 "work (us)");
}

static void
printThreadTiming(int procn, perfInfo *pi, uint64_t units,
                  uint64_t failed_units, double work_usecs)
{
  long   clk_tck = sysconf(_SC_CLK_TCK);
  double wall    = (double)pi->clk / clk_tck;
  double usercpu = pi->ru.ru_utime.tv_sec +
    pi->ru.ru_utime.tv_usec/1000000.0;
  double syscpu  = pi->ru.ru_stime.tv_sec +
    pi->ru.ru_stime.tv_usec/1000000.0;

  printf("%4d "
         "%10.2f %10.2f %10.2f "
         "%10llu %10llu "
         "%12.2f %12.2f %12.2f "
         "%10ld %10ld %10ld %10ld "
	 "%10.2f "
         "\n",
         (int) procn,
         wall, usercpu, syscpu,
         units, failed_units,
         wall/units*1000000, usercpu/units*1000000, syscpu/units*1000000,
         pi->ru.ru_minflt, pi->ru.ru_majflt,
         pi->ru.ru_nvcsw, pi->ru.ru_nivcsw,
	 work_usecs);
}
