#ident	"$Id: libasm.s,v 1.1 1994/07/21 01:14:21 davidl Exp $"
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


/*
 * libasm.s -- standalone libc'ish assembler code
 *
 * Note: This is a completely stripped verion of saio/libasm.s
 * 	 just for the M/6000 diags monitor.
 */

#include "sys/regdef.h"
#include <setjmp.h>
#include "sys/asm.h"



 #	.globl	mbyte_size_mask
 #	.globl	mbyte_size_mask_end

/*
 * setjmp(jmp_buf) -- save current context for non-local goto's
 * return 0
 */
LEAF(setjmp)
	sw	ra,JB_PC*4(a0)
	sw	sp,JB_SP*4(a0)
	sw	fp,JB_FP*4(a0)
	sw	s0,JB_S0*4(a0)
	sw	s1,JB_S1*4(a0)
	sw	s2,JB_S2*4(a0)
	sw	s3,JB_S3*4(a0)
	sw	s4,JB_S4*4(a0)
	sw	s5,JB_S5*4(a0)
	sw	s6,JB_S6*4(a0)
	sw	s7,JB_S7*4(a0)
	move	v0,zero
	j	ra
	END(setjmp)


/*
 * longjmp(jmp_buf, rval)
 */
LEAF(longjmp)
	lw	ra,JB_PC*4(a0)
	lw	sp,JB_SP*4(a0)
	lw	fp,JB_FP*4(a0)
	lw	s0,JB_S0*4(a0)
	lw	s1,JB_S1*4(a0)
	lw	s2,JB_S2*4(a0)
	lw	s3,JB_S3*4(a0)
	lw	s4,JB_S4*4(a0)
	lw	s5,JB_S5*4(a0)
	lw	s6,JB_S6*4(a0)
	lw	s7,JB_S7*4(a0)
	move	v0,a1
	j	ra
	END(longjmp)

/*
 * flush_cache()
 *
 * invalidate entire cache for both instruction and data caches.
 */
LEAF(flush_cache)
	move	s6,ra
	jal	invalidate_dcache
	jal	invalidate_icache
	j	s6
END(flush_cache)

