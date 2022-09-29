/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/tcdrain.c	1.2"

#ifdef __STDC__
	#pragma weak tcdrain = _tcdrain
#endif
#include "synonyms.h"
#include <sys/termios.h>
#include <unistd.h>
#include "mplib.h"

/*
 * wait until all output on the filedes is drained
 */

int
tcdrain(int fildes)
{
	extern int __ioctl(int, int, ...);

	MTLIB_BLOCK_CNCL_RET( int, __ioctl(fildes, TCSBRK, 1) );
}
