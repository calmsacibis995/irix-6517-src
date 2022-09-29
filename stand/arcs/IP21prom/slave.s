
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

#ident "$Revision: 1.91 $"

#define DEBUG 1

#include "ml.h"
#include <sys/regdef.h>
#include <asm.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evdiag.h>
#include <sys/EVEREST/evmp.h>
#include "ip21prom.h"
#include "prom_intr.h"
#include "prom_leds.h"
#include "pod.h"
#include "pod_failure.h"



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
	 * We marked the master's stack as dirty exclusive
	 * during our initialization (because we didn't know
	 * we are the slave), so init it again as invalid now.
	 */

	jal	pon_invalidate_IDcaches
	nop
	jal	gcache_invalidate
	nop

EXPORT(prom_slave_nmi)
	/*
 	 * Make sure that our interrupt mask is clear
	 */
	jal	set_cc_leds
	ori	a0, zero, PLED_INSLAVE
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

	.globl poll_loop
poll_loop:

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

	li	t2, (1 << STATUS_LEVEL)	# (BD)
	and	t3, t2, t1
	bne	t3, zero, slave_status	# Initialize status fields in cfginfo 

	li	t2, (1 << LAUNCH_LEVEL)	# (BD) 
	and	t3, t2, t1
	bne	t3, zero, slave_launch	# Jump through launch vec if intr	

	li	t2, (1 << POD_LEVEL)	# (BD)
	and	t3, t2, t1
	bne	t3, zero, slave_pod	# Jump into POD mode

	li	t2, (1 << MPCONF_INIT_LEVEL)
	and	t3, t2, t1
	bne	t3, zero, mpconf_init

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
	jal	set_cc_leds
	ori	a0, zero, PLED_BMARB
	DPRINT("Rearbitrating for Boot Master...\r\n")
	
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
	DPRINT("\r\n")

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
	DPRINT("Launching...\r\n")

	# Invalidate all caches
	# (Things we called before may have left stuff in here...)

	/* Margie added so we can run multiple niblet tests from pod prompt */
	jal	flush_entire_tlb
	nop

	jal	pon_invalidate_IDcaches
	nop

	jal	gcache_invalidate
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
	dli	sp, IP21PROM_STACK
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

	dli	t0, EV_ILE		# Disable TFP interrupts
	sd	zero, 0(t0)		# Clear the ILE vector

	# Get the evcpucfg block address

	jal	get_cpuinfo
	nop

	sw	s5, CPUPARM_OFF(v0)	# Save the returned value in
	nop				#   the EVCFGINFO parameter slot
	
	dli	t2, PROM_SR
	DMTC0(t2, C0_SR)

	dli	t0, LAUNCH_LEVEL
	dli	t1, EV_CIPL0
	b	poll_loop		# Go back to main Slave loop
	sd	t0, 0(t1)


	/*
	 * Jump into POD mode
	 */
slave_pod:
	dla	a0, slave_pod_mesg
	jal	pod_handler
	li	a1, EVDIAG_DEBUG		# (BD)
	
	/*
	 * Update the MPCONF block for this processor
	 */
	.globl mpconf_init
mpconf_init:
	jal	set_cc_leds
	li	a0, PLED_INV_IDCACHES 

	jal	get_cpuinfo		# Get out evconfig address
	nop
	li	t1, EVDIAG_INITING_CACHES # Tell the master we're init
					  # icache and dcache.
	sb	t1, CPUDIAGVAL_OFF(v0)


	# Invalidate all caches

	jal	pon_invalidate_IDcaches
	nop
        beqz    v0, 1f
	nop

	# if invalidate I & D caches error occurs, report to Master
	# and store the diag code.

	move	t3, v0
	jal	get_cpuinfo
	nop
	sb	t3, CPUDIAGVAL_OFF(v0)
	b 	10f	
	nop
