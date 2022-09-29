/*****************************************************************************
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 ****************************************************************************/

#include <asm.h>
#include <regdef.h>
#include <sys/fpu.h>

#include "../../src/autoheader/uassym.h"
#if (_MIPS_SIM != _MIPS_SIM_ABI64)
<<< BOMB >>> Above include makes this test only compile for 64 bit case.
#endif

#if (_MIPS_SIM == _MIPS_SIM_ABI64)
#define REGSIZE 0x8
#elif ((_MIPS_SIM == _MIPS_SIM_ABIN32) || (_MIPS_SIM == _MIPS_SIM_ABIO32))
#define REGSIZE 0x4
#endif

#define TRIPCOUNT 1024 * 1024

.extern failtest

 #
 # void single_context(kusharena_t *kusp)
 #
	.set noat
	.set noreorder
NESTED(single_context, 0x90, ra)

 # Initialize all non-essential registers to make sure that they all are
 # "live" and have unique values.

	.mask	0x0a00ff81, 0x10
	PTR_ADDIU	sp, -0x90
	REG_S	ra, 0x80(sp)
	REG_S	gp, 0x80-REGSIZE(sp)
	REG_S	s7, 0x80-REGSIZE*2(sp)
	REG_S	s6, 0x80-REGSIZE*3(sp)
	REG_S	s5, 0x80-REGSIZE*4(sp)
	REG_S	s4, 0x80-REGSIZE*5(sp)
	REG_S	s3, 0x80-REGSIZE*6(sp)
	REG_S	s2, 0x80-REGSIZE*7(sp)
	REG_S	s1, 0x80-REGSIZE*8(sp)
	REG_S	s0, 0x80-REGSIZE*9(sp)
	REG_S	a0, 0x80-REGSIZE*10(sp)
	INT_L	a0, PRDA + PRDA_NID
	INT_S	a0, 0x80-REGSIZE*11(sp)

	REG_S	zero, 0x8(sp)			# yield successful count
	li	a1, TRIPCOUNT
	REG_S	a1, 0x10(sp)			# iteration count

#define NIDOFST 0x100

starttest:

#define INIT(register) daddi $register, a0, register * NIDOFST
	INIT(1); INIT(2); INIT(3)
 # keep nid in a0, $4
	INIT(5); INIT(6); INIT(7); INIT(8); INIT(9); INIT(10); INIT(11); INIT(12)
	INIT(13); INIT(14); INIT(15); INIT(16); INIT(17); INIT(18); INIT(19)
	INIT(20); INIT(21); INIT(22); INIT(23); INIT(24); INIT(25)
 # k0, $26 is not a user register
 # k1, $27 is not a user register
	INIT(28)
 # sp, $29 is necessary to retrieve data from the stack
	INIT(30)

#define FINIT(register) \
	addi $31, a0, ((register + 32) * NIDOFST); \
	mtc1 $31, $f/**/register

	FINIT(0); FINIT(1); FINIT(2); FINIT(3); FINIT(4); FINIT(5); FINIT(6)
	FINIT(7); FINIT(8); FINIT(9); FINIT(10); FINIT(11); FINIT(12); FINIT(13)
	FINIT(14); FINIT(15); FINIT(16); FINIT(17); FINIT(18); FINIT(19)
	FINIT(20); FINIT(21); FINIT(22); FINIT(23); FINIT(24); FINIT(25)
	FINIT(26); FINIT(27); FINIT(28); FINIT(29); FINIT(30); FINIT(31)

starttest1:
	
 # assert(nid = PRDA + PRDA_NID)
	INT_L	$31, PRDA+PRDA_NID
	bne	$31, a0, fail_test_nid
	nada
	beq	a0, zero, fail_test_nid
	nada
 # assert(rltN = N * 0x800 + nid)

#define TESTREG(register) \
	daddi	$31, $register, register * -NIDOFST; \
	bne	$31, a0, fail_test_r/**/register; \
	nada

	TESTREG(1); TESTREG(2); TESTREG(3); TESTREG(5); TESTREG(6); TESTREG(7)
	TESTREG(8); TESTREG(9); TESTREG(10); TESTREG(11); TESTREG(12)
	TESTREG(13); TESTREG(14); TESTREG(15); TESTREG(16); TESTREG(17)
	TESTREG(18); TESTREG(19); TESTREG(20); TESTREG(21); TESTREG(22)
	TESTREG(23); TESTREG(24); TESTREG(25); TESTREG(28)
 # need a test to assure sp is not corrupt
	TESTREG(30)

