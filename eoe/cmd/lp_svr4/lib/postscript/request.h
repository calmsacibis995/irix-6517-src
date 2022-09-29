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
/* $Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/postscript/RCS/request.h,v 1.1 1992/12/14 13:33:03 suresh Exp $ */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


/*
 *
 * Things used to handle special PostScript requests (like manual feed) globally
 * or on a per page basis. All the translators I've supplied accept the -R option
 * that can be used to insert special PostScript code before the global setup is
 * done, or at the start of named pages. The argument to the -R option is a string
 * that can be "request", "request:page", or "request:page:file". If page isn't
 * given (as in the first form) or if it's 0 in the last two, the request applies
 * to the global environment, otherwise request holds only for the named page.
 * If a file name is given a page number must be supplied, and in that case the
 * request will be looked up in that file.
 *
 */

#define MAXREQUEST	30

typedef struct {

	char	*want;
	int	page;
	char	*file;

} Request;


