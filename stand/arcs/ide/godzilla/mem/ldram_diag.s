/*
 * ldram_diag.s - contains several ass'y level functions
 *
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or 
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 *
 * ldram_diag.s - contains a few ass'y level functions
 *
 *  The memsetup call parses the incoming arguments and assigns a default
 *  address if no range is specified Generic_l is the actual test code.
 *   modules:  	low_dram_test used by low_memtest
 *              low_kh_test (low_kh_test, called by khlow_drv in cpu cmds), 
 *		utilities (trap_handler, put_hex, _puts, macro DELAY,
 *			switch_led_color) and a description of the trap handler
 */
#ident	"ide/godzilla/mem/ldram_diag.s  $Revision: 1.13 $"

#include "asm.h"
#include "regdef.h"
#include "ide_msg.h"
#include "sys/cpu.h"
#include "sys/RACER/IP30addrs.h"
#include "sys/sbd.h"

#define LMEM_START	(PHYS_RAMBASE+0x0800000)/* 8MB */
#define LMEM_SIZE	0x00600000		/* 6MB */
#define LMEM_OFFSET	(IP30_SCRATCH_MEM-LMEM_START)
#define FLASH_MASK	0x1fffff

/*
 * cannot be used to call routines that have the 'jal' instruction since 'jal'
 * uses absolute address
 */
#define	CALL(f)								\
	LA	t0,f;							\
	PTR_ADD	t0,s0;							\
	jalr	t0;							\
	nop

	.set	noreorder

/* n64: local + reg save + arg save */
FRAMESIZE1=16*SZREG+2*SZREG
#define REPORT_OFFSET	FRAMESIZE1-14*SZREG
#define PFLAG_OFFSET	FRAMESIZE1-15*SZREG

NESTED(_low_dram_test, FRAMESIZE1, zero)
	PTR_SUB	sp,FRAMESIZE1
	PTR_S	zero,FRAMESIZE1-1*SZREG(sp)		# error count
	PTR_S	ra,FRAMESIZE1-3*SZREG(sp)
	PTR_S	s0,FRAMESIZE1-4*SZREG(sp)		# lmem
	PTR_S	s1,FRAMESIZE1-5*SZREG(sp)
	PTR_S	s2,FRAMESIZE1-6*SZREG(sp)
	PTR_S	s3,FRAMESIZE1-7*SZREG(sp)
	PTR_S	s4,FRAMESIZE1-8*SZREG(sp)
	PTR_S	s5,FRAMESIZE1-9*SZREG(sp)
	PTR_S	s6,FRAMESIZE1-10*SZREG(sp)
	PTR_S	s7,FRAMESIZE1-11*SZREG(sp)
	PTR_S	fp,FRAMESIZE1-12*SZREG(sp)
	PTR_S	zero,FRAMESIZE1-13*SZREG(sp)		# led state (fullhouse)
	PTR_S	a1,REPORT_OFFSET(sp)			# Reportlevel
	PTR_S	a0,PFLAG_OFFSET(sp)			# pflag

	/* DO NOT MODIFY, USE THROUGH OUT THE ENTIRE FILE */
	li	s0,LMEM_OFFSET

	/* move from LMEM_START to buffer area */
	LI	a0,PHYS_TO_K0(LMEM_START)
	PTR_ADD	a1,a0,s0
	li	a2,LMEM_SIZE
	jal	bcopy
	nop						# BDSLOT

	/* On IP30, with IDE cached, we need to flush the stack after we move
	 * it to avoid any writebacks during the diag, which can corrupt
	 * things.  Do not dirty low memory after this point.
	 */
	jal	flush_cache
	nop						# BDSLOT

	/*  Use bootstrap vector for exceptions.
	 */
	LA	fp,trap_handler
	PTR_ADD	fp,s0					# relocate it
	mfc0	t0,C0_SR
	or	t0,SR_BEV
	mtc0	t0,C0_SR

	/* Execute IDE in high memory */
	PTR_ADD	gp,s0
	LA	t0,1f
	PTR_ADD	t0,s0
	j	t0
	PTR_ADD	sp,s0					# BDSLOT

