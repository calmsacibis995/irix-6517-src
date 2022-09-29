/*************************************************************************
*                                                                        *
*               Copyright (C) 1992-1994, Silicon Graphics, Inc.       	 *
*                                                                        *
*  These coded instructions, statements, and computer programs  contain  *
*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
*  are protected by Federal copyright law.  They  may  not be disclosed  *
*  to  third  parties  or copied or duplicated in any form, in whole or  *
*  in part, without the prior written consent of Silicon Graphics, Inc.  *
*                                                                        *
**************************************************************************/
#ident  "$Revision: 1.2 $ $Author: jeffreyh $"

#define _POSIX_4SOURCE
#include "synonyms.h"
#include <errno.h>
#include "old_aio.h"
#include <assert.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <bstring.h>
#include <sys/resource.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include "old_local.h"
#include "ulocks.h"
#define SINGLE_IO_PER_FD	1

/*
 * User level aio implementation
 * Not handled:
 * 1) listio
 * 2) real time signals
 * - Use sigqueue() instead of kill() to allow client process to 
 * distinguish between signals. We can queue signals if client
 * turns on SA_SIGINFO.
#ifdef SINGLE_IO_PER_FD
 * 3) We permit only one in-progress I/O per file descriptor.
 *	- we don't have atomic seek and read AND
 *	- we currently single thread in the O.S. per inode (so atomic
 *		seek and read won't help)
#endif
 * 4) no interaction with close - make sure to wait for completion
 *	before closing file descriptor
 * 5) asuspend, lio_listio use suspsema, won't work when having multiple
 * threads calling them.
 */

struct old_aiolink _old_ahead;		/* outstanding aio requests list */
struct old_aiolink *_old_aiofree;		/* head of free list  */
ulock_t _old_alock;			/* protects list of outstanding I/O */
/* aio_suspend support - XXX multiple threads doing async I/O won't work */
ulock_t _old_susplock;
usema_t *_old_suspsema;
int _old_suspfd;		/* file descriptor for old_suspsema */
int _old_needwake;			/* someone is about to suspend waiting */
static int astarted;		/* is aio system initialized */

/*
 * gather all aio required variables into a single struct so we
 * can dynamically allocate it - should move those above in here
 * also. This saves space in the dso for folks that aren't
 * really using this stuff
 */
struct aioinfo {
	old_aiolink_t alist[_OLD_AIO_LISTIO_MAX]; /* number of queued aio */ 
#ifdef SINGLE_IO_PER_FD
	old_aiolink_t **afd;		/* inprogress I/O per fd */
#endif
	int maxfd;			/* maximum open file descriptors */
	usema_t *wsema;		/* I/O thread wait */
	char *afile;		/* arena file for synchronization */
	pid_t *spids;		/* pids of I/O performing threads */
	sigset_t inuseset;	/* set of 'in-use' for aio signals */
};
static struct aioinfo *_aioinfo;

fd_set _old_suspset;		/* select fd set -- XXX get rid of it */

static ssize_t sread(old_aiocb_t *);
static ssize_t swrite(old_aiocb_t *);
static void slave(void *);
#ifdef DEBUG
static void dumpit(void);
#endif
int _old_aqueue(struct old_aiocb *, int);

/*
 * __old_aio_init(maxaio)
 * maxaio tells how many slaves to create in order to process aio requests
 */
