/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/putp.c	1.10"
/*
 * Handy functions to put out a string with padding.
 * These make two assumptions:
 *	(1) Output is via stdio to stdout through putchar.
 *	(2) There is no count of affected lines.  Thus, this
 *	    routine is only valid for certain capabilities,
 *	    i.e. those that don't have *'s in the documentation.
 */
#include	"curses_inc.h"

/*
 * Routine to act like putchar for passing to tputs.
 * _outchar should really be a void since it's used by tputs
 * and tputs doesn't look at return code.  However, tputs also has the function
 * pointer declared as returning an int so we didn't change it.
 */
int
#ifdef __STDC__
_outchar(int ch)
#else
_outchar(ch)
char	ch;
#endif
{
    return (putchar(ch));
}

/* Handy way to output a string. */
int
putp(char *str)
{
    return (tputs(str, 1, _outchar));
}

/* Handy way to output video attributes. */

int
vidattr(chtype newmode)
{
    return (vidputs(newmode, _outchar));
}
