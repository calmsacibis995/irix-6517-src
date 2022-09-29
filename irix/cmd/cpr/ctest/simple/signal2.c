#include <stdio.h>
#include <signal.h>

int succeeded = 0;

void
on_ckpt()
{
  printf("\n\t*** signal2: received SIGCKPT ***\n");
}

void
on_sig()
{
  printf("\n\t*** signal2: received test signal ***\n");
  succeeded++;
}



main(int argc, char ** argv)
{
	pid_t ctest_pid = getppid();
	int child;
	struct sigaction * action;
	int sig = atol(argv[1]);
	if (sig == 0) sig = SIGUSR1;
	signal(SIGCKPT, on_ckpt);
	signal(sig, on_sig);
	pause();
	printf("\n\t *** signal2: past SIGCKPT. ***\n");
	sigaction(sig, 0, action);
	if ((child = fork()) == 0)
	{
		sleep(7);
		kill(getppid(), sig);
		exit(0);
	}
	waitpid(child, 0, 0);
	printf("\n\t *** signal2: Done ***\n");
	if (succeeded)
		kill(ctest_pid, SIGUSR1);
	else
		kill(ctest_pid, SIGUSR2);
}
