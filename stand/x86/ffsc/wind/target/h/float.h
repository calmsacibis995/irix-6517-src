/* float.h - float header file */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,22sep92,rrr  added support for c++
01b,04jul92,jcf  cleaned up.
01a,03jul92,smb  written.
*/

#ifndef __INCfloath
#define __INCfloath

#ifdef __cplusplus
extern "C" {
#endif

#include "types/vxansi.h"

#define DBL_DIG		_ARCH_DBL_DIG
#define DBL_EPSILON	_ARCH_DBL_EPSILON
#define DBL_MANT_DIG	_ARCH_DBL_MANT_DIG
#define DBL_MAX		_ARCH_DBL_MAX
#define DBL_MAX_10_EXP	_ARCH_DBL_MAX_10_EXP
#define DBL_MAX_EXP	_ARCH_DBL_MAX_EXP
#define DBL_MIN		_ARCH_DBL_MIN
#define DBL_MIN_10_EXP	_ARCH_DBL_MIN_10_EXP
#define DBL_MIN_EXP	_ARCH_DBL_MIN_EXP

#define FLT_DIG		_ARCH_FLT_DIG
#define FLT_EPSILON	_ARCH_FLT_EPSILON
#define FLT_MANT_DIG	_ARCH_FLT_MANT_DIG
#define FLT_MAX		_ARCH_FLT_MAX
#define FLT_MAX_10_EXP	_ARCH_FLT_MAX_10_EXP
#define FLT_MAX_EXP	_ARCH_FLT_MAX_EXP
#define FLT_MIN		_ARCH_FLT_MIN
#define FLT_MIN_10_EXP	_ARCH_FLT_MIN_10_EXP
#define FLT_MIN_EXP	_ARCH_FLT_MIN_EXP

#define FLT_RADIX	_ARCH_FLT_RADIX
#define FLT_ROUNDS	_ARCH_FLT_ROUNDS

#define LDBL_DIG	_ARCH_LDBL_DIG
#define LDBL_EPSILON	_ARCH_LDBL_EPSILON
#define LDBL_MANT_DIG	_ARCH_LDBL_MANT_DIG
#define LDBL_MAX	_ARCH_LDBL_MAX
#define LDBL_MAX_10_EXP	_ARCH_LDBL_MAX_10_EXP
#define LDBL_MAX_EXP	_ARCH_LDBL_MAX_EXP
#define LDBL_MIN	_ARCH_LDBL_MIN
#define LDBL_MIN_10_EXP	_ARCH_LDBL_MIN_10_EXP
#define LDBL_MIN_EXP	_ARCH_LDBL_MIN_EXP

#ifdef __cplusplus
}
#endif

#endif /* __INCfloath */
