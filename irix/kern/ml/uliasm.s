/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1995, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.1 $"

#include <ml/ml.h>

#ifdef ULI

#if defined(R4000) || defined(R10000)
LEAF(uli_getasids)
	.set	noreorder
	MFC0	(v0, C0_TLBHI)
	j	ra
	andi	v0, 0xff
	END(uli_getasids)

LEAF(uli_setasids)
	.set	noreorder
	MTC0	(a0, C0_TLBHI)
	j	ra
	nop
	END(uli_setasids)
#endif	/* R4000 || R10000 */

#ifdef TFP
LEAF(uli_getasids)
	.set	noreorder
	ssnop	# protect from jal that got us here
	dmfc0	v0, C0_TLBHI
	dsrl	v0, TLBHI_PIDSHIFT
	andi	v0, 0xff
	ssnop	# ensure next dmfc0 is in next cycle
	dmfc0	t0, C0_ICACHE
	dsrl	t0, (40-8)	# shift to second byte position
	andi	t0, 0xff00
	j	ra
	or	v0, t0
	END(uli_getasids)

LEAF(uli_setasids)
	.set	noreorder
	andi	t0, a0, 0xff
	dsll	t0, TLBHI_PIDSHIFT
	ssnop	# protect from the jal that got us here
	dmtc0	t0, C0_TLBHI
	andi	a0, 0xff00
	dsll	a0, (40-8)
	ssnop	# protect from previous dmtc0
	dmtc0	a0, C0_ICACHE
	ssnop	# 2 ssnops to ensure things work if we do a jal on return
	j	ra
	ssnop
	END(uli_setasids)
#endif	/* TFP */

 # uli_eret(struct uli *uli, k_machreg_t sr)
 # "resume" into a user process as if we were returning from an
 # exception. In reality we are calling a function entry point
LEAF(uli_eret)

#ifdef TFP
	.set	reorder		# so the LI will interleave
	ld	t3, ULI_CONFIG(a0) # get new config register
	LI	t2, SR_IMASK
	.set	noreorder
	dmtc0	t3, C0_CONFIG	# set ULI config
	ld	t1, ULI_SR(a0)	# get ULI status register
	PTR_L	t9, ULI_PC(a0)	# load ULI handler func pointer
	and	t0, a1, t2	# isolate IMASK bits in kernel sr
	dmtc0	t9, C0_EPC	# set ULI handler entry point
	or	t1, t0		# set kernel IMASK bits in ULI sr
	PTR_L	sp, ULI_SP(a0)	# get ULI sp
	PTR_L	gp, ULI_GP(a0)	# get ULI gp
	dmtc0	t1, C0_SR	# set ULI sr
#endif /* TFP */

#ifdef R4000
	.set	noreorder
	ori	t0, a1, SR_EXL	# preset EXL to avoid KSU/EXL hazard
	ld	t1, ULI_SR(a0)	# get ULI status register
	mtc0	t0, C0_SR	# preset EXL
	PTR_L	t9, ULI_PC(a0)	# load ULI handler func pointer
	andi	t0, SR_IMASK	# isolate IMASK bits in kernel sr
	PTR_L	sp, ULI_SP(a0)	# get ULI sp
	or	t1, t0		# set kernel IMASK bits in ULI sr
	PTR_L	gp, ULI_GP(a0)	# get ULI gp
	dmtc0	t9, C0_EPC	# set ULI handler entry point
	mtc0	t1, C0_SR	# set ULI sr
#endif /* R4000 */

#ifdef R10000
	.set	noreorder
	ld	t1, ULI_SR(a0)	# get ULI status register
	andi	a1, SR_IMASK	# isolate IMASK bits in kernel sr
	PTR_L	t9, ULI_PC(a0)	# load ULI handler func pointer
	or	t1, a1		# set kernel IMASK bits in ULI sr
	PTR_L	sp, ULI_SP(a0)	# get ULI sp
	dmtc0	t9, C0_EPC	# set ULI handler entry point
	PTR_L	gp, ULI_GP(a0)	# get ULI gp
	mtc0	t1, C0_SR	# set ULI sr
#endif /* R10000 */

	PTR_SUBU sp, (4*SZREG)	# leave an arg save frame for ULI handler
#ifdef ULI_TSTAMP1
	ULI_GET_TS(t0, t1, TS_ERET, uli_saved_tstamps)
	.set	noreorder
#endif
	PTR_L	a0, ULI_FUNCARG(a0) # get ULI arg
#ifdef DEBUG
	li	ra, 0xbad1	# barf if ULI handler tries to return
#endif
	eret
	.set	reorder
	
	END(uli_eret)
#endif /* ULI */
