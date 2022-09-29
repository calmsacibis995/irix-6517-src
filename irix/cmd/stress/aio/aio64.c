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

/*
 * A test harness
 */

static int betweenrand(int, int);
static void comp(), leave();
void rmfiles();
int outstanding, nvalid;
int nslaves;
char **fnames;
aiocb64_t *list;

main(argc, argv)
int argc;
char **argv;
{
	extern int	optind;
	extern char	*optarg;
	int		totsize, c, nloops;
	char *Cmd;
	int rv, n, i,  naio, maxsize, *fd, persize, perfd,  *fdsize;
	char *bufs, *sbufs;
	aiocb64_t *a, **alistlist;
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
		fnames[i] = tempnam(NULL, "aio");
		fd[i] = open(fnames[i], O_RDWR|O_CREAT|O_TRUNC, 0666);
	}

	aio_sgi_init64(NULL);
	{
		sigaction_t newact;
		/* set up sigaction */
		sigemptyset(&newact.sa_mask);
		newact.sa_flags = SA_SIGINFO;
		newact.sa_handler = comp;
		if (sigaction(SIGUSR1, &newact, (sigaction_t *)0)) {	
			perror("Failed sigaction");	
			exit(1);
		}
	}
	sigset(SIGINT, leave);
	sigset(SIGHUP, leave);
	sigset(SIGTERM, leave);
	for (n = 0; n < nloops; n++) {
		/* get random # requests */
		naio = betweenrand(10, 500);
		maxsize = betweenrand(1, 20000);

		list = malloc(naio * sizeof(aiocb64_t));
		sbufs = bufs = malloc(maxsize * naio);
		outstanding = 0;
		nvalid = 0;

		printf("aio(1):writing %d requests maxsize %d loop %d\n", naio, maxsize, n);
		/* fill in & start requests */
		for (i = 0; i < naio;) {
			persize = betweenrand(1, maxsize);
			perfd = rand() % nslaves;
			list[i].aio_fildes = fd[perfd];
			list[i].aio_sigevent.sigev_notify = SIGEV_SIGNAL;
			list[i].aio_sigevent.sigev_signo = SIGUSR1;
			list[i].aio_sigevent.sigev_value.sival_int =i;
			list[i].aio_offset = betweenrand(0, 1000000);
			list[i].aio_buf = (volatile char *)bufs;
			list[i].aio_nbytes = persize;
#if 0
			list[i].aio_reqprio = betweenrand(0, _AIO_PRIO_DELTA_MAX);
#else
			list[i].aio_reqprio = 0;
#endif
			/* hold now so nvalid can be correctly maintained */
			sighold(SIGUSR1);
		    tryagain1:
			if (aio_write64(&list[i]) != 0) {
				sigrelse(SIGUSR1);
				if (oserror() == EAGAIN)
					goto tryagain1;
				perror("aio_write64");
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
	list = malloc(naio * sizeof(aiocb64_t));
	alistlist = (aiocb64_t **)malloc(naio * sizeof(aiocb64_t *));
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
		bzero(alistlist, naio * sizeof(aiocb64_t *));
		bufs = sbufs;

		printf("aio(2):reading %d requests maxsize %d\n", naio, maxsize);
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
		      again:
			if (aio_read64(&list[i]) != 0) {
				if (oserror() == EAGAIN) {
#if 0
				  /* We now will get eagain as
				   * we only keep 4 going at a time
				   * and POSIX wants us to return EAGAIN */
					printf("got EAGAIN from read!\n");
#endif
					goto again;
				}
				perror("aio_read64");
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
			if (aio_suspend64((const aiocb64_t **)alistlist,
					i, NULL) < 0) {
				perror("aio_suspend64");
			} else {
				int j;

				/* look for a done one */
				for (j = 0; j < i; j++) {
					if (!(a = alistlist[j]))
						continue;
					if (aio_error64(a) != EINPROGRESS)
						break;
				}
				if (j == i) {
					printf("suspend returned but no completed io\n");
					goto bad;
				}
				n = afd[j];
				if ((rv = aio_error64(a)) != 0)
					printf("aio:completion error %s\n",
					       strerror(rv));
				else {
					int ret = aio_return64(a);
					if (ret != a->aio_nbytes)
						printf("aio:expected %d bytes read/wrote %d\n", aio->aio_nbytes, ret);
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

}

void
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
comp(int sig, siginfo_t *si, void *scp)
{
	int rv, found, i, err;

	/*
	 * since we can complete multiple transactions per signal
	 * we could get a signal when there is nothing left
	 */
#if 0
	printf("sig %d code %d val %d\n",si->si_signo, si->si_code,si->si_value.sival_int);
#endif
	if (outstanding == 0)
		return;
	for (i = 0, found = 0; i < nvalid; i++) {
		if (list[i].aio_buf == NULL)
			continue;
		if ((err = aio_error64(&list[i])) != EINPROGRESS) {
			found++;
			if (err) {
				printf("error = %d\n",err);
				printf("aio:completion error %s\n", strerror(err));
			}
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
