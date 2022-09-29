/*************************************************************************
 *									  *
 * 		 Copyright (C) 1994, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 *************************************************************************/

#ident "$Revision: 1.2 $"

#ifdef __STDC__
	#pragma weak clock_settime = _clock_settime
#endif
#include "synonyms.h"

#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/syssgi.h>
#include <sys/timers.h>
/*  POSIX 1003.1b-1993 style settimeofday(2) */
int
clock_settime(clockid_t clock_id, const struct timespec *tp)
{
	struct timeval atv;
	time_t sec;

	if (clock_id != CLOCK_REALTIME ||
	    tp->tv_nsec < 0 ||
	    tp->tv_nsec >= NSEC_PER_SEC) {
	    	setoserror(EINVAL);
		return (-1);
	}
	timespec_to_timeval(&atv, tp);
	if (atv.tv_usec >= 1000000) {
		sec = (time_t)(atv.tv_usec/1000000);
		atv.tv_sec += sec;
		atv.tv_usec -= sec*1000000;
	} 

	return((int)syssgi(SGI_SETTIMEOFDAY, &atv));
}
