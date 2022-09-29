/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/ftok.c	1.4.1.5"
#ifdef __STDC__
	#pragma weak ftok = _ftok
#endif
#include "synonyms.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mkdev.h>

key_t
ftok(const char *path, int id)
{
	struct stat64 st;

	/* cast id to a minor_t to follow the rest of the sizes in the 
	   return line and then cast the whole return line to an int to 
	   return the correct value */

	return(stat64(path, &st) < 0 ? (key_t)-1 :
	    (key_t)((minor_t)id << 24 | ((minor(st.st_dev)&0x0fff) << 12) | 
		(st.st_ino&0x0fff)));
}
