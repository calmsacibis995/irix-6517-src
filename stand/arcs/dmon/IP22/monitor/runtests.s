#ident	"$Id: runtests.s,v 1.1 1994/07/21 00:19:00 davidl Exp $"
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


	.extern	_mark_start
	.extern _mark_end

	.globl	test_return	# all tests' return point
	.globl	_test_return	# an aliase of "test_return"

	.text

/*
 * int RunTests(int runmode, int test-index)
 *
 * Test execution driver. If a test is to be run from K0seg, then
 * the code is copied into cache first.
 *
 * Test Interface:
 *
 * Invocation syntax:	
 *	int	test_x(CnfgStatus, parm1)
 *
 *	where	CnfgStatus is a control register defined in monitor.h
 *		parm1 is the parameter from the test table.
 *
 * Return statue:
 *	0 - pass, otherwise fail
 *
 * registers used:
 *	a0 - a2
 *	v0 - v1
 *	s0 - s4
 *	t0 - t1
 *	CnfgStatusReg
 *
 */

LEAF(RunTests)
	move	s0, a0			# save a0 in s0
	move	s1, a1			# save a1 in s1

	move	a0, ra			# push ra onto nvram stack
	jal	push_nvram

	move	a0, s0
	li	a1, _RUNMODE		# save runmode 
	jal	sb_nvram

	beq	s0,RUN_ONETEST,1f

	or	s1,zero,1		# for run all, start at test 1

	bne	s0,RUN_ALLMODS,1f
	li	a0, 1			# for run all modules, start at module 1
	li	a1, _MODINDX
	jal	sb_nvram
	PRINT("\n\n>>>> Run All Test Modules <<<<")
1:
	move	a0, s1
	li	a1, _TESTINDX
	jal	sb_nvram 		# save test index

	move	a0, zero
	li	a1, _ERRORCNT
	jal	sw_nvram		# zero error count

	or	a0,zero,1		# set passcount to 1
	li	a1, _PASSCNT
	jal	sw_nvram

#ifdef DEBUG1
	PRINT("\nTest Number=")
	li	a0, _TESTINDX		# get next test index in s0
	jal	lb_nvram
	move	a0, v0
	move	a1, zero
	jal	putudec
	la	a0, _crlf
	jal	puts
#endif DEBUG1
	
_next_test:
	/*
	 * register usage:
	 *	s0 - current test number
	 *	s1 - ptr to test entry 
	 *	s2 - ptr to test module in main table (run all mods only)
	 *	s3 - run mode
	 *	temp used: a0, a1, t0 & t1
	 */

	/*
	 * get current table ptr
	 */
	li	a0,_TTBLPTR		# get test table ptr
	jal	lw_nvram
	move	s1,v0			# s1 is ptr to current test table

	li	a0, _RUNMODE		# check run mode
	jal	lb_nvram
	move	s3, v0
	bne	v0, RUN_ALLMODS, 1f
	
	li	a0, _MODINDX		# get table in execution
	jal	lb_nvram
	subu	v0, 1			# get table index offset
	mul	v0, TTBLENTRY_SIZE	# get entry offset
	addu	s2, s1, v0		# s2 = ptr to test module in main table

	lw	t0, (s2)		# is it the end of table?
	beq	t0, zero, _test_unknown

	lw	s1, OFFSET_PARM1(s2)	# s1 = ptr to test table in execution 
1:
	
	/* 
	 * get the table entry ptr
	 */	
	li	a0, _TESTINDX		# get next test index in s0
	jal	lb_nvram
	and	s0, v0, 0xff

	subu	t0, s0, 1
	mul	t0, TTBLENTRY_SIZE	# get entry offset
	addu	s1, t0			# s1 = ptr to test entry in table

	lw	a0,(s1)			# is it the end of table?
	beq	a0,zero,_test_unknown

	PUTS(_crlf)
	PUTS(_crlf)

	bne	s3, RUN_ALLMODS, 1f
	lw	a0, OFFSET_TITLE(s2)	# get test module name if RUN_ALLMODS
	j	2f
1:
	li	a0, _MODNAME		# print current module name
	jal	lw_nvram
	move	a0, v0
