/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1994 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.9 $"

/*
 *	Emulation of BSD 4.3 getpriority() and setpriority() system calls
 *	based on the IRIX "schedctl()" system call.
 */

#ifdef __STDC__
	#pragma weak getpriority = _getpriority
	#pragma weak setpriority = _setpriority
#endif
#include "synonyms.h"

#include <sys/types.h>
#include <sys/schedctl.h>
#include <sys/resource.h>
#include <errno.h>

int
getpriority(int	which, id_t who)
{
	switch (which) {
	case PRIO_PROCESS:
		return((int)schedctl(GETNICE_PROC, who));
	case PRIO_PGRP:
		return((int)schedctl(GETNICE_PGRP, who));
	case PRIO_USER:
		return((int)schedctl(GETNICE_USER, who));
	default:
		setoserror(EINVAL);
	}
	return(-1);
}

int
setpriority(int	which, id_t who, int prio)
{
	switch (which) {
	case PRIO_PROCESS:
		return((int)schedctl(RENICE_PROC, who, prio));
	case PRIO_PGRP:
		return((int)schedctl(RENICE_PGRP, who, prio));
	case PRIO_USER:
		return((int)schedctl(RENICE_USER, who, prio));
	default:
		setoserror(EINVAL);
	}
	return(-1);
}
