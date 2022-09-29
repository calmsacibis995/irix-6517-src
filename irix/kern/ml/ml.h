#ident "$Revision: 1.125 $"

#include "sys/asm.h"
#include "sys/regdef.h"
#include "sys/fpu.h"
#include "sys/softfp.h"
#include "sys/signal.h"
#include "sys/sbd.h"
#include "assym.h"
#include "sys/traplog.h"
#include "sys/timers.h"

#ifdef	EVEREST
#include "sys/EVEREST/everest.h"
#include "sys/EVEREST/evintr.h"
#endif	/* EVEREST */
#ifdef	SN
#include "sys/cpu.h"
#endif /* SN */
#if TFP
#include "sys/tfp.h"
#endif

#ifdef DEBUG
#define OVFLWDEBUG	1		/* check stack overflow */
#undef STKDEBUG	1		/* check stack switching */
#undef TLBDEBUG	1
#endif

#define lreg	ld
#define sreg	sd
#define BPREG	8

#define CPUID_L		lh
#define CPUID_S		sh
#define NODEID_L	lh
#define NODEID_S	sh
#define NASID_L		lh	/* Laod a nasid_t */
#define NASID_S		sh	/* Store a nasid_t */


#ifdef	PTE_64BIT
/* Under PTE_64BIT, page table entries are 64 bits. On all other platforms
 * they are 32 bit entities. So, there is no correlation between a 
 * 64 bit kernel, and a 64 bit pte. 64 bit kernels by default have
 * 32 bit ptes, and only under PTE_64BIT configuration they would have
 * 64 bit ptes.
 */
#define	PTE_L           ld
#define	PTE_S           sd
#define	PTESZ_WORD	.dword
#define	PTESZ_SHFT	1	/* Amount by which ctxt regr shld be shifted */
#define	PTE_SIZE_SHIFT	3	/* Shift factor to convert pte offeset to byte
				 * offset 
				 */ 
#else
#ifdef	TFP
#define	PTE_L           lwu
#else
#define	PTE_L           lw
#endif
#define	PTE_S           sw
#define	PTESZ_WORD	.word
#define	PTESZ_SHFT	0	/* Amount by which ctxt regr shld be shifted */
#define	PTE_SIZE_SHIFT	2	/* Shift factor to convert pte offeset to byte
				 * offset 
				 */ 
#endif	/* PTE_64BIT */

/* The R10000 can speculatively issue loads and stores,
 *	with the side effect that random cache fills are
 *	triggered, sometimes followed by a writeback of
 *	the same data (spec store makes line dirty).
 *
 * The compiler and assembler can place these cache
 *	barriers for us in nearly all cases, when we use
 *	the "-Wa,-t5_no_spec_mem" option. However,
 *	inside "noreorder" sections, we need to
 *	establish our own barriers.
 *
 * Use the CACHE_BARRIER macro when the stack pointer is
 *	a valid address; otherwise, use CACHE_BARRIER_AT
 *	and specify the offset and base register to be
 *	something that won't cause a fault when it gets
 *	translated. Too bad it doesn't discard the TLB
 *	MISS fault, since the physical address is not
 *	actually needed for anything ...
 *
 * The CACHE_BARRIER_WANTED macro can be planted where
 *	the assembler claims a cache operation is
 *	needed, then found later and changed into an
 *	appropriate barrier, or DISABLE/ENABLE pairs can
 *	be planted. It just needs to be something that
 *	will silence the assembler warning. If you use
 *	CACHE_BARRIER_WANTED, you are promising to go
 *	back and figure out what the right thing to do
 *	is at that location ...
 */

#if R10000_SPECULATION_WAR && (!MH_R10000_SPECULATION_WAR)
#define	CACHE_BARRIER_AT(o,r)		cache CACH_BARRIER,o(r)
#define	ORD_CACHE_BARRIER_AT(o,r)	.set noreorder; CACHE_BARRIER_AT(o,r); .set reorder
#define	CACHE_BARRIER			CACHE_BARRIER_AT(0,sp)
#define	AUTO_CACHE_BARRIERS_DISABLE	.set no_spec_mem
#define	AUTO_CACHE_BARRIERS_ENABLE	.set spec_mem
#define	CACHE_BARRIER_WANTED		CACHE_BARRIER
#else
#define	CACHE_BARRIER_AT(o,r)
#define	ORD_CACHE_BARRIER_AT(o,r)
#define	CACHE_BARRIER
#define	AUTO_CACHE_BARRIERS_DISABLE
#define	AUTO_CACHE_BARRIERS_ENABLE
#define	CACHE_BARRIER_WANTED
#endif	/* R10000_SPECULATION_WAR && (!MH_R10000_SPECULATION_WAR) */

/*
 * flush any write buffer - this macro/routine needs to make sure that any
 * write-behind writes are out and any possible errors (bus-error) have had
 * time to be received
 * WARNING: uses label 9
 */

/* this definition matches wbflush() in EVEREST/evmp.c; do uncached load
 * from Everest error block
 */
