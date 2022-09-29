/*
 * IP28 specific assembly routines; cpuid always 0, also make semaphore
 * macros a no-op.
 */
#ident "$Revision: 1.58 $"

#include "ml/ml.h"
#include <sys/RACER/gda.h>
#include <sys/dump.h>

/*	dummy routines whose return value is unimportant (or no return value).
	Some return reasonable values on other machines, but should never
	be called, or the return value should never be used on other machines.
*/
LEAF(dummy_func)
XLEAF(check_delay_tlbflush)
XLEAF(check_delay_iflush)
XLEAF(da_flush_tlb)
XLEAF(dma_mapinit)
XLEAF(apsfail)
XLEAF(disallowboot)
XLEAF(rmi_fixecc)
XLEAF(vme_init)
XLEAF(vme_ivec_init)
XLEAF(debug_stop_all_cpus)
XLEAF(bump_leds)
XLEAF(reset_enet_carrier)		/* for if_ec2.c */
	j ra
END(dummy_func)

LEAF(dcache_wb)
XLEAF(dcache_wbinval)
XLEAF(dki_dcache_wb)
XLEAF(dki_dcache_wbinval)
	LI	a2,CACH_DCACHE|CACH_INVAL|CACH_WBACK|CACH_IO_COHERENCY
	j	cache_operation
	END(dcache_wb)

LEAF(dki_dcache_inval)
	LI	a2,CACH_DCACHE|CACH_INVAL|CACH_IO_COHERENCY
	j	cache_operation
	END(dki_dcache_inval)

/* dummy routines that return 0 */
LEAF(dummyret0_func)

XLEAF(vme_adapter)
XLEAF(is_vme_space)
XLEAF(getcpuid)
XLEAF(disarm_threeway_trigger)
XLEAF(threeway_trigger_armed)
#ifdef DEBUG
XLEAF(getcyclecounter)
#endif /* DEBUG */

/* Semaphore call stubs */
XLEAF(appsema)
XLEAF(apvsema)
XLEAF(apcvsema)
	move	v0,zero
	j ra
	END(dummyret0_func)

/* dummy routines that return 1 */
LEAF(dummyret1_func)
XLEAF(apcpsema)	/* can always get on non-MP machines */
XLEAF(enet_carrier_on)			/* for if_ec2.c */
	li	v0, 1
	j ra
	END(dummyret1_func)

/* unsigned int get_count(void)
 */
LEAF(get_count)
XLEAF(get_r4k_counter)			/* for compat and R4800 is a r4k */
XLEAF(_get_timestamp)			/* return timestamp on SP */
	.set	noreorder
	MFC0(v0, C0_COUNT)
	.set	reorder
	j	ra
	END(get_count)

/* clears processor clock interrupt and we continue */
LEAF(pcount_intr)
	.set	noreorder
	MFPC(t0,PRFCNT0)		# hardware performance counters
	bltz    t0,1f                   # counter 0 overflow
	nop
	MFPC(t0,PRFCNT1)
	bltz    t0,1f                   # counter 1 overflow
	nop				# not performance, fall thru & return
	j	ra
	mtc0	zero,C0_COMPARE		# BDSLOT: ack intr
1:					# performance
	j	hwperf_intr		# call hw performance interrupt
	nop				# BDSLOT
	.set	reorder
	END(pcount_intr)

/*
 *
 * writemcreg (reg, val)
 * 
 * Basically this does *(volatile uint *)(PHYS_TO_COMPATK1(reg)) = val;
 *	a0 - physical register address
 *	a1 - value to write
 *
 * This was a workaround for a bug in the first rev MC chip, but IP28
 * has only rev D (or greater) MCs, so just do the actual operation.
 */

LEAF(writemcreg)
	or	a0,COMPAT_K1BASE	# a0 = PHYS_TO_COMPATK1(a0)
	sw	a1,0(a0)
	j	ra
END(writemcreg)


/*
 * Write the VDMA MEMADR, MODE, SIZE, STRIDE registers
 *
 * write4vdma (buf, mode, size, stride);
 */

LEAF(write4vdma)
#if DMA_MEMADR & 0x8000
#error	DMA_MEMADDR broken for IP28!
#endif
	LI	v0, (COMPAT_K1BASE | DMA_MEMADR)& (~0xffff)

	sw	a0,DMA_MEMADR & 0xffff(v0)
	sw	a1,DMA_MODE   & 0xffff(v0)
	sw	a2,DMA_SIZE   & 0xffff(v0)
	sw	a3,DMA_STRIDE & 0xffff(v0)

	j	ra
END(write4vdma)

#define MEMACC_XOR		(CPU_MEMACC_SLOW&0x3fff)
#define CPU_MEMACC_OFFSET	CPU_MEMACC-CPUCTRL0
#define MEMCFG1_OFFSET		MEMCFG1-CPUCTRL0
#define LINESIZE		CACHE_SLINE_SIZE

/* Enable uncachedable writes via slow memory, returning the old state.
 *
 * Critical section on one cache line to prevent writebacks during
 * the mode switch.
 *
 * May be called from ECC handler w/o an SP, so do not allow cache ops
 * here as all stores are to constructed addresses.
 */
