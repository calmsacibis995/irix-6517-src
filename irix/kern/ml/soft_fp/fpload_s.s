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

#ident "$Revision: 1.3 $"

#include "ml/ml.h"

/* loads the floating point register whose number is in a0 into register v0;
 */
#if _MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32

#define CP_1W(fr)				\
			mfc1	v0,fr;		\
			nop;			\
			dsll32	v0,0;		\
			dsrl32	v0,0;		\
			j	ra;	nop

#elif _MIPS_SIM == _MIPS_SIM_ABI32

#define CP_1W(fr)				\
			mfc1	v1,fr;		\
			move	v0,zero;	\
			j	ra;	nop
#endif

	PICOPT
	.text

LEAF(fpunit_fpload_s)
	sll	v0,a0,PTR_SCALESHIFT

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
cp_1w_fpr0:	CP_1W($f0)
cp_1w_fpr1:	CP_1W($f1)
cp_1w_fpr2:	CP_1W($f2)
cp_1w_fpr3:	CP_1W($f3)
cp_1w_fpr4:	CP_1W($f4)
cp_1w_fpr5:	CP_1W($f5)
cp_1w_fpr6:	CP_1W($f6)
cp_1w_fpr7:	CP_1W($f7)
cp_1w_fpr8:	CP_1W($f8)
cp_1w_fpr9:	CP_1W($f9)
cp_1w_fpr10:	CP_1W($f10)
cp_1w_fpr11:	CP_1W($f11)
cp_1w_fpr12:	CP_1W($f12)
cp_1w_fpr13:	CP_1W($f13)
cp_1w_fpr14:	CP_1W($f14)
cp_1w_fpr15:	CP_1W($f15)
cp_1w_fpr16:	CP_1W($f16)
cp_1w_fpr17:	CP_1W($f17)
cp_1w_fpr18:	CP_1W($f18)
cp_1w_fpr19:	CP_1W($f19)
cp_1w_fpr20:	CP_1W($f20)
cp_1w_fpr21:	CP_1W($f21)
cp_1w_fpr22:	CP_1W($f22)
cp_1w_fpr23:	CP_1W($f23)
cp_1w_fpr24:	CP_1W($f24)
cp_1w_fpr25:	CP_1W($f25)
cp_1w_fpr26:	CP_1W($f26)
cp_1w_fpr27:	CP_1W($f27)
cp_1w_fpr28:	CP_1W($f28)
cp_1w_fpr29:	CP_1W($f29)
cp_1w_fpr30:	CP_1W($f30)
cp_1w_fpr31:	CP_1W($f31)
	.set	reorder

END(fpunit_fpload_s)

