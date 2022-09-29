/*
 * Test basic signal save/restore
 */
#include <stdio.h>
#include <signal.h>
#include <sys/syssgi.h>
#include "ckpt_sys.h"

void
sigacthandler(int signo, siginfo_t *sip, void *ptr)
{
	printf("sighandler: %d\n", signo);
}

void on_ckpt() {;}


main(int argc, char *argv[])
{
	pid_t ctest_pid = getppid();
	int blkcnt = 0;
	int signo;
	stack_t sigstack;
	sigaction_t act;
	sigaction_t array[NSIG];
	sigaction_t act1;
	int rval;
	extern int errno;
	signal(SIGCKPT, on_ckpt);
	if (argc > 1) {
		blkcnt = atoi(argv[1]);
		printf("block count: %d\n", blkcnt);
	}
	act.sa_flags = SA_NODEFER|SA_SIGINFO|SA_NOCLDSTOP|SA_NOCLDWAIT;
	act.sa_handler = NULL;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGTERM);
	sigaddset(&act.sa_mask, SIGUSR1);
	sigaddset(&act.sa_mask, SIGUSR2);
	act.sa_sigaction = sigacthandler;

	pause();

	for (signo = 1; signo < NSIG; signo++) {
		if ((signo == SIGKILL)||(signo == SIGSTOP))
			continue;
		if (sigaction(signo, &act, NULL) < 0) {
			fprintf(stderr, "signal %d:", signo);
			perror("sigaction");
			exit(1);
		}
		if (sigaction(signo, NULL, &array[signo]) < 0) {
			fprintf(stderr, "signal %d:", signo);
			perror("sigaction");
			exit(1);
		}
	}
	if ((rval = syssgi(SGI_CKPT_SYS, CKPT_SLOWSYS, SLOWSYS_EINTR, 7, blkcnt)) < 0) {
		printf("slowsys 1:\n");
		printf("errno %d\n", errno);
		printf("oserror %d\n", oserror());
		perror("syssgi");
		printf("errno %d\n", errno);
	} else
		printf("syssgi retval: %d\n", rval);

	if ((rval = syssgi(SGI_CKPT_SYS, CKPT_SLOWSYS, SLOWSYS_EINTR|SLOWSYS_SEMAWAIT, 7, blkcnt)) < 0) {
		printf("slowsys 2:\n");
		printf("errno %d\n", errno);
		printf("oserror %d\n", oserror());
		perror("syssgi");
		printf("errno %d\n", errno);
	} else
		printf("syssgi retval: %d\n", rval);
		
	if ((rval = syssgi(SGI_CKPT_SYS, CKPT_SLOWSYS, 0, 7, blkcnt)) < 0) {
		printf("slowsys 3:\n");
		printf("errno %d\n", errno);
		perror("syssgi");
		printf("errno %d\n", errno);
	} else {
		printf("slowsys 3:\n");
		printf("syssgi retval: %d\n", rval);
	}
	if ((rval = syssgi(SGI_CKPT_SYS, CKPT_SLOWSYS, SLOWSYS_SEMAWAIT, 7, blkcnt)) < 0) {
		printf("slowsys 4:\n");
		printf("errno %d\n", errno);
		perror("syssgi");
		printf("errno %d\n", errno);
	 } else {
		printf("slowsys 4:\n");
		printf("syssgi retval: %d\n", rval);
	}

	printf("Exiting!\n");
	kill(ctest_pid, SIGUSR1);
}
