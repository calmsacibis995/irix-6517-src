/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wmisc/scrwidth.c	1.1.1.2"
#include	<widec.h>
#include	"libw.h"
#include	<ctype.h>
#include	"_wchar.h"

#ifdef _WCHAR16
# define MY_EUCMASK	H_EUCMASK
# define MY_P11		H_P11
# define MY_P01		H_P01
# define MY_P10		H_P10
# define MY_SHIFT	8
#else
# define MY_EUCMASK	EUCMASK
# define MY_P11		P11
# define MY_P01		P01
# define MY_P10		P10
# define MY_SHIFT	7
#endif

int
scrwidth(wchar_t c)
{
	switch (c & MY_EUCMASK)	/* no invalid combination checks */
	{
	default:
		if (c >= 0400 || !isprint(c))	/* >= 0400 protects isprint */
			return 0;
		return 1;
	case MY_P11:
		return scrw1;
	case MY_P01:
#ifdef _WCHAR16
		if (c < 0240 || !multibyte && c < 0400)
			return isprint(c) != 0;
#endif
		return scrw2;
	case MY_P10:
		return scrw3;
	}
}
