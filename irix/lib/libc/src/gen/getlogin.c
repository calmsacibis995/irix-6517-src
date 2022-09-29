/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/getlogin.c	1.17"
/*LINTLIBRARY*/
#ifdef __STDC__
	#pragma weak getlogin = _getlogin
#ifndef  _LIBC_ABI
	#pragma weak getlogin_r = _getlogin_r
#endif /* _LIBC_ABI */
#endif
#include "synonyms.h"
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>		/* for prototyping */
#include <fcntl.h>		/* for prototyping */
#include "utmp.h"
#include <unistd.h>

int
getlogin_r(char *name, size_t len)
{
	register int uf;
        register char *tp, *p;
	struct utmp ubuf ;

	if ((sizeof(ubuf.ut_user)+1) > len)
		return(ERANGE);

        if((tp=ttyname(0)) == NULL && (tp=ttyname(1)) == NULL &&
                                        (tp=ttyname(2)) == NULL)
                return(ENOTTY);

        if((p=strrchr(tp, '/')) == NULL)
                p = tp;
        else
                p++;

	if((uf = open((const char *)UTMP_FILE, 0)) < 0)
		return(ENOENT);

	while(read(uf, (char*)&ubuf, sizeof(ubuf)) == (int)(sizeof(ubuf))) {
		if(    (ubuf.ut_type == INIT_PROCESS ||
			ubuf.ut_type == LOGIN_PROCESS ||
			ubuf.ut_type == USER_PROCESS ||
			ubuf.ut_type == DEAD_PROCESS ) &&
                        strncmp(p, ubuf.ut_line, sizeof(ubuf.ut_line)) == 0){

			(void) close(uf);
			goto found;
		}
	}
	(void) close(uf);
	return (ENOENT);

found:
	if(ubuf.ut_user[0] == '\0')
		return(ENOENT);
	strncpy(name, &ubuf.ut_user[0], sizeof(ubuf.ut_user)) ;
	name[len-1] = '\0' ;
	return(0);
}

/* move out of function scope so we get a global symbol for use with data cording */
static char answer[8+1]; /* sizeof ut_user+1 */

char *
getlogin(void)
{

	if (getlogin_r(answer, sizeof(answer)))
		return NULL;
	return answer;
}
