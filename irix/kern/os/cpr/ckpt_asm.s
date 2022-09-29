/*
 * ckpt_asm.c
 *
 * Checkpoint/restart assembly language support routines.
 *
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident	"$Revision: 1.2 $"

#include "sys/asm.h"

#include "sys/regdef.h"
/*
 * Conditionally set the pointer to a chkpt_shm struct.
 * Used to resolve init races between sprocs
 */
LEAF(ckpt_cond_set)
	.set	noreorder

1:	PTR_LL	v0,0(a0)
	bne	v0,zero,2f
	move	t0,a1

	PTR_SC	t0,0(a0)
#ifdef R10K_LLSC_WAR
	beql	t0,zero,1b
#else
	beq	t0,zero,1b
#endif
	nop
2:	j	ra
	nop
	.set	reorder
END(ckpt_cond_set)
