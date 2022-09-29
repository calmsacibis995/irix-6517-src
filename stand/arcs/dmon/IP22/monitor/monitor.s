#ident	"$Id: monitor.s,v 1.1 1994/07/21 00:18:27 davidl Exp $"
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

#include "monitor.h"
#include "pdiag.h"


	.align	4

   	.text

	.extern maintest_menu
	.extern	test_table
	.extern	cmd_table
	.extern	print_menu

	.globl	warm_reset
	.globl	_nvstack_overflow
	.globl	_nvstack_underflow
	.globl	_test_return
	.globl	test_return


LEAF(main)
	RESETVECTOR()			# reset fault vector
	/* 
	 * set variable default values 
 	 */
	la	a0, maintest_menu	# set modname to main test menu
	li	a1, _MODNAME
	jal	sw_nvram
	nop

	la	a0,test_table		# set the main test table to _TTBLPTR 
	li	a1,_TTBLPTR
	jal	sw_nvram

	la	a0,cmd_table		# set the cmd table to _CMDTBLPTR 
	li	a1,_CMDTBLPTR
	jal	sw_nvram

	li	a0, ERRLIMIT_DEFAULT	# error limit default
	li	a1, _ERRORLMT
	jal	sw_nvram

	li	a0, LOOPLMT_DEFAULT	# loop-limit default
	li	a1, _LOOPLMT
	jal	sw_nvram

	and	CnfgStatusReg, ~ERRMODE_MASK
	li	a0,ERRMODE_DEFAULT	# on-error mode default
	or	CnfgStatusReg,a0

	move	a0,CnfgStatusReg
	li	a1,_CNFGSTATUS		# set _CNFGSTATUS
	jal	sw_nvram

/*
	jal	SizeMemory		# Size main memory
	move	s0, v0
	move	a0, v0
	li	a1, _MEMSIZE
	jal	sw_nvram

*/
	li	a0, 0xa8000000		# FIRSTADDR default = 0xa0000000
	li	a1, _FIRSTADDR
	jal	sw_nvram

	li	a0, 0xa8fffffc		# LASTADDR = MEMSIZE | K1BASE
	li	a1, _LASTADDR
	jal	sw_nvram
 
	#jal	cmd_config		# show sys hardware configurations

	jal	print_menu		# display menu on reset

/*
 * reset monitor states
 */

warm_reset:

	RESETVECTOR()			# reset fault vector

	.set	noreorder
	mfc0	t0,C0_SR
	nop
	or	t0, SR_BEV		# set BEV bit
	mtc0	t0, C0_SR
	nop
	.set	reorder

	li	a0,_CNFGSTATUS
	jal	lw_nvram

	and	CnfgStatusReg,v0,(~TAFLAG_MASK) # reset test-active flag

	move	a0,CnfgStatusReg
	li	a1,_CNFGSTATUS
	jal	sw_nvram


	li	a0, NVSTACK_BASE	# reset nvstack ptr
	jal	addr_nvram		# get mapped addr of nvstack-top

	move	a0, v0
	li	a1, _NV_SP		# reset nvstack pointer to top
	jal	sw_nvram

	move	a0, zero		# reset runmode
	li	a1, _RUNMODE
	jal	sb_nvram

	jal	cmd_parser		# call command parser

	b	warm_reset

_nvstack_overflow:

	PRINT("\n\nRuntime Error: nvram stack overflow!!! \r\n")
	# jal	_pause
	b	warm_reset

_nvstack_underflow:
	PRINT("\n\nRuntime Error: nvram stack underflow!!! \r\n")
	# jal	_pause
	b	warm_reset

END(main)


LEAF(SizeMemory)

        move    a0, ra
        jal     push_nvram

        /*
         * Check if we need to initialize the memory first
         */
        li      a0, _DIAG_FLAG
        jal     lw_nvram
        and     a0, v0, DBIT_MEMINIT    # bit set if already done
        bne     a0, zero, 2f

        or      a0, v0, DBIT_MEMINIT    # set this bit
        li      a1, _DIAG_FLAG
        jal     sw_nvram

	jal	dmon_mem_init

2:
        jal     pop_nvram
        move    s5, v0

	jal	szmem
	j	s5

END(SizeMemory)
/* end */

