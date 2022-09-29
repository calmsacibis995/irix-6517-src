/**************************************************************************/
/*									  */
/* 		 Copyright (C) 1989, Silicon Graphics, Inc.		  */
/*									  */
/*  These coded instructions, statements, and computer programs  contain  */
/*  unpublished  proprietary  information of Silicon Graphics, Inc., and  */
/*  are protected by Federal copyright law.  They  may  not be disclosed  */
/*  to  third  parties  or copied or duplicated in any form, in whole or  */
/*  in part, without the prior written consent of Silicon Graphics, Inc.  */
/*									  */
/**************************************************************************/
#include <regdef.h>
#include <asm.h>
#include <stdlib.h>

/* div - integer division returning structure with quot/rem */

/*
 *	C Synopsis:
 *	
 *		extern div_t	div(int, int);
 *	
 *		struct __div_s {
 *			int quot, rem;
 *		};
 *		typedef struct __div_s div_t;
 *
 */
LEAF(div)
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
/*
 *	a0 - addr of base of return structure.
 *	a1 - dividend
 *	a2 - divisor
 */
	div 	a1,a2
	mflo 	t0
	sw	t0,_QUOT_OFFSET(a0)
	mfhi	t1
	sw	t1,_REM_OFFSET(a0)
	move	$2, a0
	j ra
#endif
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
/*
 *	a0 - dividend
 *	a1 - divisor
 *	(v0 << 32) | v1 is the result. Caller will do 'sd v0' to save it
 *	in the struct.
 */
	div 	a0,a1
	mflo 	v0
	dsll32	v0,0
	mfhi	v1
	dsll32	v1,0		# clear the high order bits
	dsrl32	v1,0		# in v1 since it's a 32 bit value
	or	v0,v1		# return (v0 << 32) | v1
	j ra
#endif
	END(div)