2:
	jal	puts			# print module name

   	# PRINT("\r\n\nTest ")       	# print "Test X: Lap Y: "
	li	a0, char_SP
	jal	putchar

   	PUTUDEC(s0)

	PRINT(": Lap ")
	li	a0, _PASSCNT
	jal	lw_nvram 		# get pass number
	PUTUDEC(v0)
	PRINT(": ")

	lw	a0, OFFSET_TITLE(s1)
	jal	puts			# print test title

	/*
	 * now dispatch the test into specified addr space: kseg0 or kseg1
	 */
	lw	s0,OFFSET_TEST(s1)	# get test addr in s0
	or	s0, K1BASE		# default to kseg1 addr space 

	lw	t0,OFFSET_ICACHED(s1)	# get test's icached flag
	bne	t0, CACHED, _test_go	# run test in kseg0 space? 

	PRINT(" (I Cached)")

	sll	s0, 3			# clear 3 msb
	srl	s0, 3
	or	s0, K0BASE		# mask to kseg0 space

	lw	a0, OFFSET_TEST(s1)	# get test start addr
	lw	a1, OFFSET_CODESZ(s1)	# get test code size
	bne	a1, zero, 1f		# if zero, use default size
	li	a1, TESTSZ_MAX
1:
	addu	a1, a0, a1		# get test last addr

_test_go:
	li	a0, char_SP
	jal	putchar
	jal	putchar
	li	a0, 0x3C		# print '<'
	jal	putchar
	jal	date			# print starting time
	li	a0, 0x3E		# print '>'
	jal	putchar
	PUTS(_crlf)

	/*
	 * get 1st and last addresses in s2 & s3
	 */
	li	a0, _FIRSTADDR
	jal	lw_nvram
	move	s2, v0			# first address
	li	a0, _LASTADDR
	jal	lw_nvram
	move	s3, v0			# last address

	/* 
	 * Set TA (test active) bit & reset ERR_FLAG in CnfgStatusReg
	 */
	and	CnfgStatusReg,~ERRFLAG_MASK	# clear error bit
	or	CnfgStatusReg,TAFLAG_MASK 	# set test-active bit 

	move	a0, CnfgStatusReg		# save it in nvram
	li	a1,_CNFGSTATUS
	jal	sw_nvram
	
	/* 
 	 * setup test parameters
 	 */
	move	a0, CnfgStatusReg	# 1st argument
	lw	a1, OFFSET_PARM1(s1)	# 2nd argument
	move	a2, s2			# 3rd argument: first address
	move	a3, s3			# 4th argument: last address

	move	v0, zero
	jal	s0			# start the test

/*
 * Every test must jump to here at exit OR after unexpected exceptions
 *
 * At this point, v0 has the test return status: 0 - passed, >0 - failed
 *
 * register usage:
 *	s0 - current test number
 *	s1 - pass count
 *	s2 - number of errors
 *	s3 - total errors
 */

test_return:
_test_return:
	move	s2, v0			# test status returned in v0

	li	a0, _CNFGSTATUS		# get CnfgStatusReg from nvram
	jal	lw_nvram		#   since it might be trashed by the test
	and	CnfgStatusReg, v0, ~TAFLAG_MASK # reset TA bit

	move	a0, CnfgStatusReg
	li	a1, _CNFGSTATUS
	jal	sw_nvram		# save it in nvram

   	PRINT("\r\nTest ")       	# print "Test X: Pass Y "

	li	a0, _TESTINDX		# get current test index
	jal	lb_nvram
	move	s0, v0
   	PUTUDEC(s0)

	PRINT(": Pass ")
	li	a0, _PASSCNT		# get pass count
	jal	lw_nvram
	move	s1, v0
	PUTUDEC(s1)

	bne	s2,zero,_test_failed

	PUTS(pass_msg)			# print "...PASSED"
	j	1f

_test_failed:
	PUTS(fail_msg)			# print "...FAILED"
1:
	# PRINT("Errors = ")
	# PUTUDEC(s2)

	# PRINT("  Total Errors= ")
	la	a0, msg_totalerr	# print total errors
	jal	puts

	li	a0, _ERRORCNT
	jal	lw_nvram 		# get previous error count
	addu	s3, s2, v0		# update total error count
	PUTUDEC(s3)

	move	a0, s3
	li	a1, _ERRORCNT
	jal	sw_nvram		# save total error count

	li	a0, char_SP
	jal	putchar
	jal	putchar			# print "  "
	li	a0, 0x3C		# print '<'
	jal	putchar
	jal	date			# print ending time
	li	a0, 0x3E		# print '>'
	jal	putchar
	PUTS(_crlf)

	/* 
 	 * decide what to do next
	 *
	 * At this point:
	 * s0 - test index
	 * s1 - current pass count
	 * s2 - number of errors in the last test
	 * s3 - total error count
	 * s4 - current run mode
	 */

	li	a0, _RUNMODE		# get run mode
	jal	lb_nvram
	move	s4, v0

	/*
	 * 1. Check if total error >= error limit
	 */
	li	a0, _ERRORLMT		# get error limit
	jal	lw_nvram

	beq	v0,zero,_ignore_errcnt	# ignore error count if errlimit == 0
	bgeu	s3,v0,_runtests_abort	# done if total succeeds error limit

