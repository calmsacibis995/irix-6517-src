
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/*
 * find the maximum number of open files, but no more than MAX
 */

#define MAX 32

main()
{
	int n = 2;
	int m;
	while(1)
	{
		m = dup(n);
		if(m <0 || m>=MAX)
		{
			if(n > 15)
				printf("#define NFILE\t%d\n",n+1);
			else
				printf("/*** Warning, fewer than 15 open files ***/");
			exit(0);
		}
		n = m;
	}
}
