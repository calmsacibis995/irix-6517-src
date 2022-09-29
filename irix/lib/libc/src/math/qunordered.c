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
 * Module: qunordered
 * $Revision: 1.2 $
 * $Date: 1996/08/02 23:50:50 $
 * $Author: vegas $
 * $Source: /proj/irix6.5.7m/isms/irix/lib/libc/src/math/RCS/qunordered.c,v $
 *
 * Revision history:
 *  25-jul-93 - Original Version
 *
 * Description: quad precision version of unordered
 *
 * ====================================================================
 * ====================================================================
 */

#ident	"$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/math/RCS/qunordered.c,v 1.2 1996/08/02 23:50:50 vegas Exp $"

#ifdef __STDC__

extern	int	qunordered(long double, long double);
extern	int	unorderedl(long double, long double);

#pragma weak qunordered = _qunordered
#pragma weak unorderedl = _qunordered

#endif

#include <values.h>
#include "fpparts.h"
#include <inttypes.h>
#include <ieeefp.h>
#include "quad.h"

/* ====================================================================
 *
 * FunctionName: qunordered
 *
 * Description:  qunordered(u,v) returns 1 if u is unordered with v,
 *		 otherwise it returns 0;
 *		 u is unordered with v if either u or v is NAN
 *
 * ====================================================================
 */

int
_qunordered(u,v)
long double	u,v;
{	
ldquad	x, y;

	x.ld = u;
	y.ld = v;

	if ((EXPONENT(x.q.hi) == MAXEXP) && (HIFRACTION(x.q.hi) || LOFRACTION(x.q.hi)))
		return 1;
	if ((EXPONENT(y.q.hi) == MAXEXP) && (HIFRACTION(y.q.hi) || LOFRACTION(y.q.hi)))
		return 1;
	return 0;
}	

