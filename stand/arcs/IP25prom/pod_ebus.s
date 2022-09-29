/* 
 * pod_ebus.s -
 *	POD EBUS tests.
 */

#include <asm.h>

#include <sys/regdef.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/sbd.h>
#include <sys/mips_addrspace.h>

#include <sys/EVEREST/IP25.h>

#include "ip25prom.h"
#include "prom_intr.h"
#include "pod_failure.h"

#define EV_CELMAX	127

	.text
	.set	noreorder

/**************************************************************************
 * pod_check_ebus1 is called by all processors.  It will run more or less
 * concurrently but not in lock step so we must keep in mind that parts
 * of the test may run on one CPU while another one runs different parts.
 **************************************************************************/
LEAF(pod_check_ebus1)
	move	v1, ra			# Save return address

	MFC0(a0, C0_SR)
	dli	a1, ~SR_IE
	and	a0, a1
	MTC0(a0, C0_SR)		# Make sure the IE bit is off.

	dli	a0, EV_CEL
	dli	a1, EV_CELMAX		# Disable interrupts on the
					# IP21 level too.
	sd	a1, 0(a0)

	dli	a0, EV_ILE
	dli	a1, 1
	sd	a1, 0(a0)		# Enable level 0 Everest interrupts

	dli	a3, EV_HPIL
	ld	a1, 0(a3)
#ifndef SABLE
	bne	a1, zero, stuck_hipl
	nop
#endif

	jal	pod_clear_ints
	sd	zero, (a0)		# Remove us from all groups

	# At this point, all interrupts are clear.

	dli	a0, EV_SPNUM
	ld	t0, 0(a0)		# t0 = SPNUM
	li	a0, EV_CELMAX		# (LD)
	andi	t0, EV_SPNUM_MASK	# Mask off appropriate bits

	li	a1, 0
	dli	a2, EV_SENDINT
	# a3 still contains EV_HPIL
check_levels:
	sll	t1, a1, 8	# Put interrupt level in bits 14..8
	or	t1, t0		# Put SPNUM in bits 5..0
	sd	t1, 0(a2)	# Send ourselves an interrupt
	nop

	srl	t1, t0, 2	# Get our slot number
	EV_GET_CONFIG(t1, EV_A_LEVEL, t1)	# Get a config reg to stall

	ld	t2, 0(a3)	# Get HPIL and compare.
	nop
#ifndef SABLE
	bne	a1, t2, lost_int
	nop
	addi	a1, 1
	ble	a1, a0, check_levels
	nop
#endif

	jal	pod_clear_ints	# Clear all the interrupts again.
	nop

	dli	a0, EV_IGRMASK
	li	a1, 1 << 15
	sd	a1, 0(a0)	# Add us to group 15.
	li	t2, 15 | 64	# Group 15 (+ 64 = group interrupt)

	dli	a2, EV_SENDINT
	li	t1, EV_CELMAX << 8	# Level 127 interrupt
	or	t2, t1			# Sent to interrupt group 15
	sd	t2, 0(a2)
	nop

	srl	t1, t0, 2
	EV_GET_CONFIG(t1, EV_A_LEVEL, t1)	# Get a config reg to stall

	ld	t1, 0(a3)		# Get HPIL
	li	t2, EV_CELMAX		# t2 = 127

#ifndef SABLE
	bne	t1, t2, group_fail	# if HPIL isn't 127, fail.
	nop
#endif

	jal	pod_clear_ints		# Clear 'em all out again.
	sd	zero, 0(a0)		# Remove us from group 15
					# (It's important to do this now
					#  as some processors may lag behind
					#  us and try to send the group
					#  interrupt later.
	
	# Enable Everest interrupt level 0
	dli	t1, EV_CEL
	sd	zero, 0(t1)

	li	t0, 1
	dli	t1, EV_ILE
	sd	t0, 0(t1)

	sd	zero, 0(a2)		# Send ourselves a lvl 0, #0 interrupt

	dli	t2, EV_SPNUM
	ld	t0, 0(t2)		# t2 = SPNUM
	nop
	andi	t2, t0, EV_SPNUM_MASK	# Mask off bottom 6 bits
	ori	t2, 1 << 8		# Interrupt #1

	dli	t1, EV_SENDINT
	sd	t2, 0(t1)		# Send the interrupt

	dli	t1, EV_IP0
	ld	t1, 0(t1)		# Read reg to force completion of intr
	nop

	MFC0(t0, C0_CAUSE)

	andi	t0, CAUSE_IP3		# Check for pending interrupts.

#ifndef SABLE
	beq	t0, zero, proc_int_fail
	nop
#endif

	jal	pod_clear_ints	# Clear all the interrupts one last time.
	nop
	dli	t1, EV_ILE
	sd	zero, 0(t1)		# Turn level zero interrupts back off
	
	# Darn!  Can't test "all CPUs" interrupt here because someone may
	# 	still be doing the clear interrupts or each-level tests.
	# Guess this is it for ebus1.

	j	v1	
	li	v0, 0		# Return 0 == happy CPU
END(pod_check_ebus1)


LEAF(stuck_hipl)	
	j	v1
	li	v0, EVDIAG_STUCK_HPIL		# (BD)

	END(stuck_hipl)

LEAF(lost_int)
	j	v1
	ori	v0, zero, EVDIAG_LOST_INT		# (BD)

	END(lost_int)

LEAF(group_fail)
	j	ra
	li	v0, 3

	jal	clear_ints
	nop
	j	v1
	ori	v0, zero, EVDIAG_GROUP_FAIL	# (BD)
	END(group_fail)

LEAF(proc_int_fail)
	j	v1
	ori	v0, zero, EVDIAG_R4K_INTS		# (BD)
	END(proc_int_fail)

LEAF(pod_clear_ints)
	dli	a0, EV_CIPL0
	li	a1, EV_CELMAX		# Highest interrupt number
clear_ints:
	sd	a1, 0(a0)		# Clear interrupt (BD)
	bne	a1, zero, clear_ints
	addi	a1, -1			# Decrement counter (BD)

	j	ra
	nop
	
	END(pod_clear_ints)
