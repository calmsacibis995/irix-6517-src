#ident	"lib/libsk/ml/IP30asm.s:  $Revision: 1.39 $"

/*
 * IP30asm.s - IP30 specific assembly language functions
 */

#include "ml.h"
#include <asm.h>
#include <regdef.h>
#include <early_regdef.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/RACER/gda.h>
#include <sys/ds1687clk.h>
#include <sys/RACER/IP30nvram.h>

/* XXX may need one for each ASIC XXX */
/*
 * wbflush() -- spin until write buffer empty
 */
LEAF(wbflush)
XLEAF(flushbus)
	ld	zero,PHYS_TO_COMPATK1(HEART_BASE)
	/* mfc0 serializes execution and forces wait on ld */
	sync
	.set	noreorder
	mfc0	v0,C0_SR
	mfc0	v0,C0_SR
	.set	reorder
	j	ra
	END(wbflush)

/* long splerr(void)
 *	Set SR to ignore interrupts and errors.
 */
LEAF(splerr)
	.set	noreorder
	MFC0(a0,C0_SR)
	li	v0,~SR_IMASK			# all interrupts off
	and	a0,a0,v0
	ori	a0,SR_IE|SR_IBIT_BERR		# enable exceptions
	# FALLSTHROUGH to spl to set SR

/*
 * old_sr = spl(new_sr) -- set the interrupt level (really the sr)
 *	returns previous sr
 */
XLEAF(spl)
	.set	noreorder
	MFC0(v0,C0_SR)
	li	t0,~SR_IMASK
	and	t0,v0,t0
	or	t0,t0,a0
	MTC0(t0,C0_SR)
	.set	reorder
	j	ra
	END(splerr)

#define	HEART_10MS_COUNT	(HEART_COUNT_RATE/100)
#define	HEART_1MS_COUNT		(HEART_COUNT_RATE/1000)
/*
 * use the HEART fixed time-base register as a reference to measure number
 * of CPU ticks (processor clock) in 10ms
 */

LEAF(_ticksper10ms)
	LI	t0,PHYS_TO_COMPATK1(HEART_COUNT)# fixed time-base counter addr
	LI	a0,HEART_10MS_COUNT
1:
	.set	noreorder
	ld	a1,0(t0)		# HEART initial counter value
	MFC0(v0,C0_COUNT)		# CPU initial count value
	ld	a2,0(t0)
2:
	MFC0(v1,C0_COUNT)
	dsubu	a2,a2,a1
	blt	a2,a0,2b
	ld	a2,0(t0)		# BDSLOT
	.set 	reorder

	dsubu	v0,v1,v0		# ok even with wrap around
	dsll32	v0,0			# get rid of sign extension bits
	dsrl32	v0,0

	j	ra
	END(_ticksper10ms)

/* return processor id */
LEAF(cpuid)
	ld	v0,PHYS_TO_COMPATK1(HEART_PRID)	# as will fill delay slot
	j	ra
	END(cpuid)

/*
 * call_coroutine()
 * 	Switches stacks and calls a subroutine using the given stack
 * 	with the specified parameter.
 *
 * On entry:
 *	a0 -- The address of the subroutine to call.
 *	a1 -- The parameter to call the subroutine with.
 * 	a2 -- The stack to use for the subroutine.
 */

LEAF(call_coroutine)
	.set	reorder
	PTR_SUBU	a2,BPREG*2	# Allocate enough space for ra and sp
	sreg	ra,BPREG(a2)		# Save the return address
	sreg	sp,0(a2)		# Save the stack pointer

	move	t0,a0			# Move routine address to t0
	move	a0,a1			# Shift the parameter down
	move	sp,a2			# Change the stack pointer

	jal	t0			# Call the subroutine

	lreg	ra,BPREG(sp)		# Get the return address back
	lreg	sp,0(sp)		# Restore the old stack pointer

	j	ra
	END(call_coroutine)

#if NVOFF_AUTOPOWER > 114
#error NVOFF_AUTOPOWER must be in bank 0 64-bit chunk.
#endif

