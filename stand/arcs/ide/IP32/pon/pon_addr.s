/*
 * pon_addr.s - power-on address uniqueness test
 *
 * This module contains the function pon_addr, which may be called from
 * power-on level 4 code.  It tests the address lines by walking 1's across
 * them
 */
#ident "$Revision: 1.1 $"

#include "asm.h"
#include "early_regdef.h"

/* 
 * T30 = first address
 * T31 = memory size in bytes
 * T32 = address offset
 */
LEAF(pon_addr)			# address uniqueness test
	move	RA3,ra		# save our return address
 	move	T30,a0		# first address
	move	T31,a1		# size in bytes

1:
#ifdef	VERBOSE
	dla	a0,message
	jal	pon_puts
	move	a0,T30		# base address
	jal	pon_puthex64
	li	a0,0x20		# ASCII for space
	jal	pon_putc
	move	a0,T31		# byte count
	jal	pon_puthex64	# total memory may >= 4 GB
	dla	a0,crlf
	jal	pon_puts
#endif	/* VERBOSE */

	/* start from bit 3 since bits 0..2 are not used to address DIMMs */
	li	T32,SZREG

	/* write loop */
3:
	.set	noreorder
	daddu	a0,T30,T32
	dsll	T32,1
	blt	T32,T31,3b
	sd	a0,0(a0)	# BDSLOT
	.set	reorder

	move	v0,zero		# test result success by default = 0
	li	T32,SZREG
4:
	/* read-verify loop */
	daddu	a0,T30,T32
	ld	a2,0(a0)
	beq	a2,a0,5f
	move	a1,a0
	jal	pon_memerr
	li	v0,1		# test result error, so return 1
5:
	dsll	T32,1
	blt	T32,T31,4b

	j	RA3

	END(pon_addr)

#ifdef	VERBOSE
	.data
message:        .asciiz "Running address test: "
#endif	/* VERBOSE */