#define TESTFPREG(register) \
	mfc1	$31, $f/**/register; \
	nada; \
	addi	$31, $31, -((register + 32) * NIDOFST); \
	bne	$31, a0, fail_test_fp/**/register; \
	nada
	
	TESTFPREG(0); TESTFPREG(1); TESTFPREG(2); TESTFPREG(3); TESTFPREG(4)
	TESTFPREG(5); TESTFPREG(6); TESTFPREG(7); TESTFPREG(8); TESTFPREG(9)
	TESTFPREG(10); TESTFPREG(11); TESTFPREG(12); TESTFPREG(13)
	TESTFPREG(14); TESTFPREG(15); TESTFPREG(16); TESTFPREG(17)
	TESTFPREG(18); TESTFPREG(19); TESTFPREG(20); TESTFPREG(21)
	TESTFPREG(22); TESTFPREG(23); TESTFPREG(24); TESTFPREG(25)
	TESTFPREG(26); TESTFPREG(27); TESTFPREG(28); TESTFPREG(29)
	TESTFPREG(30); TESTFPREG(31)

	REG_L	$31, 0x80-REGSIZE*10(sp) # kusp
	REG_L	$31, KUS_RBITS(sp)
	dsll	$31, a0
	and	$31, 1
	bnez	$31, fail_test_rbits

	REG_L	$31, 0x10(sp)
	daddi	$31, -1
	bne	$31, zero, starttest1
	REG_S	$31, 0x10(sp)
	li	$31, TRIPCOUNT
	REG_S	$31, 0x10(sp)

#define INI2(register) daddi $register, a0, (register * NIDOFST) + 1
	INI2(1); INI2(2); INI2(3)
 # keep nid in a0, $4
	INI2(5); INI2(6); INI2(7); INI2(8); INI2(9); INI2(10); INI2(11); INI2(12)
	INI2(13); INI2(14); INI2(15); INI2(16); INI2(17); INI2(18); INI2(19)
	INI2(20); INI2(21); INI2(22); INI2(23); INI2(24); INI2(25)
 # k0, $26 is not a user register
 # k1, $27 is not a user register
	INI2(28)
 # sp, $29 is necessary to retrieve data from the stack
	INI2(30)

#define FINI2(register) \
	addi $31, a0, ((register + 32) * NIDOFST) + 1; \
	mtc1 $31, $f/**/register

	FINI2(0); FINI2(1); FINI2(2); FINI2(3); FINI2(4); FINI2(5); FINI2(6)
	FINI2(7); FINI2(8); FINI2(9); FINI2(10); FINI2(11); FINI2(12); FINI2(13)
	FINI2(14); FINI2(15); FINI2(16); FINI2(17); FINI2(18); FINI2(19)
	FINI2(20); FINI2(21); FINI2(22); FINI2(23); FINI2(24); FINI2(25)
	FINI2(26); FINI2(27); FINI2(28); FINI2(29); FINI2(30); FINI2(31)

starttest2:
	
 # assert(nid = PRDA + PRDA_NID)
	INT_L	$31, PRDA+PRDA_NID
	bne	$31, a0, fail_test_nid
	nada
	beq	a0, zero, fail_test_nid
	nada
 # assert(rltN = N * 0x800 + nid)

#define TESTRE2(register) \
	addi	$31, $register, -(register * NIDOFST) - 1; \
	bne	$31, a0, fail_test_r/**/register; \
	nada

	TESTRE2(1); TESTRE2(2); TESTRE2(3); TESTRE2(5); TESTRE2(6); TESTRE2(7)
	TESTRE2(8); TESTRE2(9); TESTRE2(10); TESTRE2(11); TESTRE2(12)
	TESTRE2(13); TESTRE2(14); TESTRE2(15); TESTRE2(16); TESTRE2(17)
	TESTRE2(18); TESTRE2(19); TESTRE2(20); TESTRE2(21); TESTRE2(22)
	TESTRE2(23); TESTRE2(24); TESTRE2(25); TESTRE2(28)
 # need a test to assure sp is not corrupt
	TESTRE2(30)

