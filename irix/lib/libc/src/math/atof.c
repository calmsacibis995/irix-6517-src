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
#ident	"$Id: atof.c,v 1.18 1996/07/24 00:12:15 vegas Exp $"

/*LINTLIBRARY*/
/*
 *	C library - ascii to floating (atof) and string to double (strtod)
 *
 *	This version compiles both atof and strtod depending on the value
 *	of STRTOD, which is set in the file and may be overridden on the
 *	"cc" command line.  The only difference is the storage of a pointer
 *	to the character which terminated the conversion.
 */
#ifndef STRTOD
#define STRTOD  0
#endif
#include "synonyms.h"
#include "shlib.h"
#include <ctype.h>
#include <values.h>
#include <errno.h>
#include <math.h>
#include "_locale.h"
#include "math_extern.h"

#define MAXDIGITS 17

#if STRTOD
#define STORE_PTR	(*ptr = (char *)s)
#define DEC_PTR		(*ptr = (char *)(s - 1))
#else
#define STORE_PTR
#define DEC_PTR
#endif



#if STRTOD
double strtod(const char *s, char **ptr)
#else
double atof(const char *s)
#endif
{
    register unsigned c;
    register unsigned negate, decimal_point;
    register char *d;
    register int exp;
    register double x;
    register int dpchar;
    char digits[MAXDIGITS];
#if	STRTOD
    int do_store = 0;
    char *dummy;

    if (ptr == (char **)0) 
	ptr = &dummy;	/* harmless dumping ground */
    STORE_PTR;
#endif
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
#if STRTOD
		    do_store++;	/* make sure we store the pointer if val == 0 */
#endif
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
#if !STRTOD
    /* strtod cant return until it finds the end of the exponent */
    if (d == digits) {
	return 0.0;
    }
#else
    if ((d != digits)||(do_store)) {
	/* We must store the decremented pointer in case there isnt 
	   a legitimate exp, IF we have found any digits or leading zeroes.
	*/
        DEC_PTR;
    }
#endif
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
#if !STRTOD
		else break;	/* break if atof, otherwise get to end of exp */
#endif
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
#if STRTOD
    /* atof already returned, but strtod 
       needed to find the end of the exponent */
    if (d == digits) {
	return 0.0;
    }
#endif

    if (exp < -340) {
	x = 0;
#if STRTOD
	setoserror(ERANGE);
#endif
    }
    else if (exp > 308) {
	x = HUGE_VAL;
#if STRTOD
	setoserror(ERANGE);
#endif
    }
    else {
	/* let _atod diagnose under- and over-flows */
	/* if the input was == 0.0, we have already returned,
	   so retval of +-Inf signals OVERFLOW, 0.0 UNDERFLOW
	*/
	x = _atod (digits, (int)(d - digits), exp);
#if STRTOD
	if ((x == 0.0) || (x == HUGE_VAL)) setoserror(ERANGE);
#endif
    }
    if (negate) {
	x = -x;
    }
    return x;
}

