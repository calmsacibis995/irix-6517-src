/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/putpwent.c	1.10"
/*	3.0 SID #	1.2	*/
/*LINTLIBRARY*/
/*
 * format a password file entry
 */
#ifdef __STDC__
	#pragma weak putpwent = __sgi_putpwent
#endif
#include "synonyms.h"
#include <stdio.h>
#include <pwd.h>

int
putpwent(p, f)
register const struct passwd *p;
register FILE *f;
{
	if (!p)
		return(-1);
	(void) fprintf(f, "%s:%s", p->pw_name, p->pw_passwd);
	if(p->pw_age && ((*p->pw_age) != '\0'))
		(void) fprintf(f, ",%s", p->pw_age);
	(void) fprintf(f, ":%d:%d:%s:%s:%s",
		/* pass thru -2 for NFS */
		((p->pw_uid == (uid_t)-2) ? -2 : p->pw_uid),
		((p->pw_gid == (gid_t)-2) ? -2 : p->pw_gid),
		p->pw_gecos,
		p->pw_dir,
		p->pw_shell);
	(void) putc('\n', f);
	(void) fflush(f);
	return(ferror(f));
}

