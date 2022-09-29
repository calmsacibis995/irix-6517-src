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
#ident  "$Revision: 1.3 $ $Author: jeffreyh $"

#ifdef __STDC__
	#pragma weak old_aio_cancel = _old_aio_cancel
#endif
#define _POSIX_4SOURCE
#include "synonyms.h"
#include "old_aio.h"
#include "old_local.h"

/*
 * Posix.4 draft 12 async I/O cancel
 * XXX send signal if requested for canceled ones
 */
int
old_aio_cancel(int fd, struct old_aiocb *aio)
{
        register struct old_aiolink *al, *alf;
        int inprogress = 0;
        int queued = 0;

	if (_old_alock == NULL)
		return(OLD_AIO_ALLDONE);
        ussetlock(_old_alock);
        for (al = _old_ahead.al_forw; al != &_old_ahead; al = alf) {
                alf = al->al_forw;
                if ((aio && aio == al->al_req) || (al->al_fd == fd)) {
                        assert(al->al_fd == fd);
                        if (al->al_spid) {
                                /* its been started */
                                inprogress++;
                        } else {
                                queued++;
                                /* remove from queue */
				if (aio && aio == al->al_req) {
                                	aio->aio_nobytes = -1;
                                	aio->aio_errno = OLD_ECANCELED;
				}
                                al->al_back->al_forw = al->al_forw;
                                al->al_forw->al_back = al->al_back;
                                al->al_back = NULL; /* debugging */
                                al->al_forw = _old_aiofree;
                                _old_aiofree = al;
                        }
                }
        }
        usunsetlock(_old_alock);
        if (inprogress)
                return(OLD_AIO_NOTCANCELED);
        else if (queued)
                return(OLD_AIO_CANCELED);
        return(OLD_AIO_ALLDONE);
}

