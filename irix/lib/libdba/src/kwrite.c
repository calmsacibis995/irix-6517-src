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
        #pragma weak kaio_write = _kaio_write
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	#pragma weak kaio_write64 = _kaio_write
	#pragma weak _kaio_write64 = _kaio_write
#endif
#endif
#include "synonyms.h"
#include <aio.h>
#include "local.h"	/* ../../libc/src/aio */
#include "klocal.h"

/*ARGSUSED*/
void
kaio_write(const struct aiocb *aio, struct kaio_ret *kr)
{
     kr->back2user = 0;
     return;
}
