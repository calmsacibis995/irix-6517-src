/*
 * sem3.c	Tests adjust on exit feature of sysV semaphores.
 *
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>


char *ProgName;

void
error(
	const char	*msg
	)
{
	fprintf(stderr, "%s -- %s\n", ProgName, msg);
	exit(1);
}


void
panic(
	const char	*msg
	)
{
	fprintf(stderr, "%s -- ", ProgName);
	perror(msg);
	exit(1);
}


void
doit()
{
	int		semid;
	struct sembuf	sops[1];
	union semun	sm;
	pid_t		pid;
	int		wval;
	int		wstatus;

	if ((semid = semget(IPC_PRIVATE, 1, IPC_CREAT|IPC_EXCL|0777)) < 0)
		panic("semget");

	if ((pid = fork()) < 0) {
		sm.val = 0;
		semctl(semid, 0, IPC_RMID, sm);
		panic("fork");
	}


	if (pid) {			/* Parent */

		if ((wval = wait(&wstatus)) < 0)
			panic("wait");

		if (wval != pid)
			error("ERROR: wrong pid waited");

		if (WEXITSTATUS(wstatus) != 0)
			error("child failed");

		sm.val = semctl(semid, 0, GETVAL, sm);
		if (sm.val != 0) {
			fprintf(stderr, "ERROR: val = %d\n", sm.val);
			exit(1);
		}

		sm.val = 0;
		if (semctl(semid, 0, IPC_RMID, sm) < 0)
			panic("semctl(rmid)");

	} else {			/* Child */
		sops[0].sem_op = 1;
		sops[0].sem_num = 0;
		sops[0].sem_flg = SEM_UNDO;

		if (semop(semid, (struct sembuf *)sops, (size_t)1) < 0)
			panic("semop");

		exit(0);
	}
}


int
main(
	int	argc,
	char	**argv
	)
{
	int	iters;

	ProgName = argv[0];

	if (argc > 2) {
		fprintf(stderr, "Usage: %s <iters>\n", ProgName);
		exit(1);
	}

	iters = (argc == 2) ? atoi(argv[1]) : 1;
	printf("%s: doing %d iterations\n", ProgName, iters);

	while (iters--)
		doit();

	printf("%s: PASSED\n", ProgName);
	return 0;
}
