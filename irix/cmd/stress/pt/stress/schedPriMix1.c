/*
** Purpose: Exercise scheduling of pthreads with priorities
**
** Details:
**	Threads wait on a condition until initial thread changes
**	their priorities and broadcasts the condition.
**	Each thread keeps track of when it ran.
**	Flip priorities of all threads at regular intervals.
**	Optionally flip the process scope kernel thread priorities.
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

typedef struct sample {
	int		id;
	volatile int	hiNow;
	char		scope;
	int		lo;
	int		hi;
} sample_t;

char			*priMessage[] = {
				"Running at priority low",
				"Running at priority HIGH" };

volatile int		mustStop;	/* set by master */
volatile int		stoppedSoFar;	/* incr by slaves */
pthread_mutex_t		waitMtx = PTHREAD_MUTEX_INITIALIZER;	/* slave */
pthread_cond_t		waitCv = PTHREAD_COND_INITIALIZER;	/* slave */

int			flipProcessScope;


main(int argc, char *argv[])
{
#define	ARGS	"b:c:pr:s:t::O:U"
#define	USAGE	"Usage: %s [-b #][-c #][-p][-r #][-s #][-t #][-U(sage)]\n"
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
		case 'p'	:
			flipProcessScope = 1;
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
				"\t-p #\t- flip process scope [%d]\n"
				"\t-r #\t- priority changes [%d]\n"
				"\t-s #\t- system scope threads [%d]\n"
				"\t-t #\t- threads [%d]\n"
				"\t-U\t- usage message\n",
				flipProcessScope,
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
	pthread_t	*ptIds;
	sample_t	*ptData;
	pthread_attr_t	lopa;
	pthread_attr_t	hipa;
	int		p = 0;
	int		j;
	struct sched_param	losp;
	struct sched_param	hisp;
#define ODD(v)	((v)&1)

	void		*threadMain();

	losp.sched_priority = sched_get_priority_min(pol) + 1;
	hisp.sched_priority = losp.sched_priority + 10;
	ChkInt( pthread_setschedparam(pthread_self(), pol, &hisp), == 0 );
	if (flipProcessScope) {
		ChkInt( sched_setscheduler(0, pol, &losp), != -1 );
	}
	hisp.sched_priority--;

	ChkInt( pthread_attr_init(&hipa), == 0 );
	ChkInt( pthread_attr_setschedpolicy(&hipa, pol), == 0 );
	ChkInt( pthread_attr_setschedparam(&hipa, &hisp), == 0 );

	ChkInt( pthread_attr_init(&lopa), == 0 );
	ChkInt( pthread_attr_setschedpolicy(&lopa, pol), == 0 );
	ChkInt( pthread_attr_setschedparam(&lopa, &losp), == 0 );

	TstInfo("Main: SCHED %s test\n", polName);
	TstInfo("Main: Create threads\n");

	mustStop = 1;
	stoppedSoFar = 0;

	/* XXX cache contention
	 */
	ChkPtr( ptData = calloc(threads, sizeof(sample_t)), != 0 );
	ChkPtr( ptIds = malloc(threads * sizeof(pthread_t)), != 0 );

	if (sysThreads) {
		ChkInt( pthread_attr_setscope(&hipa, PTHREAD_SCOPE_SYSTEM),
			== 0 );
		ChkInt( pthread_attr_setscope(&lopa, PTHREAD_SCOPE_SYSTEM),
			== 0 );

		for (; p < sysThreads; p++) {
			ptData[p].id = p;
			ptData[p].scope = 'S';
			ChkInt( pthread_create(ptIds+p, ODD(p)?&hipa:&lopa, threadMain,
				 ptData + p), == 0 );
		}
	}

	if (p < threads) {
		ChkInt( pthread_attr_setscope(&hipa, PTHREAD_SCOPE_PROCESS),
			== 0 );
		ChkInt( pthread_attr_setscope(&lopa, PTHREAD_SCOPE_PROCESS),
			== 0 );
		for (; p < threads; p++) {
			ptData[p].id = p;
			ptData[p].scope = 'P';
			ChkInt( pthread_create(ptIds+p, ODD(p)?&hipa:&lopa, threadMain,
				 ptData + p), == 0 );
		}
	}

	for (j = 0; j < changes; j++) {

		for (;;) {
			ChkInt( pthread_mutex_lock(&waitMtx), == 0 );
			if (stoppedSoFar == threads) {
				break;
			}
			ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );
			sched_yield();
		}
		stoppedSoFar = 0;
		ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );
		mustStop = 0;

		TstInfo("Main: Updating priorities %d: ALL CHANGE\n", j);
		for (p = 0; p < threads; p++) {
			TstInfo("t%d%c hi %4d, lo %4d\n",
				ptData[p].id, ptData[p].scope,
				ptData[p].hi, ptData[p].lo);
			ptData[p].hiNow = ODD(j+p) ? 1 : 0;
			ChkInt( pthread_setschedparam(ptIds[p], pol, ODD(j+p)?&hisp:&losp),
				== 0 );
		}
		if (flipProcessScope) {
			ChkInt( sched_setscheduler(0, pol, ODD(j)?&hisp:&losp),
				!= -1 );
		}
		ChkInt( pthread_cond_broadcast(&waitCv), == 0 );
		sleep(2);
		mustStop = 1;
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
		TstInfo("t%d%c hi %4d, lo %4d\n",
			ptData[p].id, ptData[p].scope,
			ptData[p].hi, ptData[p].lo);
	}

	TstInfo("Main: done SCHED %s test\n", polName);

	free(ptIds);
	free(ptData);
}


void
threadResults(void *arg)
{
	ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );
}


void	*
threadMain(void *arg)
{
	volatile int		i;
	int			pol;
	struct sched_param	pri;
	sample_t		*stats = (sample_t *)arg;

	ChkInt( pthread_getschedparam(pthread_self(), &pol, &pri), == 0 );
	ChkExp( pol == policy, ("bad policy %d [%d]\n", pol, policy) );
	TstTrace("[0x%08lx]%3d threadMain Start: [%d]\n",
		pthread_self(), stats->id, pri.sched_priority);

	pthread_cleanup_push(threadResults, 0);
	for (;;) {

		while (mustStop) {
			ChkInt( pthread_mutex_lock(&waitMtx), == 0 );
			stoppedSoFar++;
			ChkInt( pthread_cond_wait(&waitCv, &waitMtx), == 0 );
			ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );
		}

		ChkInt( pthread_getschedparam(pthread_self(), &pol, &pri), == 0 );
		ChkExp( pol == policy,
			("bad policy %d [%d]\n", pol, policy) );

		if (stats->hiNow)
			stats->hi++;
		else
#ifndef	jph_hack
			stats->lo++;
#else	/* jph_hack */
{
	TstInfo("t%d: pri %d must %d stopped %d\n",
		stats->id, pri.sched_priority,
		mustStop, stoppedSoFar);
	stats->lo++;
}
#endif	/* jph_hack */

		TstTrace("[0x%08lx]%3d:[%d]%s\n",
			pthread_self(), stats->id, pri.sched_priority,
			priMessage[stats->hiNow]);

		if (spin) {
			for (i = 0; i < spin; i++);
		} else {
			sched_yield();
		}
	}

	/* NOTREACHED */
	pthread_cleanup_pop(0);

	pthread_exit(arg);
}
