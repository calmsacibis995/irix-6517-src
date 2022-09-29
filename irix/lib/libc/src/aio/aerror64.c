/*************************************************************************
*                                                                        *
*               Copyright (C) 1995, Silicon Graphics, Inc.       	 *
*                                                                        *
*  These coded instructions, statements, and computer programs  contain  *
*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
*  are protected by Federal copyright law.  They  may  not be disclosed  *
*  to  third  parties  or copied or duplicated in any form, in whole or  *
*  in part, without the prior written consent of Silicon Graphics, Inc.  *
*                                                                        *
**************************************************************************/
#ident  "$Revision: 1.4 $ $Author: ack $"

#ifdef __STDC__
        #pragma weak aio_error64 = _aio_error64
#endif
#include "synonyms.h"
#include <aio.h>
#include "local.h"

int
aio_error64(const struct aiocb64 *aio)
{
       ((struct aiocb64 *)aio)->aio_oldoff = -1;
       ((struct aiocb64 *)aio)->aio_64bit = 0xdeadbeef;
	return aio_error((const struct aiocb *)aio);
}
