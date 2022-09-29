#ident	"IP22diags/pon/pon_addr.s:  $Revision: 1.4 $"

/*
 * pon_addr.s - power-on address uniqueness test
 *
 * This module contains the function pon_addr, which may be called from
 * power-on level 4 code.  It tests the address lines by walking 1's across
 * them
 */

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
#ifdef	NOTDEF
#ifdef	PiE_EMULATOR
	la      a0,message
	jal     pon_puts
	move    a0,T30		# base address
	jal     pon_puthex
	li      v1,0x20
	jal     pon_putc
	move    a0,T31		# byte count
	jal     pon_puthex
	la      a0,crlf
	jal     pon_puts
#endif	/* PiE_EMULATOR */
#endif	/* NOTDEF */

	li	T32,SZREG	# start from bit N since bits 0/1 are not used
				# (start with bit 3 in 64 bit mode).
3:
	PTR_ADD	a0,T30,T32
	PTR_S	a0,0(a0)
	sll	T32,1
	blt	T32,T31,3b

	move	v0,zero		# test result success by default = 0
	li	T32,SZREG	# start from bit 2 (3 in 64 bit mode)
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
message:        .asciiz "Running address test \r\n"
#endif	/* PiE_EMULATOR */
