
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/*
 * split a file up into lines with alpahnumeric strings
 */

#include	<stdio.h>
#include	<ctype.h>

main(argc,argv)
char *argv[];
{
	register FILE *fd;
	register int c;
	register int mode;
	if(argc < 2)
		exit(1);
	if((fd = fopen(argv[1],"r"))==NULL)
		exit(2);
	while((c=getc(fd))!=EOF)
	{
#ifdef cray
		if(c=='$')
			c = '_';
#endif /* cray */
		if(isalpha(c) || c=='_')
			mode++;
		else if(isdigit(c) && mode)
			mode++;
		else if(mode>1)
		{
			mode = 0;
			c = '\n';
		}
		else
		{
			mode = 0;
			continue;
		}
		putc(c,stdout);
	}
	if(mode)
		putc('\n',stdout);
	exit(0);
}
			
