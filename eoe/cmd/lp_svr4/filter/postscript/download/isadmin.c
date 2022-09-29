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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/filter/postscript/download/RCS/isadmin.c,v 1.1 1992/12/14 13:14:00 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *
 * Administrator check - only for Unix 4.0 lp support. 
 *
 */

#include "downloader.h"

#if (LEVEL & 01) && (LEVEL & 02)
#include <stdio.h>
#include <pwd.h>

#include "gen.h"

#define ADMIN	"lp"

/*****************************************************************************/

isadmin()

{

    int			uid;
    char		*l;
    struct passwd	*p;

    char		*getenv();
    struct passwd	*getpwnam(), *getpwuid();

/*
 *
 * Returns TRUE if the user is root or lp - trivial variation of lp's getname().
 *
 */

    if ( (uid = getuid()) == 0 )
	return(TRUE);

    setpwent();

    if ( (l = getenv("LOGNAME")) == NULL || (p = getpwnam(l)) == NULL || p->pw_uid != uid )
	if ( (p = getpwuid(uid)) != NULL )
	    l = p->pw_name;
	else l = NULL;

    if ( l != NULL && strcmp(l, ADMIN) == 0 )
	return(TRUE);

    return(FALSE);

}   /* End of isadmin */

/*****************************************************************************/

#endif

