/*
 * Assembly-language source for calibrated delay.
 */
#ident "$Revision: 1.30 $"

#include <ml/ml.h>

#ifdef R10000
/* based on trying on a few parts, and tuning to scalar dispatch */
#define LOOP_FACTOR	nop;nop;nop;nop;nop;nop;nop
#else
#define LOOP_FACTOR
#endif

#if defined(PT_CLOCK_ADDR) || defined(PT_CLOCK_OFFSET)
/*
 * Calculate the number of ticks at 3.6864MHz to execute 1024 instructions,
 * using counter 2 on the 8254 programmable interval timer.
 *
 * This routine should *not* be called on early revision Indy systems,
 * spefically p0 systems and p1 systems with IOC1.1.  This is because
 * the 8254 did not count correctly.  On IOC1.2 (or IOC2), the 8254
 * does count correctly, it just doesn't interrupt correctly.  So this
 * routine can be used in that case.
 *
 * The routine attempts to discover if it has been erronously called
 * and if so, returns an answer consistent with a 50Mhz R4000SC (19).
 */
#define SB(src,dst)	sb src,dst; wbflushm

#define	COUNT	10000

/*
 * This routine should *not* be called on early revision Indy
 * systems, spefically p0 systems.
 */
LEAF(_ticksper1024inst)
#if IP22
	LI	t1,(K1BASE|HPC3_INT2_ADDR+PT_CLOCK_OFFSET) # assume INT2
	lw	t2,(K1BASE|HPC3_SYS_ID)
	andi	t3, t2, CHIP_IOC1
	beqz	t3, 4f			# not IOC1 - use INT2's address

	andi	t3, t2, BOARD_IP22
	bnez	t3, 5f			# IP22 w/IOC1 - use INT3's address

	andi	t3, t2, BOARD_REV_MASK
	sub	t3, 2 << BOARD_REV_SHIFT
	bgez	t3, 5f			# At least a P1 board - use INT3's adrs

	li	v0, 19			# PTC doesn't work - use 50Mhz constant
	j	ra

5:	LI	t1,(K1BASE|HPC3_INT3_ADDR+PT_CLOCK_OFFSET)
4:
#elif IP26
	LI	t1,(K1BASE|HPC3_INT2_ADDR+PT_CLOCK_OFFSET)
#elif IP28
	CLI	t1,(COMPAT_K1BASE32|HPC3_INT2_ADDR+PT_CLOCK_OFFSET)
#else
	li	t1,PT_CLOCK_ADDR
#endif
2:
	li	t0,2			# loop twice in case of I-cache
1:
#ifdef R10000
	li	ta1,1024		# scale for super-scalar
#else
	li	ta1,512
#endif
	li	t2,PTCW_SC(2)|PTCW_16B|PTCW_MODE(2) 
	li	t3,PTCW_SC(2)|PTCW_CLCMD
	SB(t2, PT_CONTROL(t1))
	li	t2,COUNT&0xff		# lsb
	SB(t2, PT_COUNTER2(t1))
	li	t2,COUNT>>8		# msb
	SB(t2, PT_COUNTER2(t1))

	# 1024 inst
	.set	noreorder
3:	LOOP_FACTOR
	bgt	ta1,zero,3b
	subu	ta1,1
	.set reorder
	SB(t3, PT_CONTROL(t1))		# stop counting now!
	.set	noreorder
	lbu	t3,PT_COUNTER2(t1)	# lsb
	nop				# BDSLOT
	lbu	ta0,PT_COUNTER2(t1)	# msb
	nop				# BDSLOT
	.set	reorder
	sll	ta0,8
	or	t3,ta0			# t3 contains ending count
	subu	t0,1			# loop again from primed cache
	bnez	t0,1b

	li	t2,PTCW_SC(2)|PTCW_16B|PTCW_MODE(MODE_STS)
	SB(t2, PT_CONTROL(t1))
	li	v0,COUNT
	blt	v0,t3,2b		# XXX sometimes get msb before lsb!
	subu	v0,t3
	j	ra
	END(_ticksper1024inst)

#if defined(IP20) || defined(IP22) || defined(IP26) || defined(IP28)
#define	COUNT0	0x164

/*
 * find out the r4000 timer count delta in ~100 i8254 ticks
 * THIS IS FOR R4000 INDIGO and INDIGO2 (and >P1 INDY) ONLY
 *
 * This routine should *not* be called on early revision Indy systems,
 * spefically p0 systems and p1 systems with IOC1.1.  This is because
 * the 8254 did not count correctly.  On IOC1.2 (or IOC2), the 8254
 * does count correctly, it just doesn't interrupt correctly.  So this
 * routine can be used in that case.
 *
 * The routine attempts to discover if it has been erronously called
 * and if so, returns an answer consistent with a 50Mhz R4000SC.
 *
 * The early logic here determines which type of running system we have:
 *
 *    IP22 w/o IOC  - use address of INT2 + 8254 offset 
 *    IP22 w   IOC  - use address of INT3 + 8254 offset
 *    IP24 rev >= 2 - use address of INT3 + 8254 offset
 *    IP24 rev < 2  - use R4000SC constant (5153)
 *
 *   (The number 5153 was an average of a few test runs of this routine.
 *    The range was from 5040 to 5207.)
 *
 */
