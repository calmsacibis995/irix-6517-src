/*************************************************************************
#                                                                        *
#               Copyright (C) 1995, Silicon Graphics, Inc.               *
#                                                                        *
#  These coded instructions, statements, and computer programs  contain  *
#  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
#  are protected by Federal copyright law.  They  may  not be disclosed  *
#  to  third  parties  or copied or duplicated in any form, in whole or  *
#  in part, without the prior written consent of Silicon Graphics, Inc.  *
#                                                                        *
#************************************************************************/

#ifdef __STDC__
	#pragma weak getash = _getash
#endif
#include "synonyms.h"
#include <unistd.h>
#include <sys/syssgi.h>
#include <sys/types.h>

ash_t
getash(void)
{
	ash_t ash;

	return(syssgi(SGI_GETASH, -1, &ash) < 0 ? -1 : ash);
}
