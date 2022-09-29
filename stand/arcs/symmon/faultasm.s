/*	
 *
 * faultasm.s -- symmon fault handling code
 *
 * "symmon/faultasm.s: $Revision: 1.90 $"
 *
 * symmon stack organization:
 *
 *                            ___
 * dbgstack[cpuid]---------->|   | Exception stack
 *                           |   |    ^
 *                           |   |    |
 *                           |   | EXSTKSZ
 *                           |   |    |
 *                           |   |    v
 *        Main symmon stack->|   |
 *                           | v |
 *                           |   |
 *
 * Modes:
 *	MODE_DBGMON
 *	MODE_CLIENT
 *	MODE_IDBG
 *
 * Fault algorithm:
 *
 * UTLB Miss->
 *		private._sv_k1 = k1  not on IP5
 *		private._sv_at = AT
 *		k1 = private
 *		private.dbgexc = EXCEPT_UTLB
 *		goto general exception
 *
 * Normal->
 *		private._sv_k1 = k1  not on IP5
 *		private._sv_at = AT
 *		k1 = private
 *		private.dbgexc = EXCEPT_NORM
 *		goto general exception
 *
 * breakpoint-> enter with k0 = AT
 *		private._sv_at = k0
 *		private._sv_k1 = k1  not on IP5
 *		k1 = private
 *		private.dbgexc = EXCEPT_BREAKPOINT
 *		goto general exception
 *
 * general exception-> enter with k1 = private
 *              if ( mode != MODE_CLIENT )
 *			sp = dbgstack[cpuid()];
 *		else
 *			sp = sp - E_SIZE;
 *
 *		 symmon exception frame built on stack
 *		sp[E_K1]      = private._sv_k1
 *		sp[E_GP]      = gp;
 *		sp[E_EPC]     = epc;
 *		sp[E_BADADDR] = bad addr;
 *		sp[E_CAUSE]   = cause;
 *		sp[E_GP]      = gp;
 *		sp[..]         = + some machine dependent error registers
 *
 *		private.dbgmodesav = private.dbgmode;
 *		private.dbgmode = MODE_DBGMON;
 *
 *		if (private.dbgmodesav != MODE_DBGMON) {
 *			private.dbgmode = MODE_DBGMON;
 *			private._regs[R_CAUSE] = C0_CAUSE;
 *			private._regs[R_K0] = 0xbad00bad;
 *			private._regs[R_K1] = 0xbad00bad  IP5 only
 *			...etc.
 *			if (private.dbgmodesav != MODE_IDBG)
 *				_save_vectors();
 *		}
 *		private.excstack = sp;
 *		symmon_exchandler(sp);
 *
 */

#include <sys/signal.h>
#include <regdef.h>
#include <asm.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <fault.h>
#include "mp.h"
#include "dbgmon.h"
#include "ml.h"

#ifdef MCCHIP
#define WBFLUSHM	\
	.set	noat	;\
	CLI	AT,PHYS_TO_COMPATK1(CPUCTRL0) ;\
	lw	zero,0(AT) ;\
	.set	at ;
#endif /* MCCHIP */

#if IP30 /* turn on count/compare interrupt checking here */
#define SRB_SCHEDCLK	SR_IBIT8
#endif

	.text
/*
 * symmon_brkhandler -- client fielded a breakpoint it doesn't want to handle
 *
 * kernel (and other clients) should restore all registers except AT and k0
 * k0 should be set to AT at time of breakpoint (the state of k0 can't be
 * saved, unfortunately), then the client should jump here by loading
 * AT with the address of "breakpoint" and jumping via AT
 */
LEAF(symmon_brkhandler)
	.set	noat
	move	AT,k0			# save at, so can use k0
	_get_gpda(k1,k0)		# k1 <-- pda
	PTR_L	k0,GPDA_REGS(k1)
	REG_S	AT,R_AT*SZREG(k0)	# save at in safe _reg 
	.set	at
	li	k0,EXCEPT_BRKPT		# save info, to know where I came from
	INT_S	k0,GPDA_DBG_EXC(k1)
	j	exception
	
/*
 * exception vector code
 * hook_exceptions() sets-up E_VEC to jump here
 */
EXPORT(symmon_exceptnorm)
	.set	noat
	_get_gpda(k1,k0)		# k1 <-- pda
	PTR_L	k0,GPDA_REGS(k1)
	REG_S	AT,R_AT*SZREG(k0)	# save at in safe _reg 
	.set	at
	li	k0,EXCEPT_NORM
	INT_S	k0,GPDA_DBG_EXC(k1)
	j	exception

/*
 * utlb miss
 * hook_exceptions() sets-up UT_VEC to jump here
 */
EXPORT(symmon_exceptutlb)
	.set	noat
	_get_gpda(k1,k0)		# k1 <-- pda
	PTR_L	k0,GPDA_REGS(k1)
	REG_S	AT,R_AT*SZREG(k0)	# save at in safe _reg 
	.set	at
	li	k0,EXCEPT_UTLB
	INT_S	k0,GPDA_DBG_EXC(k1)
	j	exception

#if R4000 || R10000
/*
 * xut miss
 * hook_exceptions() sets-up XUT_VEC to jump here
 */
EXPORT(symmon_exceptxut)
	.set	noat
	_get_gpda(k1,k0)		# k1 <-- pda
	PTR_L	k0,GPDA_REGS(k1)
	REG_S	AT,R_AT*SZREG(k0)	# save at in safe _reg 
	.set	at
	li	k0,EXCEPT_XUT
	INT_S	k0,GPDA_DBG_EXC(k1)
	j	exception

/*
 * cache error (ecc)
 * hook_exceptions() sets-up ECC_VEC to jump here
 */
EXPORT(symmon_exceptecc)
EXPORT(exceptecc)
	.set	noat
	_get_gpda(k1,k0)			# k1 <-- pda
	PTR_L	k0,GPDA_REGS(k1)
	REG_S	AT,R_AT*SZREG(k0)		# save at in safe _reg 
	.set	at
	li	k0,EXCEPT_ECC
	INT_S	k0,GPDA_DBG_EXC(k1)
	j	exception
#endif /* R4000 || R10000 */


/*
 * NMI
 * Symmon sets up a pointer in the GDA to jump here on NMI.
 */
EXPORT(symmon_nmi)
#ifdef IP28
EXPORT(ip28_nmi)
#endif
	.set	noat
	.set	noreorder
	LI	k0,SYMMON_SR
	MTC0(k0,C0_SR)
	NOP_1_4
	.set	reorder
	_get_gpda(k1,k0)		# k1 <-- pda
	PTR_L	k0,GPDA_REGS(k1)
	REG_S	AT,R_AT*SZREG(k0)	# save at in safe _reg


#if IP19	
	/* Need to restore EPCUART pointer into FP register 3 for POD */
	
	.set	noreorder
	LI	k0, SYMMON_SR|SR_CU1|SR_FR
	MTC0(k0,C0_SR)
	NOP_1_4
	
	ld	k0, pod_nmi_fp3
	mtc1	k0, $f3
	nop
	nop
	nop
	nop

	LI	k0, SYMMON_SR|SR_CU1|SR_FR
	MTC0(k0,C0_SR)
	NOP_1_4
	.set	reorder
	/* FP3 restored */
#endif /* IP19 */	
	
	.set	at
	li	k0,EXCEPT_NMI
	INT_S	k0,GPDA_DBG_EXC(k1)
	li	k0,MODE_CLIENT
	INT_S	k0,GPDA_DBG_MODE(k1)
	j	exception

