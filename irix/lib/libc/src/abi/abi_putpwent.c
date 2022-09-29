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
#ident	"$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/abi/RCS/abi_putpwent.c,v 1.1 1993/07/21 23:33:20 ajit Exp $"
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	3.0 SID #	1.2	*/
/*LINTLIBRARY*/
/*
 * format a password file entry
 */
#ifdef __STDC__
	#pragma weak putpwent = _putpwent
#endif
#include "synonyms.h"
#include <stdio.h>
#include <pwd.h>

int
putpwent(p, f)
register const struct passwd *p;
register FILE *f;
{
	(void) fprintf(f, "%s:%s", p->pw_name, p->pw_passwd);
	if((*p->pw_age) != '\0')
		(void) fprintf(f, ",%s", p->pw_age);
	(void) fprintf(f, ":%u:%u:%s:%s:%s",
		p->pw_uid,
		p->pw_gid,
		p->pw_gecos,
		p->pw_dir,
		p->pw_shell);
	(void) putc('\n', f);
	(void) fflush(f);
	return(ferror(f));
}
