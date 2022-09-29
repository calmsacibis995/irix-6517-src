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
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef __FPPARTS_H__
#define __FPPARTS_H__
#ident "$Id: fpparts.h,v 1.6 1997/10/11 19:58:45 vegas Exp $"

#ifndef __SGIDEFS_H__
#include <sgidefs.h>
#endif

/* This version of fpparts.h includes definitions for quad
 * precision floating point. As of 8/10/93 this is only used by
 * the quad to/from string/print/scan routines. 
 */

/* defines put here temporarily until they can find a home in 
 * /usr/include
 */

#define	_IEEE_RM	0
#define _ROUNDUP_RM	1

/* Macros to pull apart parts of single and  double precision
 * floating point numbers in IEEE format
 * Be sure to include /usr/include/values.h before including
 * this file to get the required definition of _IEEE
 */

#if _IEEE

#if M32 || u3b || u3b2 || u3b5 || u3b15  || _MIPSEB
/* byte order with high order bits at lowest address */

/* double precision */
typedef  union {
#if (_MIPS_SIM != _MIPS_SIM_ABI32)
	struct {
		unsigned  sign	:1;
		unsigned  exp	:11;
		unsigned long long mantissa :52;
	} lparts;
#endif
	struct {
		unsigned  sign	:1;
		unsigned  exp	:11;
		unsigned  hi	:20;
		unsigned  lo	:32;
	} fparts;
	struct {
		unsigned  sign	:1;
		unsigned  exp	:11;
		unsigned  qnan_bit	:1;
		unsigned  hi	:19;
		unsigned  lo	:32;
	} nparts;
	struct {
		unsigned hi;
		unsigned lo;
	} fwords;
	double	d;
} _dval;

typedef union
{
	struct
	{
		unsigned hi;
		unsigned lo;
	} word;

	double	d;
} dblu;

#if (_MIPS_SIM != _MIPS_SIM_ABI32)
/* quad precision */
typedef  union {
	struct {
		_dval	hi;
		_dval	lo;
	} qparts;
	struct {
		unsigned long long hi;
		unsigned long long lo;
	} fwords;
	long double	ldbl;
} _ldblval;
#endif

/* single precision */
typedef  union {
	struct {
		unsigned sign	:1;
		unsigned exp	:8;
		unsigned fract	:23;
	} fparts;
	struct {
		unsigned sign	:1;
		unsigned exp	:8;
		unsigned qnan_bit	:1;
		unsigned fract	:22;
	} nparts;
	unsigned long	fword;
	float	f;
} _fval;


#else
#if i386 || _MIPSEL
/* byte order with low order bits at lowest address */

/* quad precision */
typedef  union {
	struct {
		_dval	lo;
		_dval	hi;
	} qparts;
	struct {
		unsigned long long lo;
		unsigned long long hi;
	} fwords;
	long double	ldbl;
} _ldblval;

/* double precision */
typedef  union {
#if (_MIPS_SIM != _MIPS_SIM_ABI32)
	struct {
		unsigned  lo	:32;
		unsigned  hi	:20;
		unsigned  exp	:11;
		unsigned  sign	:1;
	} lparts;
#endif
	struct {
		unsigned  lo	:32;
		unsigned  hi	:20;
		unsigned  exp	:11;
		unsigned  sign	:1;
	} fparts;
	struct {
		unsigned  lo	:32;
		unsigned  hi	:19;
		unsigned  qnan_bit	:1;
		unsigned  exp	:11;
		unsigned  sign	:1;
	} nparts;
	struct {
		unsigned  lo	:32;
		unsigned  hi	:32;
	} fwords;
	double	d;
} _dval;

/* single precision */
typedef  union {
	struct {
		unsigned fract	:23;
		unsigned exp	:8;
		unsigned sign	:1;
	} fparts;
	struct {
		unsigned fract	:22;
		unsigned qnan_bit	:1;
		unsigned exp	:8;
		unsigned sign	:1;
	} nparts;
	unsigned long	fword;
	float	f;
} _fval;
#else
#if i286
/* byte order with low order bits at lowest address
 * machines which cannot address 32 bits at a time
 */

/* quad precision */
typedef  union {
	struct {
		_dval	lo;
		_dval	hi;
	} qparts;
	struct {
		unsigned long long lo;
		unsigned long long hi;
	} fwords;
	long double	ldbl;
} _ldblval;

/* double precision */
typedef  union {
	struct {
		unsigned  long lo;
		unsigned  hi1   :16;
		unsigned  hi2	:4;
		unsigned  exp	:11;
		unsigned  sign	:1;
	} fparts;
	struct {
		unsigned  long lo;
		unsigned  hi1	:16;
		unsigned  hi2	:3;
		unsigned  qnan_bit	:1;
		unsigned  exp	:11;
		unsigned  sign	:1;
	} nparts;
	struct {
		unsigned  long lo;
		unsigned  long hi;
	} fwords;
	double	d;
} _dval;

/* single precision */
typedef  union {
	struct {
		unsigned fract1	:16;
		unsigned fract2	:7;
		unsigned exp	:8;
		unsigned sign	:1;
	} fparts;
	struct {
		unsigned fract1	:16;
		unsigned fract2	:6;
		unsigned qnan_bit	:1;
		unsigned exp	:8;
		unsigned sign	:1;
	} nparts;
	unsigned long	fword;
	float	f;
} _fval;
#endif /* i286 */
#endif /* i386 */
#endif /* 3B  */

/* parts of a double precision floating point number */
#define	SIGNBIT(X)	(((_dval *)&(X))->fparts.sign)
#define EXPONENT(X)	(((_dval *)&(X))->fparts.exp)

#if M32 || u3b || u3b2 || u3b5 || u3b15 || i386 || __mips
#define HIFRACTION(X)	(((_dval *)&(X))->fparts.hi)
#if (_MIPS_SIM != _MIPS_SIM_ABI32)
#define FRACTION(X)	(((_dval *)&(X))->lparts.mantissa)
#endif
#endif

#define LOFRACTION(X)	(((_dval *)&(X))->fparts.lo)
#define QNANBIT(X)	(((_dval *)&(X))->nparts.qnan_bit)
#define HIWORD(X)	(((_dval *)&(X))->fwords.hi)
#define LOWORD(X)	(((_dval *)&(X))->fwords.lo)

#define MAXEXP	0x7ff /* maximum exponent of double*/
#define ISMAXEXP(X)	((EXPONENT(X)) == MAXEXP)

/* macros used to create quiet NaNs as return values */
#define SETQNAN(X)	((((_dval *)&(X))->nparts.qnan_bit) = 0x1)
#define HIQNAN(X)	((HIWORD(X)) = 0x7ff80000)
#define LOQNAN(X)	((((_dval *)&(X))->fwords.lo) = 0x0)

/* macros used to extract parts of single precision values */
#define	FSIGNBIT(X)	(((_fval *)&(X))->fparts.sign)
#define FEXPONENT(X)	(((_fval *)&(X))->fparts.exp)

#if M32 || u3b || u3b2 || u3b5 || u3b15 || i386 || __mips
#define FFRACTION(X)	(((_fval *)&(X))->fparts.fract)
#endif

#define FWORD(X)	(((_fval *)&(X))->fword)
#define FQNANBIT(X)	(((_fval *)&(X))->nparts.qnan_bit)
#define MAXEXPF	255 /* maximum exponent of single*/
#define FISMAXEXP(X)	((FEXPONENT(X)) == MAXEXPF)

#endif  /*IEEE*/

#endif