1:
	/*  Set up high memory boundary, LMEM_SIZE, and determine data
	 * patterns to use according to pflag.
	 */
	LI	s7,PHYS_TO_K1(LMEM_START+LMEM_SIZE)	# end of test

	PTR_L	t0,PFLAG_OFFSET(sp)		# pflag
	move	s2,zero
	beq	t0,zero,2f
	not	s3,zero				# address/data bit test
	li	s3,0x01010101			# parity bit test XXX ECC?

2:
	li	s1,0x10				# address bit mask

	/*
	 * load data pattern into memory. the data pattern in each location is
	 * determined by the current address mask, s1, and the address, s6.
	 * if (s1 & s6) == 0, use s2, otherwise s3
	 */
3:
	LI	s6,PHYS_TO_K1(LMEM_START)	# first location
	and	s4,s6,s1			# determine data pattern
4:
	beq	s4,zero,5f
	sw	s2,0(s6)
	sw	s3,0(s6)
5:
	/* blink health led occasionally to show the program is still alive */
	and	t0,s6,FLASH_MASK
	bne	t0,zero,6f
	PTR_ADD	s6,4				# BDSLOT: next location

        CALL(switch_led_color)

6:
	bne	s6,s7,4b			# all done ?
	and	s4,s6,s1			# BDSLOT:determine data pattern

	/* verify */
	LI	s6,PHYS_TO_K1(LMEM_START)	# first location
	and	s4,s6,s1
7:
	lw	v0,0(s6)			# parity exception may occur
	beq	s4,zero,8f
	move	s5,s2				# BDSLOT
	move	s5,s3
8:
	beq	v0,s5,10f			# data match ?

	/* data mismatch, print out error message */
	PTR_L	t0,FRAMESIZE1-1*SZREG(sp)	# error count
	PTR_L	t1,REPORT_OFFSET(sp)		# Reportlevel

9:
	PTR_ADD	t0,1				# increment error count
	PTR_S	t0,FRAMESIZE1-1*SZREG(sp)
	blt	t1,ERR,10f			# print error message ?

	/* save read data */
	sd	v0,FRAMESIZE1-2*SZREG(sp)

	/* print error message */
	LA	a0,address_msg			# print 'Address: '
	CALL(_puts)
	move	a0,s6				# print address value
	CALL(_puthex)

	LA	a0,expected_msg			# print ', Expected: '
	CALL(_puts)
	move	a0,s5				# print expected value
	CALL(_puthex)

	LA	a0,actual_msg			# print ', Actual '
	CALL(_puts)
	PTR_L	a0,FRAMESIZE1-2*SZREG(sp)
	CALL(_puthex)

	LA	a0,crlf_msg			# print newline
	CALL(_puts)
10:
	/* blink health led occasionally to show the program is still alive */
	and	t0,s6,FLASH_MASK		# blink health led ?
	bne	t0,zero,11f
	PTR_ADD	s6,4				# BDSLOT: next location

        CALL(switch_led_color)

11:
	bne	s6,s7,7b			# all done ?
	and	s4,s6,s1			# BDSLOT

	sll	s1,1
	bne	s1,s0,3b			# through all address bit ?
	nop					# BDSLOT

	bne	s2,zero,12f			# if not done with data
	move	t0,s2				# BDSLOT: patterns' complement,
	move	s2,s3				# complement and repeat
	b	2b
	move	s3,t0				# BDSLOT
12:
	/* move from store area back home.  use uncached access for
	 * destination to avoid race condition between the slave
	 * processors' instruction fetching stream and own data stream
	 */
	LI	a1,PHYS_TO_K0(LMEM_START)
	li	a0,LMEM_OFFSET
	PTR_ADD a0,a1,a0
	LI	a1,0xb800000000000000|LMEM_START
	li	a2,LMEM_SIZE
	CALL(bcopy)

	/* Restore the normal vectors for exceptions.
	 */
	mfc0	t0,C0_SR
	xor	t0,SR_BEV
	mtc0	t0,C0_SR

	/* set up to execute IDE back in low memory (it is cached) */
	PTR_SUB	gp,s0
	LA	t0,13f
	j	t0
	PTR_SUB	sp,s0					# BDSLOT
