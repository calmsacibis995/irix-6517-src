#ident	"$Id: early_regdef.h,v 1.1 1994/07/20 23:48:46 davidl Exp $"
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


#ifndef __EARLY_REGDEF_H__
#define __EARLY_REGDEF_H__


/*
 * Power-on IDE diagnostic register definitions.
 *
 * Early in the game we wish to use general purpose routines without a run-time
 * stack (memory integrity is unknown).  Therefore we define abstract register
 * names for return address chaining and local variables for each level of
 * code that must run register-bound.  The lowest level is the (true) leaf,
 * pon_putc, level 0.  The structure from the top down is:
 *	4	pon_diags
 *	3		pon_addruniq, pon_walkingbit
 *	2			pon_showstat, pon_memerr
 *	1				pon_puts, pon_puthex
 *	0					pon_putc
 *
 */

#include <sys/regdef.h>

/* return address registers */
#define RA1	t9
#define RA2	t8
#define RA3	k1
#define RA4	k0

/* general purpose registers */
#define T00	t0	/* C-callable level */
#define T01	t1
#define T02	t2
#define T10	t3	/* C-callable level */
#define T11	t4
#define T20	t5	/* not C-callable */
#define T21	t6
#define T22	t7
#define T23	s0
#define T30	s1	/* not C-callable */
#define T31	s2
#define T32	s3
#define T33	s4
#define T34	s5
#define T40	s6	/* not C-callable */
#define T41	s7

#endif	/* __EARLY_REGDEF_H__ */
