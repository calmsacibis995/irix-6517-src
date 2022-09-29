/* --------------------------------------------------- */
/* | Copyright (c) 1986 MIPS Computer Systems, Inc.  | */
/* | All Rights Reserved.                            | */
/* --------------------------------------------------- */
/* $Header: /proj/irix6.5.7m/isms/stand/arcs/ide/IP32/fpu/RCS/fabs.s,v 1.2 1997/05/15 16:07:50 philw Exp $ */
#include <regdef.h>

/* fabs - floating absolute value */

.globl fabs
.ent fabs
fabs:
	.frame sp,0,ra
	abs.d $f0,$f12
	j ra
.end fabs
