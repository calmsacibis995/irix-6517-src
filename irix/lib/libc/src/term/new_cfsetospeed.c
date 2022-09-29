/*	Copyright (c) 1996 SGI */

#ident	"@(#)libc-port:gen/__new_cfsetospeed.c	1.1"

#ifdef __STDC__
	#pragma weak __new_cfsetospeed = ___new_cfsetospeed
#endif
#include "synonyms.h"
#include <errno.h>
#undef _OLD_TERMIOS
#include <sys/termios.h>

/*
 *sets the output baud rate stored in c_ospeed to speed
 */

int __new_cfsetospeed(struct termios *termios_p, speed_t speed)
{
	if (!termios_p) {
		setoserror(EINVAL);
		return(-1);
	}

	termios_p->c_ospeed = speed;

	return(0);
}
