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
#ident  "$Revision: 1.60 $ $Author: jph $"

#include "synonyms.h"
#include <pthread.h>
#include <mplib.h>
#include <errno.h>
#include <aio.h>
#include <assert.h>
#include <limits.h>
#include <signal.h>
#define _pread _preadx
#define _pwrite _pwritex
#include <unistd.h>
#undef _pread
#undef _pwrite
#include <string.h>
#include <bstring.h>
#include <sys/resource.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/mman.h>
#include <sys/syssgi.h>
#include "local.h"
#include "priv_extern.h"
#include "../signal/sig_extern.h"
#include "ulocks.h"
#include <sys/stat.h>
#include <semaphore.h>
/* #define DEBUG */ /*XXX */

/*
 * User level aio implementation
 * Not handled:
 * 1) no interaction with close - make sure to wait for completion
 *	before closing file descriptor
 */


/*
 * gather all aio required variables into a single struct so we
 * can dynamically allocate it.
 * This saves space in the dso for folks that aren't
 * really using this stuff
 */
aiowaitsema_t * _aio_new_waitsema(int);
static void _aio_release_alock_nosig(void);
static void _aio_grab_alock_nosig(void);
struct aioinfo *_aioinfo _INITBSS;
static void _aio_reset_signals(void);
static void slave(void *, size_t);
static void ctlslave(void *, size_t);
#ifdef DEBUG
int _lastowner;
char * _lastaddr;
int _lastpowner;
char * _lastpaddr;
static void dumpit();
#endif


extern ssize_t _pread(int, void *, size_t, off64_t);
extern ssize_t _pwrite(int, const void *, size_t, off64_t);


/*
 * __aio_reset(void)
 * used by fork and sproc to free old _aioinfo structure.
 */
void
__aio_reset(void)
{
	int	old_val = __us_rsthread_malloc;
	if (_aioinfo) {

		__us_rsthread_malloc = 0;	/* disable locking */
		free(_aioinfo->activefds);
		if (KAIO_IS_ACTIVE)
		     libdba_kaio_reset();
		free(_aioinfo);
		_aioinfo = NULL;
		__us_rsthread_malloc = old_val;	/* reenable locking */
	}
}

/*
 * __aio_closechk(int fd)
 * called by close when using aio to notice when raw-kaio descriptors
 * have closed and also deal with operations that are still in progress.
 */
void
__aio_closechk(int fd)
{
	/*	struct aiocb a; */
	/* This function is called only if _aioinfo is set */
	if (KAIO_IS_ACTIVE)
	     libdba_kaio_closechk(fd);
	/*
	 * Close needs to wait for all currently outstanding
	 * aio operations to finish
	 */
	if (USR_AIO_IS_ACTIVE)
	     aio_cancel(fd, NULL);
}

/*
 * __aio_init(maxaio, numlocks, numusers)
 * maxaio tells how many slaves to create in order to process aio requests
 * numlocks is the number of semaphores for waitsema to preallocate
 * numusers is the number of user threads that might access the arena
 */
void
__aio_init(int maxaio, int numlocks, int numusers)
{
     if (getenv("__SGI_USE_DBA"))
	  aio_sgi_init(NULL);
     else
	  __aio_init1(maxaio, numlocks, numusers, INIT_DEFAULT);
}