1:
	jal	set_cc_leds
	li	a0, PLED_INV_SCACHE 

	jal	get_cpuinfo		# Get out evconfig address
	nop
	dli	t1, EVDIAG_INITING_SCACHE  # Tell the master we're init
					  # gcache
	sb	t1, CPUDIAGVAL_OFF(v0)

	# invalidate gcache
	jal	gcache_invalidate
	nop
        beqz    v0, 1f
	nop

        # if invalidate scaches error occurs, report to Master
        # and store the diag code.

        move    t3, v0
        jal     get_cpuinfo
        nop
        sb      t3, CPUDIAGVAL_OFF(v0)
	b	10f
	nop
1:
	# init processor's MPCONF block

	jal	set_cc_leds
	li	a0, PLED_WRCONFIG	# (BD)
	DPRINT("Initializing this processor's MPCONF block\r\n")

	dla	a0, prom_versnum	# Get address of prom rev num
	jal	get_char		# Get value.
	nop				# (BD)

	jal	get_cpuinfo		# Get our evconfig address
	move	s0, v0			# (BD) save value

	li	t1, EVDIAG_WRCPUINFO	# Tell the master we're writing.
	sb	t1, CPUDIAGVAL_OFF(v0)

	sb	s0, CPUPROMREV_OFF(v0)	# Record our prom revision

	EV_GET_SPNUM(t0, t1)            # t0 gets slot
                                        # t1 gets proc. number
	EV_GET_PROCREG(t0, t1, EV_CACHE_SZ, t0) # Get scache size
	andi	t0, 0x1f
	dla	t2, log2vals		# Conert to log2 format
	daddu	t2, t0			# by reading from log2 table
	lb	t1, 0(t2)		# using register content as index
	sb	t1, CPUCACHE_OFF(v0)	# Write it into the evcfginfo struct
	/*
	DMFC1(t2,EV_PROCRATE_REG)	# Get the processor rate
	li	t3, 1000000		# Load a scaling factor (one million)
	divu	t2, t3			# Calculate in terms of megahertz
	nop
	mflo	t2
	*/
        #hard code for 75 MHZ for now
	dli	t2, CPU_SPEED
	sb	t2, CPUSPEED_OFF(v0)
	nop

	jal	get_mpconf		# Find this processor's MPCONF block 
	nop				# (BD)
	move	t0, v0

	DPRINT("Doing basic initialization\r\n")

        li	t1, EV_CPU_TFP		# Get cpu type value
        sb	t1, MP_PROCTYPE(t0)     # Store in MPCONF struct
	DMFC0(v0, C0_PRID)		# Get out implementation/revision num
        /* TFP revision info may be encoded in the IC field of the CONFIG
         * register, so check for this case and return modified value.
         */
        andi    t1, v0, 0xff
        bne     t1, zero, 9f    # if revision non-zero, go report it
        nop

        /* Revision field is zero, use bits in IC field of CONFIG register
         * to correctly determine revision.
         */

        DMFC0(t2,C0_CONFIG)
        and     t2, CONFIG_IC
        dsrl    t2, CONFIG_IC_SHFT      # IC field now isolated
        lb      t3, revtab(t2)          # load minor revision
        daddu   v0, t3                  # add into implementation
