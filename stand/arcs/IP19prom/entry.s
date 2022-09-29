/***********************************************************************\
*	File:		entry.s						*
*	Creation Date:	April 1, 1992					*
*									*
*	This file contains the exception vectors and startup code for	*
*	the Everest system.  Among other things, this file configures	*
*	the R4000, sets up the CC functions and uart, and arbitrates	*
*	for the status of Boot Master.					*
*									*
\***********************************************************************/

#ident	"$Revision: 1.49 $"

#include <asm.h>
#include <regdef.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/EVEREST/gda.h>
#include <sys/EVEREST/everror.h>
#include <sys/EVEREST/evconfig.h>
#include "ip19prom.h"
#include "prom_leds.h"
#include "prom_config.h"
#include "pod.h"
#include "pod_failure.h"
#include "prom_intr.h"

		.text
		.set 	noreorder
		.set	at
/*
 * The power-on vector table starts here.  It is important
 * to ensure that this file is loaded at 0xbfc00000 so that
 * the vectors appear in the memory locations expected by
 * the R4000.
 * 
 * If we are running under NEWT, this file must be loaded at
 * address 0x80000000.
 */

#define JUMP(_label)	j _label ; nop

LEAF(start)
	JUMP(entry)			# 0xbfc00000	
        JUMP(ip19prom_restart)		# 0xbfc00008
        JUMP(ip19prom_reslave)		# 0xbfc00010
        JUMP(ip19prom_podmode)		# 0xbfc00018
        JUMP(ip19prom_epcuart_podmode)	# 0xbfc00020
        JUMP(ip19prom_flash_leds)	# 0xbfc00028
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x040 offset */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x080 offset */
        JUMP(bev_xtlbrefill)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x0c0 offset */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x100 offset */
        JUMP(bev_ecc)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x140 offset */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x180 offset */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x1c0 offset */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x200 offset */
        JUMP(bev_tlbrefill)	/* 32-bit EXL==0 TLB exc vector */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x240 offset */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x280 offset */
        JUMP(bev_xtlbrefill)	/* Extended addressing TLB exc vector */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x2c0 offset */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x300 offset */
        JUMP(bev_ecc)		/* Cache error boot exception vector */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x340 offset */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x380 offset */
        JUMP(bev_general)	/* General exception vector */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
	END(start)
 
/*
 * entry -- When the CPU begins executing it jumps
 *	through the power-on vector to this point.
 *	This routine initializes the R4000 and starts 
 *	basic system configuration.
 */
LEAF(entry)
	/*
	 * Check to see if we got an NMI.  If so, jump to the NMI
	 * handler code.
	 */
	mfc0	k0, C0_SR		# Load the status register
	nop
	nop
	and	k0, SR_SR		# Check for an NMI
	bnez	k0, bev_nmi		# IF NMI jump to NMI handler
	nop

initialize:
	
	jal	init_cpu		# Set up the main processor
	nop				# (BD)

	/*
 	 * Make sure that cop1 is working well enough to
	 * store stuff in it.
	 */
	li	v0, 0xdeadbabe		# Pick an arbitrary value
	mtc1	v0, $f5			# Stuff it into the FP cop
	nop				# Pause for station identification
	mfc1	v1, $f5			# Get value back again
	nop				#
	beq	v0, v1, 1f		# IF values don't match THEN
	nop				#   (BD)		
	jal	flash_cc_leds		#   Flash a death message
	li	a0, FLED_DEADCOP1	# ENDIF