void
__aio_init1(int maxaio, int numlocks, int numusers, int initlevel)
{
    usptr_t *handle;
    long i;
    long otype, ousers, osize, oaddress, odebug;
    sigset_t oldsigset;
    int numthreads = maxaio + 1; /* number of threads is number user
				  * wanted plus one for ctl messages
				  */
    int naio =  _AIO_SGI_MAX;  /* This is the total number of AIO
			        * allowed per process in the system
			        */
    int pthreads = (_mplib_get_thread_type() == MT_PTHREAD);
    struct aioinfo *aioinfo;
    struct sh_aio_kaio *sh_a_k = 0;
    LOCKDECLINIT(l, LOCKMISC);

    if (_aioinfo && ((initlevel != INIT_FORCE_SPROCS)
		     || _aioinfo->all_init_done)) {
	    UNLOCKMISC(l);
	    /* printf(" Warning, calling aio_sgi_init() multiple times \n"); */
	    return;
    }
    if (!_aioinfo) { /* !_aioinfo */
	 if ((aioinfo = calloc(1, sizeof(*_aioinfo))) == NULL)  {
	      UNLOCKMISC(l);
	      perror("aio_sgi_init(nomem)");
	      exit(-1);
	 }
    aioinfo->alistfreecount = naio;
    aioinfo->sh_aio_kaio = sh_a_k;

    if (!pthreads) {
	/*
	 * Turn on user-level sigprocmask.
	 */
	__sgi_prda_procmask(USER_LEVEL);

#ifdef DEBUG
	sigset(SIGUSR2, dumpit);
#endif

    }

    /*
     * We need to block all non fatal signals when holding the _aioinfo->alock
     * here we build a mask to make that easier
     */
    for (i = 1; i<= MAXSIG; i++) {
	if (i != SIGINT && i != SIGBUS && i != SIGILL &&
	    i != SIGABRT && i != SIGKILL && i != SIGSEGV && i != SIGHUP
	    && i != SIGTERM )
		sigaddset(&aioinfo->sigblockset, (int)i);
    }

    /* set up private arena */
    otype = usconfig(CONF_ARENATYPE, US_SHAREDONLY);
    ousers = usconfig(CONF_INITUSERS, numthreads + numusers );
#ifdef DEBUG
    odebug = usconfig(CONF_LOCKTYPE, US_DEBUGPLUS);
#else
    odebug = usconfig(CONF_LOCKTYPE, US_NODEBUG);
#endif
    osize = usconfig(CONF_INITSIZE, 65536 * numthreads);
    oaddress = usconfig(CONF_ATTACHADDR, (void *)~0L); 
    if ((handle = usinit("/dev/zero")) == NULL) {
	UNLOCKMISC(l);
	perror("aio_sgi_init(usinit failed opening /dev/zero)");
	exit(-1);
    }
    aioinfo->ahandle = handle;
    usconfig(CONF_ARENATYPE, otype);
    usconfig(CONF_INITUSERS, ousers);
    usconfig(CONF_LOCKTYPE, odebug);
    usconfig(CONF_INITSIZE, osize);
    usconfig(CONF_ATTACHADDR, oaddress); 

    /* initialize queue lock */
    sem_init(&aioinfo->alock, 0, 1);
    sem_mode(&aioinfo->alock, SEM_MODE_NOCNCL);

    /* allocate callback lock */
    aioinfo->acblock = usnewlock(handle);
    assert(aioinfo->acblock);

    /* allocate waiting semaphores */
    aioinfo->wsema = usnewsema(handle, 0);
    assert(aioinfo->wsema);
    aioinfo->wcsema = usnewsema(handle, 0);
    assert(aioinfo->wcsema);
    
    /* chain up free aiolinks */
    for (i = 1; i < naio ; i++)
	aioinfo->alist[i].al_forw = &aioinfo->alist[i-1];

    aioinfo->maxfd = getdtablesize();
    aioinfo->activefds = calloc(aioinfo->maxfd, sizeof(aioinfo->activefds));
    aioinfo->aiofree = &aioinfo->alist[naio - 1];
    aioinfo->alist[0].al_forw = NULL;
	/* Clear the outstanding I/O queue */
    aioinfo->ahead.al_forw = &aioinfo->ahead;
    aioinfo->ahead.al_back = &aioinfo->ahead;
    aioinfo->ahead.al_busy = 1; /* Must be non-zero */

	/* Clear the outstanding AIO control queue */
    aioinfo->achead.al_forw = &aioinfo->achead;
    aioinfo->achead.al_back = &aioinfo->achead;
    aioinfo->achead.al_busy = 1; /* Must be non-zero */

    aioinfo->waitfree = NULL;
    aioinfo->lioaiocbfree = NULL;
    aioinfo->waitcanfree = NULL; /* XXX need to preallocate some of these */
	/*
	 * Do a little init for aio_hold
	 */
    aioinfo->aiocbbeingheld = 0;
    aioinfo->aiocbholdlist = NULL;
    
#ifdef DEBUG
    usctlsema(aioinfo->wsema, CS_HISTON);
    usctlsema(aioinfo->wcsema, CS_HISTON);
    usconfig(CONF_HISTON, handle);
#endif

    /* All initialisation done - make it visible to the world.
     */
    _aioinfo = aioinfo;
    /* If just kaio, don't do the rest yet */
    if (initlevel == INIT_KAIO) {
	 UNLOCKMISC(l);
	 return;
    }
    } /* !_aioinfo */
    UNLOCKMISC(l);

    /* crank up slaves */
    {
	caddr_t *stks;
	size_t	stksize = 32 * 1024;
	int fd;

	if ((fd = open("/dev/zero", O_RDWR)) < 0) {
	    perror("aio_sgi_init failed to open /dev/zero");
	    exit(-1);
	}
	stks = calloc(numthreads, sizeof(void *));
	for (i = 0; i < numthreads; i++) {
		if ((stks[i] = mmap(0, stksize, PROT_READ|PROT_WRITE, 
				    MAP_PRIVATE, fd, 0)) == (caddr_t)-1L) {
			perror("aio_sgi_init mmap failed for stack ");
			exit(-1);
		}
	}
	close(fd);

	/*
	 * Thread 0 is the control thread, all others only do I/O.
	 * Allowing all threads to do both control and I/O
	 * would be better, but can cause deadlocks if all
	 * the threads are waiting on control sema's and
	 * no threads are doing IO.
	 */

	sigprocmask(SIG_SETMASK, &_aioinfo->sigblockset, &oldsigset);
	if (pthreads) {
		pthread_t	pt;
		pthread_attr_t	pta;

		pthread_attr_init(&pta);
		pthread_attr_setdetachstate(&pta, PTHREAD_CREATE_DETACHED);
		pthread_attr_setstacksize(&pta, stksize);

		for (i = 0; i < numthreads ; i++) {

			pthread_attr_setstackaddr(&pta, stks[i]);

			if (pthread_create(&pt, &pta,
					   i ? (void *(*)(void*))slave
					     : (void *(*)(void*))ctlslave, 0)) {
				perror("aio_sgi_init(pthread_create)");
				exit(-1);
			}
		}
		pthread_attr_destroy(&pta);
	} else {
		pid_t *spids = malloc(sizeof(pid_t) * numthreads);

		for (i = 0; i < numthreads ; i++) {
			if ((spids[i] = sprocsp(i ? slave : ctlslave,
						AIO_SPROC, 0,
						stks[i] + stksize,
						stksize)) < 0) {

				perror("aio_sgi_init(sprocsp)");
				while (--i >= 0){
					if (spids[i] > 0)
						kill(spids[i], SIGKILL);
					}
				exit(-1);
			}
		}
		free(spids);
	}
	free(stks);
	sigprocmask(SIG_SETMASK, &oldsigset, NULL);
    }

    _aio_grab_alock(&oldsigset);
    for (i = 0; i < numlocks; i++){
	aiowaitsema_t	*as1;
	aiocb_t *aiocb;
	
	as1 = _aio_new_waitsema(SEMA_NONE);
	aiocb = _aio_new_lioaiocb();
	if (!as1 || !aiocb){
		/* This really should never happen */
	    perror("aio_sgi_init(allocate locks)");
	    break;
	}
	_aio_free_waitsema(as1);
	_aio_free_lioaiocb(aiocb);
    }
    _aioinfo->all_init_done = 1;
    if (KAIO_IS_ACTIVE)
	 libdba_kaio_user_active(1);
    _aio_release_alock(&oldsigset);
}

