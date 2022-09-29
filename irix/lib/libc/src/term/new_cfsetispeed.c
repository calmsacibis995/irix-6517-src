/*	Copyright (c) 1996 SGI */

#ident	"@(#)libc-port:gen/__new_cfsetispeed.c	1.1"

#ifdef __STDC__
	#pragma weak __new_cfsetispeed = ___new_cfsetispeed
#endif
#include "synonyms.h"
#include <errno.h>
#undef _OLD_TERMIOS
#include <sys/termios.h>

/*
 *sets the input baud rate stored in c_ispeed to speed
 */

int __new_cfsetispeed(struct termios *termios_p, speed_t speed)
{
        if (!termios_p) {
		setoserror(EINVAL);
		return(-1);
	}

	termios_p->c_ispeed = speed;

	return(0);
}
