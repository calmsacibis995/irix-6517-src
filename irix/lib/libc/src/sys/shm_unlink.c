/*************************************************************************
*                                                                        *
*               Copyright (C) 1995 Silicon Graphics, Inc.       	 *
*                                                                        *
*  These coded instructions, statements, and computer programs  contain  *
*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
*  are protected by Federal copyright law.  They  may  not be disclosed  *
*  to  third  parties  or copied or duplicated in any form, in whole or  *
*  in part, without the prior written consent of Silicon Graphics, Inc.  *
*                                                                        *
**************************************************************************/

#ifdef __STDC__
#pragma weak shm_unlink = _shm_unlink
#endif

#include "synonyms.h"
#include <stdio.h>
#include <unistd.h>

int
shm_unlink(const char *name)
{
	return (unlink(name));
}