LEAF(_cpuclkper100ticks)
#if IP22
	LI	t1,(K1BASE|HPC3_INT2_ADDR+PT_CLOCK_OFFSET) # assume INT2
	lw	t2,(K1BASE|HPC3_SYS_ID)
	andi	t3, t2, CHIP_IOC1
	beqz	t3, 2f			# not IOC1 - use INT2's address

	andi	t3, t2, BOARD_IP22
	bnez	t3, 3f			# IP22 w/IOC1 - use INT3's address

	andi	t3, t2, BOARD_REV_MASK
	sub	t3, 2 << BOARD_REV_SHIFT
	bgez	t3, 3f			# At least a P1 board - use INT3's adrs

	li	v0, 5153		# PTC doesn't work - use 50Mhz constat
	j	ra

3:	LI	t1,(K1BASE|HPC3_INT3_ADDR+PT_CLOCK_OFFSET)
2:
#elif IP26 || IP28
	.set	noreorder
#ifdef TFP
	dmtc0	zero,C0_COUNT		# ensure no interrupt
#endif
	.set	reorder
	LI	t1,(K1BASE|HPC3_INT2_ADDR|PT_CLOCK_OFFSET)
#else
	LI	t1,PT_CLOCK_ADDR
#endif
	li	t2,PTCW_SC(2)|PTCW_16B|PTCW_MODE(2) 
	li	t3,PTCW_SC(2)|PTCW_CLCMD
	SB(t2, PT_CONTROL(t1))
	li	t2,COUNT0&0xff		# lsb
	SB(t2, PT_COUNTER2(t1))
	li	t2,COUNT0>>8		# msb
	SB(t2, PT_COUNTER2(t1))

	.set	noreorder
#if TFP
	dmfc0	v0,C0_COUNT		# initial TFP timer count
#else
	mfc0	v0,C0_COUNT		# initial r4000 timer count
#endif
	.set	reorder

1:	SB(t3, PT_CONTROL(t1))		# latch counter
	.set	noreorder
#if TFP
	dmfc0	v1,C0_COUNT		# current TFP timer count
#else
	mfc0	v1,C0_COUNT		# current r4000 timer count
#endif
	lbu	a1,PT_COUNTER2(t1)	# lsb
	lbu	a2,PT_COUNTER2(t1)	# msb
	nop				# BDSLOT
	.set	reorder
	bnez	a2,1b

	li	t2,PTCW_SC(2)|PTCW_16B|PTCW_MODE(MODE_STS)
	SB(t2, PT_CONTROL(t1))		# stop counting

	li	a2,COUNT0
	subu	a1,a2,a1
	sw	a1,0(a0)		# actual number of i8254 timer ticks

	subu	v0,v1,v0		# r4000 timer count delta 
	j	ra
	END(_cpuclkper100ticks)
#endif	/* IP20 || IP22 || IP26 || IP28 */
#endif	/* defined(PT_CLOCK_ADDR) */

#if IP32
/*
 * this value is chosen because the crime counter has an input
 * frequency of 66Mhz while the 8254 on IP22 has an input frequency
 * of 1Mhz.
 */
#define COUNT (0x164 * 66)
#define TRIAL_COUNT 4
	
/*
 * _ticksper1024inst() -- returns number of CRM_TIME ticks in the
 *      time it takes to execute 1024 instructions.
 *
 * register usage:
 *	v0 -- number of ticks
 *	t0 -- Address of CRM_TIME register
 *	t1 -- Count of instructions to execute
 *	t2 -- Starting number of ticks
 *	t3 -- scratch
 */	
LEAF(_ticksper1024inst)
	la	t0,CRM_TIME|K1BASE
	li	t1,1		# for first time around, just prime i-cache
	li	t3,2		# we'll do through the code twice  
1:
	ld	t2,0(t0)
	.set noreorder
2:	bgt	t1,zero,2b
	subu	t1,1
	.set reorder
	ld	v0,0(t0)
	dsubu	v0,t2

	li	t1,512
	subu	t3,1		# first time was just to prime i-cache
	bgt	t3,zero,1b	# go back for the real calibration

	j	ra
	END(_ticksper1024inst)


/*
 * _cpuclkper100ticks(&tick_count) - returns the number of cpu clocks
 * required for COUNT ticks of the CRIME time base register.  tick_count
 * will contain the actual number of ticks of the CRIME timer (which will
 * not be 100).
 *
 * We prime the cache before beginning measurement and then take the
 * average of 5 trials.
 *
 * register usage:
 *	t0 -- limit timer count
 *	t1 -- address of crime time register
 *	t2 -- current value of CRM_TIME
 *	t3 -- loop count register
 *	ta0 -- scratch
 *	ta1 -- total of CRM_TIME ticks
 *      ta2 -- total of C0_COUNT ticks
 *
 * We make no provision for checking for wrap around on the CRM_TIME
 * register or C0_COUNT since each of these registers will take several
 * minutes from boot to wrap around (about 30 for C0_COUNT, much much 
 * longer for CRM_TIME -- 71079 minutes).
 *
 * NB:	 it is OK to write C0_COUNT here since we have yet to initialize 
 *       the clock.
 */
