#ident	"$Id: nv_ram.s,v 1.1 1994/07/21 01:15:04 davidl Exp $"
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


#include "sys/regdef.h"
#include "sys/asm.h"
#include "sys/sbd.h"

#include "monitor.h"

	.text

/*
 * NOTE: all NVRAM data are Big-Endian based.
 */


/*
 * int l_nvram(int nv_byte_offset, int data_size)
 *
 * Load a byte, half-word or word from nvram
 * <data_size>: 0 - word (default)
 *		1 - byte
 *		2 - half-word
 *		3 - tri-byte
 *		4 - word
 *
 * uses registers a0, a1, v0 & v1
 */
LEAF(l_nvram)
	bne	a1, zero, 1f		# branch if not default word size
	j	lw_nvram
1:
	bne	a1, 1, 1f		# branch if not byte size
	j	lb_nvram
1:
	bne	a1, 2, 1f		# branch if not half-word size
	j	lh_nvram
1:
	bne	a1, 4, 1f		# branch if not word size
	j	lw_nvram
1:
	bne	a1, 3, 1f		# branch if not tri-byte size

	/*
	 * read tri-byte data from nvram
	 */
	sll	a0, 2			# shift to word offset 
	addu	a0, NVRAM_BASE+3	# form actual k1seg address
	lbu	v0, (a0)		# byte 0
	lbu	v1, 4(a0)		# byte 1
	sll	v0, 8
	or	v0, v1
	lbu	v1, 8(a0)		# byte 2
	sll	v0, 8
	or	v0, v1
	j	ra
1:
	li	v0, -1			# return -1 for error
	j	ra
END(l_nvram)


/*
 * int s_nvram(int data, int nv_byte_offset, int data_size)
 *
 * Store a byte, half-word or word to nvram
 * <data_size>: 0 - word (default)
 *		1 - byte
 *		2 - half-word
 *		3 - tri-byte
 *		4 - word
 *
 * uses registers a0, a1, a2, v0 & v1
 */
LEAF(s_nvram)
	move	v0, zero
	bne	a2, zero, 1f		# branch if not default word size
	j	sw_nvram
1:
	bne	a2, 1, 1f		# branch if not byte size
	j	sb_nvram
1:
	bne	a2, 2, 1f		# branch if not half-word size
	j	sh_nvram
1:
	bne	a2, 4, 1f		# branch if not word size
	j	sw_nvram
1:
	bne	a2, 3, 1f		# branch if not tri-byte size

	/*
	 * write tri-byte data to nvram
	 */
	sll	a1, 2			# shift to word offset 
	addu	a1, NVRAM_BASE+3	# form actual k1seg address
	sb	a0, 8(a1)		# byte 2
	srl	a0, 8
	sb	a0, 4(a1)		# byte 1
        srl     a0, 8
	sb	a0, (a1)		# byte 0
	j	ra
1:
	li	v0, -1			# return -1 for error
	j	ra
END(s_nvram)



/*------------------------------------------------------------------------+
| Routine name: addr_nvram(int byte_offset)				  |
| Description : convert nvram byte offset into address ptr. 		  |
| Syntax: unsigned addr_nvram(unsigned nv_byte_offset)                    |
| Register Usage:                                                         |
|       - a0  byte offset               - v0   return address ptr.        |
+------------------------------------------------------------------------*/
LEAF(addr_nvram)
	sll	a0, 2
	addu	v0, a0, NVRAM_BASE+3
	j	ra
END(addr_nvram)


/*------------------------------------------------------------------------+
| Routine name: char lb_nvram(int nv_byte_offset)			  |
| Description : load a byte from NVRAM.                                   |
| Register Usage:                                                         |
|       - a0  byte offset               - v0   return byte value.         |
+------------------------------------------------------------------------*/
LEAF(lb_nvram)
	sll	a0, 2			# shift to word offset 
	lbu	v0, NVRAM_BASE+3(a0)
	j	ra
END(lb_nvram)


/*------------------------------------------------------------------------+
| Routine name: short lh_nvram(int nv_byte_offset)			  |
| Description : load half word from NVRAM.                                |
| Register Usage:                                                         |
|       - a0  byte offset               - v0   return half value.         |
+------------------------------------------------------------------------*/
LEAF(lh_nvram)
	sll	a0, 2			# shift to word offset 
	addu	a0, NVRAM_BASE+3	# form actual k1seg address
	lbu	v0, (a0)		# read byte 0
	lbu	a0, 4(a0)		# read byte 1
	sll	v0, 8
	or	v0, a0
	j	ra
