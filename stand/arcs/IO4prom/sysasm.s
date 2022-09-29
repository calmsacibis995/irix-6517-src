/***********************************************************************\
*	File:		sysasm.s					*
*									*
*	This file contains miscellaneous assembly language routines	*
*	which are called at various points.				*
*									*
\***********************************************************************/

#include <fault.h>
#include <asm.h>
#include <regdef.h>
#include <sys/immu.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/EVEREST/gda.h>
#include <ml.h>

#ifdef SABLE
#include <sys/i8251uart.h>
#endif

#define BSSMAGIC 0xfeeddead

/*
 * entry
 *	Clears BSS and stack space, sets up the stack, 
 *	and jumps to sysinit.
 */

LEAF(entry)
	.set	noreorder
#if R4000
	li	v0, SR_CU1|SR_FR|SR_KX
	mtc0	v0, C0_SR		# Put SR into known state
	nop; nop; nop; nop

	/*
	 * We may have been loaded using the 32 bit io4prom. Make
	 * sure we're running at the appropriate 64 bit address.
	 */
	LA	v0,1f
	j	v0
	nop
1:
	mtc0	zero, C0_CAUSE		# Clear software interrupts
	nop
	mtc0	zero, C0_WATCHLO	# Clear all watchpoints
	nop
	mtc0	zero, C0_WATCHHI	#
	nop
#endif	/* R4000 */
#if TFP && SABLE
	LI	v0, SR_CU1|SR_FR
	dmtc0	v0, C0_SR		# Put SR into known state
	NOP_MTC0_HAZ

	LI	t0, GDA_ADDR
	LI	t1, GDA_MAGIC		# Load actual magic number
	sw	t1, G_MAGICOFF(t0)	# Store magic number
	sw	zero, G_PROMOPOFF(t0)	# Set actual operation

	jal	ccuart_init
	nop

	LI	t0, 1
	LA	t1, sk_sable		# set sable flag for libsk
	sw	t0, 0(t1)

	LA	t1, sc_sable		# set sable flag for libsc
	sw	t0, 0(t1)
#endif	/* TFP && SABLE */

#if R10000 && SABLE
	LI	t0, GDA_ADDR
	LI	t1, GDA_MAGIC		# Load actual magic number
	sw	t1, G_MAGICOFF(t0)	# Store magic number
	sw	zero, G_PROMOPOFF(t0)	# Set actual operation

	jal	ccuart_init
	nop

	LI	t0, 1
	LA	t1, sk_sable		# set sable flag for libsk
	sw	t0, 0(t1)

	LA	t1, sc_sable		# set sable flag for libsc
	sw	t0, 0(t1)
#endif
	.set	reorder

	# Check to see whether we should do a standard startup or
	# go through one of the alternate startup vectors. 
	# 
	LI	t0, GDA_ADDR
	lw	t1, G_MAGICOFF(t0)	# Load contents of magic number
	LI	t2, GDA_MAGIC		# Load actual magic number
	bne	t1, t2, 1f		# 

	lw	t1, G_PROMOPOFF(t0)	# Get actual operation
	blez	t1, 1f			# Skip if op <= 0 
	subu	t2, t1, 5		# Skip if op > 5	
	bgtz	t2, 1f			

	j	arcs_common		# Call arcs_common to do actual work
1:
#if	SABLE
	jal	ccuart_init
#endif
	jal	clear_prom_ram
	j	sysinit
	END(entry)


/*******************************************************************/
/* 
 * ARCS Prom entry points
 */

LEAF(Halt)
        li      s0,PROMOP_HALT
        j       arcs_common
        END(Halt)

LEAF(PowerDown)
        li      s0,PROMOP_POWERDOWN
        j       arcs_common
        END(PowerDown)

LEAF(Restart)
        li      s0,PROMOP_RESTART
        j       arcs_common
        END(Restart)

LEAF(Reboot)
        li      s0,PROMOP_REBOOT
        j       arcs_common
        END(Reboot)

