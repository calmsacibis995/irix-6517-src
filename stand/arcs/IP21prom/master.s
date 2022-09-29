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

#ident "$Revision: 1.37 $"

#include "ml.h"
#include <sys/regdef.h>
#include <asm.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include "ip21prom.h"
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

	jal	delay
	li	a0, 10000

	dla	a0, everest_mesg
	jal	sysctlr_message
	nop

	li	a0, 50000
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

	dla	a0, no_diags_mesg
	jal	sysctlr_message
	nop				# (BD)
	
	j	3f			# Skip dcache diags
	nop				# (BD)
	
1:

	GET_BSR(a1)
	andi	a1, BS_NO_DIAGS
	bnez	a1, 3f			# Skip diags if bit's set
	nop

	jal	set_cc_leds
	ori	a0, zero, PLED_CKSCACHE1
	SCPRINT("Secondary Cache test..")

	jal	pon_invalidate_dcache
	nop
        jal     gcache_invalidate
    	nop
    	jal 	gcache_check
    	nop

    	move	s0, v0
        jal     gcache_invalidate
    	nop
	beqz	v0, 3f
	nop
	b	prom_abdicate
	move	a0, v0

        /*
         * Do preliminary primary data cache testing before we
         * use it as a stack.
         */
3:
        jal     set_cc_leds
        ori     a0, zero, PLED_CKPDCACHE1
        SCPRINT("PD Cache test..")
	/* additional dcache test */
        jal     dcache_tag
        nop
        beqz    v0, 4f
        nop



	/* Abdicate bootmastership.  Send the diagnostic code as the
	 * reason for abdicatoing.
	 */
	b	prom_abdicate
	move	a0, v0

4:

	jal	pon_invalidate_dcache
	nop

        jal     gcache_invalidate

	nop
	/*
 	 * Set up a stack in the primary dcache so that we can 
	 * execute C code, we set set 1 low 16KB as stack regrion.
	 */

	jal	set_cc_leds		# Set up the LEDS
	ori	a0,zero, PLED_MAKESTACK # (BD)

	SCPRINT("Building stack..")

        jal     pon_fix_dcache_parity   # 
        nop                             #   of it.

	#dli	a0, PROM_SR | SR_IE
	dli	a0, PROM_SR 
	DMTC0(a0, C0_SR)		# Set up the status register

	jal	get_dcachesize		# Get the primary dcache size 
	nop

        /* Force set 3 OFF and Writeback Disable OFF */

        dli     t0, BB_STARTUP_CTRL
        li      t1, 0           # WriteBackInhibit
        sd      t1, 0(t0)       # clear both ForceSet3 and WriteBackInhibit

        dli     t1, BB_SET_ALLOW
        li      a0, 0x0f
        sd      a0, 0(t1)       # set all 4 SetAllow bits to 1
	/* stack is created at the last 16 kbytes of First MB of set1 */
	dli	sp, POD_STACKADDR	# Load the base stack address
	subu	v0,8
	daddu	sp,v0			# The stack grows down, so inc sp


	SCPRINT("Jumping to MAIN")
 
	jal	set_cc_leds		# Set the LEDS
	ori	a0, zero, PLED_MAIN

	j	main			# Now call the main C routine.
	nop
END(prom_master)


LEAF(prom_abdicate)
	# Load diagval reg with the reason we're abdicating 
	DMTC1(a0, DIAGVAL_REG)

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
	dli	t0, EV_IGRMASK
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
