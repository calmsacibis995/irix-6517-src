/*************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 *************************************************************************/

#ident "$Revision: 1.3 $"

#ifdef __STDC__
	#pragma weak settimeofday = _settimeofday
#endif
#include "synonyms.h"

#include <errno.h>
#include <sys/syssgi.h>
#include <sys/time.h>
#include <stdarg.h>

/*  SVR4 style settimeofday(2) */
/*VARARGS1*/
int
settimeofday(struct timeval *tp, ...)
{
	struct timeval atv;
	long sec;

	atv = *tp;
	if (atv.tv_usec >= 1000000) {
		sec = atv.tv_usec/1000000;
		atv.tv_sec += sec;
		atv.tv_usec -= sec*1000000;
	} else if (atv.tv_usec < 0) {
		sec = -1 - (1000000-atv.tv_usec)/1000000;
		atv.tv_sec += sec;
		atv.tv_usec += sec*1000000;
	}

	return((int)syssgi(SGI_SETTIMEOFDAY, &atv));
}
