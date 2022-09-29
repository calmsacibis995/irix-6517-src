#define _POSIX_4SOURCE
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <bstring.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/prctl.h>
#include "aio.h"
#include "ulocks.h"
#define SINGLE_IO_PER_FD	1

/*
 * User level aio implementation
 * Not handled:
 * 1) listio
 * 2) real time signals
#ifdef SINGLE_IO_PER_FD
 * 3) We permit only one in-progress I/O per file descriptor.
 *	- we don't have atomic seek and read AND
 *	- we currently single thread in the O.S. per inode (so atomic
 *		seek and read won't help)
#endif
 * 4) no interaction with close - make sure to wait for completion
 *	before closing file descriptor
 *
 * aio_read:
 *	checks for valid fd, whence are not done prior to queueing
 */

#define DEFMAXAIO	4		/* default maximum outstanding aios */
#define MAXQAIO		255		/* maximum queued aios */

#define AREAD		1
#define AWRITE		2
struct aiolink {
	struct aiolink	*al_forw;
	struct aiolink	*al_back;
	aiocb_t *al_req;
	pid_t	al_spid;	/* slave pid handling request */
	pid_t	al_rpid;	/* requesting pid */
	int	al_fd;		/* fd to do I/O on */
	int	al_op;		/* read or write */
};

static struct aiolink alist[MAXQAIO];
static struct aiolink ahead, *aiofree;
static ulock_t alock;		/* protects list of outstanding I/O */
#ifdef SINGLE_IO_PER_FD
static struct aiolink **afd;	/* inprogress I/O per fd */
#endif
static maxfd;			/* maximum open file descriptors */
static usema_t *wsema;		/* I/O thread wait */
static char *afile;		/* arena file for synchronization */
static int astarted = 0;	/* is aio system initialized */
/* aio_suspend support - XXX multiple threads doing async I/O won't work */
static ulock_t susplock;
static usema_t *suspsema;
static int suspfd;		/* file descriptor for suspsema */
static fd_set suspset;		/* select fd set */
static int needwake = 0;	/* someone is about to suspend waiting */
static pid_t *spids;		/* pids of I/O performing threads */
static sigset_t inuseset;	/* set of 'in-use' for aio signals */

static int sread(int, aiocb_t *);
static int swrite(int, aiocb_t *);
static void slave(), dumpit();
static int aqueue(int, struct aiocb *, int);

void
_aio_init(int maxaio)
{
	usptr_t *handle;
	int i, oldstacksize;

	if (astarted)
		return;
#ifdef DEBUG
	sigset(SIGUSR2, dumpit);
#endif
	sigset(SIGHUP, SIG_DFL);	/* for TERMCHILD to work */
	sigemptyset(&inuseset);

	/* set up private arena */
	usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	usconfig(CONF_INITUSERS, maxaio+1);


	afile = tempnam(NULL, "aio");
	if ((handle = usinit(afile)) == NULL) {
		perror("aio:usinit");
		exit(-1);
	}

	/* allocate queue lock */
	alock = usnewlock(handle);
	assert(alock);

	/* allocate waiting semaphore */
	wsema = usnewsema(handle, 0);
	assert(wsema);

	/* allocate suspend lock */
	susplock = usnewlock(handle);
	assert(susplock);

	/* allocate iosuspend semaphore */
	suspsema = usnewpollsema(handle, 0);
	assert(suspsema);
	if ((suspfd = usopenpollsema(suspsema, 0600)) < 0) {
		perror("aio:usopenpollsema");
		exit(-1);
	}
	FD_ZERO(&suspset);
	FD_SET(suspfd, &suspset);

	/* chain up free aiolinks */
	for (i = 1; i < MAXQAIO; i++)
		alist[i].al_forw = &alist[i-1];
	aiofree = &alist[MAXQAIO-1];
	alist[0].al_forw = NULL;
	ahead.al_forw = &ahead;
	ahead.al_back = &ahead;

	maxfd = getdtablesize();
#ifdef SINGLE_IO_PER_FD
	/* alloc per fd in progress list */
	afd = calloc(maxfd, sizeof(afd));
#endif

	/* crank up slaves */
	oldstacksize = prctl(PR_GETSTACKSIZE);
	prctl(PR_SETSTACKSIZE, 64 * 1024);
	spids = malloc(sizeof(pid_t) * maxaio);
	for (i = 0; i < maxaio; i++) {
		if ((spids[i] = sproc(slave, PR_SALL, 0)) < 0) {
			perror("aio:sproc");
			while (--i >= 0)
				if (spids[i] > 0)
					kill(spids[i], SIGKILL);
			exit(-1);
		}
	}
	prctl(PR_SETSTACKSIZE, oldstacksize);
	astarted++;
#ifdef DEBUG
	usctlsema(wsema, CS_HISTON);
	usconfig(CONF_HISTON, handle);
#endif
}

