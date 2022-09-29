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
#include <sys/SN/gda.h>
#include <ml.h>
#include <hub.h>
#include "ip27prom.h"
#include "io6prom_private.h"

#ifdef SABLE
#include <sys/i8251uart.h>	/* XXX */
#endif

#define BSSMAGIC 0xfeeddead

/*
 * entry - IO6prom
 * 	All CPUs are running a single copy of this code.
 * 	set up stack for each CPU in its own node memory.
 * 	if slave, jump to sysinit
 * 	if global master
 *		Clear BSS and stack space, jump to sysinit.
 */

LEAF(entry)
	.set	noreorder

	move	s4, a0		# Diagnostics mode

	jal	get_nasid
	nop

	HUB_LED_SET_REMOTE(v0, 0, 0x55)
	HUB_LED_SET_REMOTE(v0, 1, 0xaa)

	GET_NASID_ASM(a0)	# Pass in our nasid
	jal	tlb_setup	# Set up our special TLB entry
				# uses regs s0-s3
	nop			# BD

#if defined(SABLE) && defined(FAKE_PROM)

        # Only let one processor run the master code.
        dla      k0, sable_mastercounter

        # Increment the counter
1:      ll      a0, (k0)
        add     k1, a0, 1
        sc      k1, (k0)
        beqz    k1, 1b;
        nop

	beqz	a0, 2f
	nop

	# Even the slaves need a stack pointer.
        jal     IO6prom_stack_addr
        nop
        move    sp, v0

	j	ip27_slaveloop
	nop


2:

/* Do some ip27prom specific init now for sable. When the real IP27PROM
   really launches us, delete these 2 lines. */

	jal 	ip27prom_asminit
	nop			# BD

#endif /* SABLE && FAKE_PROM */

/* Set CALIAS size to 8k to contain the SPB. */

	dli	k1, LOCAL_HUB(PI_CALIAS_SIZE)
	dli	k0, PI_CALIAS_SIZE_8K
	sd	k0, 0(k1)

	jal	IO6prom_stack_addr
	nop
	move	sp, v0

	jal	hub_scratch_init 	# so things like hub_barrier() will work
	nop

	.set	reorder
	jal	clear_prom_ram

	move	a0, s4			# restore diag mode arg
	j	sysinit
	END(entry)


LEAF(hub_scratch_init)

	/*
	 * Clear the MD_PERF_CNTx registers so the PROM can use them
	 * as scratch regs (see comment at beginning of hub.c).
	 * XXX - WARNING!!!!! do not clear if ip27prom stores required info in these
	 */

	sd	zero, LOCAL_HUB(MD_PERF_SEL )	/* Disable counting */
	sd	zero, LOCAL_HUB(MD_PERF_CNT0)	/* Clear all */
	sd	zero, LOCAL_HUB(MD_PERF_CNT1)
	sd	zero, LOCAL_HUB(MD_PERF_CNT2)
	sd	zero, LOCAL_HUB(MD_PERF_CNT3)
	sd	zero, LOCAL_HUB(MD_PERF_CNT4)
	sd	zero, LOCAL_HUB(MD_PERF_CNT5)
	j 	ra

	END(hub_scratch_init)

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

	jal	IO6prom_stack_addr
	move	sp, v0
        sreg	sp,_fault_sp
        jal     main
        # if main fails fall through and re-init
1:
	# clear_prom_ram assumes that the SP is already setup
	# if we come here from any of the beq 1f above, the 
	# stack will not be setup. Rewind the stack here.

        jal     IO6prom_stack_addr
	nop
	move	sp, v0
        sreg	sp,_fault_sp

        li      s0,PROMOP_IMODE
        j       arcs_common
        END(EnterInteractiveMode)

	.data
e_msg:	.asciiz	"In EnterInteractiveMode in IO6\r\n"
l_msg: 	.asciiz "Doing light cleanup\r\n"
w_msg:  .asciiz "Reinitializing the whole shebang\r\n"

	.text
LEAF(arcs_common)
        .set    noreorder
	LI	a0, IO6PROM_SR & ~(SR_IE)
        DMTC0(a0, C0_SR)              	# no interrupts
	nop
        .set    reorder
	move	s3, s0			# make addnl copies as s0 is getting trashed
        jal     clear_prom_ram          # clear_prom_ram initializes sp
        move    a0,zero
        jal     init_prom_soft          # initialize saio and everything else

	move	s0, s3 			# restore s0

	li	t0,PROMOP_IMODE
        bne     s0,t0,1f
        jal     main
        j       never                   # never gets here
1:
	li	t0,PROMOP_HALT
        bne     s0,t0,1f
        jal     halt
        j       never                   # never gets here
