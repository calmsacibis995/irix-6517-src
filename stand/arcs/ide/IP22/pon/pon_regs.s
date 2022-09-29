
/* 
 * pon_regs.s - short test of data paths to onboard devices,
 *	assuming that the CPU-PIC-HPC3-DUART path is functional
 */

#include "sys/asm.h"
#include "early_regdef.h"
#include "sys/cpu.h"
#include "sys/sbd.h"

#define BYTE0		0xa5
#define BYTE1		0x5a

#define WD_SCSI_ADDR0		SCSI0A_ADDR	/* do scsi 0 only; test of */
#define WD_SCSI_ADDR1		SCSI0D_ADDR	/* second scsi in scsi_diag.c code */
#ifdef	_MIPSEB
#define SEEQ_ENET_RX_ADDR	(HPC3_ETHER_REG+0x3)
#define SEEQ_ENET_TX_ADDR	(HPC3_ETHER_REG+0x7)
#define RTC_NVRAM_ADDR		((RT_CLOCK_ADDR+(0x3e*4))|0x3)
#else	/* _MIPSEL */
#define SEEQ_ENET_RX_ADDR	HPC3_ETHER_REG
#define SEEQ_ENET_TX_ADDR	(HPC3_ETHER_REG+4)
#define RTC_NVRAM_ADDR		(RT_CLOCK_ADDR+(0x3f*4))
#endif	/* _MIPSEL */

#define SCSI_FAIL	1
#define ENET_FAIL	2
#define RTC_FAIL	4
#define INT_FAIL	8

LEAF(pon_regs)
	move	RA2,ra	
	move	T22,zero
	.set noreorder
	move	T30,fp				# set new exception hndlr
	LA	fp,regs_hndlr
	.set reorder


	/* test scsi path - write and readback value of
	 * timeout register
	 */

	/* first clear any SCSI interrupts by reading the 
	 * status register 
	 */
	li	T20,0x17			# SCSI reg 0x17 - status
	LI	T21,PHYS_TO_K1(WD_SCSI_ADDR0)
	sb	T20,0(T21)
	LI	T21,PHYS_TO_K1(WD_SCSI_ADDR1)
	lb	zero,0(T21)

	LI	T20,PHYS_TO_K1(WD_SCSI_ADDR0)	# select timeout reg
	li	T21,2
	sb	T21,0(T20)

	LI	T20,PHYS_TO_K1(WD_SCSI_ADDR1)	# write value
	li	T21,BYTE0
	sb	T21,0(T20)

	LI	T20,PHYS_TO_K1(WD_SCSI_ADDR0)	# select timeout reg
	li	T21,2
	sb	T21,0(T20)

	LI	T20,PHYS_TO_K1(WD_SCSI_ADDR1)	# readback value
	lbu	T21,0(T20)

	beq	T21,BYTE0,1f
	ori	T22,SCSI_FAIL

1:	
	/* test enet path - read value of tx status
	 * write value of rx cmd
	 * if data path is bad, maybe a bus error will get
	 * generated
	 */
	LI	T20,PHYS_TO_K1(SEEQ_ENET_TX_ADDR)	# read tx status reg
	lbu	T21,0(T20)
	LI	T20,PHYS_TO_K1(SEEQ_ENET_RX_ADDR)	# write rx control reg
	li	T21,BYTE1
	sb	T21,0(T20)

	/* test RTC path - write and readback values to two registers
	 */
	LI	T20,PHYS_TO_K1(RTC_NVRAM_ADDR)	# write rtc nvram
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

	/* test INT2/INT3 path - lio mask 0 and 2
	 * write & readback values
	 */
	LI	T20,PHYS_TO_K1(HPC3_INT3_ADDR)		# assume IOC1/INT3
	IS_IOC1(T21)
	bnez	T21,10f					# branch if IOC1/INT3
	LI	T20,PHYS_TO_K1(HPC3_INT2_ADDR)		# use INT2
10:
	li	T21,BYTE0			# write new data to INT2/3
	sb	T21,LIO_0_MASK_OFFSET(T20)
	li	T21,BYTE1
	sb	T21,LIO_2_MASK_OFFSET(T20)
	move	T21,zero
	lbu	T21,LIO_0_MASK_OFFSET(T20)
	bne	T21,BYTE0,1f
	lbu	T21,LIO_2_MASK_OFFSET(T20)
	bne	T21,BYTE1,1f
	b	2f

1:	ori	T22,INT_FAIL

2: 	sb	zero,LIO_0_MASK_OFFSET(T20)
	sb	zero,LIO_2_MASK_OFFSET(T20)


	/* tests done, print messages for any errant paths
	 */
	beqz	T22,regs_done
	LA	a0,reg_msg
	jal	pon_puts
	LA	a0,failed
	jal	pon_puts

	and	T20,T22,SCSI_FAIL
	beqz	T20,1f
	LA	a0,scsifail_msg
	jal	pon_puts

1: 	and	T20,T22,RTC_FAIL
	beqz	T20,1f
	LA	a0,rtcfail_msg
	jal	pon_puts

1:	and	T20,T22,INT_FAIL
	beqz	T20,regs_done
	LA	a0,intfail_msg
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
	LA	a0,reg_msg
	jal	pon_puts
	LA	a0,reg_fail_exc_msg
	jal	pon_puts
	j	T30
	/* exception this early never returns */
	END(regs_hndlr)


	.data
scsifail_msg:
	.asciiz "\tWD SCSI0 path test \t           *FAILED*\r\n"
rtcfail_msg:
	.asciiz "\tRTC path test \t\t           *FAILED*\r\n"
intfail_msg:
	.asciiz "\tINT path test \t\t           *FAILED*\r\n"
reg_msg:
	.asciiz "Data path test\t\t\t    "
reg_fail_exc_msg:
	.asciiz "       *FAILED* with EXCEPTION:\r\n"
