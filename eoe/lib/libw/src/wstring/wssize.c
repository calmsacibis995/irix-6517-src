/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wstring/wssize.c	1.1.1.2"

/*
 * Wssize returns number of bytes in EUC for wchar_t string.
 */

#include <widec.h>
#include <sys/euc.h>
#include "pcode.h"

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
_wssize(register wchar_t *s)
{
	register wchar_t wchar;
	register int size;

	for (size = 0;;)	/* don't check for bad sequences */
	{
		switch ((wchar = *s++) & MY_EUCMASK)
		{
		default:
			if (wchar == 0)
				return size;
			size++;
			continue;
		case MY_P11:
			size += eucw1;
			continue;
		case MY_P01:
			size++;
#ifdef _WCHAR16
			if (wchar < 0240 || !multibyte && wchar < 0400)
				break;
#endif
			size += eucw2;
			continue;
		case MY_P10:
			size++;
			size += eucw3;
			continue;
		}
	}
}