LEAF(ip28_enable_ucmem)
XLEAF(ip26_enable_ucmem)
	AUTO_CACHE_BARRIERS_DISABLE		# all stores have dependancies
	lw	a2,ip28_memacc_slow		# slow mode bits from memory
	LI	a0,K1BASE			# K1
	or	a4,a0,ECC_CTRL_REG		# ECC PAL ctrl reg.
	or	a0,a0,CPUCTRL0			# PHYS_TO_K1(CPUCTRL0)

	lw	t2,MEMCFG1_OFFSET(a0)		# set-up memory config
	and	t3,t2,0xffff0000		# save good side of register
	or	t3,ECC_MEMCFG			# add ECC register

	.set	noreorder
	mfc0	t0,C0_SR			# disable interrupts
	ori	t1,t0,SR_IE
	xori	t1,SR_IE

	.align	7
	mtc0	t1,C0_SR			# critical begin
	mfc0	zero,C0_SR			# barrier
	sw	t3,MEMCFG1_OFFSET(a0)		# map ecc part
	lw	t1,CPU_MEMACC_OFFSET(a0)	# get MC memory config
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
	lw	zero,0(a0)			# flushbus continued

	sw	t2,MEMCFG1_OFFSET(a0)		# restore mapping
	lw	zero,0(a0)			# flushbus
	sync

	mtc0	t0,C0_SR			# restore C0_SR
	.set	reorder

	j	ra
	AUTO_CACHE_BARRIERS_ENABLE
	END(ip28_enable_ucmem)

/* Disable uncacheable writes via faster memory, returning the old state.
 *
 * Critical section on one cache line to prevent writebacks during
 * the mode switch.
 *
 * May be called from ECC handler w/o an SP, so do not allow cache ops
 * here as all stores are to constructed addresses.
 */
LEAF(ip28_disable_ucmem)
XLEAF(ip26_disable_ucmem)
	AUTO_CACHE_BARRIERS_DISABLE		# all stores have dependancies
	lw	a2,ip28_memacc_norm		# norm mode bits from memory
	LI	a0,K1BASE			# K1
	or	a4,a0,ECC_CTRL_REG		# ECC PAL ctrl reg.
	or	a0,a0,CPUCTRL0			# PHYS_TO_K1(CPUCTRL0)

	lw	t2,MEMCFG1_OFFSET(a0)		# set-up memory config
	and	t3,t2,0xffff0000		# save good side of register
	or	t3,ECC_MEMCFG			# add ECC register

	.set	noreorder
	mfc0	t0,C0_SR			# disable interrupts
	ori	t1,t0,SR_IE
	xori	t1,SR_IE

	.align	7
	mtc0	t1,C0_SR			# critical begin
	mfc0	zero,C0_SR			# barrier
	sw	t3,MEMCFG1_OFFSET(a0)		# map ecc part
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

	sw	t2,MEMCFG1_OFFSET(a0)		# restore mapping
	lw	zero,0(a0)			# flushbus
	sync

	mtc0	t0,C0_SR			# restore C0_SR
	.set	reorder

	j	ra
	AUTO_CACHE_BARRIERS_ENABLE
	END(ip28_disable_ucmem)

LEAF(unmap_ecc)
	AUTO_CACHE_BARRIERS_DISABLE		# address dependancy on k0
	CLI	t0,PHYS_TO_COMPATK1(MEMCFG1)
	lw	v0,0(t0)			# get current
	and	t3,v0,0xffff0000		# keep upper word (drop ECC)
	sw	t3,0(t0)			# write back
	lw	zero,0(t0)
	sync
	j	ra
	AUTO_CACHE_BARRIERS_ENABLE
	END(unmap_ecc)

/* Routine to map PAL, do write, then unmap PAL */
LEAF(ip28_write_pal)
	CLI	t0,PHYS_TO_COMPATK1(MEMCFG1)
	LI	t1,PHYS_TO_K1(ECC_CTRL_REG)

	.set	noreorder
	AUTO_CACHE_BARRIERS_DISABLE		# all stores are uncached
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
	AUTO_CACHE_BARRIERS_ENABLE
	.set	reorder

	j	ra
	END(ip28_write_pal)

/* Do correct ECC errors in line if possible (normal mode)
 */
LEAF(ip28_ecc_correct)
	dli	a0,ECC_DEFAULT

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
	LI	a1,PHYS_TO_K0(ECC_CTRL_BASE)
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
	cache	CACH_SD|C_HWBINV,0(t0)
	cache	CACH_BARRIER,0(t0)
	.set	reorder

	/* Disable Bank and re-enable interrupts */
	sw	v0,0(t0)			# write MC
	.set	noreorder
	lw	zero,0(t0)			# flush
	sync
	mfc0	zero,C0_SR			# barrier
	.set	reorder

	j	ra
	END(ip28_ecc_correct)

/* Return to previous memory system state.  1 == fast, 0 == slow.
 */
