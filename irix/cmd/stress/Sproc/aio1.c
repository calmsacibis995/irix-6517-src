#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include "aio.h"
#include "ulocks.h"
#include "fcntl.h"

/*
 * A test harness
 */

static int betweenrand(int, int);
static void comp(), leave();
int outstanding, nvalid;
int nslaves;
char **fnames;
aiocb_t *list;

main(argc, argv)
int argc;
char **argv;
{
	extern int	optind;
	extern char	*optarg;
	int		totsize, c, nloops;
	char *Cmd;
	int rv, n, i, *fd, naio, maxsize, persize, *fdsize, perfd;
	char *bufs, *sbufs;
	aiocb_t *a, **alistlist;
	int *gotzero, *afd;
	struct stat sb;

	Cmd = argv[0];
	nslaves = 4;
	nloops = 4;
	while ((c = getopt(argc, argv, "n:l:")) != EOF) {
		switch (c) {
		case 'l':
			nloops = atoi(optarg);
			break;
		case 'n':
			nslaves = atoi(optarg);
			break;
		}
	}

	fd = malloc(nslaves * sizeof(int));
	fdsize = malloc(nslaves * sizeof(int));
	fnames = malloc(nslaves * sizeof(char *));
	for (i = 0; i < nslaves; i++) {
		fnames[i] = strdup(tempnam(NULL, "aio"));
		fd[i] = open(fnames[i], O_RDWR|O_CREAT|O_TRUNC, 0666);
	}

	_aio_init(nslaves);
	sigset(SIGUSR1, comp);
	sigset(SIGINT, leave);
	sigset(SIGHUP, leave);
	sigset(SIGTERM, leave);
	for (n = 0; n < nloops; n++) {
		/* get random # requests */
		naio = betweenrand(10, 500);
		maxsize = betweenrand(1, 20000);

		list = malloc(naio * sizeof(aiocb_t));
		sbufs = bufs = malloc(maxsize * naio);
		outstanding = 0;
		nvalid = 0;

		printf("aio:doing %d requests maxsize %d\n", naio, maxsize);
		/* fill in & start requests */
		for (i = 0; i < naio;) {
			persize = betweenrand(1, maxsize);
			perfd = rand() % nslaves;
			list[i].aio_flag = AIO_EVENT;
			list[i].aio_event.sigev_signo = SIGUSR1;
			list[i].aio_whence = SEEK_SET;
			list[i].aio_offset = betweenrand(0, 1000000);
			list[i].aio_buf = (volatile char *)bufs;
			list[i].aio_nbytes = persize;
			list[i].aio_reqprio = betweenrand(AIO_PRIO_MIN, AIO_PRIO_MAX);
			/* hold now so nvalid can be correctly maintained */
			sighold(SIGUSR1);
			if (aio_write(fd[perfd], &list[i]) != 0) {
				sigrelse(SIGUSR1);
				if (oserror() == EAGAIN)
					break;
				perror("aio_write");
				continue;
			}
			nvalid++;
			bufs += persize;
			fdsize[perfd] += persize;
			outstanding++;
			sigrelse(SIGUSR1);
			i++;
		}
		printf("aio:waiting for %d requests\n", outstanding);

		/* wait for all to complete */
		sighold(SIGUSR1);
		while (outstanding) {
			sigpause(SIGUSR1);
			sighold(SIGUSR1);
		}
		sigrelse(SIGUSR1);

		free(sbufs);
		free(list);
	}

	naio = 64;
	maxsize = 32*1024;
	list = malloc(naio * sizeof(aiocb_t));
	alistlist = (aiocb_t **)malloc(naio * sizeof(aiocb_t *));
	afd = malloc(naio * sizeof(*afd));
	gotzero = malloc(naio * sizeof(*gotzero));
	sbufs = bufs = malloc(maxsize * naio);
	outstanding = 0;

	printf("aio:\nREADING\n");
	for (i = 0, totsize = 0; i < nslaves; i++) {
		fstat(fd[i], &sb);
		fdsize[i] = sb.st_size;
		printf("aio:File %d size %d\n", i, fdsize[i]);
		totsize += fdsize[i];
		lseek(fd[i], 0, SEEK_SET);
	}

	/*
	 * read all files
	 */
	while (totsize) {
		/* fill in & start requests */
		bzero(alistlist, naio * sizeof(aiocb_t *));
		bufs = sbufs;

		printf("aio:doing %d requests maxsize %d\n", naio, maxsize);
		for (i = 0; i < naio;) {
			persize = betweenrand(1, maxsize);
			list[i].aio_flag = 0;
			list[i].aio_whence = SEEK_CUR;
			list[i].aio_offset = 0;
			list[i].aio_buf = (volatile char *)bufs;
			list[i].aio_nbytes = persize;
			list[i].aio_reqprio = betweenrand(AIO_PRIO_MIN, AIO_PRIO_MAX);
			/* pick a file/slave */
			do {
				n = betweenrand(0, nslaves-1);
			} while (totsize && fdsize[n] == 0);
			if (totsize == 0)
				break;

			if (aio_read(fd[n], &list[i]) != 0) {
				if (oserror() == EAGAIN) {
					printf("got EAGAIN from read!\n");
					goto bad;
				}
				perror("aio_read");
				continue;
			}
			alistlist[i] = &list[i];
			afd[i] = n;
			gotzero[i] = 0;
			bufs += persize;
			outstanding++;
			i++;
		}
		printf("aio:waiting for %d requests\n", outstanding);
		while (outstanding) {
			if (aio_suspend(i, (const aiocb_t **)alistlist) < 0) {
				perror("aio_suspend");
			} else {
				int j;

				/* look for a done one */
				for (j = 0; j < i; j++) {
					if (!(a = alistlist[j]))
						continue;
					if (aio_error(a->aio_handle) != EINPROG)
						break;
				}
				if (j == i) {
					printf("suspend returned but no completed io\n");
					goto bad;
				}
				n = afd[j];
				if ((rv = aio_error(a->aio_handle)) != 0)
					printf("aio:completion error %s\n",
					 strerror(rv));
				else {
					fdsize[n] -= aio_return(a->aio_handle);
					totsize -= aio_return(a->aio_handle);
				}
				alistlist[j] = NULL;
				outstanding--;
				if (aio_return(a->aio_handle) == 0)
					gotzero[n]++;
			}
		}
		for (n = 0; n < nslaves; n++) {
			if (gotzero[n] && fdsize[n]) {
				printf("aio:got a zero but still %d bytes in fd:%d!\n",
					fdsize[n], n);
			}
		}
	}

	rmfiles();
	exit(0);
bad:
	rmfiles();
	exit(-1);

}

