/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)ksh:shlib/cannon.c	1.3.6.1"

/*
 *  pathcanon - Generate canonical pathname from given pathname.
 *  This routine works with both relative and absolute paths.
 *  Relative paths can contain any number of leading ../ .
 *  Each pathname is checked for access() before each .. is applied and
 *     NULL is returned if not accessible
 *  A pointer to the end of the pathname is returned when successful
 *  The operator ... is also expanded by this routine when LIB_3D is defined
 *  In this case length of final path may be larger than orignal length
 *
 *   David Korn
 *   AT&T Bell Laboratories
 *   Room 3C-526B
 *   Murray Hill, N. J. 07974
 *   Tel. x7975
 *
 */

#include	"io.h"

char	*pathcanon(path)
char *path;
{
	register char *dp=path;
	register int c = '/';
	register char *sp;
	register char *begin=dp;
#ifdef LIB_3D
	extern char *pathnext();
#endif /* LIB_3D */
#ifdef PDU
	/* Take out special case for /../ as */
	/* Portable Distributed Unix allows it */
	if ((*dp == '/') && (*++dp == '.') &&
	    (*++dp == '.') && (*++dp == '/') &&
	    (*++dp != 0))
		begin = dp = path + 3;
	else
		dp = path;
#endif /* PDU */

	if(*dp != '/')
		dp--;
	sp = dp;
	while(1)
	{
		sp++;
		if(c=='/')
		{
#ifdef apollo
			if(*sp == '.')
#else
			if(*sp == '/')
				/* eliminate redundant / */
				continue;
			else if(*sp == '.')
#endif /* apollo */
			{
				c = *++sp;
				if(c == '/')
					continue;
				if(c==0)
					break;
				if(c== '.')
				{
					if((c= *++sp) && c!='/')
					{
#ifdef LIB_3D
						if(c=='.')
						{
							char *savedp;
							int savec;
							if((c= *++sp) && c!='/')
								goto dotdotdot;
							/* handle ... */
							savec = *dp;
							*dp = 0;
							savedp = dp;
							dp = pathnext(path,sp);
							if(dp)
							{
								*dp = savec;
								sp = dp;
								if(c==0)
									break;
								continue;
							}
							dp = savedp;
							*dp = savec;
						dotdotdot:
							*++dp = '.';
						}
#endif /* LIB_3D */
					dotdot:
						*++dp = '.';
					}
					else /* .. */
					{
						if(dp>begin)
						{
							*dp = 0;
							if(sh_access(path,X_OK) < 0)
								return((char*)0);
							while(*--dp!='/')
								if(dp<begin)
									break;
						}
						else if(dp < begin)
						{
							begin += 3;
							goto dotdot;
						}
						if(c==0)
							break;
						continue;
					}
				}
				*++dp = '.';
			}
		}
		if((c= *sp)==0)
			break;
		*++dp = c;
	}
#ifdef LIB_3D
	*++dp= 0;
#else
	/* remove all trailing '/' */
	if(*dp!='/' || dp<=path)
		dp++;
	*dp= 0;
#endif /* LIB_3D */
	return(dp);
}
