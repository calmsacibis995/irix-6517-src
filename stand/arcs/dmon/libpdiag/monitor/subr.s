#ident	"$Id: subr.s,v 1.1 1994/07/21 01:16:20 davidl Exp $"
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

#define char_SP		0x20	/* ' ' */
#define char_DASH	0x2d	/* '-' */

/*
 *  Diags Monitor Subroutines (non-machine dependent)
 *
 */
	.data

msg_crlf:	.asciiz	"\r\n"
msg_crlf2:	.asciiz	"\r\n\n"
msg_pause:	.asciiz "\r\n\nHit a key to continue... "

_tick_sym:	.asciiz	"|/-\\"

BSS(_clk_ticks, 4)		# count clock ticks for do_clk().
BSS(_dot_cnt, 4)		# count dots for do_dot().

	.text

/*------------------------------------------------------------------------+
| void regs_init( void )                                                  |
|									  |
| Description : clear all general purpose registers.                      |
+------------------------------------------------------------------------*/
LEAF(regs_init)
	.set	noat
	move	AT, zero
	.set	at
	move	v0, zero
	move	v1, zero
	move	a0, zero
	move	a1, zero
	move	a2, zero
	move	a3, zero
	move	t0, zero
	move	t1, zero
	move	t2, zero
	move	t3, zero
	move	t4, zero
	move	t5, zero
	move	t6, zero
	move	t7, zero
	move	t8, zero
	move	t9, zero
	move	s0, zero
	move	s1, zero
	move	s2, zero
	move	s3, zero
	move	s4, zero
	move	s5, zero
	move	s6, zero
	move	s7, zero
	move	k0, zero
	move	k1, zero
	move	gp, zero
	move	sp, zero
#ifndef DIAG_MONITOR
	move	fp, zero	# can't be cleared in diags monitor.
#endif !DIAG_MONITOR
	mthi	zero
	mtlo	zero
	j	ra
END(regs_init)


/*
 * void _pause(int print_date)
 *
 * print the current date if print_data != 0
 * and pause until a key entered
 *
 * registers used: a0 & t0
 */
LEAF(_pause)
	move	t0, ra		# t0 not trashed by _puts() & _putchar()
	la	a0, msg_pause
	jal	_puts
	jal	_getchar	# wait for a character
	la	a0, msg_crlf	# linefeed & cr
	jal	_puts
	j	t0
END(_pause)

/*
 * int strlen(char *str_ptr)
 *
 * return the length of a null terminated char string
 *
 * registers used: a0, a1, v0 
 */
LEAF(strlen)
	move	v0,zero		# zero the count
1:
	lbu	a1,(a0)		# check next char
	addu	a0,1		# point to next char
	beq	a1,zero,2f	
	addu	v0,1		# bump the count
	b	1b
2:
	j	ra
END(strlen)
	
/*
 * void printsp(int x)
 *
 * print x number of spaces
 *
 * registers used: a0, t0 & t1 
 */
LEAF(printsp)
	move	t1, ra
	beq	a0, zero, 2f
	move	t0, a0		# t0 is the counter
1:
	li	a0, char_SP
	jal	_putchar	# send the char
	subu	t0, 1
	bne	t0, zero, 1b
2:
	j	t1
END(printsp)
	

/*
 * unsigned chkopt_nv(char *nvram_bufptr, char opt_char)
 *
 * Check (verify) command option
 *
 * Return 1 if the option specified on the command line is 
 * 'opt_char', otherwise, 0. Command options are prefixed
 * with "-". Advance the buf ptr if the option matches.
 * Option char check is case-insensitive.
 * 
 * register used:
 *	a0, a1, a2, v0
 */
LEAF(chkopt_nv)
	.set 	noreorder
	move	v0,zero		# assume no match
1:
	lbu	a2,(a0)		# skip spaces 
	bne	a2,char_SP,1f
	nop
	b	1b
	addu	a0,4		# (BDSLOT) bump to next char
1:
	bne	a2,char_DASH,1f	# all options have a prefix of '-'
	nop
	lbu	a2,4(a0)	# get option char
	nop
	and	a2,0x5F		# map to upper case
	and	a1,0x5F		# map to upper case
	bne	a1,a2,1f	# return if no match
	nop
	addu	a0,8		# bump buf ptr
	or	v0,zero,1	# return 1 if option matches
1:
	j	ra
	nop
	.set	reorder
END(chkopt_nv)


/*
 * unsigned getopt_nv(char *nvram_bufptr)
 *
 * Return (get) command option
 *
 * Return the next command option specified on the command line.
 * If no option given, return 0. Command options are prefixed 
 * with "-". Advance the buf ptr if option exists.
 * 
 * register used:
 *	a0, a1, v0
 */
