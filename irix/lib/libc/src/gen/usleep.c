/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.7 $"

#ifdef __STDC__
	#pragma weak usleep = _usleep
#endif
#include "synonyms.h"

#include <errno.h>
#include <unistd.h>
#include <time.h>
#include "mplib.h"

#define	NSPUS	1000		/* number of nanoseconds in a microsecond */
#define	USPS	1000000		/* number of microseconds in a second */

int
usleep(useconds_t useconds)
{
	struct timespec *tspecp;
	struct timespec tspec;
	int		rv;
	extern int __nanosleep(const struct timespec *, struct timespec *);

	MTLIB_CNCL_TEST();
	if (!useconds)
		return(0);
	if (useconds >= USPS) {
		setoserror(EINVAL);
		return(-1);
	}
	tspecp = &tspec;
	tspecp->tv_sec = (time_t)0;
	tspecp->tv_nsec = (long)(useconds * NSPUS);

	MTLIB_BLOCK_CNCL_VAL_1( rv,
		__nanosleep((const struct timespec *) tspecp, 0) );
	if (rv) {
		setoserror(EINVAL);
		return(-1);
	}
	return(0);
}