LEAF(ip28_return_ucmem)
XLEAF(ip26_return_ucmem)
	AUTO_CACHE_BARRIERS_DISABLE		# may be called from ECC hndlr
	beqz	a0,1f
	b	ip28_disable_ucmem		# going to normal mode
1:	b	ip28_enable_ucmem		# going to slow mode
	AUTO_CACHE_BARRIERS_ENABLE
	END(ip28_return_ucmem)

/* return the content of the R10000 C0 config register */
LEAF(get_r10k_config)
	.set	noreorder
	mfc0	v0,C0_CONFIG
	.set	reorder
	j	ra
	END(get_r10k_config)

/* Return size of secondary cache (really max cache size for start-up) */
LEAF(getcachesz)
	.set	noreorder
	mfc0	v1,C0_CONFIG
	and	v1,CONFIG_SS
	dsrl	v1,CONFIG_SS_SHFT
	dadd	v1,CONFIG_SCACHE_POW2_BASE
	li	v0,1
	j	ra
	dsll	v0,v1			# cache size in byte
	.set	reorder
	END(getcachesz)

/* Write back/invalidate one line from the cache.  This can be used by drivers
 * (enet uses it now) to have a lower overhead cacheflush when getting around
 * problems with IP28 ECC baseboard.
 *
 * Accepts a full 64-bit bit phys, K0, or K1 address (enet tends to pass K1).
 */
LEAF(__dcache_line_wb_inval)
	.set	noreorder
	and	a0,TO_PHYS_MASK			# KDM_TO_K0(a)
	or	a0,K0BASE
	cache	CACH_SD|C_HWBINV,0(a0)		# write back line in a0
	cache	CACH_BARRIER,0(a0)		# make sure line is out
	.set	reorder
	j	ra
	END(__dcache_line_wb_inval)

#define NMI_ERREPC	0
#define NMI_EPC		8
#define NMI_SP		16
#define NMI_RA		24
#define NMI_SAVE_REGS	4

LEAF(nmi_dump)
	.set noreorder
	li	k0,SR_KADDR|SR_DEFAULT	# make sure C0_SR is sane
	MTC0	(k0,C0_SR)

	# Save some registers in nmi_saveregs.
	LA	k0,nmi_saveregs
	DMFC0 	(k1,C0_ERROR_EPC)
	CACHE_BARRIER_AT(NMI_ERREPC,k0)	# probably not needed
	sd	k1,NMI_ERREPC(k0)
	DMFC0	(k1,C0_EPC)
	sd	k1,NMI_EPC(k0)
	sd	sp,NMI_SP(k0)
	sd	ra,NMI_RA(k0)

	LA	sp,dumpstack		# move to dump stack
	LI	gp,DUMP_STACK_SIZE-16
	PTR_ADD sp,gp			# Set our stack pointer.
	LA	gp,_gp			# reload gp
	jal	ip28_ecc_error		# error handler for NMI/IP28 ECC
	nop				# BDSLOT

	.set	reorder

	END(nmi_dump)

	.data

EXPORT(nmi_saveregs)
	.dword	0: NMI_SAVE_REGS

	.text

/* void delayloop(int count, int decr)
 *	- delay loop with a loop factoring, and also helps sometimes with
 *	  messy compilers.
 *	- more scale is needed as there are no uncached loads/cacheops around
 *	  this code like the _ticksper1024instr code.
 */
LEAF(delayloop)
	.set noreorder
	sync					# force loads out of T5
#ifdef US_DELAY_DEBUG
	mfc0	t0,C0_COUNT
#endif
	sll	a0,1				# scale a bit more
1:
	nop;nop;nop;nop;nop;nop;nop
	bgt	a0,zero,1b
	subu	a0,a1				# BDSLOT
#ifdef US_DELAY_DEBUG
	mfc0	t1,C0_COUNT
	CACHE_BARRIER
	sw	t0,us_before
	sw	t1,us_after
#endif
	.set reorder
	j	ra
	END(delayloop)

/* time dallas clock for 8 hundreths of a second, then scale to 10ms */
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

/*  Code to allow crippled support of IP26 baseboard to handle ECC exceptions.
 * It requires the cache to work at CACHE_ERR_FRAME for a few lines.  This
 * will not be the case for IP28, which can do double word stores in fast
 * mode.
 *
 *  Also must jump to SEG0_BASE version of ecc_handler as cannot jump from
 * alias to high memory w/o a jump register.
 */
