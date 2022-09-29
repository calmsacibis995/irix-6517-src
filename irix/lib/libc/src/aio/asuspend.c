/*************************************************************************
*                                                                        *
*               Copyright (C) 1992, Silicon Graphics, Inc.       	 *
*                                                                        *
*  These coded instructions, statements, and computer programs  contain  *
*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
*  are protected by Federal copyright law.  They  may  not be disclosed  *
*  to  third  parties  or copied or duplicated in any form, in whole or  *
*  in part, without the prior written consent of Silicon Graphics, Inc.  *
*                                                                        *
**************************************************************************/
#ident  "$Revision: 1.27 $ $Author: jph $"

#include <sgidefs.h>	/* for _MIPS_SIM defs */
#ifdef __STDC__
	#pragma weak aio_suspend = _aio_suspend
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	#pragma weak aio_suspend64 = _aio_suspend
	#pragma weak _aio_suspend64 = _aio_suspend
#endif
#endif
#include "synonyms.h"
#include <aio.h>
#include <sys/time.h>
#include <memory.h>
#include "local.h"
#include <sys/types.h>
#include <sys/syssgi.h>
#include <sys/stat.h>
#include <mplib.h>
#include <pthread.h>

/* *** Warning :set shiftwidth=4 *** */

typedef struct cleanup_data {
    const aiocb_t		**list;
    aiowaitsema_t		*as;
    int				cnt;
} cleanup_data_t;

static void aio_suspend_cleanup(void *);


/*
 * Posix 1003.1b-1993 async I/O suspend
 */
int
aio_suspend(const aiocb_t * const list[], int cnt, const timespec_t *ts)
{
    register int i, rv;
    register aiocb_t *a;
    fd_set lfd;
    int ninprog;
    struct timeval tv;
    aiowaitsema_t *as = NULL;
    aiowaitcan_t *awc;
    int fd, kaiofd = 0;
    int select_errno;
    sigset_t oldsigset;
    cleanup_data_t cleaner;
    int pthreads = (_mplib_get_thread_type() == MT_PTHREAD);

#ifdef __AIO_52_COMPAT__
    /*
     * We check the value of the notify field in the first
     * non-null list[x] to see if it has a notify field that looks
     * like it really is a signal number from the previous
     * aio implementation. If so call the old version of the function.
     */

    extern int old_aio_suspend(const aiocb_t *const[], int, const timespec_t *);

    for (i = 0; i< cnt; i ++) {
	if (list[i])
	    if (list[i]->aio_sigevent.sigev_notify < 128)
		return (old_aio_suspend(list, cnt, ts));
	    else
		break;
    }
#endif /*  __AIO_52_COMPAT__ */

    /* Check for a pending cancellation before we do any work
     */
    if (pthreads)
	pthread_testcancel();
    if (ts) {
	tv.tv_sec = ts->tv_sec;
	tv.tv_usec = ts->tv_nsec/1000;
    }
    if (KAIO_IS_ACTIVE) {
	 struct kaio_ret kaio_ret;
	 libdba_kaio_suspend(list, cnt, ts?&tv:0, pthreads, &kaio_ret);
	/* If !USR_AIO_IS_ACTIVE, kaio_suspend() has already done everything
	 * and it's time to return to the user.
	 */
	 if (kaio_ret.back2user == 1)
	      return kaio_ret.rv;
	 /* Otherwise, kaio_ret.rv is the /dev/kaio file descriptor, if
	  * any of the passed-in aiocb's were in kaio-land. */
	 kaiofd = kaio_ret.rv;
    }
    _aio_grab_alock(&oldsigset);
    /*
     * Before we do anything, we need to look if we really
     * need to wait on anything. Often we are done before
     * we start. This is a little stupid that we do the loop
     * two times, but it is better then finding out at the end
     * that someone is done, and having to go unmark
     * the ones marked before.
     */
    for (i = 0, ninprog = 0; i < cnt; i++) {
	if ((a = (aiocb_t *)list[i]) == NULL)
	    continue;
	if (a->aio_errno != EINPROGRESS) {
	    _aio_release_alock(&oldsigset);
	    /*printf("aio_suspend: nothing to wait for(0)\n"); */
	    return(0);
	}
	ninprog++;
    }
    if (!ninprog && !kaiofd) {
	/*
	 * Nothing to wait for, we are done.
	 */
	/*printf("aio_suspend: nothing to wait for(1)\n"); */
	_aio_release_alock(&oldsigset);
	return(0);
    }
    /*
     * Now that we know there is at least one
     * we mark all the possible ones
     * and go to sleep
     */
    as = _aio_get_waitsema(SEMA_POLL);

    /* The following two tests return the wrong error code, but there
     * is no correct one and this really should not happen if the system
     * is configured right */

    if (!as){
	_aio_release_alock(&oldsigset);
	setoserror(EAGAIN);
	return(-1);
    }
    if ((fd = usopenpollsema(as->sema, 0600)) < 0) {
	_aio_free_waitsema(as);
	_aio_release_alock(&oldsigset);
	setoserror(EAGAIN);
	return(-1);
    }
    as->count = 0;

    for (i = 0; i < cnt; i++) {
	if ((a = (aiocb_t *)list[i]) == NULL)
		continue;
	/* Don't do normal libc stuff if this aiocb is in kaio realm */
	if (KAIO_IS_ACTIVE && a->aio_kaio)
		continue;

	as->count++;
	awc = _aio_get_waitcan(4);
	awc->next = (aiowaitcan_t *)(a->aio_waithelp);
	awc->aws = as;
	a->aio_waithelp = (unsigned long)awc;
    }
    as->targcount = as->count - 1; /* Wakeup after first is found */
    /* Make the free count such that the free happens in aio_suspend */
    as->freecount = -1;
    _aio_release_alock(&oldsigset);

    FD_ZERO(&lfd);
    if (ninprog)
	FD_SET(fd, &lfd);
    if (kaiofd)
	FD_SET(kaiofd, &lfd);

    /* no outstanding wait yet */
    rv = uspsema(as->sema);
    /* printf("aio_suspend: uspsema returns %d\n",rv); */
    /*
     * We do not need to go to select if one of the operations that we are
     * waiting on is already done
     *
     * Set up data so a cancellation request can clean up after itself
     * This consists of bundling data so that aio_suspend_cleanup()
     * can do the job and, for pthreads, wiring that function into the
     * cancellation cleanup list.
     */
    cleaner.cnt		= cnt;
    cleaner.list	= (const aiocb_t **)list;
    cleaner.as		= as;
    if (rv != 1) {
	int nfds = MAX(fd,kaiofd);

	/* Wait for the IO
	 */
	if (!pthreads) {

	    rv = select(nfds+1, &lfd, NULL, NULL, (ts) ? &tv : NULL);

	} else {
	    extern int __select(int, fd_set *, fd_set *, fd_set *,
	   			 struct timeval *);
	    struct __pthread_cncl_hdlr **list;

	    /* Reuse pthread macro by replacing the libpthread routine
	     * with an mt callback and thus avoiding a direct call.
	     */
#define __pthread_cancel_list() (_mtlib_ctl(MTCTL_CNCL_LIST, &list), list)
	    pthread_cleanup_push(aio_suspend_cleanup, &cleaner);
#undef __pthread_cancel_list

	    MTLIB_BLOCK_CNCL_VAL(
		    rv, __select(nfds+1, &lfd, NULL, NULL, (ts) ? &tv : NULL) );

	    pthread_cleanup_pop(&cleaner == 0);	/* always false */
	}
    }
    select_errno = errno;
    aio_suspend_cleanup(&cleaner);

    if (rv == 0 && ts) {
	/* case where timing out */
	setoserror(EAGAIN);
    }
    if ((rv == 1) || (rv == 2)) { /* usema and/or /dev/kaio */
	/* printf("*** FREE count 2 is %d\n",as->freecount); */
	return (0);
    }
    if (rv < 0) {
	setoserror(select_errno);
	if (oserror() != EINTR) {
	    perror("aio_suspend(select error)");
	    return(-1);
	}
    } else if (rv != 1 && !ts) {
	fprintf(stderr, "aio_suspend:select returned %d!\n", rv);
    }

    return (-1);
}

