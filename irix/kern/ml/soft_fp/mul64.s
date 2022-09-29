/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.1 $"

#include <regdef.h>
#include <sys/asm.h>

	/* computes the product of two 64 bit integers, returning
	 * the high bits in v0 and the low bits in the address
	 * pointed to by register a2.
	 */

	PICOPT
	.text

LEAF(mul64)

#if _MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32
	dmultu	a0,a1
	mflo	v0
	sd	v0,0(a2)
	mfhi	v0
	j	ra
#elif _MIPS_SIM == _MIPS_SIM_ABI32
	sw	a0,0(sp)
	sw	a1,4(sp)
	sw	a2,8(sp)
	sw	a3,12(sp)
	ld	t0, 0(sp)
	ld	t1, 8(sp)
	dmultu	t0,t1
	mflo	v0		/* low 64bits of 128bit result. */
	dsll	v1,v0,32	/* low-order half of low order of result */
	dsra	v1,32		/* sign-extend low order half */
	dsra	v0,32		/* sign-extend high order half */
	lw	t0,16(sp)	/* pointer to long long for low order result */
	sw	v0,0(t0)	/* high-order half of low-order result */
	sw	v1,4(t0)	/* low-order half of low-order result */
	mfhi	v0		/* high-order 64bit of 128bit result */
	dsll	v1,v0,32	/* low-order half of high result */
	dsra	v1,32		/* sign-extend low order half */
	dsra	v0,32		/* sign-extend high-order half */
	j	ra
#else
<<<BOMB - need correct version for whatever this compilation model might be>>>
#endif


END(mul64)

