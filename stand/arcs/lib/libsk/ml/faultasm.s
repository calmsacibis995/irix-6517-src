#ident	"lib/libsk/ml/faultasm.s:  $Revision: 1.105 $"

/*
 * faultasm.s -- standalone io library fault handling code
 */

#include <ml.h>
#include <regdef.h>
#include <asm.h>
#include <fault.h>
#include <genpda.h>
#include <arcs/debug_block.h>
#include <sys/signal.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <arcs/spb.h>

	.text

#ifdef  MULTIPROCESSOR
/* USE ONLY K1 and AT */
#ifdef	HEART_CHIP
#define saveat  \
	.set noat;		\
	CLI	k1, PHYS_TO_COMPATK1(HEART_PRID);	\
	ld	k1, 0(k1);	\
        LA	k0, atsave;     \
        sll     k1, PTR_SCALESHIFT;   \
        daddu   k0, k1;         \
        sreg    AT, 0(k0);	\
	.set at
#define restoreat       \
	CLI	k1, PHYS_TO_COMPATK1(HEART_PRID);	\
	ld	k1, 0(k1);	\
	.set	noat;		\
        LA	AT, atsave;	\
        sll     k1, PTR_SCALESHIFT;	\
        daddu   k1, AT;		\
        lreg    AT,0(k1);	\
	.set at
#endif	/* HEART_CHIP */
#if EVEREST
#define saveat  \
	.set noat;		\
        LI	k1, EV_SPNUM;  	\
	ld	k1, 0(k1);	\
	li	k0, EV_SPNUM_MASK; \
	and	k1, k0;		\
        LA	k0, atsave;     \
        sll     k1, PTR_SCALESHIFT;   \
        daddu   k0, k1;         \
        sreg    AT, 0(k0);	\
	.set at
#define restoreat       \
        LI	k1, EV_SPNUM;  	\
	ld	k1, 0(k1);	\
	.set	noat;		\
	li	AT, EV_SPNUM_MASK; \
	and	k1, AT;		\
        LA	AT, atsave;	\
        sll     k1, PTR_SCALESHIFT;	\
        daddu   k1, AT;		\
        lreg    AT,0(k1);	\
	.set at
#endif /* EVEREST */
#if SN0
/* For SN0, force a fault if we are ever in these routines.
 * symmon has its exception handlers and the IO6prom uses the ones
 * in the cpu prom.
 */
#define saveat  \
	.set noat;		\
	LI	k0, 0x00000000ffffffff; \
        sreg    AT, 0(k0);	\
	.set at
#define restoreat       \
	.set	noat;	\
	LI	k1, 0x00000000ffffffff; \
        lreg    AT, 0(k1);	\
	.set	at;
#endif  /* SN0 */
#else   /* UNIPROCESSOR */
#define saveat  \
	.set noat;		\
        LA	k0, atsave;     \
	CACHE_BARRIER_AT(0,k0);	\
        sreg    AT, 0(k0);	\
	.set at
#define restoreat       \
	.set	at;	\
        LA	k1, atsave;	\
	.set	noat;		\
        lreg    AT, 0(k1);
#endif /* MULTIPROCESSOR */

#if IP22
	BSS(_button_svc,SZREG)			# indy button service routine
	.text
#endif

/*
 * exception vector code
 * hook_exceptions() sets-up E_VEC to jump here
 */
LEAF(exceptnorm)
	/*
	 * save return address (currently in k1), restored AT and v0 in
	 * the gpda
	 */
	.set	noreorder
	.set	noat
	move	AT,k1		# at was saved earlier in vector code !!!
#if !defined (SN0)
	_get_gpda(k0,k1)			# k0 <- pda
#else
	LI	k0, 0x00000000ffffffff
#endif	
	CACHE_BARRIER_AT(0,k0)
	sreg	AT, GPDA_RTN_ADDR_SV(k0)
	restoreat
	sreg	AT,GPDA_AT_SV(k0)
	sreg	v0,GPDA_V0_SV(k0)
	.set	reorder
	.set	at

#if IP22 || IP28
#if IP28	/* clear MC error (probably) from speculative loads */
	.set	noreorder
	lw	v0,GPDA_NOFAULT(k0)		# if no fault is set
	beqz	v0,no_mc_addr_err		# let C code handle it
	nop					# BDSLOT
	CLI	k1,PHYS_TO_COMPATK1(CPU_ERR_STAT)
	lw	v0,0(k1)
	and	v0,CPU_ERR_STAT_ADDR
	beqz	v0,no_mc_addr_err
	lreg	v0,GPDA_RTN_ADDR_SV(k0)		# BDSLOT (does not hurt)
	AUTO_CACHE_BARRIERS_DISABLE		# addr constructed above
	sw	zero,0(k1)			# clear error
	AUTO_CACHE_BARRIERS_ENABLE
	lw	zero,0(k1)			# flushbus
	sync
	lw	zero,0(k1)			# flushbus continued
	lw	zero,0(k1)			# wait a bit for MC to clear
	lw	zero,0(k1)
	lw	zero,0(k1)
	lw	zero,0(k1)
	lw	zero,0(k1)
	lw	zero,0(k1)
	lw	zero,0(k1)
	.set	noat
	lreg	AT,GPDA_AT_SV(k0)
	j	v0
	lreg	v0,GPDA_V0_SV(k0)		# BDSLOT
	.set	reorder
	.set	at
