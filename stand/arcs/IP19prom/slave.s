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

#ident "$Revision: 1.38 $"

#include <regdef.h>
#include <asm.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evmp.h>
#include "ip19prom.h"
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

#define TPID_REG	s2

LEAF(prom_slave)
	/*
 	 * Make sure that our interrupt mask is clear
	 */
	jal	set_cc_leds
	ori	a0, zero, PLED_INSLAVE
	DPRINT("Switching into slave mode\r\n")

	jal	pod_clear_ints
	nop

	/*
	 * Put us in the slave processor group
	 */
	li	t0, EV_IGRMASK
	li	t1, IGR_SLAVE
	sd	t1, 0(t0)
	nop

	/*
	 * Tell the Boot master processor that this processor
	 * lives.
	 */
	DPRINT("\tSending interrupt to Master\r\n")
	li	t0, EV_SPNUM		
	ld	t0, 0(t0)		# Get the processor SPNUM
	nop
	and	t0, EV_SPNUM_MASK
	move	TPID_REG, t0		# Save the TPID
	sll	t0, EVINTR_LEVEL_SHFT+1	# Set the level (add one for aliveness) 
	or	t0, t0, PGRP_MASTER	# Set the destination 
	li	t1, EV_SENDINT
	sd	t0, 0(t1)		# Send interrupt to boot master

	.globl poll_loop
poll_loop:

	/*
	 * Main Interrupt polling loop
	 */
	DPRINT("\tEntering Interrupt polling loop...\r\n")

SlaveLoop:
	jal	change_led
	ori	a0, zero, 0x9

	li	t0, EV_IP0		# All requests are in the IP0 range
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

	li	t2, (1 << BIGEND_LEVEL)	# (BD)
	and	t3, t2, t1
	bne	t3, zero, slave_eb	# Make sure that we are big endian

	li	t2, (1 << LITTLEND_LEVEL) # (BD)
	and	t3, t2, t1
	bne	t3, zero, slave_el	# Make sure that we are little endian

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

	jal	ccuart_poll	# See if a character's waiting
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
	ori	a0, zero, 0x6			# Toggle the LEDS

	b	SlaveLoop
	nop				# LOOP again


	/* We branch here if the CPU's EAROM can't be corrected. */
bad_earom:
	j	ip19prom_flash_leds
	li	a0, FLED_BADEAROM	# (BD)

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
	 * The Boot Master broadcasts the required endianess.
	 * If our endianess is different from the required endianess,
	 * we rewrite our endianess and send a reset_request to the
	 * boo master.
	 */
slave_eb:
	DPRINT("Checking for big endianess\r\n")
	mfc0	t0, C0_CONFIG		# Grab the configuration level
	nop
	and	t0, CONFIG_BE		# Check our endianess
	bne	t0, zero, 1f		# IF we're not big-endian THEN
	nop
	DPRINT("\tSwitching to big endian\r\n")
	li	a0, EV_BE_LOC		#   Get addr of the EAROM byte for BE
	li	a1, EV_EAROM_BYTE0 | EAROM_BE_MASK
					#   Byte 0 in Big endian should have
					#    this calue.
	jal	write_earom		#   Write the EAROM
	nop

	li	a0, EV_BE_LOC		# Get the blasted address again.
	ld	t0, 0(a0)		# Get actual value.
	ori	t1, zero, EV_EAROM_BYTE0 | EAROM_BE_MASK	# (LD)
	
	bne	t0, t1, bad_earom
	nop				# (BD)

	li	t0, EV_SENDINT
	li	t1, SEND_MASTER(RESET_LEVEL)
	b	2f			# Jump out.
	sd	t1, 0(t0)		#   (BD) Send a reset request to master
1:					# ENDIF	
	li	a0, EV_BE_LOC
	ld	t0, 0(a0)		# Get EAROM byte 0
	li	a1, EV_EAROM_BYTE0 | EAROM_BE_MASK
	beq	t0, a1, 2f		# If they match, jump out!
	nop

	# If we get here, the EAROM is corrupt.
	jal	write_earom		# Write the EAROM.
	nop

	li	a0, EV_BE_LOC		# Get the blasted address again.
	ld	t0, 0(a0)		# Get actual value.
	ori	t1, zero, EV_EAROM_BYTE0 | EAROM_BE_MASK	# (LD)
	
	bne	t0, t1, bad_earom
	nop

	li	t0, EV_SENDINT
	li	t1, SEND_MASTER(FIXEAROM_LEVEL)
	sd	t1, 0(t0)		# Send a reset request to master

