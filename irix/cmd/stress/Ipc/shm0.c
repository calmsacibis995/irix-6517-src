/* shm0		This is a simple test of shared memory.  A shared memory
 *		region is created, attached, written to, and detached.
 *		It is then re-obtained, attached, read from, and detached.
 */
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define nextarg {argc--; argv++;}

int forkflag = 0;
void cleanup(int shmid);

int
main(int argc, char **argv)
{
	char message[30];
	int shmid0, shmid1;
	char *ptr;
	int mainpid, mainwval;

	nextarg;

	while (argc && (**argv == '-')) {
		switch(*++*argv) {
			case 'f':
				forkflag++;
				break;

			default:
				fprintf(stderr, "unknown key\n");
				exit(1);
		}
		nextarg;
	}

	if (forkflag) {
		if ((mainpid = fork()) == -1) {
			perror("fork");
			exit(1);
		}
	}

	if ((shmid0 = shmget(0xdcba, 4096, IPC_CREAT|IPC_EXCL|0666)) < 0) {
		if (!forkflag || errno != EEXIST) {
			perror("shmget-id0");
			exit(1);
		}
		if ((shmid0 = shmget(0xdcba, 4096, 0666)) < 0) {
			perror("shmget-id0-2nd try");
			exit(1);
		}
	}

	if ((ptr = (char *)shmat(shmid0, 0, 0)) == (char *)-1L) {
		perror("shmat-id0");
		cleanup(shmid0);
		exit(1);
	}
	sprintf(message, "Message %d\n", shmid0);
	strcpy(ptr, message);
	
	if (shmdt(ptr) < 0) {
		perror("shmdt-id0");
		cleanup(shmid0);
		exit(1);
	}

	if ((shmid1 = shmget(0xdcba, 4096, 0666)) < 0) {
		perror("shmget-id1");
		cleanup(shmid0);
		exit(1);
	}

	if ((ptr = (char *)shmat(shmid1, 0, 0)) == (char *)-1L) {
		perror("shmat-id1");
		cleanup(shmid1);
		exit(1);
	}

	if (shmctl(shmid1, SHM_LOCK)) {
		if (errno != EPERM) {
			perror("SHMLOCK failed");
			cleanup(shmid1);
			exit(1);
		}
	}
	if (strcmp(ptr, message) != 0) {
		fprintf(stderr, "Compare failed\n");
	}
	
	if (shmdt(ptr) < 0) {
		perror("shmdt-id1");
		cleanup(shmid1);
		exit(1);
	}

	if (forkflag && mainpid && ((mainwval = wait(0)) != mainpid)) {
		if (mainwval == -1) {
			perror("wait");
		} else {
			fprintf(stderr, "wait returned bogus pid\n");
		}
		exit(1);
	}
	if (!forkflag || mainpid)
		cleanup(shmid1);
	exit(0);
	/* NOTREACHED */
}

void
cleanup(int shmid)
{

	if (shmctl(shmid, IPC_RMID, 0) < 0) {
		if (!forkflag || errno != EINVAL)
			perror("shmctl (IPC_RMID)");
	}
}
