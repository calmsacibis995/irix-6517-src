/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	Copyright (c) 1993 UNIX System Laboratories, Inc.	*/
/*	(a wholly-owned subsidiary of Novell, Inc.).     	*/
/*	All Rights Reserved.                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#if 0 && ! _PIC

#ident	"@(#)libc-port:gen/wcstombs.c	1.7"
#ifdef __STDC__
	#pragma weak wcstombs = _wcstombs
#endif

/*LINTLIBRARY*/

#include "synonyms.h"
#include "wchar.h"
#include "_wchar.h"
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <errno.h>

#define EUCDOWN 28
#define DOWNMSK 0xf     /* (EUCMASK >> EUCDOWN) without the warning */
#define DOWNP00 (P00 >> EUCDOWN)
#define DOWNP11 (P11 >> EUCDOWN)
#define DOWNP01 (P01 >> EUCDOWN)
#define DOWNP10 (P10 >> EUCDOWN)

#define SS2     0x8e    /* byte that prefixes code set 2 multibyte encoding */
#define SS3     0x8f    /* byte that prefixes code set 3 multibyte encoding */

	/*
	 * Restartable wide string to multibyte string conversion.
	 * Returns:
	 *   (size_t)-1	Initial wide character is invalid.
	 *   *		Otherwise, the number of bytes written,
	 *		which can be zero if not enough room.
	 * On return, *src is updated:
	 *   0	If the end of the input wide string is reached.
	 *   *	Otherwise, the next wide character to process,
	 *	or the bad code for a (size_t)-1 return.
	 */

static size_t
_xwcstombs(char *dst, const wchar_t **src, size_t len, int *xerp)
{
	register unsigned char *dp;
	register wchar_t wc;
	register int n, cs1;
	register const wchar_t *sp;
	size_t origlen;
	unsigned char buf[MB_LEN_MAX];

	if ((dp = (unsigned char *)dst) == 0)
	{
		len = ~(size_t)0;	/* unlimited */
		dp = &buf[0];		/* place to drop bytes */
	}
	cs1 = 0;
	origlen = len;
	sp = *src;
	for (wc = *sp;; wc = *++sp)
	{
		switch ((wc >> EUCDOWN) & DOWNMSK)
		{
		default:
		bad:;
			setoserror(EILSEQ);
			*xerp = EILSEQ;
			if (*src == sp)
				return (size_t)-1;
			*src = sp;
			return origlen - len;
		case DOWNP00:
			if ((wc & ~0xff) != 0
				|| wc >= 0xa0 && multibyte && eucw1 != 0)
			{
				goto bad;
			}
			if (wc == 0)	/* reached end-of-string */
			{
				*src = 0;
				return origlen - len;
			}
			if (len == 0)
			{
			tooshort:;
				*src = sp;
				return origlen - len;
			}
			if (dst != 0)
				*dp++ = (unsigned char)wc;
			len--;
			continue;
		case DOWNP11:
			if ((n = eucw1) > len)
				goto tooshort;
			cs1 = 1;
			break;
		case DOWNP01:
			if ((n = eucw2) >= len)
				goto tooshort;
			*dp++ = SS2;
			break;
		case DOWNP10:
			if ((n = eucw3) >= len)
				goto tooshort;
			*dp++ = SS3;
			break;
		}
		switch (n)	/* fill in backwards */
		{
#if MB_LEN_MAX > 5
			int i;

		default:
			i = n;
			dp += n;
			do
			{
#if UCHAR_MAX == 0xff
				*--dp = wc | 0x80;
#else
				*--dp = (wc | 0x80) & 0xff;
#endif
				wc >>= 7;
			} while (--n != 0);
			n = i;
			break;
#endif /*MB_LEN_MAX > 5*/
		case 0:
			goto bad;
		case 4:
#if UCHAR_MAX == 0xff
			dp[3] = (unsigned char)(wc | 0x80);
#else
			dp[3] = (wc | 0x80) & 0xff;
#endif
			wc >>= 7;
			/*FALLTHROUGH*/
		case 3:
#if UCHAR_MAX == 0xff
			dp[2] = (unsigned char)(wc | 0x80);
#else
			dp[2] = (wc | 0x80) & 0xff;
#endif
			wc >>= 7;
			/*FALLTHROUGH*/
		case 2:
#if UCHAR_MAX == 0xff
			dp[1] = (unsigned char)(wc | 0x80);
#else
			dp[1] = (wc | 0x80) & 0xff;
#endif
			wc >>= 7;
			/*FALLTHROUGH*/
		case 1:
#if UCHAR_MAX == 0xff
			dp[0] = (unsigned char)(wc | 0x80);
#else
			dp[0] = (wc | 0x80) & 0xff;
#endif
			break;
		}
		if (!cs1)
		{
			if (dst == 0)
				dp = &buf[0];
			else
				dp += n;
			n++;
		}
		else
		{
			cs1 = 0;
			if (*dp < 0xa0)	/* C1 cannot be first byte */
				goto bad;
			if (dst != 0)
				dp += n;
		}
		len -= n;
	}
}

size_t
wcstombs(char *s, const wchar_t *pwcs, size_t n)
{
	int	val = 0;
	int	total = 0;
	char	temp[MB_LEN_MAX];
	int	i, xerr;

	size_t	sz;

	/*
	 *  XPG4 feature:
	 *
	 *	If 's' is a NULL pointer, 'wcstombs()' returns the length
	 *	required to convert the entire array regardless of the value
	 *	of 'n', but no values are stored.
	 */
	if (s == 0) {
		xerr = 0;
		while ((sz = _xwcstombs(s, &pwcs, n, &xerr)) != 0)
		{
			if (sz == (size_t)-1)
				return (size_t)-1;
			total += sz;
			if (pwcs == 0 || xerr == 0)
				break;
			s += sz;
			n -= sz;
		}
		if (pwcs == 0 && s != 0 && sz != n) /* reached end-of-string */
			s[sz] = '\0';
		return(total);
	}
	for (;;) {
		if (*pwcs == 0) {
			if( (size_t)total < n )
				*s = '\0';
			break;
		}
		if ((val = wctomb(temp, *pwcs++)) == -1) {
			setoserror(EILSEQ);
			return((size_t)val);
		}
		if ((size_t)(total += val) > n) {
			total -= val;
			break;
		}
		for (i=0; i<val; i++)
			*s++ = temp[i];
	}
	return((size_t)total);
}

#endif /* ! _PIC */