/*
 * common exception handling code
 * come in with k1 pointing to appropriate pda
 * only has k0 to use at first
 * GPDA_DBG_EXC(k1) has exc vector(EXCEPT_UTLB, EXCEPT_NORM, EXCEPT_BRKPT)
 * if come in from client mode then save all regs into pda->_regs so symmon can
 * resume client execution later.
 * Rebuild the stack if coming in from client mode.
 * If fault while in symmon mode then use existing stack frame
 * 
 */
EXPORT(exception)
	/* figure out what mode we're in */
	INT_L	k0,GPDA_DBG_MODE(k1)
	beq	k0,MODE_CLIENT,1f		# if client , start from scratch
	/* from idbg,symmon mode */
	REG_S	sp,E_SP*SZREG-E_SIZE(sp)	# generate new stack frame
	PTR_SUBU sp,E_SIZE
	b	2f
1:					# from client mode */
	/* build a stack to run on */
	PTR_L	k0,GPDA_DBG_STACK(k1)
	REG_S	sp,E_SP*SZREG-E_SIZE(k0)	# save client sp in temp place
	move	sp,k0
	PTR_SUBU sp,E_SIZE
2:
	/* now that stack is set up, build an exception stack frame 
	 * for symmon */
	/* k1 is destroyed on multiprocessors */
#if !MULTIPROCESSOR
	REG_L	k0,GPDA_SV_K1(k1)
	REG_S	k0,E_K1*SZREG(sp)		# save k1
#endif /* !MULTIPROCESSOR */
	REG_S	gp,E_GP*SZREG(sp)		
	LA	gp,_gp
	.set	noreorder
	DMFC0(k0,C0_EPC)
	NOP_1_4
	REG_S	k0,E_EPC*SZREG(sp)
#if IP19 || IP25 || IP27 || IP28 || IP30
	# Only platforms that support NMI do this.
	# Other platforms may redefine E_ERREPC.
	DMFC0(k0,C0_ERROR_EPC)
	NOP_1_4
	REG_S	k0,E_ERREPC*SZREG(sp)
#endif	/* IP19 || IP25 || IP28 || IP30 */
	MFC0(k0,C0_SR)
	NOP_1_4
	REG_S	k0,E_SR*SZREG(sp)
	DMFC0(k0,C0_BADVADDR)
	NOP_1_4
	REG_S	k0,E_BADVADDR*SZREG(sp)
	MFC0(k0,C0_CAUSE)
	NOP_1_4
	REG_S	ra,E_RA*SZREG(sp)
	.set	reorder
	REG_S	k0,E_CAUSE*SZREG(sp)

#if IP20 || IP22 || IP26 || IP28
	CLI	k0,PHYS_TO_COMPATK1(CPU_ERR_STAT)
	lw	k0,0(k0)
	and	k0,k0,CPU_ERR_STAT_PAR_MASK
	REG_S	k0,E_CPU_PARERR*SZREG(sp)
	CLI	k0,PHYS_TO_COMPATK1(GIO_ERR_STAT)
	lw	k0,0(k0)
	REG_S	k0,E_GIO_PARERR*SZREG(sp)
	CLI	k0,PHYS_TO_COMPATK1(CPU_ERR_ADDR)
	lw	k0,0(k0)
	REG_S	k0,E_CPUADDR*SZREG(sp)
	CLI	k0,PHYS_TO_COMPATK1(GIO_ERR_ADDR)
	lw	k0,0(k0)
	REG_S	k0,E_GIOADDR*SZREG(sp)
#if IP22 
	IS_IOC1(k0)
	beqz	k0, 1f			# branch if not IOC1 chip

	# with IOC1/INT3
	CLI	k0, PHYS_TO_COMPATK1(LIO_0_ISR_OFFSET+HPC3_INT3_ADDR)
	lbu	k0, 0(k0)
	REG_S	k0, E_LIOINTR0*SZREG(sp)
	LI	k0, PHYS_TO_COMPATK1(LIO_1_ISR_OFFSET+HPC3_INT3_ADDR)
	b	2f
1:
	# with INT2
	CLI	k0, PHYS_TO_COMPATK1(LIO_0_ISR_OFFSET+HPC3_INT2_ADDR)
	lbu	k0, 0(k0)
	REG_S	k0, E_LIOINTR0*SZREG(sp)
	CLI	k0, PHYS_TO_COMPATK1(LIO_1_ISR_OFFSET+HPC3_INT2_ADDR)
2:
	lbu	k0,0(k0)
	REG_S	k0,E_LIOINTR1*SZREG(sp)
#endif
#if IP26 || IP28
	# with INT2
	LI	k0, K1BASE+HPC3_INT2_ADDR
	.set	noat
	lbu	AT,LIO_0_ISR_OFFSET(k0)
	REG_S	AT,E_LIOINTR0*SZREG(sp)
	lbu	AT,LIO_1_ISR_OFFSET(k0)
	REG_S	AT,E_LIOINTR1*SZREG(sp)
#if IP26
	lbu	AT,LIO_2_3_ISR_OFFSET(k0)
	REG_S	AT,E_LIOINTR2*SZREG(sp)
	.set	at
	# TCC registers
	LI	k0, K1BASE+TCC_BASE
	.set	noat
	ld	AT,TCC_INTR-TCC_BASE(k0)
	REG_S	AT,E_TCC_INTR*SZREG(sp)
	ld	AT,TCC_BE_ADDR-TCC_BASE(k0)
	REG_S	AT,E_TCC_BE_ADDR*SZREG(sp)
	ld	AT,TCC_PARITY-TCC_BASE(k0)
	REG_S	AT,E_TCC_PARITY*SZREG(sp)
	ld	AT,TCC_ERROR-TCC_BASE(k0)
	REG_S	AT,E_TCC_ERROR*SZREG(sp)
#endif	/* IP26 */
	.set	at
#endif	/* IP26 || IP28 */
#if IP20
	li	k0, LIO_0_ISR_ADDR+K1BASE
	lbu	k0, 0(k0)
	REG_S	k0, E_LIOINTR0*SZREG(sp)
	li	k0, LIO_1_ISR_ADDR+K1BASE
	lbu	k0,0(k0)
	REG_S	k0,E_LIOINTR1*SZREG(sp)
#endif
	sw	zero,PHYS_TO_K1(CPU_ERR_STAT)	# clear bus error
	sw	zero,PHYS_TO_K1(GIO_ERR_STAT)	# clear bus error
	.set 	noreorder
	WBFLUSHM
	.set 	reorder
#endif /* IP20 || IP22 || IP26 || IP28 */
#if IP32
        LI     k0,PHYS_TO_K1(CRM_CPU_ERROR_ADDR)
        ld      k0,0(k0)
        REG_S   k0,E_CRM_CPU_ERROR_ADDR*SZREG(sp)
        LI     k0,PHYS_TO_K1(CRM_CPU_ERROR_STAT)
        ld      k0,0(k0)
        REG_S   k0,E_CRM_CPU_ERROR_STAT*SZREG(sp)
        LI     k0,PHYS_TO_K1(CRM_VICE_ERROR_ADDR)
        ld      k0,0(k0)
        REG_S   k0,E_CRM_CPU_ERROR_VICE_ERR_ADDR*SZREG(sp)
        LI     k0,PHYS_TO_K1(CRM_MEM_ERROR_STAT)
        ld      k0,0(k0)
        REG_S   k0,E_CRM_MEM_ERROR_STAT*SZREG(sp)
        LI     k0,PHYS_TO_K1(CRM_MEM_ERROR_ADDR)
        ld      k0,0(k0)
        REG_S   k0,E_CRM_MEM_ERROR_ADDR*SZREG(sp)
        LI     k0,PHYS_TO_K1(CRM_MEM_ERROR_ECC_SYN)
        ld      k0,0(k0)
        REG_S   k0,E_CRM_MEM_ERROR_ECC_SYN*SZREG(sp)
        LI     k0,PHYS_TO_K1(CRM_MEM_ERROR_ECC_CHK)
        ld      k0,0(k0)
        REG_S   k0,E_CRM_MEM_ERROR_ECC_CHK*SZREG(sp)
        LI     k0,PHYS_TO_K1(CRM_MEM_ERROR_ECC_REPL)
        ld      k0,0(k0)
        REG_S   k0,E_CRM_MEM_ERROR_ECC_REPL*SZREG(sp)
