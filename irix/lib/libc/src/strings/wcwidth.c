#ident "$Revision: 1.4 $"
/*	Copyright (c) 1993 UNIX System Laboratories, Inc.	*/
/*	(a wholly-owned subsidiary of Novell, Inc.).     	*/
/*	All Rights Reserved.                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF UNIX SYSTEM     	*/
/*	LABORATORIES, INC. (A WHOLLY-OWNED SUBSIDIARY OF NOVELL, INC.).	*/
/*	The copyright notice above does not evidence any actual or     	*/
/*	intended publication of such source code.                      	*/

/*	"@(#)libc-port:str/wcwidth.c	1.2"				*/

#ifdef __STDC__
	#pragma weak wcwidth = _wcwidth
	#pragma weak wcswidth = _wcswidth
#endif
#include	"synonyms.h"
#include	<wchar.h>
#include	<sys/euc.h>
#include	"pcode.h"

# define MY_EUCMASK	EUCMASK
# define MY_P11		P11
# define MY_P01		P01
# define MY_P10		P10
# define MY_SHIFT	7

/*
 * wcwidth()
 *
 * return the number of column positions for a wide character
 *
 */

int
wcwidth(wchar_t c)
{
	if (c == 0)	/* null wide character code */
		return(0);
	if (!iswprint(c)) /* non-printing wide character code */
		return -1;

	switch (c & MY_EUCMASK)	
	{
	case MY_P11:
		return eucw1;
	case MY_P01:
		return eucw2;
	case MY_P10:
		return eucw3;
	default:
		if (c >= 0400 )	
			return -1;
		return 1;
	}
}


/*
 * wcswidth()
 *
 * Return the number of columns needed to represent n wide characters (or
 * fewer if a null wide-character code is encountered before n wide chars
 * are encountered)
 *
 */
int
wcswidth(const wchar_t *pwcs, size_t n) 
{
size_t len=0;
int r;
size_t numw;

  for(numw=0; pwcs[numw] != NULL && numw < n; numw++)
	if ((r=wcwidth((wint_t) pwcs[numw])) == (size_t) -1)
		return -1;
	else
		len += r;

  return (int)len;
}
