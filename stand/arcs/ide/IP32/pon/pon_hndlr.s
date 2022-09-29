#ident	"ide/IP30/pon/pon_hndlr.s:  $Revision: 1.1 $"

/*
 * pon_hndlr.s - power-on exception handler
 */

#include "asm.h"
#include "regdef.h"
#include "fault.h"
#include "sys/cpu.h"
#include "sys/sbd.h"

LEAF(pon_handler)
	move	k0,ra

	bne	a0,EXCEPT_NORM,1f
	dla	a0,general_exception_msg
	jal	pon_puts
	b	2f

1:
	bne	a0,EXCEPT_ECC,1f
	dla	a0,cache_exception_msg
	jal	pon_puts

	dla	a0,errepc_msg		# ErrorEPC
	jal	pon_puts
	.set	noreorder
	dmfc0	a0,C0_ERROR_EPC
	.set	reorder
	jal	pon_puthex64

	dla	a0,cacherr_msg		# CacheErr
	jal	pon_puts
	.set	noreorder
	mfc0	a0,C0_CACHE_ERR
	.set	reorder
	jal	pon_puthex	

	b	3f

1:
	bne	a0,EXCEPT_UTLB,1f
	dla	a0,tlb_exception_msg
	jal	pon_puts
	b	2f

1:
	dla	a0,xtlb_exception_msg
	jal	pon_puts

2:
	dla	a0,epc_msg		# EPC
	jal	pon_puts
	.set	noreorder
	dmfc0	a0,C0_EPC
	.set	reorder
	jal	pon_puthex64

	dla	a0,badvaddr_msg		# BadVaddr
	jal	pon_puts
	.set	noreorder
	dmfc0	a0,C0_BADVADDR
	.set	reorder
	jal	pon_puthex64

3:
	dla	a0,ra_msg		# ra
	jal	pon_puts
	move	a0,k0
	jal	pon_puthex64

	dla	a0,cause_msg		# Cause
	jal	pon_puts
	.set	noreorder
	mfc0	a0,C0_CAUSE
	.set	reorder
	jal	pon_puthex

	dla	a0,status_msg		# Status
	jal	pon_puts
	.set	noreorder
	mfc0	a0,C0_SR
	.set	reorder
	jal	pon_puthex

	/* XXX CRIME interrupt/exception related registers XXX */

	/* XXX MACE interrupt/exception related registers XXX */

1:
	b	1b			# and loop forever....
	END(pon_handler) 

	.data

general_exception_msg:
	.asciiz "\r\nException: <vector=Normal>"
cache_exception_msg:
	.asciiz "\r\nException: <vector=ECC>"
tlb_exception_msg:
	.asciiz "\r\nException: <vector=UTLB Miss>"
xtlb_exception_msg:
	.asciiz "\r\nException: <vector=XUTLB Miss>"

errepc_msg:
	.asciiz "\r\nErrEPC: "
cacherr_msg:
	.asciiz ", CacheErr: "

epc_msg:
	.asciiz "\r\nEPC: "
badvaddr_msg:
	.asciiz ", BadVaddr: "

ra_msg:
	.asciiz	", RA: "

cause_msg:
	.asciiz "\r\nCause: "
status_msg:
	.asciiz ", Status: "
config_msg:
	.asciiz	", Config: "

heart_isr_msg:
	.asciiz "\r\nHEART ISR: "
heart_cause_msg:
	.asciiz ", Cause: "