#endif /* IP32 */

	.set noreorder
	/* Now that SR is saved, we ought to put in the usual value.
	 * That way, we'll be able to get better information earlier
	 * on if symmon runs into problems.
	 * NOTE: Need to preserve some of the SR bits (like DE, in order
	 * to avoid another cache error exception when debugging this code)
	 */
	.set	noat
	LI	AT,SYMMON_SR_KEEP
	MFC0(k0,C0_SR)
	and	k0,AT
	LI	AT,SYMMON_SR
	or	k0,AT
	MTC0(k0,C0_SR)
	NOP_1_4
	.set	at
	.set reorder

	INT_L	k0,GPDA_DBG_MODE(k1)	# dbg mode flag
	INT_S	k0,GPDA_DBG_MODESAV(k1)
	beq	k0,MODE_DBGMON,nosave
	/*
	 * Only save registers if in client mode
	 * then change mode to prom mode
	 * using k0,k1,v0
	 */
save:
	/* save all regs into safe place, pointed to by GPDA_REGS */
	li	k0,MODE_DBGMON
	INT_S	k0,GPDA_DBG_MODE(k1)	# set flag to indicate symmon mode
	PTR_L	k0,GPDA_REGS(k1)	# k0 now has &_regs[0] , k1 has pda
	REG_S	v0,R_V0*SZREG(k0)
/* k1 is destroyed on multiprocessors */
#if !MULTIPROCESSOR
	REG_L	v0,E_K1*SZREG(sp)	# recall k1
	REG_S	v0,R_K1*SZREG(k0)
#endif /* !MULTIPROCESSOR */
	REG_L	v0,E_GP*SZREG(sp)	# recall client gp
	REG_S	v0,R_GP*SZREG(k0)
	.set	noreorder
	DMFC0(v0,C0_EPC)
	NOP_1_4
	.set	reorder
	REG_S	v0,R_EPC*SZREG(k0)
	REG_L	v0,E_SR*SZREG(sp)
	REG_S	v0,R_SR*SZREG(k0)
	INT_L	v0,GPDA_DBG_EXC(k1)
	REG_S	v0,R_EXCTYPE*SZREG(k0)
	.set	noreorder
	DMFC0(v0,C0_BADVADDR)
	NOP_1_4
	CACHE_BARRIER_AT(R_BADVADDR*SZREG,k0)
	REG_S	v0,R_BADVADDR*SZREG(k0)
#if R4000 || R10000
	/* need to save C0_COUNT (which continues to increment) BEFORE we
	 * save the C0_CAUSE (which contains the "clock compare" interrupt)
	 * otherwise we may miss a clock interrupt upon return to the code
	 * which is being debugged (clock interrupt may occur after we save
	 * the CAUSE but before we read the COUNT, and we end up restoring
	 * the later COUNT value).
	 */
	MFC0(v0,C0_COUNT)
	NOP_1_4
	REG_S	v0,R_COUNT*SZREG(k0)
#endif /* R4000 || R10000 */
	.set	noreorder
	MFC0(v0,C0_CAUSE)
	NOP_1_4
	.set	reorder
	REG_S	v0,R_CAUSE*SZREG(k0)
	REG_L	v0,E_SP*SZREG(sp)
	REG_S	v0,R_SP*SZREG(k0)
	REG_S	zero,R_ZERO*SZREG(k0)	# we don't trust anything
	REG_S	v1,R_V1*SZREG(k0)
	REG_S	a0,R_A0*SZREG(k0)
	REG_S	a1,R_A1*SZREG(k0)
	REG_S	a2,R_A2*SZREG(k0)
	REG_S	a3,R_A3*SZREG(k0)
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	REG_S	t0,R_T0*SZREG(k0)
	REG_S	t1,R_T1*SZREG(k0)
	REG_S	t2,R_T2*SZREG(k0)
	REG_S	t3,R_T3*SZREG(k0)
	REG_S	t4,R_T4*SZREG(k0)
	REG_S	t5,R_T5*SZREG(k0)
	REG_S	t6,R_T6*SZREG(k0)
	REG_S	t7,R_T7*SZREG(k0)
#elif (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _ABIN32)
	REG_S	a4,R_A4*SZREG(k0)
	REG_S	a5,R_A5*SZREG(k0)
	REG_S	a6,R_A6*SZREG(k0)
	REG_S	a7,R_A7*SZREG(k0)
	REG_S	t0,R_T0*SZREG(k0)
	REG_S	t1,R_T1*SZREG(k0)
	REG_S	t2,R_T2*SZREG(k0)
	REG_S	t3,R_T3*SZREG(k0)
#else
	<<<BOMB>>>
#endif
	REG_S	s0,R_S0*SZREG(k0)
	REG_S	s1,R_S1*SZREG(k0)
	REG_S	s2,R_S2*SZREG(k0)
	REG_S	s3,R_S3*SZREG(k0)
	REG_S	s4,R_S4*SZREG(k0)
	REG_S	s5,R_S5*SZREG(k0)
	REG_S	s6,R_S6*SZREG(k0)
	REG_S	s7,R_S7*SZREG(k0)
	REG_S	t8,R_T8*SZREG(k0)
	REG_S	t9,R_T9*SZREG(k0)
	li	v0,0xbad00bad
	REG_S	v0,R_K0*SZREG(k0)	# make it obvious we can't save this
#if MULTIPROCESSOR
	REG_S	v0,R_K1*SZREG(k0)
#endif /* MULTIPROCESSOR */
	REG_S	fp,R_FP*SZREG(k0)
	REG_S	ra,R_RA*SZREG(k0)
	mflo	v0
	REG_S	v0,R_MDLO*SZREG(k0)
	mfhi	v0
	REG_S	v0,R_MDHI*SZREG(k0)
	.set	noreorder
	AUTO_CACHE_BARRIERS_DISABLE	# mfc0's will serialize w/o cache ops
#if !TFP
	MFC0(v0,C0_INX)
	NOP_1_4
	REG_S	v0,R_INX*SZREG(k0)
	MFC0(v0,C0_RAND)		# save just to see it change
	NOP_1_4
	REG_S	v0,R_RAND*SZREG(k0)
	DMFC0(v0,C0_CTXT)
	NOP_1_4
	REG_S	v0,R_CTXT*SZREG(k0)
#endif	/* !TFP */
	DMFC0(v0,C0_TLBLO)
	NOP_1_4
	REG_S	v0,R_TLBLO*SZREG(k0)
	DMFC0(v0,C0_TLBHI)
	NOP_1_4
	REG_S	v0,R_TLBHI*SZREG(k0)
#if R4000 || R10000
	DMFC0(v0,C0_TLBLO_1)
	NOP_1_4
	REG_S	v0,R_TLBLO1*SZREG(k0)
	MFC0(v0,C0_PGMASK)
	NOP_1_4
	REG_S	v0,R_PGMSK*SZREG(k0)
	MFC0(v0,C0_TLBWIRED)
	NOP_1_4
	REG_S	v0,R_WIRED*SZREG(k0)
	MFC0(v0,C0_COMPARE)
	NOP_1_4
	REG_S	v0,R_COMPARE*SZREG(k0)
	MFC0(v0,C0_LLADDR)
	NOP_1_4
	REG_S	v0,R_LLADDR*SZREG(k0)
