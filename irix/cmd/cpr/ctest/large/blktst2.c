/*
 * Test basic signal save/restore
 */
#include <stdio.h>
#include <signal.h>
#include <sys/syssgi.h>
#include "ckpt_sys.h"

void on_ckpt() {;}

main(int argc, char *argv[])
{
	int blkcnt = -1;
	pid_t ctest_pid = getppid();
	pid_t child;
	int rval;
	extern int errno;

	signal(SIGHUP, SIG_IGN);
	signal(SIGCKPT, on_ckpt);

	if (argc > 1) {
		blkcnt = atoi(argv[1]);
		printf("block count: %d\n", blkcnt);
	}
	if ((child = fork()) != 0) {
		if (setblockproccnt(child, blkcnt) < 0) {
			perror("blockproc");
			exit(1);
		}
		pause();
		printf("child exiting...\n");
		exit(0);
	}

	pause();
	printf("child print 1\n");
	printf("child print 2\n");

	printf("Exiting!\n");
	kill(ctest_pid, SIGUSR1);
}
