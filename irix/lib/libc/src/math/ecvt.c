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
#ident	"$Revision: 1.19 $"

#include <sgidefs.h>

#ifdef __STDC__
        #pragma weak ecvt = _ecvt
        #pragma weak fcvt = _fcvt
#ifndef _LIBC_ABI
        #pragma weak ecvt_r = _ecvt_r
        #pragma weak fcvt_r = _fcvt_r
#endif

#if (_MIPS_SIM != _MIPS_SIM_ABI32) && (!defined(_LIBC_ABI))

        #pragma weak qecvt = _qecvt
        #pragma weak ecvtl = _qecvt

        #pragma weak qfcvt = _qfcvt
        #pragma weak fcvtl = _qfcvt

        #pragma weak qecvt_r = _qecvt_r
        #pragma weak ecvtl_r = _qecvt_r

        #pragma weak qfcvt_r = _qfcvt_r
        #pragma weak fcvtl_r = _qfcvt_r

#endif

#endif

#include "synonyms.h"
#include "math_extern.h"
#include "stdlib.h"
/* Support obsolete interfaces */

#define	NDIG 82
#ifdef _LIBC_NONSHARED
static char buffer[NDIG+2];
#else  /* _LIBC_NONSHARED */
static char *buffer _INITBSS;
#endif /* _LIBC_NONSHARED */

char *
ecvt(register double arg, int ndigits, int *decpt, int *sign)
{
#ifndef _LIBC_NONSHARED
    if ((buffer == 0) && ((buffer = (char *)calloc(1, NDIG+2)) == 0))
	return 0;
#endif /* _LIBC_NONSHARED */
    return ecvt_r(arg, ndigits, decpt, sign, buffer);
}

char *
ecvt_r(register double arg, int ndigits, int *decpt, int *sign, char *buf)
{
    if (ndigits > 17) {
	register char *p, *e;
	*decpt = _dtoa (buf, 17, arg, 0) + 1;
	for (p = buf+18, e = buf + 1 + (ndigits > NDIG ? NDIG : ndigits);
	     p != e; ) *p++ = '0';
	*p++ = '\0';
    }
    else if (ndigits <= 0) {
	*decpt = _dtoa (buf, 1, arg, 0) + 1;
	buf[1] = '\0';
    }
    else {
	*decpt = _dtoa (buf, ndigits, arg, 0) + 1;
    }
    *sign = buf[0] == '-';
    return buf+1;
}

char *
fcvt(register double arg, int ndigits, int *decpt, int *sign)
{
#ifndef _LIBC_NONSHARED
    if ((buffer == 0) && ((buffer = (char *)calloc(1, NDIG+2)) == 0))
	return 0;
#endif /* _LIBC_NONSHARED */
    return fcvt_r(arg, ndigits, decpt, sign, buffer);
}

char *
fcvt_r(register double arg, int ndigits, int *decpt, int *sign, char *buf)
{
    *decpt = _dtoa (buf, ndigits, arg, 1) + 1;
    *sign = buf[0] == '-';
    return buf+1;
}

#if (_MIPS_SIM != _MIPS_SIM_ABI32)

char *
qecvt(long double arg, int ndigits, int *decpt, int *sign)
{
#ifndef _LIBC_NONSHARED
    if ((buffer == 0) && ((buffer = (char *)calloc(1, NDIG+2)) == 0))
	return 0;
#endif /* _LIBC_NONSHARED */
    return qecvt_r(arg, ndigits, decpt, sign, buffer);
}
char *
qecvt_r(long double arg, int ndigits, int *decpt, int *sign, char *buf)
{
    if (ndigits > 34) {
	register char *p, *e;
	*decpt = _qtoa (buf, 34, arg, 0) + 1;
	for (p = buf+18, e = buf + 1 + (ndigits > NDIG ? NDIG : ndigits);
	     p != e; ) *p++ = '0';
	*p++ = '\0';
    }
    else if (ndigits <= 0) {
	*decpt = _qtoa (buf, 1, arg, 0) + 1;
	buf[1] = '\0';
    }
    else {
	*decpt = _qtoa (buf, ndigits, arg, 0) + 1;
    }
    *sign = buf[0] == '-';
    return buf+1;
}

char *
qfcvt(long double arg, int ndigits, int *decpt, int *sign)
{
#ifndef _LIBC_NONSHARED
    if ((buffer == 0) && ((buffer = (char *)calloc(1, NDIG+2)) == 0))
	return 0;
#endif /* _LIBC_NONSHARED */
    return qfcvt_r(arg, ndigits, decpt, sign, buffer);
}

char *
qfcvt_r(long double arg, int ndigits, int *decpt, int *sign, char *buf)
{
    *decpt = _qtoa (buf, ndigits, arg, 1) + 1;
    *sign = buf[0] == '-';
    return buf+1;
}

#endif
