/***********************************************************************\
*	File:		bootmaster.s					*
*									*
*	This file contains the code which performs various types of	*
*	arbitrations.  Among other things it includes the code to	*
*	have the processors arbitrate for the role of the system's	*
*	Boot Master.  It also includes the code non-bootmaster 		*
*	processors execute.						*
*									*
\***********************************************************************/

#ident "$Revision: 1.19 $"

#include "ml.h"
#include <sys/regdef.h>
#include <asm.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/sysctlr.h>
#include "ip21prom.h"
#include "prom_intr.h"
#include "prom_leds.h"

	.set	noreorder
	.set	noat

/*
 * Routine arb_bootmaster
 * Arguments:
 * 	None.
 * Returns:
 *	v0 -- If v0 == 1, processor is the boot master.
 *	      If v0 == 0, processor is a slave.	
 */

#define RTC_SPEED	50000000	/* RTC will run no faster than this */ 

#if SABLE
#define SYSCTLR_TIMEOUT 1		/* Number of iterations to wait */
#else
#define SYSCTLR_TIMEOUT 20000000	/* Number of iterations to wait */
#endif

LEAF(arb_bootmaster)
	/*
	 * Get all of the processors in sync by waiting for the
	 * RTC to reach a specific value. This avoids problems in
	 * systems in which some processors are much faster than
	 * others.
	 */
	dli	t0, EV_RTC		# Load address of Realtime clock
	ld	t3, 0(t0)		# Read clock
	li	t1, (2 * RTC_SPEED)
	nop
	sub	t3, t1			# Subtract barrier time from RTC time
#if SABLE
	li	t3,-1			# Get out of loop (RTC broken in sable)
#endif
	bltz	t3, 2f			# IF we've already broken wait barrier 
	nop				# THEN
	jal	flash_cc_leds		#   Flash the LEDS
	ori	a0, zero, FLED_BROKEWB	#   (BD) load value for flashing	
					# ENDIF	
2:
	ld	t3, 0(t0)		# Read the real-time clock
	nop				#
	subu	t3, t1			# Compute difference 
#if SABLE
	li	t3,0			# Get out of loop (RTC broken in sable)
#endif
	bltz	t3, 2b			# Loop back if RTC < BARRIER_PERIOD 
	nop

	.globl	rearb_bootmaster
rearb_bootmaster:

	/*
	 * Now that everything is quiescent, clear out
	 * interrupts.
	 */
	dli	t0, EV_IGRMASK
	jal	pod_clear_ints
	sd 	zero, 0(t0)

	jal	ccuart_flush
	nop

	GET_BSR(t0)
	andi	t0, BS_ABDICATED	# See if we have already been master
	beqz	t0, 1f
	nop				# (BD)

	dli	t3, EV_IP0		# Load the interrupt pending addr
78:					# DO
	ld	ta0, 0(t3)		#   Get Interrupt Pending mask
	nop				#
	and	ta0, (1 << BOOT_LEVEL)	#   Check to see if intr arrived 
	beq	ta0, zero, 78b		# LOOP UNTIL int or timer expired
	nop				# (BD)

	j	prom_slave
	nop
1: 
	/*
	 * Calculate the time period during which the processor will
	 * wait for someone else to claim the role of boot master.
	 */
	li 	t2, (RTC_SPEED / 64)	# Time to wait per processor
	dli	t3, EV_SPNUM		#	
	ld	t3, 0(t3)		# Load our slot&proc number
	and	t3, EV_SPNUM_MASK	# Get our PPID
	multu	t3, t2			# offset = PPID * (CLKFREQ/64)
	nop
	nop
	mflo	t1			# Get the result of the multiply
	dli	t0, EV_RTC
	ld	t2, 0(t0)		# Read the RTC 
	nop				# 
	add	t2, t1			# Add delay time to current time

	/*
	 * Now we hang out in a loop waiting for either an interrupt
	 * to come in or for the RTC to creep past the timeout period.
	 */
	ld	t1, 0(t0)		# Read the RTC
	dli	t3, EV_IP0		# Load the interrupt pending addr
1:					# DO
	sub	t1, t1, t2		#   rtc_value -= wait time
#if SABLE
	li	t1, 0
