/**************************************************************************/
/*									  */
/* 		 Copyright (C) 1989, Silicon Graphics, Inc.		  */
/*									  */
/*  These coded instructions, statements, and computer programs  contain  */
/*  unpublished  proprietary  information of Silicon Graphics, Inc., and  */
/*  are protected by Federal copyright law.  They  may  not be disclosed  */
/*  to  third  parties  or copied or duplicated in any form, in whole or  */
/*  in part, without the prior written consent of Silicon Graphics, Inc.  */
/*									  */
/**************************************************************************/
/* This is loosely based on copyrighted strtol.c:		*/
/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/strtoll.c	2.15"
/*LINTLIBRARY*/
#ifdef __STDC__
	#pragma weak strtoll = _strtoll
	#pragma weak strtoull = _strtoull
	#pragma weak atoll = _atoll
#endif
#include "synonyms.h"
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>

#define DIGIT(x)	(isdigit(x) ? (x) - '0' : \
			islower(x) ? (x) + 10 - 'a' : (x) + 10 - 'A')
#define MAXBASE	('z' - 'a' + 1 + 10)

/* common routine used by strtoll and strtoull */
static __uint64_t
__string_to_ull (const char *str, char **nptr, int base, int *isneg)
{
	unsigned long long val;
	int c;
	int xx;
	unsigned long long	multmax;
	const char 	**ptr = (const char **)nptr;
	int neg = 0;

	if (ptr != (const char **)0)
		*ptr = str; /* in case no number is formed */
	if (base < 0 || base > MAXBASE || base == 1) {
		setoserror(EINVAL);
		return (0); /* base is invalid -- should be a fatal error */
	}
	if (!isalnum(c = *str)) {
		while (isspace(c))
			c = *++str;
		switch (c) {
		case '-':
			neg++;
			/* FALLTHROUGH */
		case '+':
			c = *++str;
		}
	}
	*isneg = neg;
	if (base == 0)
		if (c != '0')
			base = 10;
		else if (str[1] == 'x' || str[1] == 'X')
			base = 16;
		else
			base = 8;
	/*
	 * for any base > 10, the digits incrementally following
	 *	9 are assumed to be "abc...z" or "ABC...Z"
	 */
	if (!isalnum(c) || (xx = DIGIT(c)) >= base)
		return (0); /* no number formed */
	if (base == 16 && c == '0' && (str[1] == 'x' || str[1] == 'X') &&
		isxdigit(str[2]))
		c = *(str += 2); /* skip over leading "0x" or "0X" */

	multmax = ULONGLONG_MAX / (unsigned)base;
	val = (unsigned long long) DIGIT(c);
	for (c = *++str; isalnum(c) && (xx = DIGIT(c)) < base; ) {
		if (val > multmax)
			goto overflow;
		val *= (unsigned long long) base;
		if (ULONGLONG_MAX - val < (unsigned long long) xx)
			goto overflow;
		val += (unsigned long long) xx;
		c = *++str;
	}
	if (ptr != (const char **)0)
		*ptr = str;
	return val;

overflow:
	for (c = *++str; isalnum(c) && (xx = DIGIT(c)) < base; (c = *++str));
	if (ptr != (const char **)0)
		*ptr = str;
	setoserror(ERANGE);
	val = ULONGLONG_MAX;
	return val;
}

/* ANSI 4.10.1.5 */
extern __int64_t
strtoll(const char *str, char **nptr, int base)
{

/* assume that abs(LONGLONG_MIN) == LONGLONG_MAX+1 */
#define	ABS_LONGLONG_MIN	(((unsigned long long) LONGLONG_MAX) + 1)

	unsigned long long val;
	int isneg;
	int old_errno = errno;
	setoserror(0);
	val = __string_to_ull (str, nptr, base, &isneg);
	if (errno == ERANGE && val == ULONGLONG_MAX) {
		/* overflow of ULONGLONG_MAX */
		return((long long)(isneg ? LONGLONG_MIN : LONGLONG_MAX));
	} else if (isneg ? (val > ABS_LONGLONG_MIN) : (val > LONGLONG_MAX)) {
		/* overflow of LONGLONG_MIN/MAX */
		setoserror(ERANGE);
		return(isneg ? (long long)LONGLONG_MIN : (long long)LONGLONG_MAX);
	} else {
		setoserror(old_errno);
		return ((long long)(isneg ? -val : val));
	}
}

/* ANSI 4.10.1.6 */
extern __uint64_t
strtoull(const char *str, char **nptr, int base)
{
	unsigned long long val;
	int isneg;
	int old_errno = errno;
	setoserror(0);
	val = __string_to_ull (str, nptr, base, &isneg);
	if (errno != ERANGE) {
		/* no overflow */
		setoserror(old_errno);
		return (isneg ? -val : val);
	}
	return val;
}

/* ANSI 4.10.1.3 */
extern __int64_t
atoll(const char *p)
{
	return strtoll (p, (char **) NULL, 10);
}

