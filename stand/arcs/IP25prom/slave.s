
/***********************************************************************\
*	File:		slave.s						*
*									*
*	This file contains the code which is executed by all of  	*
*	the slave processors in the system.  It contains the 		*
*	idle loop which the slave processors spin in awaiting 		*
*	interrupts, as well as the code which implements the other	*
*	in which slave processors can find themselves.			*
*									*
\***********************************************************************/

#ident "$Revision: 1.9 $"

#include <asm.h>
	
#include <sys/regdef.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/mips_addrspace.h>
	
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evdiag.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/everror.h>
	
#include "ip25prom.h"
#include "prom_intr.h"
#include "prom_leds.h"
#include "pod.h"
#include "pod_failure.h"
#include "ml.h"


	.text
	.set 	noreorder

/*
 * Routine prom_slave
 *	Spins waiting for interrupts from the Boot Master.
 *	If we get a REARB interrupt, we jump back to the
 * 	arbitration loop and try to become the boot master.
 * 	If we get a
 * Arguments:
 *	None.
 * Returns:
 *	Never returns.
 */

LEAF(prom_slave)
	/*
 	 * Make sure that our interrupt mask is clear
	 */
	LEDS(PLED_INSLAVE)
	DPRINT("\r\nSwitching into slave mode\r\n")

	jal	pod_clear_ints
	nop

	/*
	 * Put us in the slave processor group
	 */
	dli	t0, EV_IGRMASK
	dli	t1, IGR_SLAVE
	sd	t1, 0(t0)
	nop

	/*
	 * Tell the Boot master processor that this processor
	 * lives.
	 */
	dli	t0, EV_SPNUM		
	ld	t0, 0(t0)		# Get the processor SPNUM
	nop
	and	t0, EV_SPNUM_MASK
	sll	t0, EVINTR_LEVEL_SHFT+1	# Set the level (add one for aliveness) 
	or	t0, t0, PGRP_MASTER	# Set the destination 
	dli	t1, EV_SENDINT
	sd	t0, 0(t1)		# Send interrupt to boot master

	/*
	 * Main Interrupt polling loop
	 */
SlaveLoop:
	jal	change_led
	ori	a0, zero, 0x9

	dli	t0, EV_IP0		# All requests are in the IP0 range
	ld	t1, 0(t0)		# Get pending interrupt information

	li	t2, (1 << REARB_LEVEL)	
	and	t3, t2, t1	
	bne	t3, zero, slave_rearb	# IF (IP0 & (1 << REARB_LEVEL))
        nop

	li	t2, (1 << STATUS_LEVEL)	# (BD)
	and	t3, t2, t1
	bne	t3, zero, slave_status	# Initialize status fields in cfginfo 
        nop

	li	t2, (1 << LAUNCH_LEVEL)	# (BD) 
	and	t3, t2, t1
	bne	t3, zero, slave_launch	# Jump through launch vec if intr	
        nop

	li	t2, (1 << POD_LEVEL)	# (BD)
	and	t3, t2, t1
	bne	t3, zero, slave_pod	# Jump into POD mode
        nop

	li	t2, (1 << MPCONF_INIT_LEVEL)
	and	t3, t2, t1
	bne	t3, zero, mpconf_init
        nop
 
	li	t2, (1 << MPCONF_LAUNCH_LEVEL)
	and	t3, t2, t1
	bne	t3, zero, mpconf_launch 
	nop

	jal	ccuart_poll		# See if a character's waiting
	nop				# (BD)

	beqz	v0, 1f			# Was one?
	nop				# (BD)

	jal	ccuart_getc		# Get it
	nop				# (BD)

	li	a0, 16			# ^P
	beq	a0, v0, slave_pod	# Go into pod mode
	nop

1:
	jal	change_led	
	ori	a0, zero, 0x6		# (BD) Toggle the LEDS

	b	SlaveLoop
	nop				# LOOP again

	/*
	 * For some reason the Boot Master processor needs to
	 * resign its position, so we jump back to the boot
	 * master arbitration code.
	 */
slave_rearb:
	LEDS(PLED_BMARB)
	PMESSAGE("Rearbitrating for Boot Master...\r\n")
	
	jal	rearb_bootmaster	# Call subroutine to do actual arb
	nop				# (BD)
	beq	v0, zero, prom_slave	# Goto prom_slave if v0 == 0
	nop				# (BD)
	j	prom_master		# Otherwise goto prom_slave
	nop				# (BD)

	/*
	 * When we receive the STATUS interrupt we update the
	 * processor status field in the EVCFG data structure.
	 * Note that we should NOT call this routine if we aren't
	 * sure that everyone is running with the same endianess,
	 * since this potentially can fry the evcfginfo data.
	 */