no_mc_addr_err:
#endif /* IP28 */

	/* Check soft power/volume interrupt.  Do not touch k0! */
#ifdef IP22
	LI	k1,PHYS_TO_K1(HPC3_INT3_ADDR)	# assume IOC1/INT3
	IS_IOC1(v0)
	bnez	v0,1f				# branch if IOC1/INT3
	LI	k1,PHYS_TO_K1(HPC3_INT2_ADDR)	# use INT2
1:
#else
	LI	k1,PHYS_TO_K1(HPC3_INT2_ADDR)	# use INT2 on T5 Indigo2
#endif

	lbu	k1,LIO_1_ISR_OFFSET(k1)		# k1 = int2/3 base addr
	andi	k1,LIO_POWER
	beq	k1,zero,3f			# branch if NOT power

#ifdef IP22
	/* skip power button check on fullhouse (only one button) */
	IS_FULLHOUSE(v0)
	bnez	v0,2f				# branch if fullhouse

	/* Check which button was pressed */
	.set	noat
	LI	AT,PHYS_TO_K1(HPC3_PANEL)
	lw	v0,0(AT)
	.set	at
	andi	k1,v0,POWER_INT			# check which switch got pressed
	bnez	k1,vol_button			# branch if NOT power button
#endif

2:	jal	cpu_soft_powerdown		# no return
3:
#endif	/* IP22 || IP28 */

	/*
	 * Check if this is a "kernel breakpoint" that is to be handled
	 * by the debug monitor
	 */
	.set	noreorder
	MFC0(v0,C0_CAUSE)
	nop
	.set	reorder
	and	k1,v0,CAUSE_EXCMASK
	bne	k1,+EXC_BREAK,2f	# not even a break inst
	.set	noreorder
	DMFC0(k1,C0_EPC)
	.set	reorder
	and	v0,CAUSE_BD
	beq	v0,zero,1f		# not in branch delay slot
	PTR_ADDU	k1,4			# advance to bd slot
1:
	lw	k1,0(k1)		# fetch faulting instruction
	lw	v0,kernel_bp		# bp inst used by symmon
	bne	v0,k1,2f		# not symmon's break inst

	.set	noat
	LI	AT,SPB_DEBUGADDR
	lreg	AT,0(AT)		# address of debug block
	beq	AT,zero,2f		# no debug block
	lreg	AT,DB_BPOFF(AT)		# breakpoint handler
	beq	AT,zero,2f		# no handler
	lreg	v0,GPDA_V0_SV(k0)	# restore v0
	lreg	k0,GPDA_AT_SV(k0)	# symmon wants k0 == AT
	j	AT			# enter breakpoint handler
	.set	at

2:
	li	v0,EXCEPT_NORM
	LA	k1,exception
	jr	k1

kernel_bp:
	break	BRK_KERNELBP

	END(exceptnorm)

#if defined(R4000) || defined(R10000)
LEAF(exceptxut)
	.set	noat
	move	AT,k1			# Return Address
	.set	noreorder
#if !defined (SN0)
	_get_gpda(k0,k1)			# k0 <- pda
#else
	/* For SN0, force a fault if we are ever in these routines.
	 * symmon has its exception handlers and the IO6prom uses the ones
	 * in the cpu prom.
	 */
	LI	k0, 0x00000000ffffffff
#endif	
	.set	reorder
	.set	noat
	sreg	AT,GPDA_RTN_ADDR_SV(k0)	# Return address
	restoreat
	sreg	AT,GPDA_AT_SV(k0)
	.set	at
	sreg	v0,GPDA_V0_SV(k0)
	li	v0,EXCEPT_XUT
	j	exception
	END(exceptxut)

/*
** The ECC handler probably needs to do something much more
** intelligent than this, but for now, we'll just make it look
** like the other exceptions.
*/
LEAF(exceptecc)
	.set	noat
	move	AT,k1
	.set	noreorder
#if !defined (SN0)
	_get_gpda(k0,k1)			# k0 <- pda
#else
	/* For SN0, force a fault if we are ever in these routines.
	 * symmon has its exception handlers and the IO6prom uses the ones
	 * in the cpu prom.
	 */

	LI	k0, 0x00000000ffffffff
#endif	
	.set	reorder
	.set	noat
	sreg	AT,GPDA_RTN_ADDR_SV(k0)
	restoreat
	sreg	AT,GPDA_AT_SV(k0)
	.set	at
	sreg	v0,GPDA_V0_SV(k0)
	li	v0,EXCEPT_ECC
	j	exception
	END(exceptecc)

