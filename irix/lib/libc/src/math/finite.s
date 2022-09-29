/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */

#ident "$Id: finite.s,v 1.3 1994/10/26 20:37:52 jwag Exp $"

.weakext finite _finite

#include "synonyms.h"
#include <regdef.h>
#include <sys/asm.h>

/*  int finite (double dsrc)
 *
 *  return true (1) if argument is neither infinity nor NaN
 *
 *  Just need to check for MAXEXP
 */

	.text

#if (_MIPS_SIM == _MIPS_SIM_ABI32)

LEAF(finite)
	
	.set	noreorder
	mfc1	v0, $f13	# fetch MSW of arg
	nop
	sll	v0, 1		# shift off sign bit
	srl	v0, 20+1	# shift off mantissa
	j	ra
	sltu	v0, v0, 0x7ff	# return 1 if exponent(arg) < 0x7ff, 0 otherwise

END(finite)

#else /* (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32) */

LEAF(finite)

	.set	noreorder
	dmfc1	v0, $f12	# fetch arg
	nop
	dsll	v0, 1		# shift off sign bit
	dsrl32	v0, 20+1	# shift off mantissa
	j	ra
	sltu	v0, v0, 0x7ff	# return 1 if exponent(arg) < 0x7ff, 0 otherwise

END(finite)

#endif

