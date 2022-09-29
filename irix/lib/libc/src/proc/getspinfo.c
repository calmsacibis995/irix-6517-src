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
	#pragma weak getspinfo = _getspinfo
#endif
#include "synonyms.h"
#include <sys/types.h>
#include <sys/extacct.h>
#include <sys/syssgi.h>
#include <unistd.h>

int
getspinfo(struct acct_spi *spinfo)
{
	return((int) syssgi(SGI_GETSPINFO, -1, spinfo));
}
