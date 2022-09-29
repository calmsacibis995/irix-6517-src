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
	#pragma weak aio_cancel64 = _aio_cancel64
#endif
#include "synonyms.h"
#include <aio.h>
#include "local.h"

int
aio_cancel64(int fd, struct aiocb64 *aio)
{
	aio->aio_oldoff = -1;
	aio->aio_64bit = 0xdeadbeef;
	return aio_cancel(fd, (struct aiocb *)aio);
}
