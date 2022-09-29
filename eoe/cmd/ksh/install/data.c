
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/* This program should fail if string is made readonly */
const char string[300] = "hello";
main(argc,argv)
char *argv[];
{
	exit(foo(argc>1?argv[1]:string));
}

foo(str)
char *str;
{
	str[0]='x';
	return(str[0]=='x'?0:1);
}