#if EVEREST || IPMHSIM
#define wbflushm		  \
	.set	noreorder	; \
	.set	noat		; \
	lui	AT,(K1BASE>>48)	; \
	dsll	AT,AT,32	; \
	lw	zero,0x2800(AT)	; \
	.set	at		; \
	.set	reorder
#endif

#if SN
#define wbflushm	sync
#endif

/* XXX these don't really guarantee WB (of 1) is really out ... */

 

#if MCCHIP
#ifndef R10000
#if TFP
#define MC_NOP	ssnop
#else
#define MC_NOP	nop
#endif
/*
 * Do an uncached read to flush the write buffer
 *
 * This routine is mostly used to affect changes to interrupts and
 * since it takes a little time for interrupts to propagate on the
 * R4000, some nops are added at the end.
 */
#define wbflushm_setup(RX)			  \
	CLI	RX,PHYS_TO_COMPATK1(CPUCTRL0)
#define wbflushm_reg(RX)			  \
	lw	zero,0(RX)
#define	wbflushm				  \
	.set	noreorder			; \
	.set	noat				; \
	wbflushm_setup(AT)			; \
	wbflushm_reg(AT)			; \
	MC_NOP					; \
	MC_NOP					; \
	MC_NOP					; \
	MC_NOP					; \
	MC_NOP					; \
	.set	at				; \
	.set	reorder
#else
/* mfc0 serializes execution and forces wait on lw */
#define	wbflushm				  \
	.set	noreorder			; \
	lw	zero,PHYS_TO_COMPATK1(CPUCTRL0)	; \
	sync					; \
	.set	noat				; \
	mfc0	AT,C0_SR			; \
	mfc0	AT,C0_SR			; \
	mfc0	AT,C0_SR			; \
	mfc0	AT,C0_SR			; \
	.set	at				; \
	.set	reorder
#endif /* R10000 */
#endif /* MCCHIP */

#if HEART_CHIP
/*
 * since register access to the PIU and widget space uses different
 * paths, a register read from both guarantees a previous HEART
 * register write reach its destination
 */
#define wbflushm					\
	.set	noreorder;				\
	ld	zero,PHYS_TO_COMPATK1(HEART_PRID);	\
	ld	zero,PHYS_TO_COMPATK1(HEART_WID_ID);	\
	sync;						\
	.set	reorder
#endif

#if IP32
/*
 * Do an uncached read to flush the write buffer
 *
 * This routine is mostly used to affect changes to interrupts and
 * since it takes a little time for interrupts to propagate on the
 * R4000, some nops are added at the end.
 */
#define	wbflushm				  \
	.set	noreorder			; \
	.set	noat				; \
	lui	AT,(CRM_CONTROL|K1BASE)>>16	; \
	or	AT,CRM_CONTROL&0xffff		; \
	ld	AT,0(AT)			; \
	nop					; \
	nop					; \
	nop					; \
	nop					; \
	nop					; \
	.set	at				; \
	.set	reorder
#endif

#if EVEREST || IP30 || SN
/* On 64-bit kernels, doing an LI to load the address generates about 6
 * instructions requiring several cycles to execute.  Provide an alternate
 * interface to the timer routines which lets us preserve the address
 * of the timer for subsequent calls.
 */
#if EVEREST
#define LOAD_TIMERADDR(reg)		LI	reg,EV_RTC;
#define _GET_TIMESTAMP2(reg,rtc)		\
		ld	reg,(rtc) ;		\
		dsll	reg,32 ;		\
		dsrl	reg,32 ;

#endif	/* EVEREST */

#if SN
#define LOAD_TIMERADDR(reg)		LI	reg, LOCAL_HUB_ADDR(PI_RT_COUNT);
#define _GET_TIMESTAMP2(reg,rtc)		\
		ld	reg,(rtc) ;		\
		dsll	reg,32 ;		\
		dsrl	reg,32 ;
#endif	/* SN */

#if IP30
/* Use 32-bits of heart's counter so all cpus have coordinated timestamps */
#define	LOAD_TIMERADDR(reg)			\
	CLI	reg,PHYS_TO_COMPATK1(HEART_COUNT);
#define _GET_TIMESTAMP2(reg,rtc)		\
	lwu	reg,4(rtc);
#endif

#define _GET_TIMESTAMP(reg)			\
	LOAD_TIMERADDR(reg);			\
	_GET_TIMESTAMP2(reg,reg) ;
#endif	/* EVEREST || IP30 || SN */

#if !IP20 && !IP22 && !EVEREST && !SN
#define ADD_FASTHZ_COUNTER					\
	la	k0,fclkcount ;					\
	lw	k1,0(k0) ;					\
	addiu	k1,1 ;						\
	sw	k1,0(k0)
#else
#define ADD_FASTHZ_COUNTER
#endif /* !IP20 && !IP22 && !EVEREST && !SN */

#if TFP
#if IP26
#define TLBLO_FIX_HWBITS(lo)		\
	sll	lo, TLBLO_HWBITSHIFT