#if defined(IP28)
/*
 * NMI
 * IDE sets up a pointer in the GDA to jump here on NMI.
 */
LEAF(ip28_nmi)
        .set    noat
        move    AT,k1
        .set    noreorder
#if !defined (SN0)
	_get_gpda(k0,k1)			# k0 <- pda
#else
	/* For SN0, force a fault if we are ever in these routines.
	 * symmon has its exception handlers and the IO6prom uses the ones
	 * in the cpu prom.
	 */

	LI	k0, 0x00000000ffffffff
#endif	
        .set    reorder
        .set    noat
        sreg    AT,GPDA_RTN_ADDR_SV(k0)
        restoreat
        sreg    AT,GPDA_AT_SV(k0)
        .set    at
        sreg    v0,GPDA_V0_SV(k0)
        li      v0,EXCEPT_NMI
        j       exception
        END(ip28_nmi)
#endif

#endif /* defined(R4000) || defined(R10000) */


/*
 * utlb miss
 * hook_exceptions() sets-up UT_VEC to jump here
 */
LEAF(exceptutlb)
	.set	noat
	move	AT,k1
	.set	noreorder
#if !defined (SN0)
	_get_gpda(k0,k1)			# k0 <- pda
#else
	/* For SN0, force a fault if we are ever in these routines.
	 * symmon has its exception handlers and the IO6prom uses the ones
	 * in the cpu prom.
	 */
	LI	k0, 0x00000000ffffffff
#endif	
	.set	reorder
	sreg	AT,GPDA_RTN_ADDR_SV(k0)
	restoreat
	sreg	AT,GPDA_AT_SV(k0)
	.set	at
	sreg	v0,GPDA_V0_SV(k0)

#if IP24_DEBUG			/* strictly board debugging */
	.set	noreorder
/* write sentinel and various cpu regs to fixed address for LA debugging */
	li	v0,0xf1f1
	sreg	v0,0xbfbd98e0
	DMFC0(v0,C0_EPC)
	sreg	v0,0xbfbd98e0
	MFC0(v0,C0_CAUSE)
	sreg	v0,0xbfbd98e0
	DMFC0(v0,C0_CTXT)
	sreg	v0,0xbfbd98e0
	DMFC0(v0,C0_BADVADDR)
	sreg	v0,0xbfbd98e0
	DMFC0(v0,C0_TLBLO)
	sreg	v0,0xbfbd98e0
	DMFC0(v0,C0_TLBLO_1)
	sreg	v0,0xbfbd98e0
	DMFC0(v0,C0_TLBHI)
	sreg	v0,0xbfbd98e0
	.set	reorder
#endif
	li	v0,EXCEPT_UTLB

/*
 * common exception handling code
 */
exception:
	/*
	 * Save various registers so we can print informative messages
	 * for faults (whether on normal stack or fault stack)
         * The regs that are subject to change when initially taking
         * exceptions (i.e. indicate something *about* the exception or
         * are used to determine where we came from) must not be saved
         * in the regs area until we determine if this was a nested fault:
         * the _regs area must preserve the "normal mode" context of the
         * process.  We'll move them into the regs area later.
         */
	sreg	v0,GPDA_EXC_SV(k0)
	.set	noreorder
	AUTO_CACHE_BARRIERS_DISABLE		# mfc0 serializes stores
	DMFC0(v0,C0_EPC)
	nop
	sreg	v0,GPDA_EPC_SV(k0)
	MFC0(v0,C0_SR)
	nop
	sreg	v0,GPDA_SR_SV(k0)
	DMFC0(v0,C0_BADVADDR)
	nop
	sreg	v0,GPDA_BADVADDR_SV(k0)
	MFC0(v0,C0_CAUSE)
	nop
	AUTO_CACHE_BARRIERS_ENABLE
	.set	reorder
	sreg	v0,GPDA_CAUSE_SV(k0)
	sreg	sp,GPDA_SP_SV(k0)
#if defined(R4000) || defined(R10000)
	# if ECC error, set DE bit to suppress further exceptions.
	lreg	v0, GPDA_EXC_SV(k0)
	bne	v0, EXCEPT_ECC, 1f
	lreg	v0, GPDA_SR_SV(k0)
	or	v0, SR_DE
	lreg	v0, GPDA_SR_SV(k0)
	.set	noreorder
	MFC0(v0,C0_SR)
	nop
	nop
	or	v0, SR_DE
	MTC0(v0,C0_SR)
	nop
	nop
1:
        MFC0(v0,C0_CACHE_ERR)
        nop
	AUTO_CACHE_BARRIERS_DISABLE		# mfc0 above serializes
        sreg    v0,GPDA_CACHE_ERR_SV(k0)
	AUTO_CACHE_BARRIERS_ENABLE
	# if ECC error, NMI, or process executed a soft reset
	# instruction the C0_ERROR_EPC (not EPC) contains the
	# relevant addr.
        DMFC0(v0,C0_ERROR_EPC)
        nop
        .set    reorder
        sreg    v0,GPDA_ERROR_EPC_SV(k0)

