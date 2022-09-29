#ident	"$Revsion$"

#include "asm.h"
#include "regdef.h"
#include "ide_msg.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/z8530.h"
#include "sys/IP32.h"

#define	EIGHT_MBYTE	0x800000
#define HI_MEM_BASE	0x40000000
#define FREE_MEM_START	0x01400000
/*
 */
#define	CALL(f)								\
	la	t0,f;							\
	jal	t0;							\
	nop


	.set	noreorder

FRAMESIZE1=(2*4)+(12*4)+(3*8)			# local, ra, s0..7, fp
NESTED(high_dram_test, FRAMESIZE1, zero)
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
	li	s0,(HI_MEM_BASE + FREE_MEM_START)

	/* set kx bit for 64 bit addressing */
	mfc0	t0,C0_SR
	nop
	move	s4,t0
	or	t0,SR_KX
	mtc0	t0,C0_SR

1:
	/*
	 * set up high memory base and limit, and determine data patterns to use
	 * according to pflag
	 */

	CALL(run_cached)

	LI	s6,0x9000000000000000		# xkphys uncached
	or	s6,s0				# HI_MEM_BASE
	LI	s7,0x9000000040000000		# xkphys uncached
	lw	s2,memsize
	subu	s2,8
	daddu	s7,s2				# high memory boundary

	lw	t0,FRAMESIZE1+0(sp)		# pflag
	srl	t0,1				# looking for 2 or more, get rid of 1
	move	s2,zero
	LI	s3,0xaaaaaaaaaaaaaaaa
	beq	t0,zero,2f
	nop
	LI	s3,0xffffffffffffffff		# 

2:

	/*
	 * load data pattern into memory. 
	 */
3:
4:
	sd	s3,0(s6)
5:
	/* blink health led occasionally to show the program is still alive */
	and	t0,s6,0x3fffff			# blink health led ?
	bne	t0,zero,6f
	daddi	s6,8				# next location

	sd	s6,FRAMESIZE1-56(sp)
	sd	s7,FRAMESIZE1-64(sp)
	sd	s3,FRAMESIZE1-72(sp)
        li      a0,1
        CALL(busy)
	CALL(run_cached)
	ld	s6,FRAMESIZE1-56(sp)
	ld	s7,FRAMESIZE1-64(sp)
	ld	s3,FRAMESIZE1-72(sp)



6:
	bne	s6,s7,4b			# all done ?
	nop
	/* verify */
	LI	s6,0x9000000000000000		# xkphys uncached
	or	s6,s0				# HI_MEM_BASE
7:
	ld	v0,0(s6)			# ecc exception may occur?
	xor	v1,v0,s3
	sltu	a0,zero,v1
	beq	a0,zero,10f			# data match ?

	/* data mismatch, print out error message */
	lw	t0,FRAMESIZE1-4(sp)		# error count
	lw	t1,FRAMESIZE1+4(sp)		# Reportlevel
	li	t1,3				# Reportlevel

	add	t0,1				# increment error count
	sw	t0,FRAMESIZE1-4(sp)
	blt	t1,ERR,10f			# print error message ?

	/* save read data */
	move	s5,v0

	/* print error message */
	la	a0,address_msg			# print 'Address: '
	CALL(_puts)
	move	a0,s6				# print address value
	CALL(_puthex)

	la	a0,expected_msg			# print ', Expected: '
	CALL(_puts)
	move	a0,s3				# print expected value
	CALL(_puthex)

	la	a0,actual_msg			# print ', Actual '
	CALL(_puts)
	move	a0,s5
	CALL(_puthex)

	la	a0,crlf_msg			# print newline
	CALL(_puts)
10:
	/* blink health led occasionally to show the program is still alive */
	not	v0				# complement and store data
	sd	v0,0(s6)
	and	t0,s6,0xfffff			# blink health led ?
	bne	t0,zero,11f
	daddi	s6,8				# next location

	sd	s6,FRAMESIZE1-56(sp)
	sd	s7,FRAMESIZE1-64(sp)
	sd	s3,FRAMESIZE1-72(sp)
        li      a0,1
        CALL(busy)
	CALL(run_cached)
	ld	s6,FRAMESIZE1-56(sp)
	ld	s7,FRAMESIZE1-64(sp)
	ld	s3,FRAMESIZE1-72(sp)
