
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/*
 * This program sets the VFORK options if appropriate
 * It compare the time run fork/exec against the time to run vfork/exec
 */

#define MAX 50

char dummy[10000];

#include <stdio.h>

main()
{
	long t1, t2, t3;
	int delta;
	int n,p;
	time(&t1);
	n = MAX;
	while(n--)
	{
		if((p=fork())==0)
		{
			execl("/bin/sh","sh", "-c", ":", 0);
			exit(0);
		}
		if(p>0)
			wait(&p);
	}
	time(&t2);
	n = MAX;
	while(n--)
	{
		if((p=vfork(1))==0)
		{
			execl("/bin/sh","sh", "-c", ":", 0);
			exit(0);
		}
		if(p>0)
			wait(&p);
	}
	time(&t3);
	delta = t3-t2;
	if(delta==0)
		delta = 1;
	n = (t2-t1)/delta;
	exit(n<5);
}
	
