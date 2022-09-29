/*
** Purpose: Exercise time sharing of pthreads with priorities
**
** Details:
**	Create threads, half with high priority and half with low.
**	Threads spin printing a message depending on their priority.
**	Initial thread is highest priority and periodically runs changing
**	the priorities of the high threads to low and low to high.
**	Odd threads start high, even start low.
**
**	Use at least 2 * NCPU pthreads on an MP.
**	Set -o to IT to see trace.
*/

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <Tst.h>


int	spin = 0;	/* cpu spin */
int	changes = 20;
int	policy = -1;
int	sysThreads;
int	threads;
int	concurrency;

#define LOMSG	"Running at priority low"
#define HIMSG	"Running at priority HIGH"

pthread_t		*ptIds;
char			**pthreadMsg;
struct sched_param	losp;
struct sched_param	hisp;

main(int argc, char *argv[])
{
#define	ARGS	"b:c:r:s:t::O:U"
#define	USAGE	"Usage: %s [-b #][-c #][-r #][-s #][-t #][-U(sage)]\n"
	int		c, errflg = 0;

	char		*opts = 0;
	void		*schedRunAll();

	threads = 2 * sysconf(_SC_NPROC_ONLN);
	optarg = NULL;
	while (!errflg && (c = getopt(argc, argv, ARGS)) != EOF)
		switch (c) {
		case 'b'	:
			spin = strtol(optarg, 0, 0);
			break;
		case 'c'	:
			concurrency = strtol(optarg, 0, 0);
			break;
		case 'r'	:
			changes = strtol(optarg, 0, 0);
			break;
		case 's'	:
			sysThreads = strtol(optarg, 0, 0);
			if (sysThreads <= 0) {
				sysThreads = threads;
			}
			break;
		case 't'	:
			threads = strtol(optarg, 0, 0);
			break;
		case 'O'	:
			opts = optarg;
			break;
		case 'U'	:
			printf(USAGE, argv[0]);
			printf(
				"\t-b #\t- cpu spin for thread [yield]\n"
				"\t-c #\t- concurrency [dynamic]\n"
				"\t-r #\t- priority changes [%d]\n"
				"\t-s #\t- system scope threads [%d]\n"
				"\t-t #\t- threads [%d]\n"
				"\t-U\t- usage message\n",
				changes,
				sysThreads,
				threads
			);
			exit(0);
		default :
			errflg++;
		}
	if (errflg || optind < argc) {
		fprintf(stderr, USAGE, argv[0]);
		exit(0);
	}
	TstSetOutput(opts);

	TstInfo("threads %d [P%d/S%d], changes %d, spins: %d, llism %d\n\n",
		threads, threads - sysThreads, sysThreads,
		changes, spin, concurrency);

	ChkPtr( ptIds = malloc(threads * sizeof(pthread_t)), != 0 );
	ChkPtr( pthreadMsg = malloc(threads * sizeof(char *)), != 0 );

	if (concurrency) {
		ChkInt( pthread_setconcurrency(concurrency), == 0 );
	}

	if (sysThreads) {
		pthread_attr_t	attr;
		pthread_t	p;
		ChkInt( pthread_attr_init(&attr), == 0 );
		ChkInt( pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED),
			== 0 );
		ChkInt( pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM),
			== 0 );

		ChkInt( pthread_create(&p, &attr, schedRunAll, 0), == 0 );
	} else {
		schedRunAll(0);
	}

	pthread_exit(0);
}


void *
schedRunAll(void *arg)
{
	void schedRunPolicy(int, char *);

	schedRunPolicy(policy = SCHED_TS, "TS");
	schedRunPolicy(policy = SCHED_RR, "RR");
	schedRunPolicy(policy = SCHED_FIFO, "FIFO");
	return (0);
}