LEAF(ip30_power_switch_on)
	LI	t3,IOC3_PCI_DEVIO_K1PTR|IOC3_SIO_CR;	# slow dallas mode
	lw	a2,0(t3)				# read value
	and	a1,a2,~SIO_SR_CMD_PULSE			# tweak it
	or	a1,0x8 << SIO_CR_CMD_PULSE_SHIFT;
	sw	a1,0(t3)				# write back
	lw	zero,0(t3)				# flushbus
	lw	zero,0(t3)				# flushbus
	LI	t0,RTC_ADDR_PORT			# clock address
	LI	t1,RTC_DATA_PORT			# clock data
	/* select bank 0 and save autopower value */
	move	a7,zero					# default autopower=n
	li	a0,RTC_REG_NUM(RTC_CTRL_A)
	sb	a0,0(t0)
	li	a1,RTC_OSCILLATOR_ON
	sb	a1,0(t1)
	li	a0,RTC_REG_NUM(RTC_CTRL_B)		# select ctrl B
	sb	a0,0(t0)				# ensure intrs off
	lb	a1,0(t1)
	andi	a1,RTC_INHIBIT_UPDATE			# maintain inhibit
	ori	a1,RTC_BINARY_DATA_MODE | RTC_24HR_MODE;
	sb	a0,0(t0)
	sb	a1,0(t1)
	li	a0,RTC_REG_NUM(NVRAM_REG_OFFSET+NVOFF_AUTOPOWER)
	sb	a0,0(t0)
	lb	a1,0(t1)
	beq	a1,0x6e,1f				# 'n'
	beq	a1,0x4e,1f				# 'N'
	li	a7,RTC_PWR_ACTIVE_ON_FAIL		# autopower=y
	/* select bank 1 for all extended control accesses */
1:	li	a0,RTC_REG_NUM(RTC_CTRL_A)		# select ctrl A
	sb	a0,0(t0)
	li	a1,RTC_BANK(RTC_X_CTRL_A)|RTC_OSCILLATOR_ON
	sb	a1,0(t1)
	li	a0,RTC_REG_NUM(RTC_X_CTRL_A)		# select ext ctrl A
	sb	a0,0(t0)
	lbu	v0,0(t1)				# save status for rc
	sb	a0,0(t0)
	sb	zero,0(t1)				# clear intr flags
	li	a0,RTC_REG_NUM(RTC_X_CTRL_B)		# select ext ctrl B
	sb	a0,0(t0)
	lb	a1,0(t1)				# read ext ctrl B
	/* preserve RTC_CRYSTAL_125_PF, clear RTC_WAKEUP_ALARM_INTR_ENABLE */
	and	a1,RTC_CRYSTAL_125_PF			# preserve flag
	or	a1,RTC_KICKSTART_INTR_ENABLE | RTC_AUX_BAT_ENABLE
	or	a1,a7					# or autopower/PRS?
	sb	a0,0(t0)				# select ext ctrl B
	sb	a1,0(t1)				# save new value
	li	a0,RTC_REG_NUM(RTC_CTRL_C)		# select ctrl C
	sb	a0,0(t0)
	lb	zero,0(t1)				# load C to clear intrs
	/* latch scratch register to redirect any power down noise */
	li	a0,RTC_REG_NUM(NVRAM_REG_OFFSET)
	sb	a0,0(t0)
	lb	zero,0(t1)
	sw	a2,0(t3)				# return IOC3_SIO_CR
	lw	zero,0(t3)				# flushbus
	lw	zero,0(t3)				# flushbus
	j	ra
	END(ip30_power_switch_on)

LEAF(powerspin)
7:	LI	a0,PHYS_TO_COMPATK1(BRIDGE_BASE|BRIDGE_INT_STATUS)
	lw	a0,0(a0)
	andi	a0,PWR_SWITCH_INTR
	beqz	a0,7b
	/*FALLSTRHOUGH*/
