/*	Copyright (c) 1993 UNIX System Laboratories, Inc. 	*/
/*	  All Rights Reserved                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.   	            	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*		copyright	"%c%" 	*/

#ident	"@(#)acct:common/cmd/acct/lib/copyn.c	1.5.3.3"
#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/acct/lib/RCS/copyn.c,v 1.2 1993/11/05 04:25:02 jwag Exp $"
/*
 * Copy n bytes from s2 to s1
 * return s1
 */

char *
copyn(s1, s2, n)
register char *s1, *s2;
{
	register i;
	register char *os1;

	os1 = s1;
	for (i = 0; i < n; i++)
		*s1++ = *s2++;
	return(os1);
}
