#ident "$Revision: 1.3 $"
/*	Copyright (c) 1993 UNIX System Laboratories, Inc.	*/
/*	(a wholly-owned subsidiary of Novell, Inc.).     	*/
/*	All Rights Reserved.                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF UNIX SYSTEM     	*/
/*	LABORATORIES, INC. (A WHOLLY-OWNED SUBSIDIARY OF NOVELL, INC.).	*/
/*	The copyright notice above does not evidence any actual or     	*/
/*	intended publication of such source code.                      	*/

/*	Modeled after "@(#)libc-port:str/strtoul.c	1.15"		*/
/*LINTLIBRARY*/

#ifdef SIGNED
#pragma weak wcstoll = _wcstoll
#else
#pragma weak wcstoull = _wcstoull
#endif

#include "synonyms.h"
#include <stdlib.h>
#include <ctype.h>
#include <wchar.h>
#include <limits.h>
#include <errno.h>

	/*
	* This source file compiles into two distinct functions:
	*		!SIGNED		SIGNED
	*		wcstoull	wcstoll
	*/
#   define __wc		ch	/* only used for ISSPACE(ch) */
#   ifdef SIGNED
#	define FCNNAME	wcstoll
#	define FCNTYPE	__int64_t
#   else
#	define FCNNAME	wcstoull
#	define FCNTYPE	__uint64_t
#   endif
#   define CHAR		wchar_t
#   define INT		wint_t
#   define ISSPACE(c)	(int)iswspace((wint_t)c)
#   define ISDIGIT(c)	('0' <= (c) && (c) <= '9')

	/*
	* This code must restrict its digits beyond 0-9 to just a-z and A-Z.
	* Thus, the regular isalpha(), as it is locale-dependent, cannot be
	* used.  It instead assumes that a-z and A-Z are contiguous and are
	* in order.  This assumption is not valid for EBCDIC, for example.
	*/
#define ISLOWER(c)	('a' <= (c) && (c) <= 'z')
#define ISUPPER(c)	('A' <= (c) && (c) <= 'Z')

#define DIGIT(x)	(ISDIGIT(x) ? (x) - '0' : \
			ISLOWER(x) ? (x) + 10 - 'a' : (x) + 10 - 'A')
#define MAXBASE	(1 + 'z' - 'a' + 10)

FCNTYPE
FCNNAME(const CHAR *sp, CHAR **ep, int base)
{
	register INT ch;
	FCNTYPE val;			/* accumulates value here */
	int negate;			/* result value to be negated */
	CHAR *nul;			/* used as pocket for null ep */
	FCNTYPE multmax;
	int xx;

	if (ep == 0)
		ep = &nul;
	*ep = (CHAR *)sp;
	/*
	* Skip optional leading white space.
	*/
	ch = *sp;
	while (ISSPACE(ch))
		ch = *++sp;
	/*
	* Note optional leading sign.
	*/
	negate = 0;
	if (ch == '-')
	{
		negate = 1;
		goto skipsign;
	}
	if (ch == '+')
	{
	skipsign:;
		ch = *++sp;
	}
	/*
	* Should be at the initial digit/letter.
	* Verify that the initial character is the start of a number.
	*/
	if (!ISDIGIT(ch))
	{
		if (base <= 1 || base > MAXBASE)
			goto reterr;
		if (ISLOWER(ch))
			ch -= 'a' - 10;
		else if (ISUPPER(ch))
			ch -= 'A' - 10;
		else
			goto reterr;
		if (ch >= base)
			goto reterr;
		goto gotdig;
	}
	/*
	* ch is 0-9 and is the initial digit.  0 is special.
	*/
	if (ch == '0')
	{
		/*
		* 0x can be a prefix or part of a number for certain bases.
		*/
		if ((ch = *++sp) == 'x' || ch == 'X')
		{
			if (base == 0)
				base = 16;
			else if (base <= 1 || base > MAXBASE)
				goto reterr;
			else if (base != 16)
			{
				if ('x' - 'a' + 10 >= base)
				{
				retzero:;
					*ep = (CHAR *)sp;
					val = 0;
					return val;
				}
				ch = 'x' - 'a' + 10;
				goto gotdig;
			}
			/*
			* base is 16.  Only accept 0x as a prefix if it is
			* followed by a valid hexadecimal digit.
			*/
			ch = *++sp;
			if (ISDIGIT(ch))
				ch -= '0';
			else if (ISLOWER(ch))
				ch -= 'a' - 10;
			else if (ISUPPER(ch))
				ch -= 'A' - 10;
			else
			{
			retbackzero:;
				*ep = (CHAR *)sp - 1;
				val = 0;
				return val;
			}
			if (ch >= 16)
				goto retbackzero;
			goto gotdig;
		}
		if (base == 0)
			base = 8;
		else if (base <= 1 || base > MAXBASE)
			goto reterr;
		if (ISDIGIT(ch))
			ch -= '0';
		else if (ISLOWER(ch))
			ch -= 'a' - 10;
		else if (ISUPPER(ch))
			ch -= 'A' - 10;
		else
			goto retzero;
		if (ch >= base)
			goto retzero;
		goto gotdig;
	}
	/*
	* ch is 1-9.
	*/
	ch -= '0';
	if (base == 0)
		base = 10;
	else if (base <= 1 || base > MAXBASE || ch >= base)
		goto reterr;
gotdig:;
	/*
	* ch holds the value of the first (other than 0) digit
	* of a valid number.  sp points to that character.
	* Accumulate digits (and the resulting value) until:
	*  - the character isn't 0-9a-zA-Z, or
	*  - the digit/letter is not valid for the base, or
	*  - the value overflows an unsigned long long or a signed long long.
	*/
#ifdef SIGNED
	multmax = LONGLONG_MAX / base;
#else
	multmax = ULONGLONG_MAX / (unsigned)base;
#endif
	val = (FCNTYPE) DIGIT(ch);
	for (ch = *++sp; isalnum(ch) && (xx = DIGIT(ch)) < base; ) {
		if (val > multmax)
			goto overflow;
		val *= (FCNTYPE) base;
#ifdef SIGNED
		if (ULONGLONG_MAX - val < (FCNTYPE) xx)
			goto overflow;
#else
		if (LONGLONG_MAX - val < (FCNTYPE) xx)
			goto overflow;
#endif
		val += (FCNTYPE) xx;
		ch = *++sp;
	}
	*ep = (CHAR *)sp;
#ifdef SIGNED
	if (negate)
	{
		if (val <= LONGLONG_MAX)
			return -(FCNTYPE)val;
		ep = &nul;
		goto overflow;
	}
	else if (val > LONGLONG_MAX)
	{
		ep = &nul;
		goto overflow;
	}
#else /*!SIGNED*/
	if (negate)
		return -val;
#endif /*SIGNED*/
	return (FCNTYPE)val;
reterr:;
	setoserror(EINVAL);
	return (FCNTYPE)0;
overflow:;
	setoserror(ERANGE);
	if (ep != &nul)	/* still need to locate the end of the number */
	{
		for (;;)
		{
			ch = *++sp;
			if (ISDIGIT(ch))
				ch -= '0';
			else if (ISLOWER(ch))
				ch -= 'a' - 10;
			else if (ISUPPER(ch))
				ch -= 'A' - 10;
			else
				break;
			if (ch >= base)
				break;
		}
		*ep = (CHAR *)sp;
	}
#ifdef SIGNED
	if (negate)
		return LONGLONG_MIN;
	return LONGLONG_MAX;
#else
	return ULONGLONG_MAX;
#endif
}
