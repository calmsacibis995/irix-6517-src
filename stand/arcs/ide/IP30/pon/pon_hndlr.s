#ident	"ide/IP30/pon/pon_hndlr.s:  $Revision: 1.13 $"

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
	LA	a0,general_exception_msg
	jal	pon_puts
	b	2f

1:
	bne	a0,EXCEPT_ECC,1f
	LA	a0,cache_exception_msg
	jal	pon_puts

	LA	a0,errepc_msg		# ErrorEPC
	jal	pon_puts
	.set	noreorder
	DMFC0	(a0,C0_ERROR_EPC)
	.set	reorder
	jal	pon_puthex64

	LA	a0,cacherr_msg		# CacheErr
	jal	pon_puts
	.set	noreorder
	MFC0	(a0,C0_CACHE_ERR)
	.set	reorder
	jal	pon_puthex	

	b	3f

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
	/* Default is NMI with no message (see pon_nmi_handler) */
	bne	a0,EXCEPT_NMI,1f
	LA	a0,nmi_exception_msg
	jal	pon_puts
1:	LA	a0,errepc_msg		# ErrorEPC
	jal	pon_puts
	.set	noreorder
	DMFC0	(a0,C0_ERROR_EPC)
	.set	reorder
	jal	pon_puthex64
	/* FALLSTHROUGH to EPC just in case it is interesting */

2:
	LA	a0,epc_msg		# EPC
	jal	pon_puts
	.set	noreorder
	DMFC0	(a0,C0_EPC)
	.set	reorder
	jal	pon_puthex64

	LA	a0,badvaddr_msg		# BadVaddr
	jal	pon_puts
	.set	noreorder
	DMFC0	(a0,C0_BADVADDR)
	.set	reorder
	jal	pon_puthex64

3:
	LA	a0,ra_msg		# ra
	jal	pon_puts
	move	a0,k0
	jal	pon_puthex64

	LA	a0,cause_msg		# Cause
	jal	pon_puts
	.set	noreorder
	MFC0	(a0,C0_CAUSE)
	.set	reorder
	jal	pon_puthex

	LA	a0,status_msg		# Status
	jal	pon_puts
	.set	noreorder
	MFC0	(a0,C0_SR)
	.set	reorder
	jal	pon_puthex

#define	PRINT_REG32(msg,addr)		\
	.data;				\
99:	.asciiz	msg;			\
	.text;				\
	LA	a0,99b;			\
	jal	pon_puts;		\
	LI	a0,addr;		\
	lw	a0,0(a0);		\
	jal	pon_puthex

#define	PRINT_REG64(msg,addr)		\
	.data;				\