slave_status:
	jal	get_cpuinfo		# Get CPU info base addr
	nop
	move	t2, v0
	jal	ccuart_puthex		# Print cpuinfo addr
	move	a0, t2			# (BD)
	PMESSAGE("\r\n")

	move	v0, t2

		
	dli	t0, STATUS_LEVEL	# Get level to be cleared
	dli	t1, EV_CIPL0		# Get clear register address
	b	SlaveLoop		# Return to the slave loop
	sd	t0, 0(t1)		# (BD) Clear the interrupt


	/*
	 * On receipt of the LAUNCH interrupt we
	 * examine this processor's segment of the
	 * MPCONF block and jump to the address specified
	 * in the XXX field.
	 */
slave_launch:
	PMESSAGE("Launching...\r\n")

	# Invalidate all caches
	# (Things we called before may have left stuff in here...)

	/* Margie added so we can run multiple niblet tests from pod prompt */
	jal flushTlb
	nop
        jal	invalidateCCtags
        nop
	jal	invalidateIDcache
	nop
	jal	invalidateScache
	nop

	jal	get_cpuinfo
	nop

	lw	t1, CPULAUNCH_OFF(v0)	# Load launch address
	nop

	#
	# Convert the address in t1 into a real 64-bit address.
	#	Use t0 and t2 as temp registers.
	#
	K32TOKPHYS(t1, t0, t2)

	lw	t2, CPUPARM_OFF(v0)	# Load parameter address
	nop

	#
	# Set up the stack
	#
	dli	sp, IP25PROM_STACK
        dli     t0, 0x9fffffff
	and	sp, t0
	or	sp, K0BASE
	dli	t0, EV_SPNUM
	ld	t0, 0(t0)
	andi	t0, EV_SPNUM_MASK
	dsll	t0, 16
	dadd	sp, t0
	
	#
	# Perform the actual jump to the subroutine
	#

	jal	t1			# Jump to the address
	move	a0, t2			# (BD) Load the parameter

	.globl slave_return
slave_return:
	#
	# We should get here on return
	#
	move	s5, v0			# Save the return value

	dli	t0, EV_ILE		# Disable interrupts
	sd	zero, 0(t0)		# Clear the ILE vector

	# Get the evcpucfg block address

	jal	get_cpuinfo
	nop

	sw	s5, CPUPARM_OFF(v0)	# Save the returned value in
	nop				#   the EVCFGINFO parameter slot
	
	dli	t2, PROM_SR
	MTC0(t2, C0_SR)

	dli	t0, LAUNCH_LEVEL
	dli	t1, EV_CIPL0
	sd	t0, 0(t1)
	b	SlaveLoop		# Go back to main Slave loop
        nop

	/*
	 * Jump into POD mode
	 */
slave_pod:
	MESSAGE(a1, "Slave processor entering POD mode.\r\n")
	jal	podMode
	li	a0, EVDIAG_DEBUG		# (BD)
	
	/*
	 * Update the MPCONF block for this processor
	 */
	.globl mpconf_init
mpconf_init:

	jal	get_cpuinfo		# Get out evconfig address
	nop
        move	s1,v0			# Save pointer

        /*
         * Check if we are required to run diags or not.
	 */
	dli	t0, EVCFGINFO_ADDR
	lw	t1, DEBUGSW_OFF(t0)	# Get debug switches

	li	t0, VDS_NO_DIAGS
	and	t0, t1			# Check NO_DIAGS switch

	bnez	t0, 1f			# If it's set jump over diags
	move	s2,zero			#   do cache init, 0 results

	/*
         * Test primary and secondary caches.
         */
        PMESSAGE("Testing primary D-cache\n\r")
	li	t1, EVDIAG_TESTING_DCACHE
	sb	t1, CPUDIAGVAL_OFF(s1)
        jal	testDcache
        nop
        bnez	v0,slaveDiagDone
	move	s2,v0
        PMESSAGE("Testing primary I-cache\n\r")
	li	t1, EVDIAG_TESTING_ICACHE
	sb	t1, CPUDIAGVAL_OFF(s1)
        jal	testIcache
        nop
        bnez	v0,slaveDiagDone
	move	s2,v0
        PMESSAGE("Testing secondary cache\n\r")
	li	t1, EVDIAG_TESTING_SCACHE
	sb	t1, CPUDIAGVAL_OFF(s1)
        jal	testScache
        nop
        bnez	v0,slaveDiagDone
	move	s2,v0

        PMESSAGE("Testing duplicate tags\n\r")
	jal	invalidateIcache
	nop
	jal	invalidateDcache
	nop
	jal	invalidateScache
	nop
	/*
	 * For REV "A" scc, copy test to cache, otherwise,
	 * run it from prom.
	 */
	GET_CC_REV(a0,a1)
	bne	a0,zero,doCCTestUncached
	nop
