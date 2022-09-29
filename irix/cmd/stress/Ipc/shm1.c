/* shm1		This is a test of shared memory (and semaphores).
 *		The process forks into a parent and child.  The child
 *		does a P operation to wait for the parent to write a string
 *		into a shared memory region, checks the string, and dies.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>

#define nextarg {argc--; argv++;}
int P(int semid);
int V(int semid);

int
main(int argc, char **argv)
{
	char message[30];
	int semid;
	int shmid;
	union semun sm;
	char *ptr;
	pid_t pid;
	int wval;
	int retval;

	if ((semid = semget(IPC_PRIVATE, 1, IPC_CREAT|IPC_EXCL|0777)) < 0) {
		perror("shm1:semget");
		exit(1);
	}
	sprintf(message, "Message %d\n", semid);

	if ((pid = fork()) == -1) {
		perror("shm1:fork");
		sm.val = 0;
		if (semctl(semid, 0, IPC_RMID, sm) < 0) {
			perror("shm1:semctl");
		}
		exit(1);
	} else if (pid) {	/* PARENT */
		retval = 0;
		if ((shmid = shmget((0xdcba << 16) | pid, 4096, IPC_CREAT|0666)) < 0) {
			perror("shm1:shmget");
			retval = 1;
			goto cleanup;
		}
		if ((ptr = (char *)shmat(shmid, 0, 0)) == (char *)-1L) {
			perror("shm1:shmat");
			retval = 1;
			goto cleanup;
		}
		strcpy(ptr, message);
		if (shmdt(ptr) < 0) {
			perror("shm1:shmdt");
			retval = 1;
			goto cleanup;
		}
cleanup:
		V(semid);
		if ((wval = wait(0)) != pid) {
			if (wval == -1) {
				perror("shm1:wait");
			} else {
				fprintf(stderr, "shm1:wait returned bogus pid\n");
			}
			exit(1);
		}
		sm.val = 0;
		if (semctl(semid, 0, IPC_RMID, sm) < 0) {
			perror("shm1:semctl");
		}
		if (shmctl(shmid, IPC_RMID, 0) < 0) {
			perror("shm1:shmid (IPC_RMID)");
		}
		exit(retval);
	} else {		/* CHILD */
		if ((shmid = shmget((0xdcba << 16) | getpid(), 4096, IPC_CREAT|0666)) < 0) {
			perror("shm1:shmget");
			exit(1);
		}
		if ((ptr = (char *)shmat(shmid, 0, 0)) == (char *)-1L) {
			perror("shm1:shmat");
			exit(1);
		}
		if (P(semid) < 0) {
			exit(1);
		}

		if (strcmp(ptr, message) != 0) {
			fprintf(stderr, "shm1:Compare Error :%s: vs :%s:\n", message, ptr);
		}
		if (shmdt(ptr) < 0) {
			perror("shm1:shmdt");
			exit(1);
		}
		exit(0);
	}
	return 0;
}

int
P(int semid)
{
	struct sembuf sops[1];

	sops[0].sem_num = 0;
	sops[0].sem_op = -1;
	sops[0].sem_flg = 0;
	if (semop(semid, (struct sembuf *)sops, (unsigned)1) < 0) {
		perror("shm1:semop1");
		return(-1);
	} else {
		return(0);
	}
}

int
V(int semid)
{
	struct sembuf sops[1];

	sops[0].sem_num = 0;
	sops[0].sem_op = 1;
	sops[0].sem_flg = 0;
	if (semop(semid, (struct sembuf *)sops, (unsigned)1) < 0) {
		perror("shm1:semop2");
		return(-1);
	} else {
		return(0);
	}
}
