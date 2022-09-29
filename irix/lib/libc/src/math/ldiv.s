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
#ident "$Id: ldiv.s,v 1.6 1994/10/26 20:49:40 jwag Exp $"

#include <regdef.h>
#include <asm.h>
#include <stdlib.h>

/* ldiv - long integer division returning structure with quot/rem */

/*
 *	C Synopsis:
 *	
 *		extern ldiv_t	ldiv(long int, long int);
 *	
 *		struct __ldiv_s {
 *			long int quot, rem;
 *		};
 *		typedef struct __ldiv_s ldiv_t;
 *
 */
LEAF(ldiv)
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
/*
 *	a0 - addr of base of return structure.
 *	a1 - dividend
 *	a2 - divisor
 */
	div 	a1,a2
	mflo 	t0
	sw	t0,_LQUOT_OFFSET(a0)
	mfhi	t1
	sw	t1,_LREM_OFFSET(a0)
	move	$2, a0
	j ra
#endif
#if (_MIPS_SIM == _MIPS_SIM_ABI64)
/*
 *	a0 - dividend
 *	a1 - divisor
 */
	ddiv 	a0,a1
	mflo 	v0
	mfhi	v1
	j ra
#endif
#if (_MIPS_SIM ==  _MIPS_SIM_NABI32)
	div	a0,a1
	mflo	v0
	mfhi	v1
	dsll	v0, v0, 32
	dsll	v1, v1, 32
	dsrl	v1, v1, 32
	or	v0, v0, v1
	j ra
#endif
	END(ldiv)
