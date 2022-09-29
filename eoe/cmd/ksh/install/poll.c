
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/*
 * find out whether the poll() system call works
 */

#include	<poll.h>

int main(argc,argv)
char *argv[];
{
	struct pollfd fd;
	long before,after;
	time(&before);
	poll(&fd,0,1500);
	time(&after);
	if(after>before) 
	{
		printf("#define _poll_\t1\n");
		exit(0);
	}
	exit(1);
}
