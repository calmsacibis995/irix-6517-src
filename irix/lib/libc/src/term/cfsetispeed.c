/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/cfsetispeed.c	1.1"

#ifdef __STDC__
	#pragma weak cfsetispeed = _cfsetispeed
#endif
#include "synonyms.h"
#include <errno.h>
#include <sys/termios.h>

/*
 *sets the input baud rate stored in c_cflag to speed
 */

int cfsetispeed(termios_p, speed)
struct termios *termios_p;
speed_t speed;
{
        if (!termios_p) {
		setoserror(EINVAL);
		return(-1);
	}

	if (~CBAUD & speed) {
		setoserror(EINVAL);
		return(-1);
	}

	/* first clear the field */
	termios_p->c_cflag &= ~CIBAUD;

	/* now or'ing speed will set the field */
	termios_p->c_cflag |= (tcflag_t)(speed << 16);

	return(0);
}
