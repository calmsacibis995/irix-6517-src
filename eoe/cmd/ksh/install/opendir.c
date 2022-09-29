
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

#include	<sys/types.h>
#undef direct
#ifndef FS_3D
#   define direct dirent
#endif /* FS_3D */
#ifdef _dirent_
#   include	<dirent.h>
#else
#   include	<ndir.h>
#endif /* _dirent_ */

/*
 * This routine checks for file named a.out in .
 * only define _ndir_ if ndir is there and opendir works
 */


main()
{
	DIR	 *dirf;
	struct dirent *ep;
	if(dirf=opendir("."))
	{
		while(ep=readdir(dirf))
		{
			if(strcmp("a.out",ep->d_name)==0)
			{
#ifndef _dirent_
				printf("#define _ndir_	1\n");
#endif /* _dirent_ */
				exit(0);
			}
		}
	}
	exit(1);
}

