/*
 *
 * Copyright 1991,1992 Silicon Graphics, Inc.
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

#ident "$Revision: 1.4 $"


#include "asm.h"
#include "cp0.h"
#include "cpu.h"
#include "cpu_board.h"
#include "dregdef.h"
#include "locore.h"

/*
 * Some 250MHz R4K modules will flip bits in the entrylo0 and
 * entrylo1 registers when written, when there's a certain bit
 * pattern in the register already, the workaround is to make
 * sure a zero exists before the value is written.
 */
#define TLBLO_FIX_250MHz(entrylo)	mtc0	zero, entrylo


/************************************************************************
* 									*
*	File: dlocore_r4k.s						*
*	dlocore_r4k.s is a R4000 specific file. It contains the R4000 	*
*	register accessing routines and the exception handler		*
*	"_handler_4000".						*
*									*
************************************************************************/

	.text
	.align 2
/*
 * unsigned GetEntryLo0()
 *
 * Returns the cp0 EntryLo0 register.
 */
LEAF(GetEntryLo0)
	.set noreorder
	mfc0	v0, C0_TLBLO0
	nop
	j	ra
	nop
	.set reorder
END(GetEntryLo0)


/*
 * unsigned GetEntryLo1()
 *
 * Returns the cp0 EntryLo1 register.
 */
LEAF(GetEntryLo1)
	.set noreorder
	mfc0	v0, C0_TLBLO1
	nop
	j	ra
	nop
	.set reorder
END(GetEntryLo1)

/*
 *		S e t E n t r y L o 0 ( )
 *
 * Loads the EntryLo0 register
 *
 * registers used:
 *	a0 - data
 */
LEAF(SetEntryLo0)

	.set	noreorder
	TLBLO_FIX_250MHz(C0_TLBLO0)	# 250MHz R4K workaround
	mtc0	a0, C0_TLBLO0		# LOAD EntryLo0
	nop
	j	ra
	nop
	.set	reorder
END(SetEntryLo0)

/*
 *		S e t E n t r y L o 1 ( )
 *
 * Loads the Entrylo1 register 
 *
 * registers used:
 *	a0 - data
 */
LEAF(SetEntryLo1)

	.set	noreorder
	TLBLO_FIX_250MHz(C0_TLBLO1)	# 250MHz R4K workaround
	mtc0	a0, C0_TLBLO1		# LOAD EntryLo1
	nop
	j	ra
	nop
	.set	reorder
END(SetEntryLo1)


/*
 *		G e t E n t r y H i( )
 *
 * GetEntryHi- Returns the value to the entryHi registers. Note this routine
 * requires a 64 bit buffer since the register is 64 bits wide. Only 39 bits
 * are currently implemented.
 *
 * Registers Used:
 *	a0 - address of doubleword buffer to store register contents
 */
LEAF(GetEntryHi)
	.set	noreorder

	mfc0	v0, C0_TLBHI		# Read EntryHi
	nop
	j	ra
	sd	v0, (a0)

	.set	reorder
END(GetEntryHi)


/*
 *		S e t E n t r y H i( )
 *
 * SetEntryHi- Sets the entryhi register from a double word buffer
 *
 * Register Used:
 *	a0 - Address of doubleword buffer (register is 64 bits)
 */
LEAF(SetEntryHi)
	.set 	noreorder

	lw	t0, (a0)	# get the data
	nop
	mtc0	t0, C0_TLBHI	# Load the register
	nop
	j	ra
	nop

	.set	reorder
END(SetEntryHi)


/*
 *		G e t I n d e x( )
 *
 * GetIndex- Returns the value of the index register.
 *
 * Registers Used:
 *	v0 - address of doubleword buffer to store register contents
 */
LEAF(GetIndex)
	.set	noreorder

	mfc0	v0, C0_INX		# Read Index register
	nop
	j	ra
	nop

	.set	reorder
END(GetIndex)


/*
 *		S e t I n d e x( )
 *
 * SetIndex- Sets the Index register.
 *
 * Register Used:
 *	a0 - Data
 */
LEAF(SetIndex)
	.set 	noreorder

	mtc0	a0, C0_INX	# Load the register
	nop
	j	ra
	nop

	.set	reorder
END(SetIndex)


/*
 * unsigned GetPageMask()
 *
 * Returns the cp0 PageMask register.
 */
LEAF(GetPageMask)
	.set noreorder
	mfc0	v0, C0_PAGEMASK
	nop
	j	ra
	nop
	.set reorder
END(GetPageMask)


/*
 * The procedure set and test the Page Mask Register
 * Input parameters 
 * a0 - page mask 
 * the procedure returns 1 - reading back the page mask fails
 *                       0 - reading back the page  mask pass
 */
