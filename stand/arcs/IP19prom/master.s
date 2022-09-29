/***********************************************************************\
*	File:		master.s					*
*									*
*	This file contains the code which is executed exclusively	*
*	by the Boot Master processor.  It completes the power-on	*
*	boot sequence by configuring the IO4 NVRAM and serial port,	*
*	running additional diagnostics, configuring memory, and 	*
*	downloading the IO4 prom.					*
*									*
\***********************************************************************/

#ident "$Revision: 1.21 $"

#include <regdef.h>
#include <asm.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include "ip19prom.h"
#include "prom_leds.h"
#include <sys/EVEREST/evintr.h>
#include "prom_intr.h"
#include "pod.h"
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/sysctlr.h>

	.text
	.set	noreorder
	.set	noat

/*
 * Routine prom_master
 *	Executed by the Boot Master processor.
 *	Performs rest of power-on startup.
 *
 * Arguments:
 *	None.
 * Returns:
 * 	Never returns.	
 */

LEAF(prom_master)

	/*
 	 * Announce our presence as the boot master
	 */
	jal	set_cc_leds		#
	ori	a0, zero, PLED_BMASTER	# (BD)
	DPRINT("I am the Boot Master\r\n")

	li	a0, EV_BE_LOC
	ld	t1, 0(a0)		# Get EAROM byte 0

        mfc0    t0, C0_CONFIG
        nop
        and     t0, CONFIG_BE
        bnez    t0, 1f
        nop

        # Little Endian
	li	s0, EV_EAROM_BYTE0 & (~EAROM_BE_MASK)
        b       2f
        nop
1:
        # Big Endian
	li	s0, EV_EAROM_BYTE0 | EAROM_BE_MASK
2:

	# t1 contains the current EAROM byte 0
	
	beq	t1, s0, 3f	# If byte 0 is correct, go on.
	nop

	# a0 contains EV_BE_LOC

	jal	write_earom	# Correct the EAROM.
	move	a1, s0		# (BD) Value to write

	li	a0, EV_BE_LOC
	ld	t0, 0(a0)	# Get the real byte zero.
	nop			# (LD)

	

	bne	t0, s0, prom_abdicate
	li	a0, EVDIAG_UNFIXABLE_EAROM	# (BD) Diagval to return

3:
	jal	delay
	li	a0, 10000

	la	a0, everest_mesg
	jal	sysctlr_message
	nop

	li	a0, 500000
	jal	delay
	nop	

	jal	sysctlr_getdebug
	nop

	andi	v0, VDS_NO_DIAGS
	beqz	v0, 1f
	nop				# (BD)

	GET_BSR(a1)
	ori	a1, BS_NO_DIAGS		# Turn on no diags bit
	SET_BSR(a1)

	la	a0, no_diags_mesg
	jal	sysctlr_message
	nop				# (BD)
	
	j	3f			# Skip dcache diags
	nop				# (BD)
	
1:

	GET_BSR(a1)
	andi	a1, BS_NO_DIAGS
	bnez	a1, 3f			# Skip diags if bit's set
	nop

	/*
	 * Now that the boot master has the bus all to itself,
	 * it can do some really serious diagnostics.  
	 */
	jal	set_cc_leds		# Announce start of 2nd phase EBUS test
	ori	a0, zero, PLED_CKEBUS2	# (BD)
	SCPRINT("EBUS diags 2..") 
	jal	pod_check_ebus2
	nop

	/*
	 * Do preliminary primary data cache testing before we
	 * use it as a stack.
	 */

	jal	set_cc_leds
	ori	a0, zero, PLED_CKPDCACHE1
	SCPRINT("PD Cache test..")
	jal	pod_check_pdcache1
	nop
	beqz	v0, 3f
	nop

	/* Abdicate bootmastership.  Send the diagnostic code as the
	 * reason for abdicatoing.
	 */
	b	prom_abdicate
	move	a0, v0

	/*
 	 * Set up a stack in the primary dcache so that we can 
	 * execute C code.
	 */
3:
	jal	set_cc_leds		# Set up the LEDS
	ori	a0,zero, PLED_MAKESTACK # (BD)
	SCPRINT("Building stack..")
	jal	pon_fix_dcache_parity	# Fix up the cache so we can run out
	nop				#   of it.
	li	a0, PROM_SR
	mtc0	a0, C0_SR		# Set up the status register
	nop

	jal	get_dcachesize		# Get the primary dcache size 
	nop
	li	sp, POD_STACKADDR	# Load the base stack address
	subu	v0,4			
	addu	sp,v0			# The stack grows down, so inc sp
	SCPRINT("Jumping to MAIN")
 
	jal	set_cc_leds		# Set the LEDS
	ori	a0, zero, PLED_MAIN
	j	main			# Now call the main C routine.
	nop
END(prom_master)


LEAF(prom_abdicate)
	# Load diagval reg with the reason we're abdicating 
	mtc1	a0, DIAGVAL_REG

	# Set BM "I've been master" bit
	GET_BSR(t0)
	ori	t0, BS_ABDICATED	
	ori	t1, zero, BS_USE_EPCUART
	not	t1
	and	t0, t1			# Turn off EPC UART bit.
	SET_BSR(t0)

	# Flush characters out of the CC UART
	jal	ccuart_flush
	nop

	# Get out of the master group
	li	t0, EV_IGRMASK
	sd	zero, 0(t0)
	
	# Send rearb character to sysctlr
	jal	ccuart_putc
	li	a0, SC_ESCAPE		# (BD)
	jal	ccuart_putc
	li	a0, SC_BMRESTART	# (BD)

	jal	pod_clear_ints		# Clear the interrupts the other
	nop				# (BD) CPUs sent us

	# Send interrupt to slaves
	SENDINT_SLAVES(REARB_LEVEL)

	# Jump to slave loop
	j	rearb_bootmaster	# Join the rearbitartion fun!
	nop				# (BD)	

	END(prom_abdicate)

	.data
everest_mesg:	.asciiz	"Starting System.."
no_diags_mesg:	.asciiz "* DIAGS DISABLED *"

