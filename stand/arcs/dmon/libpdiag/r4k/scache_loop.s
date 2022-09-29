#ident	"$Id: scache_loop.s,v 1.1 1994/07/21 01:25:53 davidl Exp $"
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


#include "sys/asm.h"
#include "sys/regdef.h"
#include "sys/sbd.h"

LEAF(cmd_scache1)
	.set	noreorder	
	move    t0, ra                  #save the return address

        mfc0    s1,C0_SR                # save C0_SR
        nop
        move    a0,s1
        or	a0,SR_DE               	# set the DE bit in SR
        mtc0    a0,C0_SR                


	jal	cache_init
	nop	

	jal	size_2nd_cache
	nop
	move	t6, v0
	jal	scache_ln_size
	nop	
	move	t7, v0

	li	t2, 0xa8600000
	li	t3, 0x88600000

	add	t6,t3,t6
	subu	t6,t6,t7

	li     t4, 0x5555
	li     t5, 0xaaaa
	
/*	Load Memory	*/

	sw	t4,0(t2)
	sw	t5,4(t2)
	sw	t4,8(t2)
	sw	t5,12(t2)
	sw	t4,16(t2)
	sw	t5,20(t2)
	sw	t4,24(t2)
	sw	t5,28(t2)


	sw	t4,0(t6)
	sw	t5,4(t6)
	sw	t4,8(t6)
	sw	t5,12(t6)
	sw	t4,16(t6)
	sw	t5,20(t6)
	sw	t4,24(t6)
	sw	t5,28(t6)

/*	loop	*/

1:
	sw	t4, 0(t3)
	sw	t5, 0(t6)

	move	a0,t3
	cache   CACH_PD | C_HWBINV, 0x0(a0)

	move	a0,t6
	cache   CACH_PD | C_HWBINV, 0x0(a0)

	b	1b
	nop
	j	s0

END(cmd_scache1)




LEAF(cmd_scache2)
	.set	noreorder	
	move    t0, ra                  #save the return address

        mfc0    s1,C0_SR                # save C0_SR
        nop
        move    a0,s1
        or      a0,SR_DE                # set the DE bit in SR
        mtc0    a0,C0_SR                


	jal	cache_init
	nop

	jal	size_2nd_cache
	nop
	move	t6, v0
	jal	scache_ln_size
	nop	
	move	t7, v0

	li	t2, 0xa8600000
	li	t3, 0x88600000

	add	t6,t3,t6
	subu	t6,t6,t7

	li     t4, 0x5555
	li     t5, 0xaaaa
	
/*	Load Memory	*/

	sw	t4,0(t2)
	sw	t5,4(t2)
	sw	t4,8(t2)
	sw	t5,12(t2)
	sw	t4,16(t2)
	sw	t5,20(t2)
	sw	t4,24(t2)
	sw	t5,28(t2)


	sw	t4,0(t6)
	sw	t5,4(t6)
	sw	t4,8(t6)
	sw	t5,12(t6)
	sw	t4,16(t6)
	sw	t5,20(t6)
	sw	t4,24(t6)
	sw	t5,28(t6)

/*	loop	*/

1:
	sw	t4, 0(t3)
	sw	t5, 0(t6)

	move	a0,t3
	cache   CACH_PD | C_HWBINV, 0x0(a0)

	move	a0,t3
	cache   CACH_SD | C_HWB, 0x0(a0)

	move	a0,t6
	cache   CACH_PD | C_HWBINV, 0x0(a0)

	move	a0,t6
	cache   CACH_SD | C_HWB, 0x0(a0)

	b	1b
	nop
	j	s0

END(cmd_scache2)