static void
slave()
{
	/* make volatile so we can guarentee write order */
	register volatile aiocb_t *a;
	register struct aiolink *al;
	usptr_t *handle;
	int rv;
	pid_t rpid;

	/* set up so if our parent dies we go away */
	prctl(PR_TERMCHILD);
	if (getppid() == 1)
		exit(0);
	handle = usinit(afile);
	assert(handle);
	for (;;) {
		/*
		 * pull first untaken request off
		 * Always scan list since we may have skipped over a
		 * request becuase of other outstanding requests for the
		 * same fd
		 */
		ussetlock(alock);
		for (al = ahead.al_forw; al != &ahead; al = al->al_forw) {
			/*
			 * no one servicing
#ifdef SINGLE_IO_PER_FD
			 * and no other inprogress I/O on this file descriptor
#endif
			 */
			if (al->al_spid == 0
#ifdef SINGLE_IO_PER_FD
				&& afd[al->al_fd] == NULL
#endif
				)
				break;
		}
		if (al == &ahead) {
			/* nothing there?? */
			usunsetlock(alock);
			/* wait for request */
			uspsema(wsema);
			continue;
		}

		/* mark as inprogress */
		al->al_spid = getpid();
#ifdef SINGLE_IO_PER_FD
		afd[al->al_fd] = al;
#endif
		usunsetlock(alock);

		a = (volatile aiocb_t *)al->al_req;
		rpid = al->al_rpid;
		if (al->al_op == AREAD)
			rv = sread(al->al_fd, (aiocb_t *)a);
		else
			rv = swrite(al->al_fd, (aiocb_t *)a);

		/* transaction done - pull off queue */
		ussetlock(alock);
		al->al_back->al_forw = al->al_forw;
		al->al_forw->al_back = al->al_back;
		al->al_back = NULL; /* debugging */
		al->al_forw = aiofree;
		aiofree = al;
#ifdef SINGLE_IO_PER_FD
		assert(afd[al->al_fd] == al);
		afd[al->al_fd] = NULL;
#endif

		if (rv < 0) {
			a->aio_nobytes = -1;
			a->aio_errno = oserror();
		} else {
			a->aio_nobytes = rv;
			a->aio_errno = 0;
		}
		usunsetlock(alock);

		if (a->aio_flag & AIO_EVENT) {
			/* signal requester */
			if (rpid > 0) {
				if (kill(rpid, a->aio_event.sigev_signo) < 0) {
					fprintf(stderr, "notification to pid %d failed:%s\n",
						rpid, strerror(oserror()));
					exit(-1);
				}
			} else {
				fprintf(stderr, "bad req pid %d\n", rpid);
				exit(-1);
			}
		}
		/* synchronize with aio_suspend */
		ussetlock(susplock);
		if (needwake) {
			needwake = 0;
			usvsema(suspsema);
		}
		usunsetlock(susplock);
	}
}

static int
aqueue(int fd, struct aiocb *aio, int op)
{
	register struct aiolink *al, *al1;
	sigset_t oset;

	if (!astarted)
		_aio_init(DEFMAXAIO);
	if (aio->aio_reqprio < AIO_PRIO_MIN || aio->aio_reqprio > AIO_PRIO_MAX) {
		setoserror(EINVAL);
		return(-1);
	}
	if (fd < 0 || fd >= maxfd) {
		setoserror(EINVAL);
		return(-1);
	}

	/*
	 * block possible signals since otherwise we could deadlock
	 * since user's signal handler could stgart another transaction
	 */
	sigprocmask(SIG_BLOCK, &inuseset, &oset);
	ussetlock(alock);

	/* pull one off free list */
	if (aiofree->al_forw == NULL) {
		usunsetlock(alock);
		/* XXX what if a non-blocked signal comes in and
		 * changes the mask?? We need a sigdiffset()!
		 */
		sigprocmask(SIG_SETMASK, &oset, NULL);
		setoserror(EAGAIN);
		return(-1);
	}
	al = aiofree->al_forw;
	aiofree->al_forw = al->al_forw;

	/* fill in info */
	al->al_fd = fd;
	al->al_req = aio;
	al->al_rpid = getpid();
	al->al_op = op;
	al->al_spid = 0;		/* noone handling yet */
	/* init return values */
	aio->aio_nobytes = 0;
	aio->aio_errno = EINPROG;
	aio->aio_handle = aio;

	/* thread onto queue in priority order */
	for (al1 = ahead.al_forw; al1 != &ahead; al1 = al1->al_forw) {
		if (al1->al_req->aio_reqprio < al->al_req->aio_reqprio) {
			/* insert before al1 */
			al->al_forw = al1;
			al->al_back = al1->al_back;
			al1->al_back->al_forw = al;
			al1->al_back = al;
			break;
		}
	}
	if (al1 == &ahead) {
		/* insert at end */
		al->al_back = ahead.al_back;
		ahead.al_back = al;
		al->al_back->al_forw = al;
		al->al_forw = &ahead;
	}
	usunsetlock(alock);

	if (aio->aio_flag & AIO_EVENT)
		sigaddset(&inuseset, aio->aio_event.sigev_signo);

	/* XXX for now this list just grows */
	sigprocmask(SIG_SETMASK, &oset, NULL);

	/* kick a slave */
	usvsema(wsema);
	return(0);
}

