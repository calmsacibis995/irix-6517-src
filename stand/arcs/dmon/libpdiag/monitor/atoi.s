#ident	"$Id: atoi.s,v 1.1 1994/07/21 01:13:02 davidl Exp $"
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#include "asm.h"
#include "regdef.h"

#define	CHAR_SP			0x20
#define	CHAR_NEG		0x2D
#define	CHAR_A			0x41
#define	CHAR_B			0x42
#define	CHAR_F			0x46
#define	CHAR_X			0x58
#define	CHAR_0			0x30
#define	CHAR_9			0x39

/*
 * Loop-Count/Flag/Address_Step Masks
 *
 * bit<3..0>	- Address increment value
 *		  (1 for memory based string, 4 for nvram based string)
 * bit<4>	- Negative number flag
 * bit<31..16>	- Loop counter
 */
#define ADDR_STEP	0x000f		/* address increment step. */
#define	NEG_NUM		0x0010 		/* negative number. */
#define LOOPCNT_MASK	0xffff0000
#define LOOPINC		0x10000

	.text
	.align	2

	.set	noreorder	# What you see is what you get.

/*------------------------------------------------------------------------+
| int aton(char *nptr)                                                    |
|                                                                         |
| Description : Hook-up to atoi routine.                                  |
+------------------------------------------------------------------------*/
LEAF(aton)
	j	atoi			# jump to atoi routine
	nop				# operands and "ra" are untouched.
END(aton)


/*------------------------------------------------------------------------+
| int atoi(char *nptr)                                                    |
|                                                                         |
| Description : This function  converts  a string pointed  to by  nptr to |
|   integer  representation.  The first  unrecognized  character ends the |
|   string.  It recognizes optional string of spaces,  optional  "c" like |
|   number format for hexa-decimal and octal number, optional sign,  then |
|   a string of digits.  It returns the resulting number in "v0" and  the |
|   flag in "v1".  flag=0 means success, flag=1 means failure.            |
|                                                                         |
| Syntax :                                                                |
|   int atoi(nptr)                                                        |
|       register char *nptr;                                              |
|                                                                         |
| Register Usage:                                                         |
|       - a0  pointer to string.        - a1  base value.                 |
|       - a2  read value.               - a3  scratch register.           |
|       - v0  accumulated value         - v1  loop count/flag/addr_step.  |
+------------------------------------------------------------------------*/
LEAF(atoi)
	li	v1, 1			# set address_step to 1 for mem string
	j	atoi_x			# jump to the shared code with atoi_nv
	nop
END(atoi)


/*------------------------------------------------------------------------+
| int aton_nv( char *nv_ptr )                                             |
|                                                                         |
| Description : Hook-up to atoi_nv routine.                               |
+------------------------------------------------------------------------*/
LEAF(aton_nv)
	j	atoi_nv			# jump to atoi_nv routine
	nop				# operands and "ra" are untouched.
END(aton_nv)


/*------------------------------------------------------------------------+
| int atoi_nv( char *nv_ptr )                                             |
|                                                                         |
| Description : This function converts a string pointed  to by  nv_ptr to |
|   integer  representation.  The first  unrecognized  character ends the |
|   string.  It recognizes optional string of spaces,  optional  "c" like |
|   number format for hexa-decimal and octal number, optional sign,  then |
|   a string of digits.  It returns the resulting number in "v0" and  the |
|   flag in "v1".  flag=0 means success, flag=1 means failure.            |
|                                                                         |
|   Note: nv_ptr should be a k1seg address pointing to the NVRAM address  |
|   space.                                                                |
|                                                                         |
| Register Usage:                                                         |
|       - a0  pointer to nv_ram.        - a1  base value.                 |
|       - a2  read value.               - a3  scratch register.           |
|       - v0  accumulated value         - v1  loop count/flag/addr_step.  |
+------------------------------------------------------------------------*/
LEAF(atoi_nv)

	li	v1, 4			# set address_step to 4 for nvram

