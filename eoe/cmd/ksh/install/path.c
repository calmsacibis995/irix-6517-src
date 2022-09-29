
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/* prints out define for PATH_MAX from limits.h or sys/param.h */
#ifdef LIM
#   include	<limits.h>
#else
#   include	<sys/param.h>
#endif

main()
{
#ifdef PATH_MAX
	printf("#define PATH_MAX\t%d\n",PATH_MAX);
	exit(0);
#else
	exit(1);
#endif
}