int
aio_read(int fd, struct aiocb *aio)
{
	return(aqueue(fd, aio, AREAD));
}

int
aio_write(int fd, struct aiocb *aio)
{
	return(aqueue(fd, aio, AWRITE));
}

int
aio_suspend(int cnt, const aiocb_t *list[])
{
	register int i, rv;
	register const aiocb_t *a;
	register struct aiolink *al;
	fd_set lfd;
	int ninprog;
	static int suspwait = 0;	/* already have a wait pending?? */

	for (;;) {
		/*
		 * Hold lock while scanning so that any completing
		 * I/O can decide whether to signal us or not
		 */
		ussetlock(susplock);
		for (i = 0, ninprog = 0; i < cnt; i++) {
			if ((a = list[i]) == NULL)
				continue;
			if (a->aio_errno != EINPROG) {
				usunsetlock(susplock);
				return(0);
			}
			ninprog++;
		}
		if (!ninprog) {
			/* POSIX doesn't call this an error but
			 * why wait for nothing??
			 */
			usunsetlock(susplock);
			setoserror(ESRCH);
			return(-1);
		}
		needwake++;
		usunsetlock(susplock);

		/*
		 * nothing ready yet
		 * If already have a wait pending, just go back to select
		 */
		if (!suspwait) {
			/* no outstanding wait yet */
			if (uspsema(suspsema) == 1) {
				/* fast service! */
				assert(needwake == 0);
				continue;
			}
			suspwait++;
		}
		lfd = suspset;
		rv = select(suspfd+1, &lfd, NULL, NULL, NULL);
		if (rv < 0) {
			if (oserror() != EINTR) {
				perror("aio:select error");
				exit(-1);
			}
			return(-1);
		} else if (rv != 1) {
			fprintf(stderr, "select returned %d!\n", rv);
			exit(-1);
		}
		suspwait = 0;
		assert(needwake == 0);
		/* something now ready - look again */
	}
	/* NOTREACHED */
}

int
aio_cancel(int fd, struct aiocb *aio)
{
	register aiocb_t *a;
	register struct aiolink *al, *alf;
	int inprogress = 0;
	int queued = 0;

	ussetlock(alock);
	for (al = ahead.al_forw; al != &ahead; al = alf) {
		alf = al->al_forw;
		if ((aio && aio == al->al_req) || (al->al_fd == fd)) {
			assert(al->al_fd == fd);
			if (al->al_spid) {
				/* its been started */
				inprogress++;
			} else {
				queued++;
				/* remove from queue */
				a->aio_nobytes = -1;
				a->aio_errno = ECANCELED;
				al->al_back->al_forw = al->al_forw;
				al->al_forw->al_back = al->al_back;
				al->al_back = NULL; /* debugging */
				al->al_forw = aiofree;
				aiofree = al;
			}
		}
	}
	usunsetlock(alock);
	if (inprogress)
		return(AIO_NOTCANCELED);
	else if (queued)
		return(AIO_CANCELED);
	return(AIO_ALLDONE);
}

#ifdef DEBUG
static void
dumpit()
{
	FILE *fd;

	fd = fopen("aiodump", "w");
	usdumphist(wsema, fd);
	fflush(fd);
}
#endif

/*
 * seek-n-read/write
 */
static int
sread(int fd, aiocb_t *aio)
{
	if (lseek(fd, aio->aio_offset, aio->aio_whence) == -1)
		return(-1);
	return(read(fd, (char *)aio->aio_buf, aio->aio_nbytes));
}
static int
swrite(int fd, aiocb_t *aio)
{
	if (lseek(fd, aio->aio_offset, aio->aio_whence) == -1)
		return(-1);
	return(write(fd, (char *)aio->aio_buf, aio->aio_nbytes));
}
