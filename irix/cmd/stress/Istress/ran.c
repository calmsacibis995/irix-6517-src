static char ran_c[] = "@(#)ran.c	1.1";

#include <stdio.h>
#include <errno.h>
#include <signal.h>

double atof();
unsigned atoi();
int pid[100], npid = 0;		/* list of children's pids to be cleaned up */
int parentpid = 0;		/* parent pid - to enable sigint cleanup */
int suppressecho = 0;		/* set if entering sigint from internal erc */

void
sigint()
{
	register int i;

	signal (SIGINT, SIG_IGN);
	if (getpid() == parentpid) {
		if ( ! suppressecho)
			printf("\n");
		for (i=0; i<npid; i++)
			kill (pid[i], SIGINT);
		waitforchildren();
	}
	exit (0);
}

waitforchildren()
{
	for(;;){
		int stat, w = wait(&stat);
		extern int errno;

		if (w == -1 && errno == ECHILD) {
			printf("\n");
			exit(0);
		}
	}
}

usage(name)
 char *name;
{
	printf("%s -nNPROC -mMEMSIZE(megabytes) -pNPASSES\n", name);
	exit(1);
}

#define FIFOLENGTH 100
int nproc = 4;
double memsize = 1.0;
unsigned npasses = 10 * FIFOLENGTH;


main(argc,argv)
 int argc;
 char *argv[];
{
	int c;
	extern int optind;
	extern char *optarg;
	int *begin, *end, *sbrk(), i, p;
	int membytes;

	parentpid = getpid();
	while ((c = getopt (argc, argv, "n:m:p:")) != EOF)
	switch (c) {
		case 'n':
			nproc = atoi(optarg);
			break;
		case 'm':
			memsize = atof(optarg);
			break;
		case 'p':
			npasses = atoi(optarg);
			break;
		case '?':
			usage(argv[0]);
	}
	if (optind < argc)
		usage(argv[0]);
	if (nproc < 1 || nproc > 20) {
		printf("NPROC must be between 1 and 20.\n");
		usage(argv[0]);
	}
	if (memsize < 0.1 || memsize > 32.0) {
		printf("MEMSIZE must be between 0.1 and 32.0.\n");
		usage(argv[0]);
	}
	membytes = memsize * 1024 * 1024;
	begin = sbrk(membytes);
	end = sbrk(0);
	if(begin == (int *)-1L || end == (int *)-1L || end-begin != membytes/4) {
		suppressecho = 1;
		perror ("sbrk");
		sigint();
	}
	printf("%s -n%d -m%.1f -p%d\n", argv[0], nproc, memsize, npasses);

	signal (SIGINT, sigint);

	for (i=0; i<nproc; i++)
	switch (p=fork()) {
		case -1:
			suppressecho = 1;
			perror("fork");
			sigint();
		default:
			pid[npid++] = p;
			continue;
		case 0:
			signal (SIGINT, SIG_DFL);
			rantest(begin, end, i);
			exit(0);
	}
	waitforchildren();
}

/*
 * Following fifo holds copies of the random values, and the random
 * addresses containing those values, for double checking later.
 */
struct {
	int *f_addr;
	int  f_value;
	int  f_chunck;
} f[FIFOLENGTH], *fp = &f[0], *ftop = &f[FIFOLENGTH];

#define L2NCHUNCKS 10
#define NCHUNCKS (1<<L2NCHUNCKS)
char inuse[NCHUNCKS];

rantest(begin, end, sequence)
 register int *begin, *end;	/* endpoints of memory to be tested */
 int sequence;			/* sequence # of this Test Program */
{
	register int *addr;	/* random address to place value at */
	register int value;	/* random value to write there */
	register int x;		/* double check values read back here */
	register unsigned n;	/* number of passes made so far */
	register sizechunck = (end - begin) >> L2NCHUNCKS;
	register int chunck;	/* which chunck testing this pass */

	srand(sequence);/* seed each process differently, but repeatably */

	for (n = 0; n != npasses; n++) {
		if ((addr = fp->f_addr) != 0) {
			value = fp->f_value;
			if ((x = *addr) != value)
				fail(x, value, n, sequence);
			inuse[fp->f_chunck] = 0;
		}
		do {
			chunck = rand() & (NCHUNCKS-1);
			addr = begin + sizechunck * chunck;
		} while (addr >= end || inuse[chunck] != 0);
		fp->f_addr = addr;
		fp->f_value = value = (rand() << 24) | (int)(long)addr;
		fp->f_chunck = chunck;
		*addr = value;
		inuse[chunck] = 1;
		if ((n&0xf) == 0) getpid();
		if (++fp == ftop) fp = &f[0];
	}
}

fail(newvalue, oldvalue, pass, sequence)
 register int newvalue, oldvalue;	/* double check values as read back */
 register unsigned pass;	/* which pass did we fail on? */
 register sequence;		/* which one of the test progs are we? */
{
	printf("\nTest Program #%d, pass %d addr %x wanted %x got %x ",
		sequence, pass, fp->f_addr, oldvalue, newvalue);
	exit (1);
}