#else
#define	TLBLO_FIX_HWBITS(lo)		\
	dsll	lo, TLBLO_HWBITSHIFT
#endif
#define NOINTS(x,y)			\
	LI	x,SR_DEFAULT;		\
	MTC0_NO_HAZ(x,C0_SR);		\
	NOP_MTC0_HAZ
#else	/* !TFP */
#if SR_KADDR
#define NOINTS(x,y)	li x,SR_KERN_SET; MTC0_NO_HAZ(x,C0_SR)
#else
#define NOINTS(x,y)	MTC0_NO_HAZ(zero,C0_SR)
#endif
#endif	/* !TFP */

/* TLBLO_FIX_HWBITS is used to take a pte_t and convert to TLBLO (HW) format.
 * TLBLO_UNFIX_HWBITS is used to take TLBLO (HW) and convert to pte_t (SW)
 * 		      but without the SW bits present.
 */
#if R4000
#if MCCHIP || IP32 || IPMHSIM
#define	TLBLO_FIX_HWBITS(lo)				\
        sll     lo, TLBLO_HWBITSHIFT;			\
        srl     lo, TLBLO_HWBITSHIFT
#define TLBLO_UNFIX_HWBITS(lo)
#else
#define	TLBLO_FIX_HWBITS(lo)				\
	srl	lo, TLBLO_HWBITSHIFT
#define	TLBLO_UNFIX_HWBITS(lo)				\
	sll	lo, TLBLO_HWBITSHIFT
#endif	/* MCCHIP || IP32 || IPMHSIM */
#endif	/* R4000 */

#if R10000 && !R4000
#define	TLBLO_FIX_HWBITS(lo)
#define TLBLO_UNFIX_HWBITS(lo)
#endif

/*
 * TLBLO_FIX_250MHz is a workaround for a problem seen on the 250MHz
 * R4400 modules where entrylo values may be written incorrectly when
 * certain previous values exist in the register. The workaround is to
 * make sure a zero is there before updating the register (no hazard
 * cycles needed when writing the zero). Currently this affects IP19
 * and IP22's.
 */
#if R4000 && (IP19 || IP22)
#define TLBLO_FIX_250MHz(entrylo)	mtc0 zero, entrylo
#else
#define TLBLO_FIX_250MHz(mumble)
#endif

#if IP20 || IP26 || IP28
#define	FASTCLK_ACK(x)	li x ACK_TIMER1; sb x TIMER_ACK_ADDR
#elif defined(IP22)
#define	IS_IOC1(t)				\
	lw	t, (K1BASE|HPC3_SYS_ID);	\
	andi	t, CHIP_IOC1

#define	FASTCLK_ACK(x)	\
	IS_IOC1(x);					\
	bnez	x,1f;					\
	li	x,ACK_TIMER1;				\
	sb	x,(K1BASE|TIMER_ACK_OFFSET+HPC3_INT2_ADDR);	\
	b	2f;					\
1:	li	x,ACK_TIMER1;				\
	sb	x,(K1BASE|TIMER_ACK_OFFSET+HPC3_INT3_ADDR);	\
2:
#else
#define	FASTCLK_ACK(x)	lbu	x,FASTCLK_ADDR
#endif

#if EVEREST || SN
#define	LoadCpumask	ld
#define	LLcpumask	lld
#define SCcpumask	scd
#else
#define	LoadCpumask	lw
#define	LLcpumask	ll
#define SCcpumask	sc
#endif

#if IP20 || IP22 || IP26 || IP28
#define	PTCW_SC(x)	((x)<<6)
#define	PTCW_16B	(3<<4)
#define	PTCW_MODE(x)	((x)<<1)
#define	PTCW_CLCMD	(0<<4)
#endif

#ifdef _R4600_2_0_CACHEOP_WAR

#define ERET_PATCH(xx)				  \
EXPORT(xx)					; \
	j	_r4600_2_0_cacheop_eret		; \
	nop

#if (IP20 || IP22 || IPMHSIM)
#define PD_REFILL_WAIT(xx)			  \
	.set	noreorder			; \
	PD_REFILL_WAIT_NR(xx)			; \
	.set	reorder

#if _MIPS_SIM == _ABI64
#define PD_REFILL_K1BASE	(COMPAT_K1BASE32&0xffffffff)
#else
#define PD_REFILL_K1BASE	K1BASE
#endif
	
#if (CPUCTRL0 & 0x8000) 
#define PD_REFILL_WAIT_NR(xx) 				  \
EXPORT(xx)						; \
	.set	noat					; \
	lui	AT,(CPUCTRL0|PD_REFILL_K1BASE)>>16	; \
	or	AT,CPUCTRL0&0xffff			; \
	lw	AT,0(AT)				; \
	nop						; \
	.set	at
#else
#define PD_REFILL_WAIT_NR(xx) 				  \
EXPORT(xx)						; \
	.set	noat					; \
	lui	AT,(CPUCTRL0|PD_REFILL_K1BASE)>>16	; \
	lw	AT,CPUCTRL0&0xffff(AT)			; \
	nop						; \
	.set	at