LEAF(ecc_springboard)
	.set noreorder
	.set noat				# AT is not yet saved
	AUTO_CACHE_BARRIERS_DISABLE		# code runs uncached...
	/* Save 1st register in C0 by jumping through 3 hoops */
	MTC0(k1,C0_LLADDR)			# save the 32 LSBs of k1
	dsrl32	k1,0
	dsll	k1,3				# make sure the 3 LSBs are 0's
	MTC0(k1,C0_WATCHLO)			# save the middle 29 bits of k1
	dsrl32	k1,0
	MTC0(k1,C0_WATCHHI)			# save the 3 MSBs of k1

	/* Check board revision, IP26 and IP26+ to find how to do the eframe */
	CLI	k1,PHYS_TO_COMPATK1(HPC3_SYS_ID)# board revision info
	lw	k1,0(k1)
	andi	k1,BOARD_REV_MASK		# isolate board rev
	sub	k1,IP26_ECCSYSID		# IP26, IP26+
	bgtz	k1,1f				# skip if on IP28 bd
	nop					# BDSLOT
	/*  Need to cache the in a safe buffer range.  If we have more than
	 * one error, we won't know, but that's a panic anyway.
	 */
	mfc0	k1,C0_CACHE_ERR			# get cache err.
	and	k1,CACHE_TMP_EMASK		# mask our index
	sub	k1,CACHE_TMP_EFRAME1
	bnez	k1,77f				# if no match use frame1
	nop					# BDSLOT
	LI	k1,K0BASE|CACHE_TMP_EFRAME1
	b	2f				# skip IP28 case
	nop					# BDSLOT
77:						# else use frame2
	LI	k1,K0BASE|CACHE_TMP_EFRAME2
	b	2f				# skip IP28 case
	nop					# BDLOST
1:	LA	k1,cacheErr_frames		# ptr to ECC frame
	dsll	k1,8				# convert to physical
	dsrl	k1,8				# so we avoid the cache
	PTR_L	k1,0(k1)			# get ECCF ptr
2:

	sreg	k0,EF_K0(k1)			# save K0/AT so we have
	sreg	AT,EF_AT(k1)			# some breathing room.

	/* reconstruct and save k1 in frame */
	MFC0(k0,C0_WATCHHI)
	dsll32	k0,29
	MFC0(AT,C0_WATCHLO)
	dsll32	AT,0				# remove sign extensions
	dsrl	AT,3				
	or	k0,AT
	MFC0(AT,C0_LLADDR)
	dsll32	AT,0				# remove sign extensions
	dsrl32	AT,0
	or	k0,AT
	sreg	k0,EF_K1(k1)			# ah, we can save K1 value
	.set	at

	/* jump to direct mapped, high memory version of ecc_exception */
	LA	k0,ecc_exception		# base address
	and	k0,0x7fffffff			# KDM_TO_PHYS (enough of it)
	jr	k0				# call ecc_exception
	nop					# BDSLOT
	.set	reorder
	AUTO_CACHE_BARRIERS_ENABLE
	END(ecc_springboard)

LEAF(get_scache_tag)
	.set	noreorder
	cache	CACH_S|C_ILT,0(a0)
	mfc0	v1,C0_TAGHI
	mfc0	v0,C0_TAGLO
	.set	reorder
	dsll	v1,v1,32
	or	v0,v1,v0
	j	ra
	END(get_scache_tag)

LEAF(get_dcache_tag)
	.set	noreorder
	cache	CACH_PD|C_ILT,0(a0)
	mfc0	v1,C0_TAGHI
	mfc0	v0,C0_TAGLO
	.set	reorder
	dsll	v1,v1,32
	or	v0,v1,v0
	j	ra
	END(get_dcache_tag)

/*  Specialized pacecar pagecopy routine that is unrolled a bit to avoid
 * d$ speculation workaround overhead execept on the last bit.
 * 
 *  _pagecopy(void *src, void *dst, int len)
 *
 *  Assumes src and dst are both cache line aligned and len is a multiple
 * of (n*2*CACHE_SLINE_SIZE)+2*CACHE_SLINE_SIZE, ie an even number of cache
 * lines greater than or equal to 4.
 *
 *  The code does not copy the last two lines to avoid the d$ speculation
 * on stores problem.  The T5 has a 16 deep Address queue which has to fill
 * 4 times for us to do a speculative store past the end of our buffer so
 * we are safe.  The trailer loop has an explicit cache barrier.
 *
 *  This is derived from the teton function of the same name.  We do not
 * use prefetch as I think we bog down the address queue enough so it
 * doesn't really become effective.  On some strides it helps.
 *
 *  Copy registers: a4, a5, a6, a7
 */
LEAF(_pagecopy)
	AUTO_CACHE_BARRIERS_DISABLE	# ends 2 cache lines (branches early)
	.set	noreorder

	CACHE_BARRIER			# ensure operands are known

	beqz	a2,2f			# skip zero length copies
	sltu	t1,a0,a1		# BDSLOT: if src < dst
	bnez	t1,_pagecopy_backwards	# then do backwards copy
	li	t0,2*CACHE_SLINE_SIZE	# BDSLOT: size of trailer
	addi	a2,-(2*CACHE_SLINE_SIZE)# do last 2 lines seperately