void
__old_aio_init(int maxaio)
{
	usptr_t *handle;
	long i, oldstacksize;
	long otype, ousers;

	if (astarted)
		return;
#ifdef DEBUG
	sigset(SIGUSR2, dumpit);
#endif
	sigset(SIGHUP, SIG_DFL);	/* for TERMCHILD to work */

	if (_aioinfo == NULL && (_aioinfo = calloc(1, sizeof(*_aioinfo))) == NULL) {
		perror("aio:nomem");
		exit(-1);
	}
	sigemptyset(&_aioinfo->inuseset);
	/* set up private arena */
	otype = usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	ousers = usconfig(CONF_INITUSERS, maxaio+1);
	_aioinfo->afile = tempnam(NULL, "aio");
	if ((handle = usinit(_aioinfo->afile)) == NULL) {
		perror("aio:usinit");
		exit(-1);
	}
	usconfig(CONF_ARENATYPE, otype);
	usconfig(CONF_INITUSERS, ousers);

	/* allocate queue lock */
	_old_alock = usnewlock(handle);
	assert(_old_alock);

	/* allocate waiting semaphore */
	_aioinfo->wsema = usnewsema(handle, 0);
	assert(_aioinfo->wsema);

	/* allocate suspend lock */
	_old_susplock = usnewlock(handle);
	assert(_old_susplock);

	/* allocate iosuspend semaphore */
	_old_suspsema = usnewpollsema(handle, 0);
	assert(_old_suspsema);
	if ((_old_suspfd = usopenpollsema(_old_suspsema, 0600)) < 0) {
		perror("aio:usopenpollsema");
		exit(-1);
	}
	FD_ZERO(&_old_suspset);
	FD_SET(_old_suspfd, &_old_suspset);

	/* chain up free aiolinks */
	for (i = 1; i < _OLD_AIO_LISTIO_MAX; i++)
		_aioinfo->alist[i].al_forw = &_aioinfo->alist[i-1];
	_old_aiofree = &_aioinfo->alist[_OLD_AIO_LISTIO_MAX-1];
	_aioinfo->alist[0].al_forw = NULL;
	_old_ahead.al_forw = &_old_ahead;
	_old_ahead.al_back = &_old_ahead;

	_aioinfo->maxfd = getdtablesize();
#ifdef SINGLE_IO_PER_FD
	/* alloc per fd in progress list */
	_aioinfo->afd = calloc((size_t)_aioinfo->maxfd, sizeof(_aioinfo->afd));
	bzero(_aioinfo->afd, (_aioinfo->maxfd) * (int) sizeof (_aioinfo->afd));
#endif

	/* crank up slaves */
	oldstacksize = prctl(PR_GETSTACKSIZE);
	prctl(PR_SETSTACKSIZE, 64 * 1024);
	_aioinfo->spids = malloc(sizeof(pid_t) * (size_t)maxaio);
	for (i = 0; i < maxaio; i++) {
		if ((_aioinfo->spids[i] = sproc(slave, OLD_AIO_SPROC, 0)) < 0) {
			perror("aio:sproc");
			while (--i >= 0)
				if (_aioinfo->spids[i] > 0)
					kill(_aioinfo->spids[i], SIGKILL);
			exit(-1);
		}
	}
	prctl(PR_SETSTACKSIZE, oldstacksize);
	astarted++;
#ifdef DEBUG
	usctlsema(_aioinfo->wsema, CS_HISTON);
	usconfig(CONF_HISTON, handle);
#endif
}

