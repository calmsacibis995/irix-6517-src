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

#ident "$Revision: 1.6 $"

#include "ml/ml.h"

/*
 * stores a one doubleword floating point value from gp register a0 (or
 * a0/a1, if compiled with -o32)
 * into the fp register specified by "TARGET_REG"
 * If a2 (a3 if -o32) is non-zero, the coprocessor
 * is in extended register mode.
 */
#if _MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32
#define	CP_2W(fr_even, fr_odd)				\
			mtc1	a0, fr_even;		\
			dsrl32	a0, 0;			\
			mtc1	a0, fr_odd;		\
			j	ra;	nop

#define	EXREG_CP_2W(fr)					\
			dmtc1	a0, fr;			\
			j	ra;	nop

#define	TARGET_REG	a1
#define	IS_EXMODE	a2

#elif _MIPS_SIM == _MIPS_SIM_ABI32
#define	CP_2W(fr_even, fr_odd)				\
			mtc1	a0, fr_odd;		\
			mtc1	a1, fr_even;		\
			j	ra;	nop

#define	EXREG_CP_2W(fr)					\
			dsll32	a0, 0;			\
			dsll32	a1, 0;			\
			dsrl32	a1, 0;			\
			or	a0, a1;			\
			dmtc1	a0,fr;			\
			j	ra;	nop

#define	TARGET_REG	a2
#define	IS_EXMODE	a3

#endif

	PICOPT
	.text

LEAF(fpunit_fpstore_d)
	# shift rdnum to do a table fetch
	sll	v0,TARGET_REG,PTR_SCALESHIFT
#if TFP
	/*
	 * Special case for $f4 since it gets used in bcopy/bzero and
	 * is saved/restored in a special way.
	 */
	bne	TARGET_REG,4,1f
	PTR_L	t1, VPDA_CURUTHREAD(zero)
	PTR_L	t1, UT_EXCEPTION(t1)
	PTR_ADDIU t1, U_PCB+PCB_FPREGS
	PTR_ADDU t1, v0
	REG_S	a0, 0(t1)
1:
#endif

	bne	IS_EXMODE,zero,1f		# branch if in exreg_mode
	srl	v0,1
	PTR_L	v0,cp_2w_tab(v0)
	j	v0
1:
	PTR_L	v0,exreg_cp_2w_tab(v0)
	j	v0

	.rdata
cp_2w_tab:
	PTR_WORD	cp_2w_fpr0:1,  cp_2w_fpr2:1,  cp_2w_fpr4:1
	PTR_WORD	cp_2w_fpr6:1,  cp_2w_fpr8:1,  cp_2w_fpr10:1
	PTR_WORD	cp_2w_fpr12:1, cp_2w_fpr14:1, cp_2w_fpr16:1
	PTR_WORD	cp_2w_fpr18:1, cp_2w_fpr20:1, cp_2w_fpr22:1
	PTR_WORD	cp_2w_fpr24:1, cp_2w_fpr26:1, cp_2w_fpr28:1
	PTR_WORD	cp_2w_fpr30:1
	.text

	.set	noreorder
cp_2w_fpr0:	CP_2W($f0, $f1)
cp_2w_fpr2:	CP_2W($f2, $f3)
cp_2w_fpr4:	CP_2W($f4, $f5)
cp_2w_fpr6:	CP_2W($f6, $f7)
cp_2w_fpr8:	CP_2W($f8, $f9)
cp_2w_fpr10:	CP_2W($f10, $f11)
cp_2w_fpr12:	CP_2W($f12, $f13)
cp_2w_fpr14:	CP_2W($f14, $f15)
cp_2w_fpr16:	CP_2W($f16, $f17)
cp_2w_fpr18:	CP_2W($f18, $f19)
cp_2w_fpr20:	CP_2W($f20, $f21)
cp_2w_fpr22:	CP_2W($f22, $f23)
cp_2w_fpr24:	CP_2W($f24, $f25)
cp_2w_fpr26:	CP_2W($f26, $f27)
cp_2w_fpr28:	CP_2W($f28, $f29)
cp_2w_fpr30:	CP_2W($f30, $f31)
	.set	reorder

	.rdata
