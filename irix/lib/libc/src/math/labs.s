/**************************************************************************/
/*									  */
/* 		 Copyright (C) 1989, Silicon Graphics, Inc.		  */
/*									  */
/*  These coded instructions, statements, and computer programs  contain  */
/*  unpublished  proprietary  information of Silicon Graphics, Inc., and  */
/*  are protected by Federal copyright law.  They  may  not be disclosed  */
/*  to  third  parties  or copied or duplicated in any form, in whole or  */
/*  in part, without the prior written consent of Silicon Graphics, Inc.  */
/*									  */
/**************************************************************************/
/*  labs.s 1.1 */
#include <regdef.h>
#include <asm.h>

/* labs - absolute value of long */

LEAF(labs)
#if (_MIPS_SZLONG == 32)
	abs v0,a0
	j ra
#elif (_MIPS_SZLONG == 64)

	.set    noreorder
	bgez	a0, 1f
	move	v0, a0
	dsub	v0, zero, a0
1:
	j ra
	nop
	.set	reorder
#else
BOMB!!!
#endif
	END(labs)