#endif
#elif IP32
#define PD_REFILL_WAIT(xx)			  \
	.set	noreorder			; \
	PD_REFILL_WAIT_NR(xx)			; \
	.set	reorder
	
#define PD_REFILL_WAIT_NR(xx) 			  \
EXPORT(xx)					; \
	.set	noat				; \
	lui	AT,(CRM_ID|K1BASE)>>16	; \
	ld	AT,CRM_ID&0xffff(AT)		; \
	nop					; \
	.set	at
#else 
<< bomb: must define PD_REFILL_WAIT and PD_REFILL_WAIT_NR >>
#endif
	
#else /* _R4600_2_0_CACHEOP_WAR */

#define ERET_PATCH(xx)   eret
#define PD_REFILL_WAIT(xx)
#define PD_REFILL_WAIT_NR(xx)

#endif /* _R4600_2_0_CACHEOP_WAR */

/*
 * clear bus error - this macro clears any registers that may have been
 * latched due to a bus error.
 */


#if MCCHIP
#ifdef IP20				/* only IP20 has rev A part. */
/*
 * The Rev A MC doesn't like to have its registers
 * written right after a block write (like a cache
 * line write back).  So we execute the stores
 * uncached which works.
 */
#define clearbuserrm				  \
	la	v0,9f				; \
	or	v0,SEXT_K1BASE			; \
	j	v0				; \
	.set	noreorder			; \
	nop; nop; nop; nop; nop; nop; nop; nop	; \
	nop; nop; nop; nop; nop; nop; nop; nop	; \
	nop; nop; nop; nop; nop; nop; nop; nop	; \
	nop; nop; nop; nop; nop; nop; nop; nop	; \
9:						; \
	sw	zero, CPU_ERR_STAT|K1BASE 	; \
	sw	zero, GIO_ERR_STAT|K1BASE 	; \
	j	9f				; \
	nop; nop; nop; nop; nop; nop; nop; nop	; \
	nop; nop; nop; nop; nop; nop; nop; nop	; \
	nop; nop; nop; nop; nop; nop; nop; nop	; \
	nop; nop; nop; nop; nop; nop; nop; nop	; \
	.set	reorder				; \
9:						; \
	wbflushm				; \
	# stall a bit so clear can make it to the cpu	; \
	wbflushm				; \
	wbflushm
#elif IP26
#define clearbuserrm					  \
	LI	v0, K1BASE|CPUCTRL0			; \
	sw	zero, CPU_ERR_STAT-CPUCTRL0(v0)		; \
	sw	zero, GIO_ERR_STAT-CPUCTRL0(v0)		; \
	lw	zero, 0(v0)				; \
	LI	v0, K1BASE|TCC_BASE			; \
	.set	noat					; \
	ld	AT,TCC_INTR-TCC_BASE(v0)		; \
	andi	AT,0xf800|INTR_MACH_CHECK|INTR_BUSERROR	; \
	sd	AT,TCC_INTR-TCC_BASE(v0)		; \
	ld	AT,TCC_ERROR-TCC_BASE(v0)		; \
	andi	AT,ERROR_NESTED_BE|ERROR_NESTED_MC	; \
	sd	AT,TCC_ERROR-TCC_BASE(v0)		; \
	.set	at					; \
	# stall a bit so clear can make it to the cpu	; \
	lw	zero, 0(v0)				; \
	lw	zero, 0(v0)
#else
#ifdef IP28
#define clearbuserrm_flush	wbflushm
#else
#define clearbuserrm_flush
#endif
#define clearbuserrm				  	  \
	CLI	v0,PHYS_TO_COMPATK1(CPUCTRL0)		; \
	sw	zero, CPU_ERR_STAT-CPUCTRL0(v0)		; \
	sw	zero, GIO_ERR_STAT-CPUCTRL0(v0)		; \
	wbflushm					; \
	# stall a bit so clear can make it to the cpu	; \
	CLI	v0, PHYS_TO_COMPATK1(HPC3_INTSTAT_ADDR)	; \
	lw	zero, 0(v0)				; \
	lw	zero, 0(v0)				; \
	clearbuserrm_flush
#endif

#if IP28		/* cannot enable GIO parity on IP28 */
/* right now gio errors do not fully work on IP28 */
#define checkbuserrm(x,y)				  \
	CLI	x,PHYS_TO_COMPATK1(CPU_ERR_STAT)	; \
	lw	x,0(x)					; \
	bnez	x,y					; \
	CLI	x,PHYS_TO_COMPATK1(GIO_ERR_STAT)	; \
	lw	x,0(x)					; \
	and	x,GIO_ERR_STAT_TIME|GIO_ERR_STAT_PROM	; \
	bnez	x,y