static
void
_slavedeath()
{
	exit(0);
}

/* ARGSUSED1 */
static void
slave(void *arg, size_t slen)
{
    register aiocb_t *a;
    register struct aiolink *al;
    ssize_t rv;
    int flag;
    pid_t rpid;
    sigevent_t *event;
    sigevent_t saved_event;
    aiowaitcan_t *awc;
        
    if (_mplib_get_thread_type() != MT_PTHREAD) {
	sigaction_t sa;

	_aio_reset_signals();
	(void) sigaction(SIGHUP, NULL, &sa);
	sa.sa_handler = _slavedeath;
	sigaction(SIGHUP, &sa, NULL);
	prctl(PR_TERMCHILD);

	/*
	 * Turned on user-level sigprocmask.
	 */
	__sgi_prda_procmask(USER_LEVEL);

	if (getppid() == 1)
		exit(0);

	usadd(_aioinfo->ahandle);
    } else {
	extern int __libc_threadbind(void);
	(void)__libc_threadbind();
    }

    for (;;) {
	/*
	 * pull first untaken request off
	 * Always scan list since we may have skipped over a
	 * request becuase of other outstanding requests for the
	 * same fd
	 */
	    _aio_grab_alock_nosig();
      again:
	    /*
	     * Thread 0 handles control operations, and IO operations
	     * the other threads only handle real IO
	     */
	    /*
	     * If we did not find a control message or we are
	     * not thread 0 then we need to look for real work
	     */
	    for (al = _aioinfo->ahead.al_forw;
		 al != &_aioinfo->ahead;
		 al = al->al_forw) {
		    /*
		     * no one servicing
		     */
		    if (al->al_busy == 0) {
			    if (!((_aioinfo->activefds[al->al_req->aio_fildes] != NULL)
				  && (al->al_req->aio_op == LIO_WRITE))) {
				    break;
			    }
		    }
	    }
	    if (al == &_aioinfo->ahead) {
		    int rv = 0;
		    /* nothing there?? */
		    _aioinfo->numwsema++;
		    _aio_release_alock_nosig();
		    rv = uspsema(_aioinfo->wsema);
		    if (rv == -1)
			    perror("problem with uspsema in libabi(slave)");
		    _aio_grab_alock_nosig();
		    _aioinfo->numwsema--;
		    goto again;
	    }

	    /* mark as inprogress */
	    al->al_busy = 1;	/* use cached pid */
	    rpid = al->al_rpid;
	    a = al->al_req;
	    event = &(a->aio_sigevent);
	    if (event) {
		    bcopy(event, &saved_event, sizeof(saved_event));
		    event = &saved_event;
	    }
	    
	    switch(a->aio_op) {
		case LIO_READ:
		    _aio_release_alock_nosig();
		    rv = pread64(a->aio_fildes, 
			       (char *)a->aio_buf, 
			       a->aio_nbytes,
			       AIO_OFFSET(a));

		    if (rv == -1 && errno == ESPIPE)
			    rv = read(a->aio_fildes, 
				      (char *)a->aio_buf, 
				      a->aio_nbytes);
			    
		    break;
		case LIO_WRITE:
		    if ((flag = fcntl(a->aio_fildes, F_GETFL)) < 0) {
			    rv = -1;
			    _aio_release_alock_nosig();
			    break;
		    }
		    if (flag & O_APPEND) {
			    _aioinfo->activefds[a->aio_fildes] = al;
		    }
		    _aio_release_alock_nosig();
		    rv = pwrite64(a->aio_fildes, 
				(char *)a->aio_buf, 
				a->aio_nbytes, 
				AIO_OFFSET(a));

		    if (rv == -1 && errno == ESPIPE)
			    rv = write(a->aio_fildes, 
				       (char *)a->aio_buf, 
				       a->aio_nbytes);

		    /* as this thread is the only one that can be changing this
		     * we do not need to grab the lock */
		    if (flag & O_APPEND) {
			    _aioinfo->activefds[a->aio_fildes] = NULL;
		    }

		    break;
		default:
		    _aio_release_alock_nosig();
		    printf("*** BAD OP (%lu)for slave***\n",a->aio_op);
#ifdef DEBUG
		    abort();
#endif
		    break;
	    }
	    /* transaction done - pull off queue */
	    _aio_grab_alock_nosig();
	    /*
	     * Grab the pointer to the wait help
	     * structure so that we can then go ahead and mark the
	     * operation as finished and not worry about reuse before
	     * we do the actual wakeups
	     */
	    awc = (aiowaitcan_t *)(a->aio_waithelp);
	    a->aio_waithelp = NULL;

	    _aiofreelink(al);

	    /*
	     * We need to change the errno after we free the link
	     * otherwise the user might think the operation is done
	     * and reuse the aiocb before we really are done with it
	     * it also needs to be done under the alock to avoid a race 
	     * with the aio_suspend code. The following needs to be
	     * volatile to for the writes in the correct order. This
	     * is because user programs look on aio_errno, and count
	     * on aio_nobytes already being set.
	     */

	    if (rv < 0) {
		    *(volatile unsigned long *)(&a->aio_nobytes) = -1;
		    *(volatile unsigned long *)(&a->aio_errno) = oserror();
	    } else {
		    *(volatile unsigned long *)(&a->aio_nobytes) = rv;
		    *(volatile unsigned long *)(&a->aio_errno) = 0;
	    }
	    _aioworkcans(awc);
	    _aio_release_alock_nosig();
	    if (event)
		    _aio_handle_notify(event, rpid);

    }
}

