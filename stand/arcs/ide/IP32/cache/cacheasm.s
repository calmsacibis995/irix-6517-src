/*
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.1 $"

#include <asm.h>
#include <regdef.h>
#include <sys/sbd.h>
#include "cache.h"


/* sd_hitinv(vaddr): Secondary Data cache HIT INValidate.
 * invalidates secondary and primary cache blocks if virtual address is valid
 * a0: virtual address to invalidate if present */
LEAF(sd_hitinv)
	.set	noreorder
	cache	CACH_SD|C_HINV, 0(a0)
	nop
	nop
	j       ra
	nop
	.set	reorder
	END(sd_iwbinv)

/* 
 * Execute the cache instructions below on the data cache
 * slot specified by vaddr (in a0).  vaddr must be in cached,
 * unmapped space, (K0SEG).  No error checking is done on
 * this address.  If applicable, the routines return the value
 * of the SR_CH bit in the diagnostics portion of the status
 * register, which is non-zero if the cache instruction 'hit'
 * secondary, 0 if it 'missed'.
 */

/* pd_iwbinv(vaddr): Primary Data cache Index WriteBack INValidate.
 * a0: virtual address to use as index */
LEAF(pd_iwbinv)
	.set	noreorder
	cache	CACH_PD|C_IWBINV, 0(a0)
	nop
	nop
	j       ra
	nop
	.set	reorder
	END(pd_iwbinv)


/* pd_HINV(vaddr): Primary Data cache Hit INValidate.
 * a0: virtual address
 * Return the CH status bit to indicate hit/miss of secondary. */
LEAF(pd_HINV)
	.set	noreorder
	cache	CACH_PD|C_HINV, 0(a0)
	nop				# wait for results to get thru pipe
	nop
	mfc0	v0, C0_SR		# check if we hit the 2ndary cache
	nop
	nop
	and	v0, SR_CH
	j       ra
	nop
	.set	reorder
	END(pd_HINV)


/* pi_HINV(vaddr): Primary Instruction cache Hit INValidate.
 * a0: virtual address
 * Return the CH status bit to indicate hit/miss of secondary. */
LEAF(pi_HINV)
	.set	noreorder
	cache	CACH_PI|C_HINV, 0(a0)
	nop				# wait for results to get thru pipe
	nop
	mfc0	v0, C0_SR		# check if we hit the 2ndary cache
	nop
	nop
	and	v0, SR_CH
	j       ra
	nop
	.set	reorder
	END(pi_HINV)


/* pd_HWBINV(vaddr): Primary Data cache Hit Writeback Invalidate.
 * a0: virtual address
 * Return the CH status bit to indicate hit/miss of secondary. */
LEAF(pd_HWBINV)
	.set	noreorder
	cache	CACH_PD|C_HWBINV, 0(a0)
	nop				# wait for results to get thru pipe
	nop
	mfc0	v0, C0_SR		# check if we hit the 2ndary cache
	nop
	nop
	and	v0, SR_CH
	j       ra
	nop
	.set	reorder
	END(pd_HWBINV)


/* sd_HWB(vaddr): Secondary Data cache Hit Writeback.
 * a0: virtual address
 * Return the CH status bit to indicate hit/miss of secondary. */
LEAF(sd_HWB)
	.set	noreorder
	cache	CACH_SD|C_HWB, 0(a0)
	nop				# wait for results to get thru pipe
	nop
	mfc0	v0, C0_SR		# check if we hit the 2ndary cache
	nop
	nop
	and	v0, SR_CH
	j       ra
	nop
	.set	reorder
	END(sd_HWB)


/* read_tag(WhichCache, address, &deststruct)
 * WhichCache == PRIMARYD (1), PRIMARYI (2), SECONDARY (3)
 * destruct is type struct tag_regs.
 */
LEAF(read_tag)
	li	v0, 0		# success by default
	li	t0, K0BASE
	or	a1, t0		# a1: addr of tag to fetch in cached space
	li	t0, PRIMARYI
	bltu	a0, t0, rprimd	# fetch primary data cache tag?
	beq	a0, t0, rprimi	# fetch primary instruction cache tag?
	.set	noreorder
	cache	CACH_SD|C_ILT, 0(a1)	# nope, fetch secondary cache tag
	.set	reorder

getout:	.set	noreorder
	nop
	nop
	mfc0	t0, C0_TAGLO
	# DO NOT READ C0_TAGHI IN CURRENT IMPLEMENTATION OF R4K!
	nop
	nop
	sw	t0, 0(a2)	# taglo is 1st uint in tag struct
	# sw	t1, 4(a2)	# taghi is 2nd
	.set	reorder
	j	ra

