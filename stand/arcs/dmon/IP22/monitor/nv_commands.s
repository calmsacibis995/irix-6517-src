#ident	"$Id: nv_commands.s,v 1.1 1994/07/21 00:18:33 davidl Exp $"
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
#include "sys/sbd.h"
#include "sys/asm.h"
#include "pdiag.h"
#include "monitor.h"


/*
 * void print_nv_cmd()
 *
 *	Print the nvram debug command table.
 *
 * Register Usage:
 */
LEAF(print_nv_cmd)
	move	s7,ra
	la	a0, spacing
	jal	_puts
	la	s1,nv_cmd_table
1:
	la	a0, _crlf
	jal	_puts
	lw	a0, (s1)
	beq	a0, zero, _table_end
	jal	_puts
	addu	s1, CTBLENTRY_SIZE
	b	1b
_table_end:
	la	a0, _crlf
	jal	_puts
	j	s7
END(print_nv_cmd)


/*
 * nv_cmd_parser
 *
 *	The nvram debug menu command parser. The available commands are
 *		1. read nvram byte with looping
 *		2. write nvram byte with looping
 *		3. fill nvram bytes with byte count
 *		4. dump nvram bytes with byte count
 *		5. exit to start the prom again
 */
LEAF(nv_cmd_parser)
	move	s7, ra
1:
	PRINT("\nNV_DEBUG>")
	move	t1, zero		/* char input count */
2:
	jal	_getchar
	and	v0, 0x7f

	bne	v0, char_BS, 10f	/* backspace */
	beq	t1,zero, 2b		/* can not backspace anymore */
	li	a0, char_BS
	jal	_putchar
	sub	t1,1
	b	2b
10:
	beq	v0, char_CR, 1b
	beq	v0, char_NL, 1b
	add	t1,1
	beq	v0, char_SP, 2b

	and	v0, 0xdf		/* convert to upper case */

	bne	v0, char_R, 3f		/* char r is typed */
	jal	nv_cmd_read
	j	5f
3:
	bne	v0, char_W, 4f		/* char w is typed */
	jal	nv_cmd_write
	j	5f
4:
	bne	v0, char_F, 11f		/* char f is typed */
	jal	nv_cmd_fill
	j	5f
11:
	bne	v0, char_D, 7f		/* char d is typed */
	jal	nv_cmd_dump
	j	5f
7:
	bne	v0, char_A, 12f		/* char a is typed */
	jal	nvram_addrudbg
	j	5f
12:
	bne	v0, char_E, 6f		/* char e is typed for exit */
	j	_prom_start
6:
	PRINT("\nIllegal command\n")
5:
	
	j	s7
END(nv_cmd_parser)

LEAF(nvram_debug)
	move	s6, ra
1:
	INSTALLVECTOR(2f)
	jal	print_nv_cmd

	jal	nv_cmd_parser

	j	1b
2:
	/*
	 * Save the state of the cpu
	 */
	.set	noreorder
	mfc0	s0, C0_SR
	mfc0	s1, C0_EPC
	mfc0	s2, C0_CAUSE
	mfc0	s3, C0_CACHE_ERR
	mfc0	s4, C0_ERROR_EPC
	.set	reorder

	PRINT("\nUnexpected exception.\n")
	PRINT("\n   STATUS     : 0x")
	PUTHEX(s0)
	PRINT("   EPC        : 0x")
	PUTHEX(s1)
	PRINT("\n   CAUSE      : 0x")
	PUTHEX(s2)
	PRINT("   CACHEERR   : 0x")
	PUTHEX(s3)
	PRINT("\n   ERROREPC   : 0x")
	PUTHEX(s4)

	j	1b
END(nvram_debug)

LEAF(nv_cmd_read)

	move	t7, ra

	PRINT("\nRead: address:0x________")
	PRINT("\b\b\b\b\b\b\b\b")

	jal	gethex			/* get address */
	move	s0, v0			/* save address in s0 */
	bne	v1,zero, 30f		/* 0 = good, !0 = bad input */

	PRINT("      loop count:")
	jal	getdec			/* get loop count */
	move	s1, v0			/* save loop count in s2 */
	bne	v1,zero, 40f

	PRINT("\nlbu (0x")
	PUTHEX(s0)
	PRINT(")")

	beq	s1, zero, 20f		/* if s2 == 0, infinite looping */
	nop
1:
	lbu	s2, (s0)		/* load byte */

	PUTS(_crlf)
	PUTHEX(s2)

	subu	s1,1			/* decrement loop count */
	bne	s1,zero,1b

	move	v0, zero
	j	50f

	/*
	 * Loop forever
	 */
20:
	PRINT(" --- loop forever\n")
21:
	lbu	s2, (s0)		/* load byte */

	jal	may_getchar		/* check for ESC */
	bne	v0,char_ESC,21b
	nop

	move	v0, zero
	j	50f
	
