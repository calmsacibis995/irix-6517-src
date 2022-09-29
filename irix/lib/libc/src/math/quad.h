#ifndef __SGIDEFS_H__
#include <sgidefs.h>
#endif

#if (_MIPS_SIM == _MIPS_SIM_ABI32)
#include <stdio.h>
#include <stdlib.h>
#endif

#define	DMANTWIDTH	52

typedef	struct {
double	hi;
double	lo;
} quad;

typedef union {
	long double ld;
	quad q;
} ldquad;

typedef union
{
	struct
	{
		uint32_t hi;
		uint32_t lo;
	} word;

	double	d;
} du;

typedef union
{
	struct
	{
		uint32_t hi[2];
		uint32_t lo[2];
	} word;

	long double	ld;
} ldu;

#if (_MIPS_ISA == _MIPS_ISA_MIPS1 || _MIPS_ISA == _MIPS_ISA_MIPS2)

#define	FLT2INT(x, n)	n = *(int *)&x
#define	INT2FLT(n, x)	x = *(float *)&n
#define	DBL2LL(x, l)	l = *(long long *)&x
#define	LL2DBL(l, x)	x = *(double *)&l

#else

#pragma intrinsic (__builtin_cast_d2ll);
#pragma intrinsic (__builtin_cast_ll2d);

#define	DBL2LL(x, l)	l = __builtin_cast_d2ll(x)
#define	LL2DBL(l, x)	x = __builtin_cast_ll2d(l)

#endif

