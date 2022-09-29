/***********************************************************************\
*	File:		entry.s						*
*									*
*	This file contains the exception vectors and startup code for	*
*	the Blackbird CPU.  Among other things, this file initializes	*
*	the registers and the caches, sets up the CC functions and uart *
*	and arbitrates for the status of the Boot Master.		*
*									*
\***********************************************************************/

#ident	"$Revision: 1.105 $"

#include "ml.h"
#include <asm.h>
#include <sys/regdef.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/fpu.h>
#include <sys/loaddrs.h>
#include <sys/EVEREST/gda.h>
#include <sys/EVEREST/everror.h>
#include <sys/EVEREST/evconfig.h>
#include "ip21prom.h"
#include "prom_leds.h"
#include "prom_config.h"
#include "pod.h"
#include "pod_failure.h"
#include "prom_intr.h"

#define EV_CELMAX 127

		.text
		.set 	noreorder
		.set	at

#if TFP_CC2
#define GOTO_CHANDRA_MODE		\
	LA	k0, 8f;			\
	LI	k1, 0x8fffffffffffffff;	\
	and	k0, k1;			\
	LI	k1, 0x0000000400000000;	\
	or	k0, k1;			\
	jr	k0;			\
	nop;				\
8:
#else
#define GOTO_CHANDRA_MODE		\
	LA	k0, 8f;			\
	LI	k1, 0x8fffffffffffffff;	\
	and	k0, k1;			\
	jr	k0;			\
	nop;				\
8:
#endif

/*
 * The power-on vector table starts here.  It is important to ensure
 * that this file is loaded at 0x900000001fc00000 so that the vectors
 * appear in the memory locations expected by the TFP.
 */

#define JUMP(_label)	j _label ; nop

LEAF(start)
	JUMP(entry)			# 0x900000001fc00000
        JUMP(ip21prom_restart)		# 0x900000001fc00008
        JUMP(ip21prom_reslave)		# 0x900000001fc00010
        JUMP(ip21prom_podmode)		# 0x900000001fc00018
        JUMP(ip21prom_epcuart_podmode)	# 0x900000001fc00020
        JUMP(c_ip21prom_flash_leds)	# 0x900000001fc00028
        JUMP(notimplemented)
        JUMP(notimplemented)		/* 0x040 offset */
	END(start)

/*
 * entry -- When the CPU begins executing it jumps
 *	through the power-on vector to this point.
 *	This routine initializes the processor and starts 
 *	basic system configuration.
 */
LEAF(entry)
	/*
	 * Check to see if we got an NMI.  If so, jump to the NMI
	 * handler code.
	 */
	DMFC0(k0, C0_CAUSE)		# Load the Cause register
	and	k0, CAUSE_NMI		# Check for an NMI
	bnez	k0, bev_nmi		# IF NMI, jump to NMI handler
	nop

initialize:
	dla	k0,trap_table
	DMTC0(k0,C0_TRAPBASE)		/* init TrapBase register */

	dli	v0, PROM_SR
	DMTC0(v0, C0_SR)		# Put SR into known state

	jal     set_cc_leds		# Set the LEDS
	li      a0, PLED_STARTUP	# (BD)


	jal	pon_invalidate_IDcaches	# Invalidate I&D cache at the same time
	nop

	jal     set_cc_leds		# Set the LEDS
	li      a0, PLED_TESTICACHE     # (BD)
	jal	pon_test_Icaches	# Test icache
	nop

	GOTO_CHANDRA_MODE		# icache is inited, use it

#if TFP_ADE_EBUS_WBACK_WAR
	/*
	 * The logic in the CC that reports address errors on EBus
	 * writeback channel is broken. Turn off the bit in the diag
	 * register. Note that the sense of the bits is reversed from
	 * the spec (only when we write the register)!
	 * For now, just slam a hard coded constant in there. If we
	 * ever need to touch the register for some other reason,
	 * we'll add #define's.
	 * The ip21prom writes the same value, but we do it here in case
	 * the system has old proms.
	 */
	LI	v0, EV_DIAGREG
	LI	v1, 0x40900000		# disable error bit 4 and
	sd	v1, 0(v0)		# preserve "in queue almost full" (9)