9:
	sw	v0, MP_PRID(t0)		# Save PRID value
        sb	zero, MP_ERRNO(t0)      # Clear errno field

        sw	zero, MP_LAUNCHOFF(t0)  # Clear launch vector
        sw	zero, MP_RNDVZOFF(t0)   # Clear the rendezvous field
					# Use t2 and t3 as temp registers
        sw	zero, MP_BEVUTLB(t0)    # 
        sw	zero, MP_BEVNORMAL(t0)  #

	DMFC1(t1, EV_ERTOIP_REG)	# Get register contents
	sw	t1, MP_BEVECC(t0)	# Store ertoip contents at reset

	EV_GET_SPNUM(t2, t1)            # t2 gets slot
                                        # t1 gets proc. number
	EV_GET_PROCREG(t2, t1, EV_CACHE_SZ, t1) # Get scache size
	andi	t1, 0x1f
	dla	t2, log2vals		# Conert to log2 format
	daddu	t2, t1			# by reading from log2 table
	lb	t1, 0(t2)		# using register content as index
        sw	t1, MP_SCACHESZ(t0)     # Write secondary cache size to MPCONF

	dli	t2, EV_SPNUM		# Load the SPNUM address
	ld	t1, 0(t2)		# Read the SPNUM
	andi	t1, EV_SPNUM_MASK
	sb	t1, MP_PHYSID(t0)	# Write the physical ID

	DPRINT("More init\r\n")

        dli	t1, MPCONF_MAGIC        # Get the magic number
        sw	t1, MP_MAGICOFF(t0)     # Store the magic number

	move 	ta0, t0			# Save the MPCONF block address
	jal	get_cpuinfo		# Get the evcfg cpu block
	nop				# (BD)
	lb	t1, CPUVPID_OFF(v0)	# Get the virtual processor ID
	nop
	sb	t1, MP_VIRTID(ta0)	# Write it to the MPCONF block
 

	# Check to see if we've abdicated.  If so, write the reason in DIAGVAL

	GET_BSR(t1)
	andi	t0, BS_ABDICATED
	bnez	t0, 6f
	nop				# (BD)

1:
	dli	t0, EVCFGINFO_ADDR
	lw	t1, DEBUGSW_OFF(t0)	# Get debug switches

	li	t0, VDS_NO_DIAGS
	and	t0, t1			# Check NO_DIAGS switch

	bnez	t0, 6f			# If it's set jump over diags
	nop				#   to the cache initialization

        # proceed for scache testing

        jal     set_cc_leds
        dli     a0, PLED_CKSCACHE1

        jal     get_cpuinfo             # Get out evconfig address
        nop
        li      t1, EVDIAG_TESTING_SCACHE # Tell the master we're testing
                                          # gcache
        sb      t1, CPUDIAGVAL_OFF(v0)

        # Perform gcache testing

        jal     gcache_check
        nop
	
        beqz    v0, 1f
        nop

        # if gcache test error, report to Master and store the diag code

        move    t3, v0
        jal     get_cpuinfo
        nop
        sb      t3, CPUDIAGVAL_OFF(v0)  # store the diag code
	b	10f
	nop
1:
	# proceed for dcache test

        jal     set_cc_leds
        li      a0, PLED_CKPDCACHE1
        jal     get_cpuinfo
	nop 
        li      t1, EVDIAG_TESTING_DCACHE
        sb      t1, CPUDIAGVAL_OFF(v0)  # Tell the master what we're doing.

        # Perform dcache testing

        jal     pon_invalidate_IDcaches
        nop
    	jal 	dcache_tag
    	nop
	beqz	v0, 1f
	nop

	# if dcache test error, report to Master and store the diag code

        move    t3, v0
        jal     get_cpuinfo
        nop
        sb      t3, CPUDIAGVAL_OFF(v0)  # store the diag code
	b	10f
	nop
1:
	jal	set_cc_leds
	li	a0, PLED_INSLAVE	# (BD)

6:
	jal	pon_invalidate_IDcaches
	nop				# (BD)

	# proceed cc join test

        jal     set_cc_leds
        li      a0, PLED_CCJOIN 

	jal	pod_check_ccjoin
	nop
	beqz	v0, 1f
	nop
	# if cc join test error, report to Master and store the diag code

	move	t3, v0
	jal	get_cpuinfo
	nop
	sb	t3, CPUDIAGVAL_OFF(v0)
	b	 10f
	nop
1:
	# proceed cc write gatherer test
        jal     set_cc_leds
        li      a0, PLED_WG 

        jal     pod_init_wg
        nop
	beqz	v0, 10f
	nop

	# if cc write gatherer test error, report to Master and store the diag code
	move	t3, v0
	jal	get_cpuinfo
	nop
	sb	t3, CPUDIAGVAL_OFF(v0)
	b 	11f
	nop

