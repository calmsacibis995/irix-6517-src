
/* 
 * pon_regs.s - short test of data paths to onboard devices,
 *	assuming that the CPU-PIC-HPC-DUART path is functional
 */

#include "sys/asm.h"
#include "early_regdef.h"
#include "sys/cpu.h"
#include "sys/sbd.h"

#define BYTE0	0xa5
#define BYTE1	0x5a

#define WD_SCSI_ADDR0		SCSI0A_ADDR	/* do scsi 0 only; test of */
#define WD_SCSI_ADDR1		SCSI0D_ADDR	/* second scsi in scsi_diag.c code */
#ifdef	_MIPSEB
#define SEEQ_ENET_RX_ADDR	0x1fb8005b
#define SEEQ_ENET_TX_ADDR	0x1fb8005f
#define RTC_NVRAM_ADDR		(RT_CLOCK_ADDR+0x57)
#else	/* _MIPSEL */
#define SEEQ_ENET_RX_ADDR	0x1fb80058
#define SEEQ_ENET_TX_ADDR	0x1fb8005c
#define RTC_NVRAM_ADDR		(RT_CLOCK_ADDR+0x54)
#endif	/* _MIPSEL */
#define INT_LIOMASK_ADDR	LIO_0_MASK_ADDR	
#define INT_VMEMASK_ADDR 	VME_1_MASK_ADDR	

#define SCSI_FAIL	1
#define ENET_FAIL	2
#define RTC_FAIL	4
#define INT_FAIL	8

LEAF(pon_regs)
	move	RA2,ra	
	move	T22,zero
	.set noreorder
	move	T30,fp				# set new exception hndlr
	la	fp,regs_hndlr
	.set reorder

	/* test scsi path - write and readback value of
	 * timeout register
	 */

	/* first clear any SCSI interrupts by reading the 
	 * status register 
	 */
	li	T20,0x17			# SCSI reg 0x17 - status
	li	T21,PHYS_TO_K1(SCSI0A_ADDR)
	sb	T20,0(T21)
	li	T21,PHYS_TO_K1(SCSI0D_ADDR)
	lb	zero,0(T21)

	li	T20,PHYS_TO_K1(WD_SCSI_ADDR0)	# select timeout reg
	li	T21,2
	sb	T21,0(T20)

	li	T20,PHYS_TO_K1(WD_SCSI_ADDR1)	# write value
	li	T21,BYTE0
	sb	T21,0(T20)

	li	T20,PHYS_TO_K1(WD_SCSI_ADDR0)	# select timeout reg
	li	T21,2
	sb	T21,0(T20)

	li	T20,PHYS_TO_K1(WD_SCSI_ADDR1)	# readback value
	lbu	T21,0(T20)

	beq	T21,BYTE0,1f
	ori	T22,SCSI_FAIL

1:	
	/* test enet path - read value of tx status
	 * write value of rx cmd
	 * if data path is bad, maybe a bus error will get
	 * generated
	 */
	li	T20,PHYS_TO_K1(SEEQ_ENET_TX_ADDR)	# read tx status reg
	lbu	T21,0(T20)
	li	T20,PHYS_TO_K1(SEEQ_ENET_RX_ADDR)	# write rx control reg
	li	T21,BYTE1
	sb	T21,0(T20)

	/* test RTC path - write and readback values to two registers
	 */
	li	T20,PHYS_TO_K1(RTC_NVRAM_ADDR)	# write rtc nvram
	lbu	T23,0(T20)			# save byte 0 of nvram
	lbu	a0,4(T20)			# save byte 1 of nvram
	li	T21,BYTE0			# write new data to nvram
	sb	T21,0(T20)
	li	T21,BYTE1
	sb	T21,4(T20)
	move	T21,zero
	lbu	T21,0(T20)
	bne	T21,BYTE0,1f
	lbu	T21,4(T20)
	bne	T21,BYTE1,1f
	b	2f

1:	ori	T22,RTC_FAIL

2: 	sb	T23,0(T20)			# restore rtc data
	sb	a0,4(T20)

	/* test INT2 path - lio mask 0 and vme mask 1
	 * write & readback values
	 */
	li	T20,PHYS_TO_K1(INT_LIOMASK_ADDR)	# write lio mask
	li	T23,PHYS_TO_K1(INT_VMEMASK_ADDR)	# write vme mask
	li	T21,BYTE0			# write new data to INT2
	sb	T21,0(T20)
	li	T21,BYTE1
	sb	T21,0(T23)
	move	T21,zero
	lbu	T21,0(T20)
	bne	T21,BYTE0,1f
	lbu	T21,0(T23)
	bne	T21,BYTE1,1f
	b	2f

1:	ori	T22,INT_FAIL

2: 	sb	zero,0(T20)
	sb	zero,0(T23)

	/* tests done, print messages for any errant paths
	 */
	beqz	T22,regs_done
	la	a0,reg_msg
	jal	pon_puts
	la	a0,failed
	jal	pon_puts

	and	T20,T22,SCSI_FAIL
	beqz	T20,1f
	la	a0,scsifail_msg
	jal	pon_puts

1: 	and	T20,T22,RTC_FAIL
	beqz	T20,1f
	la	a0,rtcfail_msg
	jal	pon_puts

1:	and	T20,T22,INT_FAIL
	beqz	T20,regs_done
	la	a0,intfail_msg
	jal	pon_puts

1:	b	1b				# don't continue

regs_done:
	.set noreorder				# restore old exception hndlr
	move	fp,T30
	.set reorder
	move	ra,RA2
	j	ra
	END(pon_regs)

LEAF(regs_hndlr)
	la	a0,reg_msg
	jal	pon_puts
	la	a0,reg_fail_exc_msg
	jal	pon_puts
	j	T30
	/* exception this early never returns */
	END(regs_hndlr)

	.data
scsifail_msg:
	.asciiz "\tWD SCSI path test \t           *FAILED*\r\n"
rtcfail_msg:
	.asciiz "\tRTC path test \t\t           *FAILED*\r\n"
intfail_msg:
	.asciiz "\tINT path test \t\t           *FAILED*\r\n"
reg_msg:
	.asciiz "Data path test\t\t\t    "
reg_fail_exc_msg:
	.asciiz "       *FAILED* with EXCEPTION:\r\n"