LEAF(_cpuclkper100ticks)
	.set noreorder
	mtc0	zero,C0_COUNT
	.set reorder
	la	t1,CRM_TIME|K1BASE
	li	t3,TRIAL_COUNT-1
	la	ta0,1f
	li	ta1,0
	li	ta2,0

	#
	# now suck in the code we are about to execute into the
	# cache so that we can avoid cache fill induced delays
	# during the execution.
	#
	.set noreorder
	cache	CACH_PI|C_FILL,0(ta0)
	cache	CACH_PI|C_FILL,32(ta0)
	cache	CACH_PI|C_FILL,64(ta0)
	cache	CACH_PI|C_FILL,96(ta0)
	cache	CACH_PI|C_FILL,128(ta0)
1:	
	mtc0	zero,C0_COUNT
	sd	zero,0(t1)
	li	t0,COUNT
2:	
	ld	t2,0(t1)
	dsll32	t2,0
	dsra32	t2,0
	mfc0	v0,C0_COUNT
	sltu	ta0,t2,t0
	bne	ta0,zero,2b
	nop
	.set reorder

	addu	ta1,t2		# total CRM_TIME ticks for all runs
	addu	ta2,v0		# total C0_COUNT ticks for all runs

	.set noreorder
	bne	t3,zero,1b
	addi	t3,0xffff
	.set reorder

	li	t3,TRIAL_COUNT

	divu	ta2,t3
	mflo	v0		# average of runs

	divu	ta1,t3
	mflo	ta0		# average of runs
	sw	ta0,0(a0)

	j	ra
	END(_cpuclkper100ticks)

#define CALIBRATION_LOOP_CNT	(100000)
/*
 * delay_calibrate(void) -- 
 *      calculates number of CRM_TIME ticks in the
 *      time it takes to execute the CALIBRATION_LOOP_CNT
 *	and sets the us_delay decrementer in the pda
 * 	XXX there is a possible counter roll-over but
 *	the crime counter is 48 bits and would take 150 days to
 *	roll over.
 *
 * register usage:
 *	v0 -- number of ticks
 *	t0 -- Address of CRM_TIME register
 *	t1 -- Count of calibration loop
 *	t2 -- Starting number of ticks
 *	t3 -- counter for i-cache fill
 */	
LEAF(delay_calibrate)
	la	t0,CRM_TIME|K1BASE
	li	t1,1		# for first time around, just prime i-cache
	li	t3,2		# we'll do through the code twice  
1:
	.set noreorder
	ld	t2,0(t0)
2:	
	LOOP_FACTOR
	bgt	t1,zero,2b
	subu	t1,1
	.set reorder
	ld	v0,0(t0)
	dsubu	v0,t2

	li      t1,(CALIBRATION_LOOP_CNT-1)
	subu	t3,1		# first time was just to prime i-cache
	bgt	t3,zero,1b	# go back for the real calibration
	/*
	 * on IP32 the MASTER_FREQ is 66.666500 MHz, giving
	 * DNS_PER_TICK 15 nanosecond period per tick.
	 * We want nanosecs per CALIBRATION_LOOP to use in
	 * us_delay as the nanosec decrementer ...
	 * So given
	 *    v0 = number of master_freq ticks taken for CALIBRATION_LOOP_CNT
	 *
	 *    (v0 * DNS_PER_TICK) / CALIBRATION_LOOP_CNT
	 */
	mul	v0,DNS_PER_TICK
	divu	v0,CALIBRATION_LOOP_CNT
	sw	v0,VPDA_DECINSPERLOOP(zero)	# pda.decinsperloop

	j	ra
	END(delay_calibrate)

/* void delayloop(uint count, uint decr)
 *      - delay loop with a loop factoring, and also helps sometimes with
 *        messy compilers.
 *
 * register usage:
 *	a0 -- nano secs to spin
 *	a1 -- nano secs to decrement per iteration
 */
.align 6
LEAF(delayloop)
	sync
	.set noreorder
1:
	LOOP_FACTOR
	bgt     a0,zero,1b
	subu    a0,a1
 
	.set reorder
	j       ra
	END(delayloop)
#endif /* IP32 */

	
#if defined(IP20) || defined(IP22) || defined(IP26) || defined(IPMHSIM)

/* void delayloop(int count, int decr) */
	
LEAF(delayloop)
	.set	noreorder
1:	bgt	a0,zero,1b
	subu	a0,a1				# BDSLOT
	j	ra
	nop
	.set	reorder	
	END(delayloop)	
#endif
