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
	pid_t child;
	pid_t ctest_pid = getppid();
	int rval;
	extern int errno;
	signal(SIGCKPT, on_ckpt);

	if (argc > 1) {
		blkcnt = atoi(argv[1]);
		printf("block count: %d\n", blkcnt);
	}
	if ((child = fork()) != 0) {
		/* give child a chance to run */
		sleep(1);
		if (setblockproccnt(child, blkcnt) < 0) {
			perror("blockproc");
			exit(1);
		}
		pause();
		exit(0);
	}

	pause();
	if ((rval = syssgi(SGI_CKPT_SYS, CKPT_SLOWSYS, SLOWSYS_EINTR, 7)) < 0) {
		printf("slowsys 1:\n");
		printf("errno %d\n", errno);
		printf("oserror %d\n", oserror());
		perror("syssgi");
		printf("errno %d\n", errno);
	} else
		printf("syssgi retval: %d\n", rval);

	printf("Exiting!\n");

	exit(0);
}