#endif	/* defined(R4000) || defined(R10000) */

	/* save any registers specific to the cpu type
	 */
#if IP20 || IP22 || IP26 || IP28
	CLI	v0,PHYS_TO_COMPATK1(CPU_ERR_STAT)
	lw	v0,0(v0)
	sw	v0,_cpu_parerr_save
	CLI	v0,PHYS_TO_COMPATK1(GIO_ERR_STAT)
	lw	v0,0(v0)
	sw	v0,_gio_parerr_save

#if IP22 || IP26 || IP28
#if IP22
	LI	k1,PHYS_TO_K1(HPC3_INT3_ADDR)	# assume IOC1/INT3
	IS_IOC1(v0)
	bnez	v0,1f				# branch if IOC1/INT3
	LI	k1,PHYS_TO_K1(HPC3_INT2_ADDR)	# use INT2
1:
#else
	CLI	k1,PHYS_TO_COMPATK1(HPC3_INT2_ADDR)	# use INT2
#endif
	lbu	v0, LIO_0_ISR_OFFSET(k1)
	sb	v0, _liointr0_save
	lbu	v0, LIO_1_ISR_OFFSET(k1)
	sb	v0, _liointr1_save
	lbu	v0, LIO_2_3_ISR_OFFSET(k1)
	sb	v0, _liointr2_save
#else
	li	v0,LIO_0_ISR_ADDR+K1BASE
  	lbu	v0,0(v0)
	sb	v0,_liointr0_save
	li	v0,LIO_1_ISR_ADDR+K1BASE
  	lbu	v0,0(v0)
	sb	v0,_liointr1_save
#endif
	LI	v0,CPU_ERR_ADDR+K1BASE
	lw	v0,0(v0)
	sw	v0,_cpu_paraddr_save
	LI	v0,GIO_ERR_ADDR+K1BASE
	lw	v0,0(v0)
	sw	v0,_gio_paraddr_save
	LI	v0,CPU_ERR_STAT+K1BASE	# clear CPU bus and parity error
	sw	zero,0(v0)
	LI	v0,GIO_ERR_STAT+K1BASE	# clear GIO bus and parity error
	sw	zero,0(v0)
#if IP22 || IP26 || IP28
#if IP22
	LI	v0,HPC3_PANEL+K1BASE
	lw	v0,0(v0)
	sw      v0,_power_intstat_save
#endif
	CLI	v0,PHYS_TO_COMPATK1(HPC3_INTSTAT_ADDR)
	lw	v0,0(v0)
	sw	v0,_hpc3_intstat_save
	CLI	v0,PHYS_TO_COMPATK1(HPC3_BUSERR_STAT_ADDR)
	lw	v0,0(v0)
	sw	v0,_hpc3_bus_err_stat_save
	CLI	v0,PHYS_TO_COMPATK1(HPC3_SYS_ID)
	lw	v0,0(v0)
	CLI	v0,PHYS_TO_COMPATK1(HPC3_EXT_IO_ADDR)
	lw	v0,0(v0)
	sw	v0,_hpc3_ext_io_save
#endif /* IP22 || IP26 || IP28 */
#if IP26
	LI	v1,PHYS_TO_K1(TCC_BASE)
	ld	v0,TCC_INTR-TCC_BASE(v1)
	sd	v0,TCC_INTR-TCC_BASE(v1)	# clear BE/MC for nofault
	sd	v0,_tcc_intr_save
	ld	v0,TCC_ERROR-TCC_BASE(v1)
	sd	v0,_tcc_error_save
	ld	v0,TCC_PARITY-TCC_BASE(v1)
	sd	v0,_tcc_parity_save
	ld	v0,TCC_BE_ADDR-TCC_BASE(v1)
	sd	v0,_tcc_be_addr_save
#endif
#endif	/* IP20 || IP22 || IP26 || IP28 */

#if IP20 || IP22 || IP26 || IP28
	/* GPDA_FAULT_SP wasn't being initialized */
	lreg	sp,_fault_sp
#else
	lreg	sp,GPDA_FAULT_SP(k0)		# use "fault" stack
	bnez	sp, 2f				# make sure it's not zero
	lreg	sp,_fault_sp			# in case of early exception
2:
#endif
	PTR_SRL	sp, 4				# paranoid
	PTR_SLL	sp, 4				# ensure proper alignment
	/*
	 * Only save registers if on regular stack
	 * then change mode to fault mode
	 */
	lreg	v0,GPDA_STACK_MODE(k0)
	sreg	v0,GPDA_MODE_SV(k0)
	beq	v0,MODE_FAULT,nosave		# was in fault mode
	li	v0,MODE_FAULT
	sreg	v0,GPDA_STACK_MODE(k0)		# now in fault mode

	# Load K1 with address of &pda[cpu].regs[0]
	lreg	k1,GPDA_REGS(k0)

	# if pointer not initialized yet, use cpu 0's area
	bnez	k1,1f
	LA	k1,excep_regs