2:
	li	t0, BIGEND_LEVEL
	li	t1, EV_CIPL0
	b	SlaveLoop		# Go back to main slave loop
	sd	t0, 0(t1)		# (BD) Clear interrupt
	

slave_el:
	DPRINT("Checking for little endianess\r\n")
	mfc0	t0, C0_CONFIG		# Grab the configuration info
	nop
	and	t0, CONFIG_BE		# Check our endianess
	beq	t0, zero, 1f		# IF we're not little-endian THEN
	nop
	DPRINT("\tSwitching to little endian\r\n")
	li	a0, EV_BE_LOC		#   Get addr of EAROM byte with BE
	li	a1, EV_EAROM_BYTE0 & (~EAROM_BE_MASK)
	jal	write_earom		#   Write the EAROM
	nop
	li	a0, EV_BE_LOC		# Get the blasted address again.
	ld	t0, 0(a0)		# Get actual value.
	ori	t1, zero, EV_EAROM_BYTE0 & (~EAROM_BE_MASK)	# (LD)
	
	bne	t0, t1, bad_earom
	nop				# (BD)

	li	t0, EV_SENDINT
	li	t1, SEND_MASTER(RESET_LEVEL)
	b	2f			# Jump out.
	sd	t1, 0(t0)		#   (BD) Send a reset request to master
1:					# ENDIF

	li	a0, EV_BE_LOC
	ld	t0, 0(a0)		# Get EAROM byte 0
	li	a1, EV_EAROM_BYTE0 & (~EAROM_BE_MASK)
	beq	t0, a1, 2f		# If they match, jump out!
	nop

	# If we get here, the EAROM is corrupt.
	jal	write_earom		# Write the EAROM.
	nop

	li	a0, EV_BE_LOC		# Get the blasted address again.
	ld	t0, 0(a0)		# Get actual value.
	ori	t1, zero, EV_EAROM_BYTE0 & (~EAROM_BE_MASK)	# (LD)
	
	bne	t0, t1, bad_earom
	nop


	li	t0, EV_SENDINT
	li	t1, SEND_MASTER(FIXEAROM_LEVEL)
	sd	t1, 0(t0)		# Send a reset request to master

2:
	li	t0, LITTLEND_LEVEL
	li 	t1, EV_CIPL0
	b	SlaveLoop		# Go back to main slave loop
	sd	t0, 0(t1)		# (BD) Clear interrupt


	/*
	 * When we receive the STATUS interrupt we update the
	 * processor status field in the EVCFG data structure.
	 * Note that we should NOT call this routine if we aren't
	 * sure that everyone is running with the same endianess,
	 * since this potentially can fry the evcfginfo data.
	 */
slave_status:
	DPRINT("Writing out processor status ")
	jal	get_cpuinfo		# Get CPU info base addr
	nop
	move	t2, v0
	jal	ccuart_puthex		# Print cpuinfo addr
	move	a0, t2			# (BD)
	DPRINT("\r\n")

	move	v0, t2

		
	li	t0, STATUS_LEVEL	# Get level to be cleared
	li	t1, EV_CIPL0		# Get clear register address
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

	jal	pon_invalidate_dcache
	nop

	jal	pon_invalidate_scache
	nop

	jal	pon_invalidate_icache
	nop

	jal	get_cpuinfo
	nop

	lw	t1, CPULAUNCH_OFF(v0)	# Load launch address
	nop

	lw	t2, CPUPARM_OFF(v0)	# Load parameter address
	nop

	#
	# Set up the stack
	#
	li	sp, IP19PROM_STACK
	li	t0, EV_SPNUM
	ld	t0, 0(t0)
	sll	t0, 16
	add	sp, t0

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

  	# Flush caches
	# (Don't leave dirty stuff in here.  We want to see it in mem.)

	jal	pon_flush_dcache
	nop

	jal	pon_flush_scache
	nop
 
	li	t0, EV_ILE		# Disable R4k interrupts
	sd	zero, 0(t0)		# Clear the ILE vector

	# Get the evcpucfg block address

	jal	get_cpuinfo
	nop
	
	sw	s5, CPUPARM_OFF(v0)	# Save the returned value in
	nop				#   the EVCFGINFO parameter slot
	
	li	t2, PROM_SR
	mtc0	t2, C0_SR

	li	t0, LAUNCH_LEVEL
	li	t1, EV_CIPL0
	b	poll_loop		# Go back to main Slave loop
	sd	t0, 0(t1)


	/*
	 * Jump into POD mode
	 */
