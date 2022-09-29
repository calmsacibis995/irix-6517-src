/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident "$Revision: 1.5 $"

#include "uucp.h"

/*
	Report and log file transfer rate statistics.
	This is ugly because we are not using floating point.
*/

void
statlog( direction, bytes, millisecs )
char		*direction;
unsigned long	bytes;
time_t		millisecs;
{
	char		text[ 100 ];
#ifdef sgi
	/* what happens if you send a file bigger than 4,000,000 bytes? */

	/* on fast machines, times(2) resolution may not be enough */
	/* so millisecs may be zero.  just use 1 as best guess */
	if ( millisecs == 0 )
		millisecs = 1;
	(void) sprintf(text, "%s %lu / %.1f secs, %.0f bytes/sec",
		direction, bytes, millisecs/1000.0,
		bytes*1000.0/millisecs );
	CDEBUG(4, "%s\n", text);
	uusyslog(text);
#else
	unsigned long	bytes1000;

	bytes1000 = bytes * 1000;
	/* on fast machines, times(2) resolution may not be enough */
	/* so millisecs may be zero.  just use 1 as best guess */
	if ( millisecs == 0 )
		millisecs = 1;
	(void) sprintf(text, "%s %lu / %lu.%.3u secs, %lu bytes/sec",
		direction, bytes, millisecs/1000, millisecs%1000,
		bytes1000/millisecs );
	CDEBUG(4, "%s\n", text);
	syslog(text);
#endif
}
