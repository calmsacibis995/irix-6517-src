#include <stdio.h>

/*
 * stk - check out stack growth
 * with an argument it forks a child when stack gets greater than 1 segment
 * ASSUMPTION: 1 segment is <= 2Mb
 */
int rawfd;
char *tos;
int dofork = 0;
int chpid = -1;
#define ENOUGHSTACK	(2*1024*1024)

main(argc, argv)
 int argc;
 char **argv;
{
	auto int status;
        register int i;
        int cycle;

	setbuf(stdout, NULL);
	rawfd = open("/dev/rdsk/ips0d0s0", 0);
	tos = (char *) &argc;	/* close to top of stack */
	if (argc > 1) {
		dofork = 1;
		cycle = atoi(argv[1]);
		if (cycle == 0){
			printf("Usage: stk cycles\n"); 
			exit(-1);
		}
	} else
		cycle = 1000;
	for (i=0; i < cycle; i++) {
		subr(0);
		if (chpid == 0)
			/* we are the child - exit */
			exit(0);
		else if (chpid > 0) {
			/* parent - wait for child */
			if (wait(&status) != chpid)
				printf("wait returned ??\n");
			else
				if (status != 0)
					printf("child exitted with:%x\n",
						status);
		}
	}
}

subr(i)
 int i;
{
	char buf[0x400];
	extern int errno;
	int rv;
	if ((i % 100) == 0)
		printf("%d (%x) ", i, &i);
	if ((tos - (char *)&i) > (ENOUGHSTACK+0x2000)) {
		/*
		rv = read(rawfd, buf, 0x400);
		if (rv != 0x400)
			printf("\nread failed:rv:%d errno:%d\n", rv, errno);
		*/
		if (dofork) {
			printf("Forking!\n");
			if ((chpid = fork()) < 0) {
				/* failed */
				printf("fork failed:%d\n", errno);
			}
		}
		printf("\n%d:Going uP!!!\n", getpid());
		return;
	}
	subr(i+1);
}
