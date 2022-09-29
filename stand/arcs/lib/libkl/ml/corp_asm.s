/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.2 $"

#include <sys/regdef.h>
#include <asm.h>

/*
 * corp_jump(corp_t *c, __uint64_t entry, __uint64_t sp, __uint64_t arg)
 */

	.set	reorder

LEAF(corp_jump)
	move	v0, a1
	move	sp, a2
	daddu	sp, -16
	sd	a0, 0(sp)
	move	a1, a3
	jal	v0
	ld	a0, 0(sp)
	daddu	sp, 16
	jal	corp_exit
	END(corp_jump)