_ignore_errcnt:	
	beq	s2,zero,_no_error	# the test failed then check on_error mode

	/*
	 * 2. Check on_error mode 
	 */
	and	t0, CnfgStatusReg, ERRMODE_MASK
	beq	t0, ERR_EXIT, _runtests_abort	# exit on error
	beq	t0, ERR_LOOP,_next_test	# loop on error (NOTE: passcnt not incremented)

_no_error:	
	beq	s4, RUN_ONETEST, _incr_loopcnt

	/*
 	 * Run all-tests mode or Run all-modules mode
	 *
	 * Register usage: 
	 *
	 *	s2 - current test table ptr
	 *	s3 - ptr to module entry in main table
	 */
	addu	s0, 1			# incr. the test index
	move	a0, s0
	li	a1, _TESTINDX
	jal	sb_nvram 		# save the test index

	/* 
	 * check for the end of test table
	 */
	li	a0,_TTBLPTR		# get test table ptr
	jal	lw_nvram
	move	s2, v0

	beq	s4, RUN_ALLTESTS, 1f

	li	a0, _MODINDX		# get table in execution for run_allmods
	jal	lb_nvram
	subu	v0, 1			# get table index offset
	mul	v0, TTBLENTRY_SIZE	# get entry offset
	addu	s3, s2, v0		# s3 = ptr to test module in main table 
	lw	s2, OFFSET_PARM1(s3)	# s1 = ptr to test table 
1:
	subu	t0, s0,1
	mul	t0, TTBLENTRY_SIZE	# get test entry offset
	addu	t0, s2			# ptr to the test entry
	lw	a0, (t0)		# is it the end of table?
	bne	a0, zero, _next_test	# next test if not end of table
	
	li	a0, 1			# reset test index for next pass
	li	a1, _TESTINDX
	jal	sb_nvram

	bne	s4, RUN_ALLMODS, _incr_loopcnt
	
	li	a0, _MODINDX		# bump module index if RUN_ALLMODS
	jal	lb_nvram
	add	a0, v0, 1
	li	a1, _MODINDX
	jal	sb_nvram		# store it back

	/*
	 * check for the end of main test table (run_allmods)
	 */
	lw	a0, TTBLENTRY_SIZE(s3)	# next module entry in main table
	bne	a0, zero, _next_test

	li	a0, 1			# reset test module index
	li	a1, _MODINDX
	jal	sb_nvram

	/*
	 * 3. Check if pass-count > loop-limit
	 */
_incr_loopcnt:
	addu	s1, 1			# incr. the passcount
	li	a0, _LOOPLMT		# get loop limit
	jal	lw_nvram

	beq	v0, zero, 1f		# loop forever if looplmt == 0
	bgtu	s1,v0,_runtests_done	# done if passcnt >= looplmt
1:
	move	a0, s1			# save new passcnt
	li	a1, _PASSCNT
	jal	sw_nvram

	j	_next_test

_runtests_abort:
	la	s0, msg_abort
	j	1f

_runtests_done:
	la	s0, msg_cmplt
1:
	la	a0, _crlf
	jal	puts
	li	a0, _MODNAME		# print current module name
	jal	lw_nvram
	move	a0, v0
	jal	puts
	move	a0, s0			# print reason of termination
	jal	puts
	li	a0, _PASSCNT		# print pass count
	jal	lw_nvram
	PUTUDEC(v0)
	la	a0, msg_totalerr
	jal	puts
	li	a0, _ERRORCNT
	jal	lw_nvram 		# get previous error count
	PUTUDEC(v0)
	jal	_pause			# pause after test 
	j	1f

_test_unknown:
	PRINT("Unknown test: ")
	PUTUDEC(s0)
	PUTS(_crlf)
1:
	jal	pop_nvram
	j	v0	
END(RunTests)

	.data
msg_cmplt:	.asciiz	" - COMPLETED. Laps = "
msg_abort:	.asciiz " - ABORTED by error. Laps = "
msg_totalerr:	.asciiz " Total Errors = " 

