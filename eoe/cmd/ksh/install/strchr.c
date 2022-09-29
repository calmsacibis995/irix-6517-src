
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/*
 * make sure that there is a strchr and strrchr in the library and that
 * it works
 */

extern char *strchr();

char foobar[] = "abcdefg";
main()
{
	register char *cp = strchr(foobar,'b');
	if(cp!=(foobar+1))
		exit(1);
	exit(0);
}
