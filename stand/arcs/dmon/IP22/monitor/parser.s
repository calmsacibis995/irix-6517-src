#ident	"$Id: parser.s,v 1.1 1994/07/21 00:18:41 davidl Exp $"
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
#include "monitor.h"
#include "pdiag.h"


	.text

	.globl	warm_reset

/*
 * comamnd_parser: void cmd_parser(void)
 *
 * Functions
 *	At the monitor prompt, the user enters either a number to run
 * a test in the test table or a command name.
 *
 * To run a test, RunTests() is called. For a command, it is called
 * in this function as:
 *
 *	int cmd_x(int nvram_buf_ptr, int parm1)
 * where 
 * 	nvram_buf_ptr points to the rest of command line
 *	parm1 is from the command table
 *
 * return status
 *	0 - OK, 1 - Error in command
 *
 *
 * register usage:
 *	s0 - ptr to nvram buf string
 * 	s1 - test number
 *
 * registers used:
 *	a0, v0, t0, s0, s1
 */

LEAF(cmd_parser)

_getinput:
	#
	# print command prompt
	#
	PRINT("\nDiag")
	and	t0, CnfgStatusReg, MLFLAG_MASK # main menu?
	beq	t0, zero, 1f		# if not, print module name
	PRINT("/")
	li	a0, _MODNAME		# get current module name ptr
	jal	lw_nvram
	move	a0, v0
	jal	_puts
1:
#ifdef DEBUG
	PRINT(" >>")
#else
	PRINT(" >")
#endif DEBUG

	# li	a0,_NVBUF		# convert byte offset of nvram buf into addr ptr
	# jal	addr_nvram
	# move	a0,v0
	# jal	gets_nv			# get a <cr> terminated string from console

	jal	_get_cmdstr		# get a command string into nvram buffer (_NVBUF)
	beq	v0,zero,_getinput	# wait until something entered

	li	a0,_NVBUF		# convert byte offset of nvram buf into addr ptr
	jal	addr_nvram
	move	s0,v0			# s0 is ptr to nvram buf string

	/*
	 * get rid of all leading spaces
	 */
_skipsp:
	lbu	t0,(s0)
	beq	t0,zero,_cmd_invalid
	bne	t0,char_SP,_checkinput
	addu	s0,4			# bump to next char
	b	_skipsp

_checkinput:
	blt	t0,char_0,_is_cmd
	bgt	t0,char_9,_is_cmd

	/*
	 * it is a test number
	 */
	move	a0,s0			# convert it into binary
	jal	atoi_nv
	bne	v1,zero,_cmd_invalid	# bad number if v1!=0
	move	s1,v0			# save test number in s1

	/* 
	 * check for invalid test number
	 */
	li	a0,_TTBLPTR		# get current test table ptr
	jal	lw_nvram
	move	a0,v0
	jal	last_testno		# find out the last test's number
	bgt	s1,v0,_cmd_invalid	# check for invalid test number

	/*
	 * if the test is from the top level table (main test table)
	 * then call it here directly, otherwise call RunTests()
	 */
	and	t0, CnfgStatusReg, MLFLAG_MASK  # if ML bit not set, it is the main table
	bne	t0, zero, _realtest

	/* 
	 * Call the functions in the main test table directly
	 */
	subu	s1,1			# get the table offset
	mul	s1,TTBLENTRY_SIZE
	li	a0,_TTBLPTR		# get ptr to the function in the table
	jal	lw_nvram
	addu	s1,v0			# s1 is ptr to the function tbl entry
	lw	a0, OFFSET_TITLE(s1)	# update module name ptr
	li	a1, _MODNAME
	jal	sw_nvram
	lw	a0,OFFSET_PARM1(s1)	# parm1 to the function from the table
	lw	t0,OFFSET_TEST(s1)	# get the function addr
	jal	t0

	jal	print_menu
	b	_parser_exit
	
_realtest:
	li	a0,RUN_ONETEST		# set RunMode
	move	a1,s1			# test number
	jal	RunTests		# call RunTests() to execute tests

	/*
	 * end of execution
	 */
	jal	print_menu
	b	_parser_exit