slave_pod:
	la	a0, slave_pod_mesg
	jal	pod_handler
	li	a1, EVDIAG_DEBUG		# (BD)
	
	/*
	 * Update the MPCONF block for this processor
	 */
	.globl mpconf_init
mpconf_init:
	jal	set_cc_leds
	li	a0, PLED_WRCONFIG	# (BD)

	DPRINT("Initializing this processor's MPCONF block\r\n")

	la	a0, prom_versnum	# Get address of prom rev num
	
	jal	get_char		# Get value.
	nop				# (BD)

	jal	get_cpuinfo		# Get our evconfig address
	move	s0, v0			# (BD) save value

	li	t1, EVDIAG_WRCPUINFO	# Tell the master we're writing.
	sb	t1, CPUDIAGVAL_OFF(v0)

	sb	s0, CPUPROMREV_OFF(v0)	# Record our prom revision

	li	t1, EV_CACHE_SZ_LOC	# Get the cache size info
	ld	t1, 0(t1)		# 
	nop				#
	nop				#
	sb	t1, CPUCACHE_OFF(v0)	# Write it into the evcfginfo struct

	li	t1, EV_EPROCRATE0_LOC	# Load the processor rate
	ld	t2, 0x18(t1)		# Get MSB
	ld	t3, 0x10(t1)		# Get next byte
	andi	t2, 0xff		# Mask off good data
	sll	t2, 8			# Shift up the total
	andi	t3, 0xff		# Mask off good data
	or	t2, t3			# Merge the two
	ld	t3, 0x8(t1)		# Get next byte
	sll	t2, 8			# Shift up the total
	andi	t3, 0xff		# Mask off good data
	or	t2, t3			# Merge
	ld	t3, 0(t1)		# Get LSB
	sll	t2, 8			# Shift up the total
	andi	t3, 0xff		# Mask off good data
	or	t2, t3			# Merge the two
	li	t3, 1000000		# Load a scaling factor (one million)
	divu	t2, t3			# Calculate in terms of megahertz
	nop
	mflo	t2
	sb	t2, CPUSPEED_OFF(v0)
	nop

	jal	get_mpconf		# Find this processor's MPCONF block 
	nop				# (BD)
	move	t0, v0

	DPRINT("Doing basic initialization\r\n")

        li      t1, EV_CPU_R4000        # Get cpu type value
        sb      t1, MP_PROCTYPE(t0)     # Store in MPCONF struct

	mfc0	t1, C0_PRID		# Get out implementation/revision num
	nop
	nop
	nop
	nop
	sw	t1, MP_PRID(t0)		# Save PRID value
        sb      zero, MP_ERRNO(t0)      # Clear errno field

        sw      zero, MP_LAUNCHOFF(t0)  # Clear launch vector
        sw      zero, MP_RNDVZOFF(t0)   # Clear the rendezvous field
        sw      zero, MP_BEVUTLB(t0)    # 
        sw      zero, MP_BEVNORMAL(t0)  #
        sw      zero, MP_BEVECC(t0)     #

	li	t2, EV_CKSUM0_LOC	# Get the EAROM checksum LSB address
	ld	t1, 0(t2)		# Fetch the byte
	li	t2, EV_CKSUM1_LOC	# Get the EAROM checksum MSB address
	ld	t3, 0(t2)		# Fetch the byte

	sb	t1, MP_STORED_CKSUM+1(t0)	# Store the LSB of the stored
						# EAROM checksum in MPCONF
	sb	t3, MP_STORED_CKSUM(t0)		# Store the MSB of the stored
						# EAROM checksum in MPCONF

	li	t1, EV_EAROM_BASE	# Start of EAROM
	li	t2, EV_CHKSUM_LEN	# Bytes to sum
	sll	t2, 3			# Multiply by 8. (Doubleword-aligned)
	add	t2, t1			# Stop before this address

	ld	t3, 0(t1)		# Initialize the sum.
	addi	t1, 8			# Add 8 to the address
	andi	t3, 0xff		# Mask off bottom byte.
	ori	t3, EAROM_BE_MASK	# Turn on the big-endian bit.

