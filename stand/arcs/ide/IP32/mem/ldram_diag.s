#ident	"IP32diags/mem/ldram_diag.s:  $Revision: 1.1 $"

#include "asm.h"
#include "regdef.h"
#include "ide_msg.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/z8530.h"
#include "sys/IP32.h"

#define	EIGHT_MBYTE	0x800000
/*
 * cannot be used to call routines that has the 'jal' instruction since 'jal'
 * uses absolute address
 */
#define	CALL(f)								\
	la	t0,f;							\
	addu	t0,s0;							\
	jal	t0;							\
	nop


	.set	noreorder

FRAMESIZE1=(2*4)+(11*4)			# local, ra, s0..7, fp
#ifdef MONTE
NESTED(low_dram_test, FRAMESIZE1, zero)
	subu	sp,FRAMESIZE1
	sw	zero,FRAMESIZE1-4(sp)		# error count
	sw	ra,FRAMESIZE1-12(sp)
	sw	s0,FRAMESIZE1-16(sp)
	sw	s1,FRAMESIZE1-20(sp)
	sw	s2,FRAMESIZE1-24(sp)
	sw	s3,FRAMESIZE1-28(sp)
	sw	s4,FRAMESIZE1-32(sp)
	sw	s5,FRAMESIZE1-36(sp)
	sw	s6,FRAMESIZE1-40(sp)
	sw	s7,FRAMESIZE1-44(sp)
	sw	fp,FRAMESIZE1-48(sp)
	sw	a1,FRAMESIZE1+4(sp)		# Reportlevel
	sw	a0,FRAMESIZE1+0(sp)		# pflag

	/* DO NOT MODIFY, USE THROUGH OUT THE ENTIRE FILE */
	li	s0,EIGHT_MBYTE

	/* move from 0-8MB to 8-16MB */
	li	a0,K1_RAMBASE
	addu	a1,a0,s0
	jal	bcopy
	move	a2,s0

	/* use bootstrap vector for exceptions */
	la	fp,trap_handler
	addu	fp,s0
	li	t0,SR_BEV | SR_BERRBIT | SR_IEC
	mtc0	t0,C0_SR

	/* execute IDE in high memory */
	addu	gp,s0
	la	t0,1f
	addu	t0,s0
	j	t0
	addu	sp,s0

1:
	/*
	 * set up high memory boundary, 8MB, and determine data patterns to use
	 * according to pflag
	 */
	addu	s7,s0,K1_RAMBASE		# high memory boundary

	lw	t0,FRAMESIZE1+0(sp)		# pflag
	move	s2,zero
	beq	t0,zero,2f
	not	s3,zero				# address/data bit test
	li	s3,0x01010101			# parity bit test

2:
	li	s1,0x10				# address bit mask

	/*
	 * load data pattern into memory. the data pattern in each location is
	 * determined by the current address mask, s1, and the address, s6.
	 * if (s1 & s6) == 0, use s2, otherwise s3
	 */
3:
	li	s6,K1_RAMBASE			# first location
	and	s4,s6,s1			# determine data pattern
4:
	beq	s4,zero,5f
	sw	s2,0(s6)
	sw	s3,0(s6)
5:
	/* blink health led occasionally to show the program is still alive */
	and	t0,s6,0x3fffff			# blink health led ?
	bne	t0,zero,6f
	addu	s6,4				# next location

        lw      a0,FRAMESIZE1-52(sp)
        CALL(switch_led_color)
        sw      v0,FRAMESIZE1-52(sp)



6:
	bne	s6,s7,4b			# all done ?
	and	s4,s6,s1			# determine data pattern

	/* verify */
	li	s6,K1_RAMBASE			# first location
	and	s4,s6,s1
7:
	lw	v0,0(s6)			# parity exception may occur
	beq	s4,zero,8f
	move	s5,s2
	move	s5,s3
8:
	beq	v0,s5,10f			# data match ?

	/* data mismatch, print out error message */
	lw	t0,FRAMESIZE1-4(sp)		# error count
	lw	t1,FRAMESIZE1+4(sp)		# Reportlevel

9:
	add	t0,1				# increment error count
	sw	t0,FRAMESIZE1-4(sp)
	blt	t1,ERR,10f			# print error message ?

	/* save read data */
	sw	v0,FRAMESIZE1-8(sp)

	/* print error message */
	la	a0,address_msg			# print 'Address: '
	CALL(_puts)
	move	a0,s6				# print address value
	CALL(_puthex)

	la	a0,expected_msg			# print ', Expected: '
	CALL(_puts)
	move	a0,s5				# print expected value
	CALL(_puthex)

	la	a0,actual_msg			# print ', Actual '
	CALL(_puts)
	lw	a0,FRAMESIZE1-8(sp)
	CALL(_puthex)

	la	a0,crlf_msg			# print newline
	CALL(_puts)