1:	ld	a4,  0(a0)	;	ld	a5, 32(a0)	# line 1 + 2
	addi    a2,-CACHE_SLINE_SIZE				# add
	ld	a6, 80(a0)	;	ld	a7,112(a0)	# line 3 + 4
	sd	a4,  0(a1)	;	sd	a5, 32(a1)	# bank 1
	sd	a6, 80(a1)	;	sd	a7,112(a1)	# bank 2
	ld	a4,  8(a0)	;	ld	a5, 40(a0)	# bank 1
	ld	a6, 16(a0)	;	ld	a7, 24(a0)	# bank 2
	sd	a4,  8(a1)	;	sd	a5, 40(a1)
	sd	a6, 16(a1)	;	sd	a7, 24(a1)
	ld	a4, 64(a0)	;	ld	a5, 72(a0)	# bank 1
	ld	a6, 88(a0)	;	ld	a7,120(a0)	# bank 2
	sd	a4, 64(a1)	;	sd	a5, 72(a1)
	sd	a6, 88(a1)	;	sd	a7,120(a1)
	ld	a4, 96(a0)	;	ld	a5,104(a0)	# bank 1
	ld	a6, 48(a0)	;	ld	a7, 56(a0)	# bank 2
	sd	a4, 96(a1)	;	sd	a5,104(a1)
	sd	a6, 48(a1)	;	sd	a7, 56(a1)

	daddiu	a0,CACHE_SLINE_SIZE

	bgtz	a2,1b			# keep going?
	daddiu	a1,CACHE_SLINE_SIZE	# BDSLOT: next dst cache line

1:	addi	t0,-32			# done with one chunk
	ld	a4, 0(a0)	;	ld	a5, 8(a0)
	ld	a6,16(a0)	;	ld	a7,24(a0)
	sd	a4, 0(a1)	;	sd	a5, 8(a1)
	sd	a6,16(a1)	;	sd	a7,24(a1)

	daddiu	a0,32			# next src chunk
	daddiu	a1,32			# next dst chunk
	CACHE_BARRIER_AT(-32,a0)	# quench store speculation
	bgtz	t0,1b			# keep going?
	nop				# BDSLOT

2:	j	ra
	nop				# BDSLOT

_pagecopy_backwards:
	daddu	a0,a2			# start with ending addresses
	daddu	a1,a2
	li	t0,2*CACHE_SLINE_SIZE	# size of trailer
	addi	a2,-(2*CACHE_SLINE_SIZE)# do last 2 lines seperately

1:	ld	a4,  -8(a0)	;	ld	a5, -40(a0)	# line 1 + 2
	addi    a2,-CACHE_SLINE_SIZE;
	ld	a6, -88(a0)	;	ld	a7,-120(a0)	# line 3 + 4
	sd	a4,  -8(a1)	;	sd	a5, -40(a1)	# bank 2
	sd	a6, -88(a1)	;	sd	a7,-120(a1)	# bank 1
	ld	a4, -16(a0)	;	ld	a5, -48(a0)
	ld	a6, -24(a0)	;	ld	a7, -32(a0)
	sd	a4, -16(a1)	;	sd	a5, -48(a1)	# bank 2
	sd	a6, -24(a1)	;	sd	a7, -32(a1)	# bank 1
	ld	a4, -72(a0)	;	ld	a5, -80(a0)
	ld	a6, -96(a0)	;	ld	a7,-128(a0)
	sd	a4, -72(a1)	;	sd	a5, -80(a1)	# bank 2
	sd	a6, -96(a1)	;	sd	a7,-128(a1)	# bank 1
	ld	a4,-104(a0)	;	ld	a5,-112(a0)
	ld	a6, -56(a0)	;	ld	a7, -64(a0)
	sd	a4,-104(a1)	;	sd	a5,-112(a1)	# bank 2
	sd	a6, -56(a1)	;	sd	a7, -64(a1)	# bank 1

	daddiu	a0,-CACHE_SLINE_SIZE

	bgtz	a2,1b			# keep going?
	daddiu	a1,-CACHE_SLINE_SIZE	# BDSLOT: next dst cache line

1:	ld	a4, -8(a0)	;	ld	a5,-16(a0)
	ld	a6,-24(a0)	;	ld	a7,-32(a0)
	addi	t0,-32			# done with one chunk
	sd	a4, -8(a1)	;	sd	a5,-16(a1)
	sd	a6,-24(a1)	;	sd	a7,-32(a1)

	daddiu	a0,-32			# next src chunk
	daddiu	a1,-32			# next dst chunk
	CACHE_BARRIER_AT(32-8,a0)	# quench store speculation
	bgtz	t0,1b			# keep going?
	nop				# BDSLOT

	AUTO_CACHE_BARRIERS_ENABLE

	j	ra
	nop				# BDSLOT
	.set	reorder
	END(_pagecopy)

/*  Specialized pacecar pagezero routine that is unrolled a bit to help
 * with the d$ speculation problems.  It stops 3 lines early which allows
 * us to zero at full speed, and only slow down at the end, and still
 * avoid bogus cache dirtying past the buffer.
 *
 *	_pagezero(void *dst, int len)
 *
 *  The dst address must be page cache aligned, and the length must be
 * of more than 4 cache lines long (1 then 3 trailers).
 *
 *  This is derived from the teton function of the same name, but does
 * not use prefetch as with non blocking caches, we don't have enough
 * time to really hide the latency.
 */
