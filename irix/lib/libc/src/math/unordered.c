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
#ident	"$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/math/RCS/unordered.c,v 1.1 1992/10/30 09:58:37 gb Exp $"
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/

/*	IEEE recommended functions */

#ifdef __STDC__
	#pragma weak unordered = _unordered
#endif
#include "synonyms.h"
#include <values.h>
#include "fpparts.h"

#define P754_NOFAULT 1		/* avoid generating extra code */
#include <ieeefp.h>

/* UNORDERED(x,y)
 * unordered(x,y) returns 1 if x is unordered with y, otherwise
 * it returns 0; x is unordered with y if either x or y is NAN
 */

int unordered(x,y)
double	x,y;
{	
	if ((EXPONENT(x) == MAXEXP) && (HIFRACTION(x) || LOFRACTION(x)))
		return 1;
	if ((EXPONENT(y) == MAXEXP) && (HIFRACTION(y) || LOFRACTION(y)))
		return 1;
	return 0;
}	