30:
	la	a0, bad_addr
	jal	_puts
	j	50f
40:
	la	a0, bad_lc
	jal	_puts
50:
	j	t7
END(nv_cmd_read)


LEAF(nv_cmd_write)

	move	t7, ra

	PRINT("\nWrite: address:0x________")
	PRINT("\b\b\b\b\b\b\b\b")

	jal	gethex			/* get address */
	move	s0, v0			/* save address in s0 */
	bne	v1,zero, 30f		/* 0 = good, !0 = bad input */

	PRINT("       data:0x__")
	PRINT("\b\b")
	jal	gethex			/* get byte data */
	move	s1, v0			/* save byte count in s1 */
	bne	v1,zero, 40f 

	PRINT("       loop count:")
	jal	getdec			/* get loop count */
	move	s2, v0			/* save loop count in s2 */
	bne	v1,zero, 50f

	PRINT("\nsb 0x")
	PUTHEX(s1)
	PRINT(", (0x")
	PUTHEX(s0)
	PRINT(")")

	beq	s2, zero, 20f		/* if s2 == 0, infinite looping */
	nop
	PRINT("\n")
1:
	sb	s1, (s0)		/* store byte */

	subu	s2,1			/* decrement loop count */
	bne	s2,zero,1b

	move	v0, zero
	j	60f

	/*
	 * Loop forever
	 */
20:
	PRINT(" --- loop forever\n")
21:
	sb	s1, (s0)		/* store byte */

	jal	may_getchar		/* check for ESC */
	bne	v0,char_ESC,21b
	nop

	move	v0, zero
	j	60f
	
30:
	la	a0, bad_addr
	jal	_puts
	j	60f
40:
	la	a0, bad_data
	jal	_puts
	j	60f
50:
	la	a0, bad_lc
	jal	_puts
60:
	j	t7
END(nv_cmd_write)


LEAF(nv_cmd_fill)

	move	t7, ra

	PRINT("\nFill: address:0x________")
	PRINT("\b\b\b\b\b\b\b\b")

	jal	gethex			/* get address */
	move	s0, v0			/* save address in s0 */
	bne	v1,zero, 30f	/* 0 = good, !0 = bad input */

	PRINT("      data:0x__")
	PRINT("\b\b")
	jal	gethex			/* get data to write */
	move	s1, v0			/* save data in s1 */
	bne	v1, zero, 40f

	PRINT("      byte count:")
	jal	getdec			/* get byte count */
	move	s2, v0			/* save byte count in s1 */
	bne	v1,zero, 50f 

	bgt	s2, zero, 1f		/* byte count must be at least 1 */
	li	s2, 1
1:
	PRINT("\nFill 0x")
	PUTHEX(s1)
	PRINT(", (0x")
	PUTHEX(s0)
	PRINT(") --- ")
	PUTDEC(s2)
	PRINT(" bytes\n")
2:
	sb	s1, (s0)		/* store byte */

	addu	s0, 4			/* next byte */
	subu	s2,1			/* decrement byte count */
	bne	s2,zero, 2b

	move	v0, zero
	j	60f
	
30:
	la	a0, bad_addr
	jal	_puts
	j	60f
40:
	la	a0, bad_data
	jal	_puts
	j	60f
50:
	la	a0, bad_lc
	jal	_puts
60:
	j	t7
END(nv_cmd_fill)


LEAF(nv_cmd_dump)

	move	t7, ra

	PRINT("\nDump: address:0x________")
	PRINT("\b\b\b\b\b\b\b\b")

	jal	gethex			/* get address */
	move	s0, v0			/* save address in s0 */
	bne	v1,zero, 30f	/* 0 = good, !0 = bad input */

	PRINT("      byte count:")
	jal	getdec			/* get byte count */
	move	s1, v0			/* save byte count in s1 */
	bne	v1,zero, 40f 

	bne	s1, zero, 1f		/* byte count must at least 1 */
	li	s1, 1
1:
	lbu	s2, (s0)		/* load byte */

	PUTS(_crlf)			/* display data */
	PUTHEX(s2)

	addu	s0, 4			/* next byte */
	subu	s1,1			/* decrement byte count */
	bne	s1,zero, 1b

	move	v0, zero
	j	50f
	
30:
	la	a0, bad_addr
	jal	_puts
	j	50f
40:
	la	a0, bad_bc
	jal	_puts
50:
	j	t7
END(nv_cmd_dump)

	.data
spacing:
	.asciiz	"\r\n\n\n"
bad_addr:
	.asciiz	"\nBad address"
bad_data:
	.asciiz	"\nBad data"
bad_bc:
	.asciiz	"\nBad byte count"
bad_lc:
	.asciiz	"\nBad loop count"
/* end */
