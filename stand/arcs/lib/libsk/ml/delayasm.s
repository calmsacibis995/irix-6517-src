#ident	"saio/lib/delayasm.s:  $Revision: 1.43 $"

#include	"ml.h"
#include <asm.h>
#include <regdef.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/i8254clock.h>

#if defined(IP28)
/* based on trying on a few parts, and tuning to scalar dispatch */
#define LOOP_FACTOR	nop;nop;nop;nop;nop;nop;nop
#elif defined(IP30)
#define SUB_LOOP	10
#define LOOP_FACTOR		 \
	li      t1,SUB_LOOP	;\
11:	bgt     t1,zero,11b	;\
	subu    t1,1
#else
#define LOOP_FACTOR
#endif

#define LOOP_COUNT	(512 - 1)

#if !IP30
#if MCCHIP
/* flush write buffers by reading from any internal register of
 * MC chip
 */
#ifndef R10000
#define wbflushm	\
	.set	noreorder ; \
	lw	zero,PHYS_TO_COMPATK1(CPUCTRL0) ;\
	.set	reorder
#else
#define wbflushm	\
	.set	noreorder ; \
	lw	zero,PHYS_TO_COMPATK1(CPUCTRL0) ;\
	sync					; \
	.set	noat				; \
	mfc0	AT,C0_SR			; \
	.set	at				; \
	.set	reorder
#endif	/* R10000 */
#endif	/* MCCHIP */
#if IP32
/*
 * Do an uncached read to flush the write buffer
 *
 * This routine is mostly used to affect changes to interrupts and
 * since it takes a little time for interrupts to propagate on the
 * R4000, some nops are added at the end.
 */
#define wbflushm                                  \
        .set    noreorder                       ; \
        .set    noat                            ; \
        lui     AT,(CRM_CONTROL|K1BASE)>>16     ; \
        or      AT,CRM_CONTROL&0xffff           ; \
        ld      AT,0(AT)                        ; \
        nop                                     ; \
        nop                                     ; \
        nop                                     ; \
        nop                                     ; \
        nop                                     ; \
        .set    at                              ; \
        .set    reorder

#endif /* IP32 */

# define SB(src,dst)	sb src,dst; wbflushm

#if !IP32
#define	COUNT	10000

/* WARNING - this routine DOES NOT WORK on IP24.P0 (broken 8254),
 *      should use _timerticksper1024inst() instead.
 */
LEAF(_ticksper1024inst)
2:
	li	t0,2			# loop twice in case of I-cache
1:
	li	ta1,LOOP_COUNT
#if IP22
	LI	t1,PHYS_TO_K1(HPC3_INT2_ADDR)	# assume INT2
	lw	t2, HPC3_SYS_ID|K1BASE
	andi	t3, t2, CHIP_IOC1
	beqz	t3, int2		# branch if not IOC1/INT3

	# IOC1.2+ We can use timers in INT3 (since we are only polling)
	andi	t3, t2, BOARD_REV_MASK
	sub	t3, 2 << BOARD_REV_SHIFT 
	bgez	t3, int3		# branch if IOC1.2+

	# IOC1.1  Take a guess based on if we are running cached or not
	# based on 50Mhz R4000 IP20 CPU values.
	#
	li	v0, 300			# default to uncached value
	LI	t0, K1BASE		# are we in K1 
	and	t1, t0, ra
	xor	t1, t0
	beqz	t1, 15f			# ok to use uncached value
	li	v0, 20			# use cached value
15:	j	ra

int3:	LI	t1,PHYS_TO_K1(HPC3_INT3_ADDR)	# is INT3
int2:	PTR_ADDIU	t1,PT_CLOCK_OFFSET
#elif IP28	/* is in compatability space */
	CLI	t1,PT_CLOCK_ADDR		# get clock base addr
#else
	LI	t1,PT_CLOCK_ADDR		# get clock base addr
#endif
	li	t2,PTCW_SC(2)|PTCW_16B|PTCW_MODE(2) 
	li	t3,PTCW_SC(2)|PTCW_CLCMD
	SB(t2, PT_CONTROL(t1))
	li	t2,COUNT&0xff		# lsb
	SB(t2, PT_COUNTER2(t1))
	li	t2,COUNT>>8		# msb
	SB(t2, PT_COUNTER2(t1))

	# The R4000 has 3 delay slots after a branch instruction, one
	# of which is software visible (can have an instruction executed
	# in the delay slot); the other 2 are software invisible.
	# So this loop of 1024 instructions actually takes 2048
	# internal R4000 clocks.
	# Since the R4000 internal clock executes at 2 times the rate
	# of the R4000 nominal rate (i.e. the 50mhz part has an internal
	# clock of 100mhz), these factors nicely cancel out.

	# 1024 inst
	.set	noreorder
3:
	LOOP_FACTOR
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
	blt	v0,t3,2b		# sometimes get msb before lsb!
	subu	v0,t3
	j	ra
	END(_ticksper1024inst)
#endif /* !IP32 */

LEAF(_delayend)
	.set	noreorder
	nop
	.set	reorder
	END(_delayend)


LEAF(_timerticksper1024inst)
1:	li	ta1,LOOP_COUNT
	.set	noreorder
	DMFC0(t0, C0_COUNT)
3:	bgt	ta1, zero, 3b
	subu	ta1, 1
	DMFC0(v0, C0_COUNT)
	nop
	nop
	.set	reorder
	subu	v0, t0
	bltz	v0, 1b		# rolled over
	j	ra
	END(_timerticksper1024inst)
