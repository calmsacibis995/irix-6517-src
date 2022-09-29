#include <regdef.h>
#include "nsys.h"

	.text

	.globl	ENTRY
ENTRY:
	.set noreorder
	.set	noat

	li	RSYS, GPID
	syscall

	move	r1, RPARM
	add	r2, r1, 1
	add	r3, r2, 1
	add	r6, r3, 1
	add	r7, r6, 1
	add	r8, r7, 1
	add	r9, r8, 1
	add	r10, r9, 1
	add	r11, r10, 1
	add	r13, r11, 1
	add	r14, r13, 1
	add	r15, r14, 1
	add	r16, r15, 1
	add	r17, r16, 1
	add	r18, r17, 1
	add	r19, r18, 1
	add	r20, r19, 1
	add	r21, r20, 1
	add	r22, r21, 1
	add	r23, r22, 1
	add	r24, r23, 1
	add	r25, r24, 1
	add	r28, r25, 1
	add	r29, r28, 1
	add	r30, r29, 1
	add	r31, r30, 1

#ifdef SHORT	
	li	r12, 20
#else
	li	r12, 100
#endif
			
1:
	li	RSYS, PRNTHEX
	move	RPARM, r12
	syscall
	bnez	r12, 1b
	addi	r12, -1		# (BD)

	li	RSYS, GPID
	syscall

	bne	r1, RPARM, 2f
	li	r12, 1

	addi	RPARM	1
	bne	r2, RPARM, 2f
	li	r12, 2

	addi	RPARM	1
	bne	r3, RPARM, 2f
	li	r12, 3

	addi	RPARM, 1
	bne	r6, RPARM, 2f
	li	r12, 6

	addi	RPARM, 1
	bne	r7, RPARM, 2f
	li	r12, 7

	addi	RPARM, 1
	bne	r8, RPARM, 2f
	li	r12, 8

	addi	RPARM, 1
	bne	r9, RPARM, 2f
	li	r12, 9

	addi	RPARM, 1
	bne	r10, RPARM, 2f
	li	r12, 10

	addi	RPARM, 1
	bne	r11, RPARM, 2f
	li	r12, 11

	addi	RPARM, 1
	bne	r13, RPARM, 2f
	li	r12, 13

	addi	RPARM, 1
	bne	r14, RPARM, 2f
	li	r12, 14

	addi	RPARM, 1
	bne	r15, RPARM, 2f
	li	r12, 15

	addi	RPARM, 1
	bne	r16, RPARM, 2f
	li	r12, 16

	addi	RPARM, 1
	bne	r17, RPARM, 2f
	li	r12, 17

	addi	RPARM, 1
	bne	r18, RPARM, 2f
	li	r12, 18

	addi	RPARM, 1
	bne	r19, RPARM, 2f
	li	r12, 19

	addi	RPARM, 1
	bne	r20, RPARM, 2f
	li	r12, 20

	addi	RPARM, 1
	bne	r21, RPARM, 2f
	li	r12, 21

	addi	RPARM, 1
	bne	r22, RPARM, 2f
	li	r12, 22

	addi	RPARM, 1
	bne	r23, RPARM, 2f
	li	r12, 23

	addi	RPARM, 1
	bne	r24, RPARM, 2f
	li	r12, 24

	addi	RPARM, 1
	bne	r25, RPARM, 2f
	li	r12, 25

	addi	RPARM, 1
	bne	r28, RPARM, 2f
	li	r12, 28

	addi	RPARM, 1
	bne	r29, RPARM, 2f
	li	r12, 29

	addi	RPARM, 1
	bne	r30, RPARM, 2f
	li	r12, 30

	addi	RPARM, 1
	bne	r31, RPARM, 2f
	li	r12, 31

	li	RSYS, NPASS
	syscall

3:
	j	3b
	nop

2:
	li	RSYS, SUSPND
	syscall

	li	RSYS, PRNTHEX
	syscall

	li	RSYS, PRNTHEX
	move	RPARM, r12
	syscall

	li	RSYS, NFAIL
	syscall	


