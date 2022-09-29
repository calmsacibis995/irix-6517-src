/*
 * flimit.c	Tests EFBIG.
 *		We make sure we don't get SIGFSZ from semaphores.
 */
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <signal.h>
#ifndef SVR3
#include <ulimit.h>
#else
#define UL_SETFSIZE 2
#endif
#include <errno.h>
#include "stress.h"

char *Cmd;
volatile int expected;
volatile int gotfsz;

void
sigcatcher(int sig)
{
	if (sig == SIGXFSZ) {
		if (expected) {
			gotfsz++;
			return;
		} else {
			errprintf(2, "unexpected SIGXFSZ\n");
		}
	} else {
		errprintf(0, "unexpected signal %d\n", sig);
	}
}

int
main(int argc, char **argv)
{
	int fd, i, c, semid;
	ssize_t rv;
	struct sembuf sops[1];
	long newlim = 20;
	char *fname = NULL;
	char buf[512];
	void (*oldxfsz)();

	Cmd = errinit(argv[0]);
	oldxfsz = sigset(SIGXFSZ, sigcatcher);

	while ((c = getopt(argc, argv, "f:s:")) != EOF)
		switch (c) {
		case 'f':
			fname = optarg;
			break;
		case 's':	/* size to set ulimit to in 512 Byte blocks */
			newlim = atoi(optarg);
			break;
		}

	if (fname == NULL)
		fname = tempnam(NULL, "flimit");

	printf("%s: old SIGXFSZ handler 0x%x(%s)\n",
				Cmd, oldxfsz,
				oldxfsz == SIG_IGN ? "IGN" :
				oldxfsz == SIG_DFL ? "DFL" : "CATCH");

	if ((semid = semget(IPC_PRIVATE, 1, IPC_CREAT|IPC_EXCL|0777)) < 0) {
		errprintf(1, "semget");
		/* NOTREACHED */
	}

	sops[0].sem_num = 1;
	sops[0].sem_op = 0;
	sops[0].sem_flg = 0;
	expected = 0;
	rv = semop(semid, (struct sembuf *)sops, 1);
	if (rv < 0 && errno != EFBIG)
		errprintf(1, "semop");
	else if (rv >= 0)
		errprintf(0, "semop");
		/* NOTREACHED */
	if (semctl(semid, 0, IPC_RMID) < 0) {
		errprintf(1, "semctl(RMID)");
		/* NOTREACHED */
	}

	if (ulimit(UL_SETFSIZE, newlim) < 0)
		errprintf(1, "ulimit(SET)");
	
	if ((fd = open(fname, O_RDWR|O_CREAT|O_TRUNC, 0666)) < 0)
		errprintf(1, "open");
	
	expected = 1;
	gotfsz = 0;
	for (i = 0; i < newlim+1; i++) {
		rv = write(fd, buf, 512);
		if (i == newlim && rv < 0 && errno == EFBIG)
			break;
		if (rv < 0) {
			errprintf(3, "write failed i:%d newlim:%d",
				i, newlim);
			goto err;
		} else if (rv != 512) {
			errprintf(2, "short write %d", rv);
			goto err;
		}
	}
	if (gotfsz != 1) {
		errprintf(2, "received too few/too many SIGXFSZ: %d", gotfsz);
		goto err;
	}

	unlink(fname);
	return 0;

err:
	unlink(fname);
	abort();
	/* NOTREACHED */
}
