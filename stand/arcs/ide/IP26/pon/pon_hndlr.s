/*
 * pon_hndlr.s - power-on exception handler
 */

#ident "$Revision: 1.11 $"

#include "sys/cpu.h"
#include "sys/sbd.h"
#include "asm.h"
#include "early_regdef.h"
#include "regdef.h"

LEAF(pon_handler)
	/* Just count and then continue from GCache errors (both TCC and
	 * CPU detected).
	 */
	.set	noreorder
	.set	noat

	dmtc0	k1,C0_GBASE			# save k1 (error type)

	LI	k0,PHYS_TO_K1(TCC_INTR)		# clear tcc machine checks?
	ld	k1,0(k0)
	andi	k1,INTR_MACH_CHECK		# set if we have a problem
	beqz	k1,1f
	ld	k1,0(k0)			# BDSLOT: reget contetns
	b	gerror_eret			# count and eret
	sd	k1,0(k0)			# BDSLOT: clear error

1:	LI	k0,SR_IBIT9|SR_IBIT10
	dmfc0	k1,C0_CAUSE
	ssnop
	and	k0,k1,k0
	beqz	k0,1f
	nada

	LI	k0,~(SR_IBIT9|SR_IBIT10)
	and	k1,k1,k0
	dmtc0	k1,C0_CAUSE			# clear error
	ssnop; ssnop

gerror_eret:
	dmfc0	k0,TETON_GERROR			# increment error cnt
	ssnop
	daddi	k0,1				# assumes < 16 bits
	ssnop
	dmtc0	k0,TETON_GERROR
	ssnop; ssnop; ssnop
	eret					# return from error
	ssnop; ssnop; ssnop

	.set	at
	.set	reorder

1:
	.set	noreorder
	dmfc0	k1,C0_GBASE
	ssnop
	.set	reorder
	move	k0,ra

	bne	k1,1,1f
	LA	a0,general_exception_msg
	jal	pon_puts
	b	2f
1:
	bne	k1,2,1f
	LA	a0,ktblrefill_global_msg
	jal	pon_puts
	b	2f
1:
	bne	k1,3,1f
	LA	a0,ktblrefill_private_msg
	jal	pon_puts
	b	2f
1:
	bne	k1,4,1f
	LA	a0,tlbrefill_msg
	jal	pon_puts
2:
	/* TFP chip registers
	 */
	LA	a0,epc_msg		# EPC
	jal	pon_puts
	.set	noreorder
	dmfc0	a0,C0_EPC
	ssnop
	.set	reorder
	jal	pon_puthex64

	LA	a0,vaddr_msg		# BadVAddr
	jal	pon_puts
	.set	noreorder
	dmfc0	a0,C0_BADVADDR
	ssnop
	.set	reorder
	jal	pon_puthex64

	LA	a0,badpaddr_msg		# BadPAddr
	jal	pon_puts
	.set	noreorder
	dmfc0	a0,C0_BADPADDR
	ssnop
	.set	reorder
	jal	pon_puthex64

	LA	a0,ra_msg		# ra
	jal	pon_puts
	move	a0,k0
	jal	pon_puthex64

	LA	a0,cause_msg		# Cause
	jal	pon_puts
	.set	noreorder
	dmfc0	a0,C0_CAUSE
	ssnop
	.set	reorder
	jal	pon_puthex64

	LA	a0,status_msg		# Status
	jal	pon_puts
	.set	noreorder
	dmfc0	a0,C0_SR
	ssnop
	.set	reorder
	jal	pon_puthex64

	/* tcc registers
	 */
	LI	T41,PHYS_TO_K1(TCC_BASE)

	LA	a0,tccintr_msg
	jal	pon_puts
	ld	a0,TCC_INTR-TCC_BASE(T41)
	jal	pon_puthex

	LA	a0,tccerror_msg
	jal	pon_puts
	ld	a0,TCC_ERROR-TCC_BASE(T41)
	jal	pon_puthex

	LA	a0,tcc_be_addr_msg
	jal	pon_puts
	LI	v0,PHYS_TO_K1(TCC_BE_ADDR)
	ld	a0,TCC_BE_ADDR-TCC_BASE(T41)
	jal	pon_puthex

	LA	a0,tcc_parity_msg
	jal	pon_puts
	LI	v0,PHYS_TO_K1(TCC_PARITY)
	ld	a0,TCC_PARITY-TCC_BASE(T41)
	jal	pon_puthex

	/* board/asic registers
	 */
	LA	a0,cpu_parerr_msg	# CPU parity error register
	jal	pon_puts
	LI	v0,PHYS_TO_K1(CPU_ERR_STAT)
	lw	a0,0(v0)
	jal	pon_puthex

	LA	a0,gio_parerr_msg	# GIO parity error register
	jal	pon_puts
	LI	v0,PHYS_TO_K1(GIO_ERR_STAT)
	lw	a0,0(v0)
	jal	pon_puthex