rprimi:	.set	noreorder
	cache	CACH_PI|C_ILT, 0(a1)	# fetch primary instruction cache tag
	.set	reorder
	b	getout

rprimd:	.set	noreorder
	cache	CACH_PD|C_ILT, 0(a1)	# fetch primary data cache tag
	.set	reorder
	b	getout

	END(read_tag)


/*
 * write_tag(WhichCache, address, &srcstruct)
 * WhichCache == PRIMARYD (1), PRIMARYI (2), SECONDARY (3)
 */

LEAF(write_tag)
	lw	t0, 0(a2)	# a2 pts to taglo: 1st uint in tag struct
	# lw	t1, 4(a2)	# taghi is 2nd

	.set	noreorder
	mtc0	t0,C0_TAGLO
	# DO NOT WRITE C0_TAGHI IN CURRENT IMPLEMENTATION OF R4K!
	li	v0, 0		# success by default
	li	t0, K0BASE
	or	a1, t0		# a1: addr of tag to set in cached space
	.set	reorder		# this is at least 3 clocks

	li	t0, PRIMARYI
	bltu	a0, t0, wprimd	# set primary data cache tag?
	beq	a0, t0, wprimi	# set primary instruction cache tag?
	.set	noreorder
	nop
	nop
	cache	CACH_SD|C_IST, 0(a1)	# no, set secondary cache tag
	nop
	nop
	.set	reorder

setout: j	ra

wprimi:	.set	noreorder
	cache	CACH_PI|C_IST, 0(a1)	# set primary instruction cache tag
	.set	reorder
	b	setout

wprimd:	.set	noreorder
	cache	CACH_PD|C_IST, 0(a1)	# set primary data cache tag
	.set	reorder
	b	setout

	END(write_tag)


/*
 * fill_ipline(addr)
 * Fill Primary Instruction Cache line via k0seg
 * addr should always be in k0seg 
 */
LEAF(fill_ipline)
	.set	noreorder
	cache	CACH_PI|C_FILL, 0(a0)	# fill the line - easy, wasn't it?
	.set	reorder

	j	ra
	END(fill_ipline)


/*
 * write_ipline(addr)
 * Write Primary Instruction Cache line back to secondary
 * addr should always be in k0seg 
 */
LEAF(write_ipline)
	.set	noreorder
	cache	CACH_PI|C_HWB, 0(a0)	# writeback the line
	NOP_0_1
	.set	reorder
	j	ra
	END(write_ipline)


/*
 * dpline_to_secondary(addr)
 * Write Primary Data Cache line to secondary cache
 * addr should always be in k0seg 
 */
NESTED(dpline_to_secondary, 8, ra)
	subu	sp, 8			# set up the stack frame so can save
	sw	ra, 0(sp)		#   ra if calling flush2ndline
	.set	noreorder
	cache	CACH_PD|C_HWB, 0(a0)	# writeback the line
	NOP_0_1
	.set	reorder

	lw	ra, 0(sp)		# restore the stack frame and return
	addu	sp, 8
	j	ra
	END(dpline_to_secondary)


/*
 * unsigned int time_function (void (*function)())
 *
 * Return number of count register ticks that "function" takes to execute.
 * Caller is responsible for making sure that "function"s address is in
 * the appropriate memory space for timing cached vs uncached run times.
 */
TIMEFRAME=	8			# room for start counts and ra
NESTED(time_function, TIMEFRAME, ra)
	subu	sp, TIMEFRAME		# set up stack frame and
	sw	ra, TIMEFRAME-4(sp)	#  save the return address

	.set	noreorder
	mfc0	t0,C0_COUNT		# get the starting count register time
	.set	reorder

	sw	t0, 0(sp)		# save the starting count

	jal	a0			# call "function"

	.set	noreorder
	mfc0	t0, C0_COUNT		# get the ending count register time
	.set	reorder

	lw	t1, 0(sp)		# get the starting time again

	subu	v0, t0, t1		# put the time taken in the return reg

	lw	ra, TIMEFRAME-4(sp)	# get the return address
	addu	sp, TIMEFRAME		#  restore the stack frame
	j	ra			#  and return to caller
	END(time_function)

LEAF(DoEret)
XLEAF(DoExceptionEret)
XLEAF(DoErrorEret)
	.set	noreorder
	mtc0	ra,C0_EPC
	mtc0	ra,C0_ERROR_EPC
	nop
	nop
	nop
	eret
	.set	reorder
	END(DoEret)
