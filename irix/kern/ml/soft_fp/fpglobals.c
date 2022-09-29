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
 * Module: fpglobals.c
 * $Revision: 1.9 $
 * $Date: 1997/09/19 01:04:04 $
 * $Author: sfc $
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/soft_fp/RCS/fpglobals.c,v $
 *
 * Revision history:
 *  12-Jun-94 - Original Version
 *
 * Description:	contains globals and global initialization routine
 *		for softfp emulation package
 *
 * ====================================================================
 * ====================================================================
 */

#ident "$Revision: 1.9 $"

/* define __FPT to compile for the fpt test package */

#include <inttypes.h>
#include "fpparams.h"
#include "fpexterns.h"

#ifdef	_KERNEL
#include <sys/types.h>
#include <sys/pda.h>
#endif

const int32_t EXP_BIAS[2] = {SEXP_BIAS, DEXP_BIAS};

const int32_t EXP_MIN[2] = {SEXP_MIN, DEXP_MIN};

const int32_t EXP_NAN[2] = {SEXP_NAN, DEXP_NAN};

const int32_t EXP_SHIFT[2] = {SEXP_SHIFT, DEXP_SHIFT};

const int32_t MANTWIDTH[2] = {SMANTWIDTH, DMANTWIDTH};

const int32_t SIGN_SHIFT[2] = {SSIGN_SHIFT, DSIGN_SHIFT};

const int32_t EXP_INF[2] = {SEXP_INF, DEXP_INF};

const int32_t EXP_MASK[2] = {SEXP_MASK, DEXP_MASK};

const int64_t FRAC_MASK[2] = {SFRAC_MASK, DFRAC_MASK};

const int64_t IMP_1BIT[2] = {SIMP_1BIT, DIMP_1BIT};

const int64_t INFINITY[2] = {SINFINITY, DINFINITY};

const int64_t MTWOP63[2] = {MSTWOP63, MDTWOP63};

const int64_t QUIETNAN[2] = {SQUIETNAN, DQUIETNAN};

const int64_t SIGNBIT[2] = {SSIGNBIT, DSIGNBIT};

const int64_t TWOP0[2] = {STWOP0, DTWOP0};

const uint64_t SNANBIT_MASK[2] = {SSNANBIT_MASK, DSNANBIT_MASK};

const PFV round[2] = {round_s, round_d};

const PFI renorm[2] = {renorm_s, renorm_d};

/* underflow table is indexed by rounding mode, sign bit,
 * guard_bit, and first two sticky bits
 */

const int8_t	underflow_tab[4][2][2][2][2] =
{
0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
};

const int8_t	round_bit[4][2][2][2][2] =
{
0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1,
};

#if !defined(FUSED_MADD) && defined(MIPS4_ISA)

const PFLL maddrnd[2] = {maddrnd_s, maddrnd_d};
#endif
