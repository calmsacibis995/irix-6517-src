
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/*
 * see if getlimit actually works
 *
 */

#include	<sys/time.h>
#include	<sys/resource.h>

main()
{
	struct rlimit rp;
	exit(getrlimit(RLIMIT_CPU,&rp));
}