#endif
	bgez	t1, 2f			#   IF timer expired THEN jump forward 	
	ld	ta0, 0(t3)		#   Get Interrupt Pending mask
	nop				#
	and	ta0, (1 << BOOT_LEVEL)	#   Check to see if intr arrived 
	beq	ta0, zero, 1b		# LOOP UNTIL int or timer expired
	ld	t1, 0(t0)		# (BD) reload real-time clock

	/*
	 * If we get here, we received an interrupt and someone
	 * else has claimed the role of Boot Master.  Set the slave bit
	 * and off we go!
	 */
	GET_BSR(v0)
	ori	v0, BS_SLAVE
	SET_BSR(v0)

	j	prom_slave
	nop

2:
	/*
	 * If we get here, no one has assumed boot master responsibility,
	 * so add this processor to the Master Processor group, broadcast 
	 * a notification interrupt and return.
	 */
	dli	t0, EV_IGRMASK		# Load the Int Group Mask addr
	li	t1, IGR_MASTER		# Calculate the MASTER bit
	sd	t1, 0(t0)		# Add this processor to Master group

	dli	t0, EV_SENDINT		
	li	t1, EVINTR_VECTOR(BOOT_LEVEL, EVINTR_GLOBALDEST) 
	sd	t1, 0(t0)		# Send the interrupt to all procs
	nop

	GET_BSR(t0)
	li	t1, BS_SLAVE		# Turn off the slave bit
	not	t1
	and	t0, t1
	SET_BSR(t0)

	/*
	 * Wait for the SysCtrlr to talk to us, and then
	 * make it known that we are serving as the boot master.
	 */

	jal	ccuart_flush		# Get rid of any previous garbage
	nop
2:
	li	t1, SYSCTLR_TIMEOUT	# Number of times to wait
3:					# DO
	jal	ccuart_poll		#   Check to see if a char is available
	sub	t1, 1			#   (BD) Decrement the wait counter

	beqz	t1, 5f			#   If wait == 0 give up on SysCtlr
	nop				#   (BD)

	beqz	v0, 3b			# UNTIL a character arrives
	nop				# (BD)

	# A character arrived.  [[ We first check to see if the character
	# is destined for this processor.  If it is, ]] we switch on the 
	# character itself.
	# [[ According to John Kraft, this was done to get around a hardware
	# bug on IP19 board in the way the communication between system
	# controller and processor(s) is implemented.  This has been fixed (?)
	# on IP21 and the following code can be simplified.  I will simplify
	# it after getting confirmation from hardware folks.  I suppose the
	# system controller software will have to change too. ]] 
	#
	jal	ccuart_getc		# Read a character from the CC UART
	nop				# (BD)

	andi	t1, v0, 0x0f		# Put processor number into t1
	andi	v0, 0xf0		# Put mode (normal or manu) into v0 

	dli	t2, EV_SPNUM		# Read the SPNUM
	ld	t2, 0(t2)		
	nop
	andi	t2, EV_PROCNUM_MASK	# Mask out the processor number

	bne	t1, t2, 2b		# If not our processor, ignore
	ori	t0, zero, SC_BMREQ	# (BD) Load the BM request char.

	bne	t0, v0, 4f		# Jump forward if character isn't '@' 
	li	t0, SC_MANUMODE		# (BD) Set up for next comparison

	# We got an '@'. Do the right thing.
	#
	jal	ccuart_putc		# Write response character
	ori	a0, zero, SC_BMRESP	# (BD) stuff a 'K' into a0

	jal	delay			# Give the Sysctlr time to react 
	ori	a0, zero, 40000		# (BD)  40 ms

	j	prom_master
	nop

	# We didn't get an '@'. Check to see if it was a 'P'.
4:	#
	bne	t0, v0, 2b		# If not a 'P' go back to poll loop
	nop

	jal	ccuart_putc		# Send response to SysCtlr 
	ori	a0, zero, SC_BMRESP	# (BD) Stuff a 'K' into a0
	
	GET_BSR(t0)			# Set a bit indicating that we're
	or	t0, BS_MANUMODE		#    talking directly to a user
	SET_BSR(t0) 			#    rather than the SysCtlr

	jal	delay
	ori	a0, zero, 40000		# Give sysctlr time to react
	
5:	
	j	prom_master
	nop
END(arb_bootmaster)
