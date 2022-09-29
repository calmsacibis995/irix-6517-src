
#ident	"ide/IP19/lib/EVERESTasm.s:  $Revision: 1.10 $"

/*
 * EVERESTasm.s - ide's EVEREST-specific assembly language functions
 */

#include <asm.h>
#include <regdef.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/sbd.h>
#include <sys/immu.h>

#define POD_STACKADDR	0x90000000000fc000
#define MIN_CACH_POW2	12

#define TWO_LINES_TCODE                         \
        addiu   ta2, s5, 0x0001;               \
        addiu   s5, ta2, 0x0002;               \
        addiu   ta2, s5, 0x0004;               \
        addiu   s5, ta2, 0x0008;               \
        addiu   ta2, s5, 0x0010;               \
        addiu   s5, ta2, 0x0020;               \
        addiu   ta2, s5, 0x0040;               \
        addiu   s5, ta2, 0x0080;               \
        addiu   ta2, s5, 0x0100;               \
        addiu   s5, ta2, 0x0200;               \
        addiu   ta2, s5, 0x0400;               \
        addiu   s5, ta2, 0x0800;               \
        addiu   ta2, s5, 0x1000;               \
        addiu   s5, ta2, 0x2000;               \
        addiu   ta2, s5, 0x4000;               \
        addiu   s5, ta2, 0x8000



/* Four icache lines per gcache line (128 bytes) */
#define FOUR_LINES_TCODE                \
        TWO_LINES_TCODE;                \
        TWO_LINES_TCODE

/* 32 icache lines per 1k */
#define THIRTYTWO_LINES_TCODE                   \
        FOUR_LINES_TCODE; FOUR_LINES_TCODE;     \
        FOUR_LINES_TCODE; FOUR_LINES_TCODE;     \
        FOUR_LINES_TCODE; FOUR_LINES_TCODE;     \
        FOUR_LINES_TCODE; FOUR_LINES_TCODE      \

#define ICACHE_LINE_CODE                                \
        addu    zero, zero, zero;       /* 0 */         \
        addu    zero, zero, zero;       /* 4 */         \
        addu    zero, zero, zero;       /* 8 */         \
        addu    zero, zero, zero;       /* 12 */        \
        addu    zero, zero, zero;       /* 16 */        \
        addu    zero, zero, zero;       /* 20 */        \
        addu    zero, zero, zero;       /* 24 */        \
        addu    zero, zero, zero        /* 28 */

/* Four icache lines per gcache line (128 bytes) */
#define FOUR_LINES_CODE         \
        ICACHE_LINE_CODE;       \
        ICACHE_LINE_CODE;       \
        ICACHE_LINE_CODE;       \
        ICACHE_LINE_CODE        \

/* 32 icache lines per 1k */
#define THIRTYTWO_LINES_CODE                    \
        FOUR_LINES_CODE; FOUR_LINES_CODE;       \
        FOUR_LINES_CODE; FOUR_LINES_CODE;       \
        FOUR_LINES_CODE; FOUR_LINES_CODE;       \
        FOUR_LINES_CODE; FOUR_LINES_CODE        \



/*
 * Routine _get_timestamp
 * Arguments:
 *	None.
 * Returns:
 *	32-bit value from EV_RTC
 * Uses:
 *	v0, v1.
 */
LEAF(_get_timestamp)
#if _MIPS_SIM == _ABI64
	dli	v1,EV_RTC
#else
	lw	v0,4(v1)
#endif
	j	ra
	END(_get_timestamp)

/* clear the bus tags. */
LEAF(clear_bustags)

	.set	noreorder
	mfc0	v1, C0_CONFIG
	.set	reorder

	lw	v0, _sidcache_size

        # Figure out the scache block size
#ifdef IP19
        and	v1, v1, CONFIG_SB	# Pull out the bits
        srl	v1, CONFIG_SB_SHFT
#endif
        addu	v1, 4			# Turn 0,1,2,3 into 4,5,6,7

	srl	t1, v0, v1		# Divide scache size by line size.
	sll	t1, 3			# Four bytes per doubleword

	/* Calculate the size of bus tag RAM 
	 * 2^18 = 256k is the smallest possible cache.
	 * It requires 24 bits of bustags.
	 * There's a tag for each scache line.
	 * 	t1 = size of tag RAM space
	 */

	# v0 still = cachesize in bytes

	move	t0, zero
	srl	v0, 19
1:	
	sll	t0, 1
	srl	v0, 1
	ori	t0, 1
	bnez	v0, 1b

	/* Bus tags are 24 bits aligned at the top of a word.
	 *	Tag RAM space size == (SCache_size >> 7) * 4
	 */
	# t1: tag RAM size
	# t0: tag RAM mask

#ifdef TFP
	dli	t2, BB_BUSTAG_ADDR
	daddu    s0, t2, t1			# loop termination
#elif IP19
	li      t2, EV_BUSTAG_BASE
	addu    t4, t2, t1			# loop termination
#endif
1:
	sd	zero, 0(t2)			# write tags
#if _MIPS_SIM == _ABI64
	daddu	t2, 8				# next word
	bne	t2, s0, 1b
#else
	addu	t2, 8				# next word
	bne	t2, t4, 1b
#endif

	j	ra
	END(clear_bustags)

/*
 * Routine set_cc_leds
 * Arguments:
 *	a0 -- Value to display on CC leds (0 - 63)
 * Returns:
 *	Nothing.
 * Uses:
 *	a0, v0.
 */