/* ARGSUSED1 */
static void
ctlslave(void *arg, size_t slen)
{
    register aiocb_t *a;
    register struct aiolink *al;
    ssize_t rv;
    pid_t rpid;
    sigevent_t *event;
    sigevent_t saved_event;
    int freeaio;
    aiowaitcan_t *awc;

    if (_mplib_get_thread_type() != MT_PTHREAD) {
	sigaction_t sa;

	_aio_reset_signals();
	sigaction(SIGHUP, NULL, &sa);
	sa.sa_handler = _slavedeath;
	sigaction(SIGHUP, &sa, NULL);

	
	prctl(PR_TERMCHILD);

	/*
	* Turned on user-level sigprocmask.
	*/
	__sgi_prda_procmask(USER_LEVEL);

	if (getppid() == 1)
		exit(0);
	usadd(_aioinfo->ahandle);
    } else {
	extern int __libc_threadbind(void);
	(void)__libc_threadbind();
    }
    for (;;) {
      	    /*
	     * pull first untaken request off
	     * Always scan list since we may have skipped over a
	     * request becuase of other outstanding requests for the
	     * same fd
	     */
	    _aio_grab_alock_nosig();
      ctlagain:
	    /*
	     * Thread 0 handles control operations, and IO operations
	     * the other threads only handle real IO
	     */
	    for (al = _aioinfo->achead.al_forw;
		 al != &_aioinfo->achead;
		 al = al->al_forw) {
		    
		    /*
		     * no one servicing
		     */
		    if (al->al_busy == 0) {
			    break;
		    } 
	    }

	    if (al == &_aioinfo->achead) {
		    int rv = 0;
		    /* nothing there?? */
		    /* wait for request */
		    /* If there is something on the list go again */
		    if(&_aioinfo->achead != _aioinfo->achead.al_forw ){
			    goto ctlagain;
		    }
		    _aioinfo->numwcsema++;
		    _aio_release_alock_nosig();

		    rv = uspsema(_aioinfo->wcsema);
		    if (rv == -1)
			    perror("problem with uspsema in libabi(ctlslave)");
		    _aio_grab_alock_nosig();
		    _aioinfo->numwcsema--;
		    goto ctlagain;
	    }

	    /* mark as inprogress */
	    al->al_busy = 1;	/* use cached pid */
	    rpid = al->al_rpid;
	    a = al->al_req;
	    event = &(a->aio_sigevent);
	    if (event) {
		    bcopy(event, &saved_event, sizeof(saved_event));
		    event = &saved_event;
	    }
	    
	    freeaio = FALSE;
	    switch(a->aio_op) {
		case LIO_NOWAIT:
		    freeaio = TRUE;
		    _aio_release_alock_nosig();
		    rv = _aio_wait_help(a);
		    if (event) {
			    /* 
			     * The following check is so that we
			     * can keep common code to handle
			     * the notify, and also deal with the
			     * fact that I think POSIX made a
			     * mistake in that it does not require
			     * the notify field to be set for
			     * lio_listio calls. We need this 
			     * check to keep listio calls working
			     * even when POSIX is wrong
			     */
			    if((event->sigev_notify != SIGEV_NONE) &&
			       (event->sigev_notify != SIGEV_SIGNAL) &&
			       (event->sigev_notify != SIGEV_CALLBACK) &&
			       (event->sigev_notify != SIGEV_THREAD) &&
			       (event->sigev_signo > 0))
				    event->sigev_notify = SIGEV_SIGNAL;
			    
		    }
		    break;
		case LIO_DSYNC:
		case LIO_SYNC:
		    _aio_release_alock_nosig();
		    /* Wait for the fsync to happen */
		    rv = _aio_wait_help((aiocb_t *)a);
		    break;
		default:
		    _aio_release_alock_nosig();
		    printf("*** BAD OP (%lu)for ctlslave***\n",a->aio_op);
		    break;
	    }
	    /* transaction done - pull off queue */
	    _aio_grab_alock_nosig();
	    /*
	     * Grab the pointer to the wait help
	     * structure so that we can then go ahead and mark the
	     * operation as finished and not worry about reuse before
	     * we do the actual wakeups
	     */
	    awc = (aiowaitcan_t *)(a->aio_waithelp);
	    a->aio_waithelp = NULL;
	    _aiofreelink(al);
	    
	    /*
	     * We need to change the errno after we free the link
	     * otherwise the user might think the operation is done
	     * and reuse the aiocb before we really are done with it
	     * it also needs to be done under the alock to avoid a race 
	     * with the aio_suspend code.
	     */
	    
	    if (rv < 0) {
		    *(volatile unsigned long *)(&a->aio_nobytes) = -1;
		    *(volatile unsigned long *)(&a->aio_errno) = oserror();
	    } else {
		    *(volatile unsigned long *)(&a->aio_nobytes) = rv;
		    *(volatile unsigned long *)(&a->aio_errno) = 0;
	    }
	    _aioworkcans(awc);
	    _aio_release_alock_nosig();
	    if (event)
		    _aio_handle_notify(event, rpid);

	    /*
	     * this free is needed as lio_listio needs to create an aio
	     * to send to _aqueue. 
	     */
	    if(freeaio){
		    _aio_grab_alock_nosig();
		    _aio_free_lioaiocb(a);
		    _aio_release_alock_nosig();
	    }
    }
}

