
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/*
 * see whether there is a select() that can be used for fine timing
 * This defines _SELECT4_ or _SELECT5_ if select() exists and delays
 */

#include	<sys/types.h>
#include	<sys/time.h>

main()
{
	time_t t1,t2;
#ifdef S4
	int milli = 2000;
#else
	struct timeval timeloc;
	timeloc.tv_sec = 2;
	timeloc.tv_usec = 0;
#endif /* S4 */
	time(&t1);
#ifdef S4
	select(0,(fd_set*)0,(fd_set*)0,milli);
#else
	select(0,(fd_set*)0,(fd_set*)0,(fd_set*)0,&timeloc);
#endif /* S4 */
	time(&t2);
	if(t2 > t1)
	{
#ifdef S4
		printf("#define _SELECT4_	1\n");
#else
		printf("#define _SELECT5_	1\n");
#endif /* S4 */
		exit(0);
	}
	exit(1);
}
