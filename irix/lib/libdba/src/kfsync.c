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
        #pragma weak kaio_fsync = _kaio_fsync
#endif
#include "synonyms.h"
#include <errno.h>
#include <aio.h>
#include "local.h"	/* from ../../libc/src/aio */
#include "klocal.h"

/*
 * aio_fsync is supposed to flush all data on indicated file descriptor out
 * to disk. Since kaio doesn't track in-flight aiocb's it can't tell when
 * they have all finished. Also, if kaio and libc aio are mixed on the same
 * fd, the same kaio problem arises. For now, we do not support sync on
 * kaio aio operations.
 */

/*ARGSUSED*/
void
kaio_fsync(int op, struct aiocb *aio, struct kaio_ret *kr)
{
     if (_AIO_SGI_ISKAIORAW(aio->aio_fildes)) {
	  /* If we allow fallback to libc on raw fd failures, this needs
	   * to check USR_AIO_IS_ACTIVE before returning (when sync is
	   * supported). */
	  kr->back2user = 1;
	  kr->rv = -1;
#ifdef DEBUG
	  printf("kaio_fsync: was called for a raw fd (%d)\n", aio->aio_fildes);
#endif
	  setoserror(ENOTSUP);
     } else if (USR_AIO_IS_ACTIVE) {
	  kr->back2user = 0;
	  kr->rv = -2;
     } else {
	  /* Hmm... libc might assume it's old 5.2 i/o in this case */
	  /* Do not do a MAYBE_REINIT.. here, since it wouldn't be legal to
	   * call aio_fsync before submitting an aio request. */
	  kr->back2user = 1;
	  kr->rv = -1;
	  setoserror(ENOTSUP);
     }
     return;
}
