/*
 *
 * Copyright 1991,1992 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.2 $"


#include "asm.h"
#include "regdef.h"
/* #include "../include/mach_ops.h"*/
#include "cpu.h"

/*
 *		C r e a t e C l u s t r s ( )
 *
 * CreateClustrs()- Stores complimenting data at two different addressses.
 * If the addresses are choosen correctly, this will cause a dirty writeback
 * with an implied read and explicit store. The second address will overwrite
 * the first address in the cache forcing a writeback. The second store will
 * do an implied read of the cache since it does not own the line.
 *
 * register used:
 *	a0 - first address
 *	a1 - second address
 *	a2 - number of locations to write
 *	a3 - data pattern
 *	t1 - compliment of data pattern
 */
LEAF(CreateClustrs)
	.set	noreorder
	
	beq	a2,zero, 2f
	not	t1, a3

1:
	sw	a3, 0(a0)
	sw	t1, 0(a1)

	sw	a3, 4(a0)
	sw	t1, 4(a1)

	sw	a3, 8(a0)
	sw	t1, 8(a1)

	sw	a3, 12(a0)
	sw	t1, 12(a1)

	sw	a3, 16(a0)
	sw	t1, 16(a1)

	sw	a3, 20(a0)
	sw	t1, 20(a1)

	sw	a3, 24(a0)
	sw	t1, 24(a1)

	sw	a3, 28(a0)
	sw	t1, 28(a1)

	sw	a3, 32(a0)
	sw	t1, 32(a1)

	sw	a3, 36(a0)
	sw	t1, 36(a1)


	addiu	a0, 40
	addiu	a1, 40
	addiu	a2, -10
	#bne	a2,zero, 1b
	bgtz	a2, 1b
	move	v0, a1
	nop
2:
	j	ra
	nop

	.set	reorder
END(CreateClustrs)

/*
 *		C r e a t e C l u s t r s ( )
 *
 * Clustrwb()- Stores complimenting data at two different addressses.
 * If the addresses are choosen correctly, this will cause a dirty writeback
 * with an implied read and explicit store. The second address will overwrite
 * the first address in the cache forcing a writeback. The second store will
 * do an implied read of the cache since it does not own the line.
 *
 * register used:
 *	a0 - first address
 *	a1 - alias of first address
 *	a2 - second address
 *	a3 - alias of second address
 *	t0 - data pattern
 *	t1 - compliment of data pattern
 *	t2 - data patter
 */
LEAF(Clustrwb)
	.set	noreorder
	
	li	t0, 0x55555555
	not	t1, t0
	li	t2, 0x11111111

1:
	sw	t0, (a2)	# Implied READ, now dirty the line - line 1
	sw	t1, 0(a0)	# Implied READ, now dirty the line - line 2

	sw	t0, 0(a1)	# Alias line 2 in cache, causes a CLUSTER

	sw	t2, (a0)	# Cause another CLUSTER on line 2

	cache	Hit_Writeback_SD, 0x0(a0)  # Follow cluster with a HITWRITEBACK on line 2

	cache	Hit_Writeback_SD, 0x0(a2)  # Do a HITWRITEBACK on line 1 

	sw	t1, (a3)	  # Follow up with a CLUSTER on line 1

	j	ra
	nop

	.set	reorder
END(Clustrwb)

/*
 *		G e n C l u s t r s ( )
 *
 * CreateClustrs()- Stores complimenting data at two different addressses.
 * If the addresses are choosen correctly, this will cause a dirty writeback
 * with an implied read and explicit store. The second address will overwrite
 * the first address in the cache forcing a writeback. The second store will
 * do an implied read of the cache since it does not own the line.
 *
 * register used:
 *	a0 - first address
 *	a1 - second address
 *	a2 - number of locations to write
 *	a3 - data pattern
 *	t1 - compliment of data pattern
 */
LEAF(GenClustrs)
	.set	noreorder
	
	li	a3, 0xaaaaaaaa
	not	t1, a3
	lw	t0, scache_linesize

1:
	sw	a3, 0(a0)
	sw	t1, 0(a1)

	add	a0, t0
	addiu	a2, -1
	bne	a2, zero, 1b
	add	a1, t0

	j	ra
	nop

	.set	reorder
END(GenClustrs)