11:
	bne	s6,s7,7b			# all done ?
	nop
	/* verify */
	LI	s6,0x9000000000000000		# xkphys uncached
	or	s6,s0				# HI_MEM_BASE
	not	s3				# check complement pattern
17:
	ld	v0,0(s6)			# ecc exception may occur?
	xor	v1,v0,s3
	sltu	a0,zero,v1
	beq	a0,zero,18f			# data match ?

	/* data mismatch, print out error message */
	lw	t0,FRAMESIZE1-4(sp)		# error count
	lw	t1,FRAMESIZE1+4(sp)		# Reportlevel
	li	t1,3

	add	t0,1				# increment error count
	sw	t0,FRAMESIZE1-4(sp)
	blt	t1,ERR,18f			# print error message ?

	/* save read data */
	move	s5,v0

	/* print error message */
	la	a0,address_msg			# print 'Address: '
	CALL(_puts)
	move	a0,s6				# print address value
	CALL(_puthex)

	la	a0,expected_msg			# print ', Expected: '
	CALL(_puts)
	move	a0,s3				# print expected value
	CALL(_puthex)

	la	a0,actual_msg			# print ', Actual '
	CALL(_puts)
	move	a0,s5
	CALL(_puthex)

	la	a0,crlf_msg			# print newline
	CALL(_puts)
18:
	/* blink health led occasionally to show the program is still alive */
	not	v0				# complement and store data
	sd	v0,0(s6)
	and	t0,s6,0xfffff			# blink health led ?
	bne	t0,zero,19f
	daddi	s6,8				# next location

	sd	s6,FRAMESIZE1-56(sp)
	sd	s7,FRAMESIZE1-64(sp)
	sd	s3,FRAMESIZE1-72(sp)
        li      a0,1
        CALL(busy)
	CALL(run_cached)
	ld	s6,FRAMESIZE1-56(sp)
	ld	s7,FRAMESIZE1-64(sp)
	ld	s3,FRAMESIZE1-72(sp)
19:
	bne	s6,s7,17b			# all done ?
	nop


20:

	mtc0	s4,C0_SR

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
END(high_dram_test)


/* print out a hex value. hex value in a0. derived from pon_puthex */
FRAMESIZE2=4+(2*4)+4+16				# ra, s1..2
NESTED(_puthex, FRAMESIZE2, zero)
	subu	sp,FRAMESIZE2
	sw	ra,FRAMESIZE2-4(sp)
	sw	s1,FRAMESIZE2-8(sp)
	sw	s2,FRAMESIZE2-12(sp)

	li	s1,16				# 16 digits in each hex value
	move	s2,a0
1:
	dsrl32	a0,s2,28			# get leftmost digit
	lbu	a0,hexdigit(a0)		 	# convert it to hex
	sd	s2,FRAMESIZE2-16(sp)
	sd	s1,FRAMESIZE2-24(sp)
	CALL(putchar)
	ld	s2,FRAMESIZE2-16(sp)
	ld	s1,FRAMESIZE2-24(sp)
	sub	s1,1
	bne	s1,zero,1b			# entire number printed?
	dsll	s2,4				# set up next hex digit

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
FRAMESIZE3=4+(3*4)				# ra, s1
NESTED(_puts, FRAMESIZE3, zero)
	subu	sp,FRAMESIZE3
	sw	ra,FRAMESIZE3-4(sp)
	sw	s1,FRAMESIZE3-8(sp)
	sw	a0,FRAMESIZE3-12(sp)
	lw	a1,FRAMESIZE3-12(sp)

	la	s1,putchar

	b	2f
	nop
1:
	jal	s1				# print one character at a time
	nop
	lw	a1,FRAMESIZE3-12(sp)
	addu	a1,1				# address of next character
	sw	a1,FRAMESIZE3-12(sp)
2:
	lbu	a0,0(a1)			# get character
	nop
	bne	a0,zero,1b			# null character ?

	lw	ra,FRAMESIZE3-4(sp)
	lw	s1,FRAMESIZE3-8(sp)
	j	ra
	addu	sp,FRAMESIZE3
END(_puts)



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