13:
	jal	flush_cache
	nop						# BDSLOT

	PTR_L	v0,FRAMESIZE1-1*SZREG(sp)		# pass error count

	PTR_L	ra,FRAMESIZE1-3*SZREG(sp)
	PTR_L	s0,FRAMESIZE1-4*SZREG(sp)
	PTR_L	s1,FRAMESIZE1-5*SZREG(sp)
	PTR_L	s2,FRAMESIZE1-6*SZREG(sp)
	PTR_L	s3,FRAMESIZE1-7*SZREG(sp)
	PTR_L	s4,FRAMESIZE1-8*SZREG(sp)
	PTR_L	s5,FRAMESIZE1-9*SZREG(sp)
	PTR_L	s6,FRAMESIZE1-10*SZREG(sp)
	PTR_L	s7,FRAMESIZE1-11*SZREG(sp)
	PTR_L	fp,FRAMESIZE1-12*SZREG(sp)
	j	ra
	PTR_ADD	sp,FRAMESIZE1
END(_low_dram_test)

/********************************************************/
/*  Begin kh low test					*/
/*							*/
/*  CALL, FRAMESIZE1, REPORT_OFFSET, PFLAG_OFFSET	*/
/*  are the same values as defined for low_dram_test	*/ 
/********************************************************/

#define	INTERVAL	0x8		/* bytes */
#define ALL_ONES        0xffffffffffffffff

NESTED(_low_kh_test, FRAMESIZE1, zero)
	PTR_SUB	sp,FRAMESIZE1
	PTR_S	zero,FRAMESIZE1-1*SZREG(sp)		# error count
	PTR_S	ra,FRAMESIZE1-3*SZREG(sp)
	PTR_S	s0,FRAMESIZE1-4*SZREG(sp)		# lmem
	PTR_S	s1,FRAMESIZE1-5*SZREG(sp)
	PTR_S	s2,FRAMESIZE1-6*SZREG(sp)
	PTR_S	s3,FRAMESIZE1-7*SZREG(sp)
	PTR_S	s4,FRAMESIZE1-8*SZREG(sp)
	PTR_S	s5,FRAMESIZE1-9*SZREG(sp)
	PTR_S	s6,FRAMESIZE1-10*SZREG(sp)
	PTR_S	s7,FRAMESIZE1-11*SZREG(sp)
	PTR_S	fp,FRAMESIZE1-12*SZREG(sp)
	PTR_S	a1,REPORT_OFFSET(sp)			# Reportlevel
	PTR_S	a0,PFLAG_OFFSET(sp)			# pflag (not used)

	/* DO NOT MODIFY, USE THROUGH OUT THE ENTIRE FILE */
	li	s0,LMEM_OFFSET

	/* move from LMEM_START to buffer area */
	LI	a0,PHYS_TO_K0(LMEM_START)
	PTR_ADD	a1,a0,s0
	li	a2,LMEM_SIZE
	jal	bcopy
	nop						# BDSLOT

	/* On IP30, with IDE cached, we need to flush the stack after we move
	 * it to avoid any writebacks during the diag, which can corrupt
	 * things.  Do not dirty low memory after this point.
	 */
	jal	flush_cache
	nop						# BDSLOT

	/*  Use bootstrap vector for exceptions.
	 */
	LA	fp,trap_handler
	PTR_ADD	fp,s0
	mfc0	t0,C0_SR
	or	t0,SR_BEV
	mtc0	t0,C0_SR

	/* execute IDE in high memory */
	PTR_ADD	gp,s0
	LA	t0,khstart 
	PTR_ADD	t0,s0
	j	t0
	PTR_ADD	sp,s0

khstart:
	LI	s6, PHYS_TO_K1(LMEM_START)
	move	s2, s6
	LI	s7, PHYS_TO_K1(LMEM_START+LMEM_SIZE)

	/* Start Knaizuk Hartmann test */

	/*
	 * Set partitions 1 and 2 to 0's	
	 */
	LI      s7, PHYS_TO_K1(LMEM_START+LMEM_SIZE)
	LI      s6, PHYS_TO_K1(LMEM_START)

	move	s3,zero			# set expected pattern
	PTR_ADDIU s2,s6,INTERVAL	# get first partition 1 address