#ifdef R4600
	mfc0	v0,C0_PRID
	NOP_1_4
	andi	v0,0xFF00
	sub	v0,0x2000
	bnez	v0,96f
	nop
	sub	v0,(0x2100-0x2000)
	beqz	v0,96f			#br if R4700
	nop
	sub	v0,(0x2300-0x2100)
	beqz	v0,96f			#br if R5000
	nop
	sub	v0,(0x2800-0x2300)
	bnez	v0,97f			#br if not RM5271
	nop

96:	REG_S	zero,R_WATCHLO*SZREG(k0)
	REG_S	zero,R_WATCHHI*SZREG(k0)	
	b	98f
	nop
97:
#endif
	MFC0(v0,C0_WATCHLO)
	NOP_1_4
	REG_S	v0,R_WATCHLO*SZREG(k0)
	MFC0(v0,C0_WATCHHI)
	NOP_1_4
	REG_S	v0,R_WATCHHI*SZREG(k0)
#ifdef R4600
98:
#endif
#ifndef SABLE			/* currently not supported by sable. */
	DMFC0(v0,C0_EXTCTXT)
	NOP_1_4
	REG_S	v0,R_EXTCTXT*SZREG(k0)
	MFC0(v0,C0_ECC)
	NOP_1_4
	REG_S	v0,R_ECC*SZREG(k0)
#endif
	MFC0(v0,C0_CACHE_ERR)
	NOP_1_4
	REG_S	v0,R_CACHERR*SZREG(k0)
	MFC0(v0,C0_TAGLO)
	NOP_1_4
	REG_S	v0,R_TAGLO*SZREG(k0)
	MFC0(v0,C0_TAGHI)
	NOP_1_4
	REG_S	v0,R_TAGHI*SZREG(k0)
	DMFC0(v0,C0_ERROR_EPC)
	NOP_1_4
	REG_S	v0,R_ERREPC*SZREG(k0)
	MFC0(v0,C0_CONFIG)
	NOP_1_4
	REG_S	v0,R_CONFIG*SZREG(k0)
#endif /* R4000 || R10000 */
#if TFP
	MFC0(v0,C0_TLBSET)
	REG_S	v0,R_TLBSET*SZREG(k0)

	MFC0(v0,C0_UBASE)
	REG_S	v0,R_UBASE*SZREG(k0)

	MFC0(v0,C0_SHIFTAMT)
	REG_S	v0,R_SHIFTAMT*SZREG(k0)

	MFC0(v0,C0_TRAPBASE)
	REG_S	v0,R_TRAPBASE*SZREG(k0)

	MFC0(v0,C0_BADPADDR)
	REG_S	v0,R_BADPADDR*SZREG(k0)

	MFC0(v0,C0_COUNT)
	REG_S	v0,R_COUNT*SZREG(k0)

	MFC0(v0,C0_PRID)
	REG_S	v0,R_PRID*SZREG(k0)

	MFC0(v0,C0_CONFIG)
	REG_S	v0,R_CONFIG*SZREG(k0)

	MFC0(v0,C0_WORK0)
	REG_S	v0,R_WORK0*SZREG(k0)

	MFC0(v0,C0_WORK1)
	REG_S	v0,R_WORK1*SZREG(k0)

	MFC0(v0,C0_PBASE)
	REG_S	v0,R_PBASE*SZREG(k0)

	MFC0(v0,C0_GBASE)
	REG_S	v0,R_GBASE*SZREG(k0)

	MFC0(v0,C0_WIRED)
	REG_S	v0,R_WIRED*SZREG(k0)

	MFC0(v0,C0_DCACHE)
	REG_S	v0,R_DCACHE*SZREG(k0)

	MFC0(v0,C0_ICACHE)
	REG_S	v0,R_ICACHE*SZREG(k0)
#endif
	.set	reorder
#if IP22
	jal	_r4600sc_cache_on_test	# was 4600sc cache on?
	lw	t0,_r4600sc_cache_on
	beq	t0,zero,99f
	jal	_r4600sc_disable_scache	# if it was, turn it off
99:
#endif
	AUTO_CACHE_BARRIERS_ENABLE	# done with block of C0 stores
	# if from client code executing on our behalf don't touch vectors
	INT_L	v0,GPDA_DBG_MODESAV(k1)
	beq	v0,MODE_IDBG,nosave	# was in idbg mode
#if IP32
	jal     _ip32_disable_serial_dma # and save state
	.set    noreorder
	mfc0    t0,C0_PRID
	NOP_0_4
	.set reorder
	andi    t0,C0_IMPMASK
	subu    t0,(C0_IMP_R5000 << C0_IMPSHIFT)
	beq     t0,zero,2f
	subu    t0,((C0_IMP_RM5271 << C0_IMPSHIFT) - (C0_IMP_R5000 << C0_IMPSHIFT))
	beq     t0,zero,2f
	.set noreorder
	mfc0    t0,C0_CONFIG
	NOP_0_4
	.set reorder
	sw	zero,_ip32_config_reg2
	sw      t0,_ip32_config_reg
	andi    t0,CONFIG_K0
	subu    t0,CONFIG_NONCOHRNT
	blez    t0,2f                   # if K0 uncached or cached non-coherent leave alone
	lw      t0,_ip32_config_reg
	li      t1,~CONFIG_K0
	and     t0,t1
	ori     t0,CONFIG_NONCOHRNT     #else set K0 to cached non-coherent
	sw	t0,_ip32_config_reg2	# for debugging
	.set noreorder
	mtc0    t0,C0_CONFIG
	NOP_0_4
	.set reorder
2:

#endif
#if IP26 || IP28
	li	v0,EXCEPT_BRKPT		# only switch mode on breakpoints
	INT_L	v1,GPDA_DBG_EXC(k1)
	bne	v0,v1,99f
#if IP26
	jal	ip26_enable_ucmem	# turn-on uncached mode if needed
	sw	v0,ip26_ucmem		# if comming from slow skip save
	sd	v1,tcc_gcache		# save kernel gcache state
#else	/* !IP26 */
	jal     ip28_enable_ucmem       # turn-on uncached mode if needed
        sw      v0,ip28_ucmem           # if comming from slow skip save
#endif	/* IP26 */
99:
#endif	/* IP26 || IP28 */
	/* now that all regs have been saved */
/* XXX	jal	_remove_brkpts		# reinstall original code*/
	jal	_save_vectors		# save user vectors
nosave:
	PTR_S	sp,GPDA_EXC_STACK(k1)
	/* set up to pass exception frame pointer */
	move	a0,sp
	j	symmon_exchandler
	END(symmon_brkhandler)

/*
 * _resume_brkpt -- resume execution of client code
 */
LEAF(_resume_brkpt)
#ifdef NETDBX
	.globl	resumenwk
	.set	noreorder
	nop
	jal	resumenwk
	nop
	.set	reorder
#endif /* NETDBX */
#if IP22
	lw	t0,_r4600sc_cache_on	
	beq	t0,zero,1f
	jal	_r4600sc_enable_scache	# restore 4600sc cache state
1:
#endif
	_get_gpda(k1,k0)		# k1 <- pda
	INT_L	v0,GPDA_DBG_MODESAV(k1)
	bne	v0,MODE_CLIENT,norestore	# restore only if to client
#if IP26
	lw	a0,ip26_ucmem
	ld	a1,tcc_gcache			# get returning mode
	jal	ip26_return_gcache_ucmem
