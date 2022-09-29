/*************************************************************************
#                                                                        *
#               Copyright (C) 1997, Silicon Graphics, Inc.               *
#                                                                        *
#  These coded instructions, statements, and computer programs  contain  *
#  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
#  are protected by Federal copyright law.  They  may  not be disclosed  *
#  to  third  parties  or copied or duplicated in any form, in whole or  *
#  in part, without the prior written consent of Silicon Graphics, Inc.  *
#                                                                        *
#************************************************************************/

#ifdef __STDC__
	#pragma weak arsctl = _arsctl
#endif
#include "synonyms.h"
#include <sys/arsess.h>
#include <sys/syssgi.h>
#include <sys/types.h>

int
arsctl(int subfunc, void *buffer, int buflen)
{
	return((int) syssgi(SGI_ARSESS_CTL, subfunc, buffer, buflen));
}
