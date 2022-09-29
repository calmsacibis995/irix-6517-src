 /*************************************************************************
 #									  *
 # 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 #									  *
 #  These coded instructions, statements, and computer programs  contain  *
 #  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 #  are protected by Federal copyright law.  They  may  not be disclosed  *
 #  to  third  parties  or copied or duplicated in any form, in whole or  *
 #  in part, without the prior written consent of Silicon Graphics, Inc.  *
 #									  *
 #************************************************************************/

#ident "$Revision: 1.10 $"

#ifdef __STDC__
	#pragma weak BSDgettimeofday = _BSDgettimeofday
#endif
#include "synonyms.h"

#include <sys/time.h>
#include "sys_extern.h"

/*  BSD style gettimeofday(2) */
int					/* return 0 or -1 */
BSDgettimeofday(struct timeval *tp, struct timezone *tzp)
{
	/*
	 * In the SUN kernel the 'tzp' argument can be 0, in which case
	 * the timezone is not returned.  This feature isn't documented
	 * in the 4.2 manual from Berkeley.
	 */
	if (tzp) {
		tzset();
		tzp->tz_minuteswest = timezone / 60;
		tzp->tz_dsttime = daylight;
	}

	return (tp!=0 ? BSD_getime(tp) : 0);
}
