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

#define	Basekey		0XC0FFEE

void play(int, int, int);

#define	NPROCS_DEFAULT	10
#define	ITER_DEFAULT	1000

char *Cmd;
struct info {
	key_t key;
	long id;
};

int
main(int argc, char **argv)
{
	pid_t pid;
	int	i, id;
	int	c;
	int iterations = 0;
	int nprocs = 0;
	int shid;
	int maxkeys = 1000;
	int amchild = 0;
	struct info *arenas;

	Cmd = errinit(argv[0]);
	opterr = 0;
	while ((c = getopt(argc, argv, "I:Ck:i:p:")) != EOF) {
		switch (c) {
		case	'k':
			maxkeys = atoi(optarg);
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
		printf("usage: %s [-i iterations][-p nprocs][-k maxkeys]\n",
			Cmd);
		exit(1);
	}
	if (amchild) {
		play(iterations, shid, maxkeys);
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
				sizeof(struct info) * maxkeys,
				IPC_CREAT|0777)) < 0) {
		errprintf(ERR_ERRNO_EXIT, "shmget");
		/* NOTREACHED */
	}
	if ((arenas = shmat(shid, (void *) 0, 0)) == (struct info *)-1L) {
		errprintf(ERR_ERRNO_EXIT, "shmat");
		/* NOTREACHED */
	}

	/* init keys */
	for (i = 0; i < maxkeys; i++) {
		arenas[i].key = Basekey+i;
		arenas[i].id = -1;
	}
	/* cleanup from last run! */
	for (i = 0; i < maxkeys; i++) {
		if ((id = shmget(arenas[i].key, 4096, 0)) >= 0)
			shmctl(id, IPC_RMID);
	}

	for (i = 0; i < nprocs; i++) {
		if ((pid = fork()) < 0) {
			perror("fork");
			break;
		} else if (pid == 0) {
			/* child */
			char niterations[32];
			char nshid[32];
			char nmaxkeys[32];
			char *sargv[20];
			int j;

			sprintf(niterations, "-i %d", iterations);
			sprintf(nshid, "-I %d", shid);
			sprintf(nmaxkeys, "-k %d", maxkeys);
			j = 0;
			sargv[j++] = "shm3";
			sargv[j++] = "-C";
			sargv[j++] = niterations;
			sargv[j++] = nshid;
			sargv[j++] = nmaxkeys;
			sargv[j] = NULL;
			execvp(argv[0], sargv);
			errprintf(ERR_ERRNO_EXIT, "exec failed");
		}
	}
	while (wait(0) >= 0 || errno == EINTR)
		;
	for (i = 0; i < maxkeys; i++) {
		if ((id = shmget(arenas[i].key, 4096, 0)) >= 0)
			shmctl(id, IPC_RMID);
	}
	shmctl(shid, IPC_RMID);
	return 0;
}

void
play(int iterations, int shid, int maxkeys)
{
	int i, idx, id, count;
	struct shmid_ds shmstat;
	struct info *arenas;

	if ((arenas = shmat(shid, (void *) 0, 0)) == (struct info *)-1L) {
		errprintf(ERR_ERRNO_EXIT, "play: shmat");
		/* NOTREACHED */
	}
	srand(getpid());
	for (i = 0; i < iterations; i++) {
		/*
		 * pick a random key and if it exists, stat it, set some perm
		 * then delete it
		 */
		count = 0;
		do {
			idx = rand() % maxkeys;
			id = (int)test_and_set((unsigned long *)&arenas[idx].id, -1L);
			if (id > 0) {
				shmctl(id, IPC_STAT, &shmstat);
				shmctl(id, IPC_SET, &shmstat);
				shmctl(id, IPC_RMID);
			}
		} while (id == -1 && ++count < 75);

		/* now create one */
		do {
			idx = rand() % maxkeys;
			id = shmget(arenas[idx].key, 4096,
					IPC_CREAT|IPC_EXCL|0777);
		} while (id == -1);
		id = (int)test_and_set((unsigned long *)&arenas[idx].id, (long)id);
		if (id > 0) {
			shmctl(id, IPC_STAT, &shmstat);
			shmctl(id, IPC_SET, &shmstat);
			shmctl(id, IPC_RMID);
		}
	}
}
