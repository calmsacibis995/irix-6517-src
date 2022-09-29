#include "stdio.h"
#include "getopt.h"
#include "math.h"
#include "mon.h"
#include "sys/types.h"
#include "errno.h"
#include "sys/times.h"
#include "stdlib.h"
#include "task.h"

/*
 * multi-processor sieve - shared address version
 * If compiled with -p does all sorts of profiling madness
 */

/* defaults if no options given */
#define MAXPROC 4
#define LIMIT 100

int Verbose = 0;
#ifndef HERTZES
#define HERTZES 100
#endif
#ifndef DOFLOAT
#define DOUBLE	double
#endif

char *list;		/* list to be sieved */

/* to keep track of spawned pids/threads */
int nprocs = MAXPROC;	/* # processes/threads */
int limit = LIMIT;	/* highest # to look for */
int rootlimit;
int mainplace = 0;	/* starting point */

extern int getnext(void);
extern void monphase(void), monon(void), monoff(void), sinit(void), slave(void);

int
main(int argc, char **argv)
{
	extern int _utrace;
	int print = 0;
	register int n;
	int c;
	struct tms tim;
	long stime, eltime;

	moncontrol(0);
	/*setbuf(stdout, NULL);*/
	setbuf(stderr, NULL);
	while ((c = getopt(argc, argv, "pvl:n:")) != EOF)
		switch (c) {
		case 'v':	/* verbose */
			_utrace = 1;
			Verbose++;
			break;
		case 'l':	/* limit */
			limit = atoi(optarg);
			break;
		case 'n':	/* nprocs */
			nprocs = atoi(optarg);
			break;
		case 'p':	/* print results */
			print = 1;
			break;
		case '?':
		default:
			fprintf(stderr, "Usage:msieve [-v][-p][-l max #][-n nprocs]\n");
			exit(-1);
		}

	m_set_procs(nprocs);
	printf("# slave processes:%d limit:%d\n", nprocs, limit);

	/* set up shared address space */
	list = malloc(limit * sizeof(*list));

	/* start up slaves & turn off profiling */
	m_fork(monoff);

	/* init array */
	m_fork(monphase);	/* first profile section */
	m_fork(sinit);
	m_fork(monoff);
	if (Verbose) 
		printf("all slaves initialized\n");

	mainplace = 1;
	rootlimit = (int)sqrt((DOUBLE)limit) + 1;	/* account for error */
	if (Verbose) 
		printf("rootlimit:%d\n", rootlimit);

	/*
	 * start timing here
	 */
	m_fork(monphase);
	stime = times(&tim);

	/* start real thing */
	m_fork(slave);

	/*
	 * stop timing here
	 */
	eltime = times(&tim) - stime;
	m_fork(monoff);

	m_kill_procs();
	printf("elapsed time (real):%d mS\n", (eltime * 1000) / HERTZES);

	/* print results */
	if (print) {
		for (n = 0; n < limit; n++) {
			if (list[n] == '*')
				printf("%d ", n);
		}
		printf("\n");
	} else {
		for (n = limit-1; n >= 0; n--) {
			if (list[n] == '*')
				break;
		}
		printf("last:%d\n", n);
	}
	return 0;
}

/*
 * slave process
 * Uses nproc & limit globals
 */
void
slave(void)
{
	register int stepsize, place;

	/* main algorithm */
	for (;;) {
		stepsize = getnext();
		if (stepsize > rootlimit)
			break;
		
		place = stepsize + stepsize;
		while (place < limit) {
			list[place] = ' ';
			place += stepsize;
		}
	}
}

/*
 * sinit - initialize sieve
 */
void
sinit(void)
{
	register int start, n;
	register int me = m_get_myid();		/* which processor am i */
	register int maxprocs = m_get_numprocs();

	/* first do our part in initialization */
	n = (limit + maxprocs - 1) / maxprocs;
	start = me * n;
	if ((start + n) >= limit)
		n = limit - start;
	while (n--)
		list[start++] = '*';
}

/*
 * getnext - get next number to search for
 */
int
getnext(void)
{
	register int i;

	m_lock();
	do {
		mainplace++;
	} while ((list[mainplace] == ' ') && (mainplace < rootlimit));
	i = mainplace;
	m_unlock();
	return(i);
}

void
monoff(void)
{
	moncontrol(0);
}

void
monon(void)
{
	moncontrol(1);
}

void
monphase(void)
{
	moncontrol(2);
}
