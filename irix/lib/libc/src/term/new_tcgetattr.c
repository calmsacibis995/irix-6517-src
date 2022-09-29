/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/__new_tcgetattr.c	1.2"

#ifdef __STDC__
	#pragma weak __new_tcgetattr = ___new_tcgetattr
#endif
#include "synonyms.h"
#undef _OLD_TERMIOS
#include <sys/termios.h>
#include <unistd.h>

/* 
 * get parameters associated with fildes and store them in termios
 */

int __new_tcgetattr (int fildes, struct termios *termios_p)
{
	return(ioctl(fildes,TCGETS,termios_p));
}
