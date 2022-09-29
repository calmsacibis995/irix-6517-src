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
static void comp(), leave(), synccomp(), readcomp(), liocomp(sigval_t);
void rmfiles();
int outstanding, nvalid, sleeping = 1;
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
	int rv, n, i = 0, j ,*fd, naio, maxsize, persize, *fdsize, perfd;
	char *bufs, *sbufs;
	aiocb_t *a, **alistlist;
	aiocb_t *reallist[100];
	sigevent_t sig;
	sigval_t sigval;
	int *gotzero, *afd;
	struct stat sb;
	int wait = 0;

	Cmd = argv[0];
	nslaves = 4;
	nloops = 4;
	while ((c = getopt(argc, argv, "n:l:w")) != EOF) {
		switch (c) {
		case 'l':
		    nloops = atoi(optarg);
		    break;
		case 'n':
		    nslaves = atoi(optarg);
		    break;
		case 'w':
		    wait = 1;
		    break;
		}
	}

	fd = malloc(nslaves * sizeof(int));
	fdsize = malloc(nslaves * sizeof(int));
	fnames = malloc(nslaves * sizeof(char *));
	fd[i] = open("/etc/passwd", O_RDONLY);
	
	aio_sgi_init(NULL);

	sigset(SIGINT, leave);
	sigset(SIGHUP, leave);
	sigset(SIGTERM, leave);
	for (n = 0; n < nloops; n++) {
		/* get random # requests */
		naio = 6;
		sleeping = 1;
#if 0
		maxsize = 2000000;
#else
		maxsize = 25;
#endif

		list = malloc((naio +1) * sizeof(aiocb_t));
		sbufs = bufs = malloc(maxsize * naio);
		bzero(bufs, maxsize * naio);
		outstanding = 0;
		nvalid = 0;

		printf("aio:doing %d requests maxsize %d\n", naio, maxsize);
		/* fill in & start requests */
		for (i = 0; i < naio; i++) {
		    
			persize = betweenrand(1, maxsize);
			perfd = rand() % nslaves;
			list[i].aio_fildes = fd[0];
			list[i].aio_lio_opcode = LIO_READ;
			list[i].aio_sigevent.sigev_notify = SIGEV_NONE;
			list[i].aio_offset = persize;
			list[i].aio_buf = (volatile char *)bufs;
			list[i].aio_nbytes = persize;
			list[i].aio_reqprio = 0;
/*			printf("setting up list item %d \n",i);*/
			reallist[i] = &list[i];

			/* hold now so nvalid can be correctly maintained */
			outstanding++;

			nvalid++;
			bufs += (persize + 1);;
			fdsize[perfd] += persize;
		}
		sig.sigev_notify = SIGEV_CALLBACK;
		sig.sigev_func = liocomp;
		sig.sigev_value.sival_int = i ;
		printf("aio_lio: before lio_listio\n");
		if (wait) {
		    lio_listio(LIO_WAIT, reallist, i , &sig);
		    printf("aio_lio: after lio_listio\n");
		    sigval.sival_int = i ;
		    liocomp(sigval);
		} else {
		    lio_listio(LIO_NOWAIT, reallist, i , &sig);
		    printf("aio_lio: after lio_listio, about to sleep \n");
		    
		    while(sleeping) {
			sleep(1);
		    }
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
comp(int x)
{

	int found, i, err;
	int ret;

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

		ret = aio_return(&list[x]);
		if (err) {
			printf("XXX problem that error = %d\n",err);
			printf("nobytes = %d\n",ret);
			printf("aio:completion error %s\n", strerror(err));
		}

		if (ret != list[x].aio_nbytes)
			printf("aio:expected %d bytes read/wrote %d\n",
			       list[x].aio_nbytes, ret);
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
liocomp(sigval_t x)
{
    static int cnt = 0;
    int i, j;
    cnt++;
    j = x.sival_int;
    printf("lio: lio_listio[%d] should have been done for %d entries!\n",cnt,j);
    for (i = 0; i < j; i++) {
	printf("For item[%d] got a value of '%s' \n",i,list[i].aio_buf  );
    }
	sleeping = 0;
}