LEAF(_pagezero)
	.set	noreorder
	AUTO_CACHE_BARRIERS_DISABLE	# loop ends 4 lines early.

	CACHE_BARRIER			# ensure operands are known

	beqz	a1,2f			# make sure length is non-zero
	li	t0,3*CACHE_SLINE_SIZE	# BDSLOT: size of secondary copy
	addi	a1,-(3*CACHE_SLINE_SIZE)# do last 4 lines seperately

1:	sd	zero,  0(a0)	;	sd	zero, 32(a0)	# line 1 + 2
	addi	a1,-CACHE_SLINE_SIZE
	sd	zero, 80(a0)	;	sd	zero,112(a0)	# line 3 + 4
	sd	zero,  8(a0)	;	sd	zero, 40(a0)	# bank 1
	sd	zero, 16(a0)	;	sd	zero, 24(a0)	# bank 2
	sd	zero, 64(a0)	;	sd	zero, 72(a0)	# bank 1
	sd	zero, 88(a0)	;	sd	zero,120(a0)	# bank 2
	sd	zero, 96(a0)	;	sd	zero,104(a0)	# bank 1
	sd	zero, 48(a0)	;	sd	zero, 56(a0)	# bank 2
	bgtz	a1,1b			# loop more?
	daddiu	a0,CACHE_SLINE_SIZE	# BDSLOT: bump address

1:	sd	zero,  0(a0)	;	sd	zero, 32(a0)	# line 1 + 2
	addi	t0,-CACHE_SLINE_SIZE
	sd	zero, 80(a0)	;	sd	zero,112(a0)	# line 3 + 4
	sd	zero,  8(a0)	;	sd	zero, 40(a0)	# bank 1
	sd	zero, 16(a0)	;	sd	zero, 24(a0)	# bank 2
	sd	zero, 64(a0)	;	sd	zero, 72(a0)	# bank 1
	sd	zero, 88(a0)	;	sd	zero,120(a0)	# bank 2
	sd	zero, 96(a0)	;	sd	zero,104(a0)	# bank 1
	sd	zero, 48(a0)	;	sd	zero, 56(a0)	# bank 2
	CACHE_BARRIER_AT(0,a0)		# prevent speculation
	bgtz	t0,1b			# loop more?
	daddiu	a0,CACHE_SLINE_SIZE	# BDSLOT: bump address

	AUTO_CACHE_BARRIERS_ENABLE

2:	j	ra
	nop
	.set	reorder
	END(_pagezero)

/* void nowar_bcopy(from, to, count);
 *	unsigned char *from, *to;
 *	unsigned long count;
 *
 * Copied from usercopy.s, removed the #ifdefs, and turn off cache barriers.
 */
#define	MINCOPY	12
#define	from	a0		/* registers used */
#define	to	a1
#define	count	a2

#define LWS	lwl
#define LWB	lwr
#define LDS	ldl
#define LDB	ldr
#define SWS	swl
#define SWB	swr
#define SDS	sdl
#define SDB	sdr

/* Use backwards copying code if the from and to regions overlap.
 *   Do not worry about zero-length or other silly copies.  They are not
 *   worth the time to optimize.
 */
LEAF(nowar_bcopy)
	AUTO_CACHE_BARRIERS_DISABLE
	LI	t0,1<<63		# bit 63 means a kernel addr
	and	t0,a1,t0		# kernel dst needs WAR
	beqz	t0,1f
	j	bcopy
1:
	ORD_CACHE_BARRIER_AT(0,sp)	# ensure above check is ok

	PTR_ADDU v0,from,count		# v0 := from + count
	ble	to,from,goforwards	# If to <= from then copy forwards
	blt	to,v0,gobackwards	# backwards if from<to<from+count

/* Forward copy code.  Check for pointer alignment and try to get both
 * pointers aligned on a long boundary.
 */
goforwards:
	blt	count,MINCOPY,fbcopy
/* If possible, align source & destination on 64-bit boundary. 
 */
	and	v0,from,7
	and	v1,to,7
	li	a3,8
	bne	v0,v1,align32		# low bits are different

/* Pointers 64-bit alignable (may be aligned).  Since v0 == v1, we need only
 * check what value v0 has to see how to get aligned.  Also, since we have
 * eliminated tiny copies, we know that the count is large enough to
 * encompass the alignment copies.
 */
	beq	v0,zero,1f		# If v0==0 then aligned
	subu	a3,a3,v1		# a3 = # bytes to get aligned
	LDS	v0,0(from)
	SDS	v0,0(to)		# copy partial word
	PTR_ADDU from,a3
	PTR_ADDU to,a3
	subu	count,a3
1:
/* When we get here, source and destination are 64-bit aligned.  Check if
 * we have at least 64 bytes to move.
 */
	and	a3,count,~(64-1)
	beq	a3,zero,forwards	# go do 32-bit copy
	PTR_ADDU a3,a3,to
