#ident	"$Id: locore.s,v 1.1 1994/07/21 01:14:34 davidl Exp $"
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#include "sys/regdef.h"
#include "sys/asm.h"
#include "sys/sbd.h"

	.text

/*
 * PROCEDURE FlushWB: Wait until the write buffer is empty.
 *
 * CALLING SEQUENCE
 *
 *	FlushWB();
 *
 * PROCESSING
 */
LEAF(FlushWB)

	nop
	nop
	nop
	nop
#if	defined (RB3125) || defined (R3030)
1:
	bc0f	1b
#endif	(RB3125)

	j	ra

END(FlushWB)

/*
 * PROCEDURE SetSR: Set the contents of the status register.
 *
 * CALLING SEQUENCE
 *
 *      SetSR(enables);
 *
 * PROCESSING
 */
LEAF(SetSR)
	.set noreorder
	mtc0    a0,C0_SR
	nop
 	nop
	.set reorder
	j       ra

END(SetSR)


/*		G e t S R ( )
 *
 * PROCEDURE GetSR: Get the contents of the status register.
 *
 * CALLING SEQUENCE
 *
 *      status - GetSR();
 *
 * PROCESSING
 */
LEAF(GetSR)
	.set noreorder
	mfc0    v0,C0_SR
	nop
	nop
	.set reorder
	j       ra

END(GetSR)



/*		G e t S P ( )
 *
 * GetSP() - Returns the value of the stack pointer register
 */
LEAF(GetSP)
	.set	noreorder
	addu	t0, sp, a0
	lw	v0, (t0)
	nop
	j	ra
	nop
	.set	reorder

END(GetSP)



/*		G e t E c c ()
 *
 * GetEcc() - Returns the ecc register in the r4k.
 */
LEAF(GetEcc)
        .set    noreorder
        mfc0    v0,C0_ECC               # get the current ECC value
        .set    reorder
        j       ra
END(GetEcc)


/*		S e t E c c ()
 *
 * SetEcc() - Set the ecc register in the r4k.
 */
LEAF(SetEcc)
        .set    noreorder
        mtc0    a0,C0_ECC               # set the ECC register
        .set    reorder
        j       ra
END(SetEcc)

/* end */