1:
	#
	lreg	v0,GPDA_EPC_SV(k0)
	sreg	v0,ROFF(R_EPC)(k1)
#if defined(R4000) || defined(R10000)
	lreg      v0,GPDA_ERROR_EPC_SV(k0)
	sreg      v0,ROFF(R_ERREPC)(k1)
	lreg      v0,GPDA_CACHE_ERR_SV(k0)
	sreg      v0,ROFF(R_CACHERR)(k1)
#endif /* R4000 || R10000 */
	lreg	v0,GPDA_SR_SV(k0)
	sreg	v0,ROFF(R_SR)(k1)

	lreg	v0,GPDA_AT_SV(k0)
	sreg	v0,ROFF(R_AT)(k1)
	lreg	v0,GPDA_V0_SV(k0)
	sreg	v0,ROFF(R_V0)(k1)
	lreg	v0,GPDA_EXC_SV(k0)
	sreg	v0,ROFF(R_EXCTYPE)(k1)
	lreg	v0,GPDA_BADVADDR_SV(k0)
	sreg	v0,ROFF(R_BADVADDR)(k1)
	lreg	v0,GPDA_CAUSE_SV(k0)
	sreg	v0,ROFF(R_CAUSE)(k1)
	lreg	v0,GPDA_SP_SV(k0)
	sreg	v0,ROFF(R_SP)(k1)
	sreg	zero,ROFF(R_ZERO)(k1)		# we don't trust anything
	sreg	v1,ROFF(R_V1)(k1)
	sreg	a0,ROFF(R_A0)(k1)
	sreg	a1,ROFF(R_A1)(k1)
	sreg	a2,ROFF(R_A2)(k1)
	sreg	a3,ROFF(R_A3)(k1)
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	sreg	t0,ROFF(R_T0)(k1)
	sreg	t1,ROFF(R_T1)(k1)
	sreg	t2,ROFF(R_T2)(k1)
	sreg	t3,ROFF(R_T3)(k1)
	sreg	t4,ROFF(R_T4)(k1)
	sreg	t5,ROFF(R_T5)(k1)
	sreg	t6,ROFF(R_T6)(k1)
	sreg	t7,ROFF(R_T7)(k1)
#elif (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _ABIN32)
	sreg	a4,ROFF(R_A4)(k1)
	sreg	a5,ROFF(R_A5)(k1)
	sreg	a6,ROFF(R_A6)(k1)
	sreg	a7,ROFF(R_A7)(k1)
	sreg	t0,ROFF(R_T0)(k1)
	sreg	t1,ROFF(R_T1)(k1)
	sreg	t2,ROFF(R_T2)(k1)
	sreg	t3,ROFF(R_T3)(k1)
#else
	<<<BOMB>>>
#endif
	sreg	s0,ROFF(R_S0)(k1)
	sreg	s1,ROFF(R_S1)(k1)
	sreg	s2,ROFF(R_S2)(k1)
	sreg	s3,ROFF(R_S3)(k1)
	sreg	s4,ROFF(R_S4)(k1)
	sreg	s5,ROFF(R_S5)(k1)
	sreg	s6,ROFF(R_S6)(k1)
	sreg	s7,ROFF(R_S7)(k1)
	sreg	t8,ROFF(R_T8)(k1)
	sreg	t9,ROFF(R_T9)(k1)
	move	t9,k0			# save k0 
	li	k0,0xbad00bad		# make it obvious we can't save this
	sreg	k0,ROFF(R_K0)(k1)
	li	k0,0xbad11bad
	sreg	k0,ROFF(R_K1)(k1)		# Mark k1, as trashed
	move	k0,t9
	lreg	t9,ROFF(R_T9)(k1)		# Heck, get t9 back..
	sreg	fp,ROFF(R_FP)(k1)
	sreg	gp,ROFF(R_GP)(k1)
	sreg	ra,ROFF(R_RA)(k1)
	mflo	v0
	sreg	v0,ROFF(R_MDLO)(k1)
	mfhi	v0
	sreg	v0,ROFF(R_MDHI)(k1)
	.set	noreorder
	AUTO_CACHE_BARRIERS_DISABLE	# mfc0s will serialize stores
#if !TFP
	MFC0(v0,C0_INX)
	nop
	sreg	v0,ROFF(R_INX)(k1)
	MFC0(v0,C0_RAND)		# save just to see it change
	nop
	sreg	v0,ROFF(R_RAND)(k1)
	DMFC0(v0,C0_CTXT)
	nop
	sreg	v0,ROFF(R_CTXT)(k1)
#endif
	DMFC0(v0,C0_TLBLO)
	nop
	sreg	v0,ROFF(R_TLBLO)(k1)
	DMFC0(v0,C0_TLBHI)
	nop
	sreg	v0,ROFF(R_TLBHI)(k1)
