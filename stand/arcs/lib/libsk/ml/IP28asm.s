#ident	"lib/libsk/ml/IP28asm.s:  $Revision: 1.8 $"

/*
 * IP28asm.s - IP28 specific assembly language functions
 */

#include "ml.h"
#include <asm.h>
#include <regdef.h>
#include <sys/sbd.h>
#include <sys/cpu.h>

/*
 * wbflush() -- spin until write buffer empty
 */
LEAF(wbflush)
XLEAF(flushbus)
	lw      zero,PHYS_TO_COMPATK1(CPUCTRL0)
	/* mfc0 serializes execution and forces wait on lw */
	sync
	.set	noreorder
	mfc0	v0,C0_SR
	mfc0	v0,C0_SR
	.set	reorder
	j	ra
	END(wbflush)

/*
 * old_sr = spl(new_sr) -- set the interrupt level (really the sr)
 *	returns previous sr
 */
LEAF(spl)
	.set	noreorder
	mfc0	v0,C0_SR
	li	t0,~SR_IMASK
	and	t0,v0,t0
	or	t0,t0,a0
	mtc0	t0,C0_SR
	.set	reorder
	j	ra
	END(spl)


/*
 *
 * writemcreg (reg, val)
 *
 * Basically this does *(volatile uint *)(PHYS_TO_K1(reg)) = val;
 *      a0 - physical register address
 *      a1 - value to write
 *
 *  This was a workaround for a bug in the first rev MC chip, which is not
 * in fullhouse, but we make still need a routine by this name.
 */

LEAF(writemcreg)
        or      a0,K1BASE       # a0 = PHYS_TO_K1(a0)
        sw      a1,(a0)         # write val in a1 to MC register *a0
        j       ra
END(writemcreg)

#define MEMACC_XOR		(CPU_MEMACC_SLOW&0x3fff)
#define CPU_MEMACC_OFFSET	CPU_MEMACC-CPUCTRL0
#define MEMCFG1_OFFSET		MEMCFG1-CPUCTRL0

/* Enable uncachedable writes via slow memory, returning the old state.
 *
 * Critical section on one cache line to prevent writebacks during
 * the mode switch.
 *
 * The Read from K1BASE below is to force a read from main memory before
 * entering slow mode.  This will check for any bad (uncached) writes
 * done previously, which have gone undetected until here.  (The bus error
 * can't be raised for an uncached write until the next read cycle.)
 *
 */
LEAF(ip26_enable_ucmem)
XLEAF(ip28_enable_ucmem)
	LI	a0,K1BASE			# K1
	lw	zero,0(a0)			# Force mem read to flush errs.

	li	a2,CPU_MEMACC_SLOW		# slow memory setting
	lw	t0,ip28_memacc_flag
	beqz	t0,19f
	li	a2,CPU_MEMACC_SLOW_IP26		# slow IP26 memory setting
19:
	or	a4,a0,ECC_CTRL_REG		# ECC PAL ctrl reg.
	or	a0,a0,CPUCTRL0			# PHYS_TO_K1(CPUCTRL0)

	lw	t2,MEMCFG1_OFFSET(a0)		# set-up memory config
	and	t3,t2,0xffff0000		# save good side of register
	or	t3,ECC_MEMCFG			# add ECC register

	.set	noreorder
	AUTO_CACHE_BARRIERS_DISABLE		# all addrs constructed
	mfc0	t0,C0_SR			# disable interrupts
	ori	t1,t0,SR_IE
	xori	t1,SR_IE

	.align 7
	mtc0	t1,C0_SR			# critical begin
	mfc0	zero,C0_SR			# barrier
	sw	t3,MEMCFG1_OFFSET(a0)		# map ECC part
	lw	t1,CPU_MEMACC_OFFSET(a0)	# get MC memory config (flush)
	andi	v0,t1,0x3fff			# important bits
	xori	v0,MEMACC_XOR			# 0=slow, !0=normal
	sw	a2,CPU_MEMACC_OFFSET(a0)	# go to slow mode on MC
	lw	zero,0(a0)			# flushbus
	sync
	lw	zero,0(a0)			# flushbus continued
	li	a2,ECC_CTRL_DISABLE		# disable ECC chk (uc writes ok)
	sd	a2,0(a4)			# Enter slow mode.
	lw	zero,0(a0)			# flushbus
	sync
	lw	zero,0(a0)			# flushbus
						# end of critical
	sw	t2,MEMCFG1_OFFSET(a0)		# restore mapping
	lw	zero,0(a0)			# flushbus
	sync
	mtc0	t0,C0_SR			# restore C0_SR
	.set	reorder
	AUTO_CACHE_BARRIERS_ENABLE

	j	ra
	END(ip26_enable_ucmem)