_is_cmd:
	/*
	 * search the command in the command table
	 */
	li	a0,_CMDTBLPTR		# get current cmd table ptr
	jal	lw_nvram
	move	a0,v0			# pass cmd strptr
	move	a1,s0			# pass nvram buf ptr
	jal	lookup_cmdtbl
	beq	v0,zero,_cmd_invalid	# cmd invalid if null returned

	/*
	 * setup parameters to pass to the command routine
	 *
	 * v0 now points to the command entry in the table
	 */

	/* 
	 * advance s0 to the 1st parameter on command line 
	 */
1:
	lbu	t0,(s0)
	beq	t0, zero,2f		# end of line?
	addu	s0,4			# bump to next char
	bne	t0,char_SP,1b
2:
	move	a0,s0			# pass ptr to the rest of command line
	lw	a1,OFFSET_CMDPARM1(v0)	# pass parm1
	
	lw	t0,OFFSET_CMDPTR(v0)	# ptr to command routine
	move	v0,zero			# reset return value
	jal	t0			# execute command

	beq	v0,zero,_parser_exit	# Command Return Status: 0-Ok, 1-Invalid cmd syntax

	PRINT("\nSyntax error\n")
	j	_parser_exit

_cmd_invalid:
	PRINT("\r\nInvalid command\r\n")

_parser_exit:
	j	warm_reset
END(cmd_parser)

#define	CR_CHAR		0x0d
#define NL_CHAR		0x0a
#define	BS_CHAR		0x08
#define	SP_CHAR		0x20
#define char_CTRLP	0x10

/*
 * int _get_cmdstr(void)
 *
 * Get a command string from console. This routine currently implements 
 * these Unix lineedit like commands: 
 *	^P (one command deep)
 *
 * Retrun status: 
 *	0 - No command 
 *	otherwise number of char's entered
 *
 * Registers used:
 *      - t0  pointer to temporary nv_ram cmd buf	
 *	- t1  character counter. 
 *      - t2  pointer to nv_ram cmd buf	
 *      - t3  saved return address.                                
 *      - a0-a3, v0, v1  ared all used by _getchar() and _putchar().
 */
LEAF(_get_cmdstr)
	move	t3, ra			# save return address

	li	a0,_NVBUF		# convert byte offset of nvram buf into addr ptr
	jal	addr_nvram
	move	t2,v0			# t2 is the cmd buffer

	li	a0,_NVBUF2
	jal	addr_nvram
	move	t0, v0			# t0 is the temp buf ptr

	move	t1, zero		# zero char count

_get_nextchar:
	sb	zero, (t0)		# write end of line mark
	jal	_getchar		# read a character from console.
	and	v0, 0x7f

	beq	v0, CR_CHAR, _getcmd_done	# <return> ends input
	beq	v0, NL_CHAR, _getcmd_done	# new-line also ends input
	beq	v0, char_CTRLC, _getcmd_abort	# ctrl-C aborts 
	beq	v0, char_CTRLP, _editcmd_p	# do line-edit cmd p
	beq	v0, BS_CHAR, _getcmd_bs		# check for back space char.
	blt	v0, SP_CHAR, _get_nextchar	# unprintable char?
	bge	t1, _NVBUF_LEN, _getcmd_overflow # cmd string overflow?

	addi	t1, 1			# increment char count.
	sb	v0, (t0)		# write char to nvram cmd buffer
	addiu	t0, 4			# advance buffer offset.
	b	_get_nextchar

_getcmd_overflow:
	li	a0, BS_CHAR
	jal	_putchar		# wipe out character on screen.
	li	a0, SP_CHAR
	jal	_putchar
	li	a0, BS_CHAR
	jal	_putchar
	b	_get_nextchar

_getcmd_bs:
	beq	t1, zero, _get_nextchar	# if (count == 0) don't do backsp
	li	a0, BS_CHAR
	jal	_putchar		# wipe out character on screen.
	li	a0, SP_CHAR
	jal	_putchar
	li	a0, BS_CHAR
	jal	_putchar
	subu	t1, 1			# char-count -= 1
	subu	t0, 4			# backup buf ptr one char
	b	_get_nextchar

_editcmd_p:
	/*
	 * Clear the current cmd line
	 */
1:
	beq	t1, zero, 1f
	li	a0, char_BS
	jal	_putchar
	li	a0, SP_CHAR
	jal	_putchar
	li	a0, BS_CHAR
	jal	_putchar
	sub	t1, 1
	b	1b
