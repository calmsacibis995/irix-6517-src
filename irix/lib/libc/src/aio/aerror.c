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
#ident  "$Revision: 1.7 $ $Author: kostadis $"

#include <sgidefs.h>	/* for _MIPS_SIM defs */
#ifdef __STDC__
        #pragma weak aio_error = _aio_error
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	#pragma weak aio_error64 = _aio_error
	#pragma weak _aio_error64 = _aio_error
#endif
#endif
#define _POSIX_4SOURCE
#include "synonyms.h"
#include <aio.h>
#include "local.h"

/*
 * Posix 1003.1b async I/O aio_error
 */
#ifdef __AIO_52_COMPAT__
	extern int old_aio_error(const aiocb_t *);
#endif /*  __AIO_52_COMPAT__ */
int
aio_error(const struct aiocb *aio)
{
	if (!aio) {
		setoserror(EINVAL);
                return(-1);
	}
#ifdef __AIO_52_COMPAT__
	if (aio->aio_sigevent.sigev_notify < 128)
		return (old_aio_error(aio));
#endif /*  __AIO_52_COMPAT__ */
	if (KAIO_IS_ACTIVE) {
	     /* Let kaio do some of its housekeeping */
	     struct kaio_ret kaio_ret;
	     libdba_kaio_error(aio, &kaio_ret);
	     if ((kaio_ret.back2user) == 1) {
		  return(kaio_ret.rv);
	     }
	}
	if (aio->aio_ret == 0)
		return((int)aio->aio_errno);
	else {
		setoserror(EINVAL);
		return(-1);
	}
}

