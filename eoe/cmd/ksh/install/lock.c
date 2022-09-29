
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

#include	<sys/file.h>

int main(argc,argv)
char *argv[];
{
	register int fd;
	register int r;
	if(( fd = open(argv[1],0)) < 0)
		exit(1);
	if(flock(fd,LOCK_NB|LOCK_EX)!=0)
		exit(2);
	if(( fd = open(argv[1],0)) < 0)
		exit(3);
	r = flock(fd,LOCK_NB|LOCK_EX);
	return(r==0);
}