64:
	ld t0,0(from);   ld t1,8(from)
	ld t2,16(from);  ld t3,24(from)
	ld ta0,32(from); ld ta1,40(from);  ld ta2,48(from);  ld ta3,56(from)
	sd t0,0(to);     sd t1,8(to);     sd t2,16(to);    sd t3,24(to)
	sd ta0,32(to);    sd ta1,40(to);    sd ta2,48(to);    sd ta3,56(to)
	PTR_ADDU from,64
	PTR_ADDU to,64

	bne	a3,to,64b

	and	count,64-1	# still have to copy non-64 multiple bytes
	b	forwards		# complete with 32-bit copy

align32:
	and	v0,from,3
	and	v1,to,3
	li	a3,4
	bne	v0,v1,fmcopy		# low bits are different

/* Pointers are alignable and may be aligned.  Since v0 == v1, we need only
 * check what value v0 has to see how to get aligned.  Also, since we have
 * eliminated tiny copies, we know that the count is large enough to
 * encompass the alignment copies.
 */
	beq	v0,zero,forwards	# If v0==0 then aligned
	subu	a3,a3,v1		# a3 = # bytes to get aligned
	LWS	v0,0(from)
	SWS	v0,0(to)		# copy partial word
	PTR_ADDU from,a3
	PTR_ADDU to,a3
	subu	count,a3

/* Once we are here, the pointers are aligned on 32-bit boundaries
 */
forwards:
	and	a3,count,~(32-1)
	beq	a3,zero,16f
	PTR_ADDU a3,a3,to
32:
	lw t0,0(from);   lw t1,4(from);   lw t2,8(from);   lw t3,12(from)
	lw ta0,16(from);  lw ta1,20(from);  lw ta2,24(from);  lw ta3,28(from)
	sw t0,0(to);     sw t1,4(to);     sw t2,8(to);     sw t3,12(to)
	sw ta0,16(to);    sw ta1,20(to);    sw ta2,24(to);    sw ta3,28(to)
	PTR_ADDU from,32
	PTR_ADDU to,32
	bne	a3,to,32b

/* We know we have fewer than 32 bytes remaining, so we do no more
 *	adjustments of the count.
 */
16:	and	v0,count,16
	beq	v0,zero,8f
	lw t0,0(from);   lw t1,4(from);   lw t2,8(from);   lw t3,12(from)
	sw t0,0(to);     sw t1,4(to);     sw t2,8(to);     sw t3,12(to)
	PTR_ADDU from,16
	PTR_ADDU to,16

8:	and	v1,count,8
	beq	v1,zero,4f
	lw	t0,0(from)
	lw	t1,4(from)
	sw	t0,0(to)
	sw	t1,4(to)
	PTR_ADDU from,8
	PTR_ADDU to,8

4:	and	v0,count,4
	beq	v0,zero,3f
	lw	t0,0(from)
	sw	t0,0(to)
	PTR_ADDU from,4
	PTR_ADDU to,4

3:	and	v1,count,3
	PTR_ADDU from,v1
	beq	v1,zero,ret
	PTR_ADDU to,v1
	LWB	t0,-1(from)
	SWB	t0,-1(to)
	j	ra

fmcopy:
/* Missaligned, non-overlap copy of many bytes. This happens too often.
 *  Align the destination for machines with write-thru caches.
 *
 *  This code is always for machines that prefer nops between stores.
 *
 * Here v1=low bits of destination, a3=4.
 */
	beq	v1,zero,fmcopy4		# If v1==0 then destination is aligned
	subu	a3,a3,v1		# a3 = # bytes to align destination
	subu	count,a3
	PTR_ADDU a3,to
1:	lb	v0,0(from)
	PTR_ADDU from,1
	sb	v0,0(to)
	PTR_ADDU to,1
	bne	to,a3,1b

fmcopy4:
	and	a3,count,~(16-1)
	beq	a3,zero,8f
	PTR_ADDU a3,a3,to
16:	LWS t0,0(from);  LWB t0,0+3(from)
	LWS t1,4(from);  LWB t1,4+3(from);  sw t0,0(to)
	LWS t2,8(from);  LWB t2,8+3(from);  sw t1,4(to)
	LWS t3,12(from); LWB t3,12+3(from); sw t2,8(to)
					    sw t3,12(to)
	PTR_ADDU from,16
	PTR_ADDU to,16
	bne	a3,to,16b

8:	and	v1,count,8
	beq	v1,zero,4f
	LWS t0,0(from);  LWB t0,0+3(from)
	LWS t1,4(from);  LWB t1,4+3(from);  sw t0,0(to)
					    sw t1,4(to)
	PTR_ADDU from,8
	PTR_ADDU to,8

4:	and	v0,count,4
	and	count,3
	beq	v0,zero,fbcopy
	LWS t0,0(from);  LWB t0,0+3(from);  sw t0,0(to)
	PTR_ADDU from,4
	PTR_ADDU to,4


/* Byte at a time copy code.  This is used when the byte count is small.
 */
fbcopy:
	PTR_ADDU a3,from,count		# a3 = end+1
	beq	count,zero,ret		# If count is zero, then we are done