/*  EnterInteractive mode goes to prom menu.  If called from outside the
 * prom or bssmagic, or envdirty is true, do full clean-up, else just
 * re-wind the stack to keep the prom responsive.
 */
LEAF(EnterInteractiveMode)
	# Print a message telling us we got here
	#
        LA	t0, _ftext
        LA	t1, _etext
        sgeu    t2,ra,t0
        sleu    t3,ra,t1
        and     t0,t2,t3
        beq     t0,zero,1f

        # check if bssmagic was trashed
	# form a 32 bit sign-extended constant to match
	# the sign-extended result of the lw above.
	# (The Ragnarok compiler may not treat the 'li' macro
	# properly, so make it explicit.
        lw	v0,bssmagic		# Check to see if bss valid
	lui	t0,(BSSMAGIC >> 16)
	ori	t0,(BSSMAGIC & 0xffff)
	bne     v0,t0,1f		# If not, do full init

        # check if envdirty true (program was loaded)
        lw      v0,envdirty
        bne     v0,zero,1f

        LA	sp,_end
	and	sp,~0xf			# make sure it's aligned properly
	PTR_ADDU sp,IO4STACK_SIZE-(4*BPREG)	# reset the stack
        sreg	sp,_fault_sp
        jal     main
        # if main fails fall through and re-init
1:
        li      s0,PROMOP_IMODE
        j       arcs_common
        END(EnterInteractiveMode)

	.data
e_msg:	.asciiz	"In EnterInteractiveMode in IO4\r\n"
l_msg: 	.asciiz "Doing light cleanup\r\n"
w_msg:  .asciiz "Reinitializing the whole shebang\r\n"

	.text
LEAF(arcs_common)
        .set    noreorder
	LI	a0, NORMAL_SR & ~(SR_IE)
        DMTC0(a0, C0_SR)              	# no interrupts
	nop
        .set    reorder
        jal     clear_prom_ram          # clear_prom_ram initializes sp
        move    a0,zero
        jal     init_prom_soft          # initialize saio and everything else

        bne     s0,PROMOP_IMODE,1f
        jal     main
        j       never                   # never gets here
1:
        bne     s0,PROMOP_HALT,1f
        jal     halt
        j       never                   # never gets here
1:
        bne     s0,PROMOP_POWERDOWN,1f
        jal     powerdown
        j       never                   # never gets here
1:
        bne     s0,PROMOP_RESTART,1f
        jal     restart
        j       never                   # never gets here
1:
        bne     s0,PROMOP_REBOOT,never
        jal     reboot

never:  j       _exit                   # never gets here
        END(arcs_common)

/*
 * _exit()
 *
 * Re-enter prom command loop without initialization
 * Attempts to clean-up things as well as possible while
 * still maintaining things like current enabled consoles, uart baud
 * rates, and environment variables.
 */
LEAF(_exit)
        .set    noreorder
	LI	v0, NORMAL_SR	
        DMTC0(v0,C0_SR)                 # back to a known sr
	nop
        DMTC0(zero,C0_CAUSE)            # clear software interrupts
	nop
        .set    reorder

        lw      v0,bssmagic             # Check to see if bss valid
        bne     v0,BSSMAGIC,_init       # If not, do a hard init

        LA	sp,_end
	and	sp,~0xf			# make sure it's aligned properly
	PTR_ADDU sp,IO4STACK_SIZE-(4*BPREG)  # reset the stack
        sreg	sp,_fault_sp
        PTR_SUBU sp,EXSTKSZ		# top 1/4 of stack for fault handling
        jal     config_cache            # determine cache sizes
        jal     flush_cache             # flush cache
        jal     enter_imode
1:      b       1b                      # never gets here
        END(_exit)

/*
 * notimplemented -- deal with calls to unimplemented prom services
 */
NOTIMPFRM=(4*BPREG)+BPREG+BPREG
NESTED(notimplemented, NOTIMPFRM, zero)
        PTR_SUBU sp,NOTIMPFRM
        beqz    fp,1f
        move    a0,fp
        move    fp,zero
        j       a0