#endif

	dli	a0, EV_ERTOIP		# clear all pending interrupts
	ld	a1, 0(a0)		# read ERTOIP, we may print it later
	DMTC1(a1, EV_ERTOIP_REG)	# Stuff it into the FP for now
	li	a1, 0x0
	sd	a1, 0(a0)

	dli	a0, EV_CERTOIP		# clear all pending interrupts
	li	a1, 0x3fff
	sd	a1,0(a0)

        jal     set_cc_leds             # Set the LEDS
        li      a0, PLED_CHKSCACHESIZE  # (BD)
        jal     check_scachesize        # calculate scache tag ram size
        nop
        EV_GET_SPNUM(t0, t1)            # t0 gets slot 
					# t1 gets proc. number
        EV_SET_PROCREG(t0, t1, EV_CACHE_SZ, v0)  # store in gcache size reg

	jal	init_cpu		# Set up the main processor
	nop				# (BD)

	/*
 	 * Make sure that cop1 is working well enough to
	 * store stuff in it.
	 */
	dli     v0, 0xa5a5a5a5a5a5a5a5  # Pick an arbitrary value
	DMTC1(v0, $f5)			# Stuff it into the FPGR $f5
	DMFC1(v1, $f5)			# Get value back (from $f5)
	beq	v0, v1, 1f		# IF values don't match THEN
	nop				# (BD)		
	jal	flash_cc_leds		#   Flash a death message
	li	a0, FLED_DEADCOP1	# ENDIF

	dli     v0, 0xa65a5a5a5a5a5a5a  # Pick an another value
	DMTC1(v0, $f5)			# Stuff it into the FPGR $f5
	DMFC1(v1, $f5)			# Get value back (from $f5)
	beq	v0, v1, 1f		# IF values don't match THEN
	nop				# (BD)		
	jal	flash_cc_leds		#   Flash a death message
	li	a0, FLED_DEADCOP1	# ENDIF
1:
	not	v0			# invert pattern
	DMTC1(v0, $f5)			# Stuff it into the FPGR $f5
	DMFC1(v1, $f5)			# Get value back (from $f5)
	beq	v0, v1, 2f		# IF values don't match THEN
	nop				# (BD)		
	jal	flash_cc_leds		#   Flash a death message
	li	a0, FLED_DEADCOP1	# ENDIF
2:
	SET_BSR(zero)			# Clear the Boot status register

	/*
	 * Clear the cache tags
	 */
	jal     set_cc_leds		# Set the LEDS
	li      a0, PLED_CLEARTAGS
	jal     pon_invalidate_dcache	# Invalidate dcache tags 
	nop

	/*
	 * Can't have all cpus doing config read/write tests at
	 * the same time, so delay a while. Don't have RTC set up
	 * yet, so just kludge it.
	 */
	dli	a0, EV_SPNUM		# Get the processor SPNUM
	ld	a0, 0(a0)		# Load slot and processor number
	and	a0, EV_SLOTNUM_MASK | EV_PROCNUM_MASK
	sll	a0, 18

8:	addi	a0, -1
	nop				# Don't want the branch in the
	nop				# same quad as the target for
	nop				# predictable branch cache behavior
	bgtz	a0, 8b
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

	jal 	clear_regs		# clear registers initially
	nop

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

	li	a1, 0			# Clear piggyback read enable
	EV_SET_PROCREG(t0, t1, EV_PGBRDEN, a1)

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
	 * Print PROM header, which includes version and (endianess ??)
	 */
	SCPRINT("\r\n\nBB_EVEREST IP21 BRINGUP PROM ");
	DMFC0(t0, C0_CONFIG)
	and	t0, CONFIG_BE
	bnez	t0, 1f
	nop
	SCPRINT("(LE)\r\n")
	b	2f
	nop
