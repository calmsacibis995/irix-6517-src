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
	#pragma weak getprid = _getprid
#endif
#include "synonyms.h"
#include <unistd.h>
#include <sys/syssgi.h>
#include <sys/types.h>

prid_t
getprid(void)
{
	prid_t prid;

	return(syssgi(SGI_GETPRID, -1, &prid) < 0 ? -1 : prid);
}