#if defined(R4000) || defined(R10000)
	DMFC0(v0,C0_TLBLO_1)
	nop
	sreg	v0,ROFF(R_TLBLO1)(k1)
	DMFC0(v0,C0_PGMASK)
	nop
	sreg	v0,ROFF(R_PGMSK)(k1)
	MFC0(v0,C0_TLBWIRED)
	nop
	sreg	v0,ROFF(R_WIRED)(k1)
	MFC0(v0,C0_COUNT)
	nop
	sreg	v0,ROFF(R_COUNT)(k1)
	MFC0(v0,C0_COMPARE)
	nop
	sreg	v0,ROFF(R_COMPARE)(k1)
	MFC0(v0,C0_LLADDR)
	nop
	sreg	v0,ROFF(R_LLADDR)(k1)
#ifdef R4600
	mfc0	v0,C0_PRID
	NOP_1_4
	andi	v0,0xFF00
	sub	v0,0x2000
	beqz	v0,96f
	nop
	sub	v0,(0x2100-0x2000)		# check for R4700
	beqz	v0,96f
	nop
	sub	v0,(0x2300-0x2100)		# check for r5000
	beqz    v0,96f
        nop
        sub     v0,(0x2800-0x2300)              # check for rm5271
	bnez	v0,97f
	nop
96:
	sw	zero,ROFF(R_WATCHLO)(k0)
	sw	zero,ROFF(R_WATCHHI)(k0)	
	b	98f
	nop
97:
#endif
	MFC0(v0,C0_WATCHLO)
	nop
	sreg	v0,ROFF(R_WATCHLO)(k1)
	MFC0(v0,C0_WATCHHI)
	nop
	sreg	v0,ROFF(R_WATCHHI)(k1)
	DMFC0(v0,C0_EXTCTXT)
	nop
	sreg	v0,ROFF(R_EXTCTXT)(k1)
#ifdef R4600
98:
#endif
	MFC0(v0,C0_ECC)
	nop
	sreg	v0,ROFF(R_ECC)(k1)
	#DMFC0(v0,C0_CACHE_ERR)
	#nop
	#sreg	v0,ROFF(R_CACHERR)(k1)
	MFC0(v0,C0_TAGLO)
	nop
	sreg	v0,ROFF(R_TAGLO)(k1)
	MFC0(v0,C0_TAGHI)
	nop
	sreg	v0,ROFF(R_TAGHI)(k1)
	#DMFC0(v0,C0_ERROR_EPC)
	#nop
	#sreg	v0,ROFF(R_ERREPC)(k1)		
	MFC0(v0,C0_CONFIG)
	nop
	sreg	v0,ROFF(R_CONFIG)(k1)
#endif /* R4000 || R10000 */
	AUTO_CACHE_BARRIERS_ENABLE
	.set	reorder
	lreg	v0,GPDA_SR_SV(k0)
	and	v0,SR_CU1
	beq	v0,zero,nosave
	swc1	$f0,ROFF(R_F0)(k1)
	swc1	$f1,ROFF(R_F1)(k1)
	swc1	$f2,ROFF(R_F2)(k1)
	swc1	$f3,ROFF(R_F3)(k1)
	swc1	$f4,ROFF(R_F4)(k1)
	swc1	$f5,ROFF(R_F5)(k1)
	swc1	$f6,ROFF(R_F6)(k1)
	swc1	$f7,ROFF(R_F7)(k1)
	swc1	$f8,ROFF(R_F8)(k1)
	swc1	$f9,ROFF(R_F9)(k1)
	swc1	$f10,ROFF(R_F10)(k1)
	swc1	$f11,ROFF(R_F11)(k1)
	swc1	$f12,ROFF(R_F12)(k1)
	swc1	$f13,ROFF(R_F13)(k1)
	swc1	$f14,ROFF(R_F14)(k1)
	swc1	$f15,ROFF(R_F15)(k1)
	swc1	$f16,ROFF(R_F16)(k1)
	swc1	$f17,ROFF(R_F17)(k1)
	swc1	$f18,ROFF(R_F18)(k1)
	swc1	$f19,ROFF(R_F19)(k1)
	swc1	$f20,ROFF(R_F20)(k1)
	swc1	$f21,ROFF(R_F21)(k1)
	swc1	$f22,ROFF(R_F22)(k1)
	swc1	$f23,ROFF(R_F23)(k1)
	swc1	$f24,ROFF(R_F24)(k1)
	swc1	$f25,ROFF(R_F25)(k1)
	swc1	$f26,ROFF(R_F26)(k1)
	swc1	$f27,ROFF(R_F27)(k1)
	swc1	$f28,ROFF(R_F28)(k1)
	swc1	$f29,ROFF(R_F29)(k1)
	swc1	$f30,ROFF(R_F30)(k1)
	swc1	$f31,ROFF(R_F31)(k1)
	cfc1	v0,$30
	sreg	v0,ROFF(R_C1_EIR)(k1)
	cfc1	v0,$31
	sreg	v0,ROFF(R_C1_SR)(k1)