END(lh_nvram)


/*------------------------------------------------------------------------+
| Routine name: int lw_nvram(int nv_byte_offset)			  |
| Description : load word from NVRAM.                                     |
| Register Usage:                                                         |
|       - a0  byte offset               - v0   return word value.         |
|       - v1  scratch register                                            |
+------------------------------------------------------------------------*/
LEAF(lw_nvram)
	sll	a0, 2			# shift to word offset 
	addu	a0, NVRAM_BASE+3	# form actual k1seg address
	lbu	v0, (a0)		# byte 0
	lbu	v1, 4(a0)		# byte 1
	sll	v0, 8
	or	v0, v1
	lbu	v1, 8(a0)		# byte 2
	sll	v0, 8
	or	v0, v1
	lbu	v1, 12(a0)		# byte 3
	sll	v0, 8
	or	v0, v1
	j	ra
END(lw_nvram)


/*------------------------------------------------------------------------+
| Routine name: void sb_nvram(int data, int nv_byte_offset)		  |
| Description : store byte to NVRAM.                                      |
| Register Usage:                                                         |
|       - a0  write byte               - a1  byte offset                  |
|       - v0  scratch register                                            |
+------------------------------------------------------------------------*/
LEAF(sb_nvram)
	sll	a1, 2			# shift to word offset 
	sb	a0, NVRAM_BASE+3(a1)
	j	ra
END(sb_nvram)


/*------------------------------------------------------------------------+
| Routine name: void sh_nvram(int data, int nv_byte_offset)		  |
| Description : store half word to NVRAM.                                 |
| Register Usage:                                                         |
|       - a0  write half word          - a1  byte offset                  |
|       - v0  scratch register                                            |
+------------------------------------------------------------------------*/
LEAF(sh_nvram)
	sll	a1, 2			# shift to word offset 
	addu	a1, NVRAM_BASE+3	# form actual k1seg address
	ror	a0, 8
	sb	a0, (a1)		# byte 0
        rol     a0, 8
	sb	a0, 4(a1)		# byte 1
	j	ra
END(sh_nvram)


/*------------------------------------------------------------------------+
| Routine name: void sw_nvram(int data, int nv_byte_offset)		  |
| Description : store word to NVRAM.                                      |
| Register Usage:                                                         |
|       - a0  write byte               - a1  byte offset                  |
|       - v0  scratch register                                            |
+------------------------------------------------------------------------*/
LEAF(sw_nvram)
	sll	a1, 2			# shift to word offset 
	addu	a1, NVRAM_BASE+3	# form actual k1seg address
	rol	a0, 8
	sb	a0, (a1)		# byte 0
        rol     a0, 8
	sb	a0, 4(a1)		# byte 1
        rol     a0, 8
	sb	a0, 8(a1)		# byte 2
        rol     a0, 8
	sb	a0, 12(a1)		# byte 3
	j	ra
END(sw_nvram)


/*
 * PUSH & POP functions of nvram stack
 *	void push_nvram(data)	- clobbers only v0 & v1
 *	int  pop_nvram(void)	- clobbers only v0 & v1
 */

