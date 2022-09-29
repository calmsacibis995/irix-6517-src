#ident "$Revision: 1.7 $"

#include "sys/types.h"
#include "task.h"
#include "limits.h"
#include "stdio.h"
#include "sys/times.h"
#include "signal.h"
#include "errno.h"

/*
 * test simple use of setblocktaskcnt
 */
#define Maxprocs()	prctl(PR_MAXPROCS)
int Pid;
int nprocs;
int iter;
char *Cmd;
volatile int slavego = 0;

main(argc, argv)
 int argc;
 char **argv;
{
	extern void slave();
	extern int errno;
	char *sp;
	struct tms tm;
	register int i, j, anyrun;
	int maxpthreads;		/* max threads per loop */

	if (argc < 3) {
		fprintf(stderr, "Usage:%s num_threads iterations\n", argv[0]);
		exit(-1);
	}
	Pid = getpid();
	nprocs = atoi(argv[1]);
	iter = atoi(argv[2]);
	Cmd = argv[0];

	usconfig(CONF_LOCKTYPE, US_DEBUGPLUS);
	maxpthreads = Maxprocs();
	if (nprocs > maxpthreads)
		nprocs = maxpthreads;
	printf("%s:Creating %d processes\n", Cmd, nprocs);
	signal(SIGCLD, SIG_IGN);

	for (i = 0; i < iter; i++) {
		slavego = 0;
		/* create the tasks */
		for (j = 1; j <= nprocs; j++) {
			if (taskcreate("slave", slave, 0, 0) < 0) {
				fprintf(stderr, "%s:taskcreate failed errno:%d\n", Cmd, errno);
				goto bad;
			}
		}

		/* created all threads so allow slaves to proceed */
		slavego++;
		/* block myself */
		for (j = 0; j < nprocs; j++)
			if (taskblock(0) < 0) {
				fprintf(stderr, "%s:ERROR:block failed!\n", Cmd);
				exit(-1);
			}
		/*
		if (tasksetblockcnt(0, -nprocs) < 0) {
			fprintf(stderr, "%s:ERROR:setcount failed!\n", Cmd);
			exit(-1);
		}
		*/

		for (j = 0; j < 20; j++)
			sginap(0);
again:
		anyrun = 0;
		for (j = 1; j < nprocs+1; j++) {
			if (taskctl(j, TSK_ISBLOCKED) >= 0) {
				if (anyrun == 0)
					printf("%s:loop:%d task %d ", Cmd, i, j);
				else
					printf("%s:%d ", Cmd, j);
				anyrun++;
			}
		}
		if (anyrun) {
			printf(" still running\n");
			goto again;
		}
	}
	printf("%s:master exitting\n", Cmd);
	exit(0);

bad:
	while (--j > 0)
		taskdestroy(j);
	printf("%s:master exitting\n", Cmd);
	exit(-1);
}

void
slave(arg)
{
	int rv;

	/* wait for main thread to go suspended or die */
	while (!slavego)
		sginap(0);
	/* wait for main thread to go suspended or die */
	while ((rv = taskctl(0, TSK_ISBLOCKED)) == 0)
		sginap(0);
	if (rv < 0)
		printf("%s:ERROR:slave %d bad status for tid 1\n", Cmd, get_tid());
	if (taskunblock(0) != 0)
		printf("%s:ERROR:slave %d bad status for unblock 0\n", Cmd, get_tid());
}