1:
        LA	a0,notimp_msg
        jal     printf
        jal     EnterInteractiveMode
        END(notimplemented)

        .data
notimp_msg:
        .asciiz "ERROR: call to unimplemented prom routine\n"
        .text

/*******************************************************************/
/*
 * Subroutines
 *
 */

/*
 * _init -- reinitialize prom and reenter command parser
 */
LEAF(_init)
        .set    noreorder
	LI	v0, NORMAL_SR	
        DMTC0(v0,C0_SR)
	nop
        .set    reorder
        jal     clear_prom_ram          # clear_prom_ram initializes sp
        move    a0,zero
        jal     init_prom_soft
        jal     main
        jal     EnterInteractiveMode    # shouldn't get here
        END(_init)

/*
 * clear_prom_ram -- config and init cache, clear prom bss.  Note that
 *	This routine assumes that BSS follows the text and data sections.
 */

INITMEMFRM=(4*BPREG)+BPREG
NESTED(clear_prom_ram, INITMEMFRM, zero)
#ifndef SABLE
        LA	a0,_fbss                # clear bss and stack 
        LA	a1,_end			# Up to stack value 
	PTR_ADDU a1, IO4STACK_SIZE	

	LI	t0, 0x9fffffff
	and	v0, a0, t0
	and	v1, a1, t0
	or	v0, K0BASE
	or	v1, K0BASE

	/* Clear 7 bytes, then align */

	sb	zero,0(v0)
	sb	zero,1(v0)
	sb	zero,2(v0)
	sb	zero,3(v0)
	sb	zero,4(v0)
	sb	zero,5(v0)
	sb	zero,6(v0)
	sb	zero,7(v0)

	daddiu	v0,7
	and	v0,~7

        .set    noreorder
1:      PTR_ADDU v0,64			# clear 64 bytes at a time
	sd	zero, -8(v0)
	sd	zero, -16(v0)
	sd	zero, -24(v0)
	sd	zero, -32(v0)
	sd	zero, -40(v0)
	sd	zero, -48(v0)
	sd	zero, -56(v0)
        bltu    v0,v1,1b
        sd      zero,-64(v0)            # (BD) Do the final store 

	.set	reorder
#endif	/* !SABLE */

XLEAF(init_prom_ram)
        /*
         * Remember that bss is now okay
         */
        li      v0,BSSMAGIC
        sw      v0,bssmagic

        /*
         * Initialize stack
         */
        LA	sp,_end
	and	sp,~0xf			# make sure it's aligned properly
	PTR_ADDU sp, IO4STACK_SIZE - (4*BPREG)
        sreg	sp,_fault_sp		# base of fault stack
        PTR_SUBU sp,EXSTKSZ		# top 1/4 of stack for fault handling
        
        j       ra
        END(clear_prom_ram)



/* Clear all the TLB entries, but leave TLB entry 0 alone.
 * TLB entry 0 is used by graphics to access the graphics
 * hardware.
 */
LEAF(init_tlb)
	.set	noreorder
#if TFP
	j	flush_tlb		# j not jal
	li	a0,0			# BDSLOT (flush all entries)
#else
	DMTC0(zero,C0_TLBLO_0)
	DMTC0(zero,C0_TLBLO_1)
	LI      v0,K0BASE&TLBHI_VPNMASK
	DMTC0(v0,C0_TLBHI)             # invalidate entry
	move    v0,zero
	addiu	v0,1
	nop
1:
	DMTC0(v0,C0_INX)
	NOP_0_3
	c0      C0_WRITEI

	addiu   v0,1
	blt     v0,NTLBENTRIES,1b
	nop

	j       ra
	nop                             # BDSLOT
#endif	/* TFP */
	.set    reorder
	END(init_tlb)

LEAF(set_leds)
	.set	noreorder
	LI	t0, EV_LED_BASE
	j	ra
	sd	a0, 0(t0)
	END(set_leds)


