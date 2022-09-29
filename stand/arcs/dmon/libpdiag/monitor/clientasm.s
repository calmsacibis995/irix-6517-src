#ident	"$Id: clientasm.s,v 1.1 1994/07/21 01:13:05 davidl Exp $"
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
 * clientasm.s -- assembler routines for transfering to and from client code
 *
 * This routine is a revised version of the one in SA/prom for diags monitor.
 */

#include "sys/regdef.h"
#include "sys/asm.h"
#include "sys/sbd.h"

#ifdef DEBUG
	.extern	_puts
	.extern puthex
#endif DEBUG

	.extern warm_reset
	.extern flush_cache

	.text
/*
 * client_start(environ, client_pc, client_sp)
 *
 * Always set argc & argv to zero.
 */
LEAF(client_start)
	move	s0, a0			# flush for r6000 changes a0/a1
	move	s1, a1
	move	s2, a2
	jal	flush_cache		# make sure i cache is consistent
	#
	# invoke the client by
	#
	#	client(argc, argv, environ)
	#
	move	a0, zero		# always set argc & argv to 0
	move	a1, zero
	move	a2, s0			# restore environ & client_pc
	move	a3, s1
	move	v0, s2			# client_sp
	bne	v0, zero, 1f		# if client_sp is nonzero, then use
	move	v0, sp			#  that as the sp; otherwise, use the
1:					#   current stack pointer
	li	v1,0xe0000000		# mask for high-order address bits
	and	v1,a3			#  isolate those bits...
	bne	v1,K0BASE,2f		# if entrypoint is K0, then use
	sll	v0,3			#  a K0 stackpointer
	srl	v0,3			#   otherwise don't alter sp
	or	v0,K0BASE		#    from what caller has specified
2:	
	move	sp,v0			# switch stacks

#ifdef DEBUG
	move	s0, a0
	move	s1, a1
	move	s2, a2
	move	s3, a3
	la	a0, _msg1
	jal	_puts
	move	a0, s2
	jal	puthex			# show environment ptr
	la	a0, _msg2
	jal	_puts
	move	a0, s3
	jal	puthex			# show client pc
	la	a0, _msg3
	jal	_puts
	move	a0, sp
	jal	puthex			# show client stack pointer
	la	a0, _nline
	jal	_puts
	move	a0, s0
	move	a1, s1
	move	a2, s2
	move	a3, s3
#endif DEBUG

	.set	noreorder
	mtc0	zero, C0_SR		# set C0_SR to desired states (BEV off)
	nop
	.set	reorder
	jal	a3			# call client

	j	warm_reset		# return to diags monitor

END(client_start)

	.data
_msg1:	.asciiz "\nclient_start: environ=0x"
_msg2:	.asciiz " client_pc=0x"
_msg3:	.asciiz " sp=0x"
_nline:	.asciiz "\n\n"
