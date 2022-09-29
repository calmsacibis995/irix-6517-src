/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1994 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Id: schedctl.c,v 3.9 1997/06/18 01:50:35 joecd Exp $"

#ifdef __STDC__
	#pragma weak schedctl = _schedctl
#endif
#include "synonyms.h"

#include <stdarg.h>
#include <sys/types.h>
#include <sys/schedctl.h>

ptrdiff_t
schedctl(int cmd, ...)
{
	va_list ap;
	long	arg;
	long	val;
	ptrdiff_t rv;

	va_start(ap, cmd);
	arg = va_arg(ap, long);
	val = va_arg(ap, long);
	rv = sysmp(MP_SCHED, cmd, arg, val);
	va_end(ap);

	return rv;
}