1:
	SCPRINT("(BE)\r\n")
2:
	/* 
	 * Check and do initial configuration of the A chip.
	 * In order to avoid conflicts between the processors as they
	 * test their connection with the A chip, we introduce a delay
	 */
	jal	set_cc_leds		# Announce start of A chip init 
	ori	a0, zero, PLED_CKACHIP 	# (BD)

	SCPRINT("Testing A chip...\t\t\t"); 
	dli	a0, EV_SPNUM		# Get the processor SPNUM
	ld	a0, 0(a0)		# Load slot and processor number
	and	a0, EV_SLOTNUM_MASK | EV_PROCNUM_MASK
	jal	delay
	sll	a0, 11			# Multiply slot number by 2048
		
	jal	pod_check_achip		# Call the A chip diagnostic
	nop				# (BD)

	SCPRINT("A CHIP Test Passed\r\n")	
	jal	set_cc_leds		#
	ori	a0, zero, PLED_AINIT 	# (BD)
	SCPRINT("Initializing A chip...\t\t\t")
	EV_GET_SPNUM(t0,zero)		# Get slot number
	EV_GET_CONFIG(t0, EV_A_ERROR_CLEAR, t1)

	li	a1, EV_RSCTOUT
	EV_SET_CONFIG(t0, EV_A_RSC_TIMEOUT, a1)

	li	a1, EV_AURGTOUT
	EV_SET_CONFIG(t0, EV_A_URGENT_TIMEOUT, a1)

2:
	SCPRINT("Done\r\n")
	/*
	 * Check the ebus over
	 */
	jal	set_cc_leds		# Announce start of Ebus testing 
	ori	a0, zero, PLED_CKEBUS1 	# (BD)
	SCPRINT("Running EBUS test...\t\t")
	jal	pod_check_ebus1
	nop
	beq	v0, zero, 1f		# IF the EBUS test failed THEN
	nop
	SCPRINT("EBUS test failed.\r\n")	
	jal	flash_cc_leds	#   Flash the status message
	ori	a0, zero, PLED_CKEBUS1 	#   (BD) Just flash the base value
					# ENDIF 
1:
	SCPRINT("EBUS Test Passed\r\n")
	jal	set_cc_leds		# Announce SysCtlr init
	ori 	a0, zero, PLED_SCINIT 	# (BD)
	
	/*
	 * Perform Boot master arbitration. If this processor is
 	 * the Boot Master, jump to the master processor thread of
	 * execution.  Otherwise, jump to the slave thread of execution.
	 */
	jal	set_cc_leds		# Boot Master Arb 
	ori	a0, zero, PLED_BMARB

#if 0
/* Margie added */
debug_init:
   	dli	k1, EV_SPNUM 
	ld	k1, 0(k1) 
	nop 
	andi	k1, EV_PROCNUM_MASK
   	bne 	k1, r0, proc1_debug
   	nop
proc0_debug:
   	dli 	k1, 0x9000000000001000
   	sd  	r0, 0(k1)
   	j   	debug_init_done
	nop
proc1_debug:
   	dli 	k1, 0x9000000000002000
   	sd  	r0, 8(k1)

debug_init_done:
#endif

	SCPRINT("Starting Boot Master arbitration...\t")

	j	arb_bootmaster		# Call subroutine to do actual arb
	nop				# (BD)
	# DOESN'T RETURN
	END(entry) 


/*
 * Routine to clear registers.  Registers are not cleared in IP21
 * after a hardware reset and so, the registers might have non-zero
 * values at the beginning
 */

LEAF(clear_regs)
	/* clear all level 0 interrupts(0-127) first */
	dli    a0, EV_CIPL0
        li      a1, EV_CELMAX           # Highest interrupt number
