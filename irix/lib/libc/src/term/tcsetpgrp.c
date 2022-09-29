/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/tcsetpgrp.c	1.2"
#ifdef __STDC__
	#pragma weak tcsetpgrp = _tcsetpgrp
#endif
#include "synonyms.h"
#include <sys/termios.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

int
tcsetpgrp(int fd, pid_t pgrp)
{
	int ret;

	/*
	 * POSIX and XPG4 specify that tcsetpgrp() return ENOTTY if the
	 * terminal is no longer associated with the session of the current
	 * process.  Currently the IRIX kernel returns EIO in cases where the
	 * session generation number on a terminal no longer matches the
	 * generation number associated with the file descriptor.  This
	 * standards conformance bug is documented in bug #693664.  We need
	 * to fix this for IRIX 6.5.5 because of critical acceptance issues
	 * but it's too late in the 6.5.5 development cycle to do anything
	 * radical.  It *might* be appropriate to change this at a lower
	 * level but for now the safest fix is to remap EIO to ENOTTY here.
	 */
	if (tcgetsid(fd) < 0) {
		if (oserror() == EIO)
			setoserror(ENOTTY);
		return -1;
	}
	ret = ioctl(fd, TIOCSPGRP, &pgrp);
	if (ret < 0 && oserror() == EIO)
		setoserror(ENOTTY);
	return ret;
}