1:
	PTR_S	s3,(s2)			# set partition 1 to zero
	PTR_ADDIU	s2,s2,INTERVAL	# increment to partition 2 address
	bgeu	s2,s7,2f		# reached the end yet?
	nop
	PTR_S	s3,(s2)			# set partition 2 to zero

	PTR_ADDIU s2,s2,INTERVAL*2	# increment to partition 1 address
	bltu	s2,s7,1b		# reached the end yet?
	nop

	/*
	 * Set partition 0 to 1's	
	 */
2:
	LI	s3, ALL_ONES	
	LI      s7, PHYS_TO_K1(LMEM_START+LMEM_SIZE)
        LI      s6, PHYS_TO_K1(LMEM_START)

	move	s2,s6			# get first partition 0 address
3:
	PTR_S	s3,(s2)			# set partition 0 to one's
	PTR_ADDIU s2,s2,INTERVAL*3	# next address
	bltu	s2,s7,3b		# reached the end yet?
	nop				# BDSLOT

	/*
	 * Set partition 1 to 1's		
	 */
	LI	s3, ALL_ONES		
	LI      s7, PHYS_TO_K1(LMEM_START+LMEM_SIZE)
        LI      s6, PHYS_TO_K1(LMEM_START)

	PTR_ADDIU s2,s6,INTERVAL	# get first partition 1 address
5:
	PTR_S	s3,(s2)			# set partition 1 to one's
	PTR_ADDIU s2,s2,INTERVAL*3	# next address
	bltu	s2,s7,5b		# reached the end yet?
	nop				# BDSLOT
 
	/*
	 * Verify partition 2 is still 0's
	 */
	move	s3,zero			# set expected pattern
        LI      s6,PHYS_TO_K1(LMEM_START)
	LI      s7,PHYS_TO_K1(LMEM_START+LMEM_SIZE)

	PTR_ADDIU s2,s6,INTERVAL*2	# get first partition 2 address
6:
	PTR_L	s4,(s2)			# read data
	bne	s4,s3,error		# does it match?
	nop
	PTR_ADDIU s2,s2,INTERVAL*3	# bump to next location
	bltu	s2,s7,6b		# reached the end yet?
	nop				# BDSLOTop

	/*
	 * Verify partition 0 is still 1's	
	 */
	LI	s3,ALL_ONES
        LI      s6,PHYS_TO_K1(LMEM_START)
	LI      s7,PHYS_TO_K1(LMEM_START+LMEM_SIZE)
	move	s2,s6			# get first partition 0 address
7:
	PTR_L	s4,(s2)			# read data
	bne	s4,s3,error		# does it match?
	nop
	PTR_ADDIU s2,s2,INTERVAL*3	# bump to next location
	bltu	s2,s7,7b		# reached the end yet?
	nop				# BDSLOT
 
	/*
	 * Verify partition 1 is still 1's
	 */
	LI	s3,ALL_ONES		
        LI      s6, PHYS_TO_K1(LMEM_START)
	LI      s7, PHYS_TO_K1(LMEM_START+LMEM_SIZE)

	PTR_ADDIU s2,s6,INTERVAL	# get first partition 1 address
8:
	PTR_L	s4,(s2)			# read data
	bne	s4,s3,error		# does it match?
	nop				# BDSLOT
	PTR_ADDIU s2,s2,INTERVAL*3	# bump to next location
	bltu	s2,s7,8b		# reached the end yet?
	nop				# BDSLOT

	/*
	 * Set partition 0 to 0's		
	 */
	move	s3,zero			# set expected pattern
        LI      s6,PHYS_TO_K1(LMEM_START)
	LI      s7,PHYS_TO_K1(LMEM_START+LMEM_SIZE)

	move	s2,s6			# get first partition 0 address
9:
	PTR_S	s3,(s2)			# set partition 0 to zero's
	PTR_ADDIU s2,s2,INTERVAL*3	# next address
	bltu	s2,s7,9b		# reached the end yet?
	nop				# BDSLOT
 
	/*
	 * Verify partition 0 is still 0's
	 */
	move	s3,zero			# set expected pattern
        LI      s6,PHYS_TO_K1(LMEM_START)
	LI      s7,PHYS_TO_K1(LMEM_START+LMEM_SIZE)

	move	s2,s6			# get first partition 0 address