1:
	SET_BSR(zero)			# Clear the Boot status register

	/*
	 * Clear the cache tags
	 */
	jal     set_cc_leds		# Set the LEDS
	li      a0, PLED_CLEARTAGS
	jal     pon_invalidate_dcache	# Invalidate dcache tags 
	nop

	/*
	 * Initialize the local processor resource registers.
	 */
	jal	set_cc_leds		# Indicate we're gonna test the CC 
	ori	a0, zero, PLED_CKCCLOCAL # (BD)
	jal	pod_check_cclocal	# Call the diagnostic
	nop				# (BD)

	jal	set_cc_leds		# 
	ori	a0, zero, PLED_CCINIT1 	# (BD)

	li	a0, EV_IGRMASK		# 
	sd	zero, 0(a0)		# Turn off all Interrupt Groups
	li	a0, EV_ILE		# 
	sd	zero, 0(a0)		# Disable interrupts
	li	a0, EV_CIPL124		#
	sd	zero, 0(a0)		# Clear all R4K lev 1,2,4,5 ints
	li	a0, EV_CERTOIP		# 
	sd	zero, 0(a0)		# Clear any error states

	li	a0, EV_ECCSB_DIS	# Load the CC chip local resource value
	sd	zero, 0(a0)		# Enable single bit ECC
        
	/*
         * Set up the local CC chip's configuration registers.
	 * Because a number of these values are sensitive to 
	 * the processor and bus speed, we pull the appropriate
	 * values out of the EAROM.
         */
	jal	set_cc_leds		# Set up for CC config reg test
	ori	a0, zero, PLED_CKCCCONFIG
	jal	pod_check_ccconfig
	nop
	
        jal     set_cc_leds         	#
        ori	a0, zero, PLED_CCINIT2 	# (BD)

	EV_GET_SPNUM(t0, t1)

	li	a0, EV_ECCENB_LOC	# Set the ECC disable
	ld	a1, 0(a0)
	nop				# (LD)
	xori	a1, 1
	EV_SET_PROCREG(t0, t1, EV_ECCHKDIS, a1)

	li	a0, EV_PGBRDEN_LOC	# Set piggyback read enable
	ld	a1, 0(a0)
	nop				# (LD)
	EV_SET_PROCREG(t0, t1, EV_PGBRDEN, a1)

	li	a0, EV_CACHE_SZ_LOC	# Set secondary cache size
	ld	a1, 0(a0)
	ori	a0, zero, 22		# 22 = log2(four meg)
	sub	a1, a0, a1		# a1 = 22 - log2(cache size)
	EV_SET_PROCREG(t0,t1, EV_CACHE_SZ, a1)

	li	a0, EV_IW_TRIG_LOC	# Set Intervention resp. trig value
	ld	a1, 0(a0)
	nop				# (LD)
	EV_SET_PROCREG(t0,t1, EV_IW_TRIG, a1)

	li	a0, EV_RR_TRIG_LOC	# Set Read Response trigger value
	ld	a1, 0(a0)
	nop				# (LD)
	EV_SET_PROCREG(t0,t1, EV_RR_TRIG, a1)

	/*
	 * Configure the CC UART.
	 */
	jal	set_cc_leds		#
	ori	a0, zero, PLED_UARTINIT
	jal	ccuart_init	# Call the UART configuration code
	nop				# (BD)
	jal	set_cc_leds		# Show completion of leds
	ori	a0, zero, PLED_CCUARTDONE

	GET_BSR(t0)
	or	t0, BS_CCUART_INITED
	SET_BSR(t0)

	/*
	 * Print PROM header, which includes version and endianess
	 */
	DPRINT("\r\n\nEVEREST IP19 BRINGUP PROM ");
	mfc0	t0, C0_CONFIG
	nop
	and	t0, CONFIG_BE
	bnez	t0, 1f
	nop
	DPRINT("(LE)\r\n")
	b	2f
	nop
1:
	DPRINT("(BE)\r\n")
