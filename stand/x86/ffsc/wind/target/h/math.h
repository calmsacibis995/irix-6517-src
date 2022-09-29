/* math.h - math routines */

/* Copyright 1989-1995 Wind River Systems, Inc. */

/*
modification history
--------------------
01n,29mar95,kdl  added prototype for __fixunssfsi() (gcc callout routine).
01m,15oct92,rrr  silenced warnings
01l,22sep92,rrr  added support for c++
01k,19sep92,kdl	 added prototypes for gccMathInit, gccUssInit, gccUss040Init.
01j,30jul92,kdl	 changed to ANSI single precision names (e.g. fsin -> sinf);
		 merged various architectures; fixed 01i mod history.
01i,08jul92,smb  merged with ANSI math.h
01h,04jul92,jcf  cleaned up.
01g,26may92,rrr  the tree shuffle
01f,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01e,16jul91,ajm  added MIPS specific externs, and made MIPS unsupported
		 functions 68k dependent
01d,28jan91,kdl  added single-precision, cbrt(), hypot(); changed def's of
		 irint() and iround() to int; changed def of sincos() to void.
01c,19oct90,elr  ANSI cleaned up
01b,05oct90,dnw  deleted private routines.
01a,23aug90,elr	 written.
*/

#ifndef __INCmathh
#define __INCmathh

#ifdef __cplusplus
extern "C" {
#endif

#include "types/vxansi.h"

/* ANSI-compatible definitions */

#define HUGE_VAL        _ARCH_HUGH_VAL


#if defined(__STDC__) || defined(__cplusplus)

/* ANSI-compatible functions */

extern double 	acos (double x);
extern double 	asin (double x);
extern double 	atan (double x);
extern double 	atan2 (double y, double x);
extern double 	ceil (double x);
extern double 	cos (double x);
extern double 	cosh (double x);
extern double 	exp (double x);
extern double 	fabs (double x);
extern double 	floor (double x);
extern double 	fmod (double x, double y);
extern double 	frexp (double x, int *exponent);
extern double 	ldexp (double x, int __exp);
extern double 	log (double x);
extern double 	log10 (double x);
extern double 	modf (double x, double *pIntResult);
extern double 	pow (double x, double y);
extern double 	sin (double x);
extern double 	sinh (double x);
extern double 	sqrt (double x);
extern double 	tan (double x);
extern double 	tanh (double x);



/* Single-precision versions of ANSI functions    *
 *    Not available on all VxWorks architectures. */

extern float 	acosf (float x);
extern float 	asinf (float x);
extern float 	atanf (float x);
extern float 	atan2f (float x, float y);
extern float 	ceilf (float x);
extern float 	cosf (float x);
extern float 	coshf (float x);
extern float 	expf (float x);
extern float 	fabsf (float x);
extern float 	floorf (float x);
extern float 	fmodf (float x);
extern float 	logf (float x);
extern float 	log10f (float x);
extern float 	powf (float x, float y);
extern float 	sinf (float x);
extern float 	sinhf (float x);
extern float 	sqrtf (float x);
extern float 	tanf (float x);
extern float 	tanhf (float x);


/* Miscellaneous math functions                   *
 *    Not available on all VxWorks architectures. */


extern double 	cbrt (double x);
extern double 	hypot (double x, double y);
extern double 	infinity (void);
extern int 	irint (double x);
extern int 	iround (double x);
extern double 	log2 (double x);
extern double 	rint (double x);
extern double 	round (double x);
extern void 	sincos (double x, double *pSinResult, double *pCosResult);
extern double 	trunc (double x);

extern float 	cbrtf (float x);
extern float 	hypotf (float x, float y);
extern float 	infinityf (void);
extern int 	irintf (float x);
extern int 	iroundf (float x);
extern float 	log2f (float x);
extern float 	roundf (float x);
extern void 	sincosf (float x, float *pSinResult, float *pCosResult);
extern float 	truncf (float x);


/* VxWorks math library calls */

extern void 	floatInit (void);
extern void 	mathHardInit (void);
extern void 	mathSoftInit (void);
extern void	gccMathInit (void);
extern void	gccUssInit (void);
extern void	gccUss040Init (void);
extern unsigned	__fixunssfsi (long a);

#else	/* !__STDC__ */

/* ANSI-compatible functions */

extern double 	acos ();
extern double 	asin ();
extern double 	atan ();
extern double 	atan2 ();
extern double 	ceil ();
extern double 	cos ();
extern double 	cosh ();
extern double 	exp ();
extern double 	fabs ();
extern double 	floor ();
extern double 	fmod ();
extern double 	frexp ();
extern double 	ldexp ();
extern double 	log ();
extern double 	log10 ();
extern double 	modf ();
extern double 	pow ();
extern double 	sin ();
extern double 	sinh ();
extern double 	sqrt ();
extern double 	tan ();
extern double 	tanh ();



/* Single-precision versions of ANSI functions    *
 *    Not available on all VxWorks architectures. */

extern float 	acosf ();
extern float 	asinf ();
extern float 	atanf ();
extern float 	atan2f ();
extern float 	ceilf ();
extern float 	cosf ();
extern float 	coshf ();
extern float 	expf ();
extern float 	fabsf ();
extern float 	floorf ();
extern float 	fmodf ();
extern float 	logf ();
extern float 	log10f ();
extern float 	powf ();
extern float 	sinf ();
extern float 	sinhf ();
extern float 	sqrtf ();
extern float 	tanf ();
extern float 	tanhf ();


/* Miscellaneous math functions                   *
 *    Not available on all VxWorks architectures. */


extern double 	cbrt ();
extern double 	hypot ();
extern double 	infinity ();
extern int 	irint ();
extern int 	iround ();
extern double 	log2 ();
extern double 	rint ();
extern double 	round ();
extern void 	sincos ();
extern double 	trunc ();

extern float 	cbrtf ();
extern float 	hypotf ();
extern float 	infinityf ();
extern int 	irintf ();
extern int 	iroundf ();
extern float 	log2f ();
extern float 	roundf ();
extern void 	sincosf ();
extern float 	truncf ();


/* VxWorks math library calls */

extern void 	floatInit ();
extern void 	mathHardInit ();
extern void 	mathSoftInit ();
extern void	gccMathInit ();
extern void	gccUssInit ();
extern void	gccUss040Init ();
extern unsigned	__fixunssfsi();


#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCmathh */
