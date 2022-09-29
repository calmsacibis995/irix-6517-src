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



/*		i s S C a c h e( )
 *
 * noSCache()- Reads the config register and determines of a secondary
 * cache is present.
 *
 * Registers Used:
 *	t0 - Contains config register
 *	t1 - temp register
 *	v0 - return value: 1= no secondary cache, 0= secondary cache
 */
LEAF(noSCache)
	.set	noreorder
	mfc0	t0, C0_CONFIG
	li	v0, 1			# Assume no secondary cache...
	and	t1, t0, CONFIG_NOSC
	bne	t1, zero, 1f		# zero means secondary cache present
	nop				# BDSLOT
	li	v0, 0			# Secondary cache present
1:
	j	ra
	nop
	.set	reorder
END(noSCache)
