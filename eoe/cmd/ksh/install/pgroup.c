
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/*
 * Check setpgrp() and getpgrp()
 */

extern int getpgrp();
extern int setpgrp();

main()
{
	int pid,pgrp;
	int status;
	pid = getpid();
	pgrp = getpgrp(pid);
	if(pid==pgrp)
		pid = fork();
	else
		pid = 0;
	if(pid < 0)
	{
		printf("/* fork failed may need to set [gs]etpgrp */");
		exit(0);
	}
	if(pid >0)
	{
		wait(&status);
		if(status&0xff)
			exit((status&0xff)+0400);
		exit(status>>8);
	}
	setpgrp(0,pid);
	setpgrp(0,pgrp);
	if(getpgrp(0)==pgrp)
	{
#ifdef setpgrp
#   ifdef getpgid
		if(getpgrp(-1)==pgrp)
			printf("#define getpgid(a)	getpgrp()\n");
		else
			printf("#define getpgid(a)	getpgrp(a)\n");
#   endif
		exit(0);
#else
		printf("#define getpgid(a)	getpgrp(a)\n");
		printf("#define setpgid(a,b)	setpgrp(a,b)\n");
#endif
	}
	else
	{
#ifdef setpgrp
		exit(1);
#else
		printf("#define getpgid(a)	getpgrp()\n");
		printf("#define setpgid(a,b)	((a)?0:setpgrp())\n");
#endif
	}
	exit(0);
}