10:
	/* blink health led occasionally to show the program is still alive */
	and	t0,s6,0xfffff			# blink health led ?
	bne	t0,zero,11f
	addu	s6,4				# next location

        lw      a0,FRAMESIZE1-52(sp)
        CALL(switch_led_color)
        sw      v0,FRAMESIZE1-52(sp)
11:
	bne	s6,s7,7b			# all done ?
	and	s4,s6,s1

	sll	s1,1
	bne	s1,s0,3b			# through all address bit ?
	nop

	bne	s2,zero,12f			# if not done with data
	move	t0,s2				# patterns' complement,
	move	s2,s3				# complement and repeat
	b	2b
	move	s3,t0
12:
	/* move from 8-16MB to 0-8MB */
	li	a1,K1_RAMBASE
	addu	a0,a1,s0
	move	a2,s0
	CALL(bcopy)

	/* use normal vectors for exceptions */
	li	t0,SR_BERRBIT | SR_IEC
	mtc0	t0,C0_SR

	/* set up to execute IDE in low memory */
	subu	gp,s0
	la	t0,13f
	j	t0
	subu	sp,s0
13:
	lw	v0,FRAMESIZE1-4(sp)		# pass error count

	lw	ra,FRAMESIZE1-12(sp)
	lw	s0,FRAMESIZE1-16(sp)
	lw	s1,FRAMESIZE1-20(sp)
	lw	s2,FRAMESIZE1-24(sp)
	lw	s3,FRAMESIZE1-28(sp)
	lw	s4,FRAMESIZE1-32(sp)
	lw	s5,FRAMESIZE1-36(sp)
	lw	s6,FRAMESIZE1-40(sp)
	lw	s7,FRAMESIZE1-44(sp)
	lw	fp,FRAMESIZE1-48(sp)
	j	ra
	addu	sp,FRAMESIZE1
END(low_dram_test)
#endif /* MONTE */



LEAF(trap_handler)
	/* R4000 chip registers */
	la	a0,epc_msg		# EPC
	CALL(_puts)
	mfc0	a0,C0_EPC
	CALL(_puthex)

	la	a0,errepc_msg		# ErrorEPC
	CALL(_puts)
	mfc0	a0,C0_ERROR_EPC
	CALL(_puthex)

	la	a0,badvaddr_msg		# BadVaddr
	CALL(_puts)
	mfc0	a0,C0_BADVADDR
	CALL(_puthex)

	la	a0,cause_msg		# Cause
	CALL(_puts)
	mfc0	a0,C0_CAUSE
	CALL(_puthex)

	la	a0,status_msg		# Status
	CALL(_puts)
	mfc0	a0,C0_SR
	CALL(_puthex)

	la	a0,cacherr_msg		# CacheErr
	CALL(_puts)
	mfc0	a0,C0_CACHE_ERR
	CALL(_puthex)	

#ifndef IP32
	/* board registers */
	la	a0,cpu_parerr_msg	# CPU parity error register
	CALL(_puts)
	li	v0,PHYS_TO_K1(CPU_ERR_STAT)
	lw	a0,0(v0)
	CALL(_puthex)

	la	a0,gio_parerr_msg	# GIO parity error register
	CALL(_puts)
	li	v0,PHYS_TO_K1(GIO_ERR_STAT)
	lw	a0,0(v0)
	CALL(_puthex)

	# Get INT2/3 base
	.set	reorder
	li	s1,PHYS_TO_K1(HPC3_INT3_ADDR)   # assume IOC1/INT3
	IS_IOC1(v0)
	bnez	v0,1f				# branch if IOC1/INT3
	li	s1,PHYS_TO_K1(HPC3_INT2_ADDR)	# use INT2
	.set	noreorder
1:
#endif /* !IP32 */
	la	a0,liostat0_msg		# local I/O interrupt status 0
	CALL(_puts)
	lbu	a0, LIO_0_ISR_OFFSET(s1)
	CALL(_puthex)

	la	a0, liostat1_msg	# local I/O interrupt status 1
	CALL(_puts)
	lbu	v0, LIO_1_ISR_OFFSET(s1)
	CALL(_puthex)

#ifndef IP32
	la	a0,cpuaddr_msg		# CPU error address register
	CALL(_puts)
	li	v0,PHYS_TO_K1(CPU_ERR_ADDR)
	lw	a0,0(v0)
	CALL(_puthex)

	la	a0,gioaddr_msg		# GIO error address register
	CALL(_puts)
	li	v0,PHYS_TO_K1(GIO_ERR_ADDR)
	lw	a0,0(v0)
	CALL(_puthex)
