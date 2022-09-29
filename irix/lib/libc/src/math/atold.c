/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
#ident "$Id: atold.c,v 1.5 1994/10/18 04:14:11 jwag Exp $"

#ifdef __STDC__
	#pragma weak atold = _atold
#endif

/*LINTLIBRARY*/
/*
 *	C library - ascii to long double (atold).
 *
 */

#include "synonyms.h"
#include <ctype.h>
#include <values.h>
#include <errno.h>
#include <math.h>
#include <locale.h>
#include "_locale.h"
#include "math_extern.h"

#define MAXDIGITS 34

long double
atold(const char *s)
{
    register unsigned c;
    register unsigned negate, decimal_point;
    register char *d;
    register int exp;
    long double x;
    register int dpchar;
    char digits[MAXDIGITS];

    while (c = *s++, isspace(c));

    /* process sign */
    negate = 0;
    if (c == '+') {
	c = *s++;
    }
    else if (c == '-') {
	negate = 1;
	c = *s++;
    }
    d = digits;
    dpchar = _numeric[0] - '0';
    decimal_point = 0;
    exp = 0;
    for (;;) {
	c -= '0';
	if (c < 10) {
	    if (d == digits+MAXDIGITS) {
		/* ignore more than 34 digits, but adjust exponent */
		exp += (decimal_point ^ 1);
	    }
	    else {
		if (c == 0 && d == digits) {
		    /* ignore leading zeros */
			;
		}
		else {
		    *d++ = c;
		}
		exp -= decimal_point;
	    }
	}
	else if (c == dpchar && !decimal_point) {    /* INTERNATIONAL */
	    decimal_point = 1;
	}
	else {
	    break;
	}
	c = *s++;
    }
    if (d == digits) {
        return 0.0L;
    }
    if (c == 'e'-'0' || c == 'E'-'0') {
	register unsigned negate_exp = 0;
	register int e = 0;
	c = *s++;
	if (c == '+' || c == ' ') {
	    c = *s++;
	}
	else if (c == '-') {
	    negate_exp = 1;
	    c = *s++;
	}
	if (c -= '0', c < 10) {
	    do {
	        if (e <= 340) 
	            e = e * 10 + c;
		else break;
	        c = *s++;
	    }
	    while (c -= '0', c < 10);
	    if (negate_exp) {
	        e = -e;
	    }
            if (e < -(323+MAXDIGITS) || e > 308) 
		exp = e;
	    else 
		exp += e;
	}
    }

    if (exp < -(324+MAXDIGITS)) {
	x = 0;
    }
    else if (exp > 308) {
        *((double *) &x) = HUGE_VAL;
	*((double *) ((&x) + 1)) = 0.0;
    }
    else {
	/* let _atoq diagnose under- and over-flows */
	/* if the input was == 0.0, we have already returned,
	   so retval of +-Inf signals OVERFLOW, 0.0 UNDERFLOW
	*/

	x = _atoq (digits, (int)(d - digits), exp);
    }
    if (negate) {
	x = -x;
    }
    return x;
}
