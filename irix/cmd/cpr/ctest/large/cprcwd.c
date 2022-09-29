#include <unistd.h>
#include <stdio.h>
#include <signal.h>

void on_ckpt() {;}

main()
{

	char *cwd1, *cwd2;

	pid_t ctest_pid = getppid();
	signal(SIGCKPT, on_ckpt);

	if (chdir("/usr/people/guest/aravind/ctest")) {
	     perror("cprcpwd: chdir");
	     exit(2);
	} 
	if (chroot("/usr/people/guest/aravind")) {
	     perror("cprcwd: chroot");
	     exit(2);
	}
	if ((cwd1 = getcwd(NULL, 64)) == NULL) {
	     perror("cprcwd: pwd");
	     exit(2);
	}
	printf("\n\t *** cprcwd: cwd is %s ***\n\n", cwd1);

	pause();

	if ((cwd2 = getcwd(NULL, 64)) == NULL) {
	     perror("pwd");
	     exit(2);
	}
	printf("\n\t *** cprcwd: cwd is %s\n\n", cwd2);
	if (!strcmp(cwd1, cwd2))
		kill(ctest_pid, SIGUSR1);
	else
		kill(ctest_pid, SIGUSR2);
	free(cwd1);
	free(cwd2);
	printf("\n\t*** cprcwd: Done ***\n\n");
}

