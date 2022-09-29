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

#ident "$Revision: 1.9 $"

#ifdef __STDC__
	#pragma weak BSDsettimeofday = _BSDsettimeofday
#endif
#include "synonyms.h"

#include <errno.h>
#include <sys/syssgi.h>
#include <sys/time.h>

/*  BSD style settimeofday(2) */
int
BSDsettimeofday(struct timeval *tp, struct timezone *tzp)
{
	struct timeval atv;
	long sec;

	if (tzp != 0) {			/* cannot set timezone in SV */
		setoserror(EINVAL);
		return -1;
	}

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
