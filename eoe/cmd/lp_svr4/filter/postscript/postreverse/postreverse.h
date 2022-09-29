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
/* $Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/filter/postscript/postreverse/RCS/postreverse.h,v 1.1 1992/12/14 13:20:07 suresh Exp $ */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


/*
 *
 * Definitions used by the page reversal utility program.
 *
 * An array of type Pages is used to keep track of the starting and ending byte
 * offsets for the pages we've been asked to print.
 *
 */

typedef struct {

	long	start;			/* page starts at this byte offset */
	long	stop;			/* and ends here */
	int	empty;			/* dummy page if TRUE */

} Pages;

/*
 *
 * Some of the non-integer functions in postreverse.c.
 *
 */

char	*copystdin();

