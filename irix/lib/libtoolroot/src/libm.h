/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/* ====================================================================
 * ====================================================================
 *
 * Module: libm.h
 * $Revision: 1.1 $
 * $Date: 1997/09/17 19:17:30 $
 * $Author: bean $
 * $Source: /proj/irix6.5.7m/isms/irix/lib/libtoolroot/src/RCS/libm.h,v $
 *
 * Revision history:
 *  09-Jun-93 - Original Version
 *
 * Description:  various typedefs, pragmas, and externs for libm functions
 *
 * ====================================================================
 * ====================================================================
 */

#ifndef libm_INCLUDED
#define libm_INCLUDED

#include <sgidefs.h>
#include <errno.h>
#include <svr4_math.h>

#ifndef __MATH_H__

extern	float	fabsf(float);
#pragma intrinsic (fabsf)

extern	double	fabs(double);
#pragma intrinsic (fabs)

extern float    sqrtf(float);
extern	double	sqrt(double);

#if _MIPS_ISA != _MIPS_ISA_MIPS1
#pragma intrinsic (sqrtf)
#pragma intrinsic (sqrt)
#endif

#endif

extern	const double	__libm_qnan_d;
extern	const float	__libm_qnan_f;
extern	const double	__libm_inf_d;
extern	const float	__libm_inf_f;
extern	const double	__libm_neginf_d;
extern	const float	__libm_neginf_f;


/*
 * Commented out the following lines related to errnoaddr  because I don't
 * think anyone cares about this feature from the context of libtoolroot.so.
 * And because I couldn't figure out how to get errnoaddr working correctly
 * within libtoolroot.so :-)
 *	bean 970917
 */

#if 0
extern	int	*__errnoaddr;

#define SETERRNO(x)     \
        {       \
                *__errnoaddr = x; \
        }

#ifdef _IP_NAN_SETS_ERRNO

#define NAN_SETERRNO(x)     \
        {       \
                *__errnoaddr = x; \
        }
#else

#define NAN_SETERRNO(x)

#endif

#else
#define NAN_SETERRNO(x)
#define SETERRNO(x)
#endif

typedef union
{
	struct
	{
		unsigned int hi;
		unsigned int lo;
	} word;

	double	d;
} du;

typedef union
{
	unsigned int	i;
	float	f;
} fu;

#define	EXPMASK	0x807fffff
#define	SIGNMASK	0x7fffffff

#define	DEXPWIDTH	11


#if (_MIPS_ISA == _MIPS_ISA_MIPS1 || _MIPS_ISA == _MIPS_ISA_MIPS2)

#define	ROUND(d)	(int)(((d) >= 0.0) ? ((d) + 0.5) : ((d) - 0.5))

#define	ROUNDF(d)	(int)(((d) >= (float)0.0) ? ((d) + (float)0.5) : ((d) - (float)0.5))

#define	INT	int
#define	UINT	unsigned int
#define	DEXPMASK	0x800fffff
#define	DSIGNMASK	0x7fffffff
#define	DMANTWIDTH	20
#define	MANTWIDTH	23

#define	DBLHI2INT(x, n)	n = *(int *)&x
#define	DBLLO2INT(x, n)	n = *((int *)&x + 1)
#define	INT2DBLLO(n, x)	*((int *)&x + 1) = n
#define	INT2DBLHI(n, x)	*(int *)&x = n

#else

#define	ROUND(d)	round(d)
#define	ROUNDF(f)	roundf(f)

int	roundf(float);
#pragma intrinsic (roundf)

int	round(double);
#pragma intrinsic (round)

#define	INT	long long
#define	UINT	unsigned long long
#define	DEXPMASK	0x800fffffffffffffll
#define	DSIGNMASK	0x7fffffffffffffffll
#define	DMANTWIDTH	52
#define	MANTWIDTH	23

#endif

#if (_MIPS_ISA == _MIPS_ISA_MIPS1 || _MIPS_ISA == _MIPS_ISA_MIPS2)

#define	FLT2INT(x, n)	n = *(int *)&x
#define	INT2FLT(n, x)	x = *(float *)&n
#define	DBL2LL(x, l)	l = *(long long *)&x
#define	LL2DBL(l, x)	x = *(double *)&l

#else

#pragma intrinsic (__builtin_cast_f2i);
#pragma intrinsic (__builtin_cast_i2f);

#pragma intrinsic (__builtin_cast_d2ll);
#pragma intrinsic (__builtin_cast_ll2d);

#define	FLT2INT(x, n)	n = __builtin_cast_f2i(x)
#define	INT2FLT(n, x)	x = __builtin_cast_i2f(n)
#define	DBL2LL(x, l)	l = __builtin_cast_d2ll(x)
#define	LL2DBL(l, x)	x = __builtin_cast_ll2d(l)



#endif



#endif /* libm_INCLUDED */