10:
	PTR_L	s4,0(s2)		# read data
	bne	s4,s3,error		# does it match?
	nop
	PTR_ADDIU s2,s2,INTERVAL*3	# bump to next location
	bltu	s2,s7,10b		# reached the end yet?
	nop				# BDSLOT

	/*
	 * Set partition 2 to 1's		
	 */
	LI	s3,ALL_ONES		
        LI      s6,PHYS_TO_K1(LMEM_START)
	LI      s7,PHYS_TO_K1(LMEM_START+LMEM_SIZE)

	PTR_ADDIU s2,s6,INTERVAL*2	# get first partition 2 address
11:
	PTR_S	s3,(s2)			# set partition 2 to one's
	PTR_ADDIU s2,s2,INTERVAL*3	# next address
	bltu	s2,s7,11b		# reached the end yet?
	nop				# BDSLOT

	/*
	 * Verify partition 2 is still 1's
	 */
	LI	s3,ALL_ONES		
        LI      s6,PHYS_TO_K1(LMEM_START)

	PTR_ADDIU s2,s6,INTERVAL*2	# get first partition 2 address
12:
	PTR_L	s4,(s2)			# read data
	bne	s4,s3,error		# does it match?
	nop				# BDSLOT

	PTR_ADDIU s2,s2,INTERVAL*3	# bump to next location
	bltu	s2,s7,12b		# reached the end yet?
	nop				# BDSLOT
	move	v0,zero			# indicate success
	b	14f			# and return
	nop				# BDSLOT

error:
	li	v0,1
	/* data mismatch, print out error message */
	PTR_L	t0,FRAMESIZE1-1*SZREG(sp)	# error count
	PTR_L	t1,REPORT_OFFSET(sp)		# Reportlevel

9:
	PTR_ADD	t0,1				# increment error count
	PTR_S	t0,FRAMESIZE1-1*SZREG(sp)

	/* save read data */
	PTR_S	s4,FRAMESIZE1-2*SZREG(sp)

	/* print error message */
	LA	a0,address_msg			# print 'Address: '
	CALL(_puts)

	move	a0,s2				# print address value
	CALL(_puthex)

	LA	a0,expected_msg			# print ', Expected: '
	CALL(_puts)
	move	a0,s3				# print expected value
	CALL(_puthex)

	LA	a0,actual_msg			# print ', Actual '
	CALL(_puts)
	PTR_L	a0,FRAMESIZE1-2*SZREG(sp)
	CALL(_puthex)

	LA	a0,crlf_msg			# print newline
	CALL(_puts)

        LA      a0,crlf_msg                     # print newline
        CALL(_puts)

14:
	/* move from store area back home.  use uncached access for
	 * destination to avoid race condition between the slave
	 * processors' instruction fetching stream and own data stream
	 */
	LI	a1,PHYS_TO_K0(LMEM_START)
	li	a0,LMEM_OFFSET
	PTR_ADD a0,a1,a0
	LI	a1,0xb800000000000000|LMEM_START
	li	a2,LMEM_SIZE
	CALL(bcopy)

	/* Restore the normal vectors for exceptions.
	 */
	mfc0	t0,C0_SR
	xor	t0,SR_BEV
	mtc0	t0,C0_SR

	/* set up to execute IDE in low memory */
	PTR_SUB	gp,s0
	LA	t0,13f
	j	t0
	PTR_SUB	sp,s0
13:
	jal	flush_cache
	nop						# BDSLOT

	PTR_L	v0,FRAMESIZE1-1*SZREG(sp)		# pass error count
	PTR_L	ra,FRAMESIZE1-3*SZREG(sp)
	PTR_L	s0,FRAMESIZE1-4*SZREG(sp)
	PTR_L	s1,FRAMESIZE1-5*SZREG(sp)
	PTR_L	s2,FRAMESIZE1-6*SZREG(sp)
	PTR_L	s3,FRAMESIZE1-7*SZREG(sp)
	PTR_L	s4,FRAMESIZE1-8*SZREG(sp)
	PTR_L	s5,FRAMESIZE1-9*SZREG(sp)
	PTR_L	s6,FRAMESIZE1-10*SZREG(sp)
	PTR_L	s7,FRAMESIZE1-11*SZREG(sp)
	PTR_L	fp,FRAMESIZE1-12*SZREG(sp)
	PTR_ADD	sp,FRAMESIZE1
	j	ra
	nop