#endif
#if IP28
        lw      a0,ip28_ucmem
        jal     ip28_return_ucmem
	/* clear any MC sysad errors.  (probably) from speculative loads */
	CLI	v0,PHYS_TO_COMPATK1(CPU_ERR_STAT)
	lw	v1,0(v0)
	and	v1,CPU_ERR_STAT_ADDR
	beqz	v1,nomcerr
	sw	zero,0(v0)			# clear error
	lw	zero,0(v0)			# flushbus
nomcerr:
#endif	/* IP28 */
#if IP32
        jal     _ip32_restore_serial_dma # and save state
        .set    noreorder
        mfc0    t0,C0_PRID
        NOP_0_4
        .set reorder
        andi    t0,C0_IMPMASK
        subu    t0,(C0_IMP_R5000 << C0_IMPSHIFT)
        beq     t0,zero,2f
	subu    t0,((C0_IMP_RM5271 << C0_IMPSHIFT) - (C0_IMP_R5000 << C0_IMPSHIFT))
        beq     t0,zero,2f
        lw      t0,_ip32_config_reg
        .set noreorder
        mtc0    t0,C0_CONFIG
        NOP_0_4
	.set reorder
2:

#endif
	jal	_restore_vectors	# restore user vectors
/* XXX	jal	_install_brkpts		# save code and insert BRKPTS*/
norestore:				# use k0,k1,v0 to restore
	_get_gpda(k1,k0)		# k1 <- pda
	INT_L	v0,GPDA_DBG_MODESAV(k1)	# go back to previous mode
	INT_S	v0,GPDA_DBG_MODE(k1)
	PTR_L	k1,GPDA_REGS(k1)
	REG_L	a0,R_A0*SZREG(k1)
	REG_L	a1,R_A1*SZREG(k1)
	REG_L	a2,R_A2*SZREG(k1)
	REG_L	a3,R_A3*SZREG(k1)
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	REG_L	t0,R_T0*SZREG(k1)
	REG_L	t1,R_T1*SZREG(k1)
	REG_L	t2,R_T2*SZREG(k1)
	REG_L	t3,R_T3*SZREG(k1)
	REG_L	t4,R_T4*SZREG(k1)
	REG_L	t5,R_T5*SZREG(k1)
	REG_L	t6,R_T6*SZREG(k1)
	REG_L	t7,R_T7*SZREG(k1)
#elif (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _ABIN32)
	REG_L	a4,R_A4*SZREG(k1)
	REG_L	a5,R_A5*SZREG(k1)
	REG_L	a6,R_A6*SZREG(k1)
	REG_L	a7,R_A7*SZREG(k1)
	REG_L	t0,R_T0*SZREG(k1)
	REG_L	t1,R_T1*SZREG(k1)
	REG_L	t2,R_T2*SZREG(k1)
	REG_L	t3,R_T3*SZREG(k1)
#else
	<<<BOMB>>>
#endif
	REG_L	s0,R_S0*SZREG(k1)
	REG_L	s1,R_S1*SZREG(k1)
	REG_L	s2,R_S2*SZREG(k1)
	REG_L	s3,R_S3*SZREG(k1)
	REG_L	s4,R_S4*SZREG(k1)
	REG_L	s5,R_S5*SZREG(k1)
	REG_L	s6,R_S6*SZREG(k1)
	REG_L	s7,R_S7*SZREG(k1)
	REG_L	t8,R_T8*SZREG(k1)
	REG_L	t9,R_T9*SZREG(k1)
	REG_L	gp,R_GP*SZREG(k1) 
	REG_L	fp,R_FP*SZREG(k1)
	REG_L	ra,R_RA*SZREG(k1)
	REG_L	v0,R_MDLO*SZREG(k1)
	mtlo	v0
	REG_L	v1,R_MDHI*SZREG(k1)
	mthi	v1
	.set	noreorder
#if !TFP
	REG_L	v0,R_INX*SZREG(k1)
	NOP_1_4
	MTC0(v0,C0_INX)
	REG_L	v1,R_CTXT*SZREG(k1)
	NOP_1_4
	DMTC0(v1,C0_CTXT)
#endif
	REG_L	v1,R_TLBLO*SZREG(k1)
	NOP_1_4
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	DMTC0(v1,C0_TLBLO)
	REG_L	v0,R_TLBHI*SZREG(k1)
	NOP_1_4
	DMTC0(v0,C0_TLBHI)
	REG_L	v0,R_CAUSE*SZREG(k1)
	NOP_1_4
	MTC0(v0,C0_CAUSE)		# for software interrupts
#if R4000 || R10000
	/* need to restore C0_COMPARE before restoring C0_COUNT since storing
	 * into C0_COMPARE will clear a pending clock interrupt.
	 */
	REG_L	v1,R_COMPARE*SZREG(k1)
	NOP_1_4
	MTC0(v1,C0_COMPARE)

#if defined(SRB_SCHEDCLK)
	/* On Everest & SN0 machines, COUNT/COMPARE registers are used
	 * to generate scheduling clock interrupts.  This is bit SRB_SCHEDCLK
	 * (CAUSE_IP8) in the CAUSE register.  Other platforms can use
	 * this bit as an external interrupt, so we assume the fixup
	 * is only needed when SRB_SCHEDCLK is defined.
	 *
	 * This code is on for IP30 now as well.
	 */
	
	/* If there was a clock interrupt pending, we need to generate a
	 * new clock interrupt.
	 */
	andi	v0, SRB_SCHEDCLK
	beq	v0,zero,2f
	nop

	/* to generate a clock interrupt, just back up the count register
	 * a few cycles.  Needs to complete the interrupt before we
	 * restore the original C0_COUNT.
	 */
	addi	v1,-20
	NOP_1_4
	MTC0(v1,C0_COUNT)
	
	/* loop waiting for interrupt to appear */
	
1:	NOP_1_4	
	MFC0(v1,C0_CAUSE)
	NOP_1_4
	andi	v1,SRB_SCHEDCLK
	beq	v1,zero,1b
	nop
2:
#endif /* SRB_SCHEDCLK */	
#endif /* R4000 || R10000 */
	REG_L	v1,R_SR*SZREG(k1)
	NOP_1_4
	MTC0(v1,C0_SR)
	NOP_1_4
	REG_L	v1,R_EPC*SZREG(k1)
	NOP_1_4
	DMTC0(v1,C0_EPC)
#if R4000 || R10000
	REG_L	v1,R_TLBLO1*SZREG(k1)
	NOP_1_4
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	DMTC0(v1,C0_TLBLO_1)
	REG_L	v1,R_PGMSK*SZREG(k1)
	NOP_1_4
	MTC0(v1,C0_PGMASK)
	REG_L	v1,R_WIRED*SZREG(k1)
	NOP_1_4
	MTC0(v1,C0_TLBWIRED)
	REG_L	v1,R_COUNT*SZREG(k1)
	NOP_1_4
	MTC0(v1,C0_COUNT)
#ifdef R4600
	nop
	mfc0	v1,C0_PRID
	NOP_1_4
	andi	v1,0xFF00
	sub	v1,0x2000
	beqz	v1,98f
	nop
	sub	v1,(0x2100-0x2000)
	beqz	v1,98f			#br if R4700
	nop
	sub	v1,(0x2300-0x2100)
	beqz	v1,98f			#br if R5000
	nop
	sub	v1,(0x2800-0x2300)
	beqz	v1,98f			#br if RM5271
	nop
#endif
	REG_L	v1,R_WATCHLO*SZREG(k1)
	NOP_1_4
	MTC0(v1,C0_WATCHLO)
	REG_L	v1,R_WATCHHI*SZREG(k1)
	NOP_1_4
	MTC0(v1,C0_WATCHHI)
#ifdef R4600
98:
#endif
	REG_L	v1,R_TAGLO*SZREG(k1)
	NOP_1_4
	MTC0(v1,C0_TAGLO)
	NOP_1_4
