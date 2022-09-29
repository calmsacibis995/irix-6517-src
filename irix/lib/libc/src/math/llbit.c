/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991 MIPS Computer Systems, Inc.            |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 52.227-7013.   |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Drive                                |
 * |         Sunnyvale, CA 94086                               |
 * |-----------------------------------------------------------|
 */
/* $Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/math/RCS/llbit.c,v 1.2 1994/06/16 00:02:15 mpm Exp $ */

#include <limits.h>
#define BITS_PER_LONGLONG 64
#include "lldefs.h"

/*
 * Need two extract routines because 
 * the unsigned extract should not sign-extend the value,
 * while the signed extract should.
 */
extern ulonglong_t
__ull_bit_extract (ulonglong_t *addr, unsigned start_bit, unsigned length)
{
	/* assume 32 < length < 64 */
	unsigned words = start_bit / BITS_PER_LONGLONG;
	unsigned lbits = start_bit % BITS_PER_LONGLONG;
	unsigned rbits = BITS_PER_LONGLONG - (lbits + length);
	llvalue llval, addrval, mask, one;
	
	addr += words;
	addrval.ull = *addr;
	SET_LL(one, 1);
	SET_LL(mask, 1);
	mask.ull = __ll_lshift(mask.ull, length);
	LL_SUB(mask, mask, one);
	mask.ull = __ll_lshift(mask.ull, rbits);	/* all 1's in field */
	LL_AND(llval, addrval, mask);
	llval.ull = __ull_rshift(llval.ull, rbits);
	return llval.ull;
}

extern ulonglong_t
__ll_bit_extract (ulonglong_t *addr, unsigned start_bit, unsigned length)
{
	/* assume 32 < length < 64 */
	unsigned words = start_bit / BITS_PER_LONGLONG;
	unsigned lbits = start_bit % BITS_PER_LONGLONG;
	unsigned rbits = BITS_PER_LONGLONG - (lbits + length);
	llvalue llval, addrval, mask, one;
	
	addr += words;
	addrval.ull = *addr;
	SET_LL(one, 1);
	SET_LL(mask, 1);
	mask.ull = __ll_lshift(mask.ull, length);
	LL_SUB(mask, mask, one);
	mask.ull = __ll_lshift(mask.ull, rbits);	/* all 1's in field */
	LL_AND(llval, addrval, mask);
	/* shift to left so will sign-extend */
	llval.ull = __ll_lshift(llval.ull, lbits);
	llval.ull = __ll_rshift(llval.ull, lbits+rbits);
	return llval.ull;
}

extern ulonglong_t
__ll_bit_insert (ulonglong_t *addr, unsigned start_bit, unsigned length, ulonglong_t val)
{
	/* assume 32 < length < 64 */
	unsigned words = start_bit / BITS_PER_LONGLONG;
	unsigned lbits = start_bit % BITS_PER_LONGLONG;
	unsigned rbits = BITS_PER_LONGLONG - (lbits + length);
	llvalue llval, addrval, mask, nmask, one;

	addr += words;
	llval.ull = val;
	addrval.ull = *addr;
	SET_LL(one, 1);
	SET_LL(mask, 1);
	mask.ull = __ll_lshift(mask.ull, length);
	LL_SUB(mask, mask, one);
	mask.ull = __ll_lshift(mask.ull, rbits);	/* all 1's in field */
	LL_NOT(nmask, mask);
	LL_AND(addrval, addrval, nmask);		/* clear the field */
	llval.ull = __ll_lshift(llval.ull, lbits+rbits);/* clear lhs, rhs */
	llval.ull = __ull_rshift(llval.ull, lbits);
	LL_OR(addrval, addrval, llval);
	*addr = addrval.ull;
	llval.ull = __ull_rshift(llval.ull, rbits);	/* truncated val */
	return llval.ull;
}
