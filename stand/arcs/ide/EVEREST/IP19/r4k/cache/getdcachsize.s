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

#ident "$Revision: 1.1 $"


#include "asm.h"
#include "regdef.h"
#include "cpu.h"
#include "coher.h"

LEAF(GetDcachSize)
	.set	noreorder

	mfc0	t0, C0_CONFIG
	nop
	and	t1, t0, CONFIG_DC
	srl	t1, CONFIG_DC_SHIFT
	li	v0, 4096		# Size = 2^12+DC ( 4096 = 2^12 )
	sll	v0, t1
	sw	v0, (a0)		# Store cache size in user defined var

	and	t1, t0, CONFIG_DB
	srl	t1, CONFIG_DB_SHIFT
	li	v0, 16
	beq	t1, zero, 1f
	nop
	sll	v0, 1
1:	sw	v0, (a1)		# Store linesize in user defined var
	j	ra
	nop
	.set	reorder
END(GetDcachSize)
