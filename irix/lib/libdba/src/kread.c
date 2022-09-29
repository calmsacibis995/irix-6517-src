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
        #pragma weak kaio_read = _kaio_read
#endif
#include "synonyms.h"
#include <aio.h>
#include "local.h"	/* from ../../libc/src/aio */
#include "klocal.h"

/*ARGSUSED*/
void
kaio_read(const struct aiocb *aio, struct kaio_ret *kr)
{
     kr->back2user = 0;
     return;
}
