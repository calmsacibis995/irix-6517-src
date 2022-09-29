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
#ident  "$Revision: 1.1 $ $Author: jeffreyh $"

#ifdef __STDC__
        #pragma weak old_aio_error = _old_aio_error
#endif
#define _POSIX_4SOURCE
#include "synonyms.h"
#include "old_aio.h"
#include "old_local.h"

/*
 * Posix.4 draft 12 async I/O aio_error
 */
int
old_aio_error(struct old_aiocb *aio)
{
	return (aio->aio_errno);
}

