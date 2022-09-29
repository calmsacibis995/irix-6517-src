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


LEAF(GetScachSize)
	.set	noreorder
	mfc0	t0, C0_CONFIG
	li	v0, 0			# Assume no secondary cache...
	sw	v0, (a0)		# store cachesize in user variable 
	sw	v0, (a1)		# store linesize in user variable
	and	t1, t0, CONFIG_NOSC
	bne	t1, zero, 2f		# zero means secondary cache present
	nop				# BDSLOT
	li	v0, 0x100000
	nop				# LDSLOT
	sw	v0, (a0)		# save cachesize
	and	t1, t0, CONFIG_SB
	srl	t1, t1, CONFIG_SB_SHIFT	# Make this a define ...
	li	v0, 16			# Size = 2^5+SB
	nop				# LDSLOT
	sll	v0, t1
	sw	v0, (a1)		# save linesize
2:
	j	ra
	nop
	.set	reorder
END(GetScachSize)