10:
        /* Force set 3 OFF and Writeback Disable OFF */
        dli     t0, BB_STARTUP_CTRL
        li      t1, 0           # WriteBackInhibit
        sd      t1, 0(t0)       # clear both ForceSet3 and WriteBackInhibit
        dli     t1, BB_SET_ALLOW
        li      a0, 0x0f
        sd      a0, 0(t1)       # set all 4 SetAllow bits to 1

	
        jal     get_cpuinfo
        nop
        GET_BSR(t1)
        andi    t0, BS_ABDICATED
        beqz    t0, 2f
        sb      zero, CPUDIAGVAL_OFF(v0)     # (BD) Tell the master we're ok.

	# We're a processor that has abdicated bootmastership
	# v0 still contains the CPU info struct address
	DMFC1(t0, DIAGVAL_REG)
	sb	t0, CPUDIAGVAL_OFF(v0)
11:
	sb	t3, CPUDIAGVAL_OFF(v0)
        /* Force set 3 OFF and Writeback Disable OFF */
        dli     t0, BB_STARTUP_CTRL
        li      t1, 0           # WriteBackInhibit
        sd      t1, 0(t0)       # clear both ForceSet3 and WriteBackInhibit
        dli     t1, BB_SET_ALLOW
        li      a0, 0x0f
        sd      a0, 0(t1)       # set all 4 SetAllow bits to 1

        jal     get_cpuinfo
        nop                             # (BD)
        li      t0, MPCONF_INIT_LEVEL   # The interrupt that put us here.
        dli     t1, EV_CIPL0            #
	sd	t0, 0(t1)

2:
	DPRINT("Return to slave loop....\r\n")
        li      t0, MPCONF_INIT_LEVEL   #
        dli	t1, EV_CIPL0            #
        b       SlaveLoop               # Return to the slave loop
        sd      t0, 0(t1)               # (BD) Clear the interrupt

	/*
 	 * Launch using the MPCONF block
	 */
mpconf_launch:
	# Set the LED to the launch value
	#
	jal	set_cc_leds
	ori	a0, zero, PLED_PROMJUMP

	DPRINT("Calling MPCONF launch...\r\n")

	# Invalidate the caches
	jal	pon_invalidate_IDcaches
	nop

	#	
	# Read MPCONF for function, parameter, and stack
	#	
	jal	get_mpconf		# Find this processor's MPCONF block 
	nop				# (SD)
	lwu	t0, MP_MAGICOFF(v0)	# Make sure that MPCONF block is valid
	dli	t1, MPCONF_MAGIC	# Load proper magic number
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
	DMTC0(t1, C0_SR)

	li	t0, MPCONF_LAUNCH_LEVEL
	dli	t1, EV_CIPL0
	b	SlaveLoop
	sd	t0, 0(t1)

        .rdata
        /* Sequence of minor revisions from IC field of CONFIG register:
	 * 	IC field:  0 1 2 3 4 5 6 7
	 *  	major rev  0 0 0 1 2 0 2 1
	 *      minor rev  0 0 0 1 2 0 1 2
	 */
revtab:
	.word	0x00000011
	.word	0x22002112
        .text
 
	.data
mpconf_addr:
	.asciiz		"Slave process evcfginfo_addr at "
	.text
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
 * set_ertoip_at_reset
 *	Copies contents of ertoip register at reset time into memory
 *	location so it can be displayed by the master.
 *	Ugly kludge: since IP21 doesn't have an ecc handler, we use that
 *	location in the mpconf table to save the ertoip info.
 */
LEAF(set_ertoip_at_reset)
	move	ta1, ra			# Save the return address
	jal	get_mpconf		# Find the processor's MPCONF address
	nop				# (BD)

	DMFC1(t0, EV_ERTOIP_REG)	# Get register contents
	sw	t0, MP_BEVECC(v0)	# Store results
	j	ta1			# Return
	nop				# (BD)
	END(set_ertoip_at_reset) 

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

	bne	t3, zero, 1b		# WHILE t3 > 0
	sub	t3, 1			# (BD) t3-- 
2:
	jal	set_cc_leds
	move	a0, ta0
	j	s5
	nop
	END(change_led)

	.data
slave_pod_mesg:
	.asciiz		"Slave processor entering POD mode.\r\n"
check_mesg:
	.asciiz		"checking....\r\n"
