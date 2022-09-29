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

#ident "$Revision: 1.8 $"

#include <asm.h>
#include <regdef.h>
#include <sys/sbd.h>
#include "cache.h"

/* sd_HWBINV(vaddr): Secondary Data cache Hit Writeback Invalidate
 * a0: virtual address
 * Return the CH status bit to indicate hit/miss of secondary. */
LEAF(sd_HWBINV)
	.set	noreorder
	cache	CACH_SD|C_HWBINV, 0(a0)
	nop				# wait for results to get thru pipe
	nop
	mfc0	v0, C0_SR		# check if we hit the 2ndary cache
	nop
	nop
	and	v0, SR_CH
	j       ra
	nop
	.set	reorder
	END(sd_HWBINV)
	
/* sd_hitinv(vaddr): Secondary Data cache HIT INValidate.
 * invalidates secondary and primary cache blocks if virtual address is valid
 * a0: virtual address to invalidate if present */
LEAF(sd_hitinv)
	.set	noreorder
	cache	CACH_SD|C_HINV, 0(a0)
	j       ra
	nop
	.set	reorder
	END(sd_hitinv)

/* pi_HINV(vaddr): Primary Instruction cache Hit INValidate.
 * a0: virtual address
 * Return the CH status bit to indicate hit/miss of secondary. */
LEAF(pi_HINV)
	.set	noreorder
	cache	CACH_PI|C_HINV, 0(a0)
	mfc0	v0, C0_SR		# check if we hit the 2ndary cache
	and	v0, SR_CH
	j       ra
	nop
	.set	reorder
	END(pi_HINV)

/* pd_HINV(vaddr): Primary Data cache Hit INValidate.
 * a0: virtual address
 * Return the CH status bit to indicate hit/miss of secondary. */
LEAF(pd_HINV)
	.set	noreorder
	cache	CACH_PD|C_HINV, 0(a0)
	mfc0	v0, C0_SR		# check if we hit the 2ndary cache
	and	v0, SR_CH
	j       ra
	nop
	.set	reorder
	END(pd_HINV)

/* pd_HWBINV(vaddr): Primary Data cache Hit Writeback Invalidate.
 * a0: virtual address
 * Return the CH status bit to indicate hit/miss of secondary. */
LEAF(pd_HWBINV)
	.set	noreorder
	cache	CACH_PD|C_HWBINV, 0(a0)
	mfc0	v0, C0_SR		# check if we hit the 2ndary cache
	and	v0, SR_CH
	j       ra
	nop
	.set	reorder
	END(pd_HWBINV)

/* pd_iwbinv(vaddr): Primary Data cache Index WriteBack INValidate.
 * a0: virtual address to use as index */
LEAF(pd_iwbinv)
	.set	noreorder
	cache	CACH_PD|C_IWBINV, 0(a0)
	j       ra
	nop
	.set	reorder
	END(pd_iwbinv)
	
/* read_tag(WhichCache, address, &deststruct)
 * WhichCache == PRIMARYD (1), PRIMARYI (2), SECONDARY (3)
 * destruct is type struct tag_regs.
 */
LEAF(read_tag)
	li	v0, 0		# success by default
	LI	t0, K0BASE
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

/*
 * unsigned int time_function (void (*function)())
 *
 * Return number of count register ticks that "function" takes to execute.
 * Caller is responsible for making sure that "function"s address is in
 * the appropriate memory space for timing cached vs uncached run times.
 */
TIMEFRAME=	4*SZREG			# room for start counts and ra
NESTED(time_function, TIMEFRAME, ra)
	PTR_SUBU	sp, TIMEFRAME		# set up stack frame and
	PTR_S	ra, TIMEFRAME-SZREG(sp)	#  save the return address

	.set	noreorder
	mfc0	t0,C0_COUNT		# get the starting count register time
	.set	reorder

	PTR_S	t0, 0(sp)		# save the starting count

	jal	a0			# call "function"

	.set	noreorder
	mfc0	t0, C0_COUNT		# get the ending count register time
	.set	reorder

	PTR_L	t1, 0(sp)		# get the starting time again

	PTR_SUBU	v0, t0, t1		# put the time taken in the return reg

	PTR_L	ra, TIMEFRAME-SZREG(sp)	# get the return address
	PTR_ADDU	sp, TIMEFRAME		#  restore the stack frame
	j	ra			#  and return to caller
	END(time_function)
