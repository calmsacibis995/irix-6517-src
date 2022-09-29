/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/clock.c	1.6.2.3"
/*LINTLIBRARY*/

#include "synonyms.h"
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/times.h>
#include "gen_extern.h"

#define TIMES(B)	(B.tms_utime+B.tms_stime+B.tms_cutime+B.tms_cstime)

static long first = -1L;
static int Hz = 0;

clock_t
clock(void)
{
	struct tms buffer;

	if (!Hz && (Hz = gethz()) == 0)
		Hz = (int)CLK_TCK;

	if (times(&buffer) != -1L && first == -1L)
		first = TIMES(buffer);
	return (clock_t)((TIMES(buffer) - first) * (1000000L/Hz));
}