END(_low_kh_test)

/* trap_handler output:
EPC: nnnnnnnnnnnnnnnn ErrorEPC: nnnnnnnnnnnnnnnn BadVaddr: nnnnnnnnnnnnnnnn
CpuBusErr: nnnnnnnnnnnnnnnn
MemBusErr: nnnnnnnnnnnnnnnn BadMemData: nnnnnnnnnnnnnnnn
HeartISR: nnnnnnnnnnnnnnnn HeartIMSR: nnnnnnnnnnnnnnnn


[Press reset to restart.]
 */

LEAF(trap_handler)
 	/*
	 * CPU chip registers 
	 */
	LA	a0,epc_msg		# EPC
	CALL(_puts)
	mfc0	a0,C0_EPC
	CALL(_puthex)

	LA	a0,errepc_msg		# ErrorEPC
	CALL(_puts)
	mfc0	a0,C0_ERROR_EPC
	CALL(_puthex)

	LA	a0,badvaddr_msg		# BadVaddr
	CALL(_puts)
	mfc0	a0,C0_BADVADDR
	CALL(_puthex)

	LA	a0,cause_msg		# Cause
	CALL(_puts)
	mfc0	a0,C0_CAUSE
	CALL(_puthex)

	LA	a0,status_msg		# Status
	CALL(_puts)
	mfc0	a0,C0_SR
	CALL(_puthex)

	LA	a0,cacherr_msg		# CacheErr
	CALL(_puts)
	mfc0	a0,C0_CACHE_ERR
	CALL(_puthex)	

 	/*
	 * board registers 
	 */
	LA	a0,cpu_buserr_msg	# heart bus error register
	CALL(_puts)
	ld	a0,PHYS_TO_COMPATK1(HEART_BERR_MISC)
	CALL(_puthex)

	LA	a0,mem_err_msg		# memory bus error register
	CALL(_puts)
	ld	a0,PHYS_TO_COMPATK1(HEART_MEMERR_ADDR)
	CALL(_puthex)

	/* these are the interrupt registers on the system ASICs */
	LA	a0,heart_int_msg	# heart interrupt status register
	CALL(_puts)
	ld      a0,PHYS_TO_COMPATK1(HEART_ISR)
	CALL(_puthex)
	LA	a0,heart_mint_msg	# heart masked interrupt status register
	CALL(_puts)
	ld      a0,PHYS_TO_COMPATK1(HEART_IMSR)
	CALL(_puthex)

	LA	a0,reset_msg		# reset to restart
	CALL(_puts)

	/* Loop forever in power switch code as we got an exception and there
	 * is no good way to recover since there's no good/easy way.
	 */
	CALL(pon_flash_led)
	/* NOTREACHED */
END(trap_handler) 

/* print out a hex value. hex value in a0. derived from pon_puthex */
FRAMESIZE2=(1*SZREG+(2*SZREG)+1*SZREG+1*SZREG+15)&~15 # ra, s1..2, mips2 pad
NESTED(_puthex, FRAMESIZE2, zero)
	PTR_SUB	sp,FRAMESIZE2
	PTR_S	ra,FRAMESIZE2-1*SZREG(sp)
	PTR_S	s1,FRAMESIZE2-2*SZREG(sp)
	PTR_S	s2,FRAMESIZE2-3*SZREG(sp)
	PTR_S	v0,FRAMESIZE2-4*SZREG(sp)


	li	s1,16				# Number of digits to display
	LA	s2,pon_putc			# execute in high memory
	PTR_ADD	s2,s0