doCCTestCached:	
	dla	a0,testCCtags
	dla	a1,testCCtags_END
	jal	copyToICache
	dsubu	a1,a0
	b	doCCTest
	nop
doCCTestUncached:
	dla	v0,testCCtags
doCCTest:
	jal	v0
	nop
	
        beq	zero,v0,1f
	nop
	/*
	 * Must invalidateCC tags here, because if we put it in the
	 * slaveDiagDone routine, it may get called if the D-cache
	 * or I-cache failed. That would not be good.
	 */
	jal	invalidateCCtags
	move	s2,v0			/* Save is safe place */
	b	slaveDiagDone
	nop

1:
        /*
         * Invalidate all caches and proceed - this must occur even if no 
	 * diags are requested.
	 */
        PMESSAGE("Invalidating duplicate tags\r\n");
        jal	invalidateCCtags
        nop
        PMESSAGE("Invalidating primary I&D caches\r\n")
        li	t1,EVDIAG_INITING_CACHES
        sb	t1,CPUDIAGVAL_OFF(s1)
        jal	invalidateIDcache
        nop
        PMESSAGE("Invalidating secondary cache\r\n")
        li	t1,EVDIAG_INITING_SCACHE
        sb	t1,CPUDIAGVAL_OFF(s1)
	jal	invalidateScache
	nop
	PMESSAGE("Checking EROTIP\r\n");
	dli	a0,EV_ERTOIP
	ld	a1,0(a0)
	and	a1,0xffffffff
	beqz	a1,1f			/* All clear */
	nop
	dli	a1,0xffffffff
	dli	a2,EV_CERTOIP
	sd	a1,0(a2)
	/*
	 * Exercise all the bits
	 */
	dli	a0,EV_SCRATCH
	dli	a1,0xffffffffffffffff
	sd	a1,0(a0)
	dli	a1,0xaaaaaaaaaaaaaaaa
	sd	a1,0(a0)
	dli	a1,0x5555555555555555
	sd	a1,0(a0)
	sd	zero,0(a0)
	
	dli	a0,EV_ERTOIP
	ld	a1,0(a0)
	
	beqz	a1,1f			/* All clear now */
	move	s2,zero			/* diag value */
	/*
	 * Regardless of recoverability, we save away the ERTOIP
	 * for reporting. This over-rides the ertoip saved on
	 * reset - but thats ok since we are still having problems.
	 */
	DMTBR(a1, BR_ERTOIP)
	
	/*
	 * Check if the error bits set are recoverable. Basically,
	 * we can handle singe bit errors on the sysad. If we are
	 * in debug mode, make it a fatal error.
	 */
	
#       define	ECC	(IP25_CC_ERROR_SBE_SYSAD\
			| IP25_CC_ERROR_SBE_INTR \
			| IP25_CC_ERROR_ME)
	li	a0,~ECC
	and	a0,a1
	bnez	a0,slaveDiagDone
	ori	s2,zero,EVDIAG_ERTOIP
	
	dli	a0,EVCFGINFO_ADDR	/* Check debug switches. */
	lw	a0,DEBUGSW_OFF(a0)
	li	a1,VDS_DEBUG_PROM
	and	a0,a1
	beqz	a0,2f
	nop
	b	1f
	ori	s2,zero,EVDIAG_ERTOIP
2:	
	/*
	 * Bits set are recoverable, so disable reporting of them.
	 * Then, set the diag value and continue. Be sure to
	 * write CERTOIP register after disabling the correctable errors
	 * in case the act of writting them also causes and error.
	 */

	dli	a0,EV_ECCSB
	ld	a1,0(a0)
	ori	a1,EV_ECCSB_DSBECC
	sd	a1,0(a0)
	dli	a0,EV_CERTOIP		/* Clear the ones we can handle */
	li	a1,ECC
	sd	a1,0(a0)
	dli	a0,EV_ERTOIP		/* Be sure to stall long enough */
	ld	zero,0(a0)		
	li	s2,EVDIAG_ERTOIP_COR	/* Onward - with a diag code */
