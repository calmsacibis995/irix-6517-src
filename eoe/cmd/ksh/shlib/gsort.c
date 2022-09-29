/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)ksh:shlib/gsort.c	1.2.4.1"

/*
 *  gsort - sort an array of pointers to data
 *
 *   David Korn
 *   AT&T Bell Laboratories
 *   Room 3C-526B
 *   Murray Hill, N. J. 07974
 *   Tel. x7975
 *
 *  Derived from Bourne Shell
 */

/*
 * sort the array of strings argv with n elements
 */

void	gsort(argv,n, fn)
char *argv[];
int (*fn)();
{
	register int 	i, j, m;
	int  k;
	for(j=1; j<=n; j*=2);
	for(m=2*j-1; m/=2;)
	{
		k=n-m;
		for(j=0; j<k; j++)
		{
			for(i=j; i>=0; i-=m)
			{
				register char **ap;
				ap = &argv[i];
				if((*fn)(ap[m],ap[0])>0)
					break;
				else
				{
					char *s;
					s=ap[m];
					ap[m]=ap[0];
					ap[0]=s;
				}
			}
		}
	}
}

