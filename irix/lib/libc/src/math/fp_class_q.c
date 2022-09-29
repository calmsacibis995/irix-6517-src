/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1994       MIPS Computer Systems, Inc.      |
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

/* ====================================================================
 * ====================================================================
 *
 * Module: fp_class_q
 * Description: quad precision version of fp_class_q
 *
 * ====================================================================
 * ====================================================================
 */

#ident	"$Id: fp_class_q.c,v 1.4 1997/10/01 22:42:40 vegas Exp $"

#ifdef __STDC__

extern	int	fp_class_q(long double);
extern	int	fp_class_l(long double);

#pragma weak fp_class_q = _fp_class_q
#pragma weak fp_class_l = _fp_class_q

#endif

#include "synonyms.h"
#include "fp_class.h"
#include "math_extern.h"
#include <inttypes.h>
#include "quad.h"

static const	ldu	twopm916 =
{
{0x06b00000,	0x00000000,
 0x00000000,	0x00000000}
};

static const	ldu	mtwopm916 =
{
{0x86b00000,	0x00000000,
 0x00000000,	0x00000000}
};


/* ====================================================================
 *
 * FunctionName: fp_class_q
 *
 * Description: fp_class_q(u) returns the floating point class u belongs to 
 *
 * ====================================================================
 */

int
fp_class_q(long double	u)
{	
ldquad	x;
int	class;

	x.ld = u;

	class = _fp_class_d(x.q.hi);

	if ( (class != FP_POS_NORM) && (class != FP_NEG_NORM) )
		return ( class );

	class = _fp_class_d(x.q.lo);

	if ( (class == FP_POS_DENORM) || (class == FP_NEG_DENORM) )
		return ( class );

	if ( x.ld >= twopm916.ld )
		return ( FP_POS_NORM );

	if ( x.ld <= mtwopm916.ld )
		return ( FP_NEG_NORM );

	if ( x.q.hi > 0.0 )
		return ( FP_POS_DENORM );

	return ( FP_NEG_DENORM );
}
