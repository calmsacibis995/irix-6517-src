
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

#include	<errno.h>
#include	<sys/types.h>
#include	<signal.h>


extern int errno;
int handler();
int ntry = 0;

main()
{
	int status;
	int pid;
	int nsleep = 4;
	signal(SIGALRM,handler);
	do
	{
		if((pid=fork())==0)
		{
			sleep(nsleep);
			exit(1);
		}
		else if(pid < 0)
			exit(1);
		alarm(2);
		pid = wait(&status);
		if(pid<0 && errno==EINTR)
		{
			printf("#define SIG_NORESTART	1\n");
			exit(0);
		}
	}
	while(ntry==0 && (nsleep+=2) < 12);
	exit(0);
}

int handler(sig)
{
	ntry++;
	signal(SIGALRM,handler);
	alarm(2);
}
