
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/*
 * see whether syscall() exists in C library and works.
 */

#define GETPID	20

main()
{
	int x;
	x = syscall(GETPID);
	if(x == getpid())
	{
		printf("#define SYSCALL	1\n");
		exit(0);
	}
	exit(1);
}
