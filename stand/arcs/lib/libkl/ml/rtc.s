/***********************************************************************\
*	File:		rtc.s						*
*									*
*	Library routines for manipulating the SN0 real-time clock	*
*	for micro-second timing functions.				*
*									*
\***********************************************************************/

#ident "$Revision: 1.14 $"

#include <asm.h>

#include <sys/mips_addrspace.h>
#include <sys/cpu.h>
#include <sys/regdef.h>

#include <sys/SN/agent.h>
#include <sys/SN/SN0/IP27.h>

#include <sys/SN/SN0/ip27config.h>	/* For hub clock frequency */

/*
 * Since the rtc increments every 800 ns, the following macros convert
 * from rtc cycles to microsec and vice versa.	
 */		
#define MULT_FACTOR		10
#define SHFT_FACTOR		3
		
#define MICROSEC_TO_CYCLE_ADJ(_reg1, _reg2)		\
	dli	_reg2,  MULT_FACTOR;			\
	dmultu	_reg1,  _reg2;				\
	mflo	_reg1;					\
	dsrl	_reg1,  SHFT_FACTOR

#define CYCLE_TO_MICROSEC_ADJ(_reg1, _reg2)		\
	dsll	_reg1, SHFT_FACTOR;			\
	dli	_reg2, MULT_FACTOR;			\
	ddivu	_reg1, _reg2


	.text
	.set	reorder

/*
 * Microsecond Timing Routines
 *
 * In the PROM, GCLK is set to run at as close to 1 MHz as possible.
 * It will run at exactly 1 MHz if the hub frequency is a multiple
 * of 2 MHz in the range 10 MHz to 256 MHz, inclusive.  This limitation
 * arises because the MAX_COUNT field only allows the clock divisor to
 * be an even number from 10 to 256.
 *
 * A better solution would be to fix MAX_COUNT at 49 so the GCLK would
 * always run at HUB_CLOCK / 100, and convert to microseconds using the
 * formula (ticks * 100000000 / hub_freq).  Unfortunately, this 64-bit
 * calculation overflows before ticks gets very big.
 */

/*
 * rtc_init
 *
 *   Disables the clock and clears it to zero.  Calculates clock divisor
 *   based on hub frequency, sets it in the hub, and starts the clock
 *   from the internal source.
 */

LEAF(rtc_init)
	dla	a1, LOCAL_HUB(PI_RT_LOCAL_CTRL)
	dla	a2, LOCAL_HUB(PI_RT_COUNT)		# Not same 64k
	dli	a3, IP27CONFIG_ADDR

	dli	v0, PRLC_USE_INT			# Prevent X's in RTL
	sd	v0, 0(a1)				# Disable
	sd	zero, 0(a2)				# Clear

	ld	v0, ip27c_freq_hub(a3)			# Get hub Hz
	ld	v1, ip27c_freq_rtc(a3)			# Get desired RTC Hz
	ddivu	v0, v1

	dsrl	v0, 1					# Adjust to MAX_COUNT
	dsub	v0, 1
	dsll	v0, PRLC_MAX_COUNT_SHFT
	dli	v1, PRLC_USE_INT | PRLC_GCLK_EN
	or	v0, v1

	sd	v0, 0(a1)
	j	ra
	END(rtc_init)

/*
 * rtc_stop
 *
 *   Stops counter without affecting the value.
 */

LEAF(rtc_stop)
	dli	a1, LOCAL_HUB(PI_RT_LOCAL_CTRL)
	ld	v0, 0(a1)
	and	v0, ~1					# GCLK_ENABLE off
	sd	v0, 0(a1)
	j	ra
	END(rtc_stop)

/*
 * rtc_start
 *
 *   Starts the counter incrementing.
 */

LEAF(rtc_start)
	dli	a1, LOCAL_HUB(PI_RT_LOCAL_CTRL)
	ld	v0, 0(a1)
	or	v0, 1					# GCLK_ENABLE on
	sd	v0, 0(a1)
	j	ra
	END(rtc_start)

/*
 * rtc_set(ulong usec)
 *
 *   Sets counter value in microseconds (may be best if counter is stopped).
 */

LEAF(rtc_set)
	MICROSEC_TO_CYCLE_ADJ(a0, a2)
	dli	a1, LOCAL_HUB(PI_RT_COUNT)
	sd	a0, 0(a1)
	j	ra
	END(rtc_set)

/*
 * rtc_time
 *
 *   Return current microsecond counter.
 */

LEAF(rtc_time)
	dli	a1, LOCAL_HUB(PI_RT_COUNT)
	ld	v0, 0(a1)
	CYCLE_TO_MICROSEC_ADJ(v0, a2)
	j	ra
	END(rtc_time)

/*
 * rtc_sleep
 *
 *   Delay for a given number of microseconds.
 *   WARNING: hangs if RTC is stopped.
 */

LEAF(rtc_sleep)
	MICROSEC_TO_CYCLE_ADJ(a0, a2)
	dli	a1, LOCAL_HUB(PI_RT_COUNT)
	ld	v0, 0(a1)		# Get start clock
	dadd	a0, v0			# Calculate wakeup time
	dli	a2, 1
1:
	ddivu	a1, a2			# NOP, wastes 67+ cycles (~1/3 usec)
	ddivu	a1, a2			# NOP, wastes 67+ cycles (~1/3 usec)
					#   Prevents hogging the hub.
	ld	v0, 0(a1)
#ifndef SABLE
	bltu	v0, a0, 1b		# Loop until wakeup time
#endif /* SABLE */
	j	ra
	END(rtc_sleep)


/*
 * inst_loop_delay uses the smallest possible loop to loop a specified number
 * of times.  For accurate micro-second delays based on the hub clock,
 * see rtc_sleep.
 */

LEAF(inst_loop_delay)
	.set noreorder
1:
	bnez    a0, 1b
	 daddu  a0, -1
	j       ra
	 nop
	.set reorder
	END(inst_loop_delay)


