/*
 * sem2.c	Tests SETALL
 */
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stress.h>

char *Cmd;
int verbose;

int
main(int argc, char **argv)
{
	int j, i, c, semid;
	struct sembuf *sops;
	int nsems = 5;
	int nloops = 5;
	union semun vals, ovals;

	Cmd = errinit(argv[0]);
	while ((c = getopt(argc, argv, "vl:n:")) != EOF)
		switch (c) {
		case 'v':
			verbose = 1;
			break;
		case 'n':
			nsems = atoi(optarg);
			break;
		case 'l':
			nloops = atoi(optarg);
			break;
		}
	
	sops = malloc(nsems * sizeof(struct sembuf));
	vals.array = malloc(nsems * sizeof(ushort));
	ovals.array= malloc(nsems * sizeof(ushort));

	if ((semid = semget(IPC_PRIVATE, nsems, IPC_CREAT|IPC_EXCL|0777)) < 0) {
		errprintf(1, "semget");
		/* NOTREACHED */
	}

	for (j = 0; j < nloops; j++) {
		/* start them all out at random values */
		for (i = 0; i < nsems; i++)
			ovals.array[i] = rand() % (32*1024);
		if (semctl(semid, 0, SETALL, ovals)) {
			errprintf(3, "SETALL");
			goto bad;
		}

		/* increment them all */
		for (i = 0; i < nsems; i++) {
			sops[i].sem_num = i;
			sops[i].sem_op = 1;
			sops[i].sem_flg = 0;
		}
		if (semop(semid, sops, nsems)) {
			errprintf(3, "semop - increment");
			goto bad;
		}
		if (semctl(semid, 0, GETALL, vals)) {
			errprintf(3, "GETALL");
			goto bad;
		}
		for (i = 0; i < nsems; i++) {
			if (vals.array[i] != ovals.array[i] + 1) {
				errprintf(2, "sem %d val is %d instead of %d",
					i, vals.array[i],
					ovals.array[i] + 1);
				goto bad;
			}
		}
	}

	semctl(semid, 0, IPC_RMID, NULL);
	exit(0);
bad:
	semctl(semid, 0, IPC_RMID, NULL);
	abort();
	/* NOTREACHED */
}
