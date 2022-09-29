
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/*
 * see if there is a killpg in the library and that
 * it does something reasonable
 */

extern int killpg();

main()
{
	int pid;
	int w;
	int fd[2];
	char buff[2];
	pipe(fd);
	if((pid=fork()) > 0)
	{
		close(fd[1]);
		read(fd[0],buff,1);
		if(killpg(pid,15)>=0)
			exit(0);
		exit(1);
	}
	else if(pid==0)
	{
		setpgrp(0,getpid());
		close(fd[0]);
		write(fd[1],"x",1);
		pause();
	}
	else
		exit(1);
}