1:
	li	t0,PROMOP_POWERDOWN
        bne     s0,t0,1f
        jal     powerdown
        j       never                   # never gets here
1:
	li	t0,PROMOP_RESTART
        bne     s0,t0,1f
        jal     restart
        j       never                   # never gets here
1:
	li	t0,PROMOP_REBOOT
        bne     s0,t0,never
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
	LI	v0, IO6PROM_SR & ~(SR_IE)
        DMTC0(v0,C0_SR)                 # back to a known sr
	nop
        DMTC0(zero,C0_CAUSE)            # clear software interrupts
	nop
        .set    reorder

        lw      v0,bssmagic             # Check to see if bss valid
        bne     v0,BSSMAGIC,_init       # If not, do a hard init

	jal	IO6prom_stack_addr
	move	sp, v0
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
	LI	v0, IO6PROM_SR & ~(SR_IE)
        DMTC0(v0,C0_SR)
	nop

        jal     IO6prom_stack_addr
        nop
        move    sp, v0

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
        LA	a1,_end			# Up to stack value - NO
/*	PTR_ADDU a1, IO4STACK_SIZE	 */

	/* If SN0, v0/v1 must have the phys addrs */
#ifdef SN0
	move	s2,	ra		# store ra in s2
	move	s0,	a1
	jal	kvtophys

	move	s1,	v0
	move	a0,	s0
	jal	kvtophys

	move	v1,	v0
	move	v0,	s1
	move	ra,	s2		# restore ra from s2
#else
	LI	t0, 0x9fffffff
	and	v0, a0, t0
	and	v1, a1, t0
#endif /* SN0 */

#if 0 
/* may be needed to check why the above piece of code does
   not work with BD slots */

	LI	t0, PHYS_TO_K0(0x1300000) /* Flash buf base */
	sd	v0, 0(t0)
	sd	v1, 8(t0)
#endif

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

	 * SN0 - the sp is already initted for each cpu.
	 * Do it again. Because when we do a init we need
	 * the same stack pointer we started with.
	 * just allocate space for the various things.
         */

	PTR_SUBU sp, (4*BPREG)
        sreg	sp,_fault_sp		# base of fault stack
        PTR_SUBU sp,EXSTKSZ		# top 1/4 of stack for fault handling
        
        j       ra
        END(clear_prom_ram)



/* Clear all the TLB entries, but leave TLB entry 0 alone.
 * TLB entry 0 is used by graphics to access the graphics
 * hardware.  Also leave the top entry alone.  We use that
 * to map the prom text!  Finally, tlb entries 60, 61, and 62 are used by the
 * IP27prom.
 */
LEAF(init_tlb)
	.set	noreorder
	DMTC0(zero,C0_TLBLO_0)
	DMTC0(zero,C0_TLBLO_1)
	LI      v0,K0BASE&TLBHI_VPNMASK
	DMTC0(v0,C0_TLBHI)             # invalidate entry
	move    v0,zero
	addiu	v0,1
	nop
1:
	DMTC0(v0,C0_INX)
	nop
	c0      C0_WRITEI

	addiu   v0,1
#ifdef SN0XXL
	blt     v0,NTLBENTRIES - 8,1b	# entries 56..59 used by io6prom
#else
	blt	v0,NTLBENTRIES - 7,1b	# entries 57, 58, 59 used by io6prom
#endif
	nop

	j       ra
	nop                             # BDSLOT
	.set    reorder
	END(init_tlb)

#if SABLE
/*
 * Routine hubuart_init
 *	Initializes the Hub chip UART by writing three NULLS to
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

LEAF(hubuart_init) 
	LI	a1, LOCAL_HUB(MD_UREG0_0)
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
	END(hubuart_init)

#endif	/* SABLE */

LEAF(prom_flash_leds)
	LI	t1, IP27PROM_FLASHLEDS	# Go away forever.
	j	t1
	END(prom_flash_leds)



/*
 * IO6prom_stack_addr -> returns a local stack depending on the node
 * and cpu.
 */	
LEAF(IO6prom_stack_addr)
	LA	v0, Stack
	daddiu	v0, IO6PROM_STACK_SIZE
	dsubu	v0, 16

	j	ra

	END(IO6prom_stack_addr)

/*
 * HACK - This lets us use hubuartprintf before we really define such
 * a thing in the real code.
 */
LEAF(hubprintf)
	j	printf
	END(hubprintf)

/*
 * stub for IO6prom. real one only lives in IP27prom
 */
LEAF(db_printf)
      j       printf
      END(db_printf)

/***********************************************************************
*
* Data Area
*
*/
	.data

	BSS(bssmagic,4)