/* ARGSUSED */
static void
slave(void *dummy)
{
	/* make volatile so we can guarentee write order */
	register volatile old_aiocb_t *a;
	register struct old_aiolink *al;
	ssize_t rv;
	pid_t rpid;

	/* set up so if our parent dies we go away */
	prctl(PR_TERMCHILD);
	if (getppid() == 1)
		exit(0);
	(void) usinit(_aioinfo->afile);
	for (;;) {
		/*
		 * pull first untaken request off
		 * Always scan list since we may have skipped over a
		 * request becuase of other outstanding requests for the
		 * same fd
		 */
		ussetlock(_old_alock);
		for (al = _old_ahead.al_forw; al != &_old_ahead; al = al->al_forw) {
			/*
			 * no one servicing
#ifdef SINGLE_IO_PER_FD
			 * and no other inprogress I/O on this file descriptor
#endif
			 */
			if (al->al_spid == 0
#ifdef SINGLE_IO_PER_FD
				&& _aioinfo->afd[al->al_fd] == NULL
#endif
				)
				break;
		}

		if (al == &_old_ahead) {
                        /* nothing there?? */
                        usunsetlock(_old_alock);
                        /* wait for request */
                        uspsema(_aioinfo->wsema);
                        continue;
                }


		/* mark as inprogress */
		al->al_spid = get_pid();	/* use cached pid */
#ifdef SINGLE_IO_PER_FD
		_aioinfo->afd[al->al_fd] = al;
#endif
		usunsetlock(_old_alock);

		a = (volatile old_aiocb_t *)al->al_req;
		rpid = al->al_rpid;
		if (al->al_op == OLD_LIO_READ)
			rv = sread((old_aiocb_t *)a);
		else
			rv = swrite((old_aiocb_t *)a);

		/* transaction done - pull off queue */
		ussetlock(_old_alock);
		al->al_back->al_forw = al->al_forw;
		al->al_forw->al_back = al->al_back;
		al->al_back = NULL; /* debugging */
		al->al_forw = _old_aiofree;
		_old_aiofree = al;
#ifdef SINGLE_IO_PER_FD
		assert(_aioinfo->afd[al->al_fd] == al);
		_aioinfo->afd[al->al_fd] = NULL;
#endif

		if (rv < 0) {
			a->aio_nobytes = (size_t)-1;
			a->aio_errno = oserror();
		} else {
			a->aio_nobytes = (size_t)rv;
			a->aio_errno = 0;
		}
		usunsetlock(_old_alock);

		if (a->aio_sigevent.sigev_signo) {
			/* signal requester */
			if (rpid > 0) {
				if (kill(rpid, a->aio_sigevent.sigev_signo) < 0) {
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
		ussetlock(_old_susplock);
		if (_old_needwake) {
			_old_needwake = 0;
			usvsema(_old_suspsema);
		}
		usunsetlock(_old_susplock);
	}
}

int
_old_aqueue(struct old_aiocb *aio, int op)
{
	register struct old_aiolink *al, *al1;
	sigset_t oset;
	int fd = aio->aio_fildes;
	int flag;


	if (!astarted)
		__old_aio_init(_OLD_AIO_MAX);
	if (aio->aio_reqprio < 0 || 
			aio->aio_reqprio > _OLD_AIO_PRIO_DELTA_MAX) {
		aio->aio_errno = EINVAL;
		setoserror(EINVAL);
		return(-1);
	}
	if (fd < 0 || fd >= _aioinfo->maxfd) {
		aio->aio_errno = EBADF;
		setoserror(EBADF);
		return(-1);
	}
	if ((flag = fcntl(fd, F_GETFL)) < 0) {
		aio->aio_errno = oserror();
		return(-1);
	}
	if (flag & O_APPEND)
		aio->aio_whence = SEEK_END;	/* seek to end */
	else
		aio->aio_whence = SEEK_SET;	/* seek to aio_offset */
		

	/*
	 * block possible signals since otherwise we could deadlock
	 * since user's signal handler could start another transaction
	 * ??
	 */
	sigprocmask(SIG_BLOCK, &_aioinfo->inuseset, &oset);
	ussetlock(_old_alock);

	/* pull one off free list */
	if (_old_aiofree->al_forw == NULL) {
		usunsetlock(_old_alock);
		/* XXX what if a non-blocked signal comes in and
		 * changes the mask?? We need a sigdiffset()!
		 */
		sigprocmask(SIG_SETMASK, &oset, NULL);
		aio->aio_errno = EAGAIN;
		setoserror(EAGAIN);
		return(-1);
	}
	al = _old_aiofree->al_forw;
	_old_aiofree->al_forw = al->al_forw;

	/* fill in info */
	al->al_fd = fd;
	al->al_req = aio;
	al->al_rpid = get_pid();	/* use cached pid, for speed */
	al->al_op = op;
	al->al_spid = 0;		/* noone handling yet */
	/* lower pri value means higher priority */
	al->al_sysprio = getpriority(PRIO_PROCESS, 0) + aio->aio_reqprio;
	/* init return values */
	aio->aio_nobytes = 0;
	aio->aio_errno = EINPROGRESS;
	aio->aio_ret = 0;		/* status hasn't been returned yet */

	/* thread onto queue in priority order */
	for (al1 = _old_ahead.al_forw; al1 != &_old_ahead; al1 = al1->al_forw) {
		if (al1->al_sysprio > al->al_sysprio) {
			/* insert before al1 */
			al->al_forw = al1;
			al->al_back = al1->al_back;
			al1->al_back->al_forw = al;
			al1->al_back = al;
			break;
		}
	}
	if (al1 == &_old_ahead) {
		/* insert at end */
		al->al_back = _old_ahead.al_back;
		_old_ahead.al_back = al;
		al->al_back->al_forw = al;
		al->al_forw = &_old_ahead;
	}
	usunsetlock(_old_alock);

	/* want signal posted upon completion */
	if (aio->aio_sigevent.sigev_signo)
		sigaddset(&_aioinfo->inuseset, aio->aio_sigevent.sigev_signo);

	/* XXX for now this list just grows */
	sigprocmask(SIG_SETMASK, &oset, NULL);

	/* kick a slave */
	usvsema(_aioinfo->wsema);
	return(0);
}



#ifdef DEBUG
static void
dumpit(void)
{
	FILE *fd;

	fd = fopen("aiodump", "w");
	usdumphist(_aioinfo->wsema, fd);
	fflush(fd);
}
#endif

/*
 * seek-n-read/write
 */
static ssize_t
sread(old_aiocb_t *aio)
{
	off_t offset = (aio->aio_whence == SEEK_END) ? 0 : aio->aio_offset;

	if (lseek(aio->aio_fildes, offset, aio->aio_whence) == -1) {
		if (oserror() != ESPIPE)
			return(-1);
	}
	return(read(aio->aio_fildes, (char *)aio->aio_buf, aio->aio_nbytes));
}
static ssize_t
swrite(old_aiocb_t *aio)
{
	off_t offset = (aio->aio_whence == SEEK_END) ? 0 : aio->aio_offset;

	if (lseek(aio->aio_fildes, offset, aio->aio_whence) == -1) {
		if (oserror() != ESPIPE)
			return(-1);
	}
	return(write(aio->aio_fildes, (char *)aio->aio_buf, aio->aio_nbytes));
}