2:

	/* 
	 * Check and do initial configuration of the A chip.
	 * In order to avoid conflicts between the processors as they
	 * test their connection with the A chip, we introduce a delay
	 */
	jal	set_cc_leds		# Announce start of A chip init 
	ori	a0, zero, PLED_CKACHIP 	# (BD)

	DPRINT("Testing A chip...\t\t\t"); 
	li	a0, EV_SPNUM		# Get the processor SPNUM
	ld	a0, 0(a0)		# Load slot and processor number
	nop
	nop
	and	a0, EV_PROCNUM_MASK	# Mask out the slot info
	jal	delay
	sll	a0, 11			# Multiply slot number by 2048
		
	jal	pod_check_achip		# Call the A chip diagnostic
	nop				# (BD)

	DPRINT("A CHIP Test Passed\r\n")	
	jal	set_cc_leds		#
	ori	a0, zero, PLED_AINIT 	# (BD)
	DPRINT("Initializing A chip...\t\t\t")
	EV_GET_SPNUM(t0,zero)		# Get slot number
	EV_GET_CONFIG(t0, EV_A_ERROR_CLEAR, t1)
	EV_SET_CONFIG(t0, EV_A_RSC_TIMEOUT, RSC_TIMEOUT)
	EV_SET_CONFIG(t0, EV_A_URGENT_TIMEOUT, URG_TIMEOUT)

2:
	DPRINT("Done\r\n")
	/*
	 * Check the ebus over
	 */
	jal	set_cc_leds		# Announce start of Ebus testing 
	ori	a0, zero, PLED_CKEBUS1 	# (BD)
	DPRINT("Running first EBUS test...\t\t")
	jal	pod_check_ebus1
	nop
	beq	v0, zero, 1f		# IF the EBUS test failed THEN
	nop
	DPRINT("EBUS test failed.\r\n")	
	jal	flash_cc_leds	#   Flash the status message
	ori	a0, zero, PLED_CKEBUS1 	#   (BD) Just flash the base value
					# ENDIF 
	
	/*
	 * Tell the System Controller that it needs to rearbitrate?
	 * for the boot master.
	 */
1:
	DPRINT("EBUS1 Test Passed\r\n")
	jal	set_cc_leds		# Announce SysCtlr init
	ori 	a0, zero, PLED_SCINIT 	# (BD)
	
	/*
	 * Perform Boot master arbitration. If this processor is
 	 * the Boot Master, jump to the master processor thread of
	 * execution.  Otherwise, jump to the slave thread of execution.
	 */
	jal	set_cc_leds		# Boot Master Arb 
	ori	a0, zero, PLED_BMARB
	DPRINT("Starting Boot Master arbitration...\t")
	
	j	arb_bootmaster		# Call subroutine to do actual arb
	nop				# (BD)
	# DOESN'T RETURN

	END(entry) 


/*
 * Routine init_cpu
 * 	Set up the R4000's basic control registers and TLB.
 */

LEAF(init_cpu)
	move	s6, ra			# Save return address

	li	v0, PROM_SR
	mtc0	v0, C0_SR		# Put SR into known state
	nop				# Be really paranoid about getting
	nop				#   enough NO-OPS
	nop
	nop

	mtc1	zero, BSR_REG		# Clear the BSR
	nop
	mtc1	zero, NOFAULT_REG	# Clear the fault exception pointer
	nop
	mtc1	zero, ASMHNDLR_REG	# Clear the asmfault exception ptr.
	nop
	mtc1	zero, DUARTBASE_REG	# Clear the EPC DUART base address
	nop

	jal	set_cc_leds		# Turn off the leds
	move	a0, zero

	mtc0	zero, C0_CAUSE		# Clear software interrupts
	nop
	mtc0	zero, C0_WATCHLO	# Clear all watchpoints
	nop
	mtc0	zero, C0_WATCHHI	#

	li	v0, CONFIG_COHRNT_EXLWR
	mtc0	v0, C0_CONFIG
	nop

	jal	flush_tlb		# Clear out the TLB
	nop

	j	s6			# Return to caller
	nop				#	
	END(init_cpu)


/*
 * Routine bev_nmi
 *	If the CC_UART has been initialized, jump to POD MODE
 *	(for now).  Otherwise, just jump back to initialize and
 *	redo all of the system initialization.
 */

