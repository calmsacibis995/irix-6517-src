
/* 
 * pon_regs.s - short test of data paths to onboard devices, assuming
 *		that the CPU-MC-HPC3-DUART path is functional.
 */

#include "sys/asm.h"
#include "early_regdef.h"
#include "sys/cpu.h"
#include "sys/sbd.h"

#define BYTE0		0xa5
#define BYTE1		0x5a

#define WD_SCSI_ADDR0	SCSI0A_ADDR	/* do scsi 0 only; test of */
#define WD_SCSI_ADDR1	SCSI0D_ADDR	/* second scsi in scsi_diag.c code */
#ifdef	_MIPSEB
#define SEEQ_ENET_RX_ADDR	(HPC3_ETHER_REG+0x3)
#define SEEQ_ENET_TX_ADDR	(HPC3_ETHER_REG+0x7)
#define RTC_NVRAM_ADDR		((RT_CLOCK_ADDR+(0x3e*4))|0x3)
#else	/* _MIPSEL */
#define SEEQ_ENET_RX_ADDR	HPC3_ETHER_REG
#define SEEQ_ENET_TX_ADDR	(HPC3_ETHER_REG+4)
#define RTC_NVRAM_ADDR		(RT_CLOCK_ADDR+(0x3e*4))
#endif	/* _MIPSEL */

#define SCSI_FAIL	1
#define ENET_FAIL	2
#define RTC_FAIL	4
#define INT_FAIL	8

LEAF(pon_regs)
	move	RA2,ra	
	move	T22,zero
	.set noreorder
	dmfc0	T30,TETON_BEV			# save pon handler
	ssnop
	LA	T20,regs_hndlr			# set new exception hndlr
	dmtc0	T20,TETON_BEV
	ssnop
	.set reorder

	/* test scsi path - write and readback value of
	 * timeout register
	 */

	/* first clear any SCSI interrupts by reading the 
	 * status register 
	 */
	LI	T21,PHYS_TO_K1(WD_SCSI_ADDR0)
	LI	T23,PHYS_TO_K1(WD_SCSI_ADDR1)
	li	T20,0x17			# SCSI reg 0x17 - status
	sb	T20,0(T21)
	lb	zero,0(T23)

	li	T20,2				# select timeout reg
	sb	T20,0(T21)
	li	T20,BYTE0			# write value
	sb	T20,0(T23)

	li	T20,2				# select timeout reg
	sb	T20,0(T21)
	lbu	T20,0(T23)			# read timeout reg

	beq	T20,BYTE0,1f
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

	/* test RTC path - write and readback values for one ram register
	 */
	LI	T20,PHYS_TO_K1(RTC_NVRAM_ADDR)	# write rtc nvram
	lbu	T23,4(T20)			# save byte 0 of nvram
	li	T21,BYTE0			# write new data to nvram
	sb	T21,4(T20)
	lbu	T21,4(T20)
	bne	T21,BYTE0,1f
	b	2f

1:	ori	T22,RTC_FAIL

2: 	sb	T23,0(T20)			# restore rtc data

	/* test INT2 path - lio mask 0 and 2
	 * write & readback values
	 */
	LI	T20,PHYS_TO_K1(HPC3_INT2_ADDR)		# use INT2
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
	dmtc0	T30,TETON_BEV
	ssnop
	ssnop
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
	.asciiz "\tINT2 path test\t\t           *FAILED*\r\n"
reg_msg:
	.asciiz "Data path test\t\t\t    "
reg_fail_exc_msg:
	.asciiz "       *FAILED* with EXCEPTION:\r\n"