clear_ints:
        sd      a1, 0(a0)               # Clear interrupt (BD)
        bne     a1, zero, clear_ints
        addi    a1, -1                  # Decrement counter (BD)

	/* set CEL to 127 */
        dli     a0, EV_CEL
        dli     a1, EV_CELMAX 
	
        dli     a0, EV_IGRMASK          # 
        sd      zero, 0(a0)             # Turn off all Interrupt Groups
        dli     a0, EV_ILE              # 
        sd      zero, 0(a0)             # Disable interrupts
        dli     a0, EV_CIPL124          #
        sd      zero, 0(a0)             # Clear all level 1,2 ints
        dli     a0, EV_CERTOIP          # 
        sd      zero, 0(a0)             # Clear any error states

	# dli     a0, EV_HPIL
	# sd 	zero, 0(a0)		# should already be 0 since we
					# already set EV_CIPL0	
	j 	ra
	nop
	END(clear_regs)


/*
 * Routine init_cpu
 * 	Set up the TFP's basic control registers and TLB.
 *
 *	For TFP, may need to add code for initializing Icache,
 *	Dcache, COP0 registers, SAQs, and Gcache.
 */

LEAF(init_cpu)
	move	s6, ra			# Save return address

	/* Initialize COP0 register.  C0_TRAPBASE assumed setup before calling
	 * this rountine.
	 */
        jal     set_cc_leds             # Boot Master Arb
        ori     a0, zero, PLED_INITCOP0
	dli	v0, PROM_SR
	DMTC0(v0, C0_SR)		# Put SR into known state
	DMTC0(zero, C0_TLBSET)		# Clear TLBset
	DMTC0(zero, C0_TLBLO)		# Clear EntryLo
	DMTC0(zero, C0_UBASE)
	DMTC0(zero, C0_BADVADDR)	# Clear VAddr
	DMTC0(zero, C0_COUNT)		# Clear Counts
	DMTC0(zero, C0_TLBHI)		# Clear EntryHi
	DMTC0(zero, C0_CAUSE)		# Clear interrupts
	DMTC0(zero, C0_EPC)		# Clear Exception Program Counter
/* No need to init Config reg.  It is initialized by hardware at reset. */
	DMTC0(zero, C0_WORK0)
	DMTC0(zero, C0_WORK1)
	DMTC0(zero, C0_PBASE)
	DMTC0(zero, C0_GBASE)
	DMTC0(zero, C0_WIRED)
	DMTC0(zero, C0_DCACHE)
	DMTC0(zero, C0_ICACHE)

	ctc1	zero, fpc_csr		# clear fp csr

        jal     set_cc_leds             # Boot Master Arb
        ori     a0, zero, PLED_FLUSHTLB
	jal	flush_entire_tlb	# Clear out the TLB
	nop

	jal     set_cc_leds             # Boot Master Arb
        ori     a0, zero, PLED_FLUSHSAQ
	jal	flush_SAQueue		# Initialize/flush SAQs
	nop

	jal	gcache_invalidate	# Initialize G-cache
/*	DEBUG_DUMP_TAGS(0x9000000000000000) */
	nop


	SET_BSR(zero)			# Clear the BSR
	DMTC0(zero, NOFAULT_REG)	# Clear the fault exception pointer
	DMTC0(zero, ASMHNDLR_REG)	# Clear the asmfault exception ptr.
	DMTC0(zero, DUARTBASE_REG)	# Clear the EPC DUART base address

	jal	set_cc_leds		# Turn off the leds
	move	a0, zero

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

	GOTO_CHANDRA_MODE		# icache is inited, use it

	DMFC0(k0, C0_SR)		# Load the SR
	dli	k1, PROM_SR
	or	k0, k1
	DMTC0(k0, C0_SR)		# Switch on the FP coprocessor
	DMTC0(zero, NOFAULT_REG)	# Switch off the nofault support
		
	# Setup the asm fault handler so that we can deal with a
	# bad address error if memory isn't configured (which we
 	# expect in some cases) or some other exception (which we
 	# don't expect but might happen anyway).
	#
	dla	k0, pod_wrapper		# Jump to pod_wrapper on exception
	DMTC0(k0, ASMHNDLR_REG)

	# Try loading the NMI Vector from main memory.  This
	# may very well lead to an exception, but since we've
	# installed the fault handler, we don't care!
	#
	dli	k0, GDA_ADDR		# Load address of global data area
	lw	k0, G_MAGICOFF(k0)	# Snag the actual entry
	li	k1, GDA_MAGIC		# (LD) Load magic number
	bne	k0, k1, 9f		# Didn't work. Just goto pod_handler
	nop				# (BD)

	dli	k0, GDA_ADDR		# Reload GDA address
	lw	k1, G_NMIVECOFF(k0)	# Load the vector
	nop				# (LD)

	beqz	k1, 9f			# If vec is zero, just goto pod
	nop				# (BD)

	DMTC0(t0, C0_WORK1)		# save t0 (trashes C0_WORK1)
	K32TOKPHYS(k1, k0, t0)		# compute nmi handler address

	li	k0, PLED_NMIJUMP
	dli	t0, EV_LED_BASE
	sd	k0, 0(t0)		# Set the LEDs to NMI jump value.
	DMFC0(t0, C0_WORK1)		# restore t0

	# Delay a bit so that we don't clobber the thing before
	# 	someone else jumps through it.
	#
	li	k0, 1000000		# Delay value
