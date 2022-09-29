#include <stdio.h>
#include <stdlib.h>
/*
 * Big memory image for perf testing
 */
char *bigbuf;

void
sighandlr()
{
	return;
}

main(int argc, char *argv[])
{
	pid_t ctest_pid = getppid();
	int i;
	int bufsize = 64;

	if (argc > 1)
		bufsize = atoi(argv[1]);

	if (fork() == -1) {
		printf("fork failure\n");
		exit(1);
	}
	printf("malloc %d Mbytes\n", bufsize);
	bigbuf = malloc(bufsize*1024*1024);
	if (bigbuf == NULL) {
		perror("malloc");
		exit(1);
	}
again:
	signal(33, sighandlr);
	signal(34, sighandlr);
	/*
	 * dirty all pages of bigbuf
	 */
	for (i = 0; i < bufsize*1024*1024; i++)
		bigbuf[i] = i&0xff;

	printf("ready to checkpoint!\n");

	pause();	/* wait for SIGCKPT */

	printf("checking buffer\n");

	for (i = 0; i < bufsize*1024*1024; i++) {
		if (bigbuf[i] != (char)(i&0xff)) {
			printf("mis-match at %d expected %x got %x\n",
				i, i&0xff, bigbuf[i]);
			break;
		}
	}
	printf("bvuffer okay!\n");
	kill(ctest_pid, SIGUSR1);
}