nosave:
#if IP22				/* Indy power button */
	lw	k0,GPDA_EXC_SV(k0)
	xori	k0,101
	beqz	k0,1f
	lreg	k0,_button_svc
	beqz	k0,1f
	j	k0
1:
#endif
#if defined(R4000) || defined(R10000)
	/*
	 * clear SR_EXL such that the R4000 will update EPC on nested
	 * exception, clear SR_IEC to block off interrupt
	 */
	.set	noreorder
	MFC0(k0,C0_SR)
	nop
	nop
	and	k0,~(SR_EXL|SR_IEC)
	MTC0(k0,C0_SR)
	nop
	nop
	.set	reorder
#endif	/* R4000 || R10000 */
#ifdef IP24
	LA	k0,_exception_handler
	jr	k0
#else /* not IP24 */
        jal	RFaultHandler
	jal	_resume		# leave ra trace instead of falling thru
#endif
	END(exceptutlb)

/*
 * _resume -- resume execution of mainline code
 */
LEAF(_resume)
	.set	noreorder
#if !defined (SN0)
	_get_gpda(k0,k1)			# k0 <- pda
#else	
	/* For SN0, force a fault if we are ever in these routines.
	 * symmon has its exception handlers and the IO6prom uses the ones
	 * in the cpu prom.
	 */
	
	LI	k0, 0x00000000ffffffff
#endif	
	lreg	k1,GPDA_REGS(k0)	# k1 <-- &(pda[cpu].regs[0])
	lreg	v0,ROFF(R_SR)(k1)
	and	v0,SR_CU1
	beq	v0,zero,1f	
	.set	noreorder
	MFC0(v0,C0_SR)
	nop
	nop
	or	v0,SR_CU1
	MTC0(v0,C0_SR)
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
	MTC0(v0,C0_INX)
	lreg	v1,ROFF(R_CTXT)(k1)
	nop
	DMTC0(v1,C0_CTXT)
#endif
	lreg	v1,ROFF(R_TLBLO)(k1)
	nop
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	MTC0(v1,C0_TLBLO)
	lreg	v0,ROFF(R_TLBHI)(k1)
	nop
	DMTC0(v0,C0_TLBHI)
	lreg	v0,ROFF(R_CAUSE)(k1)
	nop
	MTC0(v0,C0_CAUSE)		# for software interrupts
	lreg	v1,ROFF(R_SR)(k1)
	nop
#if defined(R4000) || defined (R10000)
	nop
	#and	v1,~(SR_KSU_MSK|SR_IE)	# not ready for these yet!
#else /* !R4000 */
#ifdef TFP
	and	v1,~(SR_PAGESIZE|SR_IEC)# not ready for these yet!
#else /* !TFP */
	and	v1,~(SR_KUC|SR_IEC)	# not ready for these yet!
#endif /* TFP */
#endif /* R4000 */
	MTC0(v1,C0_SR)
	NOP_1_4
#if defined(R4000) || defined (R10000)
	lreg	v0,ROFF(R_TLBLO1)(k1)
	nop
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	DMTC0(v0,C0_TLBLO_1)
	lreg	v0,ROFF(R_PGMSK)(k1)
	nop
	DMTC0(v0,C0_PGMASK)
	lreg	v0,ROFF(R_WIRED)(k1)
	nop
	MTC0(v0,C0_TLBWIRED)
	lreg	v0,ROFF(R_COMPARE)(k1)
	nop
	MTC0(v0,C0_COMPARE)
#ifdef R4600
	nop
	mfc0	v0,C0_PRID
	NOP_1_4
	andi	v0,0xFF00
	sub	v0,0x2000
	beqz	v0,98f
	nop
	sub	v0,(0x2100-0x2000)	# check for R4700
	beqz	v0,98f
	nop
	sub	v0,(0x2300-0x2100)	# check for r5000
	beqz    v0,98f
        nop
        sub     v0,(0x2800-0x2300)      # check for rm5271
	beqz	v0,98f
	nop
#endif
	lreg	v0,ROFF(R_WATCHLO)(k1)
	nop
	MTC0(v0,C0_WATCHLO)
	lreg	v0,ROFF(R_WATCHHI)(k1)
	nop
	MTC0(v0,C0_WATCHHI)
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
#ifdef R4600
98:
#endif
	lreg	v0,ROFF(R_TAGLO)(k1)
	nop
	MTC0(v0,C0_TAGLO)
#endif /* R4000 || R10000 */
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

/*
 * the following jumps are copied by hook_exceptions to locations E_VEC
 * and UT_VEC
 *
 * NOTE: these must be jump register since they change 256MB text pages
 */

/* For MP case, save at for use in exception handler
 * With MP IDE, there is one PDA for each cpu, and to get at right PDA
 * 2 registers are needed. In exception handlers k1 has return address
 * If we ever need to return after an exception, value in k1 has to be 
 * retained. So save at as part of exception vector, and in exception
 * handler, save return address in at
 */


	.set	noreorder
	.set	noat		# must be set so la doesn't use at