#else
#define checkbuserrm(x,y)				  \
	CLI	x,PHYS_TO_COMPATK1(CPU_ERR_STAT)	; \
	lw	x,0(x)					; \
	bnez	x,y					; \
	CLI	x,PHYS_TO_COMPATK1(GIO_ERR_STAT)	; \
	lw	x,0(x)					; \
	bnez	x,y
#endif
#endif	/* MCCHIP */

#ifdef IP32
#include "sys/mace.h"
#define clearbuserrm	\
	CLI	v0, PHYS_TO_COMPATK1(CRM_ID)		; \
	sd	zero, CRM_CPU_ERROR_STAT-CRM_ID(v0)	; \
	ld	zero, CRM_CPU_ERROR_STAT-CRM_ID(v0)	; \
	sd	zero, CRM_MEM_ERROR_STAT-CRM_ID(v0)	; \
	ld	zero, CRM_MEM_ERROR_STAT-CRM_ID(v0)	; \
	CLI	v0, PHYS_TO_COMPATK1(PCI_ERROR_FLAGS)	; \
	sw	zero, 0(v0)				; \
	lw	zero, 0(v0)

#define checkbuserrm(x,y)
#endif	/* IP32 */

#if EVEREST || SN || IPMHSIM
#define clearbuserrm
#define checkbuserrm(x,y)	
#endif

#if HEART_CHIP
#define clearbuserrm	\
	.set	noat						; \
	CLI	v0,PHYS_TO_COMPATK1(HEART_BASE)			; \
	ld	AT,HEART_WID_ERR_TYPE-HEART_BASE(v0)		; \
	sd	AT,HEART_WID_ERR_TYPE-HEART_BASE(v0)		; \
	sd	zero,HEART_WID_ERR_CMDWORD-HEART_BASE(v0)	; \
	ld	zero,HEART_WID_ID-HEART_BASE(v0)		; \
	CLI	v0,PHYS_TO_COMPATK1(HEART_CAUSE)		; \
	ld	AT,0(v0)					; \
	sd	AT,0(v0)					; \
	.set	at						; \
	ld	zero,PHYS_TO_COMPATK1(HEART_PRID)

#define checkbuserrm(x,y)	\
	CLI	x,PHYS_TO_COMPATK1(HEART_CAUSE)			; \
	ld	x,0(x)						; \
	and	x,HC_WIDGET_ERR					; \
	bnez	x,y
#endif	/* HEART_CHIP */

#define SAVECP1REG(reg)	swc1	$f/**/reg,PCB_FPREGS+4+reg*8(a3)
#define RESTCP1REG(reg)	lwc1	$f/**/reg,PCB_FPREGS+4+reg*8(a3)

#define EXREG_SAVECP1REG(reg)	sdc1	$f/**/reg,PCB_FPREGS+reg*8(a3)
#define EXREG_RESTCP1REG(reg)	ldc1	$f/**/reg,PCB_FPREGS+reg*8(a3)

/* The text addresses needed by error.c to track EXL changing.  */
#if R4000 && (! _NO_R4000)
#define EXL_EXPORT(sym)	EXPORT(sym)
#else
#define EXL_EXPORT(sym)
#endif

#ifdef ULI
#ifdef DEBUG
#define INCREMENT(label, reg1, reg2) \
	.set	noat		;\
	LA	reg1, label	;\
	lw	reg2, (reg1)	;\
	addiu	reg2, 1		;\
	sw	reg2, (reg1)	;\
	.set	at
#else
#define INCREMENT(a,b,c)
#endif /* DEBUG */
#endif /* ULI */

#if (_MIPS_SIM == _ABIO32)
#define SAVEAREGS(x)	\
	sreg	a0,EF_A0(x) ;\
	sreg	a1,EF_A1(x) ;\
	sreg	a2,EF_A2(x) ;\
	sreg	a3,EF_A3(x)

#define RESTOREAREGS(x)	\
	lreg	a0,EF_A0(x) ;\
	lreg	a1,EF_A1(x) ;\
	lreg	a2,EF_A2(x) ;\
	lreg	a3,EF_A3(x)
#endif

#if (_MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32)
#define SAVEAREGS(x)	\
	sreg	a0,EF_A0(x) ;\
	sreg	a1,EF_A1(x) ;\
	sreg	a2,EF_A2(x) ;\
	sreg	a3,EF_A3(x) ;\
	sreg	a4,EF_A4(x) ;\
	sreg	a5,EF_A5(x) ;\
	sreg	a6,EF_A6(x) ;\
	sreg	a7,EF_A7(x)

#define RESTOREAREGS(x)	\
	lreg	a0,EF_A0(x) ;\
	lreg	a1,EF_A1(x) ;\
	lreg	a2,EF_A2(x) ;\
	lreg	a3,EF_A3(x) ;\
	lreg	a4,EF_A4(x) ;\
	lreg	a5,EF_A5(x) ;\
	lreg	a6,EF_A6(x) ;\
	lreg	a7,EF_A7(x)
#endif