LEAF(bev_nmi)
	# Make sure the floating point processor is active so
	# we can store our fault vectors in the FP registers.
	#
	mfc0	k0, C0_SR		# Load the SR 
	li	k1, (SR_CU1|SR_FR)
	or	k0, k1
	mtc0	k0, C0_SR		# Switch on the FP coprocessor
	nop				# Wait for CU1 to become happy
	nop				# Still waiting
	nop				# It just keeps waiting and 
	nop				# waiting
	mtc1	zero, NOFAULT_REG 	# Switch off the nofault support
	nop
		
	# Setup the asm fault handler so that we can deal with a
	# bad address error if memory isn't configured (which we
 	# expect in some cases) or some other exception (which we
 	# don't expect but might happen anyway).
	#
	la	k0, pod_wrapper		# Jump to pod_wrapper on exception
	mtc1	k0, ASMHNDLR_REG 
	nop

	# Try loading the NMI Vector from main memory.  This
	# may very well lead to an exception, but since we've
	# installed the fault handler, we don't care!
	#
	li	k0, GDA_ADDR		# Load address of global data area
	lw	k0, G_MAGICOFF(k0)	# Snag the actual entry
	li	k1, GDA_MAGIC		# (LD) Load magic number
	bne	k0, k1, 9f		# Didn't work. Just goto pod_handler
	nop				# (BD)

	li	k0, GDA_ADDR		# Reload GDA address
	lw	k1, G_NMIVECOFF(k0)	# Load the vector
	nop				# (LD)

	beqz	k1, 9f			# If vec is zero, just goto pod
	nop				# (BD)

	mtc1	k1, NMIVEC_REG		# Store away jump address
	nop
	li	k0, PLED_NMIJUMP
	li	k1, EV_LED_BASE
	sd	k0, 0(k1)		# Set the LEDs to NMI jump value.
	mfc1	k1, NMIVEC_REG		# Retrieve the jump address
	nop				# (LD)

	# Delay a bit so that we don't clobber the thing before
	# 	someone else jumps through it.
	#
	li	k0, 1000000		# Delay value
8:
	bnez	k0, 8b
	addi	k0, -1			# Decrement counter


	li	k0, GDA_ADDR		# ReReload GDA address
	j	k1			# Jump to the NMI vector
	sw	zero, G_NMIVECOFF(k0)	# (BD) Avoid loops in NMI handler
9:
	# If we get to this point, we know that memory is at least
	# somewhat alive.  However, we don't know whether we're
	# a master or a slave processor.  So we read the masterspnum
	# to find out.
	#

	mtc1	zero, ASMHNDLR_REG	# Zap the nofault stuff.  Memory's OK
	nop

	jal	pod_gprs_to_fprs	
	mtc1	ra, RA_FP		# (BD) Save ra

	li	k0, GDA_ADDR
	lw	k0, G_MASTEROFF(k0)	# Read the master spnum
	li	k1, EV_SPNUM		# Load the SPNUM resource addr
	ld	k1, 0(k1)		# Read our spnum
	nop				# (LD)
	bne	k0, k1, 10f		# Skip forward if we're not master
	nop				# (BD)

	li	k0, GDA_ADDR		# Reload GDA address
	lw	k1, G_VDSOFF(k0)	# Load the debug settings
	nop				# (LD)
	andi	k1, VDS_MANUMODE	# Is manumode set?
	bnez	k1, 11f			# If manumode is on use CC UART.
	nop				# (BD)

	# We're not in manufacturing mode so use the EPC UART
	# 
	li	a0, BS_USE_EPCUART | BS_CCUART_INITED
	SET_BSR(a0)
	b	12f
	nop				# (BD)
11:
	# We're in manufacturing mode, use the CC UART
	#
	li	a0, BS_MANUMODE | BS_CCUART_INITED
	SET_BSR(a0)