8:
	bnez	k0, 8b
	addi	k0, -1			# Decrement counter


	dli	k0, GDA_ADDR		# ReReload GDA address
	j	k1			# Jump to the NMI vector
	sw	zero, G_NMIVECOFF(k0)	# (BD) Avoid loops in NMI handler
9:
	# If we get to this point, we know that memory is at least
	# somewhat alive.  However, we don't know whether we're
	# a master or a slave processor.  So we read the masterspnum
	# to find out.
	#

	DMTC0(zero, ASMHNDLR_REG)	# Zap the nofault stuff.  Memory's OK

	DMTC0(ra, RA_REG)		# Save ra
	jal	pod_gprs_to_fprs	
	nop

	dli	k0, GDA_ADDR
	lw	k0, G_MASTEROFF(k0)	# Read the master spnum
	dli	k1, EV_SPNUM		# Load the SPNUM resource addr
	ld	k1, 0(k1)		# Read our spnum
	nop				# (LD)
	andi	k1, EV_SPNUM_MASK	# Only look at valid bits
	bne	k0, k1, 10f		# Skip forward if we're not master
	nop				# (BD)

	dli	k0, GDA_ADDR		# Reload GDA address
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

	dla	a0, nmi_msg
	j	pod_handler
	li	a1, EVDIAG_NMI		# (BD)

	# If we get here, we're a lame slave processor, so we just
	# jump into the the prom slave loop.
10:	#

	# If the cheese-meister UART flag is set, turn on the CC UART, and
	# jump to pod_handler instead of the slave loop.

	dli	k0, GDA_ADDR		# Reload GDA address
	lw	k1, G_VDSOFF(k0)	# Load the flag
	nop				# (LD)
	andi	k1, VDS_NOARB		# Is noarb set?
	beqz	k1, 14f			# If flag is zero, do nothing.
	nop				# (BD)

	SCPRINT("Galles mode on.\r\n")

	# Set-up the BSR	
	li	a0, BS_USE_EPCUART
	SET_BSR(a0)

	dla	a0, nmi_msg
	j	pod_handler
	li	a1, EVDIAG_NMI 		# (BD)

14:
	SCPRINT("Galles mode off.\r\n")
	li	a0, BS_SLAVE
	SET_BSR(a0)
	j	prom_slave_nmi
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
	DMFC0(a0, NOFAULT_REG)		# Grab fault vector 
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
	dla	a0, panic_msg
	jal	pod_puts
	nop

	/*
	 * Call the standard POD handler
	 */

	li	a1, EVDIAG_PANIC
	j	pod_handler
	move	a0, s1			# (BD) Put the message in a0

	j	flash_cc_leds
	ori	a0, zero, FLED_IMPOSSIBLE1
	nop
	END(bev_panic)

