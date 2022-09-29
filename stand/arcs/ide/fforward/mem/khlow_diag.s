#ident	"$Id"

#include "asm.h"
#include "regdef.h"
#include "ide_msg.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/z8530.h"

#define	EIGHT_MBYTE	0x800000

	.globl	address_msg
	.globl	expected_msg
	.globl	actual_msg
	.globl	crlf_msg


/*
 * cannot be used to call routines that has the 'jal' instruction since 'jal'
 * uses absolute address
 */
#define	CALL(f)								\
	LA	t0,f;							\
	PTR_ADDU t0,s0;							\
	jal	t0;							\


	.set	noreorder

FRAMESIZE1=(2*SZREG)+(12*SZREG)			# local, ra, s0..7, fp
NESTED(kh_low, FRAMESIZE1, zero)
	PTR_ADDU sp,-FRAMESIZE1
	REG_S	zero,FRAMESIZE1-SZREG(sp)	# error count
	REG_S	ra,FRAMESIZE1-3*SZREG(sp)
	REG_S	s0,FRAMESIZE1-4*SZREG(sp)
	REG_S	s1,FRAMESIZE1-5*SZREG(sp)
	REG_S	s2,FRAMESIZE1-6*SZREG(sp)
	REG_S	s3,FRAMESIZE1-7*SZREG(sp)
	REG_S	s4,FRAMESIZE1-8*SZREG(sp)
	REG_S	s5,FRAMESIZE1-9*SZREG(sp)
	REG_S	s6,FRAMESIZE1-10*SZREG(sp)
	REG_S	s7,FRAMESIZE1-11*SZREG(sp)
	REG_S	fp,FRAMESIZE1-12*SZREG(sp)
	REG_S	a1,FRAMESIZE1+1*SZREG(sp)	# Reportlevel
	REG_S	a0,FRAMESIZE1+0*SZREG(sp)	# pflag

	/* DO NOT MODIFY, USE THROUGH OUT THE ENTIRE FILE */
	li	s0,EIGHT_MBYTE

	li	t0,SR_BEV | SR_DE
	mtc0	t0,C0_SR
	/* move from 0-8MB to 8-16MB */
	li	a0,K1_RAMBASE
	addu	a1,a0,s0
	jal	bcopy
	move	a2,s0

	/* use bootstrap vector for exceptions */
	la	fp,trap_handler
	addu	fp,s0
	#li	t0,SR_BEV | SR_BERRBIT | SR_IEC
	li	t0,SR_BEV | SR_DE
	mtc0	t0,C0_SR

	/* execute IDE in high memory */
	addu	gp,s0
	la	t0,1f
	addu	t0,s0
	and	t0, 0x1fffffff
	or	t0, 0xa0000000
	j	t0
	addu	sp,s0

1:
	#*******************

	.set	reorder

	#move	t7,ra			# save return address
	#move	t1,a0			# save first address

	LI	t1, K1_RAMBASE
	move	s2, t1
	PTR_ADDU t2, s0, K1_RAMBASE	# save last address + 4
	addi	t2, -4

	/*
	 * Start Knaizuk Hartmann test
	 */

	/*
	 * Set partitions 1 and 2 to 0's
	 */
	move	s3,zero			# set expected pattern
	addiu	s2,t1,4			# get first partition 1 address
1:
	sw	s3,(s2)			# set partition 1 to zero
	addiu	s2,s2,4			# increment to partition 2 address
	bgeu	s2,t2,2f		# reach the end yet?
	sw	s3,(s2)			# set partition 2 to zero
	addiu	s2,s2,8			# increment to partition 1 address
	bltu	s2,t2,1b		# reach the end yet?

	/*
	 * Set partition 0 to 1's
	 */
2:
	li	s3,-1			# set expected pattern
	move	s2,t1			# get first partition 0 address
3:
	sw	s3,(s2)			# set partition 0 to one's
	addiu	s2,s2,12		# next address
	bltu	s2,t2,3b		# reach the end yet?

	/*
	 * Verify partition 1 is still 0's
	 */
	move	s3,zero			# set expected pattern
	addiu	s2,t1,4			# get first partition 1 address
4:
	lw	s4,(s2)			# read data
	bne	s4,s3,error		# does it match?
	addiu	s2,s2,12		# bump to next location
	bltu	s2,t2,4b		# reach the end yet?

	/*
	 * Set partition 1 to 1's
	 */
	li	s3,-1			# set expected pattern
	addiu	s2,t1,4			# get first partition 1 address
5:
	sw	s3,(s2)			# set partition 1 to one's
	addiu	s2,s2,12		# next address
	bltu	s2,t2,5b		# reach the end yet?

	/*
	 * Verify partition 2 is still 0's
	 */
	move	s3,zero			# set expected pattern
	addiu	s2,t1,8			# get first partition 2 address
