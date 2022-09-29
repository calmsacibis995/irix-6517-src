/*************************************************************************
*                                                                        *
*               Copyright (C) 1994, Silicon Graphics, Inc.       	 *
*                                                                        *
*  These coded instructions, statements, and computer programs  contain  *
*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
*  are protected by Federal copyright law.  They  may  not be disclosed  *
*  to  third  parties  or copied or duplicated in any form, in whole or  *
*  in part, without the prior written consent of Silicon Graphics, Inc.  *
*                                                                        *
**************************************************************************/
#ident  "$Revision: 1.6 $ $Author: kostadis $"

#include <sgidefs.h>	/* for _MIPS_SIM defs */
#ifdef __STDC__
	#pragma weak aio_hold = _aio_hold
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	#pragma weak aio_hold64 = _aio_hold
	#pragma weak _aio_hold64 = _aio_hold
#endif
#endif
#include "synonyms.h"
#include <aio.h>
#include "local.h"

/* Callbacks can be in three states
 * 	1) Unheld
 *	2) Held by user
 *	3) Held due to some other callback being called
 *
 * As long as callbacks are held they will be called. As soon as the user 
 * releases the callback all the held callbacks will be called in the 
 * context of the call to aio_hold(AIO_RELEASE_CALLBACK).
 * Callbacks that can not be delivered because a user is holding them are put
 * on a delayed callback list.
 */

/*
 * MIPS ABI addition to AIO. aio_hold
 * This is used to hold callback from being sent to the user.
 * It does not effect signal delivery
 */

static 
int
_aio_hold_with_lock(int shouldhold)
{
    int retval;
    holdlist_t *tmp;
	/* 
	 * The return value is the value before the call 
	 */
    retval =  _aioinfo->aiocbbeingheld ? 1 : 0;
	
    switch(shouldhold) {
	case AIO_HOLD_CALLBACK:
	    _aioinfo->aiocbbeingheld++;
	    break;
	case AIO_RELEASE_CALLBACK:
		/* Check that callbacks will no longer being held
		 * and there are waiting callbacks. When that is
		 * the case walk the delayed callback list and call
		 * each callback */
	    if (_aioinfo->aiocbbeingheld == 1){
		while (_aioinfo->aiocbholdlist) {
		    sigevent_t *event;

		    event = &_aioinfo->aiocbholdlist->event;
		    (*event->sigev_func)(event->sigev_value);
		    tmp = _aioinfo->aiocbholdlist;
		    _aioinfo->aiocbholdlist = _aioinfo->aiocbholdlist->next;
		    usfree(tmp, _aioinfo->ahandle );
		}
	    }
		/* As long as it is being held decrement the count one */
	    if (_aioinfo->aiocbbeingheld)
		_aioinfo->aiocbbeingheld--;

	    break;
	case AIO_ISHELD_CALLBACK:
	    break;
	default:
	    retval = -1;
	    setoserror(EINVAL);
	    break;
    }
    return(retval);
}

int 
aio_hold(int shouldhold)
{
    int retval;
    if(!_aioinfo)
	aio_sgi_init(NULL);
    if (KAIO_IS_ACTIVE) {
         struct kaio_ret kaio_ret;
         libdba_kaio_hold(shouldhold, &kaio_ret);
         if ((kaio_ret.back2user) == 1)
              return(kaio_ret.rv);
    }
    ussetlock(_aioinfo->acblock);
    retval = _aio_hold_with_lock(shouldhold);
    usunsetlock(_aioinfo->acblock);
    return (retval);
}

void
_aio_call_callback(sigevent_t *event)
{
    int alreadyheld;
    assert(event);
    ussetlock(_aioinfo->acblock);
    assert(event->sigev_notify == SIGEV_CALLBACK);
	/* 
	 * Hold off callbacks, this single threads them to make it 
	 * easier to write them
	 */
    alreadyheld = _aio_hold_with_lock(AIO_HOLD_CALLBACK);
    assert (_aioinfo->aiocbbeingheld > 0); 

	/* 
	 * Check to see if we are the first to hold callbacks.
	 * If we are then we can call the callback now, otherwise
	 * we need to queue it up to be called latter and return */
    if (!alreadyheld) {
	    /* Call the callback */
	(*event->sigev_func)(event->sigev_value);
    } else {
	    /* Someone else has callbacks held. Queue it up for latter */
	holdlist_t *tmp;
	
	tmp = (holdlist_t *)usmalloc(sizeof(struct holdlist), _aioinfo->ahandle );
	    /*
	     * This needs to be a copy so that it does not get
	     * freed by the user before we can call the callback
	     * after we set the progress flag to complete.
	     */
	tmp->event = *event;
	tmp->next = _aioinfo->aiocbholdlist;
	_aioinfo->aiocbholdlist = tmp;
    }
    _aio_hold_with_lock(AIO_RELEASE_CALLBACK);
    usunsetlock(_aioinfo->acblock);
}


