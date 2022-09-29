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

#ident "$Revision: 1.4 $"

#include <asm.h>

#include <sys/regdef.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/sbd.h>
#include <sys/mips_addrspace.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evdiag.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/sysctlr.h>
#include <sys/EVEREST/IP25addrs.h>
	
#include "ip25prom.h"
#include "prom_intr.h"
#include "prom_leds.h"
#include "pod.h"
#include "pod_failure.h"
#include "ml.h"

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
	LEDS(PLED_BMASTER)
	DPRINT("I am the Boot Master\r\n")
	DELAY(50000)

	SCPRINT("Starting System..")
	DELAY(50000)

	jal	sysctlr_getdebug
	nop

        andi	v0, VDS_NO_DIAGS
	beqz	v0, 1f
	nop				# (BD)

	DMFBR(a1, BR_BSR)
	ori	a1, BSR_NODIAG		# Turn on no diags bit
	DMTBR(a1, BR_BSR)

	SCPRINT("* DIAGS DISABLED *")
	b	2f
	nop
1:	
        /*
         * Do preliminary primary data cache testing before we
         * use it as a stack.
         */
	LEDS(PLED_CKPDCACHE1)
        SCPRINT("Testing D-Cache ..")
	jal	testDcache
	nop
        beqz    v0, 2f
        nop
	SCPRINT("* FAILED *")
	DELAY(50000)
	jal	invalidateDcache
	nop

	/* Abdicate bootmastership.  Send the diagnostic code as the
	 * reason for abdicatoing.
	 */
	b	prom_abdicate
	move	a0, v0
2:
	jal	invalidateDcache
	nop
	jal	invalidateScache
	nop
        jal     invalidateScache
	nop
	LEDS(PLED_MAKESTACK)
	SCPRINT("Building stack ... ")
        jal     initDcacheStack
        nop

	dli	a0, PROM_SR 
	DMTC0(a0, C0_SR)		# Set up the status register
	
	SCPRINT("Jumping to MAIN")
	LEDS(PLED_MAIN)
	j	main			# Now call the main C routine.
	nop
END(prom_master)


LEAF(prom_abdicate)
	# Load diagval reg with the reason we're abdicating 
	DMTBR(a0, BR_DIAG)

	# Set BM "I've been master" bit
	DMFBR(t0, BR_BSR)
	ori	t0, BSR_ABDICATE
	ori	t1, zero, BSR_EPCUART
	not	t1
	and	t0, t1			# Turn off EPC UART bit.
	DMTBR(t0, BR_BSR)

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