cksum_loop:
	ld	t4, 0(t1)		# Get the next byte
	addi	t1, 8			# (LD) Increment the address
	andi	t4, 0xff		# Mask off the bottom byte.
	bne	t1, t2, cksum_loop	# If we're not done, loop.
	addu	t3, t4			# (BD) Add in this byte

	andi	t4, t3, 0xff		# Mask off bottom byte
	sb	t4, MP_EAROM_CKSUM+1(t0)	# Store LSB
	srl	t3, 8
	andi	t4, t3, 0xff		# Mask off bottom byte
	sb	t4, MP_EAROM_CKSUM(t0)	# Store MSB

        li      t2, EV_CACHE_SZ_LOC     # Get address of secondary cache size
        ld      t1, 0(t2)               # Read secondary cache size from EAROM
        nop
        sw      t1, MP_SCACHESZ(t0)     # Write secondary cache size to MPCONF

	li	t2, EV_SPNUM		# Load the SPNUM address
	ld	t1, 0(t2)		# Read the SPNUM
	sb	t1, MP_PHYSID(t0)	# Write the physical ID

	DPRINT("More init\r\n")
	
	ld	t1, 0(t2)
        li      t1, MPCONF_MAGIC        # Get the magic number
        sw      t1, MP_MAGICOFF(t0)     # Store the magic number
       
	move 	t4, t0			# Save the MPCONF block address
	jal	get_cpuinfo		# Get the evcfg cpu block
	nop				# (BD)
	lb	t1, CPUVPID_OFF(v0)	# Get the virtual processor ID
	nop
	sb	t1, MP_VIRTID(t4)	# Write it to the MPCONF block
 
	# Check to see if we've abdicated.  If so, write the reason in DIAGVAL

	GET_BSR(t1)
	andi	t0, BS_ABDICATED
	bnez	t0, 6f
	nop				# (BD)

1:
	li	t0, EVCFGINFO_ADDR
	lw	t1, DEBUGSW_OFF(t0)	# Get debug switches

	li	t0, VDS_NO_DIAGS
	and	t0, t1			# Check NO_DIAGS switch

	bnez	t0, 6f			# If it's set jump over diags
	nop				#   to the cache initialization

	jal	set_cc_leds
	li	a0, PLED_CKPDCACHE1	# (BD)

/* test primary dcache first. */
	jal	pod_check_pdcache1
	nop				# (BD)

	beqz	v0, 1f
	nop

	jal     get_cpuinfo
	move	s0, v0			# (BD) Save return value (diagval)

	sb	s0, CPUDIAGVAL_OFF(v0)	# Save failure value.
        li      t0, MPCONF_INIT_LEVEL   # The interrupt that put us here.
        li      t1, EV_CIPL0            # 

	b	poll_loop
        sd      t0, 0(t1)               # (BD) Clear the interrupt
	
1:
	jal	get_cpuinfo
	li	s0, EVDIAG_TESTING_SCACHE	# (BD)

	sb	s0, CPUDIAGVAL_OFF(v0)	# Tell the master what we're doing.

	jal	set_cc_leds	
	li	a0, PLED_CKSCACHE1	# (BD)

	jal	pod_check_scache1	# Test scache as bits.
	nop				# (BD)
	
	beqz	v0, 1f
	nop				# (BD)

	jal	get_cpuinfo
	move	s0, v0			# Save return value (BD)
	
	sb	s0, CPUDIAGVAL_OFF(v0)	# Store the error code

        li      t0, MPCONF_INIT_LEVEL   # The interrupt that put us here.
        li      t1, EV_CIPL0            # 

	b	poll_loop
        sd      t0, 0(t1)               # (BD) Clear the interrupt
1:
	jal	set_cc_leds
	li	a0, PLED_INSLAVE	# (BD)

6:
	jal	get_cpuinfo
	li	s0, EVDIAG_INITING_CACHES	# (BD)

	sb	s0, CPUDIAGVAL_OFF(v0)	# Tell the master what we're doing.

	DPRINT("\tSetup secondary cache\r\n")
	jal	pod_setup_scache
	nop				# (BD)
	DPRINT("\tClear & invalidate primary icache\r\n")
	li	a0, POD_SCACHEADDR	# Address to use for zeroes
	jal	pon_zero_icache
	nop
	jal	pon_invalidate_icache
	nop
	DPRINT("\tInvalidate primary dcache\r\n")
	jal	pon_invalidate_dcache
	nop				# (BD)
	DPRINT("\tInvalidate secondary cache\r\n")
	jal	pon_invalidate_scache
	nop				# (BD)
	DPRINT("\tTesting and clearing bustags...\t")
	jal	pod_check_bustags
	nop
	beq	v0, zero, 1f 
	move	t9, v0			# (BD) Save bustag test results

	DPRINT("FAILED\r\n")

	jal	get_cpuinfo
	nop				# (BD)

	sb	t9, CPUDIAGVAL_OFF(v0)	# Store bad bus tags value

        li      t0, MPCONF_INIT_LEVEL   # The interrupt that put us here.
        li      t1, EV_CIPL0            # 

	b	poll_loop
        sd      t0, 0(t1)               # (BD) Clear the interrupt
