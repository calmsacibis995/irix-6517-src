/*
 * ecc_asm.s - diagnostic assembler subroutines for checking ECC logic
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 *
 * NOTE: not changed for IP30 
 */
#ident "ide/godzilla/ecc/ecc_asm.s: $Revision 1.1$"


#include "asm.h"
#include "regdef.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/R10k.h"

/*
 * This file defines the following C-callable functions:
 *	int ecc_sdlr(addr, offset, value)
 *	heart_ecc_test_read(addr, value)
 */

/* ecc_sdlr --
 *	write data using SDL ins
 *	a0 - address to be written to
 *	a1 - offset
 *	a2 - value to write
 *	This is a level 0 routine (C-callable).
 *
 *	long eccsdlr(addr,offset,value,expect);
 */
LEAF(ecc_sdlr)
	beq	zero,a1,1f
	li	t0,1
	beq	t0,a1,2f
	li	t0,2
	beq	t0,a1,3f
	li	t0,3
	beq	t0,a1,4f
	li	t0,4
	beq	t0,a1,5f
	li	t0,5
	beq	t0,a1,6f
	li	t0,6
	beq	t0,a1,7f
	li	t0,7
	beq	t0,a1,8f
	j	10f
1:
	sdr	a2,4(a0)
	sdl	a2,0(a0)
	j	10f
2:
	sdr	a2,4(a0)
	sdl	a2,1(a0)
	j	10f
3:
	sdr	a2,4(a0)
	sdl	a2,2(a0)
	j	10f
4:
	sdr	a2,4(a0)
	sdl	a2,3(a0)
	j	10f
5:
	sdr	a2,4(a0)
	sdl	a2,4(a0)
	j	10f
6:
	sdr	a2,4(a0)
	sdl	a2,5(a0)
	j	10f
7:
	sdr	a2,4(a0)
	sdl	a2,6(a0)
	j	10f
8:
	sdr	a2,4(a0)
	sdl	a2,7(a0)
	j	10f
10:
	ld	v0,0(a0)
	j	ra
	END(ecc_sdlr)


/* heart_ecc_test_read --
 *	called by CHKREAD macro in heart_ecc_test_cycle()
 */
	LEAF(heart_ecc_test_read)
	.set	noreorder
	move	v0,a1
	cache	CACH_BARRIER, 0(a0)
	ld	v0,0(a0)
	cache	CACH_BARRIER, 0(a0)
	.set	reorder
	j	ra
	END(heart_ecc_test_read)
