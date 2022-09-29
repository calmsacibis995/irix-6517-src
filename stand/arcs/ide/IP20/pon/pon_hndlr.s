#ident	"IP20diags/pon/pon_hndlr.s:  $Revision: 1.5 $"

/*
 * pon_hndlr.s - power-on exception handler
 */

#include "asm.h"
#include "regdef.h"
#include "sys/cpu.h"
#include "sys/sbd.h"

LEAF(pon_handler)
	move	k0,ra

	bne	a0,1,1f
	la	a0,general_exception_msg
	jal	pon_puts
	b	2f
1:
	bne	a0,2,1f
	la	a0,cache_exception_msg
	jal	pon_puts
	b	2f
1:
	bne	a0,3,1f
	la	a0,tlb_exception_msg
	jal	pon_puts
	b	2f
1:
	bne	a0,4,1f
	la	a0,xtlb_exception_msg
	jal	pon_puts
2:
	/* R4000 chip registers */
	la	a0,epc_msg		# EPC
	jal	pon_puts
	.set	noreorder
	mfc0	a0,C0_EPC
	.set	reorder
	jal	pon_puthex
	la	a0,errepc_msg		# ErrorEPC
	jal	pon_puts
	.set	noreorder
	mfc0	a0,C0_ERROR_EPC
	.set	reorder
	jal	pon_puthex
	la	a0,badvaddr_msg		# BadVaddr
	jal	pon_puts
	.set	noreorder
	mfc0	a0,C0_BADVADDR
	.set	reorder
	jal	pon_puthex
	la	a0,ra_msg		# ra
	jal	pon_puts
	move	a0,k0
	jal	pon_puthex
	la	a0,cause_msg		# Cause
	jal	pon_puts
	.set	noreorder
	mfc0	a0,C0_CAUSE
	.set	reorder
	jal	pon_puthex
	la	a0,status_msg		# Status
	jal	pon_puts
	.set	noreorder
	mfc0	a0,C0_SR
	.set	reorder
	jal	pon_puthex
	la	a0,cacherr_msg		# CacheErr
	jal	pon_puts
	.set	noreorder
	mfc0	a0,C0_CACHE_ERR
	.set	reorder
	jal	pon_puthex	

	/* board registers */
	la	a0,cpu_parerr_msg	# CPU parity error register
	jal	pon_puts
	li	v0,PHYS_TO_K1(CPU_ERR_STAT)
	lw	a0,0(v0)
	jal	pon_puthex
	la	a0,gio_parerr_msg	# GIO parity error register
	jal	pon_puts
	li	v0,PHYS_TO_K1(GIO_ERR_STAT)
	lw	a0,0(v0)
	jal	pon_puthex
	la	a0,liostat0_msg		# local I/O interrupt status 0
	jal	pon_puts
	li	v0,PHYS_TO_K1(LIO_0_ISR_ADDR)
	lbu	a0,0(v0)
	jal	pon_puthex
	la	a0,liostat1_msg		# local I/O interrupt status 1
	jal	pon_puts
	li	v0,PHYS_TO_K1(LIO_1_ISR_ADDR)
	lbu	a0,0(v0)
	jal	pon_puthex
	la	a0,cpuaddr_msg		# CPU error address register
	jal	pon_puts
	li	v0,PHYS_TO_K1(CPU_ERR_ADDR)
	lw	a0,0(v0)
	jal	pon_puthex
	la	a0,gioaddr_msg		# GIO error address register
	jal	pon_puts
	li	v0,PHYS_TO_K1(GIO_ERR_ADDR)
	lw	a0,0(v0)
	jal	pon_puthex
	la	a0,crlf
	jal	pon_puts

1:	b	1b			# and loop forever....
	END(pon_handler) 

/*
 * The power-on exception message looks like this:

EPC: 0xnnnnnnnn, ErrEPC: 0xnnnnnnnn, BadVaddr: 0xnnnnnnnn, RA: 0xnnnnnnnn
Cause: 0xnnnnnnnn, Status: 0xnnnnnnnn, CacheErr: 0xnnnnnnnn
CpuParityErr: 0xnnnnnnnn, GioParityErr: 0xnnnnnnnn
LIOstatus0: 0xnnnnnnnn, LIOstatus1: 0xnnnnnnnn
CpuErrorAddr: 0xnnnnnnnn, GioErrorAddr: 0xnnnnnnnn
 */

epc_msg:
	.asciiz "\r\nEPC: "
errepc_msg:
	.asciiz ", ErrEPC: "
badvaddr_msg:
	.asciiz ", BadVaddr: "
ra_msg:
	.asciiz	", RA: "
cause_msg:
	.asciiz "\r\nCause: "
status_msg:
	.asciiz ", Status: "
cacherr_msg:
	.asciiz ", CacheErr: "
cpu_parerr_msg:
	.asciiz	"\r\nCpuParityErr: "
gio_parerr_msg:
	.asciiz	", GioParityErr: "
liostat0_msg:
	.asciiz	"\r\nLIOstatus0: "
liostat1_msg:
	.asciiz	", LIOstatus1: "
cpuaddr_msg:
	.asciiz	"\r\nCpuErrorAddr: "
gioaddr_msg:
	.asciiz	", GioErrorAddr: "
general_exception_msg:
	.asciiz "\r\nException: <vector=Normal>"
cache_exception_msg:
	.asciiz "\r\nException: <vector=ECC>"
tlb_exception_msg:
	.asciiz "\r\nException: <vector=UTLB Miss>"
xtlb_exception_msg:
	.asciiz "\r\nException: <vector=XUT>"
