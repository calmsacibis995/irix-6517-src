#include <errno.h>
#include <assert.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <aio.h>
#include <ulocks.h>
#include <fcntl.h>
#include <sys/sysmp.h>

/*
 * A test harness
 */

static int betweenrand(int, int);
static void comp(), leave(), rmfiles();
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
	aio_sgi_init(NULL);

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
		aio_hold(AIO_HOLD_CALLBACK);
		for (i = 0; i < naio;) {
			persize = betweenrand(1, maxsize);
			perfd = rand() % nslaves;
			list[i].aio_fildes = fd[perfd];
			list[i].aio_sigevent.sigev_notify = SIGEV_CALLBACK;
			list[i].aio_sigevent.sigev_func = comp;
			list[i].aio_sigevent.sigev_value.sival_int = i;
			list[i].aio_offset = betweenrand(0, 1000000);
			list[i].aio_buf = (volatile char *)bufs;
			list[i].aio_nbytes = persize;
#if 0
			list[i].aio_reqprio = betweenrand(0, _AIO_PRIO_DELTA_MAX);
#else
			list[i].aio_reqprio = 0;
#endif
			/* hold now so nvalid can be correctly maintained */
			outstanding++;

			if (aio_write(&list[i]) != 0) {
				if (oserror() == EAGAIN)
					break;
				perror("aio_write");
				continue;
			}
			nvalid++;
			bufs += persize;
			fdsize[perfd] += persize;
			i++;
		}
		printf("aio:waiting for %d requests\n", outstanding);
		aio_hold(AIO_RELEASE_CALLBACK);
		while(outstanding) {
			static last = 0;
			if (outstanding != last) {
				printf("Still %d to go \n",outstanding);
				last = outstanding;
			}
			sginap(1);
		}
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
	while (totsize > 0) {
		/* fill in & start requests */
		bzero(alistlist, naio * sizeof(aiocb_t *));
		bufs = sbufs;

		printf("aio:doing %d requests maxsize %d\n", naio, maxsize);
		for (i = 0; i < naio;) {
			persize = betweenrand(1, maxsize);
			list[i].aio_offset = 0;
			list[i].aio_sigevent.sigev_notify = SIGEV_SIGNAL;
			list[i].aio_sigevent.sigev_signo = 0;
			list[i].aio_buf = (volatile char *)bufs;
			list[i].aio_nbytes = persize;
			list[i].aio_reqprio = 0;
			/* pick a file/slave */
			do {
				n = betweenrand(0, nslaves-1);
			} while (totsize && fdsize[n] == 0);
			if (totsize == 0)
				break;
			list[i].aio_fildes = fd[n];

			if (aio_read(&list[i]) != 0) {
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
			if (aio_suspend((const aiocb_t **)alistlist,
					i, NULL) < 0) {
				perror("aio_suspend");
			} else {
				int j;

				/* look for a done one */
				for (j = 0; j < i; j++) {
					if (!(a = alistlist[j]))
						continue;
					if (aio_error(a) != EINPROGRESS)
						break;
				}
				if (j == i) {
					printf("suspend returned but no completed io\n");
					goto bad;
				}
				n = afd[j];
				if ((rv = aio_error(a)) != 0)
					printf("aio:completion error %s\n",
					 strerror(rv));
				else {
					int ret = aio_return(a);
					if (ret != a->aio_nbytes)
						printf("aio:expected %ld bytes read/wrote %d\n", a->aio_nbytes, rv);

					fdsize[n] -= ret;
					totsize -= ret;
					if (ret == 0)
						gotzero[n]++;
				}
				alistlist[j] = NULL;
				outstanding--;

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
	/* NOTREACHED */
}

static void
rmfiles(void)
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
comp(sigval_t sv)
{
	int x = sv.sival_int;
	int rv, found, err;
	found = 0;
	/*
	 * since we can complete multiple transactions per signal
	 * we could get a signal when there is nothing left
	 */
	if (x != list[x].aio_sigevent.sigev_value.sival_int) {
		printf("XXX problem that sigval %d does not match callback of %d\n",x, list[x].aio_sigevent.sigev_value.sival_int);
	} else {
		err = aio_error(&list[x]);
		if (err ==  EINPROGRESS)
			printf("XXX problem that err for callback is still EINPROGRESS\n");
		if (err) {
			printf("XXX problem that error = %d\n",err);
			printf("aio:completion error %s\n", strerror(err));
		}
		assert(outstanding > 0);
		outstanding--;
		list[x].aio_buf = NULL;
		found = 1;
	}
	if (!found)
		printf("aio:complete callback but nothing done!\n");
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
