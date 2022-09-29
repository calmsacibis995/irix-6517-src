
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/*
 * Check getgroups
 */

#include	<signal.h>
#ifdef _parm_
#   include	<param.h>
#endif
#ifdef _sysparm_
#   include	<sys/param.h>
#endif


extern int getgroups();
int sigsys();

main()
{
	int groups[100];
	int n;
	signal(SIGSYS,sigsys);
	n = getgroups(0,groups);
	if(n>0)
	{
		printf("#define MULTIGROUPS\t0\n");
		exit(2);
	}
	n = getgroups(100,groups);
	if(n>0)
	{
#ifdef NGROUPS
		printf("#define MULTIGROUPS\t%d\n",NGROUPS);
#endif /* NGROUPS */
		exit(0);
	}
	exit(1);
}

sigsys()
{
	exit(1);
}
