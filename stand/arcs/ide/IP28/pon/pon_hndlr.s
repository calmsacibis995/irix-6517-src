#ident	"IP22diags/pon/pon_hndlr.s:  $Revision: 1.12 $"

/*
 * pon_hndlr.s - power-on exception handler
 */

#include "asm.h"
#include "early_regdef.h"
#include "regdef.h"
#include "fault.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "ml.h"

	AUTO_CACHE_BARRIERS_DISABLE	# all code runs early and is fatal

LEAF(pon_handler)
	move	k0,ra

	/* turn LED amber to give people w/o a tty a chance. */
	CLI	a2,PHYS_TO_COMPATK1(HPC3_WRITE1)
	li	a1,(LED_AMBER|PAR_RESET|KBD_MS_RESET|EISA_RESET)
	sw	a1,0(a2)

	bne	a0,EXCEPT_NORM,1f
	LA	a0,general_exception_msg
	jal	pon_puts
	b	2f
1:
	bne	a0,EXCEPT_ECC,1f
	LA	a0,cache_exception_msg
	jal	pon_puts
	b	2f
1:
	bne	a0,EXCEPT_NMI,1f
	LA	a0, nmi_exception_msg
	jal	pon_puts

	/* on old IP26 baseboard, skip ECC status checking */
	CLI	a0,PHYS_TO_COMPATK1(HPC3_SYS_ID)# board revision info
	lw	a0,0(a0)
	andi	a0,BOARD_REV_MASK		# isolate board rev
	sub	a0,IP26_ECCSYSID		# IP26 ecc bd = 0x18
	bgtz	a0,2f				# on IP26, skip to 

	/* ECC error probably (can't really tell if manual NMI) */
	lw	a0,PHYS_TO_COMPATK1(ECC_STATUS)
	and	a0,ECC_STATUS_UC_WR
	beqz	a0,9f
	LA	a0,nmi_uc_wr
	jal	pon_puts
	b	2f
9:	lw	a0,PHYS_TO_COMPATK1(ECC_STATUS)
	and	a0,ECC_STATUS_GIO
	beqz	a0,9f
	LA	a0,nmi_mb_gio
	jal	pon_puts
	b	2f
9:	LA	a0,nmi_mb_cpu
	jal	pon_puts
	b	2f

1:
	bne	a0,EXCEPT_UTLB,1f
	LA	a0,tlb_exception_msg
	jal	pon_puts
	b	2f
1:
	bne	a0,EXCEPT_XUT,1f
	LA	a0,xtlb_exception_msg
	jal	pon_puts
	b	2f
1:
	LA	a0,misc_exception_msg
	jal	pon_puts
	/*FALLSTHROUGH*/
2:
	/* R10K chip registers */
	LA	a0,epc_msg		# EPC
	jal	pon_puts
	.set	noreorder
	dmfc0	a0,C0_EPC
	.set	reorder
	jal	pon_puthex64
	LA	a0,errepc_msg		# ErrorEPC
	jal	pon_puts
	.set	noreorder
	dmfc0	a0,C0_ERROR_EPC
	.set	reorder
	jal	pon_puthex64
	LA	a0,status_msg		# Status
	jal	pon_puts
	.set	noreorder
	mfc0	a0,C0_SR
	.set	reorder
	jal	pon_puthex
	LA	a0,ra_msg		# ra
	jal	pon_puts
	move	a0,k0
	jal	pon_puthex64
	LA	a0,badvaddr_msg		# BadVaddr
	jal	pon_puts
	.set	noreorder
	mfc0	a0,C0_BADVADDR
	.set	reorder
	jal	pon_puthex64
	LA	a0,cause_msg		# Cause
	jal	pon_puts
	.set	noreorder
	mfc0	a0,C0_CAUSE
	.set	reorder
	jal	pon_puthex
	LA	a0,cacherr_msg		# CacheErr
	jal	pon_puts
	.set	noreorder
	mfc0	a0,C0_CACHE_ERR
	.set	reorder
	jal	pon_puthex	
	LA	a0,config_msg		# CPU config register
	jal	pon_puts
	.set	noreorder
	mfc0	a0,C0_CONFIG
	.set	reorder
	jal	pon_puthex

	/* board registers */
	LA	a0,cpu_memacc_msg	# CPU_MEMACC
	jal	pon_puts
	CLI	v0,PHYS_TO_COMPATK1(CPU_MEMACC)
	lw	a0,0(v0)
	jal	pon_puthex
	LA	a0,gio_memacc_msg	# GIO_MEMACC
	jal	pon_puts
	CLI	v0,PHYS_TO_COMPATK1(GIO_MEMACC)
	lw	a0,0(v0)
	jal	pon_puthex
	LA	a0,cpu_ctrl0_msg	# CPUCTRL0
	jal	pon_puts
	CLI	v0,PHYS_TO_COMPATK1(CPUCTRL0)
	lw	a0,0(v0)
	jal	pon_puthex

	LA	a0,cpu_parerr_msg	# CPU parity error register
	jal	pon_puts
	CLI	v0,PHYS_TO_COMPATK1(CPU_ERR_STAT)
	lw	a0,0(v0)
	jal	pon_puthex
	LA	a0,gio_parerr_msg	# GIO parity error register
	jal	pon_puts
	CLI	v0,PHYS_TO_COMPATK1(GIO_ERR_STAT)
	lw	a0,0(v0)
	jal	pon_puthex

	LA	a0,cpuaddr_msg		# CPU error address register
	jal	pon_puts
	CLI	v0,PHYS_TO_COMPATK1(CPU_ERR_ADDR)
	lw	a0,0(v0)
	jal	pon_puthex
	LA	a0,gioaddr_msg		# GIO error address register
	jal	pon_puts
	CLI	v0,PHYS_TO_COMPATK1(GIO_ERR_ADDR)
	lw	a0,0(v0)
	jal	pon_puthex

	CLI	T20,PHYS_TO_COMPATK1(HPC3_INT2_ADDR)
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
	LA	a0,crlf
	jal	pon_puts

