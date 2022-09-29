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
        #pragma weak aio_init = _aio_init
#endif
#define _POSIX_4SOURCE
#include "synonyms.h"
#include "old_aio.h"
#include <ulocks.h>
#include "old_local.h"

/*
 * User accessible routine available to initialize aio interface
 * maxuser - maximum number of user threads that will use aio interface
 */
void
aio_init()
{
    __old_aio_init(_OLD_AIO_MAX);
}