12:

	la	a0, nmi_msg
	j	pod_handler
	li	a1, EVDIAG_NMI		# (BD)

	# If we get here, we're a lame slave processor, so we just
	# jump into the the prom slave loop.
10:	#

	# If the cheese-meister UART flag is set, turn on the CC UART, and
	# jump to pod_handler instead of the slave loop.

	li	k0, GDA_ADDR		# Reload GDA address
	lw	k1, G_VDSOFF(k0)	# Load the flag
	nop				# (LD)
	andi	k1, VDS_NOARB		# Is noarb set?
	beqz	k1, 14f			# If flag is zero, do nothing.
	nop				# (BD)

	DPRINT("Galles mode on.\r\n")

	# Set-up the BSR	
	li	a0, BS_USE_EPCUART
	SET_BSR(a0)

	la	a0, nmi_msg
	j	pod_handler
	li	a1, EVDIAG_NMI 		# (BD)

14:
	DPRINT("Galles mode off.\r\n")
	li	a0, BS_SLAVE
	SET_BSR(a0)
	j	prom_slave
	nop				# (BD)
	END(bev_nmi)

/*
 * bev_panic -- General exception handler.  Most of the exception handlers
 * call this when problems occur.
 */

LEAF(bev_panic)
	move	s0, a0			# Save secret exception number

	jal	set_cc_leds
	move	s1, a1			# (BD) Save secret exception string

	/*
  	 * Check to see if the nofault vector is non-zero
	 * and jump to the user-specified exception vector 
	 */
	mfc1	a0, NOFAULT_REG		# Grab fault vector 
	nop
	beqz	a0, 2f
	nop				# (BD)

	j	longjmp			# Call longjmp (should never return)
	ori	a1, zero, 1		# (BD) Tell program we're returning

	/*
	 * We didn't have any handlers in place, so just do the 
	 * normal exception handling.  We just assume that the
	 * UART is working.  If it doesn't, too bad.
	 */

	GET_BSR(a0)
	ori	a0, BS_SLAVE
	beqz	a0, 2f
	nop

	j	flash_cc_leds
	move	a0, s0
	
2:
	la	a0, panic_msg
	jal	pod_puts
	nop

	/*
	 * If an ECC error occurred, dump out the contents of the
	 * ECC register.
	 */	
	li	t0, FLED_ECC
	bne	s0, t0, 3f
	nop

	jal	pod_ecc
	nop

	/*
	 * Call the standard POD handler
	 */
3:
	li	a1, EVDIAG_PANIC
	j	pod_handler
	move	a0, s1			# (BD) Put the message in a0

	j	flash_cc_leds
	ori	a0, zero, FLED_IMPOSSIBLE1
	nop
	END(bev_panic)


/*
 * In order to avoid trashing the registers saved in the fprs,
 * we need to check for an asm fault handler immediately when
 * we enter each of the exception handlers.  If one isn't set,
 * we save the ra and then proceed.  This stuff is written as
 * macro so that we won't lose the ra.
 */

#define COMMON_CODE	 		\
	mfc0	k0, C0_SR ;		\
	nop ;				\
	li	k1, (SR_CU1 | SR_FR) ;	\
	or	k0, k1 ;		\
	mtc0	k0, C0_SR ; 		\
	nop ;				\
	nop ;				\
	nop ;				\
	nop ;				\
	mfc1	k0, ASMHNDLR_REG ;	\
	nop ;				\
	mtc1	zero, ASMHNDLR_REG ;	\
	nop ;				\
	beqz	k0, 95f ;		\
	nop ;				\
	j	k0 ;			\
	nop ;				\
95:	mtc1	ra, RA_FP ;		\
	nop

LEAF(bev_tlbrefill)
	COMMON_CODE
	jal	pod_gprs_to_fprs
	nop				# (BD)	
	la	a1, tlb_msg
	j	bev_panic
	or	a0, zero, FLED_UTLBMISS	
	END(bev_tlbrefill)	

