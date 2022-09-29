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
static void comp(), leave(), synccomp(), readcomp(), rmfiles();
volatile int outstanding, nvalid, sleeping = 1;
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
	int rv, n, i, j ,*fd, naio, maxsize, persize, *fdsize, perfd;
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
		fd[i] = open("/unix", O_RDONLY);
	}
	aio_sgi_init(NULL);

	sigset(SIGINT, leave);
	sigset(SIGHUP, leave);
	sigset(SIGTERM, leave);
	for (n = 0; n < nloops; n++) {
		/* get random # requests */
		naio = 2;
		maxsize = 2000000;

		list = malloc((naio +1) * sizeof(aiocb_t));
		alistlist = (aiocb_t **)malloc((naio + 1) * sizeof(aiocb_t *));
		sbufs = bufs = malloc(maxsize * naio);
		outstanding = 0;
		nvalid = 0;

		printf("aio:doing %d requests maxsize %d\n", naio, maxsize);
		/* fill in & start requests */
		bzero(alistlist,(naio + 1) * sizeof(aiocb_t *));
		for (i = 0; i < naio;) {
			persize = betweenrand(1, maxsize);
			perfd = rand() % nslaves;
			list[i].aio_fildes = fd[0];
			list[i].aio_sigevent.sigev_notify = SIGEV_CALLBACK;
			list[i].aio_sigevent.sigev_func = readcomp;
			list[i].aio_sigevent.sigev_value.sival_int = i;
			list[i].aio_offset = 0;
			list[i].aio_buf = (volatile char *)bufs;
			list[i].aio_nbytes = maxsize;
			list[i].aio_reqprio = 0;

			/* hold now so nvalid can be correctly maintained */
			outstanding++;

			if (aio_read(&list[i]) != 0) {
				if (oserror() == EAGAIN)
					break;
				perror("aio_write");
				continue;
			}
			nvalid++;
			bufs += persize;
			fdsize[perfd] += persize;
			alistlist[i] = &list[i];
			i++;
		}
		list[naio].aio_fildes = fd[0];
		list[naio].aio_sigevent.sigev_notify = SIGEV_CALLBACK;
		list[naio].aio_sigevent.sigev_func = synccomp;
		if(aio_suspend((const aiocb_t **)alistlist, i, NULL))
			perror("aio_fs2(aio_suspend)");
		aio_fsync(O_SYNC, &list[naio]);
		printf("aio:waiting for %d requests\n", outstanding);
		while(outstanding || sleeping) {
		}
		free(sbufs);
		free(list);
	}

	rmfiles();
	exit(0);
bad:
	rmfiles();
	exit(-1);

}

static void
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
comp(int x)
{

	int rv, found, i, err;
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
		rv = aio_return(&list[x]);
		if (rv != list[x].aio_nbytes)
			printf("aio:expected %d bytes read/wrote %d\n",
			       list[x].aio_nbytes, rv);
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
static void
synccomp(int x)
{
	printf("synccomp: suspend should have been done!\n");
	sleeping = 0;
}

static void
readcomp(int x)
{
	outstanding--;
	printf("readcomp: done reading\n");
}
