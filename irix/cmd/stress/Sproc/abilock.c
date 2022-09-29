#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "sys/wait.h"
#include "abi_mutex.h"
#include "mutex.h"
#include "errno.h"
#include "stress.h"
#include "ulocks.h"
#include "getopt.h"
#include "sys/times.h"

char *Cmd;
abilock_t lck;
void slave(void *), slave2(void *);
void playlock(int which);
void do_something(void);

int inlock;
int doloops = 10000000;
#define ACQ 1
#define SPIN 2

int
main(int argc, char **argv)
{
	auto unsigned long tst;
	unsigned long rv;
	auto __uint32_t tst32;
	__uint32_t rv32;
	int nloops = 10;
	int nsloops = 300000;
	int waitfor = 0;
	int c; 
	long i, start;
	struct tms tm;

	Cmd = errinit(argv[0]);
	while ((c = getopt(argc, argv, "l:wd:")) != EOF) {
		switch (c) {
		case 'd':
			doloops = atoi(optarg);
			break;
		case 'l':
			nloops = atoi(optarg);
			break;
		case 'w':
			waitfor = 1;
			break;
		default:
			break;
		}
	}

	init_lock(&lck);
	while (acquire_lock(&lck) != 0)
		;
	if (stat_lock(&lck) == 0)
		abort();
	release_lock(&lck);
	if (stat_lock(&lck) != 0)
		abort();

	/* measure perf */
	start = times(&tm);
	for (i = 0; i < nsloops; i++) {
		acquire_lock(&lck);
		release_lock(&lck);
	}

	i = times(&tm) - start;
	i = (i*1000)/CLK_TCK;
	printf("%s:time for %d acquire/release %d mS or %f uS per\n",
		Cmd, nsloops, i, (float)(i*1000)/(float)nsloops);

	/*
	 * Test acquire_lock
	 */
	if (sproc(slave, PR_SALL, nloops) < 0) {
		if (errno != EAGAIN) {
			errprintf(ERR_ERRNO_EXIT, "sproc failed");
			/* NOTREACHED */
		}
		printf("%s:can't sproc:%s\n", Cmd, strerror(errno));
	}
	if (waitfor)
		wait(NULL);

	i = nloops;
	while (i--)
		playlock(ACQ);

	i = nloops;
	while (i--) {
		tst = 7;

		if ((rv = test_then_add(&tst, 9)) != 7)
			errprintf(ERR_EXIT, "test/add returned %ld", rv);
			/* NOTREACHED */
		if ((rv = test_then_or(&tst, 0x7)) != 0x10)
			errprintf(ERR_EXIT, "test/or returned %ld", rv);
			/* NOTREACHED */
		if (tst != 0x17)
			errprintf(ERR_EXIT, "tst is %ld", tst);
			/* NOTREACHED */


		tst32 = 7;

		if ((rv32 = test_then_add32(&tst32, 9)) != 7)
			errprintf(ERR_EXIT, "test/add32 returned %d", rv32);
			/* NOTREACHED */
		if ((rv32 = test_then_or32(&tst32, 0x7)) != 0x10)
			errprintf(ERR_EXIT, "test/or32 returned %d", rv32);
			/* NOTREACHED */
		if (tst32 != 0x17)
			errprintf(ERR_EXIT, "tst32 is %d", tst32);
			/* NOTREACHED */
	}
	wait(NULL);

	/*
	 * Test spin_lock
	 */
	if (sproc(slave2, PR_SALL, nloops) < 0) {
		if (errno != EAGAIN) {
			errprintf(ERR_ERRNO_EXIT, "sproc failed");
			/* NOTREACHED */
		}
		printf("%s:can't sproc:%s\n", Cmd, strerror(errno));
	}
	if (waitfor)
		wait(NULL);

	i = nloops;
	while (i--)
		playlock(SPIN);
	wait(NULL);

	return 0;
}

/* this activates any RLD, etc stuff */
void
slave(void *a)
{
	auto unsigned long rv, tst;
	auto __uint32_t rv32, tst32;
	int i, nloops;

	nloops = (int)(__psint_t)a;
	i = nloops;
	while (i--)
		playlock(ACQ);

	while (nloops--) {
		tst = 7;

		if ((rv = test_then_add(&tst, 9)) != 7)
			errprintf(ERR_EXIT, "test/add returned %ld", rv);
			/* NOTREACHED */
		if (tst != 16)
			errprintf(ERR_EXIT, "test/add set %ld", tst);
			/* NOTREACHED */


		tst32 = 7;

		if ((rv32 = test_then_add32(&tst32, 9)) != 7)
			errprintf(ERR_EXIT, "test/add32 returned %d", rv32);
			/* NOTREACHED */
		if (tst32 != 16)
			errprintf(ERR_EXIT, "test/add32 set %d", tst32);
			/* NOTREACHED */
	}
	exit(0);
}

void
slave2(void *a)
{
	int i, nloops;

	nloops = (int)(__psint_t)a;
	i = nloops;
	while (i--)
		playlock(SPIN);
	exit(0);
}

void
playlock(int which)
{
	if (which == ACQ) {
		while (acquire_lock(&lck) != 0)
			;
	} else {
		spin_lock(&lck);
	}
	if (inlock)
		errprintf(ERR_EXIT, "inlock set!");
	inlock = 1;
	do_something();
	inlock = 0;
	release_lock(&lck);
}
		
void
do_something(void)
{
	volatile int i = doloops;
	while (i--)
		;
}