#endif /* R4000 || R10000 */
	.set	reorder
	REG_L	sp,R_SP*SZREG(k1)
	REG_L	v1,R_V1*SZREG(k1)
	REG_L	v0,R_V0*SZREG(k1)
	REG_L	k0,R_EPC*SZREG(k1)
	.set	noat
	REG_L	AT,R_AT*SZREG(k1)
	.set	at
	REG_L	k1,R_K1*SZREG(k1)
	.set	noreorder
	eret
	nop
	.set	reorder
	END(_resume_brkpt)

/*
 * invoke(procedure, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
 * interface for call command to client code
 * copies arguments to new frame and sets up gp for client
 */

/* Carefully do not use the asm.h macros that are based on
 * _MIPS_ISA here - since 32bit EVEREST assembler is compiled
 * with -mip3 -32bit, this causes those macros to expand to the
 * _MIPS_ISA_MIPS3 definitions.
 * Instead, use the definitions from arcs/include/ml.h.
 */

#if defined(IP32)
LOCALSZ=11			#  8 arguments, ra, gp. 
#define A0_OFF	INVOKEFRM-(3*BPREG)
#define A1_OFF	INVOKEFRM-(4*BPREG)
#define A2_OFF	INVOKEFRM-(5*BPREG)
#define A3_OFF	INVOKEFRM-(6*BPREG)
#define A4_OFF	INVOKEFRM-(7*BPREG)
#define A5_OFF	INVOKEFRM-(8*BPREG)
#define A6_OFF	INVOKEFRM-(9*BPREG)
#define A7_OFF	INVOKEFRM-(10*BPREG)
#define V0_OFF	INVOKEFRM-(11*BPREG)

#elif (_MIPS_SIM == _ABIO32)
LOCALSZ=10			#  8 arguments, ra, gp
#elif (_MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32)
LOCALSZ=2			# gp, ra
#else
<<BOMB>>
#endif

INVOKEFRM=	FRAMESZ(LOCALSZ*BPREG)
#define GP_OFF	INVOKEFRM-(2*BPREG)
#define RA_OFF	INVOKEFRM-(1*BPREG)

NESTED(invoke, INVOKEFRM, zero)
	PTR_SUBU sp,INVOKEFRM
	sreg	ra,RA_OFF(sp)
	sreg	gp,GP_OFF(sp)
	move	v0,a0
	move	a0,a1
	move	a1,a2
	move	a2,a3
#if (_MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32)
	move	a3,a4
	move	a4,a5
	move	a5,a6
	move	a6,a7
	lreg	a7,INVOKEFRM(sp)
#elif (_MIPS_SIM == _MIPS_SIM_ABI32)
	lw	a3,INVOKEFRM+(4*4)(sp)
	lw	v1,INVOKEFRM+(5*4)(sp)
	sw	v1,4*4(sp)
	lw	v1,INVOKEFRM+(6*4)(sp)
	sw	v1,5*4(sp)
	lw	v1,INVOKEFRM+(7*4)(sp)
	sw	v1,6*4(sp)
	lw	v1,INVOKEFRM+(8*4)(sp)
	sw	v1,7*4(sp)
#else
<<BOMB>>
#endif	/* _MIPS_SIM */
#if IP22
	lw	t0,_r4600sc_cache_on		# was 4600sc cache on?
	beq	t0,zero,1f
	sreg	v0
	jal	_r4600sc_enable_scache		# if so, turn it back on now
1:
#endif
#if IP32
	sreg	a0,A0_OFF(sp)
	sreg	a1,A1_OFF(sp)
	sreg	a2,A2_OFF(sp)
	sreg	a3,A3_OFF(sp)
	sreg	a4,A4_OFF(sp)
	sreg	a5,A5_OFF(sp)
	sreg	a6,A6_OFF(sp)
	sreg	a7,A7_OFF(sp)
	sreg	v0,V0_OFF(sp)
        jal     _ip32_restore_serial_dma
	lreg	a0,A0_OFF(sp)
	lreg	a1,A1_OFF(sp)
	lreg	a2,A2_OFF(sp)
	lreg	a3,A3_OFF(sp)
	lreg	a4,A4_OFF(sp)
	lreg	a5,A5_OFF(sp)
	lreg	a6,A6_OFF(sp)
	lreg	a7,A7_OFF(sp)
	lreg	v0,V0_OFF(sp)
        .set    noreorder
        mfc0    t0,C0_PRID
        NOP_0_4
        .set reorder
        andi    t0,C0_IMPMASK
        subu    t0,(C0_IMP_R5000 << C0_IMPSHIFT)
        beq     t0,zero,2f
	subu    t0,((C0_IMP_RM5271 << C0_IMPSHIFT) - (C0_IMP_R5000 << C0_IMPSHIFT))
        beq     t0,zero,2f
        lw      t0,_ip32_config_reg
        .set noreorder
        mtc0    t0,C0_CONFIG
        NOP_0_4
	.set reorder
2:

#endif
	_get_gpda(k1,k0)			# k1 <- pda
	li	k0,MODE_CLIENT
	INT_S	k0,GPDA_DBG_MODE(k1)		# entering client mode
	PTR_L	k0,GPDA_REGS(k1)
	REG_L	gp,R_GP*SZREG(k0)
	jal	v0
#if IP22
	jal	_r4600sc_cache_on_test		# when returning check if 4600sc on
	lw	t0,_r4600sc_cache_on
	beq	t0,zero,2f			# and if it is, turn it off
	jal	_r4600sc_disable_scache
2:
#endif
#if IP32
	sreg	v0,V0_OFF(sp)	# _ip32_disable_serial_dma clobbers v0, so save
        jal     _ip32_disable_serial_dma # and save state
	lreg	v0,V0_OFF(sp)   # restore value of v0
	.set    noreorder
	mfc0    t0,C0_PRID
	NOP_0_4
	.set reorder
	andi    t0,C0_IMPMASK
	subu    t0,(C0_IMP_R5000 << C0_IMPSHIFT)
	beq     t0,zero,2f
	subu    t0,((C0_IMP_RM5271 << C0_IMPSHIFT) - (C0_IMP_R5000 << C0_IMPSHIFT))
        beq     t0,zero,2f
	.set noreorder
	mfc0    t0,C0_CONFIG
	NOP_0_4
	.set reorder
	sw	zero,_ip32_config_reg2
	sw      t0,_ip32_config_reg
	andi    t0,CONFIG_K0
	subu    t0,CONFIG_NONCOHRNT
	blez    t0,2f                   # if K0 uncached or cached non-coherent leave alone
	lw      t0,_ip32_config_reg
	li      t1,~CONFIG_K0
	and     t0,t1
	ori     t0,CONFIG_NONCOHRNT    # else set K0 to cached non-coherent
	sw	t0,_ip32_config_reg2	# for debugging
	.set noreorder
	mtc0    t0,C0_CONFIG
	NOP_0_4
	.set reorder
2:
#endif

	lreg	gp,GP_OFF(sp)
	lreg	ra,RA_OFF(sp)
	PTR_ADDU sp,INVOKEFRM
	li	t0,MODE_DBGMON
	INT_S	t0,GPDA_DBG_MODE(k1)		# now in prom mode
	j	ra
	END(invoke)
/*
 * symmon_spl() -- reestablish desired symmon status register
 * clear any pending write bus error interrupts
 * returns current sr [well not really, typed void in C]
 */
LEAF(symmon_spl)
	.set	noreorder
/*	MFC0(v0,C0_SR)			typed void in C */
/* 
 * We should add a "clear_errors" entry point to the ARCS lib.  TBD
 */
