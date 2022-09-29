/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/cfgetispeed.c	1.2"

#ifdef __STDC__
	#pragma weak cfgetispeed = _cfgetispeed
#define CONST const
#else
#define CONST
#endif
#include "synonyms.h"
#include <errno.h>
#include <sys/termios.h>

/*
 * returns input baud rate stored in c_cflag pointed by termios_p
 */

speed_t cfgetispeed(termios_p)
CONST struct termios *termios_p;
{
	if (termios_p == 0) {
	       setoserror(EINVAL);
	       return((speed_t) -1);
	}
	return((speed_t)((CIBAUD & termios_p->c_cflag) >> 16));
}
