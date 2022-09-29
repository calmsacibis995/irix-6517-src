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
        #pragma weak kaio_error = _kaio_error
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	#pragma weak kaio_error64 = _kaio_error
	#pragma weak _kaio_error64 = _kaio_error
#endif
#endif
#include "synonyms.h"
#include <aio.h>
#include "local.h"	/* from ../../libc/src/aio */
#include <sys/dbacc.h>	/* for KAIOSEL_* defines */
#include "klocal.h"


void
kaio_error(const struct aiocb *aio, struct kaio_ret *kr)
{
     if (!_AIO_SGI_ISKAIORAW(aio->aio_fildes) || (aio->aio_kaio == 0)) {
	  /* Not KAIO */
	  kr->back2user = 0;
     }
	if (aio->aio_ret == 0) {
	     if (aio->aio_errno != EINPROGRESS) {
		  kaio_info->akwaiting[aio->aio_kaio] = KAIOSEL_NOTINUSE;
	     }
	} else {
	     kaio_info->akwaiting[aio->aio_kaio] = KAIOSEL_NOTINUSE;
	}
	kr->back2user = 0;
	return;
}