exreg_cp_2w_tab:
	PTR_WORD	exreg_cp_2w_fpr0:1,	exreg_cp_2w_fpr1:1
	PTR_WORD	exreg_cp_2w_fpr2:1,	exreg_cp_2w_fpr3:1
	PTR_WORD	exreg_cp_2w_fpr4:1,	exreg_cp_2w_fpr5:1
	PTR_WORD	exreg_cp_2w_fpr6:1,	exreg_cp_2w_fpr7:1
	PTR_WORD	exreg_cp_2w_fpr8:1,	exreg_cp_2w_fpr9:1
	PTR_WORD	exreg_cp_2w_fpr10:1,	exreg_cp_2w_fpr11:1
	PTR_WORD	exreg_cp_2w_fpr12:1,	exreg_cp_2w_fpr13:1
	PTR_WORD	exreg_cp_2w_fpr14:1,	exreg_cp_2w_fpr15:1
	PTR_WORD	exreg_cp_2w_fpr16:1,	exreg_cp_2w_fpr17:1
	PTR_WORD	exreg_cp_2w_fpr18:1,	exreg_cp_2w_fpr19:1
	PTR_WORD	exreg_cp_2w_fpr20:1,	exreg_cp_2w_fpr21:1
	PTR_WORD	exreg_cp_2w_fpr22:1,	exreg_cp_2w_fpr23:1
	PTR_WORD	exreg_cp_2w_fpr24:1,	exreg_cp_2w_fpr25:1
	PTR_WORD	exreg_cp_2w_fpr26:1,	exreg_cp_2w_fpr27:1
	PTR_WORD	exreg_cp_2w_fpr28:1,	exreg_cp_2w_fpr29:1
	PTR_WORD	exreg_cp_2w_fpr30:1,	exreg_cp_2w_fpr31:1

	.text
	.set	noreorder
exreg_cp_2w_fpr0:	EXREG_CP_2W($f0)
exreg_cp_2w_fpr1:	EXREG_CP_2W($f1)
exreg_cp_2w_fpr2:	EXREG_CP_2W($f2)
exreg_cp_2w_fpr3:	EXREG_CP_2W($f3)
exreg_cp_2w_fpr4:	EXREG_CP_2W($f4)
exreg_cp_2w_fpr5:	EXREG_CP_2W($f5)
exreg_cp_2w_fpr6:	EXREG_CP_2W($f6)
exreg_cp_2w_fpr7:	EXREG_CP_2W($f7)
exreg_cp_2w_fpr8:	EXREG_CP_2W($f8)
exreg_cp_2w_fpr9:	EXREG_CP_2W($f9)
exreg_cp_2w_fpr10:	EXREG_CP_2W($f10)
exreg_cp_2w_fpr11:	EXREG_CP_2W($f11)
exreg_cp_2w_fpr12:	EXREG_CP_2W($f12)
exreg_cp_2w_fpr13:	EXREG_CP_2W($f13)
exreg_cp_2w_fpr14:	EXREG_CP_2W($f14)
exreg_cp_2w_fpr15:	EXREG_CP_2W($f15)
exreg_cp_2w_fpr16:	EXREG_CP_2W($f16)
exreg_cp_2w_fpr17:	EXREG_CP_2W($f17)
exreg_cp_2w_fpr18:	EXREG_CP_2W($f18)
exreg_cp_2w_fpr19:	EXREG_CP_2W($f19)
exreg_cp_2w_fpr20:	EXREG_CP_2W($f20)
exreg_cp_2w_fpr21:	EXREG_CP_2W($f21)
exreg_cp_2w_fpr22:	EXREG_CP_2W($f22)
exreg_cp_2w_fpr23:	EXREG_CP_2W($f23)
exreg_cp_2w_fpr24:	EXREG_CP_2W($f24)
exreg_cp_2w_fpr25:	EXREG_CP_2W($f25)
exreg_cp_2w_fpr26:	EXREG_CP_2W($f26)
exreg_cp_2w_fpr27:	EXREG_CP_2W($f27)
exreg_cp_2w_fpr28:	EXREG_CP_2W($f28)
exreg_cp_2w_fpr29:	EXREG_CP_2W($f29)
exreg_cp_2w_fpr30:	EXREG_CP_2W($f30)
exreg_cp_2w_fpr31:	EXREG_CP_2W($f31)
	.set	reorder

END(fpunit_fpstore_d)

