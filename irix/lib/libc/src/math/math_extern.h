/**************************************************************************
 *									  *
 * Copyright (C) 1986-1993 Silicon Graphics, Inc.			  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __MATH_EXTERN_H__
#define __MATH_EXTERN_H__

#include "lldefs.h"
#include <sgidefs.h>

/* atod.c */
extern double _atod (char *, __int32_t, __int32_t);

/* dtoa.c */
extern int _dtoa(char *, __int32_t, double, __int32_t);

/* dwmultuc.c */
extern void __dwmultu(__uint64_t [2], __uint64_t, __uint64_t );

/* fp_class.s */
extern int _fp_class_d(double);

/* isnand.s */
extern int _isnand(double);

/* ldexp.s */
extern double _ldexp(double, int);

/* logb.s */
extern double _logb (double);

/* lldivrem.s */
extern void __ull_divrem_6416 (ulonglong_t *q, ulonglong_t *r, ulonglong_t n, ulonglong_t d);
extern void __ull_divrem_5353 (ulonglong_t *q, ulonglong_t *r, ulonglong_t n, ulonglong_t d);

#if (_MIPS_SIM != _MIPS_SIM_ABI32)
#define _IEEE 1
#include "fpparts.h"
#undef 	_IEEE
/* atoq.c */
extern	long double _atoq(char *, int, int);

/* fp_class_q.s */
extern int _fp_class_q(long double);

/* qtoa.c */
extern int _qtoa(char *, __int32_t, long double, __int32_t);

/* qldexp.d */
extern long double _qldexp(long double, int);

/* qtenscale.c */
extern void __qtenscale(_ldblval *, int, int *);

/* qmultu.s */
extern void _qwmultu(__uint64_t [4], _ldblval *, _ldblval *);
#endif

/* tenscalec.c */
extern void __tenscale(__uint64_t *, __int32_t, __int32_t *);

#endif
