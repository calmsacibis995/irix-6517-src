#ident	"$Id: cache_err.s,v 1.1 1994/07/21 01:24:10 davidl Exp $"
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
#include "cache_err.h"

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
LEAF(cache_err)
	move	t2, a0			# saved failing address.
	move	t3, a1			# saved expected value.
	move	t4, a2			# saved actual value.
	move	t5, a3			# error code


	/*----------------------------------------------------------------+
	| Display error messages.                                         |
	+----------------------------------------------------------------*/
disp_err:
	la	t7, CacheErrTbl
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

	bge	t5,DCACHE_SIZE_ERR,3f

	PRINT("\n  Failing address: 0x")
	PUTHEX(t2)

	PRINT("\n  Expected value : 0x")
	PUTHEX(t3)

	PRINT("\n  Actual   value : 0x")
	PUTHEX(t4)

	PRINT("\n  Failing  bit   : 0x")
	xor	a0, t3, t4
	jal	puthex

3:
	# 
	# The following will be used later
	#
	# lw	v0, ELOOP_OFFSET(t7)
	# j	v0

	li	v0, 1			# set error return flag
	j	_test_return		# return to RunTests()
END(cache_err)



/*------------------------------------------------------------------------+
| Error decode table.                                                     |
+------------------------------------------------------------------------*/
	.data
CacheErrTbl:
	.word	DCACHE_ERR;	.word	DCacheErrMsg;	.word	simple_err
	.word	ICACHE_ERR;	.word	ICacheErrMsg;	.word	simple_err
	.word	DTAG_ERR;	.word	DTagErrMsg;	.word	simple_err
	.word	ITAG_ERR;	.word	ITagErrMsg;	.word	simple_err
	.word	DTAG_PAR_ERR;	.word	DTagParErrMsg;	.word	simple_err
	.word	AINA_ERR;	.word	CacheAdrMsg;	.word	addr_err
	.word	FILL_ERR;	.word	FillErrMsg;	.word	line_fill_err
	.word	WBACK_ERR;	.word	WBackErrMsg;	.word	line_flush_err
	.word	K0MOD_ERR;	.word	K0ModErrMsg;	.word	k0_mod_err
	.word	K1MOD_ERR;	.word	K1ModErrMsg;	.word	k1_mod_err
	.word	DCACHE_SIZE_ERR;.word	DcacheSizeMsg;	.word	dummy_err
	.word	ICACHE_SIZE_ERR;.word	IcacheSizeMsg;	.word	dummy_err
	.word	0;		.word	0;		.word	0

	.data
DCacheErrMsg:	.asciiz "\n\n### Error - Data cache fault"
ICacheErrMsg:	.asciiz "\n\n### Error - Instruction cache fault"
DTagErrMsg:	.asciiz	"\n\n### Error - Data tag error"
ITagErrMsg:	.asciiz	"\n\n### Error - Instruction tag error"
DTagParErrMsg:	.asciiz	"\n\n### Error - Data tag parity error"
CacheAdrMsg:	.asciiz "\n\n### Error - Cache addressing fault"
FillErrMsg:	.asciiz	"\n\n### Error - Bad data in cache after line-fill"
WBackErrMsg:	.asciiz	"\n\n### Error - Bad data in memory after write back"
K0ModErrMsg:	.asciiz	"\n\n### Error - Write to Kseg1 changed cache data"
K1ModErrMsg:	.asciiz	"\n\n### Error - Write to cache changed memory image"
DcacheSizeMsg:	.asciiz	"\n\n### Error - Data cache size error"
IcacheSizeMsg:	.asciiz	"\n\n### Error - Instruction cache size error"
DtagErrMsg:	.asciiz	"\n\n### Error - Data tag fault"



/*========================================================================+
|                                                                         |
|                   E  R  R  O  R      L  O  O  P                         |
|                                                                         |
+========================================================================*/
	.text

/*------------------------------------------------------------------------+
| Error loop.                                                             |
+------------------------------------------------------------------------*/
LEAF(simple_err)
	not	a0, t3
1:
	sw	a0, (t2)
	sw	t3, (t2)		# write expected value to failing addr
	lw	t4, (t2)		# read it back.
	not	a0, t3
	b	1b			# loop forever.
END(simple_err)


/*------------------------------------------------------------------------+
|                                                                         |
+------------------------------------------------------------------------*/
LEAF(addr_err)
	nop
1:
	b	1b
END(addr_err)



/*------------------------------------------------------------------------+
|                                                                         |
+------------------------------------------------------------------------*/
LEAF(line_fill_err)
1:
	b	1b
END(line_fill_err)


/*------------------------------------------------------------------------+
|                                                                         |
+------------------------------------------------------------------------*/
LEAF(line_flush_err)
1:
	b	1b
END(line_flush_err)


/*------------------------------------------------------------------------+
|                                                                         |
+------------------------------------------------------------------------*/
LEAF(k0_mod_err)
1:
	b	1b
END(k0_mod_err)


/*------------------------------------------------------------------------+
|                                                                         |
+------------------------------------------------------------------------*/
LEAF(k1_mod_err)
1:
	b	1b
END(k1_mod_err)


/*------------------------------------------------------------------------+
|                                                                         |
+------------------------------------------------------------------------*/
LEAF(ecc_array_err)
1:
	b	1b
END(ecc_array_err)


/*------------------------------------------------------------------------+
|                                                                         |
+------------------------------------------------------------------------*/
LEAF(ecc_gen_err)
1:
	b	1b
END(ecc_gen_err)


/*------------------------------------------------------------------------+
|                                                                         |
+------------------------------------------------------------------------*/
LEAF(dummy_err)
	j	ra
END(dummy_err)