1:
	dsrl	v1,a0,32			# Isolate digit 
	dsrl	v1,v1,28			# Isolate digit 
	and	v1,0xf
	lbu	v1,hexdigit+LMEM_OFFSET(v1)

	jal	s2				# print one digit at a time
	PTR_SUB	s1,1
	PTR_SLL	a0,4				# Set up next nibble
	bne	s1,zero,1b			# entire number printed?

	PTR_L	ra,FRAMESIZE2-1*SZREG(sp)
	PTR_L	s1,FRAMESIZE2-2*SZREG(sp)
	PTR_L	s2,FRAMESIZE2-3*SZREG(sp)
	PTR_L	v0,FRAMESIZE2-4*SZREG(sp)
	j	ra
	PTR_ADD	sp,FRAMESIZE2
END(_puthex)

/*
 * print out a character string. address of the first character in a0. derived
 * from pon_puts
 */
FRAMESIZE3=((1*SZREG)+SZREG+2*SZREG+15)&~15		# ra, s1, mips pad
NESTED(_puts, FRAMESIZE3, zero)
	PTR_SUB	sp,FRAMESIZE3
	PTR_S	ra,FRAMESIZE3-1*SZREG(sp)
	PTR_S	s1,FRAMESIZE3-2*SZREG(sp)

	PTR_ADD	a0,s0				# execute in high memory
	LA	s1,pon_putc
	PTR_ADD	s1,s0

	b	2f
	nop
1:
	jal	s1				# print one character at a time
	PTR_ADD	a0,1				# address of next character
2:
	lbu	v1,0(a0)			# get character
	bne	v1,zero,1b			# null character ?

	PTR_L	ra,FRAMESIZE3-1*SZREG(sp)
	PTR_L	s1,FRAMESIZE3-2*SZREG(sp)
	j	ra
	PTR_ADD	sp,FRAMESIZE3
END(_puts)

/* switch the color of the health LED */
LEAF(switch_led_color)
	LI	a0,IOC3_PCI_DEVIO_K1PTR		# IOC3 base
	sw	zero,IOC3_GPPR(1)(a0)		# amber off
	lw	a1,IOC3_GPDR(a0)		# get LED state
	and	a1,1
	bnez	a1,1f				# bit 0 -> LED is green
	li	a2,1				# BDSLOT
	j	ra
	sw	a2,IOC3_GPPR(0)(a0)		# BDSLOT - green on
1:	j	ra
	sw	zero,IOC3_GPPR(0)(a0)		# BDSLOT - green off
	END(switch_led_color)

/* slave processors wait here for the master processor to finish the low
 * memory test
 * NO MEMORY ACCESS MUST BE MADE IN THIS ROUTINE
 */
LEAF(sync_cpu)
	LI	a0,PHYS_TO_K1(HEART_MODE)
	/* LI	a1,HM_GP_FLAG(0) */
	LI	a1,1<<36	# assembler doesn't generate HM_GP_FLAG(0)
				# correctley

	/* this loop should be in icache when master processor is running
	 * the low memory test
	 */
1:
	ld	a2,0(a0)
	and	a2,a1
	beq	a2,zero,1b
	nop

	j	ra
	nop
	END(sync_cpu)

	.data
actual_msg:
	.asciiz	" Actual: "
address_msg:
	.asciiz	"\r\nAddress: "
crlf_msg:
	.asciiz	"\r\n"
expected_msg:
	.asciiz	" Expected: "
hexdigit:
	.ascii	"0123456789abcdef"

reset_msg:
	.asciiz "\r\n\r\n\r\n[Press reset to restart.]"
epc_msg:
	.asciiz "\r\nEPC: "
cause_msg:
	.asciiz "\r\nCause: "
status_msg:
	.asciiz " Status: "

cpu_buserr_msg:
	.asciiz "\r\nCpuBusErr: "
badvaddr_msg:
	.asciiz " BadVaddr: "
mem_err_msg:
	.asciiz "\r\nMemBusErr: "
bad_memdata_msg:
	.asciiz " BadMemData: "
heart_int_msg:
	.asciiz "\r\nHeartISR: "
heart_mint_msg:
	.asciiz " HeartIMSR: "

cpuaddr_msg:
	.asciiz "\r\nCpuErrorAddr: "
cacherr_msg:
	.asciiz " CacheErr: "
errepc_msg:
	.asciiz " ErrEPC: "