99:	.asciiz	msg;			\
	.text;				\
	LA	a0,99b;			\
	jal	pon_puts;		\
	LI	a0,addr;		\
	ld	a0,0(a0);		\
	jal	pon_puthex64

	/* HEART interrupt/exception related registers */
	PRINT_REG64("\r\nHEART ISR: ",
		PHYS_TO_COMPATK1(HEART_ISR))
	PRINT_REG64(", Cause: ",
		PHYS_TO_COMPATK1(HEART_CAUSE))
	PRINT_REG64("\r\n  Pbus Addr: ",
		PHYS_TO_COMPATK1(HEART_BERR_ADDR))
	PRINT_REG64(", Pbus Misc: ",
		PHYS_TO_COMPATK1(HEART_BERR_MISC))
	PRINT_REG64("\r\n  Mem Addr/Data: ",
		PHYS_TO_COMPATK1(HEART_MEMERR_ADDR))
	PRINT_REG64(", Bad Mem Data: ",
		PHYS_TO_COMPATK1(HEART_MEMERR_DATA))
	PRINT_REG64("\r\n  PIUR Acc: ",
		PHYS_TO_COMPATK1(HEART_PIUR_ACC_ERR))
	PRINT_REG32("\r\n  Widget Err Cmd: ",
		PHYS_TO_COMPATK1(HEART_WID_ERR_CMDWORD))
	PRINT_REG32(", Widget Err Addr: ",
		PHYS_TO_COMPATK1(HEART_WID_ERR_UPPER))
	PRINT_REG32("_",
		PHYS_TO_COMPATK1(HEART_WID_ERR_LOWER))
	PRINT_REG32("\r\n  Widget Err Type: ",
		PHYS_TO_COMPATK1(HEART_WID_ERR_TYPE))
	PRINT_REG32(", Widget PIO Err Addr: ",
		PHYS_TO_COMPATK1(HEART_WID_PIO_ERR_UPPER))
	PRINT_REG32("_",
		PHYS_TO_COMPATK1(HEART_WID_PIO_ERR_LOWER))
	PRINT_REG32("\r\n  Widget PIO Read Timeout Addr: ",
		PHYS_TO_COMPATK1(HEART_WID_PIO_RTO_ADDR))

	/* XBOW interrupt/exception related registers */
	PRINT_REG32("\r\nXbow ISR: ",
		PHYS_TO_COMPATK1(XBOW_BASE+XBOW_WID_STAT))
	PRINT_REG32("\r\n  Widget Err Cmd: ",
		PHYS_TO_COMPATK1(XBOW_BASE+XBOW_WID_ERR_CMDWORD))
	PRINT_REG32(", Widget Err Addr: ",
		PHYS_TO_COMPATK1(XBOW_BASE+XBOW_WID_ERR_UPPER))
	PRINT_REG32("_",
		PHYS_TO_COMPATK1(XBOW_BASE+XBOW_WID_ERR_LOWER))

	/* Bridge interrupt/exception related registers */
	PRINT_REG32("\r\nBridge ISR: ",
		PHYS_TO_COMPATK1(BRIDGE_BASE+BRIDGE_INT_STATUS))
	PRINT_REG32("\r\n  Widget Err Cmd: ",
		PHYS_TO_COMPATK1(BRIDGE_BASE+BRIDGE_WID_ERR_CMDWORD))
	PRINT_REG32(", Widget Err Addr: ",
		PHYS_TO_COMPATK1(BRIDGE_BASE+BRIDGE_WID_ERR_UPPER))
	PRINT_REG32("_",
		PHYS_TO_COMPATK1(BRIDGE_BASE+BRIDGE_WID_ERR_UPPER))
	PRINT_REG32("\r\n  Aux Err Cmd: ",
		PHYS_TO_COMPATK1(BRIDGE_BASE+BRIDGE_WID_AUX_ERR))
	PRINT_REG32(", Resp Buf Err Addr: ",
		PHYS_TO_COMPATK1(BRIDGE_BASE+BRIDGE_WID_RESP_UPPER))
	PRINT_REG32("_",
		PHYS_TO_COMPATK1(BRIDGE_BASE+BRIDGE_WID_RESP_LOWER))
	PRINT_REG32("\r\n  PCI/GIO Err Addr: ",
		PHYS_TO_COMPATK1(BRIDGE_BASE+BRIDGE_PCI_ERR_UPPER))
	PRINT_REG32("_",
		PHYS_TO_COMPATK1(BRIDGE_BASE+BRIDGE_PCI_ERR_LOWER))

	/* IOC3 interrupt/exception related registers */
	PRINT_REG32("\r\nIOC3 SCR: ",
		PHYS_TO_COMPATK1(IOC3_PCI_CFG_BASE+IOC3_PCI_SCR))
	PRINT_REG32("\r\n  PCI Err Addr: ",
		PHYS_TO_COMPATK1(IOC3_PCI_CFG_BASE+IOC3_PCI_ERR_ADDR_H))
	PRINT_REG32("_",
		PHYS_TO_COMPATK1(IOC3_PCI_CFG_BASE+IOC3_PCI_ERR_ADDR_L))

	LA	a0,crlf
	jal	pon_puts

1:
	j	powerspin

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
nmi_exception_msg:
	.asciiz "\r\nException: <vector=NMI>"

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
