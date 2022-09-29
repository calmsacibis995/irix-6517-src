#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

main(int argc, char **argv)
{
	int count, i, status;
	pid_t pid;

	if (argc < 3) {
		fprintf(stderr, "usage: %s count command [command args]\n",
		    argv[0]);
		exit(1);
	}

	count = atoi(argv[1]);
	
	for (i = 0; i < count; i++) {
		pid = fork();
		switch (pid) {
		    case -1:
			perror("fork");
			exit(1);
		    case 0:
			/* child */
			if (execvp(argv[2], argv + 2) < 0) {
				perror("execvp");
				exit(1);
			}
		}
	}

	for (i = 0; i < count; i++) {
		pid = wait(&status);
		if (pid < 0) {
			perror("wait");
			exit(1);
		}
		fprintf(stderr, "process: %d exited with status: %d\n",
		    pid, WEXITSTATUS(status));
	}

	exit(0);
}
