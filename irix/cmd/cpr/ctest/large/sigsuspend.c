
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

void restart_handler();
void setsig(int, void (*)());
void on_ckpt() {;}

time_t ckpt_end;

int
main(int argc, char **argv)
{
	sigset_t suspendmask;
	pid_t ctest_pid = getppid();
	int rc;
	signal(SIGCKPT, on_ckpt);
	setsig(SIGRESTART, restart_handler);
	printf("\n\tsigsuspend: before sleep...\n");
	sigemptyset(&suspendmask);
	rc = sigsuspend(&suspendmask);
	if (errno != EINTR  &&  errno != ECHILD) 
	{
		kill(ctest_pid, SIGUSR2);
                printf( "sigsuspend failed with: %s (rc %d)\n", strerror(errno), rc);
        }
	else
		kill(ctest_pid, SIGUSR1);
	printf("\n\tsigsuspend: after sleep...\n");
	printf("\n\tsigsuspend: Done\n");
}

void
restart_handler(void)
{
	ckpt_end = time(NULL);
	printf("RESTART signal at %s", ctime(&ckpt_end));
}

void
setsig(int signum, void (*handler)())
{
	struct sigaction sa;

	sa.sa_flags = SA_NOCLDSTOP;
	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGCLD);

	if (sigaction(signum, &sa, NULL) != 0) {
		fprintf(stderr,
			"Unable to set handler for signal %d: %s\n",
			signum, strerror(errno));
		exit(2);
	}
}