rmfiles()
{
	int i;
	for (i = 0; i < nslaves; i++) {
		unlink(fnames[i]);
	}
}

static void
leave()
{
	rmfiles();
	exit(-1);
}

/* handle completion */
static void
comp()
{
	int rv, found, i, err;

	/*
	 * since we can complete multiple transactions per signal
	 * we could get a signal when there is nothing left
	 */
	if (outstanding == 0)
		return;
	for (i = 0, found = 0; i < nvalid; i++) {
		if (list[i].aio_buf == NULL)
			continue;
		if ((err = aio_error(list[i].aio_handle)) != EINPROG) {
			found++;
			if (err)
				printf("aio:completion error %s\n", strerror(err));
			rv = aio_return(list[i].aio_handle);
			if (rv != list[i].aio_nbytes)
				printf("aio:expected %d bytes read/wrote %d\n",
					list[i].aio_nbytes, rv);
			assert(outstanding > 0);
			outstanding--;
			list[i].aio_buf = NULL;
		}
	}
	/* we can have nothing since we scan for more than one complete
	 * per signal
	if (!found)
		printf("aio:complete signal but nothing done!\n");
	 */
}

static int
betweenrand(int low, int high)
{
	int r;
	if (low >= high)
		return(low);
	do {
		r = rand() % (high+1);
	} while (r < low);
	return(r);
}