LEAF(SetPageMask)
	.set	noreorder

	# Write and test PageMask
	mtc0	a0, C0_PAGEMASK		# write the PageMask
	li	v0,0
	nop
	mfc0	t1, C0_PAGEMASK		# read the PageMask back for compare
	nop
	nop
	bne	a0, t1, 1f
	nop
	j	ra
	nop

1:	j	ra
	li	v0,1
	nop

	.set	reorder
END(SetPageMask)


/*
 * unsigned GetWired()
 *
 * Returns the cp0 Wired register.
 */
LEAF(GetWired)
	.set noreorder
	mfc0	v0, C0_WIRED
	nop
	j	ra
	nop
	.set reorder
END(GetWired)



/*
* Return the badvaddr register
*/
LEAF(GetBadVaddr)
	.set	noreorder
	mfc0	v0,C0_BADVADDR
	nop
	j	ra
	nop
	.set	reorder
END(GetBadVaddr)
/*
 * PROCEDURE SetWired: Set the contents of the Wired register.
 *
 * CALLING SEQUENCE
 *
 *      SetWired(boundary);
 *
 * PROCESSING
 */
LEAF(SetWired)

                .set noreorder
                mtc0    a0,C0_WIRED
                nop
                j       ra
                nop
                .set reorder

END(SetWired)


/*
 * unsigned GetCount()
 *
 * Returns the cp0 Count register.
 */
LEAF(GetCount)
	.set noreorder
	mfc0	v0, C0_COUNT
	nop
	j	ra
	nop
	.set reorder
END(GetCount)


/*
 * PROCEDURE SetCount: Set the contents of the Count register.
 *
 * CALLING SEQUENCE
 *
 *      SetCount(count);
 *
 * PROCESSING
 */
LEAF(SetCount)

                .set noreorder
                mtc0    a0,C0_COUNT
                nop
                j       ra
                nop
                .set reorder

END(SetCount)


/*
 * unsigned GetCompare()
 *
 * Returns the cp0 Compare register.
 */
LEAF(GetCompare)
	.set noreorder
	mfc0	v0, C0_COMPARE
	nop
	j	ra
	nop
	.set reorder
END(GetCompare)


/*
 * PROCEDURE SetCompare: Set the contents of the Compare register.
 *
 * CALLING SEQUENCE
 *
 *      SetCompare(compare);
 *
 * PROCESSING
 */
LEAF(SetCompare)

                .set noreorder
                mtc0    a0,C0_COMPARE
                nop
                j       ra
                nop
                .set reorder

END(SetCompare)


/*
 * unsigned GetConfig()
 *
 * Returns the cp0 Config register.
 */
LEAF(GetConfig)
	.set noreorder
	mfc0	v0, C0_CONFIG
	nop
	j	ra
	nop
	.set reorder
END(GetConfig)


/*
 * PROCEDURE SetConfig: Set the contents of the Config register.
 *
 * CALLING SEQUENCE
 *
 *      SetConfig(config);
 *
 * PROCESSING
 */
LEAF(SetConfig)

                .set noreorder
                mtc0    a0,C0_CONFIG
                nop
                j       ra
                nop
                .set reorder

END(SetConfig)


/*
 * unsigned GetLLAddr()
 *
 * Returns the cp0 LLAddr register.
 */
LEAF(GetLLAddr)
	.set noreorder
	mfc0	v0, C0_LLADDR
	nop
	j	ra
	nop
	.set reorder
END(GetLLAddr)


/*
 * PROCEDURE SetLLAddr: Set the contents of the LLAddr register.
 *
 * CALLING SEQUENCE
 *
 *      SetLLAddr(addr);
 *
 * PROCESSING
 */
LEAF(SetLLAddr)

                .set noreorder
                mtc0    a0,C0_LLADDR
                nop
                j       ra
                nop
                .set reorder

END(SetLLAddr)


/*
 * unsigned GetWatchLo()
 *
 * Returns the cp0 Watch_Lo register.
 */
LEAF(GetWatchLo)
	.set noreorder
	mfc0	v0, C0_WATCHLO
	nop
	j	ra
	nop
	.set reorder
END(GetWatchLo)


/*
 * PROCEDURE SetWatchLo: Set the contents of the Watch_Lo register.
 *
 * CALLING SEQUENCE
 *
 *      SetWatchLo(PAddr0);
 *
 * PROCESSING
 */
LEAF(SetWatchLo)

                .set noreorder
                mtc0    a0,C0_WATCHLO
                nop
                j       ra
		nop
                .set reorder

END(SetWatchLo)


/*
 * unsigned GetWatchHi()
 *
 * Returns the cp0 Watch_Hi register.
 */
LEAF(GetWatchHi)
	.set noreorder
	mfc0	v0, C0_WATCHHI
	nop
	j	ra
	nop
	.set reorder
END(GetWatchHi)