1:
	/*
	 * Copy the previous cmd into temp cmd buf
	 */
	li	a0,_NVBUF2
	jal	addr_nvram
	move	t0, v0			# t0 is the temp buf ptr

	move	a0, t2
	move	a1, t0			# t0 is the temp buf ptr
	jal	strcp_nv
	move	t1, v0			# number of char's copied
1:
	lbu	a0, (t0)
	beq	a0, zero, _get_nextchar
	jal	_putchar
	addu	t0, 4
	b	1b

_getcmd_done:
	sb	zero, (t0)		# put NULL character at end of string.
	beq	t1, zero, 1f		# keep previous cmd unchanged if no cmd entered

	li	a0,_NVBUF2
	jal	addr_nvram
	move	a0, v0			# t0 is the temp buf ptr
	move	a1, t2
	jal	strcp_nv		# copy the cmd into save buffer (_NVBUF)

	# li	a0, NL_CHAR		# append new-line at end of cmd
	# jal	_putchar
1:
	move	v0, t1			# return number of char's
	j	t3			# return to caller.

_getcmd_abort:
	move	v0, zero		# no cmd entered
	j	t3
END(_get_cmdstr)

	
/*
 * int last_testno(struct test_tbl *tbl_ptr)
 *
 * return the test number of last test on a test table
 *
 * registers used: a0, a1 & v0
 */
LEAF(last_testno)
	move	v0,zero		# zero the count
1:
	lw	a1,(a0)		# look for the end of table
	beq	a1,zero,2f
	addu	a0,TTBLENTRY_SIZE	# point to next test
	addu	v0,1		# bump the count
	b	1b
2:
	j	ra
END(last_testno)


/*
 * int compstr(char *ptr1, int nvbuf_ptr2, int n)
 *
 * compare string 1 with string 2 stored in the nvram buffer. 
 * string 2 has to be at least n char long.
 * return 0 if no match, 1 if match
 *
 * register usage:
 *	a0 - cmd name ptr, a1 - nvram buf ptr, a2 - min. length of string 2 
 *
 * registers used: a0, a1, a2, a3, t0, t1, v0
 */
LEAF(compstr)
	move	v0,zero			# assume no match
	beq	a2,zero,_compstr_done	# return if min. length==0

	move	a3,zero			# counter for string 2 length
	lbu	t1,(a1)			# first char from nvram buffer string
1:
	lbu	t0,(a0)			# next char from string 1
	bne	t0,t1,_compstr_done

	addu	a0,1			# bump ptr
	addu	a1,4			# bump nvram buf ptr
	addu	a3,1			# count string 2 length
	lbu	t1,(a1)			# next char from nvram buffer string
	beq	t1,zero,2f		# end of string 2 ?
	bne	t1,char_SP,1b		# if not end of string 2, continue
2:
	blt	a3,a2,_compstr_done	# no match if string 2 not long enough
	li	v0,1			# match

_compstr_done:
	j	ra
END(compstr)


/*
 * struct cmd_tbl *lookup_cmdtbl(struct cmd_tbl *tblptr, int nvram_buf_ptr)
 *
 * Look and match a command in a command table.
 * Return the command entry ptr if found, null if not found.
 *
 * Register usage:
 *	t2 - table command string ptr
 *	t3 - nvram buffer ptr
 *
 * Registers used:
 *	a0, a1, a3, t0, t2-t4, v0
 *
 */
LEAF(lookup_cmdtbl)
	move	t4,ra			# save ra in t4

	move	t2,a0			# t2 is the table ptr
	move	t3,a1			# t3 is the nvram buf ptr

_nextcmd:
	lw	t0,(t2)			# look for the end of table
	beq	t0,zero,_cmdnotfound

	lw	a0,OFFSET_CMDSTR(t2)	# a0 is ptr to cmd string
	move	a1,t3			# a1 is ptr to string in nvram buf
	lw	a2,OFFSET_CMDLEN(t2)	# a2 is the min length of command
	jal	compstr			# compare if match

	bne	v0,zero,_cmdfound	# found if v0 != 0
	addu	t2,CTBLENTRY_SIZE	# bump to next command
	b	_nextcmd

_cmdnotfound:
	move	v0,zero			# return null if not found
	j	t4
	
_cmdfound:
	move	v0,t2			# return entry ptr if found
	j	t4
END(lookup_cmdtbl)


/* end */

