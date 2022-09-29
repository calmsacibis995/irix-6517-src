/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident  "$Revision: 1.6 $"

/*
 * SGI compatibility routines to emulate some BSD functions.
 * Some of these are complete enough that they should go in libbsd
 * eventually.
 */

#include "sh.h"
#include <sys/times.h>
#include <sys/termio.h>


/*
 * return current value of the line discipline (System V style)
 *
 * This isn't a BSD interface, it's just a replacement for
 * ioctl(TIOCGETD).
 */
int
getldisc(
	int f,		/* tty file descriptor */
	int *ldiscp,	/* gets answer for line discipline */
	int *swtchp)	/* gets answer for swtch char */
{
	struct termio tty;

	if (ioctl(f, TCGETA, &tty) < 0)
		return(-1);
	
	*ldiscp = (int) tty.c_line;
	*swtchp = (int) tty.c_cc[VSUSP];
	return(0);
}

/*
 * set current value of the line discipline (System V style)
 *
 * This isn't a BSD interface, it's just a replacement for
 * ioctl(TIOCSETD).
 */
int
setldisc(
	int f,		/* tty file descriptor */
	int ldisc,	/* new discipline (or -1) */
	int swtch)	/* new swtch character (or -1) */
{
	struct termio tty;

	if (ldisc == -1 && swtch == -1)
		/* nothing to do */
		return(0);

	if (ioctl(f, TCGETA, &tty) < 0)
		return(-1);
	
	if (ldisc != -1)
		tty.c_line = (char) ldisc;
	if (swtch != -1)
		tty.c_cc[VSUSP] = (char) swtch;

	if (ioctl(f, TCSETA, &tty) < 0)
		return(-1);
	
	return(0);
}
