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
/* $Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/math/RCS/qfinite.s,v 1.1 1997/07/22 17:23:26 vegas Exp $ */

.weakext qfinite, _finitel
.weakext finitel, _finitel

#include "synonyms.h"
#include <regdef.h>
#include <sys/asm.h>

	PICOPT
	.text

LEAF(finitel)

	dmfc1	v0, $f12	# fetch first doubleword of arg
	dsll	v0, 1		# shift off sign bit
	dsrl32	v0, 20+1	# shift off mantissa
	sltu	v0, v0, 0x7ff	# return 1 if exponent(arg) < 0x7ff, 0 otherwise
	j	ra

END(finitel)