/*
 * PROCEDURE SetWatchHi: Set the contents of the Watch_Hi register.
 *
 * CALLING SEQUENCE
 *
 *      SetWatchHi(PAddr1);
 *
 * PROCESSING
 */
LEAF(SetWatchHi)

                .set noreorder
                mtc0    a0,C0_WATCHHI
                nop
                j       ra
                nop
                .set reorder

END(SetWatchHi)


/*
 * unsigned GetECC()
 *
 * Returns the cp0 ECC register.
 */
LEAF(GetECC)
	.set noreorder
	mfc0	v0, C0_ECC
	nop
	j	ra
	nop
	.set reorder
END(GetECC)


/*
 * PROCEDURE SetECC: Set the contents of the ECC register.
 *
 * CALLING SEQUENCE
 *
 *      SetECC(ecc);
 *
 * PROCESSING
 */
LEAF(SetECC)

                .set noreorder
                mtc0    a0,C0_ECC
                nop
                j       ra
                nop
                .set reorder

END(SetECC)


/*
 * unsigned GetCacheErr()
 *
 * Returns the cp0 Cache_Err register (read only).
 */
LEAF(GetCacheErr)
	.set noreorder
	mfc0	v0, C0_CACHEERR
	nop
	j	ra
	nop
	.set reorder
END(GetCacheErr)


/*
 * unsigned GetTagLo()
 *
 * Returns the cp0 TagLo register.
 */
LEAF(GetTagLo)
	.set noreorder
	mfc0	v0, C0_TAGLO
	nop
	j	ra
	nop
	.set reorder
END(GetTagLo)


/*
 * PROCEDURE SetTagLo: Set the contents of the TagLo register.
 *
 * CALLING SEQUENCE
 *
 *      SetTagLo(taglo);
 *
 * PROCESSING
 */
LEAF(SetTagLo)

                .set noreorder
                mtc0    a0,C0_TAGLO
                nop
                j       ra
                nop
                .set reorder

END(SetTagLo)


/*
 * unsigned GetTagHi()
 *
 * Returns the cp0 TagHi register.
 */
LEAF(GetTagHi)
	.set noreorder
	mfc0	v0, C0_TAGHI
	nop
	j	ra
	nop
	.set reorder
END(GetTagHi)


/*
 * PROCEDURE SetTagHi: Set the contents of the TagHi register.
 *
 * CALLING SEQUENCE
 *
 *      SetTagHi(taghi);
 *
 * PROCESSING
 */
LEAF(SetTagHi)

                .set noreorder
                mtc0    a0,C0_TAGHI
                nop
                j       ra
                nop
                .set reorder

END(SetTagHi)


/*
 * unsigned GetErrorEPC()
 *
 * Returns the cp0 Error_EPC register.
 */
LEAF(GetErrorEPC)
	.set noreorder
	mfc0	v0, C0_ERROREPC
	nop
	j	ra
	nop
	.set reorder
END(GetErrorEPC)


/*
 * PROCEDURE SetErrorEPC: Set the contents of the ErrorEPC register.
 *
 * CALLING SEQUENCE
 *
 *      SetErrorEPC(ErrorEPC);
 *
 * PROCESSING
 */
LEAF(SetErrorEPC)

	.set noreorder
	mtc0    a0,C0_ERROREPC
	nop
	j       ra
	nop
	.set reorder

END(SetErrorEPC)


/*
 * PROCEDURE InvalidTLB_4000: Invalidates TLB entry specified by index.
 *
 * CALLING SEQUENCE
 *
 *	InvalidTLB_4000(index);
 *
 * PROCESSING
 */
LEAF(InvalidTLB_4000) 

	li	a2,K0BASE&TLBHI_VPN2MASK_4000
	move	v1,zero
1:
	.set noreorder
	mfc0	a1,C0_TLBHI		# save current TLBHI
		nop
		nop
	mfc0	a3, C0_PAGEMASK		# save page mask register
		nop
		nop
	mfc0	v0,C0_SR		# save SR
		nop
		nop
	beq	v1,zero,1b
	sub	v1,1

	mtc0	zero,C0_SR		# disable interrupt
		nop
		nop
	mtc0	zero, C0_PAGEMASK	# zero the page mask register
		nop
		nop
	mtc0	a2,C0_TLBHI		# invalidate entry
		nop
		nop
	mtc0	zero,C0_TLBLO0
		nop
		nop
	mtc0	zero,C0_TLBLO1
		nop
		nop
	sll	a0,TLBINX_INXSHIFT_4000
	mtc0	a0,C0_INX
		nop
		nop
	c0	C0_WRITEI
		nop
		nop
	mtc0	a1,C0_TLBHI
		nop
		nop
	mtc0	a3,C0_PAGEMASK
		nop
		nop
	mtc0	v0,C0_SR
		nop
		nop
	j	ra
	nop				# BDSLOT
	.set	reorder

END(InvalidTLB_4000)