atoi_x:
	/*
	 * Depending on the value in v1<15..12> which is either 1 or 4,
	 * we could be converting a string based in main memory or NVRAM.
	 */
	move	v0, zero		# clear accumulated value.

	#+----------------------------------------------------------------+
	#| check for null pointer or null string.                         |
	#+----------------------------------------------------------------+
chk_null_str:
	beq	a0, zero, atoi_done	# check for null pointer.
	nop
	lbu	a2, (a0)
	nop
	beq	a2, zero, atoi_done
	nop

	#+----------------------------------------------------------------+
	#| skip space character.                                          |
	#+----------------------------------------------------------------+
	and	a3, v1, ADDR_STEP	# a3 is address increment

chk_sp_char:
	lbu	a2, (a0)
	nop
	beq	a2, CHAR_SP, chk_sp_char
	add	a0, a3			# (BDSLOT) advance ptr for each byte
	subu	a0, a3 

	#+----------------------------------------------------------------+
	#| check for minus sign.                                          |
	#+----------------------------------------------------------------+
chk_sign:
	lbu	a2, (a0)
	nop
	bne	a2, CHAR_NEG, chk_base
	nop
	or	v1, NEG_NUM
	add	a0, a3

	#+----------------------------------------------------------------+
	#| Determine base by looking at first 2 characters                |
	#+----------------------------------------------------------------+
chk_base:
	lbu	a2, (a0)
	nop
	bne	a2, CHAR_0, do_convert
	li	a1, 10			# (BDSLOT) base 10

	add	a0, a3			# check for special format
	lbu	a2, (a0)
	nop

	and	a2, 0x5F		# hexa-decimal representation ?
	li	a1, 16			# base 16
	beq	a2, CHAR_X, do_convert
	add	a0, a3			# (BDSLOT)

	beq	a2, CHAR_B, do_convert	# binary representation ?
	li	a1, 2			# base 2

	li	a1, 8			# use base of 8 (octal number).
	subu	a0, a3			# sub twice here in case it is just "0"
	subu	a0, a3			# 

	#+----------------------------------------------------------------+
	#| check for valid number digit (0-9 or A-F) and perform ASCII to |
	#| digit convertion.                                              |
	#+----------------------------------------------------------------+
do_convert:
	lbu	a2, (a0)
	nop

	blt	a2, CHAR_0, 1f		# check for decimal number 0-9.
	li	a3, 99
	ble	a2, CHAR_9, 1f
	sub	a3, a2, CHAR_0

	and	a2, 0x5f		# check for hex-digit 'A'-'F'
	blt	a2, CHAR_A, 1f
	li	a3, 99
	bgt	a2, CHAR_F, 1f
	nop

	sub	a3, a2, CHAR_A
	addiu	a3, 10
1:
	bge	a3, a1, do_sign
	nop

	multu	v0, a1			# value = (value * base) + digit
	mflo	v0
	addu	v0, a3

	add	v1, LOOPINC		# increment loop count.
	and	a3, v1, ADDR_STEP	# a3 is address increment
	b	do_convert
	add	a0, a3			# (BDSLOT) bump to next char

	#+----------------------------------------------------------------+
	#| check for negative number.                                     |
	#+----------------------------------------------------------------+
do_sign:
	and	a3, v1, NEG_NUM
	beq	a3, zero, atoi_done
	nop
	sub	v0, zero, v0

	#+----------------------------------------------------------------+
	#| look at the loop count, lower 12 bit of v1, to determine if we |
	#| have done any conversion. if count is zero, return 1 in v1 to  |
	#| indicate failure.                                              |
	#+----------------------------------------------------------------+
atoi_done:
	and	v1, LOOPCNT_MASK	# did we do any conversion ?
	beq	v1, zero, 1f
	li	v1, 1
	move	v1, zero
1:
	j	ra			# return to caller.
	nop
END(atoi_nv)