#if	IP19 || IP25
/*
	If we're going to do this call, then symmon_spl needs
	to become a NESTED routine, rather than a LEAF.
	jal	cpu_clear_errors
	nop
*/
#endif /* IP19 || IP25 */
#if	IP20 || IP22 || IP26 || IP28
	AUTO_CACHE_BARRIERS_DISABLE		# addr constructed inline
	sw	zero,PHYS_TO_K1(CPU_ERR_STAT)
	sw	zero,PHYS_TO_K1(GIO_ERR_STAT)
	WBFLUSHM
	AUTO_CACHE_BARRIERS_ENABLE
#endif /* IP20 || IP22 || IP26 || IP28 */
	.set	noat
	LI	AT,SYMMON_SR_KEEP
	MFC0(v1,C0_SR)
	and	AT,v1
	LI	v1,SYMMON_SR
	or	v1,AT
	MTC0(v1,C0_SR)		# BDSLOT
	NOP_1_4
	.set	at
	.set	reorder
	j	ra
	END(symmon_spl)

/*
 * _do_it - call indirect a kernel function
 * a0 - converted (to number) version of argv[0]
 * a1 - addr of C function
 * a2 - #argc (argc)
 * a3 - argv[]
 * We change to client mode so we save regs if we get an exception
 * this allows us to recover from them and restart
 * returns whatever func a3 returns
 *
 * So we pass 2 args, the first is converted, and pass all args as
 * non-converted via argc, argv.
 * For backward compat we call functions like:
 * (a1)(atoi(argv[0]) == a0, argv[1], argc, &argv[0])
 */
LEAF(_do_it)
	_get_gpda(k1,k0)			# k1 <- pda
	li	k0,MODE_IDBG
	INT_S	k0,GPDA_DBG_MODE(k1)		# entering client mode
	REG_S	gp,GPDA_SV_GP(k1)
	REG_S	ra,GPDA_SV_RA(k1)
	REG_L	gp,k_gp
#if TFP
	.set	noreorder
	DMFC0(t0,C0_SR)				# kernel function may
	li	t1,SR_CU1			# call bcopy which uses
	or	t0,t1				# cop1 on TFP, so turn on
	DMTC0(t0,C0_SR)				# CU1 to avoid exception
	.set	reorder
#endif
#if  IP32 
        .set    noreorder
        mfc0    t0,C0_PRID
        NOP_0_4
        .set reorder
        andi    t0,C0_IMPMASK
        subu    t0,(C0_IMP_R5000 << C0_IMPSHIFT)
        beq     t0,zero,2f
	subu    t0,((C0_IMP_RM5271 << C0_IMPSHIFT) - (C0_IMP_R5000 << C0_IMPSHIFT))
        beq     t0,zero,2f
        lw      t0,_ip32_config_reg
        .set noreorder
        mtc0    t0,C0_CONFIG
        NOP_0_4
	.set reorder
2:
#endif 
	move	k0, a1				# func
	li	a1, 0
	slti	k1, a2, 2
	bne	k1, 0, 1f
	PTR_L	a1, P_SIZE(a3)			# argv[1]
1:
	jal	k0
#if  IP32 
	.set    noreorder
	mfc0    t0,C0_PRID
	NOP_0_4
	.set reorder
	andi    t0,C0_IMPMASK
	subu    t0,(C0_IMP_R5000 << C0_IMPSHIFT)
	beq     t0,zero,2f
	subu    t0,((C0_IMP_RM5271 << C0_IMPSHIFT) - (C0_IMP_R5000 << C0_IMPSHIFT))
        beq     t0,zero,2f
	.set noreorder
	mfc0    t0,C0_CONFIG
	NOP_0_4
	.set reorder
	sw	zero,_ip32_config_reg2
	sw      t0,_ip32_config_reg
	andi    t0,CONFIG_K0
	subu    t0,CONFIG_NONCOHRNT
	blez    t0,2f                   # if K0 uncached or cached non-coherent leave alone
	lw      t0,_ip32_config_reg
	li      t1,~CONFIG_K0
	and     t0,t1
	ori     t0,CONFIG_NONCOHRNT     #else set K0 to cached non-coherent
	sw	t0,_ip32_config_reg2	# for debugging
	.set noreorder
	mtc0    t0,C0_CONFIG
	NOP_0_4
	.set reorder
2:
#endif
	_get_gpda(k1,k0)			# k1 <- pda
	REG_L	gp,GPDA_SV_GP(k1)
	REG_L	ra,GPDA_SV_RA(k1)
	li	k0,MODE_DBGMON
	INT_S	k0,GPDA_DBG_MODE(k1)		# now in prom mode
	j	ra
	END(_do_it)

LEAF(_kernel_bp)
	.set	noreorder
	break	BRK_KERNELBP		# so C code knows brkpt inst
	j	ra
	NOP_1_4
	.set	reorder
	END(_kernel_bp)

/* the kp commands call the kernel functions directly with
 * the kernel's gp.  the address of kpprintf is stuffed into
 * the restart block and called by the kernel kp functions
 * to restore symmon's gp before calling the saio printf.
 *
 * _savearea cannot be in SBSS or SDATA because the gp is not
 * valid for symmon when kpprintf is called
 */

	.data
EXPORT(pod_nmi_fp3)
	.dword	0
		
_savearea:
	.space 16
	.text

LEAF(kpprintf)
	_get_gpda(k1,k0)			# k1 <- pda
	REG_S	gp, _savearea+0			# save kernel's gp 
	REG_S	ra, _savearea+8			# save ra
	lreg	gp,GPDA_SV_GP(k1)
	jal	printf				# call saio printf
	REG_L	gp, _savearea+0			# restore kernel's gp 
	REG_L	ra, _savearea+8			# restore ra
	j	ra
	END(kpprintf)

/*
 * the following jumps are copied by hook_exceptions to locations E_VEC
 * and UT_VEC
 *
 * NOTE: these must be jump register since they change 256MB text pages
 */
	.set	noreorder
	.set	noat		# must be set so la doesn't use at

LEAF(_j_exceptnorm)
	LA	k0,symmon_exceptnorm
	j	k0
	NOP_1_4
	END(_j_exceptnorm)

LEAF(_j_exceptutlb)
	LA	k0,symmon_exceptutlb
	j	k0
	NOP_1_4
	END(_j_exceptutlb)

#if R4000 || R10000
LEAF(_j_exceptxut)
	LA	k0,symmon_exceptxut
	j	k0
	NOP_1_4
	END(_j_exceptxut)

LEAF(_j_exceptecc)
#if IP28
	PTR_L	k0,k1_exceptecc
#else
	LA	k0,symmon_exceptecc
#endif
	j	k0
	NOP_1_4
	END(_j_exceptecc)
#endif /* R4000 || R10000 */

#if TFP
LEAF(tfp_clear_tlbx)
	.set    noreorder
	DMFC0(v0,C0_CAUSE)
	LI	v1,~CAUSE_VCI           # VCI=TLBX
	and	v0,v1
	DMTC0(v0,C0_CAUSE)
	.set 	reorder
	j	ra
	END(tfp_clear_tlbx)
#endif

	.set	at
	.set	reorder

#if IP26			/* need _resume to recover from GCaches errors */
/*
 * _resume -- resume execution of mainline code
 */
