/*	Copyright (c) 1993 UNIX System Laboratories, Inc. 	*/
/*	  All Rights Reserved                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.   	            	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*		copyright	"%c%" 	*/

#ident	"@(#)acct:common/cmd/acct/lib/expand.c	1.7.3.3"
#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/acct/lib/RCS/expand.c,v 1.3 1996/06/14 21:05:46 rdb Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/acct.h>

ulong_t
expand32(comp_t ct)
{
	int  e;
	unsigned long long f;

	e = (ct >> 13) & 07;
	f = ct & 017777;

	while (e-- > 0) 
		f <<=3;

	if (f > 0xffffffff) {
		f = 0xffffffff;
	}

	return (time_t) f;
}

unsigned long long
expand64(comp_t ct)
{
	int e;
	unsigned long long f;

	e = (ct >> 13) & 07;
	f = ct & 017777;

	while (e-- > 0) 
		f <<=3;

	return f;
}

