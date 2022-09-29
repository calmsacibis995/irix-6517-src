#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <mutex.h>
#include <stdio.h>
#include <bstring.h>
#include "stress.h"

void play(int, int);

#define	NPROCS_DEFAULT	5
#define	ITER_DEFAULT	10000

char *Cmd;

int
main(int argc, char **argv)
{
	pid_t pid;
	int i, id, idx;
	int c, shid, amchild = 0;
	int *curid, *stop;
	int iterations = 0;
	int maxids = 2;
	int nprocs = 0;

	Cmd = errinit(argv[0]);
	opterr = 0;
	while ((c = getopt(argc, argv, "I:Ci:p:m:")) != EOF) {
		switch (c) {
		case	'm':
			maxids = atoi(optarg);
			break;
		case	'i':
			iterations = atoi(optarg);
			break;
		case	'p':
			nprocs = atoi(optarg);
			break;
		case 	'I':
			shid = atoi(optarg);
			break;
		case	'C':
			amchild = 1;
			break;
		default:
			opterr++;
			break;
		}
	}

	if (argc - optind != 0)
		opterr++;

	if (opterr) {
		printf("usage: %s [-i iterations][-p nprocs][-m maxids]\n",
			Cmd);
		exit(1);
	}

	if (amchild) {
		play(shid, maxids);
		exit(0);
	}

	if (nprocs == 0) {
		printf("%s: nprocs defaulting to %d\n", Cmd, NPROCS_DEFAULT);
		nprocs = NPROCS_DEFAULT;
	}
	if (iterations == 0) {
		printf("%s: iterations defaulting to %d\n", Cmd, ITER_DEFAULT);
		iterations = ITER_DEFAULT;
	}

	if ((shid = shmget(IPC_PRIVATE,
				sizeof(int) * maxids + sizeof(int),
				IPC_CREAT|0777)) < 0) {
		errprintf(ERR_ERRNO_EXIT, "shmget");
		/* NOTREACHED */
	}
	if ((stop = shmat(shid, (void *) 0, 0)) == (int *)-1L) {
		errprintf(ERR_ERRNO_EXIT, "shmat");
		/* NOTREACHED */
	}
	*stop = 0;
	curid = stop + 1;

	/* init ids */
	for (i = 0; i < maxids; i++) {
		curid[i] = -1;
	}

	for (i = 0; i < nprocs; i++) {
		if ((pid = fork()) < 0) {
			perror("fork");
			break;
		} else if (pid == 0) {
			/* child */
			char nshid[32];
			char nmaxids[32];
			char *sargv[20];
			int j;

			sprintf(nshid, "-I %d", shid);
			sprintf(nmaxids, "-m %d", maxids);
			j = 0;
			sargv[j++] = "shm4";
			sargv[j++] = "-C";
			sargv[j++] = nshid;
			sargv[j++] = nmaxids;
			sargv[j] = NULL;
			execvp(argv[0], sargv);
			errprintf(ERR_ERRNO_EXIT, "exec failed");
			exit(0);
		}
	}

	/* parent creates and destroys */
	for (i = 0; i < iterations; i++) {
		idx = rand() % maxids;
		id = curid[idx];
		if (id >= 0) {
			shmctl(id, IPC_RMID);
		}
		if ((curid[idx] = shmget(IPC_PRIVATE, 100, IPC_CREAT|0777)) < 0) {
			errprintf(ERR_ERRNO_EXIT, "shm4: shmget");
			/* NOTREACHED */
		}
	}
	*stop = 1;

	while (wait(0) >= 0 || errno == EINTR)
		;
	for (i = 0; i < maxids; i++) {
		if ((id = curid[i]) > 0)
			shmctl(id, IPC_RMID);
	}
	shmctl(shid, IPC_RMID);
	return 0;
}

void
play(int shid, int maxids)
{
	int idx, id;
	struct shmid_ds shmstat;
	int *curid, *stop;

	if ((stop = shmat(shid, (void *) 0, 0)) == (int *)-1L) {
		errprintf(ERR_ERRNO_EXIT, "play: shmat");
		/* NOTREACHED */
	}
	curid = stop + 1;
	srand(getpid());
	while (!(*stop)) {
		idx = rand() % maxids;
		id = curid[idx];
		if (id > 0) {
			shmctl(id, IPC_STAT, &shmstat);
			shmctl(id, IPC_SET, &shmstat);
		}
	}
}
