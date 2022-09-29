/*************************************************************************
#                                                                        *
#               Copyright (C) 1989, Silicon Graphics, Inc.               *
#                                                                        *
#  These coded instructions, statements, and computer programs  contain  *
#  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
#  are protected by Federal copyright law.  They  may  not be disclosed  *
#  to  third  parties  or copied or duplicated in any form, in whole or  *
#  in part, without the prior written consent of Silicon Graphics, Inc.  *
#                                                                        *
#************************************************************************/

#ident "$Revision: 1.12 $"

#ifdef __STDC__
	#pragma weak sgi_revoke = _sgi_revoke
	#pragma weak satread = _satread
	#pragma weak satwrite = _satwrite
	#pragma weak saton = _saton
	#pragma weak satoff = _satoff
	#pragma weak satstate = _satstate
	#pragma weak satsetid = _satsetid
	#pragma weak satgetid = _satgetid
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <sys/syssgi.h>
#include <sys/sat.h>

/*
 * System call interfaces for Mandatory Access Control.
 */

int
sgi_revoke(char *path)
{
	return ((int) syssgi(SGI_REVOKE, path));
}

/*
 * System call interfaces for Audit
 */

int
satread(char *buffer, unsigned length)
{
	return ((int) syssgi(SGI_SATREAD, buffer, length));
}

int
satwrite(int rectype, int outcome, char *buffer, unsigned length)
{
	return ((int) syssgi(SGI_SATWRITE, rectype, outcome, buffer, length));
}

int
saton(int type)
{
	return ((int) syssgi(SGI_SATCTL, SATCTL_AUDIT_ON, type));
}

int
satoff(int type)
{
	return ((int) syssgi(SGI_SATCTL, SATCTL_AUDIT_OFF, type));
}

int
satstate(int type)
{
	return ((int) syssgi(SGI_SATCTL, SATCTL_AUDIT_QUERY, type));
}

int
satsetid(int satid)
{
	return ((int) syssgi(SGI_SATCTL, SATCTL_SET_SAT_ID, satid));
}

int
satgetid(void)
{
	return ((int) syssgi(SGI_SATCTL, SATCTL_GET_SAT_ID));
}
