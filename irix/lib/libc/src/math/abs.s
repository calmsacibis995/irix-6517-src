/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
/*  abs.s 1.1 */
#include <regdef.h>

/* abs - absolute value */

.globl abs
.ent abs
abs:
	.frame sp,0,ra
	abs v0,a0
	j ra
.end abs
