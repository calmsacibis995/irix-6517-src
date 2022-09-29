/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident "$Revision: 1.6 $"

#include	<unistd.h>
#include	<errno.h>
#include	<malloc.h>
#include	<string.h>
#include	<sys/types.h>
#include	<sys/stat.h>

extern int	errno;

/* copy str into data space -- caller should report errors. */

char *
strsave(str)
register char *str;
{
	register char *rval;

	rval = malloc(strlen(str) + 1);
	if (rval != 0)
		strcpy(rval, str);
	return(rval);
}

/*	Determine if the effective user id has the appropriate permission
	on a file.  Modeled after access(2).
	amode:
		00	just checks for file existence.
		04	checks read permission.
		02	checks write permission.
		01	checks execute/search permission.
		other bits are ignored quietly.
*/


int
eaccess( path, amode )
char		*path;
register int	amode;
{
	struct stat	s;

	if( stat( path, &s ) == -1 )
		return  -1;
	amode &= 07;
	if( !amode )
		return  0;		/* file exists */

	if( (amode & s.st_mode) == amode )
		return  0;		/* access permitted by "other" mode */

	amode <<= 3;
	if( getegid() == s.st_gid  &&  (amode & s.st_mode) == amode )
		return  0;		/* access permitted by group mode */

	amode <<= 3;
	if( geteuid() == s.st_uid  &&  (amode & s.st_mode) == amode )
		return  0;		/* access permitted by owner mode */

	errno = EACCES;
	return  -1;
}