#define SAVESREGS(x)	\
	sreg	s0,EF_S0(x) ;\
	sreg	s1,EF_S1(x) ;\
	sreg	s2,EF_S2(x) ;\
	sreg	s3,EF_S3(x) ;\
	sreg	s4,EF_S4(x) ;\
	sreg	s5,EF_S5(x) ;\
	sreg	s6,EF_S6(x) ;\
	sreg	s7,EF_S7(x)

#define RESTORESREGS(x)	\
	lreg	s0,EF_S0(x) ;\
	lreg	s1,EF_S1(x) ;\
	lreg	s2,EF_S2(x) ;\
	lreg	s3,EF_S3(x) ;\
	lreg	s4,EF_S4(x) ;\
	lreg	s5,EF_S5(x) ;\
	lreg	s6,EF_S6(x) ;\
	lreg	s7,EF_S7(x)

#if R4000 && EVEREST
/* The following define causes us to use a fast syscall path in locore
 * which eliminates many NOPs in systrap which were used to avoid
 * hazards.  This fast path "should" work for all R4000 platforms, but
 * has only been tested on EVEREST. After further testing this define can
 * be removed and the fast path made standard for all R4000's.
 */
#define	R4000_FAST_SYSCALL	1
#endif

#ifdef TLBDEBUG
	/* read back TLB slots before replacing with upage */
#if TFP
#define SVSLOTS \
	DMFC0_BADVADDR(k1)	;\
	DMTC0(k1, C0_WORK0)	;\
	DMFC0(k1, C0_TLBSET)	;\
	DMFC0(AT, C0_TLBHI)	;\
	DMTC0(zero, C0_TLBSET)	;\
	LI	k0, KSTACKPAGE	;\
	DMTC0(k0, C0_BADVADDR)	;\
	TLB_READ	;\
	DMFC0(k0, C0_TLBHI)	;\
	sw	k0, VPDA_SV1HI(zero)	;\
	DMFC0(k0, C0_TLBLO)	;\
	sw	k0, VPDA_SV1LO(zero)	;\
	DMTC0(k1, C0_TLBSET)	;\
	DMFC0(k1, C0_WORK0)	;\
	DMTC0(k1, C0_BADVADDR)	;\
	DMTC0(AT, C0_TLBHI)
#endif	/* TFP */

#if R4000 || R10000
#define SVSLOTS \
	DMFC0(AT, C0_TLBHI)	;\
	li	k0, UKSTKTLBINDEX	;\
	mtc0	k0, C0_INX	;\
	NOP_1_1	;\
	c0	C0_READI	;\
	NOP_1_3	;\
	DMFC0(k0, C0_TLBHI)	;\
	NOP_1_1	;\
	sw	k0, VPDA_SV1HI(zero)	;\
	mfc0	k0, C0_TLBLO	;\
	NOP_1_1	;\
	sw	k0, VPDA_SV1LO(zero) ;\
	mfc0	k0, C0_TLBLO_1	;\
	NOP_1_1	;\
	sw	k0, VPDA_SV1LO_1(zero) ;\
	DMTC0(AT, C0_TLBHI)	;
	/*
	 * R4000 NOTE: ***potential CP0 scheduling hazard*** be careful
	 * that the next instruction isn't a dangerous one.
	 * (eg: MFC0 from TLBHI or a TLBR, TLBWI, TLBWR, or TLBP)
	 */
#endif	/* R4000 || R10000 */

#else	/* TLBDEBUG */

#define SVSLOTS

#endif	/* TLBDEBUG */

#ifdef TFP
/*	SAVESREGS_GETCOP0
 *
 *	This is a special macro to take advantage of the superscaler TFP.
 *	TFP can only perform one IU store each cycle, but we can piggy back
 *	reads from COP0 for "free".  So this routine will save the
 *	s-registers and load interesting COP0 registers.
 */
#define SAVESREGS_GETCOP0(x)	\
	sreg	s0,EF_S0(x) ;\
	sreg	s1,EF_S1(x) ;\
	dmfc0	s0,C0_SR    ;\
	sreg	s2,EF_S2(x) ;\
	sreg	s3,EF_S3(x) ;\
	dmfc0	s1,C0_CONFIG;\
	sreg	s4,EF_S4(x) ;\
	sreg	s5,EF_S5(x) ;\
	dmfc0	s2,C0_CAUSE ;\
	sreg	s6,EF_S6(x) ;\
	sreg	s7,EF_S7(x) ;\
	dmfc0	s3,C0_EPC

/*	RESTORESREGS_SR_EPC
 *	
 *	This macro restores all s-registers as well as the SR and EPC.
 */
#define RESTORESREGS_SR_EPC(x)	\
	lreg	s6,EF_SR(x) ;\
	lreg	s7,EF_EPC(x);\
	lreg	s0,EF_S0(x) ;\
	lreg	s1,EF_S1(x) ;\
	dmtc0	s6,C0_SR    ;\
	lreg	s2,EF_S2(x) ;\
	lreg	s3,EF_S3(x) ;\
	lreg	s4,EF_S4(x) ;\
	dmtc0	s7,C0_EPC   ;\
	lreg	s5,EF_S5(x) ;\
	lreg	s6,EF_S6(x) ;\
	lreg	s7,EF_S7(x)