1:
	nop

	DPRINT("Passed\r\n")
5:
	jal	get_cpuinfo
	nop

	GET_BSR(t1)
	andi	t0, BS_ABDICATED
	beqz	t0, 2f
	sb	zero, CPUDIAGVAL_OFF(v0)	# (BD) Tell the master we're ok.
	
	# We're a processor that has abdicated bootmastership
	# v0 still contains the CPU info struct address
	mfc1	t0, DIAGVAL_REG
	nop
	sb	t0, CPUDIAGVAL_OFF(v0)

2:
        li      t0, MPCONF_INIT_LEVEL   #
        li      t1, EV_CIPL0            #
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
	jal	pon_invalidate_dcache
	nop
	jal	pon_invalidate_scache
	nop
	jal	pon_invalidate_icache
	nop

	#	
	# Read MPCONF for function, parameter, and stack
	#	
	jal	get_mpconf		# Find this processor's MPCONF block 
	nop				# (SD)
	lw	t0, MP_MAGICOFF(v0)	# Make sure that MPCONF block is valid
	li	t1, MPCONF_MAGIC	# Load proper magic number
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
	lw	sp, MP_STACKADDR(v0)	# Load the stack
	nop

	beq	t0, zero, 2f		# Skip launch if address is zero
	nop

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
	lw	sp, MP_STACKADDR(v0)	# Reload the stack address
	nop

	beq	t0, zero, 3f		# Skip the rendezvous func if zero
	nop				# (BD)
	
	DPRINT("Calling MPCONF rendezvous routine.\r\n")

	jal	t0			# Call the rendezvous function
	move	a0, t1			# (BD)		
3:
	#
	# We need to flush the caches now to ensure that no data
	# becomes stale.  Actually, since we're running out of PROM
	# again there is a window in which coherency becomes broken.
	# We also reset the ILE and the coprocessor status register.
	#
	jal	pon_flush_dcache	#
	nop				# (BD)
	jal	pon_flush_scache	#
	nop				# (BD)
	li	t0, EV_ILE
	sd	zero, 0(t0)
	li	t1, PROM_SR
	mtc0	t1, C0_SR
	
	li	t0, MPCONF_LAUNCH_LEVEL
	li	t1, EV_CIPL0
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
	li      t0, EV_SPNUM	     	# Load the SPNUM addr 
	ld      t0, 0(t0)		# Read slot and proc
	li      t1, CPUINFO_SIZE	# Load the size of the CPUINFO struct
	and     t2, t0, EV_PROCNUM_MASK # Mask out the processor number
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
	li      t1, EVCFGINFO_ADDR      # Grab the base config address
	j	ra			# Return
	addu    v0, t1		    	# (BD) Add offsets to get final base
	END(get_cpuinfo) 

/*
 * get_mpconf
 *	Returns the address of this processor's MPCONF block in v0.
 */

LEAF(get_mpconf)
	move	t4, ra			# Save the return address	
	jal     get_cpuinfo		# Find this processor's CPU info
	nop				# (BD)

	lb	t1, CPUVPID_OFF(v0)	# Load this processor's VPID
	li	t0, MPCONF_SIZE		# Size of an MPCONF block

	dmultu	t1, t0
	nop
	li	t0, MPCONF_ADDR		# Get MPCONF base address
	mflo	v0                      # Retrieve the the MPCONF block offset

	j	t4			# Return
	addu	v0, t0                  # (BD) Add struct offset to base addr
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
	move	t4, a0
	move	s5, ra			# Save our return address
	li	t2, EV_IP0	
	li	t3, 25000 
	
1:					# LOOP	
	ld	t1, 0(t2)		#   Get the pending interrupt mask
	and	t1, INTR_LEVEL_MASK 
	bne	t1, zero, 2f		#   IF (interrupt) break
	nop

	bne	t3, zero, 1b		# WHILE t3 > 0
	sub	t3, 1			# (BD) t3-- 
2:
	jal	set_cc_leds
	move	a0, t4
	j	s5
	nop
	END(change_led)

	.data
slave_pod_mesg:
	.asciiz		"Slave processor entering POD mode.\r\n"
