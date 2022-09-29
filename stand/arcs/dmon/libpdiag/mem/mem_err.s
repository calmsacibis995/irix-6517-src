#ident	"$Id: mem_err.s,v 1.1 1994/07/21 00:58:36 davidl Exp $"
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

#include "pdiag.h"
#include "monitor.h"
#include "mem_err.h"

#define ECODE_OFFSET		0
#define EMSG_OFFSET		4
#define ELOOP_OFFSET		8
#define ETBL_ENTRY_SZ		12

/*------------------------------------------------------------------------+
| Routine name: int cache_err(address, expected, actual, err-code)	  |
|									  |
| Description : display error messages.                                   |
+------------------------------------------------------------------------*/
	.text
LEAF(mem_err)
	move	t2, a0			# saved failing address.
	move	t3, a1			# saved expected value.
	move	t4, a2			# saved actual value.
	move	t5, a3			# error code


	/*----------------------------------------------------------------+
	| Display error messages.                                         |
	+----------------------------------------------------------------*/
disp_err:
	la	t7, MemErrTbl
1:
	lw	t1, ECODE_OFFSET(t7)	# read error code in table.
	lw	a0, EMSG_OFFSET(t7)	# get pointer to error message.
	beq	t1, t5, 2f

	addiu	t7, ETBL_ENTRY_SZ	# index to next entry in err table.
	bne	t1, zero, 1b

	subu	t7, ETBL_ENTRY_SZ	# back up to previous entry if we
					# went pass the table.
2:
	jal	_puts			# print error message.

	PRINT("\n  Failing address: 0x")
	PUTHEX(t2)

	PRINT("\n  Expected value : 0x")
	PUTHEX(t3)

	PRINT("\n  Actual   value : 0x")
	PUTHEX(t4)

	PRINT("\n  Failing  bit   : 0x")
	xor	a0, t3, t4
	jal	puthex
	PRINT("\r\n")

	move	a0, t2
	xor	a1, t3, t4
	jal	badsimm

3:
	# 
	# The following will be used later
	#
	# lw	v0, ELOOP_OFFSET(t7)
	# j	v0

	li	v0, 1			# set error return flag
	j	_test_return		# return to RunTests()
END(mem_err)



/*------------------------------------------------------------------------+
| Error decode table.                                                     |
+------------------------------------------------------------------------*/
	.data
MemErrTbl:
	.word	MEM_ERR;	.word	MemErrMsg;	.word	mem_simple_err
	.word	MEM_AINA_ERR;	.word	MemAinaErrMsg;	.word	mem_addr_err
	.word	0;		.word	0;		.word	0

	.data
MemErrMsg:	.asciiz "\n\n### Error - Memory data fault"
MemAinaErrMsg:	.asciiz "\n\n### Error - Memory address fault"


/*========================================================================+
|                                                                         |
|                   E  R  R  O  R      L  O  O  P                         |
|                                                                         |
+========================================================================*/
	.text

/*------------------------------------------------------------------------+
| Error loop.                                                             |
+------------------------------------------------------------------------*/
LEAF(mem_simple_err)
	not	a0, t3
1:
	sw	a0, (t2)
	sw	t3, (t2)		# write expected value to failing addr
	lw	t4, (t2)		# read it back.
	not	a0, t3
	b	1b			# loop forever.
END(mem_simple_err)


/*------------------------------------------------------------------------+
|                                                                         |
+------------------------------------------------------------------------*/
LEAF(mem_addr_err)
	nop
1:
	b	1b
END(mem_addr_err)

