#ident	"$Id: noscache.s,v 1.1 1994/07/21 01:25:44 davidl Exp $"
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#include "sys/regdef.h"
#include "sys/asm.h"
#include "sys/sbd.h"


/*		n o S C a c h e( )
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
	and	t1, t0, CONFIG_SC
	bne	t1, zero, 1f		# zero means secondary cache present
	nop				# BDSLOT
	li	v0, 0			# Secondary cache present
1:
	j	ra
	nop
	.set	reorder
END(noSCache)
