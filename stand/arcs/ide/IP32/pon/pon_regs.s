#ident "ide/IP30/pon/pon_regs.s:  $Revision: 1.1 $"
/* 
 * pon_regs.s - short test of data paths to onboard devices, assuming
 *		that the CPU-HEART-XBOW-BRIDGE-IOC3 path is functional.
 * 		NOTE: the code starting version is that of IP26's
 */
/* to do:
 *       - review the SCSI addresses
 *       - review the ENET addresses and real time clock code
 *       - review the entirely new IOC# code 
 */

#include "sys/asm.h"
#include "early_regdef.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/PCI/ioc3.h" 	

#define SCSI0A_ADDR 	0x00000000 	/* XXX */
#define SCSI0D_ADDR 	0x00000000 	/* XXX */
#define HPC3_ETHER_REG	0x00000000 	/* NOTE: the HPC3 notation is obsolete 
					   XXX */
#define RT_CLOCK_ADDR	0x00000000 	/* XXX */

#define BYTE0		0xa5
#define BYTE1		0x5a
#define WORD0		0x5aa55aa5 & 0xFFF00000  /* bits 20-31 */

#define WD_SCSI_ADDR0	SCSI0A_ADDR	/* do scsi 0 only; test of */
#define WD_SCSI_ADDR1	SCSI0D_ADDR	/* second scsi in scsi_diag.c code */
#ifdef	_MIPSEB
#define SEEQ_ENET_RX_ADDR	(HPC3_ETHER_REG+0x3)	/* XXX */
#define SEEQ_ENET_TX_ADDR	(HPC3_ETHER_REG+0x7)	/* XXX */
#define RTC_NVRAM_ADDR		((RT_CLOCK_ADDR+(0x3e*4))|0x3)	/* XXX */
#else	/* _MIPSEL */
#define SEEQ_ENET_RX_ADDR	HPC3_ETHER_REG		/* XXX */
#define SEEQ_ENET_TX_ADDR	(HPC3_ETHER_REG+4)	/* XXX */
#define RTC_NVRAM_ADDR		(RT_CLOCK_ADDR+(0x3e*4))	/* XXX */
#endif	/* _MIPSEL */

#define SCSI_FAIL	1
#define ENET_FAIL	2
#define RTC_FAIL	4
#define IOC3_FAIL	8

LEAF(pon_regs)
	move	RA2,ra	
	move	T22,zero
	.set noreorder
	/* NOTE: this section is similar to that of IP22's */
	/* NOTE: T30 is used because all T20,T21,T22,T23 are already used */
	move	T30,fp				# set new exception handler
	LA	fp,regs_hndlr	
	.set reorder

	/* test scsi path - write and readback value of
	 * timeout register
	 */

	/* first clear any SCSI interrupts by reading the 
	 * status register 
	 */
#if NOTDEF	/* XXX uncomment when the SCSI addresses are defined */
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
#endif
	/* test enet path - read value of tx status
	 * write value of rx cmd
	 * if data path is bad, maybe a bus error will get
	 * generated
	 */
#if NOTDEF      /* XXX  uncomment when "HPC3_ETHER_REG"  is defined */
	LI	T20,PHYS_TO_K1(SEEQ_ENET_TX_ADDR)	# read tx status reg
	lbu	T21,0(T20)
	LI	T20,PHYS_TO_K1(SEEQ_ENET_RX_ADDR)	# write rx control reg
	li	T21,BYTE1
	sb	T21,0(T20)
#endif

	/* test RTC path - write and readback values for one ram register
	 */
#if NOTDEF	/* XXX  uncomment when RTC_CLOCK_ADDR is defined */
	LI	T20,PHYS_TO_K1(RTC_NVRAM_ADDR)	# write rtc nvram
	lbu	T23,4(T20)			# save byte 0 of nvram
	li	T21,BYTE0			# write new data to nvram
	sb	T21,4(T20)
	lbu	T21,4(T20)
	bne	T21,BYTE0,1f
	b	2f

1:	ori	T22,RTC_FAIL

2: 	sb	T23,0(T20)			# restore rtc data
#endif

	/* test IOC3 configuration register: 
         * from all registers, none is entirely R/W for all bits
   	 * The one with the most number of RW bits is the IOC3 base addr
         * register, with bit 20 through 31 writable/readable 
  	 */
	LI	T20,PHYS_TO_K1(IOC3_PCI_ADDR)	# write IOC3 base address
	li	T21,WORD0			# write new data to nvram
	sw	T21,0(T20)
	lw	T21,0(T20)
	bne	T21,WORD0,set_IOC3_fail_bit
	b	2f

set_IOC3_fail_bit:	ori	T22,IOC3_FAIL

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

1:	and	T20,T22,IOC3_FAIL   	# was  INT_FAIL 
	beqz	T20,regs_done
	LA	a0,ioc3fail_msg 	# was intfail_msg
	jal	pon_puts

1:	b	1b				# don't continue

regs_done:
	.set noreorder				# restore old exception hndlr
	move	fp,T30				# similar to IP22's 
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
ioc3fail_msg:
	.asciiz "\tIOC3 path test\t\t           *FAILED*\r\n"
reg_msg:
	.asciiz "Data path test\t\t\t    "
reg_fail_exc_msg:
	.asciiz "       *FAILED* with EXCEPTION:\r\n"
