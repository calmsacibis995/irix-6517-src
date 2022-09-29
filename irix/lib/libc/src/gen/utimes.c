/*
 * utimes.c --
 *
 *	This routine maps the 4.3BSD utimes system call onto the
 *	System V utime call.
 *
 * Copyright 1988 Silicon Graphics, Inc. All rights reserved.
 */

#ident "$Revision: 1.5 $"
#ifdef __STDC__
	#pragma weak utimes = _utimes
#endif

#include "synonyms.h"
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

int
utimes(const char *file, const struct timeval tvp[2])
{
    struct utimbuf t;

    if (tvp) {
	t.actime  = (time_t)tvp[0].tv_sec;
	t.modtime = (time_t)tvp[1].tv_sec;
	return utime(file, &t);
    } else
	return utime(file, 0);
}