LEAF(bev_xtlbrefill)
	COMMON_CODE
	jal	pod_gprs_to_fprs
	nop				# (BD)
	la	a1, xtlb_msg
	j	bev_panic
	or	a0, zero, FLED_XTLBMISS
	END(bev_xtlbrefill)

LEAF(bev_ecc)
	COMMON_CODE
	jal	pod_gprs_to_fprs
	nop				# (BD)

	la	a1, ecc_msg
	j	bev_panic
	or	a0, zero, FLED_ECC 
	END(bev_ecc)


LEAF(bev_general)
	COMMON_CODE
	jal	pod_gprs_to_fprs
	nop				# (BD)

	mfc0	t1, C0_CAUSE
	li	t0, CAUSE_EXCMASK
	nop
	
	and	t0, t1
	li	t1, EXC_WATCH

	beq	t0, t1, 1f		# If watch point jump to Watch handler
	nop				# (BD)

	li	t1, EXC_BREAK
	
	beq	t0, t1, 2f		# If break point jump to break handler
	nop				# (BD)
	
	la	a1, general_msg
	j	bev_panic
	or	a0, zero, FLED_GENERAL	# (BD)
1:
	j	watch_handler
	nop

2:	j	break_handler
	nop
	END(bev_general)


LEAF(notimplemented)
	la	a1, notimpl_msg
	j	bev_panic
	or	a0, zero, FLED_NOTIMPL
	END(notimplemented)	


LEAF(pod_wrapper)
	# If the memory access failed, there will be an
	# address error on MyRequest.  Clear this.
	li	k0, EV_CERTOIP
	li	k1, CC_ERROR_MY_ADDR 
	sd	k1, 0(k0)
	nop

	GET_BSR(a0)
	li	a1, BS_USE_EPCUART
	not	a1
	and	a0, a1
	SET_BSR(a0)

	DPRINT("Took some wacky exception.")

	la	a0, nmi_exc_msg
	j	pod_handler
	li	a1, EVDIAG_NMI		# (BD)
	END(pod_wrapper)


/*
 * reinitialize
 *	Reinitializes the basic system state.
 */

LEAF(reinitialize)
	move	s7, ra			# Save return address

	jal	init_cpu		# Reinitialize the cpu registers
	nop				# (BD)

	SET_BSR(zero)			# Reset the boot status register

	jal	set_cc_leds
	ori	a0, zero, PLED_CLEARTAGS	# (BD)
	jal	pon_invalidate_dcache	# Clear the PD Cache
	nop				# (BD)

	#
	# Set up the basic processor local resource registers
	#
	jal	set_cc_leds
	ori	a0, zero, PLED_CCINIT1

	li	a0, EV_IGRMASK
	sd	zero, 0(a0)
	li	a0, EV_ILE
	sd	zero, 0(a0)
	li	a0, EV_CIPL124
	sd	zero, 0(a0)
	li	a0, EV_CERTOIP
	sd	zero, 0(a0)

	EV_GET_SPNUM(t0, zero)
	EV_GET_CONFIG(t0, EV_A_ERROR_CLEAR, zero)

	j	s7
	nop
	END(reinitialize)

/*
 * ip19prom_restart
 *	Called from the kernel when the system wants to 
 *	reboot or return the PROM monitor.  This routine
 *	sets up a stack and calls the restart C routine.
 *	It assumes that we don't want to rearbitrate for
 * 	boot mastership, that no diags should be run, and
 *	that memory is already initialized.
 */