#endif	/* !IP30 */

#if IP30
LEAF(_ticksper1024inst)
	li	a0,LOOP_COUNT
	CLI	t0,PHYS_TO_COMPATK1(HEART_COUNT)
	.align	6
	ld	v0,0(t0)
	.set	noreorder
3:
	LOOP_FACTOR
	bgt	a0,zero,3b
	subu	a0,1

	ld      v1,0(t0)
	.set	reorder
	dsubu	v0,v1,v0

	j	ra
	END(_ticksper1024inst)
#endif	/* IP30 */


#if IP32
/*
 * Do an uncached read to flush the write buffer
 *
 * This routine is mostly used to affect changes to interrupts and
 * since it takes a little time for interrupts to propagate on the
 * R4000, some nops are added at the end.
 */
#define wbflushm                                  \
        .set    noreorder                       ; \
        .set    noat                            ; \
        lui     AT,(CRM_CONTROL|K1BASE)>>16     ; \
        or      AT,CRM_CONTROL&0xffff           ; \
        ld      AT,0(AT)                        ; \
        nop                                     ; \
        nop                                     ; \
        nop                                     ; \
        nop                                     ; \
        nop                                     ; \
        .set    at                              ; \
        .set    reorder

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
 *      v0 -- number of ticks
 *      t0 -- Address of CRM_TIME register
 *      t1 -- Count of instructions to execute
 *      t2 -- Starting number of ticks
 *      t3 -- scratch
 */
LEAF(_ticksper1024inst)
        la      t0,CRM_TIME|K1BASE
        la      t3,1f
        .set noreorder
        #
        # prime the cache
        #
        cache   CACH_PI|C_FILL, 0(t3)
        cache   CACH_PI|C_FILL, 32(t3)
        cache   CACH_PI|C_FILL, 64(t3)
        .set reorder
1:
        li      t1,LOOP_COUNT
        ld      t2,0(t0)
        .set noreorder
2:      bgt     t1,zero,2b
        subu    t1,1
        .set reorder
        ld      v0,0(t0)
        dsubu   v0,t2

        j       ra
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
 *      t0 -- limit timer count
 *      t1 -- address of crime time register
 *      t2 -- current value of CRM_TIME
 *      t3 -- loop count register
 *      ta0 -- scratch
 *      ta1 -- count increment
 *      ta2 -- flag to indicate a wrap on the crime time counter
 *      ta3 -- total of CRM_TIME ticks
 *      t8 -- total of C0_COUNT ticks
 *
 * We make no provision for checking for wrap around on the CRM_TIME
 * register or C0_COUNT since each of these registers will take several
 * minutes from boot to wrap around (about 30 for C0_COUNT, much much
 * longer for CRM_TIME -- 71079 minutes).
 *
 * NB:   it is OK to write C0_COUNT here since we have yet to initialize
 *       the clock.
 */
LEAF(_cpuclkper100ticks)
        .set noreorder
        mtc0    zero,C0_COUNT
        .set reorder
        la      t1,CRM_TIME|K1BASE
        li      t3,TRIAL_COUNT-1
        li      ta1,COUNT
        la      ta0,1f
        li      ta3,0
        li      t8,0

        #
        # now suck in the code we are about to execute into the
        # cache so that we can avoid cache fill induced delays
        # during the execution.
        #
        .set noreorder
        cache   CACH_PI|C_FILL,0(ta0)
        cache   CACH_PI|C_FILL,32(ta0)
        cache   CACH_PI|C_FILL,64(ta0)
        cache   CACH_PI|C_FILL,96(ta0)
        cache   CACH_PI|C_FILL,128(ta0)
1:     
        ld      ta0,0(t1)
        mfc0    v1,C0_COUNT
        daddu   t0,ta0,ta1
2:     
        ld      t2,0(t1)
        mfc0    v0,C0_COUNT
        sltu    ta0,t2,t0
        bne     ta0,zero,2b
        nop
        .set reorder

        dsubu   ta0,t2,t0        # calculate total
        daddu   ta0,ta1           # number of CRM_TIME ticks
        daddu   ta3,ta0           # total for all runs

        subu    ta0,v0,v1        # number of C0_COUNT ticks
        addu    t8,ta0           # total for all runs

        .set noreorder
        bne     t3,zero,1b
        addi    t3,0xffff
        .set reorder

        li      t3,TRIAL_COUNT

        divu    t8,t3
        mflo    v0              # average of runs

        divu    ta3,t3
        mflo    ta0              # average of runs
        sw      ta0,0(a0)

        j       ra
        END(_cpuclkper100ticks)
#endif


/* void delayloop(ulong count, ulong decr)
 *	- delay loop with a loop factoring, and also helps sometimes with
 *	  messy compilers.
 *	also make sure the delayloop fits in 1 i-cache line
 */
.align	6
LEAF(delayloop)
	.set noreorder
#ifdef R10000
	sync
#endif
#ifdef US_DELAY_DEBUG
	mfc0	t0,C0_COUNT
#endif
1:
	LOOP_FACTOR
	bgt	a0,zero,1b
	LONG_SUBU a0,a1
#ifdef US_DELAY_DEBUG
	mfc0	t1,C0_COUNT
	CACHE_BARRIER
	sw	t0,us_before
	sw	t1,us_after
#endif
	.set reorder
	j	ra
	END(delayloop)
