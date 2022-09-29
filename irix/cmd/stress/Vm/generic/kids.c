#include "stdio.h"
#include "signal.h"
#include "errno.h"
#include "malloc.h"
#include "sys/types.h"
#include "sys/prctl.h"

#ident "$Revision: 1.3 $"

/*
 * n - # bytes
 * s - # shared kids
 * p - # procs
 * g - # generations
 */

#define SZSTACK		7000	/* # bytes for on-stack arrays */
int nkids;		/* # of children */
int nskids;		/* # of sproc children */
int nbytes;
int ngen;
int *kidpids, *skidpids;
int chunksize = (240*1024);
int perc = 30;		/* percentage of memory to hit */
void killall(), dochild(), sslave(), playin(),Usage();

main(argc, argv)
int argc;
char *argv;
{
	extern void catchit();
	extern void slave();
	extern int getopt();
	extern char *optarg;
	int c;
	int gen = 0;
	int i;

	nkids = 1;
	nskids = 0;
	nbytes = 512*1024;
	ngen = 1;

	while ((c = getopt(argc, argv, "s:g:p:n:")) != EOF)
	switch (c) {
	case 's':
		nskids = atoi(optarg);
		break;
	case 'g':
		ngen = atoi(optarg);
		break;
	case 'p':
		nkids = atoi(optarg);
		break;
	case 'n':
		nbytes = atoi(optarg);
		break;
	default:
		Usage();
		break;
	}

	prctl(PR_SETSTACKSIZE, 64*1024*1024);
	signal(SIGINT, catchit);
	if ((kidpids = (int *)malloc(nkids * sizeof(int))) == NULL) {
		perror("kids:ERROR:malloc kidpids");
		exit(-1);
	}
	for (i = 0; i < nkids; i++)
		kidpids[i] = -1;

	if ((skidpids = (int *)malloc(nskids * sizeof(int))) == NULL) {
		perror("kids:ERROR:malloc skidpids");
		exit(-1);
	}
	for (i = 0; i < nskids; i++)
		skidpids[i] = -1;

	printf("Starting generation %d kids %d shared kids %d size %d bytes\n",
		gen, nkids, nskids, nbytes);

	
	/* fire up kids, have them wait till all are fired up */
	for (i = 0; i < nkids; i++) {
		kidpids[i] = fork();
		if (kidpids[i] < 0) {
			perror("kids:ERROR:fork");
			killall();
		} else if (kidpids[i] == 0) {
			dochild();
		}
		/* parent */
	}
	/* start up shared kids */
	for (i = 0; i < nskids; i++) {
		skidpids[i] = sproc(slave, PR_SALL);
		if (skidpids[i] < 0) {
			perror("kids:ERROR:sproc");
			killall();
		}
		/* parent */
	}

	/* unblock everybody */
	for (i = 0; i < nskids; i++)
		unblockproc(skidpids[i]);
	for (i = 0; i < nkids; i++)
		unblockproc(kidpids[i]);
	for (;;) {
		if (wait(0L) == -1) {
			if (errno == ECHILD)
				break;
		}
	}
	killall();
	/* NOTREACHED */
}

void killall()
{
	int i;

	for (i = 0; i < nskids; i++)
		if (skidpids[i] != -1)
			kill(skidpids[i], SIGKILL);
	for (i = 0; i < nkids; i++)
		if (kidpids[i] != -1)
			kill(kidpids[i], SIGKILL);
	exit(0);
}

void dochild()
{
	int alloced = 0;
	char *vaddr = NULL;
	char *svaddr;
	register int lnbytes;

	/* wait for parent */
	blockproc(getpid());

	/* alter nbytes a bit - rand gived 0 - 64k number */
	lnbytes = (rand() % (10*4096)) - (5*4096) + nbytes;

	/* now alloc memory */
	while (alloced < lnbytes) {
		if ((vaddr = (char *)malloc(chunksize)) == NULL) {
			printf("kids:ERROR:malloc failed for pid %d alloced %d\n",
				getpid(), alloced);
			exit(-1);
		}
		if (svaddr == NULL)
			svaddr = vaddr;
		alloced += chunksize;
		playin(vaddr, chunksize, perc);
	}
#if 0
	/* what the heck? */
	playin(svaddr, alloced, perc);
#endif
	exit(0);
}

void
slave()
{
	int nrecurse;

	/* wait for parent */
	blockproc(getpid());

	nrecurse = nbytes / SZSTACK;

	/* only way to use up space is by recursing */
	sslave(nrecurse);
	exit(0);
}

void sslave(d)
register int d;
{
	char b[SZSTACK];
	if (d > 0) {
		playin(b, SZSTACK, perc);
		sslave(d-1);
	} else {
		playin(b, SZSTACK, perc);
	}
}

/*
 * hit p percent of area within start addr and length
 */
void playin(s, n, p)
register char *s;
register int n;
int p;
{
	extern int lrand48();
	register int nhits;
	register int i;
	register char *addr;

	nhits = (n * p) / 100;
	/*printf("playin start 0x%x n %d nhits %d\n", s, n, nhits);*/

	for (i = 0; i < nhits; i++) {
		addr = s + (lrand48() % n);
		*addr = 0xbe;
	}
}

void
catchit()
{
	killall();
	exit(-1);
}

void Usage()
{
	fprintf(stderr, "Usage:kids [-n bytes][-s skids][-g generations][-p kids]\n");
	exit(-1);
}
