#include "stdlib.h"
#include "unistd.h"
#include "sys/wait.h"
#include "stress.h"
#include "ulocks.h"
#include "mutex.h"
#include "getopt.h"

extern void playlock(void);
extern void do_something(void);
extern void do_something32(void);

char *Cmd;
unsigned long loc;
__uint32_t loc32;
void slave(void *);
int verbose;
int docnt = 1000000;

int
main(int argc, char **argv)
{
	int nloops = 10;
	int c, i; 

	Cmd = errinit(argv[0]);
	while ((c = getopt(argc, argv, "l:vp:")) != EOF) {
		switch (c) {
		case 'l':
			nloops = atoi(optarg);
			break;
		case 'p':
			docnt = atoi(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			break;
		}
	}

	if (sproc(slave, PR_SALL, nloops) < 0) {
		errprintf(3, "sproc");
		exit(0);
	}

	i = nloops;
	while (i--)
		playlock();

	wait(NULL);
	return 0;
}

void
slave(void *nloops)
{
	int i;

	i = (int)(__psint_t)nloops;
	while (i--)
		playlock();

	exit(0);
}

void
playlock(void)
{
	unsigned long oval;

	while (test_and_set(&loc, 1) == 1)
		;
	if (verbose)
		printf("%s:%d has location: %d\n", Cmd, get_pid(), loc);
	do_something();
	while (test_and_set(&loc, 0) == 0)
		;

	oval = docnt;
	while (oval--)
		;



	while (test_and_set32(&loc32, 1) == 1)
		;
	if (verbose)
		printf("%s:%d has location: %d\n", Cmd, get_pid(), loc);
	do_something32();
	while (test_and_set32(&loc32, 0) == 0)
		;

	oval = docnt;
	while (oval--)
		;
}

volatile int shared_variable;

void
do_something(void)
{
	volatile int i = docnt;

        shared_variable = docnt;

	while (i != 0) {
		shared_variable--;
		i--;
	}
	if (shared_variable != 0)
		printf("Error - %s:%d didn't have exclusive access\n",
			Cmd, getpid());
}

volatile int shared_variable32;

void
do_something32(void)
{
	volatile int i = docnt;

        shared_variable32 = docnt;

	while (i != 0) {
		shared_variable32--;
		i--;
	}
	if (shared_variable32 != 0)
		printf("Error - %s:%d didn't have exclusive access\n",
			Cmd, getpid());
}