static void
aio_suspend_cleanup(void *arg)
{
    const aiocb_t	**list = ((cleanup_data_t *)arg)->list;
    aiowaitsema_t	*as =    ((cleanup_data_t *)arg)->as;
    int			cnt =     ((cleanup_data_t *)arg)->cnt;
    int i;
    aiocb_t *a;
    sigset_t oldsigset;

    /*
     * can be either signal or timeout expired or some completion
     */
    _aio_grab_alock(&oldsigset);

    /*
     * In the case when would be as->targcount == 0 we have the problem
     * that the free of as would happen before the above usfreepollsema
     * which would be an error. In that case we do the free ourself.
     */
    /* printf("*** FREE count is %d and rv is %d\n",as->freecount, rv); */

    /*
     * If there are still in progress operation that have this waitsema
     * we need to let the last operation do the free. If the last
     * slave is done then we need to do the free.
     */
    if (as->count > 0) {
	aiowaitcan_t *wc, *lwc;
	for (i = 0; i < cnt; i++) {
	    if ((a = (aiocb_t *)list[i]) == NULL)
		continue;

	    wc = (aiowaitcan_t *)(a->aio_waithelp);
	    if (!wc)
		continue;
	    if (wc->aws == as) {
	       a->aio_waithelp = (unsigned long)wc->next;
	       _aio_free_waitcan(wc);
	       continue;
	    }
	    lwc = wc;
	    wc = lwc->next;
	    while (wc) {
		if (wc->aws == as) {
		    lwc->next = wc->next;
		    _aio_free_waitcan(wc);
		    break;
		}
		lwc = wc;
		wc = lwc->next;
	    }
	}
    }
    /*
     * We have to dispose of a semaphore.
     * Lock needs to be dropped before we call usclosepollsema()
     */
    
    _aio_release_alock(&oldsigset);
    assert(as->sema);
    usclosepollsema(as->sema);
    usfreepollsema(as->sema, _aioinfo->ahandle);
    _aio_grab_alock(&oldsigset);
    /* printf("Calling free_waitsema\n"); */
    _aio_free_waitsema(as);
    _aio_release_alock(&oldsigset);

}