LEAF(_resume)
	.set	noreorder
	_get_gpda(k0,k1)		# k0 <-- pda[cpu]
	lreg	k1,GPDA_REGS(k0)	# k1 <-- &(pda[cpu].regs[0])
	lreg	v0,ROFF(R_SR)(k1)
	and	v0,SR_CU1
	beq	v0,zero,1f	
	.set	noreorder
	DMFC0(v0,C0_SR)
	nop
	nop
	or	v0,SR_CU1
	DMTC0(v0,C0_SR)
	nop
	nop
	.set	reorder
	lwc1	$f0,ROFF(R_F0)(k1)
	lwc1	$f1,ROFF(R_F1)(k1)
	lwc1	$f2,ROFF(R_F2)(k1)
	lwc1	$f3,ROFF(R_F3)(k1)
	lwc1	$f4,ROFF(R_F4)(k1)
	lwc1	$f5,ROFF(R_F5)(k1)
	lwc1	$f6,ROFF(R_F6)(k1)
	lwc1	$f7,ROFF(R_F7)(k1)
	lwc1	$f8,ROFF(R_F8)(k1)
	lwc1	$f9,ROFF(R_F9)(k1)
	lwc1	$f10,ROFF(R_F10)(k1)
	lwc1	$f11,ROFF(R_F11)(k1)
	lwc1	$f12,ROFF(R_F12)(k1)
	lwc1	$f13,ROFF(R_F13)(k1)
	lwc1	$f14,ROFF(R_F14)(k1)
	lwc1	$f15,ROFF(R_F15)(k1)
	lwc1	$f16,ROFF(R_F16)(k1)
	lwc1	$f17,ROFF(R_F17)(k1)
	lwc1	$f18,ROFF(R_F18)(k1)
	lwc1	$f19,ROFF(R_F19)(k1)
	lwc1	$f20,ROFF(R_F20)(k1)
	lwc1	$f21,ROFF(R_F21)(k1)
	lwc1	$f22,ROFF(R_F22)(k1)
	lwc1	$f23,ROFF(R_F23)(k1)
	lwc1	$f24,ROFF(R_F24)(k1)
	lwc1	$f25,ROFF(R_F25)(k1)
	lwc1	$f26,ROFF(R_F26)(k1)
	lwc1	$f27,ROFF(R_F27)(k1)
	lwc1	$f28,ROFF(R_F28)(k1)
	lwc1	$f29,ROFF(R_F29)(k1)
	lwc1	$f30,ROFF(R_F30)(k1)
	lwc1	$f31,ROFF(R_F31)(k1)
	lreg	v0,ROFF(R_C1_EIR)(k1)
	ctc1	v0,$30
	lreg	v0,ROFF(R_C1_SR)(k1)
	ctc1	v0,$31
1:
	lreg	a0,ROFF(R_A0)(k1)
	lreg	a1,ROFF(R_A1)(k1)
	lreg	a2,ROFF(R_A2)(k1)
	lreg	a3,ROFF(R_A3)(k1)
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	lreg	t0,ROFF(R_T0)(k1)
	lreg	t1,ROFF(R_T1)(k1)
	lreg	t2,ROFF(R_T2)(k1)
	lreg	t3,ROFF(R_T3)(k1)
	lreg	t4,ROFF(R_T4)(k1)
	lreg	t5,ROFF(R_T5)(k1)
	lreg	t6,ROFF(R_T6)(k1)
	lreg	t7,ROFF(R_T7)(k1)
#elif (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _ABIN32)
	lreg	a4,ROFF(R_A4)(k1)
	lreg	a5,ROFF(R_A5)(k1)
	lreg	a6,ROFF(R_A6)(k1)
	lreg	a7,ROFF(R_A7)(k1)
	lreg	t0,ROFF(R_T0)(k1)
	lreg	t1,ROFF(R_T1)(k1)
	lreg	t2,ROFF(R_T2)(k1)
	lreg	t3,ROFF(R_T3)(k1)
#else
	<<<BOMB>>>
#endif
	lreg	s0,ROFF(R_S0)(k1)
	lreg	s1,ROFF(R_S1)(k1)
	lreg	s2,ROFF(R_S2)(k1)
	lreg	s3,ROFF(R_S3)(k1)
	lreg	s4,ROFF(R_S4)(k1)
	lreg	s5,ROFF(R_S5)(k1)
	lreg	s6,ROFF(R_S6)(k1)
	lreg	s7,ROFF(R_S7)(k1)
	lreg	t8,ROFF(R_T8)(k1)
	lreg	t9,ROFF(R_T9)(k1)
	#k1 is trashed, and being used for other purpose.
	lreg	gp,ROFF(R_GP)(k1)
	lreg	fp,ROFF(R_FP)(k1)
	lreg	ra,ROFF(R_RA)(k1)
	lreg	v0,ROFF(R_MDLO)(k1)
	mtlo	v0
	lreg	v1,ROFF(R_MDHI)(k1)
	mthi	v1
	lreg	v0,ROFF(R_INX)(k1)
	.set	noreorder
#if !TFP
	DMTC0(v0,C0_INX)
	lreg	v1,ROFF(R_CTXT)(k1)
	nop
	DMTC0(v1,C0_CTXT)
#endif
	lreg	v1,ROFF(R_TLBLO)(k1)
	nop
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	DMTC0(v1,C0_TLBLO)
	lreg	v0,ROFF(R_TLBHI)(k1)
	nop
	DMTC0(v0,C0_TLBHI)
	lreg	v0,ROFF(R_CAUSE)(k1)
	nop
	MTC0(v0,C0_CAUSE)		# for software interrupts
	lreg	v1,ROFF(R_SR)(k1)
	nop
#if R4000
	and	v1,~(SR_KSU_MSK|SR_IE)	# not ready for these yet!
#else /* !R4000 */
#ifdef TFP
	and	v1,~(SR_PAGESIZE|SR_IEC)# not ready for these yet!
#else /* !TFP */
	and	v1,~(SR_KUC|SR_IEC)	# not ready for these yet!
#endif /* TFP */
#endif /* R4000 */
	DMTC0(v1,C0_SR)
	NOP_1_4
#if R4000
	lreg	v0,ROFF(R_TLBLO1)(k1)
	nop
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	DMTC0(v0,C0_TLBLO_1)
	lreg	v0,ROFF(R_PGMSK)(k1)
	nop
	DMTC0(v0,C0_PGMASK)
	lreg	v0,ROFF(R_WIRED)(k1)
	nop
	DMTC0(v0,C0_TLBWIRED)
	lreg	v0,ROFF(R_COMPARE)(k1)
	nop
	DMTC0(v0,C0_COMPARE)
	lreg	v0,ROFF(R_WATCHLO)(k1)
	nop
	DMTC0(v0,C0_WATCHLO)
	lreg	v0,ROFF(R_WATCHHI)(k1)
	nop
	DMTC0(v0,C0_WATCHHI)
#ifdef NEVER
	/* The R4000 implements a 'fused' context/ext. context register.
	 * The PTEbase section of these registers (the only writeable
	 * portion) is shared, so writing ctxt also writes extctxt.
	 * (I know, the R-series arch. spec doesn't mention this.)
	 */
	lreg	v0,ROFF(R_EXTCTXT)(k1)
	nop
	DMTC0(v0,C0_EXTCTXT)
#endif
	lreg	v0,ROFF(R_TAGLO)(k1)
	nop
	DMTC0(v0,C0_TAGLO)
#endif /* R4000 */
	.set	reorder
	lreg	sp,ROFF(R_SP)(k1)
	lreg	v1,ROFF(R_V1)(k1)
	li	v0,MODE_NORMAL		
	sreg	v0,GPDA_STACK_MODE(k0)	# returning to normal stack
	.set	noat
	.set	noreorder
	lreg	AT,ROFF(R_AT)(k1)
	lreg	v0,ROFF(R_V0)(k1)
	lreg	k1,ROFF(R_EPC)(k1)
	DMTC0(k1,C0_EPC)
	nop
	lreg	k1,GPDA_RTN_ADDR_SV(k0)
	j	k1
	nop
	.set	reorder
	.set	at
	END(_resume)
#endif	/* IP26 */