/* If came from user code, then save FP temp regs in the PCB of
 * the FPOWNER (if there is one).  If came from kernel, we save
 * FP temps in the exception frame.
 * NOTE: Must be inside "reorder"
 * 	srreg == SR at time of exception
 */
#define	SAVEFPREGS(srreg,x,temp) \
	LI	temp,SR_CU1		;\
	and	temp,srreg		;\
	beq	temp,zero,1f		;\
	andi	temp,srreg,SR_KSU_MSK	;\
	sdc1	$f4,EF_FP4(x)		;\
	beq	temp,zero,1f		;\
	PTR_L	temp,VPDA_FPOWNER(zero)	;\
	PTR_L	temp,UT_EXCEPTION(temp)	;\
	sdc1	$f4,PCB_FPREGS+4*8(temp) ;\
1:

	/* We're returning to user code, so if there is an FPOWNER we
	 * must restore the FP temp register values from his PCB. We
	 * actually check to see if SR_CU1 is set.  If it isn't, then
	 * there's no reason to reload FP temps since they can't be
	 * accessed.  If is is, then the user we're about to execute
	 * MUST be the FPOWNER and that field has to be non-zero.
	 * NOTE: Must be inside "reorder"
	 *
	 *	 If C0_SR has FR clear (i.e. 32-bit FP registers) then
	 *	 this macro restores $f4 and $f5, otherwise just 64-bit $f4.
	 *	 Must be executing in the same FR mode as when the SAVEFPREGS
	 *	 was done (i.e. same FR mode as user process).
	 */
#define	RESTOREFPREGS_TOUSER(temp) \
	PTR_L	temp,VPDA_FPOWNER(zero) ;\
	beq	temp,zero,1f		;\
	PTR_L	temp,UT_EXCEPTION(temp)	;\
	ldc1	$f4,PCB_FPREGS+4*8(temp) ;\
1:

	/* We're returning to kernel code, so restore FP temp regs from
	 * the current exception frame.
	 */
#define	RESTOREFPREGS_TOKERNEL(x) \
	ldc1	$f4,EF_FP4(x)

#endif	/* TFP */

#if defined (BEAST)
#if defined (PSEUDO_BEAST)
#define	SAVEFPREGS(srreg,x,temp)
#define	RESTOREFPREGS_TOUSER(temp)
#define	RESTOREFPREGS_TOKERNEL(x)

/*	RESTORESREGS_SR_EPC
 *	
 *	This macro restores all s-registers as well as the SR and EPC.
 */
#define RESTORESREGS_SR_EPC(x)	\
	lreg	s6,EF_SR(x) ;\
	lreg	s7,EF_EPC(x);\
	lreg	s0,EF_S0(x) ;\
	lreg	s1,EF_S1(x) ;\
	dmtc0	s6,C0_SR    ;\
	lreg	s2,EF_S2(x) ;\
	lreg	s3,EF_S3(x) ;\
	lreg	s4,EF_S4(x) ;\
	dmtc0	s7,C0_EPC   ;\
	lreg	s5,EF_S5(x) ;\
	lreg	s6,EF_S6(x) ;\
	lreg	s7,EF_S7(x)

#else /*!PSEUDO_BEAST */

#error <BOMB: Need to fix the above for beast correctly.>

#endif /* !PSEUDO_BEAST */

#endif /* BEAST */

#if defined(R4000) || defined(R10000)

#define	SAVEFPREGS(srreg,x,temp)
#define	RESTOREFPREGS_TOUSER(temp)
#define	RESTOREFPREGS_TOKERNEL(x)

/*	RESTORESREGS_SR_EPC
 *	
 *	This macro restores all s-registers as well as the SR and EPC.
 *
 * NOTE:
 * We turn on the EXL bit here so that we avoid modifying both the
 * EXL bit & KSU bits at the same time later on.  The R4000 chip
 * can end up executing the 5 instructions following the "mtc0"
 * in user mode, instead of kernel mode if the "mtc0" is changing
 * KSU to user mode and is setting the EXL bit from the clear state.
 * Problem is avoided by setting EXL early.  This avoids adding
 * 5 nops following the "mtc0 k0,C0_SR".
 */
#define RESTORESREGS_SR_EPC(x, curSR)	\
	ori	curSR,SR_EXL;\
	mtc0	curSR,C0_SR ;\
	lreg	s6,EF_SR(x) ;\
	lreg	s7,EF_EPC(x);\
	lreg	s0,EF_S0(x) ;\
	mtc0	s6,C0_SR    ;\
	lreg	s1,EF_S1(x) ;\
	lreg	s2,EF_S2(x) ;\
	lreg	s3,EF_S3(x) ;\
	DMTC0(s7,C0_EPC)   ;\
	lreg	s4,EF_S4(x) ;\
	lreg	s5,EF_S5(x) ;\
	lreg	s6,EF_S6(x) ;\
	lreg	s7,EF_S7(x)

