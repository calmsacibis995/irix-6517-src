#if TFP

#include <sys/immu.h>
#include <sys/cpu.h>
#include <asm.h>
#include "ml.h"

#ident "$Revison$"

/*
 * This table is page aligned. _hook_exception() just points
 * C0_TRAPBASE to it.  This is in a seperate file so ld can
 * help save the alignment pad.  This saves 2K in the teton
 * prom.
 *
 *  For Teton (IP26) this also entails the initial 'boot' exeception
 * vector by checking the TETON_BEV (C0_WORK0) register.  This saves
 * 4K+ from the teton prom.
 */

	.align	12
EXPORT(_trap_table)
LEAF(_tlbrefill)			/* TrapBase + 0x000 */
	.set	noreorder
	.set	noat
#if IP26
	DMFC0	(k0,TETON_BEV)
	beq	zero,k0,1f		# non BEV exeception
	nop
	li	k1,4
	b	bev_common
	nop
1:
#endif
	j	_j_exceptutlb	/* FIXME: what should we really do? */
	nop
	.set	at
	.set	reorder
	END(_tlbrefill)

	.align	10

LEAF(_ktlbrefill_private)		/* TrapBase + 0x400 */
	.set	noreorder
	.set	noat
#if IP26
	DMFC0	(k0,TETON_BEV)
	beq	zero,k0,1f		# non BEV exeception
	nop
	li	k1,3
	b	bev_common
	nop
1:
#endif
	j	_j_exceptutlb	/* FIXME: what should we really do? */
	nop
	.set	at
	.set	reorder
	END(_ktlbrefill_private)

	.align	10

LEAF(_ktlbrefill_global)		/* TrapBase + 0x400 */
	.set	noreorder
	.set	noat
#if IP26
	DMFC0	(k0,TETON_BEV)
	beq	zero,k0,1f		# non BEV exeception
	nop
	li	k1,2
	b	bev_common
	nop
1:
#endif
	j	_j_exceptutlb	/* FIXME: what should we really do? */
	nop
	.set	at
	.set	reorder
	END(_ktlbrefill_global)

	.align	10

LEAF(_except_general)			/* TrapBase + 0xc00 */
	.set	noreorder
	.set	noat
#if IP26
	b	check_powintr		# check power interrupt
	nop
enorm:
#endif
	j	_j_exceptnorm	/* FIXME: what should we really do? */
	nop
	.set	at
	.set	reorder
	END(_except_general)

#if IP26
LEAF(check_powintr)
	.set	noat
	LI	k1,PHYS_TO_K1(HPC3_INT2_ADDR)	# use INT2 address

	/* check if we have a power interrupt */
	lb	k0,LIO_1_ISR_OFFSET(k1)
	andi	k0,LIO_POWER
	beqz	k0,2f				# branch if not a button
	.set	at

	/* Switch hit, turn off watchdog timer -- since we are gonna
	 * kill power anyway, feel free to use more than just k[01].
	 */
EXPORT(powerdown)
	LI	t0,PHYS_TO_K1(CPUCTRL0);
	LI	v0,PHYS_TO_K1(HPC3_PBUS_RTC_1286)
	lw	k0,0x2c(v0)			# read command
	ori	k0,k0,WTR_DEAC_WAM		# deactivate watchdog
	sw	k0,0x2c(v0)			# store command
	lw	zero,0(t0)			# WBFLUSHM
	sw	zero,0x30(v0)			# zero hundreths
	lw	zero,0(t0)			# WBFLUSHM
	sw	zero,0x34(v0)			# zero seconds
	lw	zero,0(t0)			# WBFLUSHM

	/* wait for power switch to be released */
9:	LI	k0,PHYS_TO_K1(HPC3_PANEL)
	li	k1,POWER_ON
	sw	k1,0(k0)
	LI	k0,PHYS_TO_K1(CPUCTRL0)
	lw	zero,0(k0)			# WBFLUSHM

	/* delay ~5ms */
	.set	noreorder
	li	k0,1000000
10:	bne	k0,zero,10b
	sub	k0,1
	.set	reorder

	/* wait for interrupt to go away */
	LI	k0,PHYS_TO_K1(HPC3_INT2_ADDR+LIO_1_ISR_OFFSET)
	lb	k1,0(k0)
	and	k1,LIO_POWER
        bne	k1,zero,9b

	/* Now go ahead and power down */
hitpow:	li	k0,~POWER_SUP_INHIBIT
	LI	k1,PHYS_TO_K1(HPC3_PANEL)
	sw	k0,0(k1)
	lw	zero,0(t0)			# WBFLUSHM

delay:	li	v1,10				# wait ~10us
	lw	k0,0x2c(v0)			# read command register
	addi	v1,-1
	bgt	zero,v1,delay

	/* If power not out, maybe "wakeupat" time hit the window.
	 */
	andi	v1,k0,WTR_TDF			# k0 holds command from delay
	beq	v1,zero,hitpow

	/* Disable and clear alarm.
	 */
	ori	k0,k0,WTR_DEAC_TDM		# k0 holds command from delay
	sw	k0,0x2c(v0)			# deactive timer
	lw	zero,0(t0)			# WBFLUSHM
	lw	zero,0x14(v0)			# read hour to clear alarm

	b	hitpow				# try power again

	/* Not a power interrupt -- decide if we should do bev or
	 * real handler.
	 */
2:
	.set	noreorder
	DMFC0	(k0,TETON_BEV)
	.set	reorder
	beq	zero,k0,1f			# not BEV exeception
	li	k1,1
	b	bev_common
1:
	b	enorm				# go back to exception
	END(check_powintr)

LEAF(bev_common)
	.set	noat
	li	k1,0x3fffffff			# for BEV, force call uncached
	and	k0,k1
	LI	k1,K1BASE
	or	k0,k1
	jr	k0				# call hander (already in k0)
1:	b	1b				# spin
	.set	at
	END(bev_common)
#endif /* IP26 */

#endif /* TFP */