1:	lb	v0,0(from)		# v0 = *from
	PTR_ADDU from,1			# advance pointer
	sb	v0,0(to)		# Store byte
	PTR_ADDU to,1			# advance pointer
	bne	from,a3,1b		# Loop until done
ret:	j	ra			# return to caller


/*****************************************************************************/

/*
 * Backward copy code.  Check for pointer alignment and try to get both
 * pointers aligned on a long boundary.
 */
gobackwards:
	PTR_ADDU from,count		# Advance to end + 1
	PTR_ADDU to,count		# Advance to end + 1

	/* small byte counts use byte at a time copy */
	blt	count,MINCOPY,backwards_bytecopy
	and	v0,from,3		# v0 := from & 3
	and	v1,to,3			# v1 := to & 3
	beq	v0,v1,backalignable	# low bits are identical
/*
 * Byte at a time copy code.  This is used when the pointers are not
 * alignable, when the byte count is small, or when cleaning up any
 * remaining bytes on a larger transfer.
 */
backwards_bytecopy:
	beq	count,zero,ret		# If count is zero quit
	PTR_SUBU from,1			# Reduce by one (point at byte)
	PTR_SUBU to,1			# Reduce by one (point at byte)
	PTR_SUBU v1,from,count		# v1 := original from - 1

99:	lb	v0,0(from)		# v0 = *from
	PTR_SUBU from,1			# backup pointer
	sb	v0,0(to)		# Store byte
	PTR_SUBU to,1			# backup pointer
	bne	from,v1,99b		# Loop until done
	j	ra			# return to caller

/*
 * Pointers are alignable, and may be aligned.  Since v0 == v1, we need only
 * check what value v0 has to see how to get aligned.  Also, since we have
 * eliminated tiny copies, we know that the count is large enough to
 * encompass the alignment copies.
 */
backalignable:
	beq	v0,zero,backwards	# If v0==v1 && v0==0 then aligned
	beq	v0,3,back_copy3		# Need to copy 3 bytes to get aligned
	beq	v0,2,back_copy2		# Need to copy 2 bytes to get aligned

/* need to copy 1 byte */
	lb	v0,-1(from)		# get one byte
	PTR_SUBU from,1			# backup pointer
	sb	v0,-1(to)		# store one byte
	PTR_SUBU to,1			# backup pointer
	subu	count,1			#  and reduce count
	b	backwards		# Now pointers are aligned

/* need to copy 2 bytes */
back_copy2:
	lh	v0,-2(from)		# get one short
	PTR_SUBU from,2			# backup pointer
	sh	v0,-2(to)		# store one short
	PTR_SUBU to,2			# backup pointer
	subu	count,2			#  and reduce count
	b	backwards

/* need to copy 3 bytes */
back_copy3:
	lb	v0,-1(from)		# get one byte
	lh	v1,-3(from)		#  and one short
	PTR_SUBU from,3			# backup pointer
	sb	v0,-1(to)		#  store one byte
	sh	v1,-3(to)		#   and one short
	PTR_SUBU to,3			# backup pointer
	subu	count,3			#  and reduce count
	/* FALLTHROUGH */
/*
 * Once we are here, the pointers are aligned on long boundaries.
 * Begin copying in large chunks.
 */
backwards:

/* 32 byte at a time loop */
backwards_32:
	blt	count,32,backwards_16	# do 16 bytes at a time
	lw	v0,-4(from)
	lw	v1,-8(from)
	lw	t0,-12(from)
	lw	t1,-16(from)
	lw	t2,-20(from)
	lw	t3,-24(from)
	lw	ta0,-28(from)
	lw	ta1,-32(from)		# Fetch 8*4 bytes
	PTR_SUBU from,32		# backup from pointer now
	sw	v0,-4(to)
	sw	v1,-8(to)
	sw	t0,-12(to)
	sw	t1,-16(to)
	sw	t2,-20(to)
	sw	t3,-24(to)
	sw	ta0,-28(to)
	sw	ta1,-32(to)		# Store 8*4 bytes
	PTR_SUBU to,32			# backup to pointer now
	subu	count,32		# Reduce count
	b	backwards_32		# Try some more

/* 16 byte at a time loop */
backwards_16:
	blt	count,16,backwards_4	# Do rest in words
	lw	v0,-4(from)
	lw	v1,-8(from)
	lw	t0,-12(from)
	lw	t1,-16(from)
	PTR_SUBU from,16		# backup from pointer now
	sw	v0,-4(to)
	sw	v1,-8(to)
	sw	t0,-12(to)
	sw	t1,-16(to)
	PTR_SUBU to,16			# backup to pointer now
	subu	count,16		# Reduce count
	b	backwards_16		# Try some more

/* 4 byte at a time loop */
backwards_4:
	blt	count,4,backwards_bytecopy	# Do rest
	lw	v0,-4(from)
	PTR_SUBU from,4			# backup from pointer
	sw	v0,-4(to)
	PTR_SUBU to,4			# backup to pointer
	subu	count,4			# Reduce count
	b	backwards_4
	AUTO_CACHE_BARRIERS_ENABLE
	END(nowar_bcopy)
#undef	from
#undef	to
#undef	count
