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
 * Module: isnanq
 * $Revision: 1.2 $
 * $Date: 1996/08/02 23:49:47 $
 * $Author: vegas $
 * $Source: /proj/irix6.5.7m/isms/irix/lib/libc/src/math/RCS/isnanq.c,v $
 *
 * Revision history:
 *  25-jul-93 - Original Version
 *
 * Description: quad precision version of isnan
 *
 * ====================================================================
 * ====================================================================
 */

#ident	"$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/math/RCS/isnanq.c,v 1.2 1996/08/02 23:49:47 vegas Exp $"

#ifdef __STDC__

extern	int	isnanq(long double);
extern	int	isnanl(long double);

#pragma weak isnanq = _isnanq
#pragma weak isnanl = _isnanq

#endif

#include "math_extern.h"
#include <inttypes.h>
#include "quad.h"


/* ====================================================================
 *
 * FunctionName: isnanq
 *
 * Description:  isnanq(u) returns 1 if u is a NaN, 0 otherwise
 *
 * ====================================================================
 */

int
_isnanq(u)
long double	u;
{	
ldquad	x;

	x.ld = u;

	return ( _isnand(x.q.hi) );
}

