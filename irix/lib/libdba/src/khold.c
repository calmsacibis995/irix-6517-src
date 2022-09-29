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
        #pragma weak kaio_hold = _kaio_hold
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	#pragma weak kaio_hold64 = _kaio_hold
	#pragma weak _kaio_hold64 = _kaio_hold
#endif
#endif
#include "synonyms.h"
#include <aio.h>
#include "local.h"	/* from ../../libc/src/aio */
#include "klocal.h"

/* Since kaio+callback isn't supported yet, anyone calling aio_hold()
 * is presumably being or about to be forced through libc
 * paths. Go ahead and create sprocs for them and let libc lock out
 * callbacks. */

/*ARGSUSED*/
void
kaio_hold(int shouldhold, struct kaio_ret *kr)
{
     if (USR_AIO_IS_ACTIVE) {
	  kr->back2user = 0;
     } else {
	  MAYBE_REINIT_TO_FORCE_SPROCS;
	  kr->back2user = 0;
	  kr->rv = -1;
     }
     return;
}
