#ifndef __FP_CLASS_H__
#define __FP_CLASS_H__
#ident "$Revision: 1.14 $"
/*
*
* Copyright 1992, Silicon Graphics, Inc.
* All Rights Reserved.
*
* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
* the contents of this file may not be disclosed to third parties, copied or
* duplicated in any form, in whole or in part, without the prior written
* permission of Silicon Graphics, Inc.
*
* RESTRICTED RIGHTS LEGEND:
* Use, duplication or disclosure by the Government is subject to restrictions
* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
* and Computer Software clause at DFARS 252.227-7013, and/or in similar or
* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
* rights reserved under the Copyright Laws of the United States.
*/
#ifdef __cplusplus
extern "C" {
#endif
/* | Copyright (c) 1986 MIPS Computer Systems, Inc.  | */
/* | All Rights Reserved.                            | */
/* --------------------------------------------------- */
/* $Revision: 1.14 $ */



#if (defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS))
extern int fp_class_d(double);
extern int fp_class_f(float);
#if _COMPILER_VERSION >= 400
extern int fp_class_q(long double);
extern int fp_class_l(long double);
#endif
#endif /* LANGUAGE_C */

/*
 * Constants returned by the floating point fp_class_[fdl]() functions.
 */
#define	FP_SNAN		0
#define	FP_QNAN		1
#define	FP_POS_INF	2
#define	FP_NEG_INF	3
#define	FP_POS_NORM	4
#define	FP_NEG_NORM	5
#define	FP_POS_DENORM	6
#define	FP_NEG_DENORM	7
#define	FP_POS_ZERO	8
#define	FP_NEG_ZERO	9

#ifdef __cplusplus
}
#endif
#endif /* !__FP_CLASS_H__ */
