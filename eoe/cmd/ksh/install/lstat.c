
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/*
 * make sure that there is an lstat in the library and that
 * it does something sensible
 * This program will only compile if S_IFLNK is defined
 */

#include	<sys/types.h>
#include	<sys/stat.h>

main()
{
	struct stat statb;
	if(lstat(".",&statb) < 0)
		exit(1);
	exit(!S_IFLNK);
}