XLEAF(bev_poweroff)
	/* Spin for 1/2 of a second to cover subtle switch bouncing */
	LI	a0,PHYS_TO_COMPATK1(HEART_COUNT)
	ld	a1,0(a0)
	li	a0,HEART_COUNT_RATE/2	# 1/2 second
	daddu	a1,a0			# base + 1/2 second
1:	LI	a0,PHYS_TO_COMPATK1(HEART_COUNT)
	ld	a0,0(a0)
	dsub	a0,a1,a0
	bgtz	a0,1b

	/* Turn off the power now */
	LI	a0,IOC3_PCI_DEVIO_K1PTR|IOC3_SIO_CR;	# go to slow dallas mode
	lw	a1,0(a0)				# read value
	and	a1,~SIO_SR_CMD_PULSE			# tweak it
	or	a1,0x8 << SIO_CR_CMD_PULSE_SHIFT;
	sw	a1,0(a0)				# write back
	lw	zero,0(a0)				# flushbus
	lw	zero,0(a0)				# flushbus
	LI	t0,RTC_ADDR_PORT			# clock address
	LI	t1,RTC_DATA_PORT			# clock data
	/* select bank 1 for all extended control accesses */
	li	a0,RTC_REG_NUM(RTC_CTRL_A)		# select ctrl A
	sb	a0,0(t0)
	li	a1,RTC_BANK(RTC_X_CTRL_A)|RTC_OSCILLATOR_ON
	sb	a1,0(t1)
	li	a0,RTC_REG_NUM(RTC_X_CTRL_A)		# select ext ctrl A
	sb	a0,0(t0)
	sb	zero,0(t1)				# clear intr flags
	li	a0,RTC_REG_NUM(RTC_X_CTRL_B)		# select ext ctrl B
	sb	a0,0(t0)
	lb	a1,0(t1)				# read in value
	/* preserve RTC_CRYSTAL_125_PF and RTC_WAKEUP_ALARM_INTR_ENABLE */
	and	a1,RTC_CRYSTAL_125_PF|RTC_WAKEUP_ALARM_INTR_ENABLE| \
		   RTC_PWR_ACTIVE_ON_FAIL
	or	a1,RTC_KICKSTART_INTR_ENABLE|RTC_AUX_BAT_ENABLE
	sb	a0,0(t0)				# select ext ctrl B
	sb	a1,0(t1)				# ensure kickstart on
2:	li	a0,RTC_REG_NUM(RTC_X_CTRL_A)		# select ext ctrl A
	sb	a0,0(t0)
	li	a0,RTC_POWER_INACTIVE			# turn power off
	sb	a0,0(t1)
	/* latch scratch register to redirect any power down noise */
	li	a0,RTC_REG_NUM(NVRAM_REG_OFFSET)
	sb	a0,0(t0)
	lb	zero,0(t1)

	/* Spin for a 10 seconds -- one shot should do it by then. */
	LI	a2,PHYS_TO_COMPATK1(HEART_COUNT)
	ld	a3,0(a2)
	li	a2,HEART_COUNT_RATE*10	# 10 seconds
	daddu	a3,a2			# base + 10 seconds
1:	LI	a2,PHYS_TO_COMPATK1(HEART_COUNT)
	ld	a2,0(a2)
	dsub	a2,a3,a2
	bgtz	a2,1b

	/* If after 10 seconds we didn't poweroff, we hit the wakeupat window
	 * where the power-up happened before we lost power after asking it
	 * be turned off.  Going back and re-writting RTC_X_CTRL_A will
	 * clear this condition.
	 */
	b	2b
	END(powerspin)

/*
 * Assert a WARM-LLP-RESET and then De-Assert. 
 * fct will write a value in heart mode register
 * wait at least 2 heart counter ticks and write a
 * second value to the heart mode register.
 * 
 * a0 Heart HMODE K1 ptr
 * a1 Heart HMODE assert Value
 * a2 Heart HMODE de-assert Value
 * a3 Heart Count K1 ptr
 *
 * entire function needs to fit in an icache line
 * since we are warm reseting the bridge we wont be
 * able to access the flash prom if we cache miss
 */