1:	b	1b			# and loop forever....
	END(pon_handler) 

	AUTO_CACHE_BARRIERS_ENABLE

/*
 * The power-on exception message looks like this:

EPC: 0xnnnnnnnnnnnnnnnn, ErrEPC:   0xnnnnnnnnnnnnnnnn, Status: 0xnnnnnnnn
RA:  0xnnnnnnnnnnnnnnnn, BadVaddr: 0xnnnnnnnnnnnnnnnn, Cause:  0xnnnnnnnn
CacheErr:     0xnnnnnnnn, Config:       0xnnnnnnnn
CpuMemAcc:    0xnnnnnnnn, GioMemAcc:    0xnnnnnnnn, CpuCtrl0: 0xnnnnnnnn
CpuError:     0xnnnnnnnn, GioError:     0xnnnnnnnn
CpuErrorAddr: 0xnnnnnnnn, GioErrorAddr: 0xnnnnnnnn
LIOstatus0:   0xnnnnnnnn, LIOstatus1:   0xnnnnnnnn, LIOstatus2: 0xnnnnnnnn
 *
 */

	.data

epc_msg:
	.asciiz "\r\nEPC: "
errepc_msg:
	.asciiz ", ErrEPC:   "
status_msg:
	.asciiz ", Status: "
ra_msg:
	.asciiz	"\r\nRA:  "
badvaddr_msg:
	.asciiz ", BadVaddr: "
cause_msg:
	.asciiz ", Cause:  "
cacherr_msg:
	.asciiz "\r\nCacheErr:     "
config_msg:
	.asciiz ", Config:       "
cpu_parerr_msg:
	.asciiz	"\r\nCpuError:     "
gio_parerr_msg:
	.asciiz	", GioError:     "
cpu_memacc_msg:
	.asciiz "\r\nCpuMemAcc:    "
gio_memacc_msg:
	.asciiz ", GioMemAcc:    "
cpu_ctrl0_msg:
	.asciiz ", CpuCtrl0: "
liostat0_msg:
	.asciiz	"\r\nLIOstatus0:   "
liostat1_msg:
	.asciiz	", LIOstatus1:   "
liostat2_msg:
	.asciiz	", LIOstatus2: "
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
	.asciiz "\r\nException: <vector=XUTLB>"
nmi_exception_msg:
	.asciiz "\r\nException: <vector=NMI>"
misc_exception_msg:
	.asciiz "\r\nException: <vector=???>"

nmi_mb_gio:
	.asciiz "\r\nMulti-bit error (GIO side)"
nmi_mb_cpu:
	.asciiz "\r\nMulti-bit error (CPU side)"
nmi_uc_wr:
	.asciiz "\r\nIllegal uncached write in fast mode"
