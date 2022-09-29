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
/*
 * Description: quad precision version of strtod
 */
#ident "$Id: strtold.c,v 1.4 1996/07/25 17:35:50 vegas Exp $"

#ifdef __STDC__
	#pragma weak strtold = _strtold
#endif

#include "synonyms.h"
#include <ctype.h>
#include <values.h>
#include <errno.h>
#include <math.h>
#include "_locale.h"
#include "math_extern.h"
#include <inttypes.h>
#include "quad.h"

#define MAXDIGITS 34

#define STORE_PTR	(*ptr = (char *)s)
#define DEC_PTR		(*ptr = (char *)(s - 1))

/* ====================================================================
 *
 * FunctionName: strtold
 *
 * Description: converts a character representation of arg to long double
 *
 * ====================================================================
 */

long double
strtold(const char *s, char **ptr)
{
    register unsigned c;
    register unsigned negate, decimal_point;
    register char *d;
    register int exp;
    ldquad x;
    register int dpchar;
    char digits[MAXDIGITS];

    int do_store = 0;
    char *dummy;

    if (ptr == (char **)0) 
	ptr = &dummy;	/* harmless dumping ground */
    STORE_PTR;

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
		/* ignore more than 17 digits, but adjust exponent */
		exp += (decimal_point ^ 1);
	    }
	    else {
		if (c == 0 && d == digits) {
		    /* ignore leading zeros */

		    do_store++;	/* make sure we store the pointer if val == 0 */
		}
		else {
		    *d++ = (char) c;
		}
		exp -= decimal_point;
	    }
	}
	else if (c == (unsigned int) dpchar && !decimal_point) {	/* INTERNATIONAL */
	    decimal_point = 1;
	}
	else {
	    break;
	}
	c = *s++;
    }

    if ((d != digits)||(do_store)) {
	/* We must store the decremented pointer in case there isnt 
	   a legitimate exp, IF we have found any digits or leading zeroes.
	*/
        DEC_PTR;
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
	            e = e * 10 + (int)c;
	        c = *s++;
	    }
	    while (c -= '0', c < 10);
	    if (negate_exp) {
	        e = -e;
	    }
            if (e < -340 || e > 340) 
		exp = e;
	    else 
		exp += e;
	    DEC_PTR;
	}
    }

    /* atof already returned, but strtod 
       needed to find the end of the exponent */

    if (d == digits) {
	return 0.0L;
    }

    if (exp < -357) {
	x.ld = 0.0L;

	setoserror(ERANGE);
    }
    else if (exp > 308) {
	x.q.hi = HUGE_VAL;
	x.q.lo = 0.0;

	setoserror(ERANGE);
    }
    else {
	/* let _atoq diagnose under- and over-flows */
	/* if the input was == 0.0, we have already returned,
	   so retval of +-Inf signals OVERFLOW, 0.0 UNDERFLOW
	*/

	x.ld = _atoq (digits, (int)(d - digits), exp);

	if ((x.ld == 0.0L) || (x.q.hi == HUGE_VAL)) setoserror(ERANGE);
    }

    if (negate) {
	x.ld = -x.ld;
    }

    return x.ld;
}
