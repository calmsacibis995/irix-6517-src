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
#ident  "$Revision: 1.25 $ $Author: kostadis $"

#include <sgidefs.h>	/* for _MIPS_SIM defs */
#ifdef __STDC__
	#pragma weak lio_listio = _lio_listio
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	#pragma weak lio_listio64 = _lio_listio
	#pragma weak _lio_listio64 = _lio_listio
#endif
#endif
#include "synonyms.h"
#include <aio.h>
#include "local.h"
#include <limits.h>

static 
int
lio_check_errors(aiocb_t * const list[], int cnt)
{
	int i;
	register aiocb_t *a;
	int rv = 0;
	
	for (i = 0; i < cnt; i++) {
			if (!(a = list[i]))
				continue;
			if (a->aio_lio_opcode == LIO_NOP)
				continue;
			if (aio_error(a)) {
				rv = EIO;
				break;
			}
		}
	return(rv);
}
/*
 * Posix 1003.1b async I/O lio_listio
 */
int
lio_listio(int mode, aiocb_t * const list[], int cnt, sigevent_t *sig)
{
    register int i;
    register aiocb_t *a, *tmpaio;
    aiowaitsema_t *as = NULL;
    aiowaitcan_t *wc;
    int rv;
    sigset_t oldsigset;
    int error = 0;
    aiocb_t **non_kaio_list;
    int non_kaio_cnt = 0;
#ifdef __AIO_52_COMPAT__
    extern int old_lio_listio(int, aiocb_t * const[], int, sigevent_t *);
    /* 
     * Here we hard code in the values of LIO_WAIT and LIO_NOWAIT
     * from the 5.2 version of the aio library. In order to make binary
     * compatibility work for this new version we have chosen new values
     * for these flags. We also need to fill in valid values for the
     * notify field to make sure that aiocb look new to other
     * functions.
     */
    if (mode == 1 || mode == 2) 
	old_lio_listio(mode, list, cnt, sig);
#endif /*  __AIO_52_COMPAT__ */
    if (cnt > _AIO_SGI_LISTIO_MAX || 
	(mode != LIO_WAIT && mode != LIO_NOWAIT)) {
	setoserror(EINVAL);
	return(-1);
    }
    if (!_aioinfo)
	aio_sgi_init(NULL);

    /* If enabled, kaio gets first crack at everything in the list. If
     * everything is eligible for kaio, no need to bother libc aio with it.
     */
    if (KAIO_IS_ACTIVE) {
	 struct kaio_ret kaio_ret;
	 libdba_kaio_lio(mode, list, cnt, sig, &non_kaio_list, &non_kaio_cnt, &kaio_ret);
	 if ((kaio_ret.back2user) == 1) {
	      return(kaio_ret.rv);
	 }
	 /* Give libc a new list, with just the unsubmitted I/O requests */
	 list = non_kaio_list;
	 cnt = non_kaio_cnt;
    }
    
    /* 
     * If we can not queue up the entire list and the notify then
     * return EAGAIN. There is a little race here as we do not
     * reserve the items.
     */
    if (!_aiochkcnt(cnt + ((mode == LIO_NOWAIT) ? 1 : 0 ))) {
	setoserror(EAGAIN);
	if (non_kaio_cnt)
	     libdba_kaio_lio_free(non_kaio_list);
	return(-1);
    }
    
/* queue requests */
    for (i = 0; i < cnt; i++) {
	if (!(a = list[i]))
	    continue;
	if (a->aio_lio_opcode == LIO_NOP)
	    continue;
	if (a->aio_lio_opcode != LIO_READ && 
	    a->aio_lio_opcode != LIO_WRITE) {  
	    setoserror(EINVAL);
	    if (non_kaio_cnt)
		 libdba_kaio_lio_free(non_kaio_list);
	    return(-1);
	}
	
	if (!as) {
	    _aio_grab_alock(&oldsigset); 
	    as =  _aio_get_waitsema(SEMA_STD);
	    _aio_release_alock(&oldsigset);
	    if (!as) {
		setoserror(EAGAIN);
		if (non_kaio_cnt)
		     libdba_kaio_lio_free(non_kaio_list);
		return(-1);
	    }
	    as->count = 0;
	    /* 
	     * So that we do not do the wakeup before we all the items 
	     * are queued we set the targcount to an impossible value
	     */
	    as->targcount = -100;
	    as->freecount = -1;
	}
	
	/*
	 * Need to grab the lock to make sure that it
	 * is not being decremented at the same time that 
	 * we increment it.
	 */
	_aio_grab_alock(&oldsigset); 
	as->count++;
	a->aio_waithelp = NULL; 
	_aio_release_alock(&oldsigset);
	/* Do not wait if the function will not return */
	if(_aqueue(a, a->aio_lio_opcode, as)) {
		_aio_grab_alock(&oldsigset); 
		as->count--;
		_aio_release_alock(&oldsigset);
	}
    }
    if (!as) {
	/* We did not queue up anything, we are done */
	 if (non_kaio_cnt)
	      libdba_kaio_lio_free(non_kaio_list);
	return(0);
    }
    _aio_grab_alock(&oldsigset); 
    /* 
     * All of the operations might already be done
     * if that is the case we cleanup and exit
     */
    as->targcount = 0;

    if (as->count == 0) {
	rv = 0;
	    /*	printf("early exit from lio_listio\n"); */
	_aio_free_waitsema(as);
	as = NULL;
	_aio_release_alock(&oldsigset);
	if (mode == LIO_NOWAIT) {
		/*
		 * Send a signal if an ASYNC call
		 */
		if(sig){
			if ((sig->sigev_notify != SIGEV_NONE) &&
			    (sig->sigev_notify != SIGEV_SIGNAL) &&
			    (sig->sigev_notify != SIGEV_CALLBACK) &&
			    (sig->sigev_notify != SIGEV_THREAD) &&
			    (sig->sigev_signo > 0)) 
				sig->sigev_notify = SIGEV_SIGNAL;
			_aio_handle_notify(sig, get_pid());
		}
	} else {
		if ((error = lio_check_errors(list, cnt)) != 0)
			rv = -1;
	}
	if (error)
		setoserror(error);
	if (non_kaio_cnt)
	     libdba_kaio_lio_free(non_kaio_list);
	return(rv);
 
    }
    wc = _aio_get_waitcan(3); /* Do this before we drop lock */
    tmpaio = _aio_get_lioaiocb();
    if (sig)
	bcopy(sig, &(tmpaio->aio_sigevent), sizeof(struct sigevent));
    tmpaio->aio_op = mode;
    wc->aws = as;
    wc->next = NULL;
    tmpaio->aio_waithelp = (unsigned long)wc;
    _aio_release_alock(&oldsigset);
    if (mode == LIO_WAIT) {
	/* wait for all to be done, return on 1st failure  */
	rv = _aio_wait_help(tmpaio);
	error = errno;
	/*
	 * If this was a successful LIO_WAIT then we need to check
	 * the results for all good aio operations.
	 */
	if (rv == 0) {
		if ((error = lio_check_errors(list, cnt)) != 0)
			rv = -1;
	}
	_aio_grab_alock(&oldsigset);
	_aio_free_waitcan(wc);
	_aio_free_lioaiocb(tmpaio);
	_aio_free_waitsema(as);	
	_aio_release_alock(&oldsigset);
    } else {
/*	printf("lio_listio: about to call _aqueue with aiocb 0x%x\n",tmpaio); */
	rv = _aqueue(tmpaio, LIO_NOWAIT, as);
	if (rv)
	    perror("lio_listop");
	
    }
    if (error)
	    setoserror(error);
    if (non_kaio_cnt)
	 libdba_kaio_lio_free(non_kaio_list);
    return (rv);
}
aiocb_t *
_aio_new_lioaiocb(void)
{
    aiocb_t *aiocb;
    aiocb = (aiocb_t *)usmalloc(sizeof(struct aiocb), _aioinfo->ahandle );
    assert(aiocb);
    if (!aiocb) {
	setoserror(EAGAIN);
	return(NULL);
    }
    	/*
	 * The bzero is important as it puts the aiocb in a know
	 * state including setting aio_reqprio to 0.
	 */
    bzero(aiocb, sizeof(struct aiocb));

    return (aiocb);
}

aiocb_t *
_aio_get_lioaiocb(void)
{
    aiocb_t *aiocb;
    assert(ustestlock(_aioinfo->alock));
    /*
     * First check and see if we have one available on the 
     * free list. If so use it, otherwise make a new one.
     */
    if (aiocb = _aioinfo->lioaiocbfree){
	_aioinfo->lioaiocbfree = (aiocb_t *)aiocb->aio_waithelp;
	return (aiocb);
    }
    
    return(_aio_new_lioaiocb());
}

void
_aio_free_lioaiocb(aiocb_t *aiocb)
{
    assert(ustestlock(_aioinfo->alock));
    aiocb->aio_waithelp = (unsigned long)_aioinfo->lioaiocbfree;
    _aioinfo->lioaiocbfree = aiocb;
}