/*  Return to previous memory system state.  1 == fast, 0 == slow.
 *	- a0 -- previous mode
 *
 *  Assumes standalone runs in slow mode, so doesn't actually switch
 * if comming from slow mode.
 *
 * Critical section on one cache line to prevent writebacks during
 * the mode switch.
 *
 * NOTE: interface is different from the kernel as it knows what the
 *	 fast mode setting is.  Also symmon must react to what the
 *	 kernel thinks it is, which is a bit different, so we need
 *	 to explicitly restore it.
 */
LEAF(ip28_return_ucmem)
	beqz	a0,2f				# return to slow -- no change

	move	a3,a1				# fast TCC_GCACHE value
	LI	a0,K1BASE			# K1
	li	a2,CPU_MEMACC_NORMAL		# normal memory setting
	lw	t0,ip28_memacc_flag
	beqz	t0,1f
	li	a2,CPU_MEMACC_NORMAL_IP26	# normal IP26 memory setting
1:
	or	a4,a0,ECC_CTRL_REG		# ECC PAL ctrl reg.
	or	a0,a0,CPUCTRL0			# PHYS_TO_K1(CPUCTRL0)

	lw	t2,MEMCFG1_OFFSET(a0)		# set-up memory config
	and	t3,t2,0xffff0000		# save good side of register
	or	t3,ECC_MEMCFG			# add ECC register

	AUTO_CACHE_BARRIERS_DISABLE
	.set	noreorder
	mfc0	t0,C0_SR			# disable interrupts
	ori	t1,t0,SR_IE
	xori	t1,SR_IE

	.align 7
	mtc0	t1,C0_SR			# critical begin
	mfc0	zero,C0_SR			# barrier
	sw	t3,MEMCFG1_OFFSET(a0)		# map ECC part
	lw	t1,CPU_MEMACC_OFFSET(a0)	# get MC memory config (flush)
	andi	v0,t1,0x3fff			# important bits
	xori	v0,MEMACC_XOR			# 0=slow, !0=normal
	sd	zero,0(a4)			# ECC_CTRL_ENABLE==0 (Fast)
	lw	zero,0(a0)			# flushbus
	sync
	lw	zero,0(a0)			# flushbus continued
	sw	a2,CPU_MEMACC_OFFSET(a0)	# go to normal mode on MC
	lw	zero,0(a0)			# flushbus
	sync
	lw	zero,0(a0)			# flushbus continued
	sw	t2,MEMCFG1_OFFSET(a0)		# unmap ECC part
	lw	zero,0(a0)			# flushbus
	sync
	mtc0	t0,C0_SR			# restore C0_SR
	.set	reorder
	AUTO_CACHE_BARRIERS_ENABLE

2:	j	ra
	END(ip28_return_ucmem)

/* used to restore previous K0 cacheability attribute */
LEAF(set_config)
	.set	noreorder
	mtc0	a0,C0_CONFIG
	j	ra
	nop					# BDSLOT
	.set	reorder
	END(set_config)

/* Routine to map PAL, do write, then unmap PAL */
LEAF(ip28_write_pal)
	AUTO_CACHE_BARRIERS_DISABLE		# all addrs constructed
	CLI	t0,PHYS_TO_COMPATK1(MEMCFG1)
	LI	t1,PHYS_TO_K1(ECC_CTRL_REG)

	.set	noreorder
	mfc0	v1,C0_SR			# disable interrupts
	or	t2,v1,SR_IE
	xori	t2,SR_IE
	mtc0	t2,C0_SR

	/* Enable ECC bank */
	lw	v0,0(t0)			# MEMCFG1
	and	t3,v0,0xffff0000		# keep upper word
	or	t3,ECC_MEMCFG			# or in the ECC mapping
	sw	t3,0(t0)			# write new value back
	lw	zero,0(t0)			# flushbus
	sync
	lw	zero,0(t0)
	mfc0	zero,C0_SR			# barrier

	/* Do write and then flush */
	sd	a0,0(t1)			# write ECC PAL
	lw	zero,0(t0)			# flush
	sync
	mfc0	zero,C0_SR			# barrier

	/* Disable Bank and re-enable interrupts */
	sw	v0,0(t0)			# write MC
	lw	zero,0(t0)			# flush
	sync
	mfc0	zero,C0_SR			# barrier

	mtc0	v1,C0_SR			# restore C0_SR
	.set	reorder
	AUTO_CACHE_BARRIERS_ENABLE

	j	ra
	END(ip28_write_pal);