#ifdef IP26_IOC
	# Get INT2/3 base
	LI	T20, PHYS_TO_K1(HPC3_INT3_ADDR)	# assume IOC1/INT3
	IS_IOC1(v0)
	bnez	v0, 1f				# branch if IOC1/INT3
	LI	T20,PHYS_TO_K1(HPC3_INT2_ADDR)	# use INT2
1:
#else
	LI	T20,PHYS_TO_K1(HPC3_INT2_ADDR)	# use INT2
#endif

	LA	a0,liostat0_msg		# local I/O interrupt status 0
	jal	pon_puts
	lbu	a0,LIO_0_ISR_OFFSET(T20)
	jal	pon_puthex

	LA	a0,liostat1_msg		# local I/O interrupt status 1
	jal	pon_puts
	lbu	a0,LIO_1_ISR_OFFSET(T20)
	jal	pon_puthex

	LA	a0,liostat2_msg		# local I/O interrupt status 2
	jal	pon_puts
	lbu	a0,LIO_2_3_ISR_OFFSET(T20)
	jal	pon_puthex

	LA	a0,cpuaddr_msg		# CPU error address register
	jal	pon_puts
	LI	v0,PHYS_TO_K1(CPU_ERR_ADDR)
	lw	a0,0(v0)
	jal	pon_puthex

	LA	a0,gioaddr_msg		# GIO error address register
	jal	pon_puts
	LI	v0,PHYS_TO_K1(GIO_ERR_ADDR)
	lw	a0,0(v0)
	jal	pon_puthex
	LA	a0,crlf
	jal	pon_puts

1:	b	1b			# and loop forever....
	END(pon_handler) 

/*
 * The power-on exception message looks like this:
EPC: 0xnnnnnnnnnnnnnnn BadVAddr: 0xnnnnnnnnnnnnnnn BadPAddr: 0xnnnnnnnnnnnnnnn
RA: 0xnnnnnnnnnnnnnnn Cause: 0xnnnnnnnnnnnnnnn Status: 0xnnnnnnnnnnnnnnn
Tcc Intr: 0xnnnnnnnn Error: 0xnnnnnnnn ErrorAddr: 0xnnnnnnnn Parity: 0xnnnnnnnn
CpuErrStat: 0xnnnnnnnn GioErrStat: 0xnnnnnnnn
LIOstatus0: 0xnnnnnnnn LIOstatus1: 0xnnnnnnnn LIOstatus2: 0xnnnnnnnn
CpuErrorAddr: 0xnnnnnnnn GioErrorAddr: 0xnnnnnnnn
 */

	.data
epc_msg:
	.asciiz "\r\nEPC: "
vaddr_msg:
	.asciiz " BadVAddr: "
badpaddr_msg:
	.asciiz " BadPAddr: "
ra_msg:
	.asciiz	"\r\nRA: "
cause_msg:
	.asciiz " Cause: "
status_msg:
	.asciiz " Status: "
tccintr_msg:
	.asciiz "\r\nTcc Intr: "
tccerror_msg:
	.asciiz " Error: "
tcc_be_addr_msg:
	.asciiz " ErrorAddr: "
tcc_parity_msg:
	.asciiz " Parity: "
cpu_parerr_msg:
	.asciiz	"\r\nCpuErrStat: "
gio_parerr_msg:
	.asciiz	" GioErrStat: "
liostat0_msg:
	.asciiz	"\r\nLIOstatus0: "
liostat1_msg:
	.asciiz	" LIOstatus1: "
liostat2_msg:
	.asciiz	" LIOstatus2: "
cpuaddr_msg:
	.asciiz	"\r\nCpuErrorAddr: "
gioaddr_msg:
	.asciiz	" GioErrorAddr: "
general_exception_msg:
	.asciiz "\r\nException: <vector=Normal>"
ktblrefill_global_msg:
	.asciiz "\r\nException: <vector=Global KTLB>"
ktblrefill_private_msg:
	.asciiz "\r\nException: <vector=Private KTLB>"
tlbrefill_msg:
	.asciiz "\r\nException: <vector=TLB>"