#endif	/* !TFP */

#if MCCHIP || IPMHSIM
#define	SAVE_ERROR_STATE(R1,R2)		\
	SAVE_MC_STATE(R1,R2);		\
	SAVE_HPC3_STATE(R1,R2)

#define SAVE_MC_STATE(R1,R2)					 \
	CLI	R1,PHYS_TO_COMPATK1(CPUCTRL0)			;\
	lw	R2,CPU_ERR_STAT-CPUCTRL0(R1)			;\
	sw	R2,cpu_err_stat					;\
	lw	R2,GIO_ERR_STAT-CPUCTRL0(R1)			;\
	sw	R2,gio_err_stat					;\
	lw	R2,CPU_ERR_ADDR-CPUCTRL0(R1)			;\
	sw	R2,cpu_par_adr					;\
	lw	R2,GIO_ERR_ADDR-CPUCTRL0(R1)			;\
	sw	R2,gio_par_adr

#if IP22 || IP26 || IP28
#define SAVE_HPC3_STATE(R1,R2)					 \
	lw	R2,PHYS_TO_COMPATK1(HPC3_EXT_IO_ADDR)		;\
	lw	R1,PHYS_TO_COMPATK1(HPC3_BUSERR_STAT_ADDR)	;\
	sw	R2,hpc3_ext_io					;\
	sw	R1,hpc3_bus_err_stat
#else
#define SAVE_HPC3_STATE(R1,R2)
#endif

#endif	/* MCCHIP */

#if PDA_CURUSRSTK != 0
much of this code makes this assumption!
#endif

#ifdef ULI_TSTAMP1

#define ULI_GET_TS(r1, r2, num, which)		\
	.set	reorder				;\
	CPUID_L	r1, VPDA_CPUID(zero)		;\
	subu	r1, TSTAMP_CPU			;\
	bne	r1, zero, 1f			;\
	LOAD_TIMERADDR(r2)			;\
	_GET_TIMESTAMP2(r2, r2)			;\
	LA	r1, which			;\
	sw	r2, num*ULITS_SIZE(r1)		;\
1:

#endif	/* ULI_TSTAMP1 */

#ifdef _MEM_PARITY_WAR
#define EF_TLBHI	(EF_SIZE+(0*SZREG))
#define EF_TLBLO_0	(EF_SIZE+(1*SZREG))
#define EF_TLBLO_1	(EF_SIZE+(2*SZREG))
#define EF_TAGLO	(EF_SIZE+(3*SZREG))
#define EF_TAGHI	(EF_SIZE+(4*SZREG))
#define EF_ERROR_EPC	(EF_SIZE+(5*SZREG))
#define EF_CACHE_ERR	(EF_SIZE+(6*SZREG))
#define EF_ECC		(EF_SIZE+(7*SZREG))
#define EF_ECCFRAME	(EF_SIZE+(8*SZREG))

#define EF_EXTENDED_SIZE (( (EF_SIZE+(8*SZREG)+ECCF_SIZE) + 0xF)&~0xF)

#endif	/* _MEM_PARITY_WAR */

#if IP32
#define LOAD_CRIME_INTMASK(offset,mask,temp)    \
	ld      mask,VPDA_SPLMASKS(offset)      ; \
	li      temp,CRM_INTMASK|K1BASE         ; \
	sd      mask,0(temp)

#define BUILD_RETURN_VALUE(rv,temp)             \
	lw      temp,VPDA_CURSPL(zero)          ; \
	and     rv,~SR_IMASK                    ; \
	.set noat                               ; \
	sll     AT,temp,8                       ; \
	or      rv,AT                           ; \
	.set at 
    
#define EXTRACT_OLD_SPL(sr,oldlvl)              \
	and     oldlvl,sr,SR_IMASK              ; \
	srl     oldlvl,8        
    
#define BUILD_SR_IMASK(sr,oldlvl,masks)         \
	EXTRACT_OLD_SPL(sr, oldlvl)             ; \
	and     sr,~SR_IMASK                    ; \
	la      masks,sr_iplmasks               ; \
	addu    masks,oldlvl                    ; \
	ld      masks,0(masks)                  ; \
	or      sr,masks

#define CRIME_SPLHI(spl,offset,mask,temp)	\
	li      offset,1<<3 			; \
	lw      spl,VPDA_CURSPL(zero)		; \
	ble     offset,spl,1f			; \
	LOAD_CRIME_INTMASK(offset,mask,temp)	; \
	sw      offset,VPDA_CURSPL(zero)	; \
1: 

#define CRIME_SPLX(offset,mask,temp)		\
	LOAD_CRIME_INTMASK(offset,mask,temp)	; \
	sw      offset,VPDA_CURSPL(zero) 
#else
#define CRIME_SPLHI(spl,offset,mask,temp)
#define CRIME_SPLX(offset,mask,temp)
#endif
