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
	#pragma weak clock_getres = _clock_getres
#endif
#include "synonyms.h"

#include <errno.h>
#include <sys/syssgi.h>
#include <sys/time.h>
#include <sys/param.h>
/*  POSIX 1003.1b-1993 clock resolution check */
int
clock_getres(clockid_t clock_id, struct timespec *res)
{

    if (res == NULL)
	    return(0);

    res->tv_sec = 0;
    switch(clock_id) {
	case CLOCK_REALTIME:
	    res->tv_nsec = NSEC_PER_SEC / HZ;
	    break;
	case CLOCK_SGI_CYCLE:
	{
	    int cycleval;
	    (void) syssgi(SGI_QUERY_CYCLECNTR, &cycleval);
	    res->tv_nsec = cycleval/1000; /* Was in picoseconds */
	    break;
	}
	case CLOCK_SGI_FAST:
	    res->tv_nsec = syssgi(SGI_QUERY_FASTTIMER);
	    break;
	default:
	    setoserror(EINVAL);
	    return (-1);
    }
    return 0;
}