1:	
	# init processor's MPCONF block

	LEDS(PLED_WRCONFIG)
	PMESSAGE("Initializing this processor's MPCONF block\r\n")

	dla	a0, prom_versnum	# Get address of prom rev num
	jal	get_char		# Get value.
	nop				# (BD)
        sb	v0, CPUPROMREV_OFF(s1)	# Record our prom revision

	li	t1, EVDIAG_WRCPUINFO	# Tell the master we're writing.
	sb	t1, CPUDIAGVAL_OFF(s1)

        jal	initCPUSpeed
        nop
        sb	v0, CPUSPEED_OFF(s1)

	jal	get_mpconf		# Find this processor's MPCONF block 
	nop				# (BD)
	move	t0, v0

	DPRINT("Doing basic initialization\r\n")

        li	t1, EV_CPU_R10000	# Get cpu type value
        sb	t1, MP_PROCTYPE(t0)     # Store in MPCONF struct
	DMFC0(v0, C0_PRID)		# Get out implementation/revision num
	sw	v0, MP_PRID(t0)		# Save PRID value
        sb	zero, MP_ERRNO(t0)      # Clear errno field

        sw	zero, MP_LAUNCHOFF(t0)  # Clear launch vector
        sw	zero, MP_RNDVZOFF(t0)   # Clear the rendezvous field
					# Use t2 and t3 as temp registers
        sw	zero, MP_BEVUTLB(t0)    # 
        sw	zero, MP_BEVNORMAL(t0)  #

	EV_GET_SPNUM(t2, t1)            # t2 gets slot/t1 gets proc
	EV_GET_PROCREG(t2, t1, EV_CFG_CACHE_SZ, t1) # Get scache size
	andi	t1, 0x1f
	dsll	t1, 2			# Word (4-byte) sized entries
	dla	t2, log2vals		# Conert to log2 format
	daddu	t2, t1			# by reading from log2 table
	lw	t1, 0(t2)		# using register content as index
        sw	t1, MP_SCACHESZ(t0)     # Write secondary cache size to MPCONF
	sb	t1, CPUCACHE_OFF(s1)

	dli	t2, EV_SPNUM		# Load the SPNUM address
	ld	t1, 0(t2)		# Read the SPNUM
	andi	t1, EV_SPNUM_MASK
	sb	t1, MP_PHYSID(t0)	# Write the physical ID

	DPRINT("More init\r\n")

        dli	t1, MPCONF_MAGIC        # Get the magic number
        sw	t1, MP_MAGICOFF(t0)     # Store the magic number

	lb	t1, CPUVPID_OFF(s1)	# Get the virtual processor ID
	sb	t1, MP_VIRTID(t0)	# Write it to the MPCONF block

 	DMFBR(t1, BR_ERTOIP)		/* Reset/Boot ERTOIP value */
	sw	t1, MP_ERTOIP(t0)
 
	/* if we've abdicated write the reason in DIAGVAL */

	DMFBR(t1, BR_BSR)
	andi	t1, BSR_ABDICATE
	beqz	t1, 1f
	nop				# (BD)

        DMFBR(s2, BR_DIAG)
1:

slaveDiagDone:
	/*
         * Diag failed, error code in s2 - store it into diag result, 
         * and go back to polling.
         */
        sb	s2,CPUDIAGVAL_OFF(s1)
        li 	t0, MPCONF_INIT_LEVEL
        dli	t1, EV_CIPL0 
        sd	t0, 0(t1)		/* Clear pending interrupt */
	jal	invalidateScache
	nop
	jal	invalidateIcache
	nop
	jal	invalidateDcache
	nop
	
        DPRINT("Return to slave loop ...\r\n");
        b	SlaveLoop
        nop
         
	/*
 	 * Launch using the MPCONF block
	 */
mpconf_launch:
	# Set the LED to the launch value
	#
	LEDS(PLED_PROMJUMP)

	DPRINT("Calling MPCONF launch...\r\n")

	# Invalidate the caches
        jal	invalidateCCtags
        nop
        jal	invalidateScache
        nop
	jal	invalidateIDcache
	nop

	#	
	# Read MPCONF for function, parameter, and stack
	#	
	jal	get_mpconf		# Find this processor's MPCONF block 
	nop				# (SD)
	lw	t0, MP_MAGICOFF(v0)	# Make sure that MPCONF block is valid
	lui	t1, MPCONF_MAGIC >> 16	# Load proper magic number
        ori	t1, MPCONF_MAGIC & 0xffff
	beq	t0, t1, 1f		# IF magic number is bogus THEN
	nop				# (bd)
	DPRINT("Invalid MPCONF block\r\n")
	b	3f
	nop				# ENDIF
