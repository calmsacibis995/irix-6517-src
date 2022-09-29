/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/cfgetospeed.c	1.2"

#ifdef __STDC__
	#pragma weak cfgetospeed = _cfgetospeed
#else
#define const
#endif
#include "synonyms.h"
#include <errno.h>
#include <sys/termios.h>

/*
 * returns output baud rate stored in c_cflag pointed by termios_p
 */

speed_t cfgetospeed(termios_p)
const struct termios *termios_p;
{
	if (!termios_p) {
		_setoserror(EINVAL);
		return((speed_t) -1);
	}
	return((speed_t)(CBAUD & termios_p->c_cflag));
}
