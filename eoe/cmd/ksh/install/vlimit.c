
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/*
 * see if there is a vlimit() function in the library
 *
 */

main()
{
	int i = vlimit(1,-1);
	int j = vlimit(1,i);
	if(j==i)
		printf("#define VLIMIT	1\n");
}
