#ident	"$Id: print_menu.s,v 1.1 1994/07/21 00:18:49 davidl Exp $"
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
#include "pdiag.h"


   	.set	noreorder         # what you see is what you get

	.text


/*
 * Macro to print menu parameter
 *
 * Note: These two macros are only used in the following routine.
 *
 * registers used: a0, v0, t0 
 *
 * NOTE: _reg should NOT be either a0, v0 or t0
 */
#define PRINTPARMDEC(msg_label, _reg) \
	PUTS(msg_label)	\
	PUTUDEC(_reg) \
        la      a0, msg_label; \
        jal     strlen; \
	nop; \
        li      t0, COLUMN_WIDTH; \
        subu    t0, v0, ; \
        jal     ndigits; \
        move    a0, _reg; \
        jal     printsp; 	/* append spaces to fill up the column */ \
	subu	a0, t0, v0;

/* 
 * puthex() always prints hex values in 8 digits
 */
#define PRINTPARMHEX(msg_label,_reg) \
	PUTS(msg_label)	\
	PUTHEX(_reg) \
        la      a0,msg_label; \
        jal     strlen; \
	nop; \
        li      t0,COLUMN_WIDTH-8; \
        jal     printsp; \
	sub	a0,t0,v0;


/*
 * void print_menu(void)
 *
 * display test table (menu)
 *
 * register used:
 *	a0, s0, s1 (in macro)
 */

	.extern test_table

LEAF(print_menu)
	move	s7,ra		# save ra in s7

	PRINT("\r\n\n")
	PUTS(diag_version+4)		# some how 1st 4 bytes are garbage
	PUTS(_crlf)
	PUTS(_crlf)

	/*
 	 * Dispaly test control and status parameters
 	 *
 	 * register usage:
	 *	s0 - status value, s1 - cursor space counter
	 */
	and	s0, CnfgStatusReg, ERRMODE_MASK
	srl	s0, ERRMODE_SHIFTCNT
	PRINTPARMDEC(msg_em,s0) 	/* print "ERROR_MODE (em) = n" */

	PUTS(msg_el)			# print "ERRORLIMIT (el) = "
	li	a0, _ERRORLMT
	jal	lw_nvram		# get error-limit
	nop
	PUTUDEC(v0)
	PUTS(_crlf)

	li	a0,_ERRORCNT
	jal	lw_nvram
	nop
	move	s0,v0
	PRINTPARMDEC(msg_ec,s0) 	/* print "ERROR_COUNT (ec) = n" */

	PUTS(msg_pc) 			/* print "PASSCOUNT = " */
	li	a0,_PASSCNT
	jal	lw_nvram
	nop
	PUTUDEC(v0)
	PUTS(_crlf)
	
	li	a0, _FIRSTADDR		# display first_addr
	jal	lw_nvram
	nop
	move	s0, v0
	PRINTPARMHEX(msg_fa, s0)

	PUTS(msg_ll)			/* print "LOOP_LIMIT (ll) = " */
	li	a0,_LOOPLMT
	jal	lw_nvram
	nop
	PUTUDEC(v0)
	PUTS(_crlf)

	li	a0, _LASTADDR		# display last_addr 
	jal	lw_nvram
	nop
	move	s0, v0
	PRINTPARMHEX(msg_la, s0)

	PUTS(_crlf)
	PUTS(_crlf)

	mfc0	s0,C0_SR
	nop
	PRINTPARMHEX(msg_c0sr, s0) 	/* print "C0_STATUS = 0x" */

	PUTS(msg_configreg)		/* print "Config/Status = 0x" */
	PUTHEX(CnfgStatusReg)
	PUTS(_crlf)
	PUTS(_crlf)

	/*
	 * Print test menu title
	 */
	li	a0, _MODNAME
	jal	lw_nvram
	nop

	move	a0, v0
	jal	_puts
	nop
	PRINT(":\n")

	/*
 	 * Display tests in two columns
 	 */
	li	a0,_TTBLPTR		# get test table ptr
	jal	lw_nvram
	nop
	move	s0,v0			# s0 is table ptr
	li	s1,1			# s1 is test index

_print_test:
	lw	t0,(s0)			# is it end of the table?
	nop
	beq	t0,zero,_print_other
	nop

	PUTS(_crlf)			# start a new line

	PUTUDEC(s1)			# print the test number on the left column
	bge	s1,10,1f
	nop
	PRINT(".  ")
	b	2f
	nop
1:
	PRINT(". ")
2:
	lw	a0,OFFSET_TITLE(s0)	# get the test title ptr
	nop
	jal	_puts			# print the test title
	nop

	lw	a0,OFFSET_TITLE(s0)	# get the test title ptr
	nop
	jal	strlen			# count the string length (in v0)
	nop

	addu	s0,TTBLENTRY_SIZE	# point to next test entry
	addu	s1,1
	lw	t0,(s0)			# is it end of the table?
	nop
	beq	t0,zero,_print_other
	nop

	li	a0,COLUMN_WIDTH-4
	bge	v0,a0,_print_test	# don't use right column if no space left 
	nop
	subu	a0,v0			# append spaces to align the right column
	jal	printsp
	nop
	
	lw	a0,OFFSET_TITLE(s0)	# get the test title ptr
	jal	strlen			# count the string length
	nop
	li	a0,COLUMN_WIDTH-4
	bge	v0,a0,_print_test	# don't use right column if title too long 
	nop

	PUTUDEC(s1)			# print the test number on the right column
	bge	s1,10,1f
	nop
	PRINT(".  ")
	b	2f
	nop
1:
	PRINT(". ")
2:
	lw	a0,OFFSET_TITLE(s0)	# get the test title ptr
	jal	_puts			# print the test title
	nop

	addu	s0,TTBLENTRY_SIZE	# point to next test entry
	addu	s1,1
	b	_print_test
	nop

_print_other:
	# PRINT("\r\n\nR -Run all tests/ Q -Quit/ H -Help\r\n\n")
	PUTS(_crlf)

	j	s7
	move	v0,zero

	.set reorder

END(print_menu)

