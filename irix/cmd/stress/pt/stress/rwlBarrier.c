/*
** Purpose: Beat up rwl locks
**
*/

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <Bnch.h>

typedef struct {
	pthread_rwlock_t	b_lock;
	int	b_limit;
	int	b_busy;
	int	b_spin;
	int	b_waiting;
	int	b_id;
} barrier_t;

barrier_t	*bar;
int		barMax = 1;
int		iterations = 100;


int
main(int argc, char *argv[])
{
	int		c, errflg = 0;
#define USAGE	"Usage: %s [-bcit #][-B rs][-T][-u(sage)]\n"

	char		*opts = 0;
	int		threads = 10;
	int		concurrency = 0;
	int		pshared = PTHREAD_PROCESS_PRIVATE;
#	define FAIR	1
#	define UNFAIR	2
#	define RANDOM	3
	int		fair = FAIR;
	uint_t		seed = (uint_t)getpid();
	int		p;
	int		i;
	int		timing = 0;
	BnchDecl(tStart);
	BnchDecl(tStop);
	pthread_t	pt;
	pthread_rwlockattr_t	rwlA;
	void		*threadMain(void *);

	optarg = NULL;
	while (!errflg && (c = getopt(argc, argv, "b:c:i:t:B:TO:u")) != EOF)
		switch (c) {
		case 'b'	:
			barMax = strtol(optarg, 0, 0);
			break;
		case 'B'	:
			if (strchr(optarg, 'u'))
				fair = UNFAIR;
			if (strchr(optarg, 'r'))
				fair = RANDOM;
			if (strchr(optarg, 's'))
				pshared = PTHREAD_PROCESS_SHARED;
			break;
		case 'c'	:
			concurrency = strtol(optarg, 0, 0);
			break;
		case 'i'	:
			iterations = strtol(optarg, 0, 0);
			break;
		case 't'	:
			threads = strtol(optarg, 0, 0);
			break;
		case 'T'	:
			timing = 1;
			break;
		case 'O'	:
			opts = optarg;
			break;
		case 'u'	:
			printf(USAGE, argv[0]);
			printf(
				"\t-b #\tbarriers (%d)\n"
				"\t-c #\tconcurrency (%d)\n"
				"\t-i #\titerations (%d)\n"
				"\t-t #\tthreads (%d)\n"
				"\t-B s\tshared(|private)\n"
				"\t   r|u\trandom|unfair(|fair)\n"
				"\t-u  \tusage message\n",
				barMax,
				concurrency,
				iterations,
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

	TstInfo("\nCreating %d threads, concurrency %d, iterations %d,"
		" %d %s barriers (%s)\n",
		threads, concurrency, iterations, barMax,
		fair == FAIR ? "fair"
			: (fair == RANDOM ? "random" : "unfair"),
		pshared == PTHREAD_PROCESS_PRIVATE ? "private" : "shared");

	if (concurrency) {
		ChkInt( pthread_setconcurrency(concurrency), == 0 );
	}

	/* Initialise the barriers
	 */
	ChkPtr( bar = malloc(sizeof(barrier_t) * barMax), != 0 );
	ChkInt( pthread_rwlockattr_init(&rwlA), == 0 );
	ChkInt( pthread_rwlockattr_setpshared(&rwlA, pshared), == 0 );

	for (i = 0; i < barMax; i++) {
		bar[i].b_busy = 0;
		bar[i].b_spin = 1;
		bar[i].b_waiting = 0;
		bar[i].b_id = i;
		if (fair == FAIR) {
			bar[i].b_limit = threads;
		} else if (fair == RANDOM) {
			bar[i].b_limit = (rand_r(&seed) % threads) + 1;
		} else {
			bar[i].b_limit = threads - (i % threads);
		}
		TstInfo("create 0x%p bar[%d] limit %d\n",
			&bar[i], i, bar[i].b_limit);
		ChkInt( pthread_rwlock_init(&bar[i].b_lock, &rwlA), == 0 );
	}

	if (timing) {
		BnchStamp(&tStart);	/* once for init */
		BnchStamp(&tStop);	/* once for init */
		BnchStamp(&tStart);
	}

	/* Start the threads
	 */
	for (p = 1; p < threads; p++) {
		ChkInt( pthread_create(&pt, 0, threadMain, (void *)p),
			== 0 );
	}

	threadMain(0);

	if (timing) {
		BnchStamp(&tStop);
		BnchDelta(&tStop, &tStart, &tStop);
		BnchPrint("Elapsed", &tStop);
	}

	ChkInt( pthread_rwlockattr_destroy(&rwlA), == 0 );
	pthread_exit(0);
}

/* ------------------------------------------------------------------ */


void *
threadMain(void *arg)
{
	int	id = (int)arg;
	int	i;
	int	b;
	void barrier(barrier_t *, int);

	for (i = 0; i < iterations; i++) {
		TstTrace("%d: iteration %d\n", id, i);
		for (b = 0; b < barMax; b++) {
			if (id >= bar[b].b_limit)
				continue;
			barrier(&bar[b], id);
		}
	}

	return (0);
}


/* Stolen from libc.
 */
void
barrier(barrier_t *bar, int id)
{
	int	busy;
	int	spin;

	TstTrace("%d/%d: enter bar\n", id, bar->b_id);
	for (;;) {
		ChkInt( pthread_rwlock_rdlock(&bar->b_lock), == 0 );
		busy = bar->b_busy;
		ChkInt( pthread_rwlock_unlock(&bar->b_lock), == 0 );
		if (!busy) break;
		sched_yield();
	}

	ChkInt( pthread_rwlock_wrlock(&bar->b_lock), == 0 );
	if (++bar->b_waiting == bar->b_limit) {
		TstTrace("%d/%d: bar release\n", id, bar->b_id);
		bar->b_busy = 1;
		bar->b_spin = 0;
	} else {
		ChkInt( pthread_rwlock_unlock(&bar->b_lock), == 0 );
		TstTrace("%d/%d: bar spin\n", id, bar->b_id);
		for (;;) {
			ChkInt( pthread_rwlock_rdlock(&bar->b_lock), == 0 );
			spin = bar->b_spin;
			ChkInt( pthread_rwlock_unlock(&bar->b_lock), == 0 );
			if (!spin) break;
			sched_yield();
		}
		ChkInt( pthread_rwlock_wrlock(&bar->b_lock), == 0 );
	}
	if (!--bar->b_waiting) {
		bar->b_busy = 0;
		bar->b_spin = 1;
	}
	ChkInt( pthread_rwlock_unlock(&bar->b_lock), == 0 );
	TstTrace("%d/%d: exit bar\n", id, bar->b_id);
}
