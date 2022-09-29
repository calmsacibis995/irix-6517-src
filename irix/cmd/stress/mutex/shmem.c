/*
 * shmem.c - organize shared memory between mutex.c's
 *
 */


#include <sgidefs.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "shmem.h"

struct shmem_st *shmem;
int shmid;

int
createShm(key_t key)
{
	int shmid;

	if ((shmid = shmget(key, sizeof(shmem), IPC_CREAT|IPC_EXCL|0666)) < 0) {
		if ((shmid = shmget(key, sizeof(shmem), 0666)) < 0) {
			perror("shmget-2nd try");
			exit(1);
		}
	}

	return shmid;
}

struct shmem_st *
initializeShm(int shmid)
{
	struct shmem_st *shmem;

	if ((shmem = shmat(shmid, (void *) 0, 0)) == (struct shmem_st *) -1L) {
		perror("shmat");
		exit(1);
	}

	return shmem;
}

void
closeShm(struct shmem_st *shmem)
{
	if (shmdt(shmem) < 0) {
		perror("shmdt");
		exit(1);
	}
}


void
destroyShm(int shmid)
{

	if (shmctl(shmid, IPC_RMID, 0) < 0) {
		perror("shmctl (IPC_RMID)");
	}
}
