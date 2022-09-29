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
#ident  "$Revision: 1.2 $ $Author: jeffreyh $"

#ifdef __STDC__
        #pragma weak old_aio_return = _old_aio_return
#endif
#define _POSIX_4SOURCE
#include "synonyms.h"
#include "old_aio.h"
#include "old_local.h"

/*
 * Posix.4 draft 12 async I/O aio_return
 */
ssize_t
old_aio_return(struct old_aiocb *aio)
{
	return ((long)aio->aio_nobytes);
}