/*------------------------------------------------------------------------+
| Routine name: void push_nvram(int data)				  |
|									  |
| Description : push a word onto nvram based stack			  | 
|									  |
| Register Usage: (only clobber v0 & v1)				  |
|									  |
|       - a0  data to store on stack	- v0   scratch register           |
|       - v1  scratch register						  |
+------------------------------------------------------------------------*/
LEAF(push_nvram)

	#
	# get nv-stack pointer: _NV_SP
	#
	li	v0, NVRAM_BASE+(_NV_SP<<2)+3 # form k1seg nvram addr

	.set	noat
	move	$at, v0			# use at as address pointer
	lbu	v0, ($at)		# byte 0
	lbu	v1, 4($at)		# byte 1
	sll	v0, 8
	or	v0, v1
	lbu	v1, 8($at)		# byte 2
	sll	v0, 8
	or	v0, v1
	lbu	v1, 12($at)		# byte 3
	sll	v0, 8
	or	v0, v1			# v0 is now the stack pointer
	move	v1, $at			# save stack-ptr address in v1
	.set	at

	#
	# store data to stack
	#
     	rol     a0, 8
        sb      a0, (v0)		# byte 0
        rol     a0, 8
        sb      a0, 4(v0)		# byte 1
        rol     a0, 8
        sb      a0, 8(v0)		# byte 2
        rol     a0, 8
        sb      a0, 12(v0)		# byte 3

	#
	# save new stack pointer
	#
	subu	v0, 16			# new top address

     	rol     v0, 8
        sb      v0, (v1)		# byte 0
        rol     v0, 8
        sb      v0, 4(v1)		# byte 1
        rol     v0, 8
        sb      v0, 8(v1)		# byte 2
        rol     v0, 8
        sb      v0, 12(v1)		# byte 3

	# 
	# check for stack pointer overflow
	#
	and	v1, v0, 0x000f0000	# get slot offset in v1
	or	v1, NVRAM_BASE+(NVSTACK_END<<2)+3
	bgeu	v0, v1, 1f		# overflow (sp < end)?

	la	v0, _nvstack_overflow	# handle overflow
	j	v0
1:
	j	ra
END(push_nvram)


/*------------------------------------------------------------------------+
| Routine name: int pop_nvram(void)					  |
|									  |
| Description : pop a word from nvram based stack			  | 
|									  |
| Register Usage: (only clobber v0 & v1)				  |
|									  |
|       - v0   scratch register           - v1  scratch register	  |
+------------------------------------------------------------------------*/
LEAF(pop_nvram)

	#
	# get nv-stack pointer: _NV_SP
	#
	li	v0, NVRAM_BASE+(_NV_SP<<2)+3 # form k1seg nvram addr

	.set	noat
	move	$at, v0			# use at as address pointer
	lbu	v0, ($at)		# byte 0
	lbu	v1, 4($at)		# byte 1
	sll	v0, 8
	or	v0, v1
	lbu	v1, 8($at)		# byte 2
	sll	v0, 8
	or	v0, v1
	lbu	v1, 12($at)		# byte 3
	sll	v0, 8
	or	v0, v1			# v0 is now the stack pointer
	.set	at

	addu	v0, 16			# new top address 

	# 
	# check for stack pointer underflow
	#
	and	v1, v0, 0x000f0000	# get slot offset in v1
	or	v1, NVRAM_BASE+(NVSTACK_BASE<<2)+3
	bleu	v0, v1, 1f 		# underflow (sp > base)?

	la	v0, _nvstack_underflow	# handle underflow
	j	v0
1:
	#
	# save new stack pointer
	#
	and	v1, v0, 0x000f0000	# get slot offset in v1
	or	v1, NVRAM_BASE+(_NV_SP<<2)+3 # form k1seg nvram addr

     	rol     v0, 8
        sb      v0, (v1)		# byte 0
        rol     v0, 8
        sb      v0, 4(v1)		# byte 1
        rol     v0, 8
        sb      v0, 8(v1)		# byte 2
        rol     v0, 8
        sb      v0, 12(v1)		# byte 3

	#
	# load data from stack
	#
	.set	noat
	move	$at, v0			# use at as address pointer
	lbu	v0, ($at)		# byte 0
	lbu	v1, 4($at)		# byte 1
	sll	v0, 8
	or	v0, v1
	lbu	v1, 8($at)		# byte 2
	sll	v0, 8
	or	v0, v1
	lbu	v1, 12($at)		# byte 3
	sll	v0, 8
	or	v0, v1			# v0 is now the stack pointer
	.set	at

	j	ra
END(pop_nvram)


/*
 * int strcp_nv(char *nvstr1, char *nvstr2)
 *
 * Copy nvram based str1 into str2 until a null char in str1
 *
 * Return the number of char's copied
 *
 * Register used: a0, a1, a2, v0 
 */
LEAF(strcp_nv)
	move	v0, zero	# zero the count
1:
	lbu	a2, (a0)	# read str1
	addu	a0, 4		# point to next char
	sb	a2, (a1)	# write to str2
	addu	a1, 4
	beq	a2, zero, 2f	
	addu	v0, 1		# bump the count
	b	1b
2:
	j	ra
END(strcp_nv)

/* end */
