/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.1 $"

#ifdef __STDC__
	#pragma weak gettimeofday = _gettimeofday
	#pragma weak settimeofday = _settimeofday
#endif
#include "synonyms.h"

#include <sys/time.h>
#include <stdarg.h>

/*
 * Wrappers for BSD version of get/set timeofday which are in libc.
 */

/*VARARGS*/
int					/* return 0 or -1 */
gettimeofday(struct timeval *tp, /* struct timezone *tzp */ ...)
{
	struct timezone *tzp;
	va_list ap;

	va_start(ap, tp);	/* make ap point to 1st unnamed arg */
	tzp = va_arg(ap, struct timezone *);
	va_end(ap);

	return(BSDgettimeofday(tp, tzp));
}

/*VARARGS*/
int
settimeofday(struct timeval *tp, /* struct timezone *tzp */ ...)
{
	struct timezone *tzp;
	va_list ap;

	va_start(ap, tp);	/* make ap point to 1st unnamed arg */
	tzp = va_arg(ap, struct timezone *);
	va_end(ap);

	return(BSDsettimeofday(tp, tzp));
}
