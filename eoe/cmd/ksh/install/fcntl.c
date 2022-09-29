
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/*
 * This program tests whether fcntl exists on the system and
 * does what the shell requires it to do
 * Also, if R_OK is defined in <fcntl.h>, prints define R_OK_fcntl_h
 */

#ifdef _fcntl_
#  include <fcntl.h>
#else
#  define F_DUPFD	0
#endif

main()
{
	int fn;
	close(6);
	fn =  fcntl(0, F_DUPFD, 6);
	if(fn < 6)
		printf("#define NOFCNTL\t1\n");
#ifdef R_OK
	else
		printf("#define R_OK_fcntl_h\t1\n");
#endif /* R_OK */
}
