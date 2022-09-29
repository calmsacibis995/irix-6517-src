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
#ident  "$Revision: 1.1 $ $Author: wombat $"

#include <sgidefs.h>	/* for _MIPS_SIM defs */
#ifdef __STDC__
	#pragma weak kaio_cancel = _kaio_cancel
#endif
#include "synonyms.h"
#define	_AIO_INTERNAL
#include <aio.h>
#include "local.h"	/* from libc/src/aio */
#include "klocal.h"

/*
 * aio_cancel is supposed to cancel a single i/o op if aio != 0, or all
 * async i/o on the indicated file descriptor if aio == 0.
 */

void
kaio_cancel(int fd, struct aiocb *aio, struct kaio_ret *kr)
{
     /*
      * If user has passed in a raw file descriptor, assume for now it is kaio
      * and therefore not cancelable.
      * libc has already verified whether or not the fd is open.
      */
     if (USR_AIO_IS_ACTIVE) {
	  if (! _AIO_SGI_ISKAIORAW(fd)) {
	       kr->back2user = 0;
	       return;
	  } else {
	       /* If this is a raw file, and we allow fallback into libc aio
		  if kaio fails, then must check to see if this i/o request
		  could have been something passed back to libc. */
	       if (aio && !aio->aio_kaio) {
		    kr->back2user = 0;
		    return;
	       }
	  }
     }
     kr->back2user = 1;
     kr->rv = AIO_NOTCANCELED;
     return;
}