LEAF(_j_exceptnorm)
	saveat
	LI	k0,SPB_GEVECTOR
	lreg	k0,0(k0)
	nop
	jal	k1,k0
	nop
	eret
	nop
	END(_j_exceptnorm)

LEAF(_j_exceptutlb)
	saveat
	LI	k0,SPB_UTLBVECTOR
	lreg	k0,0(k0)
	nop
	jal	k1,k0
	nop
	eret
	nop
	END(_j_exceptutlb)

#if defined(R4000) || defined (R10000)
LEAF(_j_exceptxut)
	saveat
	LA	k0,exceptxut
	jal	k1,k0
	nop
	eret
	nop
	END(_j_exceptxut)

LEAF(_j_exceptecc)
#ifdef R4000
	nop
#endif	/* R4000 */
	saveat
	PTR_L	k0,k1_exceptecc
	jal	k1,k0
	nop
	eret
	nop
	END(_j_exceptecc)
#endif /* R4000 R10000 */


	.set	at
	.set	reorder

#if IP22
    /* Received a volume button interrupt, deal with it here.
     *
     * note: k1 = int2/3 base addr
     *       AT = power/panel reg addr
     */
	.set	noat

vol_button:
	lreg	v0,_button_svc			# button service routine?
	beqz	v0,1f				# no

    /* IOC1 Version 1 - has a deficiency that causes constant interrupts
     * while either volume button is being depressed.  Sooo, what we do is
     * disable the interrupt at the local 1 i/o mask register and wait for
     * the button to be released, and afterwards, reenable the power interrupt.
     */
	lbu	k0,LIO_1_MASK_OFFSET(k1)
	andi	k0,(~LIO_MASK_POWER&0xff)
	sb	k0,LIO_1_MASK_OFFSET(k1)
	j	v0				# yes (return via _button_rtn)

#ifndef	ANY_BUTTON
#define	ANY_BUTTON	(PANEL_VOLUME_UP_INT|PANEL_VOLUME_UP_ACTIVE|\
			 PANEL_VOLUME_DOWN_INT|PANEL_VOLUME_DOWN_ACTIVE)
#endif

LEAF(_button_rtn)			/* need to reget int2/3 base adrs */
	.set	at
	LI	k1,PHYS_TO_K1(HPC3_INT3_ADDR)	# assume INT3
	IS_IOC1(v0)
	.set noat
	beqz	v0,3f				# branch if IOC1
	LI	k1,PHYS_TO_K1(HPC3_INT2_ADDR)	# use INT2 
3:	lbu	k0,LIO_1_MASK_OFFSET(k1)	# restore power interrupt
	ori	k0,LIO_MASK_POWER
	sb	k0,LIO_1_MASK_OFFSET(k1)

1:	LI	AT,PHYS_TO_K1(HPC3_PANEL)
	li	k0,POWER_ON			# clear power/panel reg interrupt(s)
2:	lw	v0,0(AT)			# reget status
	sw	k0,0(AT)			# clear interrupt

	xori	v0,ANY_BUTTON			# button still down?
	andi	v0,ANY_BUTTON
	bnez	v0,2b				# if so, wait for it to be released

	/* restore v0/at from gpda */
	.set	noreorder
#if !defined (SN0)
	_get_gpda(k0,k1)			# k0 <- pda
#else
	/* For SN0, force a fault if we are ever in these routines.
	 * symmon has its exception handlers and the IO6prom uses the ones
	 * in the cpu prom.
	 */
	LI	k0, 0x00000000ffffffff
#endif	
	.set	noat
	lreg	AT,GPDA_AT_SV(k0)
	.set	at
	lreg	v0,GPDA_V0_SV(k0)

	eret					# resume execution
	nop
	.set	reorder
END(_button_rtn)
#endif	/* IP22 */

/* Spin, doing any machine dependent set-up.
 */
LEAF(nested_exception_spin)

#if IP22 || IP24 || IP26 || IP28		/* enable soft power switch */
#if IP22 || IP24
	LI	k1,PHYS_TO_K1(HPC3_INT3_ADDR)	# assume IOC1/INT3
	IS_IOC1(k0)
	bnez	k0, 1f				# branch if IOC1/INT3
#endif
	LI	k1,PHYS_TO_K1(HPC3_INT2_ADDR)	# use INT2

1:	li	k0,LIO_MASK_POWER
	sb	k0,LIO_1_MASK_OFFSET(k1)
#if defined(R4000) || defined (R10000)
	li	k0,(SR_PROMBASE|SR_BEV|SR_IE|SR_IBIT4)
#endif /* R4000 || R10000 */
#if TFP
	LI	k0,(SR_PROMBASE|SR_IBIT4)
#endif
	.set	noreorder
	MTC0	(k0,C0_SR)
	.set	reorder
#endif

1:	b	1b
	END(nested_exception_spin)
