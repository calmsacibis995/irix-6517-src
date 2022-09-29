/*
** Purpose: Beat up mutexes and timed wait condition variables.
**
FIXME	add iteration count
	permit tweaking probabilites
	put back mutex priorities
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <Chk.h>

typedef struct sync {
	pthread_mutex_t	lock;
	pthread_cond_t	cv;
} sync_t;

sync_t	*syncTab;
int	syncMax = 5;
int	iterations = 100;

int	skipProb = 3;
int	wakeProb = 3;
int	waitProb = 5;

#define chance(seed, factor)	((rand_r(seed) % factor) == 0)


int
main(int argc, char *argv[])
{
	int		c, errflg = 0;
#define USAGE		"Usage: %s [-i #][-s #][-t #][-U(sage)]\n"

	char		*opts = 0;
	long		hz;
	int		threads = 10;
	int		p;
	int		i;
	pthread_t	pt;
	void		*threadMain(void *);

	optarg = NULL;
	while (!errflg && (c = getopt(argc, argv, "i:s:t:O:U")) != EOF)
		switch (c) {
		case 'i'	:
			iterations = strtol(optarg, 0, 0);
			break;
		case 's'	:
			syncMax = strtol(optarg, 0, 0);
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
				"\t-s #\tsynchronisers (%d)\n"
				"\t-t #\tthreads (%d)\n"
				"\t-U  \tusage message\n",
				syncMax,
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

	ChkPtr( syncTab = malloc(sizeof(sync_t) * syncMax), != 0 );
	for (i = 0; i < syncMax; i++) {
		ChkInt( pthread_mutex_init(&syncTab[i].lock, 0), == 0 );
		ChkInt( pthread_cond_init(&syncTab[i].cv, 0), == 0 );
	}

	while (--threads) {
		ChkInt( pthread_create(&pt, 0, threadMain, (void *)threads),
			== 0 );
	}

	threadMain(0);

	pthread_exit(0);
}

/* ------------------------------------------------------------------ */

void cvTWait(int, sync_t *, unsigned int *);

void *
threadMain(void *arg)
{
	int		j;
	int		l;
	int		id = (int)arg;
	unsigned int	seed = (unsigned int)&l;
	char		*lockHeld;	/* locks acquired in first pass */

	ChkPtr( lockHeld = calloc(sizeof(*lockHeld), syncMax), != 0 );

	for (j = 0; j < iterations; j++) {

		/* Pass1: taking locks, do wakeups and waits.
		 */
		for (l = 0; l < syncMax; l++) {

			if (chance(&seed, skipProb)) {
				continue;
			}

			ChkInt( pthread_mutex_lock(&syncTab[l].lock), == 0 );
			lockHeld[l] = 1;

			if (chance(&seed, wakeProb)) {
				TstTrace("(%d Wk %d)", id, l);
				ChkInt( pthread_cond_broadcast(&syncTab[l].cv), == 0 );
			}

			if (chance(&seed, waitProb)) {
				cvTWait(id, &syncTab[l], &seed);
			}
		}

		/* Pass2: drop locks.
		 */
		for (l = 0; l < syncMax; l++) {

			if (!lockHeld[l]) {
				continue;
			}

			lockHeld[l] = 0;
			ChkInt( pthread_mutex_unlock(&syncTab[l].lock),
				== 0 );
		}
	}

	return (0);
}


#define NSEC	1000000000

void
cvTWait(int id, sync_t *sid, unsigned int *seed)
{
	int		delay;
	struct timespec	wakeTime;
	struct timeval	timeNow;
	long		diff;
	int		ret;

	/* Compute delay: 0..7/8ths of a second */
	delay = (rand_r(seed) & 3) * (NSEC / 8);

	TstTrace("(%d Wt %d@%dus)", id, sid - syncTab, (int)(delay/1000));

	ChkInt( gettimeofday(&timeNow, 0), == 0 );

	wakeTime.tv_sec = timeNow.tv_sec;
	wakeTime.tv_nsec = timeNow.tv_usec * 1000 + delay;
	if (wakeTime.tv_nsec >= NSEC) {
		wakeTime.tv_nsec -= NSEC;
		wakeTime.tv_sec++;
	}

	ret = pthread_cond_timedwait(&sid->cv, &sid->lock, &wakeTime);

	ChkExp( ret == 0 || ret == ETIMEDOUT, ("Bad return cvwait %d\n", ret) );
}
