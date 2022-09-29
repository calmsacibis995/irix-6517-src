/*
 * sem1.c	Tests non-zero semaphore operations.  The program will fork
 *		into a parent and a child.  The child will do a effectively
 *		do a P operation, and then destroy the semaphore set.  The
 *		parent will do a V operation and wait for the child to die.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#define nextarg {argc--; argv++;}

int
main(int argc, char **argv)
{
	int semid;
	struct sembuf sops[1];
	union semun sm;
	pid_t pid;
	int wval;

	if ((semid = semget(IPC_PRIVATE, 1, IPC_CREAT|IPC_EXCL|0777)) < 0) {
		perror("semget");
		exit(1);
	}
	if ((pid = fork()) == -1) {
		perror("fork");
		sm.val = 0;
		if (semctl(semid, 0, IPC_RMID, sm) < 0) {
			perror("semctl");
		}
		exit(1);
	} else if (pid) {	/* Parent */
		sops[0].sem_num = 0;
		sops[0].sem_op = 1;
		sops[0].sem_flg = 0;
		if (semop(semid, (struct sembuf *)sops, (unsigned)1) < 0) {
			perror("semop1");
		}
		if ((wval = wait(0)) != pid) {
			if (wval == -1) {
				perror("wait");
			} else {
				fprintf(stderr, "wait returned bogus pid\n");
			}
			exit(1);
		}
		exit(0);
	} else {		/* Child */
		sops[0].sem_num = 0;
		sops[0].sem_op = -1;
		sops[0].sem_flg = 0;
		if (semop(semid, (struct sembuf *)sops, (unsigned)1) < 0) {
			perror("semop2");
			exit(1);
		}
	sm.val = 0;
		if (semctl(semid, 0, IPC_RMID, sm) < 0) {
			perror("semctl");
		}
	}
	return 0;
}