LEAF(notimplemented)
	dla	a1, notimpl_msg
	j	bev_panic
	or	a0, zero, FLED_NOTIMPL
	END(notimplemented)	


LEAF(pod_wrapper)
	# If the memory access failed, there will be an
	# address error on MyRequest.  Clear this.
	dli	k0, EV_CERTOIP
	li	k1, CC_ERROR_MY_ADDR 
	sd	k1, 0(k0)
	nop

	GET_BSR(a0)
	li	a1, BS_USE_EPCUART
	not	a1
	and	a0, a1
	SET_BSR(a0)

	SCPRINT("Took some wacky exception.")

	dla	a0, nmi_exc_msg
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

	dli	a0, EV_IGRMASK
	sd	zero, 0(a0)
	dli	a0, EV_ILE
	sd	zero, 0(a0)
	dli	a0, EV_CIPL124
	sd	zero, 0(a0)
	dli	a0, EV_CERTOIP
	sd	zero, 0(a0)

	EV_GET_SPNUM(t0, zero)		# Get slot number
	EV_GET_CONFIG(t0, EV_A_ERROR_CLEAR, zero)

	j	s7
	nop
	END(reinitialize)

/*
 * ip21prom_restart
 *	Called from the kernel when the system wants to 
 *	reboot or return the PROM monitor.  This routine
 *	sets up a stack and calls the restart C routine.
 *	It assumes that we don't want to rearbitrate for
 * 	boot mastership, that no diags should be run, and
 *	that memory is already initialized.
 */

LEAF(ip21prom_restart)

	jal	pon_invalidate_IDcaches	# Invalidate I&D cache at the same time
	nop

	jal	pon_test_Icaches
	nop

	GOTO_CHANDRA_MODE		# icache is inited, use it

	jal	reinitialize		# Reset the basic IP21 registers
	nop				# (BD)

	# Reinitialize the caches
	#
	SCPRINT("Initing caches...")
	jal	set_cc_leds
	ori	a0, zero, PLED_CKSCACHE1 # (BD)
	jal	pon_setup_stack_in_dcache
	nop				# (BD)
	jal	pon_invalidate_IDcaches
	nop				# (BD)
	jal	gcache_invalidate
/* 	DEBUG_DUMP_TAGS(0x9000000000000020) */
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
	dli	sp, POD_STACKADDR
	subu	v0, 8
	daddu	sp, v0

	# Reinitialize the EPC
	#
	SCPRINT("Reiniting IO4")
 	dla	a0, EVCFGINFO_ADDR
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

1:	b	1b
	nop
	END(ip21prom_restart)

/*
 * ip21prom_reslave
 *	Reinitializes a couple parts of the system and then
 *	switches into slave mode.  Called by non-bootmaster
 *	processors when the kernel is shutting down.
 */

LEAF(ip21prom_reslave)
	jal	pon_invalidate_IDcaches	# Invalidate I&D cache at the same time
	nop

	GOTO_CHANDRA_MODE		# icache is inited, use it

	jal	reinitialize		# Reset the basic IP21 registers
	nop				# (BD)

	j	prom_slave		# Jump back to the slave loop
	nop				# (BD)
	END(ip21prom_reslave)


/*
 * ip21prom_podmode
 *	Reinitializes the prom and jumps to podmode.  Allows code
 *	like symmon or the secondary prom to get back to home base.
 */

LEAF(ip21prom_podmode)
	jal	pon_invalidate_IDcaches	# Invalidate I&D cache at the same time
	nop

	GOTO_CHANDRA_MODE		# icache is inited, use it

	jal	reinitialize		# Reinitialize the basic IP21 registers
	nop				# (BD)

	GET_BSR(t0)
	ori	t0, zero, BS_CCUART_INITED
	SET_BSR(t0)

	dla	a0, podhdlr_msg		# Load message pointer into a0
	j	pod_handler		# Jump back to POD mode
	li	a1, EVDIAG_DEBUG		# (BD)
	END(ip21prom_podmode)

