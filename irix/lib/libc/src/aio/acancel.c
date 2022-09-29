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
#ident  "$Revision: 1.18 $ $Author: kostadis $"

#include <sgidefs.h>	/* for _MIPS_SIM defs */
#ifdef __STDC__
	#pragma weak aio_cancel = _aio_cancel
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	#pragma weak aio_cancel64 = _aio_cancel
	#pragma weak _aio_cancel64 = _aio_cancel
#endif
#endif
#include "synonyms.h"
#include <string.h>
#define	_AIO_INTERNAL
#include <fcntl.h>
#include <aio.h>
#include "local.h"
/*
 * Posix 1003.1b async I/O cancel
 */
#ifdef __AIO_52_COMPAT__
extern int old_aio_cancel(int fd, aiocb_t *);
#endif /*  __AIO_52_COMPAT__ */
int
aio_cancel(int fd, struct aiocb *aio)
{
    register struct aiolink *al, *alf;
    sigset_t oldsigset;
	
    int inprogress = 0;
    int queued = 0;
#ifdef __AIO_52_COMPAT__
    if (aio && aio->aio_sigevent.sigev_notify < 128)
	    return (old_aio_cancel(fd, aio));
    
#endif /*  __AIO_52_COMPAT__ */
    /*
     * Unix 98 really want us to test for a valid fd.
     * This is the quickest way that I found.
     */
    if (fcntl(fd, F_GETFL) < 0) {
	    return(-1);
    }
    if (!_aioinfo)
	aio_sgi_init(NULL);
    if (KAIO_IS_ACTIVE) {
	 struct kaio_ret kaio_ret;
	 libdba_kaio_cancel(fd, aio, &kaio_ret);
	 if ((kaio_ret.back2user) == 1)
	      return(kaio_ret.rv);
	 /*otherwise, continue normal libc path*/
    }
    _aio_grab_alock(&oldsigset);
    for (al = _aioinfo->ahead.al_forw; al != &_aioinfo->ahead; al = alf) {
	aiocb_t *req = al->al_req;

	alf = al->al_forw;
	if (aio && aio != req) {
		continue;
	}
	if ((aio == NULL) &&  (req->aio_fildes != fd)) {
		continue;
	}
	assert(req->aio_fildes == fd);
	if (al->al_busy) {
		/* its been started */
		inprogress++;
	} else {
		queued++;
		/* Mark it as canceled */
		req->aio_nobytes = -1;
		req->aio_errno = ECANCELED;
		/* Send signal/callback if requested */
		_aio_handle_notify((sigevent_t *)&(req->aio_sigevent), 
				   al->al_rpid);
		/* remove from queue */
		_aioworkcans((aiowaitcan_t *)(al->al_req->aio_waithelp));
		_aiofreelink(al);
	}
    }
    _aio_release_alock(&oldsigset);
    if (inprogress)
	return(AIO_NOTCANCELED);
    else if (queued)
	return(AIO_CANCELED);
#ifdef __AIO_52_COMPAT__ 
    /* 
	 * if we found no matches and we had no aio then we will need
	 * to search the list in the compatibility code
	 */
    else if (!aio)
	return (old_aio_cancel(fd, aio));
#endif /*  __AIO_52_COMPAT__ */
    return(AIO_ALLDONE);
}

