/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
/* $Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/filter/postscript/postio/RCS/ifdef.h,v 1.1 1992/12/14 13:18:15 suresh Exp $ */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *
 * Conditional compilation definitions needed in ifdef.c and postio.c.
 *
 */


#ifdef SYSV
#include <termio.h>
#endif


#ifdef V9
#include <sys/filio.h>
#include <sys/ttyio.h>

extern int	tty_ld;
#endif


#ifdef BSD4_2
#include <sgtty.h>
#include <sys/time.h>
#include <errno.h>

#define FD_ZERO(s) (s) = 0
#define FD_SET(n,s) (s) |= 1 << (n)

extern int	errno;
#endif


#ifdef DKHOST
#include <dk.h>
#include <sysexits.h>

extern char	*dtnamer();
extern int	dkminor();
#endif


/*
 *
 * External variable declarations - most (if not all) are defined in postio.c and
 * needed by the routines in ifdef.c.
 *
 */


extern char	*line;			/* printer is on this line */
extern int	ttyi;			/* input */
extern int	ttyo;			/* and output file descriptors */
extern FILE	*fp_log;		/* just for DKHOST stuff */

extern char	mesg[];			/* exactly what came back on ttyi */
extern char	*endmesg;		/* one in front of last free slot in mesg */
extern int	next;			/* next character goes in mesg[next] */

extern short	baudrate;		/* printer is running at this speed */
extern int	stopbits;		/* and expects this many stop bits */
extern int	interactive;		/* TRUE for interactive mode */

extern int	whatami;		/* a READ or WRITE process - or both */
extern int	canread;		/* allows reads */
extern int	canwrite;		/* and writes if TRUE */