int
_aqueue(struct aiocb *aio, int op, aiowaitsema_t *as)
{
    register struct aiolink *al, *tmpal;
    int fd = aio->aio_fildes;
    aiowaitcan_t *awc;
    sigset_t oldsigset;
#if 0  /* !defined( __AIO_52_COMPAT__) */
    int  notify;
#endif /*  __AIO_52_COMPAT__ */
    if (!_aioinfo)
	aio_sgi_init(NULL);
    /*
     * As we do not currently support prioritized I/O this will 
     * return an error for any value that is not 0
     */
    
    if (aio->aio_reqprio < 0 || 
	aio->aio_reqprio > _AIO_SGI_PRIO_DELTA_MAX) {
	if(_aioinfo->debug)
	    printf("AIO ERROR: Need to set aio_reqprio to 0 for aiocb 0x%p\n",aio);
	aio->aio_errno = EINVAL;
	aio->aio_nobytes = -1;
	setoserror(EINVAL);
	return(-1);
    }
#if 0
#ifndef  __AIO_52_COMPAT__
/* This code is not reached when  __AIO_52_COMPAT__ is on. */
    notify = aio->aio_sigevent.sigev_notify;
    if ((notify != SIGEV_NONE) && 
	(notify != SIGEV_SIGNAL) &&
	(notify != SIGEV_THREAD) &&
	(notify != SIGEV_CALLBACK)) {
	aio->aio_errno = EINVAL;
	setoserror(EINVAL);
	return(-1);
    }
#endif /* __AIO_52_COMPAT__ */
#endif
    /* Kernel async I/O */
    /* Check KAIO before the fcntl fd check, because kaio_rw in the kernel
     * will already do it */
    if (KAIO_IS_ACTIVE) {
	 struct kaio_ret kaio_ret;
	 libdba_kaio_submit(aio, op, &kaio_ret);
	 if ((kaio_ret.back2user) == 1) {
	      return(kaio_ret.rv);
	 }
    }
    /* Check for a valid file descripter */
    if (op == LIO_READ || op == LIO_WRITE) {
	    if ((fcntl(fd, F_GETFL) < 0) && errno == EBADF) {
		    aio->aio_nobytes = -1;
		    aio->aio_errno = EBADF;
		    /*setoserror(EBADF);*/
		    return(-1);
	    }
	    if (AIO_OFFSET(aio) < 0){
		    aio->aio_errno = EINVAL;
		    aio->aio_nobytes = -1;
		    setoserror(EINVAL);
		    return(-1);
	    }
    }

    _aio_grab_alock(&oldsigset);

    /* pull one off free list */
    if (_aioinfo->aiofree == NULL) {
	_aio_release_alock(&oldsigset);
	aio->aio_nobytes = -1;
	aio->aio_errno = EAGAIN;
	setoserror(EAGAIN);
	return(-1);
    }
    al = _aioinfo->aiofree;
    _aioinfo->aiofree = al->al_forw;
    _aioinfo->alistfreecount--;
    _aio_release_alock(&oldsigset); 
    /* fill in info */
    al->al_req = aio;
    al->al_rpid = get_pid();	/* use cached pid, for speed */
    al->al_req->aio_op = op;
    al->al_busy = 0;		/* noone handling yet */

    /* init return values */
    aio->aio_nobytes = 0;
    aio->aio_errno = EINPROGRESS;
    aio->aio_ret = 0;
    if (as) {
	    _aio_grab_alock(&oldsigset);
	    awc = _aio_get_waitcan(1);
	    awc->next = (aiowaitcan_t *)(aio->aio_waithelp);
	    awc->aws = as;
	    
	    /*
	     * fill in an initial value for waithelp
	     */
	    aio->aio_waithelp = (unsigned long)awc;
	    _aio_release_alock(&oldsigset);

    } else {
	    aio->aio_waithelp = NULL;
    }
    
    /* 
     * Queue the operation at the end of the list.
     * For priority IO we might need to change this.
     */
    if (op == LIO_READ || op == LIO_WRITE) {
	    _aio_grab_alock(&oldsigset); 
	    tmpal = _aioinfo->ahead.al_back;
	    al->al_back = tmpal;
	    al->al_forw = tmpal->al_forw;
	    tmpal->al_forw->al_back = al;
	    tmpal->al_forw = al;
	    _aio_release_alock(&oldsigset);
	    /* kick a slave */
	    if (_aioinfo->numwsema)
		    usvsema(_aioinfo->wsema);
    } else {
	    _aio_grab_alock(&oldsigset); 
	    tmpal = _aioinfo->achead.al_back;
	    al->al_back = tmpal;
	    al->al_forw = tmpal->al_forw;
	    tmpal->al_forw->al_back = al;
	    tmpal->al_forw = al;
	    _aio_release_alock(&oldsigset);
	    /* kick a slave */
	    if (_aioinfo->numwcsema)
		    usvsema(_aioinfo->wcsema);
    }
 
    return(0);
}