LEAF(getopt_nv)
	.set	noreorder
	move	v0,zero		# assume no option 
1:
	lbu	a1,(a0)		# skip spaces 
	bne	a1,char_SP,1f
	nop
	b	1b
	addu	a0,4		# (BDSLOT) bump to next char
1:
	bne	a1,char_DASH,1f	# all options have a prefix of '-'
	nop
	lbu	v0,4(a0)	# get option char
	nop
	beq	v0,zero,2f	# check for invalid option char
	nop
	beq	v0,char_SP,2f	# 
	and	v0,0x5F		# (BDSLOT) map to upper case
	addu	a0,8		# bump buf ptr
1:
	j	ra
	nop
2:
	addu	a0,4		# skip the dash if invalid option char
	j	ra
	move	v0,zero		# (BDSLOT) return for invalid option
	.set	reorder
END(getopt_nv)


/*
 * int ndigits(unsigned int decvalue)
 *
 * Returns the number of digits of decvalue
 *
 * Register used: a0 & v0 (a0 is unchanged)
 */
LEAF(ndigits)
	.set	noreorder
	move	v0, zero	# zero digit count
	bltu	a0,10,1f
	addi	v0,1		# (BDSLOT) incr. count
	bltu	a0,100,1f
	addi	v0,1		# (BDSLOT) incr. count
	bltu	a0,1000,1f
	addi	v0,1		# (BDSLOT) incr. count
	bltu	a0,10000,1f
	addi	v0,1		# (BDSLOT) incr. count
	bltu	a0,100000,1f
	addi	v0,1		# (BDSLOT) incr. count
	bltu	a0,1000000,1f
	addi	v0,1		# (BDSLOT) incr. count
	bltu	a0,10000000,1f
	addi	v0,1		# (BDSLOT) incr. count
	bltu	a0,100000000,1f
	addi	v0,1		# (BDSLOT) incr. count
	bltu	a0,1000000000,1f
	addi	v0,1		# (BDSLOT) incr. count
1:
	j	ra
	nop
	.set	reorder
END(ndigits)


#define RAND_MULT	1103515245
#define RAND_ADD	12345

/*------------------------------------------------------------------------+
| int rand(int seed)                                        		  |
|									  |
| Description : Rand uses  a multiplicative  congruential  random  number |
|   generator with period 2**32 to return successive pseudo-random number |
|   numbers in the range from 0 to (2**31)-1.                             |
|									  |
| Register Usage:                                                         |
|   - a0  seed                          - a1  scratch register.           |
|   - v0  return randome number (new seed value)                          |
+------------------------------------------------------------------------*/
LEAF(rand)
	mul	v0, a0, RAND_MULT
	addiu	v0, RAND_ADD
	j	ra
END(rand)


/* 
 * void _tick(int index)
 *
 * Print a clock tick using the index (bit0-3)
 *
 * Registers used: a0-a3, v0-v1
 */
LEAF(_tick)
	move	a1, a0		# save a0 in a1
	move	a0, ra
	jal	push_nvram	# push ra (clobbers only v0 & v1)
	move	a0, a1
	jal	push_nvram	# push a0 (clobbers only v0 & v1)

	li	a0, 0x08	# \b
	jal	_putchar

	jal	pop_nvram	# restore a0
	and	a0, v0, 0x03
	la	a1, _tick_sym
	addu	a1, a0
	lbu	a0, (a1)
	jal	_putchar

	jal	pop_nvram	# restore ra
	j	v0
END(_tick)

/*
 * void do_clk( void )
 *
 * print clock-like ticks using a memory based variable.
 *
 * registers used: a0 and a1
 */
LEAF(do_clk)
	la	a1, _clk_ticks
	lw	a0, (a1)
	addu	a0, 1
	sw	a0, (a1)
	j	_tick		# branch to _tick()
END(do_clk)


/*------------------------------------------------------------------------+
| Routine name: do_dots                                                   |
|									  |
| Description : print a dot on the screen. If the dot count reach 6, undo |
|   the dots.                                                             |
|									  |
| Registers used: a0, v0 & t0						  |
+------------------------------------------------------------------------*/
LEAF(do_dots)
	move	t0, ra			# save ra in t0
	lw	v0, _dot_cnt
	bgt	v0, 5, undo_dots

	addi	v0, 1
	sw	v0, _dot_cnt
	li	a0, 0x2e		# put a '.' on the screen
	jal	_putchar
	j	t0			# return

undo_dots:
	la	a0, 0x8			# back space
	jal	_putchar
	la	a0, 0x20		# blank out
	jal	_putchar
	la	a0, 0x8			# back space
	jal	_putchar
	addiu	v0, -1
	bgt	v0, zero, undo_dots

	sw	zero, _dot_cnt
	j	t0
END(do_dots)

/* end */

