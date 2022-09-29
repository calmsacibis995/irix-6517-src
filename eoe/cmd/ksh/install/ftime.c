
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

#include	<sys/types.h>
#include	<sys/timeb.h>
#include	<signal.h>

/*
 * only define _sys_timeb_ if ftime is there and works
 */

int sigsys();

main()
{
	struct timeb tb,ta;
	signal(SIGSYS,sigsys);
	ftime(&tb);
	sleep(2);
	ftime(&ta);
	if((ta.time-tb.time)>=1)
	{
		printf("#define _sys_timeb_	1\n");
		exit(0);
	}
	exit(1);
}

sigsys()
{
	exit(1);
}