LEAF(_set_cc_leds)
XLEAF(_1flash_cc_leds)
#if _MIPS_SIM == _ABI64
	dli	v0, EV_LED_BASE
#else
	li	v0, EV_LED_BASE
#endif
	sd	a0, 0(v0)
	j	ra	
	END(_set_cc_leds)


/*
 * Routine _flash_cc_leds
 *	Causes a specific value to be flashed on the CC leds.
 *	Note that this routine NEVER RETURNS.
 *
 * Arguments:
 *	a0 -- value to be flashed.
 *	a1 -- if == 0 --> one iteration then return
 *	      else never return)
 */

LEAF(_flash_cc_leds)
	.set	noreorder
	move	a3, a0			# Save the argument passed
1:					# LOOP FOREVER 
	jal	_set_cc_leds		#   Turn off the LEDS
	move	a0, zero		#   (BD)
	li	a0,150000		#    
2:	bne	a0,zero,2b		#   Delay 
 	subu	a0,1			#   (BD) Decrement loop count	

	jal	_set_cc_leds		#   Turn on value in LEDS
	move	a0,a3			#   (BD)	
	li	a0, 150000	
3:	bne	a0, zero, 3b		#   Delay
	subu	a0, 1			#   (BD)
	
	bne	a1, zero, 1b
	nop
	j	ra			# end LOOP
	nop
	.set	reorder

	END(_flash_cc_leds)

#if TFP
LEAF(_read_tag)				# this function is just stubbed in
	.set noreorder			# for now
	j	ra
	nop
	.set	reorder
	END(_read_tag)

LEAF(invalidate_caches)
        .set    noreorder
        .align 5                        # first invalidate the icache

        THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE      /* 2k */
        THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE      /* 4k */
        THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE      /* 6k */
        THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE      /* 8k */
        THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE      /* 10k */
        THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE      /* 12k */
        THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE      /* 14k */
        THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE      /* 16k */

        j       invalidate_dcache   # now go do the dcache
        nop
END(invalidate_caches)



/* Initialize DCache by invalidating it.  DCache is 16KB, with 32Byte line, 
 * 28 bits Tag + 1 bit Exclusive + 8 bits Valid + 32 Bytes Data
 * Need two consecutive writes to half-line boundaries to clear Valid bits
 * for each line.
 */

LEAF(invalidate_dcache)
	.set	noreorder
        move    t3, ra
        li      v1, 16			# size of the half of Dcache line
        jal     get_dcachesize
	nop

        # v1: half line size, v0: cache size to be initialized

        dli     t1,POD_STACKADDR	# Starting address for initializing
					# 	Dcache
        daddu   t2, t1, v0              # t2: loop terminator

        # for each cache line:
        # 	mark it invalid
1:
        DMTC0(t1,C0_BADVADDR)
	DMTC0(zero,C0_DCACHE)		# clear all (4) Valid bits
	dctw				# write to the Dcache Tag 
	ssnop; ssnop; ssnop		# pipeline hazard prevention
        .set    reorder

        daddu   t1, v1                 # increment to next half-line
        bltu    t1, t2, 1b		# t2 is termination count
	move	v0, zero
        j       t3
END(invalidate_dcache)
#endif	/* TFP */

/*
 * return the size of the primary data cache
 */
LEAF(get_dcachesize)
        .set    noreorder
        DMFC0(t0,C0_CONFIG)
        .set reorder
        and     t0,CONFIG_DC
        srl     t0,CONFIG_DC_SHFT
        addi    t0,MIN_CACH_POW2
        li      v0,1
        sll     v0,t0
        j       ra
        END(get_dcachesize)


#ifdef TFP
/*
 * Return the cause register to the calling routine
 */

LEAF(ret_cause)
	.set noreorder
	DMFC0(v0,C0_CAUSE)
	.set reorder
	j	ra
	END(ret_cause)
/*
 * Return the status register to the calling routine
 */

LEAF(ret_sr)
	.set noreorder
	DMFC0(v0,C0_SR)
	.set reorder
	j	ra
	END(ret_sr)

/*
 * Return the prid register to the calling routine
 */

LEAF(ret_prid)
	.set noreorder
	DMFC0(v0,C0_PRID)
	.set reorder
	j	ra
	END(ret_prid)


/*
 * Return the confif register to the calling routine
 */

LEAF(ret_config)
	.set noreorder
	DMFC0(v0,C0_CONFIG)
	.set reorder
	j	ra
	END(ret_config)


LEAF(init_interrupts)
        .set    noreorder
#if TFP_ADE_EBUS_WBACK_WAR
        /*
         * The logic in the CC that reports address errors on EBus
         * writeback channel is broken. Turn off the bit in the diag
         * register. Note that the sense of the bits is reversed from
         * the spec (only when we write the register)!
         * For now, just slam a hard coded constant in there. If we
         * ever need to touch the register for some other reason,
         * we'll add #define's.
         * The ip21prom writes the same value, but we do it here in case
         * the system has old proms.
         */
        LI      v0, EV_DIAGREG
        LI      v1, 0x40900000          # disable error bit 4 and
        sd      v1, 0(v0)               # preserve "in queue almost full" (9)
#endif
        LI      v0, EV_CIPL0
        li      v1, 127                 # Highest interrupt number
clear_ints:
        sd      v1, 0(v0)               # Clear interrupt (BD)
        bne     v1, zero, clear_ints
        addi    v1, -1                  # Decrement counter (BD)

        j       ra
        nop
        .set    reorder
        END(init_interrupts)

#endif
