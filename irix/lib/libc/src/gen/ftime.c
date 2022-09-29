/*
 * Copyright 1997 by Silicon Grapics Incorporated
 */

#ident  "$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/gen/RCS/ftime.c,v 1.5 1998/01/13 09:36:50 leedom Exp $"

#ifdef __STDC__
        #pragma weak ftime = _ftime
#endif
#include "synonyms.h"

#include <sys/timeb.h>
#include <time.h>

int
ftime(struct timeb *tp)
{
	timespec_t ts;

	if (clock_gettime(CLOCK_REALTIME, &ts) < 0)
		return -1;

	tp->time = ts.tv_sec;
	tp->millitm = (unsigned short)(ts.tv_nsec/1000000);
	/*
	 * The ftime() specification states that the contents of the timezone
	 * and dstflag fields are unspecified after a call to ftime().  Leave
	 * them as garbage here (their previous contents) so as not to
	 * encourage people with useful, but non-standard returns.
	 */
	return 0;
}