#define TESTFPRE2(register) \
	mfc1	$31, $f/**/register; \
	nada; \
	addi	$31, $31, -(((register + 32) * NIDOFST) + 1); \
	bne	$31, a0, fail_test_fp/**/register; \
	nada
	
	TESTFPRE2(0); TESTFPRE2(1); TESTFPRE2(2); TESTFPRE2(3); TESTFPRE2(4)
	TESTFPRE2(5); TESTFPRE2(6); TESTFPRE2(7); TESTFPRE2(8); TESTFPRE2(9)
	TESTFPRE2(10); TESTFPRE2(11); TESTFPRE2(12); TESTFPRE2(13)
	TESTFPRE2(14); TESTFPRE2(15); TESTFPRE2(16); TESTFPRE2(17)
	TESTFPRE2(18); TESTFPRE2(19); TESTFPRE2(20); TESTFPRE2(21)
	TESTFPRE2(22); TESTFPRE2(23); TESTFPRE2(24); TESTFPRE2(25)
	TESTFPRE2(26); TESTFPRE2(27); TESTFPRE2(28); TESTFPRE2(29)
	TESTFPRE2(30); TESTFPRE2(31)

	REG_L	$31, 0x10(sp)
	daddi	$31, -1
	bne	$31, zero, starttest2
	REG_S	$31, 0x10(sp)
	li	$31, TRIPCOUNT
	REG_S	$31, 0x10(sp)

	j	starttest
	nada

fail_test_rbits:
	j	call_failtest
	li	a2, -2

fail_test_nid:
	move	a1, $31
	j	call_failtest
	li	a2, -1

#define FAIL_TEST_HNDL(register) \
fail_test_r/**/register: \
	move a1, $31; \
	REG_L a3, 0x10(sp); \
	j call_failtest; \
	li a2, register

	FAIL_TEST_HNDL(1)
	FAIL_TEST_HNDL(2)
	FAIL_TEST_HNDL(3)
	FAIL_TEST_HNDL(4)
	FAIL_TEST_HNDL(5)
	FAIL_TEST_HNDL(6)
	FAIL_TEST_HNDL(7)
	FAIL_TEST_HNDL(8)
	FAIL_TEST_HNDL(9)
	FAIL_TEST_HNDL(10)
	FAIL_TEST_HNDL(11)
	FAIL_TEST_HNDL(12)
	FAIL_TEST_HNDL(13)
	FAIL_TEST_HNDL(14)
	FAIL_TEST_HNDL(15)
	FAIL_TEST_HNDL(16)
	FAIL_TEST_HNDL(17)
	FAIL_TEST_HNDL(18)
	FAIL_TEST_HNDL(19)
	FAIL_TEST_HNDL(20)
	FAIL_TEST_HNDL(21)
	FAIL_TEST_HNDL(22)
	FAIL_TEST_HNDL(23)
	FAIL_TEST_HNDL(24)
	FAIL_TEST_HNDL(25)
	FAIL_TEST_HNDL(26)
	FAIL_TEST_HNDL(27)
	FAIL_TEST_HNDL(28)
	FAIL_TEST_HNDL(29)
	FAIL_TEST_HNDL(30)
	FAIL_TEST_HNDL(31)

#define FAIL_TEST_FPHNDL(register) \
fail_test_fp/**/register: \
	move a1, $31; \
	REG_L a3, 0x10(sp); \
	j call_failtest; \
	li a2, register + 32

	FAIL_TEST_FPHNDL(0)
	FAIL_TEST_FPHNDL(1)
	FAIL_TEST_FPHNDL(2)
	FAIL_TEST_FPHNDL(3)
	FAIL_TEST_FPHNDL(4)
	FAIL_TEST_FPHNDL(5)
	FAIL_TEST_FPHNDL(6)
	FAIL_TEST_FPHNDL(7)
	FAIL_TEST_FPHNDL(8)
	FAIL_TEST_FPHNDL(9)
	FAIL_TEST_FPHNDL(10)
	FAIL_TEST_FPHNDL(11)
	FAIL_TEST_FPHNDL(12)
	FAIL_TEST_FPHNDL(13)
	FAIL_TEST_FPHNDL(14)
	FAIL_TEST_FPHNDL(15)
	FAIL_TEST_FPHNDL(16)
	FAIL_TEST_FPHNDL(17)
	FAIL_TEST_FPHNDL(18)
	FAIL_TEST_FPHNDL(19)
	FAIL_TEST_FPHNDL(20)
	FAIL_TEST_FPHNDL(21)
	FAIL_TEST_FPHNDL(22)
	FAIL_TEST_FPHNDL(23)
	FAIL_TEST_FPHNDL(24)
	FAIL_TEST_FPHNDL(25)
	FAIL_TEST_FPHNDL(26)
	FAIL_TEST_FPHNDL(27)
	FAIL_TEST_FPHNDL(28)
	FAIL_TEST_FPHNDL(29)
	FAIL_TEST_FPHNDL(30)
	FAIL_TEST_FPHNDL(31)

call_failtest:
	REG_L	gp, 0x80-REGSIZE(sp)
	REG_L	jp, %call16(failtest)(gp)
	jr	jp
	nada
END(single_context)	
