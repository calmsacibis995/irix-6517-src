#ident "$Revision: 1.1 $"

#include "sys/asm.h"
#include "sys/regdef.h"
#include "sys/fpu.h"
#include "sys/softfp.h"
#include "sys/signal.h"
#include "sys/sbd.h"
#include "IP32_assym.h"

#ifdef	EVEREST
#include "sys/EVEREST/everest.h"
#include "sys/EVEREST/evintr.h"
#endif	/* EVEREST */
#if TFP
#include "sys/tfp.h"
#endif

#if defined(_K32U64) || defined(_K64U64)
#define lreg	ld
#define sreg	sd
#define BPREG	8
#else
#define lreg	lw
#define sreg	sw
#define BPREG	4
#endif

#ifdef	PTE_64BIT
#define	PTE_L		ld
#define	PTE_S		sd
#define	PTESZ_WORD	.dword
#else
#define	PTE_L		lw
#define	PTE_S		sw
#define	PTESZ_WORD	.word
#endif	/* PTE_64BIT */

/*
 * flush any write buffer - this macro/routine needs to make sure that any
 * write-behind writes are out and any possible errors (bus-error) have had
 * time to be received
 * WARNING: uses label 9
 */

#if IP19 || IP21 || IP25 || IPMHSIM
#define wbflushm	/* TBD */
#endif

#if IP17
#define wbflushm		\
	.set	noreorder ;	\
	.set	noat	;	\
	lui	AT,(MPSR_ADDR>>16)	/* force a xfer through the WB chip */;\
	lw	AT,0(AT) ;\
	nop		 ;\
	.set	at ; \
	.set	reorder
#endif


/* XXX these don't really guarantee WB (of 1) is really out ... */

 

#if MCCHIP
#if TFP || R10000
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
	LI	RX,(CPUCTRL0|K1BASE)
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
#endif

#if IP30
#define wbflushm				\
	.set	noreorder;			\
	.set	noat;				\
	li	AT,PHYS_TO_COMPATK1(HEART_SYNC);\
	ld	zero,0(AT);			\
	sync;					\
	.set	at;				\
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

#if IP19 || IP21 || IP25 || (IP30 && MP)
/* On 64-bit kernels, doing an LI to load the address generates about 6
 * instructions requiring several cycles to execute.  Provide an alternate
 * interface to the timer routines which lets us preserve the address
 * of the timer for subsequent calls.
 */
#if SABLE && EVEREST
#define	LOAD_TIMERADDR(reg)	\
	LI	reg,EV_RTC;
#define _GET_TIMESTAMP2(reg,rtc)	\
	ld	reg,(rtc) ;	\
	dsll	reg,32 ;		\
	dsrl	reg,32 ;
#elif EVEREST
#define	LOAD_TIMERADDR(reg)	\
	LI	reg,EV_RTC;
#define _GET_TIMESTAMP2(reg,rtc)	\
	lw	reg,4(rtc) ;
#elif IP30
/* Use 32-bits of heart's counter so all cpus have coordinated timestamps */
#define	LOAD_TIMERADDR(reg)	\
	CLI	reg,PHYS_TO_COMPATK1(HEART_COUNT);
#define _GET_TIMESTAMP2(reg,rtc)		\
	lwu	reg,4(rtc);
#endif

#define _GET_TIMESTAMP(reg)	\
	LOAD_TIMERADDR(reg);	\
	_GET_TIMESTAMP2(reg,reg) ;

#define USR2SYS_TIMER_SWITCH2(rtc) /* trash a0-3,ra,t0-7 and need stack */ \
	_GET_TIMESTAMP2(a0,rtc)					\
	jal	usr2sys_timer_switch				

#define BACK2USR_TIMER_SWITCH2(rtc) /* trash a0-3,ra,t0-7 and need stack */ \
	_GET_TIMESTAMP2(a0,rtc)					\
	jal	back2usr_timer_switch				

#define USR2SYS_TIMER_SWITCH	/* trash a0-3,ra,t0-7 and need stack */ \
	_GET_TIMESTAMP(a0)					\
	jal	usr2sys_timer_switch				

#define BACK2USR_TIMER_SWITCH	/* trash a0-3,ra,t0-7 and need stack */ \
	_GET_TIMESTAMP(a0)					\
	jal	back2usr_timer_switch				

#define INTTRAP_TIMER_SWITCH	/* trash a0-3,ra,t0-7 and need stack */ \
	lw	a1, VPDA_TIMERSTATE(zero) ;			\
	beq	a1, zero, 1f ;					\
	_GET_TIMESTAMP(a0)					\
	jal	inttrap_timer_switch ;				\
1:

/* really intr timer back to idle or process system timer */
#define INT2KERN_TIMER_RETURN					\
	_GET_TIMESTAMP(a0)					\
	jal	int2kern_timer_return 

#define INT2KERN_TIMER_SWITCH					\
	_GET_TIMESTAMP(a0)					\
	jal	int2kern_timer_switch 
#endif
#if IP17 || IP20 || IP22 || IP26 || IP28 || (IP30 && !MP) || IP32 || IPMHSIM
/* take advantage of the C0_COUNT register -- must be called .set reorder */
#define USR2SYS_TIMER_SWITCH	/* trash a0-3,ra,t0-7 and need stack */ \
	jal	get_r4k_counter ;				\
	move	a0, v0 ;					\
	jal	usr2sys_timer_switch				

#define BACK2USR_TIMER_SWITCH	/* trash a0-3,ra,t0-7 and need stack */ \
	jal	get_r4k_counter ;				\
	move	a0, v0 ;					\
	jal	back2usr_timer_switch				

#define INTTRAP_TIMER_SWITCH	/* trash a0-3,ra,t0-7 and need stack */ \
	lw	a1, VPDA_TIMERSTATE(zero) ;			\
	beq	a1, zero, 1f ;					\
	jal	get_r4k_counter ;				\
	move	a0, v0 ;					\
	jal	inttrap_timer_switch ;				\
1:

/* really intr timer back to idle or process system timer */
#define INT2KERN_TIMER_RETURN					\
	jal	get_r4k_counter ;				\
	move	a0, v0 ;					\
	jal	int2kern_timer_return 

#define INT2KERN_TIMER_SWITCH					\
	jal	get_r4k_counter ;				\
	move	a0, v0 ;					\
	jal	int2kern_timer_switch 
#endif /* IP17 || IP20 || IP22 || IP26 || IP28 || IP30 || IP32 || IPMHSIM */

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
#endif	/* R3000 || R4000 */

#if R10000
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

#ifdef EVEREST
#define	LoadCpumask	ld
#define	LLcpumask	lld
#define SCcpumask	scd
#else
#define	LoadCpumask	lw
#define	LLcpumask	ll
#define SCcpumask	sc
#endif

#if IP17 || IP20 || IP22 || IP26 || IP28
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

#ifdef _K64U64
#define PD_REFILL_K1BASE	COMPAT_K1BASE32	
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
