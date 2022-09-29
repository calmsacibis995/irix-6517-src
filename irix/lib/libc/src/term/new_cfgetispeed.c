/*	Copyright (c) 1996 SGI */

#ident	"@(#)libc-port:gen/__new_cfgetispeed.c	1.2"

#ifdef __STDC__
	#pragma weak __new_cfgetispeed = ___new_cfgetispeed
#endif
#include "synonyms.h"
#include <errno.h>
#undef _OLD_TERMIOS
#include <sys/termios.h>

/*
 * returns input baud rate stored in c_ispeed pointed by termios_p
 */

speed_t __new_cfgetispeed(const struct termios *termios_p)
{
	if (termios_p == 0) {
		setoserror(EINVAL);
		return((speed_t) -1);
	}
	return(termios_p->c_ispeed);
}
