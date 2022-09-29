
#include <stdio.h>
#include <ulocks.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

void on_ckpt() {;}

main()
{
	int stat;
	pid_t cpid;
	pid_t ctest_pid = getppid();
	FILE *fp;

	signal(SIGCLD, SIG_IGN);
	signal(SIGCKPT, on_ckpt);
	fp = fopen("INFO", "w");
	if ((cpid = fork()) == 0) {

		sleep(20);

		exit (0);
	}
	if (cpid < 0) {
		perror("fork");
		return (-1);
	}
	pause();

	wait(&stat);
	printf("sleep done\n");
	fwrite("\n", 2, 1, fp);	
	kill(ctest_pid, SIGUSR1);
}