/* Do not correct ECC errors.  Used for diags with no NMI reporting
 */
LEAF(ip28_ecc_flowthru)
	dli	a0,ECC_FLOWTHRU
	b	ip28_write_ecc
	END(ip28_ecc_flowthru)

/* Do correct ECC errors in line if possible (normal mode)
 */
LEAF(ip28_ecc_correct)
	dli	a0,ECC_DEFAULT
	b	ip28_write_ecc
	END(ip28_ecc_correct)

/* Do cache writeback to hit both ECC parts at the same time.
 */
LEAF(ip28_write_ecc)
	/* Enable ECC bank */
	CLI	t0,PHYS_TO_COMPATK1(MEMCFG1)
	lw	v0,0(t0)			# MEMCFG1
	and	t3,v0,0xffff0000		# keep upper word
	or	t3,ECC_MEMCFG			# or in the ECC mapping
	sw	t3,0(t0)			# write new value back
	.set	noreorder
	lw	zero,0(t0)			# flushbus
	sync
	lw	zero,0(t0)
	mfc0	zero,C0_SR			# barrier
	.set	reorder

	/* Make sure ecc write address does not conflict with instructions */
	LI	a1,PHYS_TO_K0(ECC_CTRL_BASE)
	LA	a2,1f				# addr of instructions
	and	a2,0x3ff			# 8 line mask
	add	a2,4*CACHE_SLINE_SIZE		# split the difference
	PTR_ADDU a1,a2
	b	1f

	/* Cached write_chip0/1 and ctrl */
	.align	7
	.set	noreorder
1:	sd	a0,0(a1)			# Write 49C466 0
	sd	a0,8(a1)			# Write 49C466 1
	sd	a0,16(a1)			#  Replicate the above
	sd	a0,24(a1)			#  writes throughout the
	sd	a0,32(a1)			#  cache line.  Each quad-
	sd	a0,40(a1)			#  word actually writes
	sd	a0,48(a1)			#  the ECC parts!
	sd	a0,56(a1)			# 
	sd	a0,64(a1)			# 
	sd	a0,72(a1)			# 
	sd	a0,80(a1)			# 
	sd	a0,88(a1)			# 
	sd	a0,96(a1)			# 
	sd	a0,104(a1)			# 
	sd	a0,112(a1)			# 
	sd	a0,120(a1)			# 

	/* Write back invalidate the line */
	cache	CACH_SD|C_HWBINV,0(a1)
	cache	CACH_BARRIER,0(a1)
	.set	reorder

	/* Disable Bank and re-enable interrupts */
	sw	v0,0(t0)			# write MC
	.set	noreorder
	lw	zero,0(t0)			# flush
	sync
	mfc0	zero,C0_SR			# barrier
	.set	reorder

	j	ra
	END(ip28_write_ecc)

/* Time dallas clock for 8 hundreths of a second, then scale to 10ms
 * must call cpu_restart_rtc before calling this.
 */
#define MAXSPIN	0x1fffff		/* semi arbitrary... */
LEAF(_ticksper80ms)
	LI	t0,RT_CLOCK_ADDR

	li	a6,MAXSPIN
1:	lw	a0,0(t0)		# wait for lower nibble of BCD == 0
	add	a6,-1
	blez	a6,9f			# loop limiter
	and	a0,0x0f
	bnez	a0,1b

	li	a1,1
	li	a6,MAXSPIN
1:	lb	a0,0(t0)		# wait for lower nibble of BCD == 1
	and	a0,0x0f			# incase already @ 0.
	add	a6,-1
	blez	a6,9f			# loop limiter
	bne	a0,a1,1b

	.set	noreorder
	mtc0	zero,C0_COUNT		# start @ 0.
	li	a1,9			# wait for 8/100ths of a second
	li	a6,MAXSPIN

1:	lw	a0,0(t0)		# get current ticker
	add	a6,-1
	blez	a6,9f			# loop limiter
	and	a0,0x0f			# BDSLOT: hundreths
	blt 	a0,a1,1b		# spin for time
	nop				# BDSLOT

	mfc0	v0,C0_COUNT		# end time
	.set	reorder

	j	ra
9:
	li	v0,7800000		# gag, like at 195Mhz

	j	ra
	END(_ticksper80ms)