6:
	lw	s4,(s2)			# read data
	bne	s4,s3,error		# does it match?
	addiu	s2,s2,12		# bump to next location
	bltu	s2,t2,6b		# reach the end yet?

	/*
	 * Verify partition 0 is still 1's
	 */
	li	s3,-1			# set expected pattern
	move	s2,t1			# get first partition 0 address
7:
	lw	s4,(s2)			# read data
	bne	s4,s3,error		# does it match?
	addiu	s2,s2,12		# bump to next location
	bltu	s2,t2,7b		# reach the end yet?

	/*
	 * Verify partition 1 is still 1's
	 */
	li	s3,-1			# set expected pattern
	addiu	s2,t1,4			# get first partition 1 address
8:
	lw	s4,(s2)			# read data
	bne	s4,s3,error		# does it match?
	addiu	s2,s2,12		# bump to next location
	bltu	s2,t2,8b		# reach the end yet?

	/*
	 * Set partition 0 to 0's
	 */
	move	s3,zero			# set expected pattern
	move	s2,t1			# get first partition 0 address
9:
	sw	s3,(s2)			# set partition 0 to zero's
	addiu	s2,s2,12		# next address
	bltu	s2,t2,9b		# reach the end yet?

	/*
	 * Verify partition 0 is still 0's
	 */
	move	s3,zero			# set expected pattern
	move	s2,t1			# get first partition 0 address
10:
	lw	s4,(s2)			# read data
	bne	s4,s3,error		# does it match?
	addiu	s2,s2,12		# bump to next location
	bltu	s2,t2,10b		# reach the end yet?

	/*
	 * Set partition 2 to 1's
	 */
	li	s3,-1			# set expected pattern
	addiu	s2,t1,8			# get first partition 2 address
11:
	sw	s3,(s2)			# set partition 2 to one's
	addiu	s2,s2,12		# next address
	bltu	s2,t2,11b		# reach the end yet?

	/*
	 * Verify partition 2 is still 1's
	 */
	li	s3,-1			# set expected pattern
	addiu	s2,t1,8			# get first partition 2 address
12:
	lw	s4,(s2)			# read data
	bne	s4,s3,error		# does it match?
	addiu	s2,s2,12		# bump to next location
	bltu	s2,t2,12b		# reach the end yet?

	move	v0,zero			# indicate success
	b	14f			# and return
error:
	li	v0,1
	/* data mismatch, print out error message */
	REG_L	t0,FRAMESIZE1-SZREG(sp)		# error count
	REG_L	t1,FRAMESIZE1+SZREG(sp)		# Reportlevel

9:
	add	t0,1				# increment error count
	REG_S	t0,FRAMESIZE1-SZREG(sp)
	#blt	t1,ERR,10f			# print error message ?

	/* save read data */
	REG_S	s4,FRAMESIZE1-2*SZREG(sp)

	/* print error message */
	la	a0,address_msg			# print 'Address: '
	CALL(_puts)
	move	a0,s2				# print address value
	CALL(_puthex)

	la	a0,expected_msg			# print ', Expected: '
	CALL(_puts)
	move	a0,s3				# print expected value
	CALL(_puthex)

	la	a0,actual_msg			# print ', Actual '
	CALL(_puts)
	REG_L	a0,FRAMESIZE1-2*SZREG(sp)
	CALL(_puthex)

	la	a0,crlf_msg			# print newline
	CALL(_puts)

	.set	noreorder
	#*****************************
14:
	/* move from 8-16MB to 0-8MB  */
	li	a1,K1_RAMBASE
	addu	a0,a1,s0
	move	a2,s0
	CALL(bcopy)

	/* use normal vectors for exceptions */
	#li	t0,SR_BERRBIT | SR_IEC
	mtc0	zero,C0_SR

	/* set up to execute IDE in low memory */
	subu	gp,s0
	la	t0,13f
	j	t0
	subu	sp,s0
13:
	REG_L	v0,FRAMESIZE1-SZREG(sp)		# pass error count

	REG_L	ra,FRAMESIZE1-3*SZREG(sp)
	REG_L	s0,FRAMESIZE1-4*SZREG(sp)
	REG_L	s1,FRAMESIZE1-5*SZREG(sp)
	REG_L	s2,FRAMESIZE1-6*SZREG(sp)
	REG_L	s3,FRAMESIZE1-7*SZREG(sp)
	REG_L	s4,FRAMESIZE1-8*SZREG(sp)
	REG_L	s5,FRAMESIZE1-9*SZREG(sp)
	REG_L	s6,FRAMESIZE1-10*SZREG(sp)
	REG_L	s7,FRAMESIZE1-11*SZREG(sp)
	REG_L	fp,FRAMESIZE1-12*SZREG(sp)
	j	ra
	PTR_ADDU sp,FRAMESIZE1
	.set	reorder
END(kh_low)

	.data
actual_msg:
	.asciiz	", Actual: "
address_msg:
	.asciiz	"\r\nAddress: "
expected_msg:
	.asciiz	"\r\nExpected: "
crlf_msg:
	.asciiz	"\r\n"