#endif /* IP32 */

	la	a0,reset_msg		# reset to restart
	CALL(_puts)

	/*
	 * loop forever when we got an exception since there's no good/easy way
	 * for us to recover
	 */
1:	b	1b
END(trap_handler) 



/* print out a hex value. hex value in a0. derived from pon_puthex */
FRAMESIZE2=4+(2*4)				# ra, s1..2
NESTED(_puthex, FRAMESIZE2, zero)
	subu	sp,FRAMESIZE2
	sw	ra,FRAMESIZE2-4(sp)
	sw	s1,FRAMESIZE2-8(sp)
	sw	s2,FRAMESIZE2-12(sp)

	li	s1,8				# 8 digits in each hex value
	la	s2,pon_putc			# execute in high memory
	addu	s2,s0
1:
	srl	v1,a0,28			# get leftmost digit
	lbu	v1,hexdigit+EIGHT_MBYTE(v1) 	# convert it to hex
	jal	s2				# print one digit at a time
	sub	s1,1
	bne	s1,zero,1b			# entire number printed?
	sll	a0,4				# set up next hex digit

	lw	ra,FRAMESIZE2-4(sp)
	lw	s1,FRAMESIZE2-8(sp)
	lw	s2,FRAMESIZE2-12(sp)
	j	ra
	addu	sp,FRAMESIZE2
END(_puthex)



/*
 * print out a character string. address of the first character in a0. derived
 * from pon_puts
 */
FRAMESIZE3=4+(1*4)				# ra, s1
NESTED(_puts, FRAMESIZE3, zero)
	subu	sp,FRAMESIZE3
	sw	ra,FRAMESIZE3-4(sp)
	sw	s1,FRAMESIZE3-8(sp)

	addu	a0,s0				# execute in high memory
	la	s1,pon_putc
	addu	s1,s0

	b	2f
	nop
1:
	jal	s1				# print one character at a time
	addu	a0,1				# address of next character
2:
	lbu	v1,0(a0)			# get character
	nop
	bne	v1,zero,1b			# null character ?

	lw	ra,FRAMESIZE3-4(sp)
	lw	s1,FRAMESIZE3-8(sp)
	j	ra
	addu	sp,FRAMESIZE3
END(_puts)



#ifdef	DELAY
#undef	DELAY
#endif
#define	DELAY	nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;	\
		nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;

#ifndef IP32
/* switch the color of the health LED */

	.data
bits22:	.byte	LED_GREEN, 0
bits24:	.byte	LED_RED_OFF,LED_RED_OFF|LED_GREEN_OFF
	.text

LEAF(switch_led_color)

	.set	reorder

	/* get color table base address */

	la	t0, bits22
	lw      t1, HPC3_SYS_ID|K1BASE
	andi    t1, BOARD_IP22
	bnez	t1, 3f
	la	t0, bits24
3:
	addu	t0, s0

	/* get the color byte and set the leds */

	move	t1, a0
	andi	t1, 1
	add	t0, t1
	lb	t1, 0(t0)		/* t1 = color bits */

	la	t2, _hpc3_write1	/* merge in the other write1 bits */
	addu	t2, s0
	lw	t0, 0(t2)
	andi	t0, 0xcf
	or	t1, t0

        la      t0, PHYS_TO_K1(HPC3_WRITE1)
	sb      t1, 3(t0)

	xori	v0, a0, 1

	j	ra
#endif /* !IP32 */

	.set	noreorder


	.data
actual_msg:
	.asciiz	", Actual: "
address_msg:
	.asciiz	"\r\nAddress: "
crlf_msg:
	.asciiz	"\r\n"
expected_msg:
	.asciiz	", Expected: "
hexdigit:
	.ascii	"0123456789abcdef"
reset_msg:
	.asciiz "\r\n\r\n\r\n[Press reset to restart.]"


/*
 * The exception message looks like this:

EPC: 0xnnnnnnnn, ErrEPC: 0xnnnnnnnn, BadVaddr: 0xnnnnnnnn
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
cause_msg:
	.asciiz "\r\nCause: "
status_msg:
	.asciiz ", Status: "
cacherr_msg:
	.asciiz ", CacheErr: "
cpu_parerr_msg:
	.asciiz "\r\nCpuParityErr: "
gio_parerr_msg:
	.asciiz ", GioParityErr: "
liostat0_msg:
	.asciiz "\r\nLIOstatus0: "
liostat1_msg:
	.asciiz ", LIOstatus1: "
cpuaddr_msg:
	.asciiz "\r\nCpuErrorAddr: "
gioaddr_msg:
	.asciiz ", GioErrorAddr: "