void
schedRunPolicy(int pol, char *polName)
{
	pthread_attr_t	lopa;
	pthread_attr_t	hipa;
	int		p = 0;
	int		j;

	void		*threadMain();

	losp.sched_priority = sched_get_priority_min(pol) + 1;
	hisp.sched_priority = losp.sched_priority + 10;
	ChkInt( pthread_setschedparam(pthread_self(), pol, &hisp), == 0 );
	hisp.sched_priority--;

	ChkInt( pthread_attr_init(&hipa), == 0 );
	ChkInt( pthread_attr_setschedpolicy(&hipa, pol), == 0 );
	ChkInt( pthread_attr_setschedparam(&hipa, &hisp), == 0 );

	ChkInt( pthread_attr_init(&lopa), == 0 );
	ChkInt( pthread_attr_setschedpolicy(&lopa, pol), == 0 );
	ChkInt( pthread_attr_setschedparam(&lopa, &losp), == 0 );

	TstInfo("Main: SCHED %s test\n", polName);
	TstInfo("Main: Create threads\n");

	if (sysThreads) {
		ChkInt( pthread_attr_setscope(&hipa, PTHREAD_SCOPE_SYSTEM),
			== 0 );
		ChkInt( pthread_attr_setscope(&lopa, PTHREAD_SCOPE_SYSTEM),
			== 0 );

		for (; p < sysThreads; p++) {
			pthreadMsg[p] = (p&1) ? HIMSG : LOMSG;
			ChkInt( pthread_create(ptIds+p, (p&1)?&hipa:&lopa, threadMain,
				 ptIds+p), == 0 );
		}
	}

	if (p < threads) {
		ChkInt( pthread_attr_setscope(&hipa, PTHREAD_SCOPE_PROCESS),
			== 0 );
		ChkInt( pthread_attr_setscope(&lopa, PTHREAD_SCOPE_PROCESS),
			== 0 );
		for (; p < threads; p++) {
			pthreadMsg[p] = (p&1) ? HIMSG : LOMSG;
			ChkInt( pthread_create(ptIds+p, (p&1)?&hipa:&lopa, threadMain,
				 ptIds+p), == 0 );
		}
	}

	for (j = 1; j < changes; j++) {
		TstInfo("Main: Update done.  Sleeping...\n");
		sleep(2);
		TstInfo("Main: Updating priorities %d: ALL CHANGE\n", j);
		for (p = 0; p < threads; p++) {
			pthreadMsg[p] = ((j+p)&1) ? HIMSG : LOMSG;
			ChkInt( pthread_setschedparam(ptIds[p], pol, ((j+p)&1)?&hisp:&losp),
				== 0 );
		}
	}

	TstInfo("Main: Kill all threads...\n");
	for (p = 0; p < threads; p++) {
		ChkInt( pthread_cancel(ptIds[p]), == 0 );
	}

	TstInfo("Main: Join all threads...\n");
	for (p = 0; p < threads; p++) {
		void	*ret;

		ChkInt( pthread_join(ptIds[p], &ret), == 0 );
		ChkExp( ret == PTHREAD_CANCELED,
			("bad join %#x %#x\n", ret, PTHREAD_CANCELED) );
	}

	TstInfo("main Done\n");
}


struct sample {
	int	id;
	int	lo;
	int	hi;
};


void
threadResults(void *arg)
{
	struct sample *r = (struct sample *)arg;
	TstInfo("t%d hi %4d, lo %4d\n", r->id, r->hi, r->lo);
}


void	*
threadMain(void *arg)
{
	int		id = (pthread_t *)arg - ptIds;
	volatile int	i;
	int			pol;
	struct sched_param	pri;
	struct sample		local;

	local.id = id;
	local.lo = 0;
	local.hi = 0;

	ChkInt( pthread_getschedparam(pthread_self(), &pol, &pri), == 0 );
	ChkExp( pol == policy, ("bad policy %d [%d]\n", pol, policy) );
	TstTrace("[0x%08lx]%3d threadMain Start: [%d]%s\n",
		pthread_self(), id, pri.sched_priority, pthreadMsg[id]);

	/* Use of cancellation is a bit of a stretch.
	 */
	pthread_cleanup_push(threadResults, &local);
	for (;;) {
		ChkInt( pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0),
			== 0 );

		ChkInt( pthread_getschedparam(pthread_self(), &pol, &pri), == 0 );
		ChkExp( pol == policy,
			("bad policy %d [%d]\n", pol, policy) );

		if (pri.sched_priority == hisp.sched_priority)
			local.hi++;
		else
			local.lo++;

		TstTrace("[0x%08lx]%3d:[%d]%s\n",
			pthread_self(), id, pri.sched_priority, pthreadMsg[id]);

		if (spin) {
			for (i = 0; i < spin; i++);
		} else {
			sched_yield();
		}

		ChkInt( pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0),
			== 0 );
		pthread_testcancel();
	}

	/* NOTREACHED */
	pthread_cleanup_pop(0);

	pthread_exit(arg);
}
