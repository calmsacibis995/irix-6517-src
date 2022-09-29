/*************************************************************************
*                                                                        *
*               Copyright (C) 1992, 1994 Silicon Graphics, Inc.       	 *
*                                                                        *
*  These coded instructions, statements, and computer programs  contain  *
*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
*  are protected by Federal copyright law.  They  may  not be disclosed  *
*  to  third  parties  or copied or duplicated in any form, in whole or  *
*  in part, without the prior written consent of Silicon Graphics, Inc.  *
*                                                                        *
**************************************************************************/
#ident  "$Revision: 1.13 $ $Author: kostadis $"

#include <sgidefs.h>	/* for _MIPS_SIM defs */
#ifdef __STDC__
        #pragma weak aio_fsync = _aio_fsync
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
        #pragma weak aio_fsync64 = _aio_fsync
        #pragma weak _aio_fsync64 = _aio_fsync
#endif
#endif
#include "synonyms.h"
#include <aio.h>
#include "local.h"

/*
 * Posix 1003.1b-1993 async I/O aio_fsync
 */
int
aio_fsync(int op, struct aiocb *aio)
{
    register struct aiolink *al;
    aiowaitsema_t *as = NULL;
    aiowaitcan_t *awc;
    sigset_t oldsigset;
#ifdef __AIO_52_COMPAT__
    extern int old_aio_fsync(aiocb_t *);
#endif /*  __AIO_52_COMPAT__ */
    if(!aio)
	return (0);
#ifdef __AIO_52_COMPAT__
    if (aio->aio_sigevent.sigev_notify < 128){
	setoserror(ENOSYS);
	return (-1);
    }
#endif /*  __AIO_52_COMPAT__ */
    if (!_aioinfo)
	aio_sgi_init(NULL);
    if ((fcntl(aio->aio_fildes, F_GETFL) < 0) && errno == EBADF) {
	setoserror(EBADF);
	return (-1);
    }
	/*
	 * for now we do a sync for either O_SYNC or O_DSYNC
	 * We will do something different when the system suports
	 * fdatasync
	 */
#ifdef O_DSYNC
    if (op != O_SYNC && op != O_DSYNC) { 
#else
    if (op != O_SYNC) { 
#endif
	setoserror(EINVAL);
	return (-1);
    }
    if (KAIO_IS_ACTIVE) {
	 struct kaio_ret kaio_ret;
	 libdba_kaio_fsync(op, aio, &kaio_ret);
	 if (kaio_ret.back2user == 1)
	      return kaio_ret.rv;
    }
    /*
     * _aqueue checks the value of aio_reqprio for all operations. 
     * aio_fsync does not require it to be filled in, so we st its value here
     */
    aio->aio_reqprio = 0;	
    _aio_grab_alock(&oldsigset);
	/* 
	 * We have to walk the list of all active aiocb structures looking
	 * to see if their fd's match the one we are looking for.
	 * Whenever we find one then add a pointer to a mutex that we be
	 * decremented at the time the operation is finished. We then launch a 
	 * sproc that waits on that semaphore. When it wakes up it means
	 * that all the operations that we were waiting on have finished
	 * and that we can call fsync on the descriptor and then call
	 * the right notification.
	 */
    for (al = _aioinfo->ahead.al_forw; 
	 al != &_aioinfo->ahead; 
	 al = al->al_forw) {
	aiocb_t *req;

	req = al->al_req;
	    /*
	     * Not only do we need to check the fd, but also the operation.
	     * We need to avoid picking other operations such as another
	     * aio_fsync on the same fd
	     */
	if ((req->aio_fildes == aio->aio_fildes) &&
	    (req->aio_op == LIO_READ || req->aio_op == LIO_WRITE)) {
		/* 
		 * We have found one with the same file descriptor
		 */
		if (as == NULL) {
		    as = _aio_get_waitsema(SEMA_STD);
		    if (!as) {
			_aio_release_alock(&oldsigset);
			setoserror(EAGAIN);
			return(-1);
		    }
		    as->count = 0;
		    as->targcount = 0;
		    as->freecount = -1;
		}
		    /* 
		     * Increment the count of outstanding AIO
		     * operations
		     */
		as->count++;
		    /* Allocate a new container structure */
		awc = _aio_get_waitcan(2);
		awc->next = (aiowaitcan_t *)(req->aio_waithelp);
		awc->aws = as;
		req->aio_waithelp = (unsigned long) awc;
	    }
    }
    _aio_release_alock(&oldsigset);
	/* 
	 * If we do not have a handle then we need to do the fsync
	 * and send a notification.
	 */
    if (!as){
	int error;
/*	printf("aio_fsync: do fsync now\n"); */
	if (op == O_DSYNC) 
	    error = fdatasync(aio->aio_fildes);
	else
	    error = fsync(aio->aio_fildes);
		    
	if(error != 0)
		aio->aio_errno = oserror();
	else
		aio->aio_errno = 0;
	_aio_handle_notify(&(aio->aio_sigevent), get_pid());
	return (error);
    } else {
	    /* 
	     * All entries are now marked, 
	     * queue op the operation 
	     */
/*	printf("aio_fsync: do fsync latter [0x%x]\n",as); */
#ifdef O_DSYNC
	if (op == O_DSYNC)
	    return(_aqueue(aio, LIO_DSYNC, as));
	else
#endif
	    return(_aqueue(aio, LIO_SYNC, as));
    }
/*NOTREACHED*/
}
