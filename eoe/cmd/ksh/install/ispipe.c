
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/*
 * This program prints a define constant that occurs when you seek
 * on a pipe
 */

#include	<errno.h>

extern int errno;
extern long lseek();

main()
{
	int fildes[2];
	if(pipe(fildes) < 0)
		p_exit(0);
	ispipe(fildes[0]);
}

int ispipe(fno)
int fno;
{
	if(lseek(fno,0L,1)>=0)
		p_exit(0);
	if(errno==ESPIPE)
		p_exit(ESPIPE);
	else if(errno==EINVAL)
		p_exit(EINVAL);
	else
		p_exit(0);
}

p_exit(val)
{
	if(val)
		printf("#define PIPE_ERR	%d\n",val);
	exit(!val);
}