/*
 * load_lwin_half
 *	Given a region index, these routines read either a halfword or
 * 	a word from the given large window.  They are used primarily
 *	for manipulating the flash eprom.
 * 
 * On entry:
 *	a0 = region #.
 *	a1 = offset within region.
 */

LEAF(load_lwin_half)
	.set 	noreorder
	mfc0	t0, C0_SR
	nop; nop; nop
	or	t0, SR_KX	# make sure we're in extended mode
	mtc0	t0, C0_SR
	nop; nop; nop
	daddi	a0, 0x10	# Convert to physical offset
	dsll	a0, 30		# Shift it up
	dadd	a0, a1		# Merge in the offset
	lui	a1, 0x9000
	dsll	a1, 16		# Build the uncached xkphys address
	dsll	a1, 16
	dadd	a0, a1		# Make LWIN into a real address
	j	ra		# Return
	lhu	v0, 0(a0)	# (BD) Perform the actual read
	END(load_lwin_half)

/*
 * store_lwin_half
 *	Given the region of a large window, this routine writes a
 *	halfword to an offset in the large window.  Used primarily
 *	for manipulating the flash eprom.
 *
 * On entry:
 *	a0 = region #.
 *	a1 = offset.
 *	a2 = value to write.
 */

LEAF(store_lwin_half)
	.set	noreorder
	mfc0	t0, C0_SR
	nop; nop; nop
	or	t0, SR_KX	# make sure we're in extended mode
	mtc0	t0, C0_SR
	nop; nop; nop
	daddi	a0, 0x10
	dsll	a0, 30
	dadd	a0, a1
	lui	a1, 0x9000
	dsll	a1, 16
	dsll	a1, 16
	dadd	a0, a1
	j	ra
	sh	a2, 0(a0)
	END(store_lwin_half)

#if SABLE
/*
 * Routine ccuart_init
 *	Initializes the CC chip UART by writing three NULLS to
 *	get it into a known state, doing a soft reset, and then
 *	bringing it into ASYNCHRONOUS mode.  
 * 
 * Arguments:
 *	None.
 * Returns:
 * 	Nothing.
 * Uses:
 *	a0, a1.
 */
#define		CMD	0x0
#define 	DATA	0x8

LEAF(ccuart_init) 
	LI	a1, EV_UART_BASE
	sd	zero, CMD(a1)		# Clear state by writing 3 zero's
	nop
	sd	zero, CMD(a1)
	nop
	sd	zero, CMD(a1)
	LI	a0, I8251_RESET		# Soft reset
	sd	a0, CMD(a1)
	LI	a0, I8251_ASYNC16X | I8251_NOPAR | I8251_8BITS | I8251_STOPB1
	sd	a0, CMD(a1)
	LI	a0, I8251_TXENB | I8251_RXENB | I8251_RESETERR
	j	ra 
	sd	a0, CMD(a1)		# (BD)
	END(ccuart_init)

#endif	/* SABLE */

/*
 * my_delay
 *	A simple, tight delay routine based on the RTC.
 */

LEAF(my_delay)
	.set	noreorder
	LI	a1, 50			# 50 cycles per microsecond
	dmultu	a0, a1			# Calculate number of cycles to wait
	LI	a2, EV_RTC
	ld	a2, 0(a2)		# Get the current RTC value
	mflo	a0			# Grab the result
	addu	a0, a2			# Add offset to delay time
	LI	a2, EV_RTC		#
1:
	ld	a1, 0(a2)		# Read the RTC
	dsubu	a1, a0			# Subtract delay time from current
	blez	a1, 1b			# Wait while RTC is less
	nop				# (BD)
	j	ra
	nop 				# (BD)
	END(my_delay)

LEAF(prom_flash_leds)
	LI	t1, EV_PROM_FLASHLEDS	# Go away forever.
	j	t1
	nop
	END(prom_flash_leds)

/***********************************************************************
*
* Data Area
*
*/
	.data

	BSS(bssmagic,4)
