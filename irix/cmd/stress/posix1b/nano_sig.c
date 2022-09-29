#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>

#define SECONDS	20

void
hdlSig(int sig)
{
	if (sig != SIGHUP) {
		printf("Unexpected signal: %d\n", sig);
	}
	printf("Handler\n");
}


main()
{
	struct sigaction 	act;
        struct timespec 	tp;
        struct timespec 	tp2;

	/*
	 * Set handler for signal used to interrupt sleep.
	 */
	act.sa_handler = hdlSig;
	sigfillset(&act.sa_mask);
	act.sa_flags = 0;
	if (sigaction(SIGHUP, &act, NULL) != 0) {
		exit(1);
	}

	/*
	 * Set amount of time to sleep.
	 */
        tp.tv_sec = (time_t)SECONDS;
        tp.tv_nsec = 0;

	/*
	 * Sleep.
	 */
        (void)nanosleep(&tp, &tp2);

	/*
	 * Report results.
	 */
	printf("Returned seconds: %d\n", tp2.tv_sec);
}
