/*
 *  SpiderTCP Network Daemon
 *
 *      Debugging utilities
 *
 *  Copyright 1991  Spider Systems Limited
 *
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/etc/netd/0/s.debug.c
 *	@(#)debug.c	1.7
 *
 *	Last delta created	17:06:02 2/4/91
 *	This file extracted	14:52:21 11/13/92
 *
 */

#ident "@(#)debug.c	1.7	11/13/92"

#include <stdio.h>
#include "debug.h"

int	debug = 0;		/* debugging OFF by default */

/*
 *  db_print()		--	conditional diagnostic routine
 *
 *  This routine handles up to three arguments in addition to the
 *  format string.  Diagnostics are printed only if the debug flag
 *  has been set on the command line.
 */

void
db_print(format, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9)
char	*format;
char	*arg1, *arg2, *arg3, *arg4, *arg5, *arg6, *arg7, *arg8, *arg9;
{
	if (debug)
	{
		fprintf(stderr, "\033)");
		fprintf(stderr, format, arg1, arg2, arg3,
					arg4, arg5, arg6,
					arg7, arg8, arg9);
		fprintf(stderr, "\033(");
	}
}
