/* --------------------------------------------------- */
/* | Copyright (c) 1986 MIPS Computer Systems, Inc.  | */
/* | All Rights Reserved.                            | */
/* --------------------------------------------------- */
/* $Header: /proj/irix6.5.7m/isms/stand/arcs/ide/EVEREST/IP19/r4k/fpu/RCS/fabs.s,v 1.1 1992/11/06 17:57:28 swong Exp $ */
#include <regdef.h>

/* fabs - floating absolute value */

.globl fabs
.ent fabs
fabs:
	.frame sp,0,ra
	abs.d $f0,$f12
	j ra
.end fabs