.align 6
LEAF(heart_warm_llp_reset)
	.set	noreorder
	sd	a1,0(a0)		# [1:0x00] assert warm reset */

	/* setup to wait 2 heart counts */
	ld	t0,0(a3)		# [2] load current heart count
	daddiu	t0,2			# [3] 2 cycles later
1:	ld	t1,0(a3)		# [4] spin for 2 heart counts */
	dsub	t1,t0,t1		# [5:0x10]
	bgez	t1,1b			# [6]
	nop				# [7]

	sd	a2,0(a0)		# [8] de-assert warm reset */

	/* spin to allow links to re-connect */
	ld	t0,0(a3)		# [9:0x20] load heart count
	daddiu	t0,100			# [10] 100 cycles * 80 (8 usecs)
1:	ld	t1,0(a3)		# [11]
	dsub	t1,t0,t1		# [12]
	bgez	t1,1b			# [13:0x30]
	nop				# [14]

	j	ra			# [15]
	nop				# [16]
	.set	reorder
	END(heart_warm_llp_reset)


/*
 * reset the IO devices by having heart send warm llp reset xtalk pkt
 * to xbow, which in turn will reset all the links on the xbow ports.
 * NOTE: we will only reset if we're running cached from prom
 *       pon_initio should be called to re-init the duarts by the caller
 *
 * void warm_reset_ip30_xio(void)
 */
LEAF(warm_reset_ip30_xio)

	/* running UN-cached from PROM check */ 

	/* below prom base ? */
	LI	v0,PHYS_TO_COMPATK1(FLASH_MEM_BASE)
	sltu	v0,ra,v0

	/* above the 2MB limit ? */
	LI	v1,PHYS_TO_COMPATK1(FLASH_MEM_ALT_BASE)
	sgtu	v1,ra,v1

	/* if either of above is true then we're OK to reset */
	or	v0,v0,v1
	bne	v0,zero,1f
	j	ra				# skip it
1:
	/* turn off Wid Err interrupts */
	LI	v0,GDA_ADDR
	ld	v0,G_MASTEROFF(v0)		# get master processor id

	dsll	v0,3				# masterid * 8
	LI	v1,PHYS_TO_COMPATK1(HEART_IMR0) # v1 = &heart->h_imr
	daddu	v1,v0,v1			# v1 += masterid
	ld	v0,0(v1)			# v0 = *v1
	and	v0,~(HEART_INT_EXC << HEART_INT_L4SHIFT)	
	sd	v0,0(v1)			# *v1 = v0

	LI	a0,PHYS_TO_COMPATK1(HEART_MODE) # a0 = &heart->h_mode
	ld	a2,0(a0)			# a2 = heart->mode
	or	a1,a2,HM_LLP_WARM_RST		# a1 = a2 | HM_LLP_WARM_RST
	LI	a3,PHYS_TO_COMPATK1(HEART_COUNT)# a3 = &heart->h_count

#if VERBOSE
	/* allow serial output to drain, spin for 10 msecs */
	ld	t0,0(a3)
	LI	t1,HEART_10MS_COUNT
	daddu	t0,t0,t1
1:	ld	t1,0(a3)
	dsub	t1,t0,t1
	bgez	t1,1b
#endif
	move	RA3,ra			# save ra, early_regdef convention
	jal	heart_warm_llp_reset

	/* spin 5 msec to give the serialio chip chance to reset */
	/* this prevents garbage char being output at pon_initio */

	LI	a3,PHYS_TO_COMPATK1(HEART_COUNT)# a3 = &heart->h_count
	ld	t0,0(a3)
	LI	t1,5*HEART_1MS_COUNT	# spin for 5 msec
	daddu	t0,t0,t1
1:	ld	t1,0(a3)
	bltu	t1,t0,1b

	j	RA3
	END(warm_reset_ip30_xio)
