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
        #pragma weak aio_return64 = _aio_return64
#endif
#include "synonyms.h"
#include <aio.h>
#include "local.h"

ssize_t
aio_return64(struct aiocb64 *aio)
{
	aio->aio_oldoff = -1;
	aio->aio_64bit = 0xdeadbeef;
	return aio_return((struct aiocb *)aio);
}
