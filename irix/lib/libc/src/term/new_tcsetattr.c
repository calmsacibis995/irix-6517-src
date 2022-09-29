/*	Copyright (c) 1996 SGI */

#ident	"@(#)libc-port:gen/new_tcsetattr.c	1.4"
#ifdef __STDC__
	#pragma weak __new_tcsetattr = ___new_tcsetattr
#endif
#include "synonyms.h"
#include <sys/types.h>
#undef _OLD_TERMIOS
#include <sys/termios.h>
#include <errno.h>
#include <unistd.h>

/* 
 * set parameters associated with termios
 */

int __new_tcsetattr (int fildes, 
	       int optional_actions, 
	       const struct termios *termios_p)
{

	int rval;
	struct termios tios, *tios_p;

	/*
	 * Bug 575716.  Because the old compatibility interfaces
	 * use (c_cflag&CBAUD) == B1800 to represent an out-of-range
	 * baud rate, we don't allow the new interfaces to set
	 * this rate.  If we did, confusion would result if the
	 * old interface were then called.
	 */
	if (   (termios_p->c_ispeed == __NEW_INVALID_BAUD)
	    || (termios_p->c_ospeed == __NEW_INVALID_BAUD)) {
		setoserror(EINVAL);
		return -1;
	}

	/*
	 * POSIX foo.  Setting the input baud rate to numeric 0 means that
	 * tcsetattr should set the input baud rate == the output baud rate.
	 * Also see bug 269596.
	 */
	if (termios_p->c_ispeed == 0) {
		tios = *termios_p;
		tios_p = &tios;
		tios_p->c_ispeed = tios_p->c_ospeed;
	} else {
		tios_p = (struct termios *)termios_p;
	}

	switch (optional_actions) {
	
		case TCSANOW:

			rval = ioctl(fildes,TCSETS,tios_p);
			break;

		case TCSADRAIN:

			rval = ioctl(fildes,TCSETSW,tios_p);
			break;

		case TCSAFLUSH:

			rval = ioctl(fildes,TCSETSF,tios_p);
			break;

		default:

			rval = -1;
			setoserror(EINVAL);
	}
	return(rval);
}
