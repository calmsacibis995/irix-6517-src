#if (_MIPS_SIM == _ABIO32) || (_MIPS_SIM == _MIPS_SIM_ABI32)
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
/* $Header: /proj/irix6.5.7m/isms/stand/arcs/lib/libsc/ml/RCS/llshift.c,v 1.2 1995/11/20 08:51:25 jeffs Exp $ */

#include <limits.h>
#define bitsperword	WORD_BIT
#include "lldefs.h"

#define SHIFT_MASK	0x3f	/* 6 bit mask = 0...0111111 */

/* shift routines */

extern longlong_t
__ll_lshift (longlong_t left, longlong_t right)
{
	llvalue a, b, r, mask;

	a.ll = left;
	b.ll = right;
	/*
	 * shift by negative value or > 32/64 is undefined,
	 * but for compatibility we do what our hardware does,
	 * which is to mask the right value with 6 rightmost bits.
	 */
	SET_LL(mask, SHIFT_MASK);
	LL_AND(b, b, mask);
	/* assert(b.dw.msw == 0); */
	if (b.dw.lsw >= bitsperword) {
		/* everything shifted to msw */
		r.dw.msw = a.dw.lsw << (b.dw.lsw - bitsperword);
		r.dw.lsw = 0;
	} else if (b.dw.lsw > 0) {
		/* lsw shifted, then msw combines lsw and msw */
		r.dw.msw = (a.dw.msw << b.dw.lsw) 
			| (a.dw.lsw >> (bitsperword - b.dw.lsw));
		r.dw.lsw = a.dw.lsw << b.dw.lsw;
	} else {
		/* b == 0 */
		return left;
	}
	return r.ll;
}

/* unsigned long long right-shift */
extern longlong_t
__ull_rshift (ulonglong_t left, longlong_t right)
{
	llvalue a, b, r, mask;

	a.ll = left;
	b.ll = right;
	/*
	 * shift by negative value or > 32/64 is undefined,
	 * but for compatibility we do what our hardware does,
	 * which is to mask the right value with 6 rightmost bits.
	 */
	SET_LL(mask, SHIFT_MASK);
	LL_AND(b, b, mask);
	/* assert(b.dw.msw == 0); */
	if (b.dw.lsw >= bitsperword) {
		/* everything shifted to lsw */
		r.dw.lsw = a.dw.msw >> (b.dw.lsw - bitsperword);
		r.dw.msw = 0;
	} else if (b.dw.lsw > 0) {
                /* msw shifted, then lsw combines lsw and msw */
		r.dw.lsw = (a.dw.lsw >> b.dw.lsw) 
			| (a.dw.msw << (bitsperword - b.dw.lsw));
		r.dw.msw = a.dw.msw >> b.dw.lsw;
	} else {
		/* b == 0 */
		return left;
	}
	return r.ll;
}


/* Signed long long right-shift.
 * For negative values, result is implementation-defined,
 * but for compatibility with 32-bit shift, we fill with sign bit. */
extern longlong_t
__ll_rshift (longlong_t left, longlong_t right)
{
	llvalue a, b, r, mask;

	a.ll = left;
	b.ll = right;
	/*
	 * shift by negative value or > 32/64 is undefined,
	 * but for compatibility we do what our hardware does,
	 * which is to mask the right value with 6 rightmost bits.
	 */
	SET_LL(mask, SHIFT_MASK);
	LL_AND(b, b, mask);
	/* assert(b.dw.msw == 0); */
	if (b.dw.lsw >= bitsperword) {
		/* everything shifted to lsw */
		r.dw.lsw = ((int) a.dw.msw) >> (b.dw.lsw - bitsperword);
		/* return 0 or -1 */
		r.dw.msw = (((int) a.dw.msw < 0) ? -1 : 0);
	} else if (b.dw.lsw > 0) {
                /* msw shifted, then lsw combines lsw and msw */
		r.dw.lsw = (a.dw.lsw >> b.dw.lsw) 
			| (a.dw.msw << (bitsperword - b.dw.lsw));
		r.dw.msw = ((int) a.dw.msw) >> b.dw.lsw;
	} else {
		/* b == 0 */
		return left;
	}
	return r.ll;
}
#endif
