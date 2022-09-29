/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/tcsetattr.c	1.4"
#ifdef __STDC__
	#pragma weak tcsetattr = _tcsetattr
#define CONST const
#else
#define CONST
#endif
#include "synonyms.h"
#include <sys/types.h>
#include <sys/termios.h>
#include <errno.h>
#include <unistd.h>

/* 
 * set parameters associated with termios
 */

int tcsetattr (fildes, optional_actions, termios_p)
int fildes;
int optional_actions;
CONST struct termios *termios_p;
{

	int rval;
	struct termios tios, *tios_p;
	
	/*
	 * POSIX foo.  Setting the input baud rate to numeric 0 means that
	 * tcsetattr should set the input baud rate == the output baud rate.
	 * Also see bug 269596.
	 */
	if (!(termios_p->c_cflag & CIBAUD)) {
		tios = *termios_p;
		tios_p = &tios;
		tios_p->c_cflag |= ((tios_p->c_cflag & CBAUD) << 16);
	} else
		tios_p = (struct termios *)termios_p;

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
