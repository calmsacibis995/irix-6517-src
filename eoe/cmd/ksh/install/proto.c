
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/*
 * This program should only compile on systems that allow
 * function protocols
 */

extern foo(void);
extern bar(int,char*[],int(*)(void));

main()
{
	char *argv[4];
	foo();
	exit(bar(foo(),argv,foo));
}

foo()
{
	return(1);
}

bar(n,com,fun)
char *com[];
int (*fun)();
{
	foo();
	return(0);
}
