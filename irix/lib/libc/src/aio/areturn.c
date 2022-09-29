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
#ident  "$Revision: 1.9 $ $Author: kostadis $"

#include <sgidefs.h>	/* for _MIPS_SIM defs */
#ifdef __STDC__
        #pragma weak aio_return = _aio_return
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
        #pragma weak aio_return64 = _aio_return
        #pragma weak _aio_return64 = _aio_return
#endif
#endif
#include "synonyms.h"
#include <aio.h>
#include "local.h"

/*
 * Posix 1003.1b-1993 async I/O aio_return
 */
#ifdef __AIO_52_COMPAT__
	extern int old_aio_return(aiocb_t *);
#endif /*  __AIO_52_COMPAT__ */
ssize_t
aio_return(struct aiocb *aio)
{
	ssize_t rv;

	if (!aio) {
		setoserror(EINVAL);
                return(-1);
	}
#ifdef __AIO_52_COMPAT__
	if (aio->aio_sigevent.sigev_notify < 128)
		return (old_aio_return(aio));
#endif /*  __AIO_52_COMPAT__ */
	
	if (KAIO_IS_ACTIVE) {
	     struct kaio_ret kaio_ret;
	     libdba_kaio_return(aio, &kaio_ret);
	     if ((kaio_ret.back2user) == 1) {
		  return(kaio_ret.rv);
	     }
	}
	if ((aio->aio_errno != EINPROGRESS) &&
	    (aio->aio_ret == 0)) {
			rv = (ssize_t)aio->aio_nobytes;
			aio->aio_ret = 1;
	} else {
		rv = -1;
		setoserror(EINVAL);
	}
        return(rv);
}

