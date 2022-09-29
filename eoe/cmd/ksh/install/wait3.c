
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/*
 * This  program tests to see if there is a wait3 system call
 */
#ifdef waitpid
#   undef waitpid
#   define wait3(a,b,c)	waitpid(pid,a,b)
#endif

main()
{
	int pid;
	int w;
	if((pid=fork()) > 0)
	{
		if(wait3(&w,0,0)!=pid)
			exit(2);
		exit(0);
	}
	exit(1);
}