#ifdef DEBUG
static void
dumpit()
{
    FILE *fd;

    fd = fopen("aiodump", "w");
    usdumphist(_aioinfo->wsema, fd);
    usdumphist(_aioinfo->wcsema, fd);
    fflush(fd);
}
#endif

static void *
aio_notify_thread(void *arg) 
{       
	sigevent_t	*sev = (sigevent_t *)arg;
	void		(*user_func)(union sigval) = sev->sigev_notify_function;
	union sigval	user_arg = sev->sigev_value;

	free(arg);
	user_func(user_arg);
	return (0);
}

void
_aio_handle_notify(sigevent_t *event, int pid)
{
    switch(event->sigev_notify) {
    case SIGEV_CALLBACK:
	_aio_call_callback(event);
	break;
    case SIGEV_SIGNAL:
	if (event->sigev_signo) {
#ifdef IRIX53
	    if (sigqueue(pid, 
			 event->sigev_signo,
			 event->sigev_value) < 0) {
#else
	    if (ksigqueue(pid, 
			 event->sigev_signo,
			 SI_ASYNCIO,
			 event->sigev_value.sival_ptr) < 0) {
#endif
		perror("aio(sigqueue)");
		break;
	    }
	}
	break;
    case SIGEV_THREAD:
	if (_mplib_get_thread_type() != MT_PTHREAD)
	    break;
	{
		pthread_t	pt;
		pthread_attr_t	pta;
		pthread_attr_t	*ptap = event->sigev_notify_attributes;
		sigevent_t	*sev = malloc(sizeof(sigevent_t));

		if (!sev) {
			break;
		}
		*sev = *event;
		if (!ptap) {
			ptap = &pta;
			pthread_attr_init(ptap);
		}
		pthread_attr_setdetachstate(ptap, PTHREAD_CREATE_DETACHED);

		if (pthread_create(&pt, ptap, aio_notify_thread, sev)) {
			free(sev);
		}

		if (ptap == &pta) {
			pthread_attr_destroy(ptap);
		}
	}
	break;
    case SIGEV_NONE:
    default: 
	break;
    }
}

/*
 * _aioworkcans decrements the waithelp count if one is available and wakes up
 * waiters when the count is right.
 */
void
_aioworkcans(aiowaitcan_t *awc)
{
	aiowaitcan_t *awc1, *awc2;
	assert(ustestlock(_aioinfo->alock));
	awc1 = awc;

	while (awc1) {
		aiowaitsema_t *as;
	    
		as = awc1->aws;
		/*
		 * take us out of the count
		 */
	    
		as->count--;
#if 0
		printf("_aiofreelink: count is %d targ is %d free is %d for 0x%x\n",as->count,as->targcount, as->freecount, as);
#endif 
		/* 
		 * Wake up the thread if the count is right
		 */
		if (as->count == as->targcount) {
			if (as->type == SEMA_POLL)
				usvsema(as->sema);
			else
				sem_post(as->psema);
		}
		if (as->count == as->freecount) {
			_aio_free_waitsema(as);
		}
		/* 
		 * the container has done its work, advance to the
		 * next one and free it
		 */
		awc2 = awc1;
		awc1 = awc1->next;
		_aio_free_waitcan(awc2);
	}
}

/*
 * This function is used to return link structures to the free list.
 * It needs to be called with the _aioinfo->alock held. 
 */
void
_aiofreelink(struct aiolink *al)
{
	/*
	 * READ/WRITE might have a wait counter that needs to be 
	 * handled for aio_fsync and lio_listio
	 */
	assert(ustestlock(_aioinfo->alock));
	/* With that out of the way, free the link structure */
	
	al->al_back->al_forw = al->al_forw;
	al->al_forw->al_back = al->al_back;
	al->al_back = NULL; /* debugging */
	al->al_req = NULL;
	al->al_forw = _aioinfo->aiofree;
	_aioinfo->aiofree = al;
	_aioinfo->alistfreecount++;
}
/*
 * returns TRUE if we have more then "cnt" items in the free list
 * this is called by lio_listio to make sure that it can proceed
 */
int
_aiochkcnt(int cnt)
{
    return ((_aioinfo->alistfreecount - cnt) >= 0);
}

/*
 * There is a deadlock if a proc is holding the _aioinfo->alock
 * and a signal handler interupts it and tries itself to do an 
 * an aio_* call that also needs the _aioinfo->alock. The only
 * way to prevent the deadlock is to block all signals while we
 * are holding the lock.
 */

void
_aio_grab_alock(sigset_t *oldsigset)
{
	sigprocmask(SIG_BLOCK, &_aioinfo->sigblockset, oldsigset);
	while (sem_wait(&_aioinfo->alock) < 0) {
		if (oserror() != EINTR)
			perror("aio(_aio_grab_alock)");
	}
#ifdef DEBUG
	_lastowner = _mplib_get_thread_id();
	_lastaddr = (char *)__return_address;
#endif
}
static
void
_aio_grab_alock_nosig()
{
	while (sem_wait(&_aioinfo->alock) < 0) {
		if (oserror() != EINTR)
			perror("aio(_aio_grab_alock)");
	}
#ifdef DEBUG
	_lastowner = _mplib_get_thread_id();
	_lastaddr = (char *)__return_address;
#endif
}
void
_aio_release_alock(sigset_t *oldsigset)
{
#ifdef DEBUG
    	_lastpowner = _mplib_get_thread_id();
	_lastpaddr = (char *)__return_address;
#endif
	sem_post(&_aioinfo->alock);
	sigprocmask(SIG_SETMASK, oldsigset, NULL);
}
static
void
_aio_release_alock_nosig()
{
#ifdef DEBUG
    	_lastpowner = _mplib_get_thread_id();
	_lastpaddr = (char *)__return_address;
#endif
	sem_post(&_aioinfo->alock);
}
/*
 * This routine is called by slave threads to handle async events
 * other then read and write. It relies on having a semaphore stashed
 * away in the aiocb structure.
 */
int
_aio_wait_help(aiocb_t *aio)
{
    aiowaitsema_t *as;
    int rv = 0;
    aiowaitcan_t *awc;
	
    awc = (aiowaitcan_t *)(aio->aio_waithelp);
    /*
     * It is possible to have multiple wait containers here if
     * we are here for a sync call and someone is waiting in
     * aio_suspend for us. As we always queue at the front,
     * the last one is the one we want to wait on here.
     */
    while(awc->next)
	    awc = awc->next;
    as = awc->aws;
    assert(as->sema);
    /*
     * Sleep until all the operations that are in progress are done
     * This can be null if lio_listio has finished all operations
     * before the call here.
     */
    if (as->type == SEMA_POLL)
	    rv = uspsema(as->sema);
    else {
	    rv = sem_wait(as->psema);
    }
    /*
     * We are now done with the sema, now we can do the fsync
     */
    switch(aio->aio_op) {
	case LIO_DSYNC:
	    rv = fdatasync(aio->aio_fildes);
	    break;
	case LIO_SYNC:
	    rv = fsync(aio->aio_fildes);
	    break;
	case LIO_WAIT:
	case LIO_NOWAIT:
	    break;
	default:
	    printf("*****_aio_wait_help: unknown op******\n");
    }
    return (rv);
}
/*
 * The following routines are used to alocate and cache structures
 * used by the _aio_wait_help routine. We need to allocate one
 * aiowaitcan_t per aiowaitsema_t per aiobc_t. The aio_waithelp field
 * in the reserved section of the aiocb_t is the head of the list of
 * aiowaitcan_t's
 */
aiowaitsema_t *
_aio_new_waitsema(int type)
{
    aiowaitsema_t *as;
    as = (aiowaitsema_t *)usmalloc(sizeof(struct aiowaitsema), _aioinfo->ahandle );
    if (!as) {
	setoserror(EAGAIN);
	return(NULL);
    }
    switch (type) {
	case SEMA_STD:
	    as->psema = usmalloc(sizeof(sem_t), _aioinfo->ahandle);
	    sem_init(as->psema, 0, 0);
	    sem_mode(as->psema, SEM_MODE_NOCNCL);
	    break;
	case SEMA_POLL:
	    as->sema = usnewpollsema(_aioinfo->ahandle, 0);
	    break;
	case SEMA_NONE:
	    as->sema = (void *) -1L;
	    break;
	default:
	     printf("*** BAD TYPE (%d)for _aio_new_waitsema***\n",type);
    }
    as->type = type;
    if(!as->sema)
	perror("aio(_aio_new_waitsema)");
    assert(as->sema);
    return (as);
}

aiowaitsema_t *
_aio_get_waitsema(int type)
{
    aiowaitsema_t *as;
    assert(ustestlock(_aioinfo->alock));
    /*
     * First check and see if we have one available on the 
     * free list. If so use it, otherwise make a new one.
     */
    assert(type > 0);
    if (as = _aioinfo ->waitfree) {
	_aioinfo->waitfree = as->next;
	switch(type) {
	    case  SEMA_STD:
		as->psema = usmalloc(sizeof(sem_t), _aioinfo->ahandle);
		sem_init(as->psema, 0, 0);    
	        sem_mode(as->psema, SEM_MODE_NOCNCL);
		break;
	    case SEMA_POLL:
		as->sema = usnewpollsema(_aioinfo->ahandle, 0);
		break;
	    default:
		printf("*** BAD TYPE (%d)for _aio_get_waitsema***\n",type);
	}
    }
    if(!as)
	as = _aio_new_waitsema(type);
    as->type = type;
    if (!as->sema)
	perror("aio(_aio_get_waitsema)");
    return (as);
}

void
_aio_free_waitsema(aiowaitsema_t *as)
{
    assert(ustestlock(_aioinfo->alock));
    switch(as->type) {
    case SEMA_STD:
	    sem_destroy(as->psema);
	    usfree(as->psema,_aioinfo->ahandle);
	    break;
    case SEMA_POLL:
    case SEMA_NONE:
            break;
    default:
            printf("*** BAD TYPE (%d)for _aio_free_waitsema***\n",as->type);
    }
    
    as->next = _aioinfo->waitfree;
    _aioinfo->waitfree = as;
    
 
}

aiowaitcan_t *
_aio_new_waitcan()
{
    aiowaitcan_t *wc;

    assert(ustestlock(_aioinfo->alock));
    wc = (aiowaitcan_t *)usmalloc(sizeof(struct aiowaitcan), _aioinfo->ahandle);
    if (!wc) {
	setoserror(EAGAIN);
    }
    return (wc);
}

aiowaitcan_t *
_aio_get_waitcan(int id)
{
	aiowaitcan_t *wc;

    
	assert(ustestlock(_aioinfo->alock));
	/*
	 * First check and see if we have one available on the 
	 * free list. If so use it, otherwise make a new one.
	 */
	if (wc = _aioinfo->waitcanfree){
		_aioinfo->waitcanfree = wc->next;
	}else {
		wc = _aio_new_waitcan();
	}
	wc->id = id;
	wc->next = NULL;
	return(wc);
}

void
_aio_free_waitcan(aiowaitcan_t *wc)
{
    assert(ustestlock(_aioinfo->alock));
    /*    printf("free id is %d\n",wc->id); */
    wc->next = _aioinfo->waitcanfree;
    _aioinfo->waitcanfree = wc;
}
static
void
_aio_reset_signals()
{
	int i;
	for (i = 1; i<= MAXSIG; i++) {
		sigset(i,SIG_DFL );
	}
	sigrelse(SIGHUP);

}