LEAF(ip19prom_restart)

	jal	reinitialize		# Reset the basic IP19 registers
	nop				# (BD)

	# Reinitialize the caches
	#
	SCPRINT("Initing caches...")
	jal	set_cc_leds
	ori	a0, zero, PLED_CKSCACHE1 # (BD)
	jal	pod_setup_scache
	nop				# (BD)
	li	a0, POD_SCACHEADDR	# Address to use for zeroes
	jal	pon_zero_icache
	nop				# (BD)
	jal	pon_invalidate_icache
	nop				# (BD)
	jal	pon_invalidate_dcache
	nop				# (BD)
	jal	pon_invalidate_scache
	nop				# (BD)

	# Reinitialize the bus tags
	#
	SCPRINT("Initing BUS TAGS...")
	jal	set_cc_leds		
	ori	a0, zero, PLED_CKBT
	jal	pod_check_bustags
	nop

	# Set up the stack in cache
	#
	SCPRINT("Building stack")
	jal	set_cc_leds
	ori	a0, zero, PLED_MAKESTACK
	jal	get_dcachesize
	nop
	li	sp, POD_STACKADDR
	subu	v0, 4
	addu	sp, v0

	# Reinitialize the EPC
	#
	SCPRINT("Reiniting IO4")
	la	a0, EVCFGINFO_ADDR
	jal	io4_initmaster	
	nop
	SCPRINT("Initing EPC UART")
	jal	init_epc_uart
	nop

	GET_BSR(t0)
	ori	t0, BS_USE_EPCUART
	SET_BSR(t0)

	SCPRINT("Loading IO4 PROM...")
	jal	load_io4prom
	nop

	jal	flash_cc_leds
	ori	a0, zero, PLED_PROMJUMP	

1:	 b	1b
	END(ip19prom_restart)


/*
 * ip19prom_reslave
 *	Reinitializes a couple parts of the system and then
 *	switches into slave mode.  Called by non-bootmaster
 *	processors when the kernel is shutting down.
 */

LEAF(ip19prom_reslave)
	jal	reinitialize		# Reset the basic IP19 registers
	nop				# (BD)

	j	prom_slave		# Jump back to the slave loop
	nop				# (BD)
	END(ip19prom_reslave)


/*
 * ip19prom_podmode
 *	Reinitializes the prom and jumps to podmode.  Allows code
 *	like symmon or the secondary prom to get back to home base.
 */

LEAF(ip19prom_podmode)
	jal	reinitialize		# Reinitialize the basic IP19 registers
	nop				# (BD)

	GET_BSR(t0)
	ori	t0, zero, BS_CCUART_INITED
	SET_BSR(t0)

	la	a0, podhdlr_msg		# Load message pointer into a0
	j	pod_handler		# Jump back to POD mode
	li	a1, EVDIAG_DEBUG		# (BD)
	END(ip19prom_podmode)


LEAF(ip19prom_epcuart_podmode)
	jal	reinitialize
	nop

	li	t0, BS_CCUART_INITED|BS_USE_EPCUART|BS_POD_MODE|BS_NO_DIAGS
	
	SET_BSR(t0)

	j	prom_master

	END(ip19prom_epcuart_podmode)

/*
 * Various messages and text strings.
 */
		.data
panic_msg:	.asciiz "\r\nPanic in IP19 PROM: "
epc_msg:	.asciiz "\r\nEPC: "
cause_msg:	.asciiz "\r\nCAUSE: "
sr_msg:		.asciiz "\r\nSR: "
badvaddr_msg:	.asciiz "\r\nBADVADDR: "
podhdlr_msg:	.asciiz "\r\nJumping to POD mode.\r\n"
tlb_msg: 	.asciiz "TLB miss\r\n"
xtlb_msg:	.asciiz	"XTLB miss exception\r\n"
ecc_msg:	.asciiz "ECC exception\r\n"
general_msg:	.asciiz "General exception\r\n"
notimpl_msg:	.asciiz "jump to unimplemented vector\r\n"
watch_msg:	.asciiz	"Watch\r\n"
pod_msg:	.asciiz	"Jumping to POD mode.\r\n"
nmi_msg:	.asciiz "\r\n\n*** Non-maskable interrupt occurred. \r\n\n"
nmi_exc_msg:	.asciiz "\r\n\n*** Non-maskable intr/exception handler \r\n"
