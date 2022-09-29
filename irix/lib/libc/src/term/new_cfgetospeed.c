/*	Copyright (c) 1996 SGI */

#ident	"@(#)libc-port:gen/__new_cfgetospeed.c	1.2"

#ifdef __STDC__
	#pragma weak __new_cfgetospeed = ___new_cfgetospeed
#endif
#include "synonyms.h"
#include <errno.h>
#undef _OLD_TERMIOS
#include <sys/termios.h>

/*
 * returns output baud rate stored in c_ospeed pointed by termios_p
 */

speed_t __new_cfgetospeed(const struct termios *termios_p)
{
	if (termios_p == 0) {
		setoserror(EINVAL);
		return((speed_t) -1);
	}
	return(termios_p->c_ospeed);
}