LEAF(ip21prom_epcuart_podmode)
	jal	pon_invalidate_IDcaches	# Invalidate I&D cache at the same time
	nop

	GOTO_CHANDRA_MODE		# icache is inited, use it

	jal	reinitialize
	nop

	li	t0, BS_CCUART_INITED|BS_USE_EPCUART|BS_POD_MODE|BS_NO_DIAGS
	
	SET_BSR(t0)

	j	prom_master
	nop

	END(ip21prom_epcuart_podmode)

LEAF(c_ip21prom_flash_leds)

	GOTO_CHANDRA_MODE

	j	ip21prom_flash_leds
	nop

	END(c_ip21prom_flash_leds)

/*
 * In order to avoid trashing the registers saved in the fprs,
 * we need to check for an asm fault handler immediately when
 * we enter each of the exception handlers.  If one isn't set,
 * we save the ra and then proceed.  This stuff is written as
 * macro so that we won't lose the ra.
 */

#define COMMON_CODE	 		\
	DMFC0(k0, C0_SR) ;		\
	dli	k1, PROM_SR ;		\
	or	k0, k1 ;		\
	DMTC0(k0, C0_SR) ; 		\
	DMFC0(k0, ASMHNDLR_REG) ;	\
	DMTC0(zero, ASMHNDLR_REG) ;	\
	beqz	k0, 95f ;		\
	nop ;				\
	j	k0 ;			\
	nop ;				\
95:	DMTC0(ra, RA_REG)

	.align	12
	.globl	trap_table
trap_table:				/* must be page aligned */

LEAF(bev_tlbrefill)			/* TrapBase + 0x000 */
	COMMON_CODE
	jal	pod_gprs_to_fprs
	nop				# (BD)	
	dla	a1, tlb_msg
	j	bev_panic
	or	a0, zero, FLED_UTLBMISS	
	END(bev_tlbrefill)

	.align	10

LEAF(bev_ktlbrefill)			/* TrapBase + 0x400 */
	COMMON_CODE
	jal	pod_gprs_to_fprs
	nop				# (BD)
	dla	a1, ktlb_msg
	j	bev_panic
	or	a0, zero, FLED_KTLBMISS
	END(bev_ktlbrefill)

	.align	10

        JUMP(bev_ktlbrefill)		/* TrapBase + 0x800 */
	.align	10

LEAF(bev_general)			/* TrapBase + 0xc00 */
	COMMON_CODE
	jal	pod_gprs_to_fprs
	nop				# (BD)

	DMFC0(t1, C0_CAUSE)
	li	t0, CAUSE_EXCMASK
	nop
	
	and	t0, t1
	li	t1, EXC_BREAK
	
	beq	t0, t1, 2f		# If break point jump to break handler
	nop				# (BD)
	
	dla	a1, general_msg
	j	bev_panic
	or	a0, zero, FLED_GENERAL	# (BD)

2:	j	break_handler
	nop
	END(bev_general)

/*
 * Various messages and text strings.
 */
		.data
panic_msg:	.asciiz "\r\nPanic in IP21 PROM: "
epc_msg:	.asciiz "\r\nEPC: "
cause_msg:	.asciiz "\r\nCAUSE: "
sr_msg:		.asciiz "\r\nSR: "
badvaddr_msg:	.asciiz "\r\nBADVADDR: "
podhdlr_msg:	.asciiz "\r\nJumping to POD mode.\r\n"
tlb_msg: 	.asciiz "TLB miss\r\n"
ktlb_msg:	.asciiz	"KTLB miss exception\r\n"
general_msg:	.asciiz "General exception\r\n"
notimpl_msg:	.asciiz "jump to unimplemented vector\r\n"
watch_msg:	.asciiz	"Watch\r\n"
pod_msg:	.asciiz	"Jumping to POD mode.\r\n"
nmi_msg:	.asciiz "\r\n\n*** Non-maskable interrupt occurred. \r\n\n"
nmi_exc_msg:	.asciiz "\r\n\n*** Non-maskable intr/exception handler \r\n"

