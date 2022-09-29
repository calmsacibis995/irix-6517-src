/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/tcflow.c	1.2"

#ifdef __STDC__
	#pragma weak tcflow = _tcflow
#endif
#include "synonyms.h"
#include <sys/termios.h>
#include <unistd.h>
#include <errno.h>

/* 
 *suspend transmission or reception of input or output
 */

/*
 * TCOOFF (0) -> suspend output 
 * TCOON  (1) -> restart suspend output
 * TCIOFF (2) -> suspend input 
 * TCION  (3) -> restart suspend input
 */

int tcflow(fildes,action)
int fildes;
int action;
{
struct termios t;
tcflag_t olflag;

	switch(action) {
	case TCOOFF:
	case TCOON:
		return(ioctl(fildes,TCXONC,action));
	case TCIOFF:
	case TCION:

	/* The POSIX definition of TCIOFF & TCION is that STOP/START chars
	 * shall be emitted regardless of the state of the IXOFF mechanism.
	 * ioctl(fd, TCXONC, TCION/TCIOFF) is not appropriate, since it
	 * interacts with that mechanism. */

	/* It must also ignore any TOSTOP so for now I'll just fiddle
	 * TOSTOP off here while writing the XON/XOFF char.  This would be
	 * better done with special action values for this ioctl, but that
	 * would require hacking all the drivers and that's too risky this
	 * close to release.
	 */

		if (tcgetattr(fildes, &t) < 0)
			return (-1);
		olflag = t.c_lflag;
		t.c_lflag &= ~TOSTOP;
		if (tcsetattr(fildes, TCSADRAIN, &t) < 0)
			return (-1);

		if (action == TCIOFF)
			write(fildes, &t.c_cc[VSTOP], 1);
		else
			write(fildes, &t.c_cc[VSTART], 1);

		t.c_lflag = olflag;
		if (tcsetattr(fildes, TCSADRAIN, &t) < 0)
			return (-1);

		return (0);
	default:
		setoserror(EINVAL);
		return (-1);
	}
}