1:	
	lw	t0, MP_LAUNCHOFF(v0)	# Load the address of the launch func
	nop

	lw	t1, MP_LPARM(v0)	# Load the parameter
	nop
	ld	sp, MP_REAL_SP(v0)	# Load the stack
	nop

	beq	t0, zero, 2f		# Skip launch if address is zero
	nop

	#
	# Convert the address in t0 into a real 64-bit address.
	# 	Use t1 and a0 as temp regs.
	#
	K32TOKPHYS(t0, t1, a0)

	jal	t0			# Call the subroutine
	move	a0, t1

	#
	# Reload the MPCONF, since values may have been trashed
	#
	jal	get_mpconf		# Reload MPCONF addr
	nop
2:
	lw	t0, MP_RNDVZOFF(v0)	# Load the rendezvous address
	nop

	lw	t1, MP_RPARM(v0)	# Load the rendezvous parameter
	nop
	ld	sp, MP_REAL_SP(v0)	# Reload the stack address
	nop

	beq	t0, zero, 3f		# Skip the rendezvous func if zero
	nop				# (BD)
	
	DPRINT("Calling MPCONF rendezvous routine.\r\n")

	#
	# Convert the rendezvous function pointer into a real 64-bit address
	# 	Use t2 and t3 as temp registers.
	#
	K32TOKPHYS(t0, t2, t3)

	jal	t0			# Call the rendezvous function
	move	a0, t1			# (BD)		
3:
	dli	t0, EV_ILE
	sd	zero, 0(t0)
	dli	t1, PROM_SR
	MTC0(t1, C0_SR)

	li	t0, MPCONF_LAUNCH_LEVEL
	dli	t1, EV_CIPL0
	b	SlaveLoop
	sd	t0, 0(t1)

	END(prom_slave)	


/* 
 * get_cpuinfo
 *	Returns the address of the evcfginfo cpu information structre
 *	in v0.
 *	Uses registers t0, t1, t2, v0.
 */
LEAF(get_cpuinfo)
	dli	t0, EV_SPNUM	     	# Load the SPNUM addr 
	ld      t0, 0(t0)		# Read slot and proc
	li      t1, CPUINFO_SIZE	# Load the size of the CPUINFO struct
	and     t2, t0, EV_PROCNUM_MASK # Mask out the processor number
	srl	t2, EV_PROCNUM_SHFT	# Grab the slice number
	multu   t2, t1		    	# offset1 = proc# * CPU_INFO size
	nop				#
	mflo    t2			# Result of multiply
	and     t1, t0, EV_SLOTNUM_MASK #
	srl     t1, EV_SLOTNUM_SHFT     # Grab the slot number
	li      t0, BRDINFO_SIZE	# Load the BRDINFO size
	multu   t1, t0		    	# offset2 = slot# * BRDINFO_SIZE
	nop				#
	mflo    v0			# offset2 = Result of multiply
	addu    v0, t2		    	# Add offsets
	dli	t1, EVCFGINFO_ADDR      # Grab the base config address
	j	ra			# Return
	daddu	v0, t1		    	# (BD) Add offsets to get final base
	END(get_cpuinfo) 

/*
 * get_mpconf
 *	Returns the address of this processor's MPCONF block in v0.
 */

LEAF(get_mpconf)
	move	ta0, ra			# Save the return address	
	jal     get_cpuinfo		# Find this processor's CPU info
	nop				# (BD)

	lb	t1, CPUVPID_OFF(v0)	# Load this processor's VPID
	li	t0, MPCONF_SIZE		# Size of an MPCONF block

	dmultu	t1, t0
	nop
	dli	t0, MPCONF_ADDR		# Get MPCONF base address
	mflo	v0                      # Retrieve the the MPCONF block offset

	j	ta0			# Return
	daddu	v0, t0                  # (BD) Add struct offset to base addr
	END(get_mpconf)

/*
 * Routine change_leds
 *	Delays for a period of time and then changes the LEDs
 *	If an interrupt comes in while we are delaying, break  
 *	out of the delay loop and set the value immediately.
 * Parameters:
 *	a0 -- value to set the leds to.
 */
	
LEAF(change_led)
	move	ta0, a0
	move	s5, ra			# Save our return address
	dli	t2, EV_IP0	
	li	t3, 140000 
1:					# LOOP	
	ld	t1, 0(t2)		#   Get the pending interrupt mask
	and	t1, INTR_LEVEL_MASK 
	bne	t1, zero, 2f		#   IF (interrupt) break
	nop

	sub	t3, 1			# (BD) t3-- 
	bne	t3, zero, 1b		# WHILE t3 > 0
	nop
2:
	jal	set_cc_leds
	move	a0, ta0
	j	s5
	nop
	END(change_led)
