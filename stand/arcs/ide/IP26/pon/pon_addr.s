/*
 * pon_addr.s - power-on address uniqueness test
 *
 * This module contains the function pon_addr, which may be called from
 * power-on level 4 code.  It tests the address lines by walking 1's across
 * them
 */
#ident "$Revision: 1.4 $"

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

	li	v0,256*1024*1024# cannot test more than 256M
	ble	T31,v0,1f
	move	T31,v0

1:
#ifdef	PiE_EMULATOR
	LA      a0,message
	jal     pon_puts
	move    a0,T30		# base address
	jal     pon_puthex64
	li      v1,0x20
	jal     pon_putc
	move    a0,T31		# byte count
	jal     pon_puthex
	LA      a0,crlf
	jal     pon_puts
#endif	/* PiE_EMULATOR */

	li	T32,SZREG	# start from bit N since bits 0/1 are not used
				# (start with bit 3 in 64 bit mode).

	.align	4		# loop is almost 2 quads with trailer instrs
3:
	.set	noreorder
	PTR_ADD	a0,T30,T32
	sll	T32,1
	slt	a1,T32,T31
	bne	a1,zero,3b
	PTR_S	a0,0(a0)	# BDSLOT
	.set	reorder

	move	v0,zero		# test result success by default = 0
	li	T32,SZREG	# start from bit 2 (3 in 64 bit mode)

	.align	4		# loop is three quads
4:
	PTR_ADD	a0,T30,T32
	PTR_L	a2,0(a0)
	beq	a0,a2,5f
	move	a1,a0
	jal	pon_memerr
	li	v0,1		# test result error, so return 1
5:
	sll	T32,1
	blt	T32,T31,4b

	j	RA3

	END(pon_addr)

#ifdef	PiE_EMULATOR
message:        .asciiz "Running address test: "
#endif	/* PiE_EMULATOR */
