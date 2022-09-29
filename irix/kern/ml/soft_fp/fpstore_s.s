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
 * stores a one word floating point value from gp register "SOURCE_REG"
 * into the fp register specified by "TARGET_REG".
 */
#if _MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32
#define	SOURCE_REG	a0
#define	TARGET_REG	a1

#elif _MIPS_SIM == _MIPS_SIM_ABI32
#define	SOURCE_REG	a1
#define	TARGET_REG	a2
#endif

	PICOPT
	.text

LEAF(fpunit_fpstore_s)
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
	sreg	SOURCE_REG, 0(t1)
1:
#endif

	PTR_L	v0,cp_1w_tab(v0)
	j	v0

	.rdata
cp_1w_tab:
	PTR_WORD	cp_1w_fpr0:1,   cp_1w_fpr1:1,   cp_1w_fpr2:1
	PTR_WORD	cp_1w_fpr3:1,   cp_1w_fpr4:1,   cp_1w_fpr5:1
	PTR_WORD	cp_1w_fpr6:1,   cp_1w_fpr7:1,   cp_1w_fpr8:1
	PTR_WORD	cp_1w_fpr9:1,   cp_1w_fpr10:1,  cp_1w_fpr11:1
	PTR_WORD	cp_1w_fpr12:1,  cp_1w_fpr13:1,  cp_1w_fpr14:1
	PTR_WORD	cp_1w_fpr15:1,  cp_1w_fpr16:1,  cp_1w_fpr17:1
	PTR_WORD	cp_1w_fpr18:1,  cp_1w_fpr19:1,  cp_1w_fpr20:1
	PTR_WORD	cp_1w_fpr21:1,  cp_1w_fpr22:1,  cp_1w_fpr23:1
	PTR_WORD	cp_1w_fpr24:1,  cp_1w_fpr25:1,  cp_1w_fpr26:1
	PTR_WORD	cp_1w_fpr27:1,  cp_1w_fpr28:1,  cp_1w_fpr29:1
	PTR_WORD	cp_1w_fpr30:1,  cp_1w_fpr31:1
	.text

	.set	noreorder
cp_1w_fpr0:	mtc1	SOURCE_REG,$f0;		j	ra; 	nop
cp_1w_fpr1:	mtc1	SOURCE_REG,$f1;		j	ra; 	nop
cp_1w_fpr2:	mtc1	SOURCE_REG,$f2;		j	ra;	nop
cp_1w_fpr3:	mtc1	SOURCE_REG,$f3;		j	ra;	nop
cp_1w_fpr4:	mtc1	SOURCE_REG,$f4;		j	ra;	nop
cp_1w_fpr5:	mtc1	SOURCE_REG,$f5;		j	ra;	nop
cp_1w_fpr6:	mtc1	SOURCE_REG,$f6;		j	ra;	nop
cp_1w_fpr7:	mtc1	SOURCE_REG,$f7;		j	ra;	nop
cp_1w_fpr8:	mtc1	SOURCE_REG,$f8;		j	ra;	nop
cp_1w_fpr9:	mtc1	SOURCE_REG,$f9;		j	ra;	nop
cp_1w_fpr10:	mtc1	SOURCE_REG,$f10;	j	ra;	nop
cp_1w_fpr11:	mtc1	SOURCE_REG,$f11;	j	ra;	nop
cp_1w_fpr12:	mtc1	SOURCE_REG,$f12;	j	ra;	nop
cp_1w_fpr13:	mtc1	SOURCE_REG,$f13;	j	ra;	nop
cp_1w_fpr14:	mtc1	SOURCE_REG,$f14;	j	ra;	nop
cp_1w_fpr15:	mtc1	SOURCE_REG,$f15;	j	ra;	nop
cp_1w_fpr16:	mtc1	SOURCE_REG,$f16;	j	ra;	nop
cp_1w_fpr17:	mtc1	SOURCE_REG,$f17;	j	ra;	nop
cp_1w_fpr18:	mtc1	SOURCE_REG,$f18;	j	ra;	nop
cp_1w_fpr19:	mtc1	SOURCE_REG,$f19;	j	ra;	nop
cp_1w_fpr20:	mtc1	SOURCE_REG,$f20;	j	ra;	nop
cp_1w_fpr21:	mtc1	SOURCE_REG,$f21;	j	ra;	nop
cp_1w_fpr22:	mtc1	SOURCE_REG,$f22;	j	ra;	nop
cp_1w_fpr23:	mtc1	SOURCE_REG,$f23;	j	ra;	nop
cp_1w_fpr24:	mtc1	SOURCE_REG,$f24;	j	ra;	nop
cp_1w_fpr25:	mtc1	SOURCE_REG,$f25;	j	ra;	nop
cp_1w_fpr26:	mtc1	SOURCE_REG,$f26;	j	ra;	nop
cp_1w_fpr27:	mtc1	SOURCE_REG,$f27;	j	ra;	nop
cp_1w_fpr28:	mtc1	SOURCE_REG,$f28;	j	ra;	nop
cp_1w_fpr29:	mtc1	SOURCE_REG,$f29;	j	ra;	nop
cp_1w_fpr30:	mtc1	SOURCE_REG,$f30;	j	ra;	nop
cp_1w_fpr31:	mtc1	SOURCE_REG,$f31;	j	ra;	nop
	.set	reorder

END(fpunit_fpstore_s)


