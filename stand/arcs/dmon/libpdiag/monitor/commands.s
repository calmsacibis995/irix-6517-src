#ident	"$Id: commands.s,v 1.1 1994/07/21 01:13:08 davidl Exp $"
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


/*
 * Monitor Commands 
 *
 * Command Interface:
 *
 * Invocation syntax
 *	int cmd_x(char *env_nvptr, int parm1)
 *	where parm1 is from the command table
 *
 * Return status
 *	0 - OK, 1 - Invalid command syntax or Error
 */

#include "sys/regdef.h"
#include "sys/asm.h"
#include "sys/sbd.h"

#include "pdiag.h"
#include "monitor.h"

/*
 * ISOLATECACHE() - If the isolate-cache flag (cachopt) is non-zero, 
 * isolate the cache prior to a write/read from memory. Save the status 
 * register for later restoral. This macro must be preceded by 
 * ".set noreorder".
 * 
 */
#define ISOLATECACHE(savsr_reg, srdata, cachopt) \
	mfc0	savsr_reg, C0_SR; \
	nop; \
	or	srdata, savsr_reg, SR_ISC; \
	bne	cachopt, zero, 99f; \
	nop; \
	and	srdata, savsr_reg, ~SR_ISC; \
99:	mtc0	srdata, C0_SR;\
	nop;

/*
 * RESTORE_C0_SR() - Restore the C0_SR after ISOLATECACHE().
 * Along with the above macro, this makes a matching pair of 
 * macros for isolating the cache.
 */
#define RESTORE_C0_SR(savsr_reg) \
	mtc0	savsr_reg, C0_SR; \
	nop;


	.globl	test_table

/*
 * cmd_dummy()
 */
LEAF(cmd_dummy)
	move	s0,ra
	PRINT("\r\n->Not yet implemented<-\r\n")
	move	v0,zero
	j	s0
END(cmd_dummy)


/*
 * int cmd_settable(struct test_tbl *tableptr)
 *
 * Set the current test table ptr to be tableptr and Set the ML
 * bit (Menu Level).
 *
 * This command routine is entered in the main test table as a test
 * and executed as a special test by the command parser.
 */
LEAF(cmd_settable)
	move	s0,ra
	move	s1, a0			# save the new table ptr

	li	a1,_TTBLPTR		# set the new test table ptr
	jal	sw_nvram

	li	a0,_CNFGSTATUS
	jal	lw_nvram

	or	CnfgStatusReg, v0, MLFLAG_MASK	# set ML bit

	move	a0, CnfgStatusReg
	li	a1,_CNFGSTATUS
	jal	sw_nvram

	move	v0,zero
	j	s0
END(cmd_settable)


#if	defined (RB3125)
/*
 * int cmd_config(char *env_nvptr, int parm1)
 *
 * Display sys hardware configurations
 *
 */
LEAF(cmd_config)
	move	s0, ra

	PRINT(" \n\nCPU/PRID= 0x")
	.set	noreorder
	mfc0	a0, C0_PRID
	nop
	.set	reorder
	move	a1, zero
	jal	putbcd

	.set	noreorder
	mfc0	a0, C0_SR		# print FPA/Rev only if SR_CU1=1
	nop
	.set	reorder
	and	a0, SR_CU1
	beq	a0, zero, 1f

	PRINT(" FPA/Rev= 0x")
	.set	noreorder
	cfc1	a0, fcr0		# read FPA's rev
	nop
	.set	reorder
	move	a1, zero
	jal	putbcd
1:
	jal	SizeMemory		# Size main memory
	move	s1, v0
	move	a0, v0
	li	a1, _MEMSIZE
	jal	sw_nvram		# save the size in nvram
	PRINT("\nMemory Size: ")
	move	a0, s1
	li	a1, 9			# 9 digits
	jal	putudec
	PRINT(" (0x")
	PUTHEX(s1)
	PRINT(") Bytes, ")
	srl	s1, 20			# number of MB
	PUTUDEC(s1)

	PRINT(" MB\nICACHE Size: ")
	li	a0, icacheSize
	li	a1, 9			# 9 digits
	jal	putudec
	PRINT(" (0x")
	li	a0, icacheSize
	jal	puthex
	PRINT(") Bytes\nDCACHE Size: ")

	li	a0, dcacheSize
	li	a1, 9			# 9 digits
	jal	putudec
	PRINT(" (0x")
	li	a0, dcacheSize
	jal	puthex
#ifdef	RB3125
	PRINT(") Bytes\n")
#endif	RB3125

	move	v0, zero		# set normal return value
	j	s0
END(cmd_config)

#endif	RB3125

/* 
 * int cmd_run(char *env_nvptr, int parm1)
 *
 * Run all tests in the current table. Return if ML bit of CnfgStatusReg
 * is not set, i.e. it is in the top menu level and no tset table is
 * selected yet.
 */
LEAF(cmd_run)
	move	s0, ra

	li	a0, RUN_ALLTESTS	# pass run all tests mode

	and	v0, CnfgStatusReg, MLFLAG_MASK
	bne	v0, zero, 1f		# if main menu, run all modules

	li	a0, RUN_ALLMODS		# run all modules in main test table
1:
	li	a1, 1			# start from test 1
	jal	RunTests

	jal	print_menu

	move	v0, zero
	j	warm_reset
END(cmd_run)


/*
 * int cmd_quit(char *env_nvptr, int parm1)
 *
 * Reset ML bit of test ctrl register and test table ptr
 * back to the main test table.
 *
 */
LEAF(cmd_quit)
	move	s0,ra			# save ra

	li	a0,_CNFGSTATUS
	jal	lw_nvram

	and	CnfgStatusReg, v0, (~MLFLAG_MASK) # reset ML bit

	move	a0, CnfgStatusReg
	li	a1,_CNFGSTATUS
	jal	sw_nvram

	la	a0,test_table		# set _TTBLPTR to main test table
	li	a1,_TTBLPTR
	jal	sw_nvram

	la	a0, maintest_menu	# reset modname to main test menu
	li	a1, _MODNAME
	jal	sw_nvram

	move	v0,zero
	j	s0
END(cmd_quit)


/*
 * int cmd_exit(char *env_nvptr, int parm1)
 *
 * Call exit(0)
 *
 */
LEAF(cmd_exit)
	move	a0, zero
	la	t0, exit
	j	t0
END(cmd_exit)

 /*
 * int cmd_help(char *env_nvptr, int parm1)
 *
 * Syntax:	help	[cmd]    OR
 *		? 	[cmd]
 *
 * Display help on a command if given, or all commands
 *
 * Registers used: a-0a3, t0-tx, s0-s3
 *
 */
	.data
msg_keys:
	.ascii  "\r\n\n"
	.ascii  " Where <x> - Required parameter, [x] - Optional parameter.\r\n"
	.ascii  " Options -q = Quiet mode (no display of data), -c = Write/Read Cache\r\n" 
	.ascii  " lpcnt = If zero, loop until <Esc> entered.\r\n"
	.asciiz " Valid input formats: Decimal, Octal(0), Binary(0b) or Hex(0x).\r\n"

	.text

LEAF(cmd_help)
	move	s0,ra		# save ra

	/*
	 * Check for any cmd specified on the cmd line
	 */
1:
	lbu	t0, (a0)
	beq	t0, zero,_help_all
	bne	t0, char_SP, 1f
	addu	a0, 4			# bump to next char
	b	1b
1:
	move	s1, a0			# save the cmd str ptr
	/*
	 * search the command in the command table
	 */
	li	a0,_CMDTBLPTR		# get current cmd table ptr
	jal	lw_nvram
	move	a0,v0			# pass cmd table ptr
	move	a1,s1			# pass nvram buf ptr
	jal	lookup_cmdtbl
	beq	v0, zero, 1f		# cmd found?
	lw	s1, OFFSET_CMDSTR(v0)	# get cmd string ptr in s1
	beq	s1, zero, 1f

	li	a0, char_NL
	jal	_putchar
	li	a0, char_NL
	jal	_putchar
	move	a0, s1
	jal	_puts			# print the command help 
	PUTS(msg_keys)
	b	_help_done
1:
	PRINT("\r\nHelp: Command not found\n")
	b	_help_done

	/*
	 * Display help for all commands
	 *
	 * A command table is divided into pages. Char '\n' is
	 * used to indicate the start of a new page.
	 *
	 * Commands of help paging:
	 *	n 	- next page
	 *	p 	- previous page
	 *	q 	- quit help
	 *	ctrl-C 	- abort help
	 *
	 * Register usage:
	 *	s1 - command entry ptr
	 *	s2 - command string ptr
	 *	s3 - ptr to 1st page
	 */
_help_all:
        li      a0, _CMDTBLPTR   	# get current cmd table ptr
        jal     lw_nvram
	move	s1, v0			# point to next cmd entry
	move	s3, s1			# save page 1 ptr in s3
	lw	a0, OFFSET_CMDSTR(s1)	# check for page deliminator
	lbu	a0, (a0)
	bne	a0, char_NL, _next_cmdpage
	addu	s1, CTBLENTRY_SIZE	# skip page del. for 1st page
	move	s3, s1			# save page 1 ptr in s3

_next_cmdpage:
	la	a0, _crlf		# print '\n'
	jal	_puts
1:
        lw      a0, (s1)         	# look for the end of table
        beq     a0, zero, _help_page_end

	lw	s2, OFFSET_CMDSTR(s1)	# get command str ptr
	beq	s2, zero, _help_next_cmd
	lbu	a0, (s2)			# get 1st char
	beq	a0, char_NL, _help_page_end	# end of current page

	la	a0, _crlf		# '\n'
	jal	_puts
	move	a0, s2			# print a command help line
	jal	_puts

_help_next_cmd:
	addu	s1, CTBLENTRY_SIZE	# bump to next command
	b	1b

_help_page_end:
	PUTS(msg_keys)

_help_get_cmd:
	PRINT("\r--help-- [n-next, p-previous, q-quit]  \b")
	lw	a0, (s1)
	bne	a0, zero, 1f
	PRINT("(END)  \b")
1:
	jal	_getchar			# get a command char
	and	v0, 0x5F			# map into upper case
	beq	v0, char_CTRLC, _help_done	# ctrl-C aborts help
	beq	v0, char_CR, _help_cmd_n	# <CR> == command 'n'
	blt	v0, char_SP, _help_get_cmd	# wait for a valid char
	beq	v0, char_N, _help_cmd_n
	beq	v0, char_P, _help_cmd_p
	beq	v0, char_Q, _help_done
	b	_help_get_cmd

_help_cmd_n:
	lw	a0, (s1)
	# beq	a0, zero, _help_get_cmd		# end of table?
	beq	a0, zero, _help_done		# end of table?
	addu	s1, CTBLENTRY_SIZE		# bump to start of next page
	lw	a0, (s1)
	beq	a0, zero, _help_get_cmd		# end of table?
	b	_next_cmdpage			# print the page

_help_cmd_p:
	/*
	 * since s1 now points to the end of current page, we need
	 * to go backward two pages.
	 */
	li	t0, 2				# page loop count
1:
	lw	s2, OFFSET_CMDSTR-CTBLENTRY_SIZE(s1)	# get previous cmd str 
	lbu	a0, (s2)			# look for the start of a page
	beq	a0, char_NL, 2f			# at page boundary
	subu	s1, CTBLENTRY_SIZE		# bump to previous command
	b	1b
2:
	bleu	s1, s3, _next_cmdpage		# at top of table?
	sub	t0, 1
	beq	t0, zero, _next_cmdpage
	subu	s1, CTBLENTRY_SIZE		# bump to previous command
	b	1b				# back one more page
	
_help_done:
	move	v0,zero
	j	s0
END(cmd_help)

/*
 * int cmd_jal(char *env_nvptr, int parm1)
 *
 * jal to an address
 *
 * command syntax: "jal <addr>"
 *
 * registers used:
 *	s0, s1, v0 & v1
 *
 */
LEAF(cmd_jal)
	.set	noreorder
	move	s0,ra		# save ra

	/*
	 * get address to jump to
	 */
	jal	aton_nv		# convert to binary: v1!=0 if bad number
	nop
	bne	v1,zero,1f
	nop

	# and	s1, v0, ~0x03	# align to word address
	move	s1, v0

	PRINT("\n\n	li	s1, 0x")
	PUTHEX(s1)
	PRINT("\n	jal	s1\n")

	jal	s1
	nop

	j	warm_reset	# return to monitor loop
	nop
1:
	j	s0		# no address given or error
	or	v0, zero, 1
	.set	reorder
END(cmd_jal)


/*
 * int cmd_jmp(char *env_nvptr, int parm1)
 *
 * jump to an address (the same as boot prom's go command)
 *
 * command syntax: "go <addr>"
 *
 * registers used:
 *	s0, s1, v0 & v1
 *
 */
LEAF(cmd_jmp)
	.set	noreorder
	move	s0,ra		# save ra

	/*
	 * get address to jump to
	 */
	jal	aton_nv		# convert to binary: v1!=0 if bad number
	nop
	bne	v1,zero,1f
	nop

	srl	s1, v0, 2	# align to word address
	sll	s1, 2
	PRINT("\nJump to 0x")
	PUTHEX(s1)

	jal	SizeMemory	# get memory size
	nop

	sw	zero, environ	# zero environ ptr
	la	a0, environ	# pass environ pointer
	move	a1, s1		# pass client program pc
	or	a2, v0, K1BASE	# pass client sp in kseg1 addr
	subu	a2, 0x4000	# this is what bootprom does

	jal	client_start	# start client program
	nop
1:
	j	s0		# no address given or error
	or	v0, zero, 1
	.set	reorder
END(cmd_jmp)


/*
 * int cmd_sload(char *env_nvptr, int parm1)
 *
 * serial download program - the prom side of "sdownload" on Unix
 *
 * command syntax: "sload [-b]"
 *
 *	where	-b == do binary sload, otherwise ascii
 *
 * registers used:
 *	s0, s1, v0 & v1
 *	
 */
LEAF(cmd_sload)
	move	s0, ra		# save ra

	move	s1, zero	# set default mode to ascii (s1=0)
	/*
	 * check if "-b" given on the command line
	 */
	jal	getopt_nv
	beq	v0, zero, 1f	# default option == ascii mode
	bne	v0, char_B, 3f	# check for command option "-b"
	li	s1, 1		# set mode to binary
1:
	jal	mem_init	# init main memory if not initialized yet
	bne	v0, zero, 4f	# return if no main mem.

	jal	cache_init	# reset cache to know state before loading

	/*
	 * Set environ to null ptr
	 */
	sw	zero, environ

#ifdef DEBUG
	PRINT("\nsp = 0x")
	PUTHEX(sp)
	PUTS(_crlf)
#endif DEBUG

	bne	s1, zero, 1f
	nop
	PRINT("\nASCII mode")
	j	2f
1:
	PRINT("\nBinary mode")
2:
	PRINT(" (port B at 9600 baud)  ")

	move	a0, s1		# pass mode flag: 0 -ascii, 1 -binary
	jal	sload		# void sload(int do_binary) <- C routine!

	move	v0,zero
	j	s0		# return
3:
	PRINT("\nUnknown option: valid option = -b\n")
	move	v0,zero
	j	s0
4:
	PRINT("\nError: No main memory!\n")
	move	v0, zero
	j	s0
END(cmd_sload)

/*
 * int cmd_em(char *env_nvptr, int parm1)
 *
 * syntax: "em [n]"
 *
 * Get error mode: 0-exit, 1-continue, 2-loop, ...
 * LAST_ERRMODE defines the last error-mode number.
 */
LEAF(cmd_em)
	.set	noreorder
	move	s0,ra			# save ra

	/*
	 * use the number passed on command line if it is valid
	 */
	jal	aton_nv			# convert to binary: v1!=0 if bad number
	nop
	bne	v1,zero,1f
	nop
	bleu	v0,LAST_ERRMODE,_new_em
	nop
1:
	PRINT("\r\nEnter error-mode (0-exit, 1-continue, 2-loop): ")
	and	v0, CnfgStatusReg, ERRMODE_MASK
	srl	v0,ERRMODE_SHIFTCNT
	PUTUDEC(v0)
	PRINT("\b")

	jal	getdec			# get a dec value 
	nop
	bne	v1,zero,_em_done	# no number entered if v1!=0
	nop
	ble	v0,LAST_ERRMODE,_new_em
	nop
	PRINT("Invalid error-mode!\r\n")
	b	_em_done
	nop
_new_em:
	sll	v0, ERRMODE_SHIFTCNT
	and	v0, ERRMODE_MASK
	and	CnfgStatusReg, (~ERRMODE_MASK)
	or	CnfgStatusReg, v0

	move	a0, CnfgStatusReg	# store the new mode
	li	a1, _CNFGSTATUS
	jal	sw_nvram
	nop
_em_done:
	j	s0
	move	v0,zero
	.set	reorder
END(cmd_em)


/*
 * int cmd_ll(char *env_nvptr, int parm1)
 *
 * syntax: "ll [n]"  0 == ignore loop limit (infinite loop)
 *
 * Get loop-limit
 */
LEAF(cmd_ll)
	.set	noreorder
	move	s0,ra			# save ra

	/*
	 * use the number passed on command line if it is valid
	 */
	jal	aton_nv			# convert any number on command line to binary
	nop
	beq	v1,zero,_new_ll		# if v1!=0 bad number or error
	nop
1:
	PRINT("\r\nEnter loop-limit (0 = ignore loop limit): ")

	jal	getdec			# get a dec value 
	nop
	bne	v1,zero,_ll_done	# no number entered if v1!=0
	nop
_new_ll:
	move	a0,v0			# store new loop-limit
	jal	sw_nvram
	li	a1,_LOOPLMT
_ll_done:
	j	s0
	move	v0,zero
	.set	reorder
END(cmd_ll)



/*
 * int cmd_el(char *env_nvptr, int parm1)
 *
 * syntax: "el [n]"  0 == ignore error limit
 *
 * Get error-limit
 */
LEAF(cmd_el)
	.set	noreorder
	move	s0,ra			# save ra

	/*
	 * use the number passed on command line if it is valid
	 */
	jal	aton_nv			# convert any number on command line to binary
	nop
	beq	v1,zero,_new_el		# if v1!=0, bad number or error
	nop
1:
	PRINT("\r\nEnter error-limit (0 = ignore error limit): ")

	jal	getdec			# get a dec value 
	nop
	bne	v1,zero,_el_done	# no number entered if v1!=0
	nop

_new_el:
	move	a0, v0
	li	a1,_ERRORLMT		# save new error limit
	jal	sw_nvram
	nop
_el_done:
	j	s0
	move	v0,zero
	.set	reorder
END(cmd_el)

/*
 * int cmd_fa(char *env_nvptr, int parm1)
 *
 * syntax: "fa [n]"
 *
 * set first address for memory tests
 */
LEAF(cmd_fa)
	move	s0,ra			# save ra
	/*
	 * use the number passed on command line if it is valid
	 */
	jal	aton_nv			# convert to binary: v1!=0 if bad number
	beq	v1, zero, _new_fa
	PRINT("\r\nEnter First_Addr for memory tests: 0x")
	jal	gethex			# get a dec value 
	beq	v1,zero,_new_fa		# no number entered if v1!=0
	li	v0, 1			# set syntax error status
	j	s0
_new_fa:
	move	a0, v0			# store the new value 
	li	a1, _FIRSTADDR
	jal	sw_nvram
	move	v0,zero
	j	s0
END(cmd_fa)

/*
 * int cmd_la(char *env_nvptr, int parm1)
 *
 * syntax: "la [n]"
 *
 * set last address for memory tests
 */
LEAF(cmd_la)
	move	s0,ra			# save ra
	/*
	 * use the number passed on command line if it is valid
	 */
	jal	aton_nv			# convert to binary: v1!=0 if bad number
	beq	v1, zero, _new_la
	PRINT("\r\nEnter Last_Addr for memory tests: 0x")
	jal	gethex			# get a dec value 
	beq	v1,zero,_new_la		# no number entered if v1!=0
	li	v0, 1			# set syntax error status
	j	s0
_new_la:
	move	a0, v0			# store the new value 
	li	a1, _LASTADDR
	jal	sw_nvram
	move	v0,zero
	j	s0
END(cmd_la)


/*
 * init mem_init(void)
 *
 * Initialize main memory in kseg1 space if not yet done.
 * Return zero if OK, 1 if no main memory found.
 *
 * Registers used: a0, a1 & v0
 */
LEAF(mem_init)
	move	a0, ra
	jal	push_nvram

	/*
	 * Check if already done
	 */
	li	a0, _DIAG_FLAG
	jal	lw_nvram
	and	a0, v0, DBIT_MEMINIT	# bit set if already done
	bne	a0, zero, 2f
	
	or	a0, v0, DBIT_MEMINIT	# set this bit
	li	a1, _DIAG_FLAG
	jal	sw_nvram

	/*
	 * Allow the heart of this algorithm to execute i-cached
	 */

	jal	SizeMemory	# any main memory there?
	beq	v0, zero, 3f

#ifdef DEBUG
	PRINT("\n- Initializing main memory -\n") 
#endif DEBUG

	li	a0, K1BASE	# first address
	addu	a1, a0, v0	# last address
	subu	a1, 32		# target on last pass of loop
	
/*
 * Do not run in i-cached mode.
	la	v0, 1f		# flip into cached i-fetch mode
	sll	v0, 3
	srl	v0, 3
	or	v0, K0BASE
	j	v0
*/
1:
	sw	zero, 0(a0)
	sw	zero, 4(a0)
	sw	zero, 8(a0)
	sw	zero, 12(a0)
	sw	zero, 16(a0)
	sw	zero, 20(a0)
	sw	zero, 24(a0)
	sw	zero, 28(a0)
	.set	noreorder
	bne	a0, a1, 1b
	addu	a0, 32
	.set	reorder
	la	v0, 2f
	j	v0		# exit the cached space
2:
	jal	pop_nvram
	move	ra, v0
	move	v0, zero	# normal return
	j	ra
3:
	jal	pop_nvram
	move	ra, v0
	li	v0, 1		# no main memory 
	j	ra
END(mem_init)

/*
 * int cmd_init(char *env_nvptr, int parm1)
 *
 * init command: init caches or main memory
 *
 * command syntax: "init [-c | -m]"
 * options: 
 *	-c	- init all caches
 *	-m	- init main memory to zero
 *
 * registers used:
 *	a0 - nv string ptr
 * 	s0 - saved ra
 *	s1 - option char
 */
LEAF(cmd_init)
	move	s0,ra		# save ra in s0
	/*
	 * get parameters from the command line
	 */
	jal	getopt_nv
	and	s1, v0, 0x5F	# option char (mapped to upper case) (0 if none)

	bne	s1, char_C, 1f
	jal	cache_init	# "-c": init caches
	j	2f
1:
	bne	s1, char_M, 1f	# "-m": init memory

	li	a0, _DIAG_FLAG
	jal	lw_nvram	# Reset bit0 to force memory init
	or	a0, v0, DBIT_MEMINIT
	li	a1, _DIAG_FLAG
	jal	sw_nvram

	/* 
	 * Note, this is a wrapper that calls the appropriate memory
	 * routine depending on the defines at compile time.
	 */
	jal	dmon_mem_init
	nop

	j	2f
1:
	li	v0, 1		# syntax error
	j	s0
2:
	move	v0, zero	# normal return
	j	s0
END(cmd_init)
 
/*
 * int cmd_c0(void)
 *
 * Display contents of C0 registers
 *
 * command syntax: "c0"
 *
 */
LEAF(cmd_c0)
	move	s7,ra

	jal	show_cpustate
	
	move	v0,zero
	j	s7
END(cmd_c0)


/*
 * int cmd_c0sr(char *env_nvptr, int parm1)
 *
 * Set C0_SR (BEV is always kept the same)
 *
 * command syntax: "c0sr <n>"
 *
 */
LEAF(cmd_c0sr)
	.set	noreorder
	move	s0,ra

	jal	aton_nv		# get new C0_SR value
	nop
	bne	v1,zero,1f	# if error, then return
	nop
	move	s1,v0		# s1 is the new C0_SR value

	mfc0	t0,C0_SR
	nop
	and	t0,SR_BEV	# Keep BEV bit unchanged
	and	s1,~SR_BEV	# Keep BEV bit unchanged
	or	t0,s1
	mtc0	t0,C0_SR
	nop
	j	s0
	move	v0,zero
1:
	j	s0		# error return
	or	v0,zero,1
	.set	reorder
END(cmd_c0sr)


#if	defined (RB3125) || defined (R3030)
/*
 * int cmd_cm(void)
 *
 * reset CM bits of C0_SR
 *
 * command syntax: "cm" 
 *
 */
LEAF(cmd_cm)
	.set	noreorder
	mfc0	t0,C0_SR
	nop
	and	t0,~SR_CM
	mtc0	t0,C0_SR
	j	ra
	move	v0, zero	# (LDSLOT)
	.set	reorder
END(cmd_cm)


/*
 * int cmd_pz(char *env_nvptr, int parm1)
 *
 * Set/Reset PZ bit of C0_SR
 *
 * command syntax: "pz [0|1]" - "pz" == "pz 0"
 *
 */
LEAF(cmd_pz)
	.set	noreorder
	move	s0,ra
	move	s1,zero		# default PZ value: 0 (reset)

	jal	aton_nv		# get parameter: 0 or 1
	nop
	bne	v1,zero,1f	# if error (v1!=0) or no number given
	nop			#	use 0 (reset PZ)
	move	s1,v0		# s1 is the bit value
	bgtu	s1,1,3f		# error if it is not 0 or 1
	nop
1:
	mfc0	t0,C0_SR
	nop
	and	t0,~SR_PZ	# reset PZ bit
	beq	s1,zero,2f
	nop
	or	t0,SR_PZ	# set PZ bit
2:
	mtc0	t0,C0_SR
	nop
	j	s0
	move	v0,zero
3:
	j	s0		# error return
	or	v0,zero,1
	.set	reorder
END(cmd_pz)

#endif	RB3125


/*
 * int cmd_sw(char *env_nvptr, int parm1)
 *
 * write word into memory
 *
 * command syntax: "sw -[whb] -[c] <addr> <data> [loopcount]"
 * options: 
 *	loopcount - default=1, 0-loop forever
 *
 * registers used:
 *	a0 - nv string ptr
 * 	s0 - saved ra
 *	s1 - address
 *	s2 - data to store
 *	s3 - loopcount (default = 1)
 *	s4 - size of write (word, halfword, byte)
 *	s5 - Cache option (yes=1, no=0)
 *	s6 - Save status register
 *	s7 - scratch register
 */
LEAF(cmd_sw)
	.set	noreorder
	move	s0,ra		# save ra in s0

	/*
	 * get WORD SIZE parameter from the command line
	 */
	jal	getopt_nv	# get the option: default=="-w"
	nop
	move	s4,v0		# s4 is the option: 0==default= WORD access

#if	defined (RB3125) || defined (R3030)
	/*
	 * get CACHE parameter from the command line
	 */
	li	a1, char_C	# Check for cache option -c
	jal	chkopt_nv	# get the option: default=="-c"
	nop
	move	s5,v0		# if zero, cache option not selected
#endif	RB3125

	/*
	 * get ADDRESS to write
	 */
	jal	aton_nv		# get addr
	nop
	bne	v1,zero,2f	# v1!=0 if error
	move	s1,v0		# s1 is the address

	/*
	 * Get DATA
	 */
	jal	aton_nv		# get data
	nop
	bne	v1,zero,2f	# v1!=0 if error
	move	s2,v0		# s2 is data to fill 

	/*
	 * Get LOOPCOUNT
	 */
	jal	aton_nv		# get loop count
	nop
	bne	v1,zero,1f	# if v1!=0 (error), use default
	or	s3,zero,1	# set default loop count
	move	s3,v0		# s3 is the unit count 
1:
	beq	s4,zero,1f	# zero -> option==default
	nop
	beq	s4,char_W,1f	# "-w"
	nop
	and	s2,0x0000FFFF	# mask the upper half word
	beq	s4,char_H,1f	# "-h"
	nop
	and	s2,0x00FF	# keep only the LSB
	bne	s4,char_B,2f	# error if illegal option given
	nop
1:

	PRINT("\r\nsw 0x")
	PUTHEX(s2)
	PRINT(" , (0x")
	PUTHEX(s1)
	PRINT(")")


_write_data:

#if	defined	(RB3125) || defined (R3030)
	ISOLATECACHE(s6,s7,s5)
#endif	RB3125

	beq	s4,zero,11f	# if no option, the default is word
	nop
	nop
	bne	s4,char_W,1f	# fill memory in words
	nop
11:
	sw	s2,(s1)
	subu	s3,1
	bne	s3, zero, 11b
	move	v0,zero
	b	32f    		# return
1:
	nop
	bne	s4,char_H,1f	# fill memory in half-words 
	nop
11:
	sh	s2,(s1)
	subu	s3,1
	bne	s3, zero, 11b
	move	v0,zero
	b	32f		# return
	nop
1:
	sb	s2,(s1)		# fill memory in bytes
	subu	s3,1
	bne	s3, zero, 1b
	move	v0,zero

32:
#if	defined (RB3125) || defined (R3030)

	/* Restore status register */
	RESTORE_C0_SR(s6)	

#endif	RB3125
	j	s0		# return
	nop
2:
	j	s0		# error return
	or	v0,zero,1
	.set	reorder
END(cmd_sw)


/*
 * int cmd_swloop(char *env_nvptr, int parm1)
 *
 * write word into memory in an infinite loop
 *
 * command syntax: "swloop <addr> <data>"
 *
 * registers used:
 *	a0 - nv string ptr
 * 	s0 - saved ra
 *	s1 - address
 *	s2 - data to store
 *	s4 - Word size (byte, halfword, word)
 *	s5 - Cache Option 0=non-cach 1=write cache
 */
LEAF(cmd_swloop)
	.set	noreorder
	move	s0,ra		# save ra in s0
	/*
	 * get WORD SIZE parameter from the command line
	 */
	jal	getopt_nv	# get the option: default=="-w"
	nop
	move	s4,v0		# s4 is the option: 0==default= WORD access

#if	defined (RB3125) || defined (R3030)
	/*
	 * get CACHE parameter from the command line
	 */
	li	a1, char_C	# Check for cache option -c
	jal	chkopt_nv	# get the option: default=="-c"
	nop
	move	s5,v0		# if zero, cache option not selected
#endif	RB3125
	/*
	 * get ADDRESS to write
	 */
	jal	aton_nv		# get addr
	nop
	bne	v1,zero,3f	# v1!=0 if error
	move	s1,v0		# s1 is the address

	/*
	 * Get DATA
	 */
	jal	aton_nv		# get data
	nop
	bne	v1,zero,3f	# v1!=0 if error
	move	s2,v0		# s2 is data to fill 

1:
	beq	s4,zero,1f	# zero -> option==default
	nop
	beq	s4,char_W,1f	# "-w"
	nop
	and	s2,0x0000FFFF	# mask the upper half word
	beq	s4,char_H,1f	# "-h"
	nop
	and	s2,0x00FF	# keep only the LSB
	bne	s4,char_B,3f	# error if illegal option given
	nop
1:

	PRINT("\r\nSW Scope Loop  (reset sys to reboot):\n\n sw 0x")
	PUTHEX(s2)
	PRINT(" , (0x")
	PUTHEX(s1)
	PRINT(")")


_swloop_fault:
	INSTALLVECTOR(_swloop_fault)	# Get back here on exception
_swloop:

#if	defined (RB3125) || defined (R3030)
	ISOLATECACHE(s6,s7,s5)
#endif	RB3125

	beq	s4,zero,11f	# if no option, the default is word
	nop
	bne	s4,char_W,1f	# fill memory in words
	nop
11:
	sw	s2,(s1)
	b	11b
	nop
        b       illterm                 # return
        nop
1:
	bne	s4,char_H,1f	# fill memory in half-words 
	nop
11:
	sh	s2,(s1)
	b	11b
	nop
        b       illterm             # return
        nop
1:
	sb	s2,(s1)		# fill memory in bytes
	b	1b
	nop

illterm:

#if     defined (RB3125) || defined (R3030)

        /* Restore status register */
        RESTORE_C0_SR(s6)

#endif  RB3125

        PRINT("\nUnexpected termination of sw scope loop")
        j       s0
        move    v0, zero        # (LDSLOT)
3:				# error
	PRINT("\nSyntax: swloop <addr> <data>\n")
	j	s0
	or	v0,zero,1
	.set	reorder
END(cmd_swloop)


/*
 * int cmd_lw(char *env_nvptr, int parm1)
 *
 * load word from memory
 *
 * command syntax: "lw <addr> [loopcount] [-q]"
 * options: 
 *	loopcount - default=1, 0-loop forever
 *	-q	  - quiet mode (no display of data read)
 *
 * registers used:
 *	a0 - nv string ptr
 * 	s0 - saved ra
 *	s1 - address
 *	s2 - data read
 *	s3 - loopcount (default = 1)
 *	s4 - read size (byte, halfword, word)
 *	s5 - Cache option 0=non-cache 1=read cache
 *	s6 - Saved status register
 *	s7 - OR'd status register | s5
 */
LEAF(cmd_lw)
	.set	noreorder
	move	s0,ra		# save ra in s0

	or	s3,zero,1	# set default loop count
	/*
	 * get WORD SIZE parameter from the command line
	 */
	jal	getopt_nv	# get the option: default=="-w"
	nop
	move	s4,v0		# s4 is the option: 0==default= WORD access

#if	defined (RB3125) || defined (R3030)
	/*
	 * get CACHE parameter from the command line
	 */
	li	a1, char_C	# Check for cache option -c
	jal	chkopt_nv	# get the option: default=="-c"
	nop
	move	s5,v0		# if zero, cache option not selected
#endif	RB3125

	/*
	 * get ADDRESS to READ
	 */
	jal	aton_nv		# get addr
	nop
	bne	v1,zero,66f	# v1!=0 if error
	move	s1,v0		# s1 is the address

	/*
	 * Get LOOPCOUNT
	 */
	jal	aton_nv		# get loop count
	nop
	bne	v1,zero,1f	# if v1!=0 (error), use default
	nop
	move	s3,v0		# s3 is the unit count 
	/*
	 * Check for QUIET parameter from the command line
	 */
1:
	li	a1, char_Q	# Check for cache option -q
	jal	chkopt_nv	# get the option: default=="-q"
	nop
	move	s2,v0		# if zero, quite option not selected

	beq	s4,zero,1f	# zero -> option==default
	nop
	beq	s4,char_W,1f	# "-w"
	nop
	beq	s4,char_H,1f	# "-h"
	nop
	bne	s4,char_B,66f	# error if illegal option given
	nop
1:
	
	PRINT("\r\nlw (0x")
	PUTHEX(s1)
	# PRINT(") -- loop ")
	PRINT(") ")

	/*
	 * If LOOP ==0 then loop forever
	 */
	beq	s3,zero,20f	# loop forever until <esc>
	nop
	# PUTUDEC(s3)		# print loop count

1:	
#if	defined (RB3125) || defined (R3030)
	ISOLATECACHE(s6,s7,s5)
#endif	RB3125

	beq	s4,zero,2f	# zero -> option==default
	nop
	bne	s4,char_W,3f	# "-w"
	nop
2:				/* READ WORD */
	lw	t2, (s1)
	b	5f
	nop
3:
	bne	s4,char_H,4f	# "-h"
	nop
	lhu	t2, (s1)	/* READ HALFWORD */
	b	5f
	nop
4:				/* ELSE READ BYTE */
	lbu	t2, (s1)
5:
	bne	s2,zero,11f	# no display if quiet mode ("-q")
	nop

#if	defined (RB3125) || defined (R3030)
	/* Restore status register */
	RESTORE_C0_SR(s6)	
#endif	RB3125

	PUTS(_crlf)		# display data read from scache 
	PUTHEX(t2)
11:
	subu	s3,1
	bne	s3,zero,1b	# loop s3 times
	nop

#if	defined (RB3125) || defined (R3030)
	/* Restore status register */
	RESTORE_C0_SR(s6)	
#endif	RB3125

	j	s0
	move	v0,zero
	/*
	 * LOOP FOREVER CODE
	 */
20:
	PRINT(" -> loop until <esc>")

#if	defined (RB3125) || defined (R3030)
	ISOLATECACHE(s6,s7,s5)
#endif	RB3125

	beq	s4,zero,2f	# zero -> option==default
	nop
	bne	s4,char_W,3f	# "-w"
	nop
2:				/* READ WORD */
	lw	t2, (s1)
	b	5f
	nop
3:
	bne	s4,char_H,4f	# "-h"
	nop
	lhu	t2, (s1)	/* READ HALFWORD */
	b	5f
	nop
4:				/* ELSE READ BYTE */
	lbu	t2, (s1)
5:

#if	defined (RB3125) || defined (R3030)
	/* Restore status register */
	RESTORE_C0_SR(s6)	
#endif	RB3125

	bne	s2,zero,11f	# no display if quiet mode ("-q")
	nop
	PUTS(_crlf)		# display data read from scache 
	PUTHEX(t2)
11:				/* CHECK for CTRL-C */
	jal	may_getchar
	nop
	bne	v0,char_ESC,20b
	nop
	j	s0
	move	v0,zero
66:				# cmd error
	j	s0
	or	v0,zero,1
	.set	reorder
END(cmd_lw)



/*
 * int cmd_lwloop(char *env_nvptr, int parm1)
 *
 * load word from memory in an infinite loop
 *
 * command syntax: "lwloop <addr> <data>"
 *
 * registers used:
 *	a0 - nv string ptr
 * 	s0 - saved ra
 *	s1 - address
 *	s2 - data read
 *	s4 - access size, byte, halfword, word
 *	s5 - Cache option 0= read cache 1=read memory
 *	s6 - saved status register
 *	s7 - OR'd status register with s5 flag
 */
LEAF(cmd_lwloop)
	.set	noreorder
	move	s0,ra		# save ra in s0
	/*
	 * get WORD SIZE parameter from the command line
	 */
	jal	getopt_nv	# get the option: default=="-w"
	nop
	move	s4,v0		# s4 is the option: 0==default= WORD access

#if	defined (RB3125) || defined (R3030)
	/*
	 * get CACHE parameter from the command line
	 */
	li	a1, char_C	# Check for cache option -c
	jal	chkopt_nv	# get the option: default=="-c"
	nop
	move	s5,v0		# if zero, cache option not selected
#endif	RB3125

	/*
	 * get ADDRESS to write
	 */
	jal	aton_nv		# get addr
	nop
	bne	v1,zero,3f	# v1!=0 if error
	move	s1,v0		# s1 is the address

1:
	beq	s4,zero,1f	# zero -> option==default
	nop
	beq	s4,char_W,1f	# "-w"
	nop
	beq	s4,char_H,1f	# "-h"
	nop
	bne	s4,char_B,3f	# error if illegal option given
	nop
1:

	PRINT("\r\nLW Scope Loop  (reset sys to reboot):\n\n lw s2, (0x")
	PUTHEX(s1)
	PRINT(")")


_lwloop_fault:
	INSTALLVECTOR(_lwloop_fault)	# Get back here on exception
_lwloop:

#if	defined (RB3125) || defined (R3030)
	ISOLATECACHE(s6,s7,s5)
#endif	RB3125

	beq	s4,zero,11f	# if no option, the default is word
	nop
	bne	s4,char_W,1f	# fill memory in words
	nop
11:
	lw	s2,(s1)
	b	11b
	nop
	b	illterm2    		# return
	nop
1:
	bne	s4,char_H,1f	# fill memory in half-words 
	nop
11:
	lhu	s2,(s1)
	b	11b
	nop
	b	illterm2		# return
	nop
1:
	lbu	s2,(s1)		# fill memory in bytes
	b	1b
	nop

illterm2:
#if	defined (RB3125) || defined (R3030)
	/* Restore status register */
	RESTORE_C0_SR(s6)	
#endif	RB3125

	PRINT("\nUnexpected termination of lw scope loop")
	j	s0
	move	v0, zero	# (LDSLOT)
3:				# error
	PRINT("\nSyntax: lwloop <addr> <data>\n")
	j	s0
	or	v0,zero,1
	.set	reorder
END(cmd_lwloop)


/*
 * int cmd_rw(char *env_nvptr, int parm1)
 *
 * write then read word from memeory. 
 *
 * command syntax: "rw <addr> <data> [loopcount] [-q]"
 * options: 
 *	loopcount - default=1, 0-loop forever
 *	-q	  - quiet mode (no display of data read)
 *
 * registers used:
 *	a0 - nv string ptr
 * 	s0 - saved ra
 *	s1 - address
 *	s2 - data to write
 *	s3 - loopcount (default = 1)
 *	s4 - set if option "-q" specified
 *	s5 - actual data read
 *	s6 - expected XOR actual
 */
LEAF(cmd_rw)
	.set	noreorder
	move	s0,ra		# save ra in s0

	or	s3,zero,1	# default loop count
	/*
	 * get parameters from the command line
	 */
	jal	aton_nv		# get addr
	nop
	bne	v1,zero,3f	# error if v1!=0
	nop
	# and	s1,v0,0xFFFFFFFC # s1 is the aligned word address
	move	s1, v0		# s1 is the word address

	jal	aton_nv		# get data
	nop
	bne	v1,zero,3f	# error if v1!=0
	move	s2,v0		# s2 is the data

	jal	aton_nv		# get loop count
	nop
	bne	v1,zero,1f	# if v1!=0, use defautl loop count 
	nop
	move	s3,v0		# loop count in s3: 0-loop-forever
1:
	li	a1,char_Q	# check for command option "-q"
	jal	chkopt_nv
	nop
	move	s4,v0		# set if "-q" given
	
	PRINT("\r\nsw/lw 0x")
	PUTHEX(s2)
	PRINT(", (0x")
	PUTHEX(s1)
	# PRINT(") -- loop ")
	PRINT(")")

	beq	s3,zero,2f	# loop forever until <esc>
	nop
	# PUTUDEC(s3)		# print loop count
1:
	sw	s2,(s1)		# store to memory
	nop
	nop
	lw	s5,(s1)		# load from memory

	bne	s4,zero,11f	# no display if quiet mode ("-q")
	nop

	PUTS(_crlf)		# display data read from memory
	PUTHEX(s2)
	PRINT("/")
	PUTHEX(s5)
	xor	s6,s2,s5	# xor of actual and expected
	beq	s6,zero,11f
	nop
	PRINT(" miscompare - xor: 0x")	# show xor only when not equal
	PUTHEX(s6)
11:
	subu	s3,1
	bne	s3,zero,1b	# loop s3 times
	nop
	j	s0		# return
	move	v0,zero
2:
	# PRINT("until <esc>")
	PRINT(" -> loop until <esc>")
1:
	sw	s2,(s1)		# store to memory
	nop
	nop
	lw	s5,(s1)		# load from memory

	bne	s4,zero,11f	# no display if quiet mode ("-q")
	nop

	PUTS(_crlf)		# display data read from memory
	PUTHEX(s2)
	PRINT("/")
	PUTHEX(s5)
	xor	s6,s2,s5	# xor of actual and expected
	beq	s6,zero,11f
	nop
	PRINT(" miscompare - xor: 0x")	# show xor only when not equal
	PUTHEX(s6)
11:
	jal	may_getchar 	# loop until <esc> entered
	nop
	bne	v0,char_ESC,1b
	nop
	j	s0
	move	v0,zero
	
3:				# cmd error
	j	s0
	or	v0,zero,1
	.set	reorder
END(cmd_rw)



/*
 * int cmd_fill(char *env_nvptr, int parm1)
 *
 * fill memory or scache
 *
 * command syntax: "fill -[w|h|b] -[c] <addr> <data> [count]"
 * options: 
 *	-c	fill cache
 *	-w	fill memory in words
 *	-h	fill memory in half words
 *	-b	fill memory in bytes
 *	count   - number of units to fill (default=1)
 *
 * registers used:
 *	a0 - nv string ptr
 * 	s0 - saved ra
 *	s1 - start address
 *	s2 - data read
 *	s3 - number of units then end address
 *	s4 - fill mode:  'w'-word, 'h'-half word, 'b'-byte
 *	s5 - cache flag, 0-non cache reads 1-reads cache
 *	s6 - Saved status register
 *	s7 - OR'd with isolate bit depending on s5
 */
LEAF(cmd_fill)
	.set	noreorder
	move	s0,ra		# save ra in s0

	/*
	 * get WORD SIZE parameter from the command line
	 */
	jal	getopt_nv	# get the option: default=="-w"
	nop
	move	s4,v0		# s4 is the option: 0==default= WORD access

#if	defined (RB3125) || defined (R3030)
	bne	s4,char_C,1f	# branch if not -c option
	li	s5,1		# (BDSLOT) set cache option flag
	li	s4,char_W	# set size to word
	b	2f
	nop
1:
	/*
	 * get CACHE parameter from the command line
	 */
	li	a1, char_C	# Check for cache option -c
	jal	chkopt_nv	# get the option: default=="-c"
	nop
	move	s5,v0		# if zero, cache option not selected
2:
#endif	RB3125

	/*
	 * get ADDRESS to WRITE
	 */
	jal	aton_nv		# get addr
	nop
	bne	v1,zero,2f	# v1!=0 if error
	move	s1,v0		# s1 is the address
	/*
	 * get DATA PATTERN 
	 */
	jal	aton_nv		# get data
	nop
	bne	v1,zero,2f	# v1!=0 if error
	move	s2,v0		# s2 is the fill data

	/*
	 * Get LOOPCOUNT
	 */
	or	s3, zero, 1
	jal	aton_nv		# get loop count
	nop
	bne	v1,zero,1f	# if v1!=0 (error), use default
	nop
	bne	v0,zero,1f
	move	s3,v0		# (BDSLOT) s3 is the unit count 
	li	s3,1		# count of 0 is illegal
1:
	subu	s3,1		# subtract count by 1
	sll	s3,2		# shift to word offset
	beq	s4,zero,1f	# zero -> option==default (word)
	nop
	beq	s4,char_W,1f	# "-w"
	nop
	srl	s3,1		# shift back to half-word offset
	and	s2,0x0000FFFF	# mask the upper half word
	beq	s4,char_H,1f	# "-h"
	nop
	srl	s3,1		# shift back to byte offset
	and	s2,0x00FF	# keep only the LSB
	bne	s4,char_B,2f	# error if illegal option given
	nop
1:
	addu	s3,s1		# Determine last address

	PRINT("\r\n\nfill 0x")
	PUTHEX(s2)
	PRINT(" from 0x")
	PUTHEX(s1)
	PRINT(" to 0x")
	PUTHEX(s3)


_fill_mem:

#if	defined (RB3125) || defined (R3030)
	ISOLATECACHE(s6,s7,s5)
#endif	RB3125

	beq	s4,zero,11f	# if no option, the default is word
	nop
	bne	s4,char_W,1f	# fill memory in words
	nop
11:
	sw	s2,(s1)
	blt	s1,s3,11b
	addi	s1, 4
	b	gd_exit		# return
	move	v0,zero
1:
	bne	s4,char_H,1f	# fill memory in half-words 
	nop
11:
	sh	s2,(s1)
	blt	s1,s3,11b
	addi	s1, 2
	b	gd_exit		# return
	move	v0,zero
1:
	sb	s2,(s1)		# fill memory in bytes
	blt	s1,s3,1b
	addi	s1, 1
	move	v0,zero

gd_exit:

#if	defined (RB3125) || defined (R3030)
	/* Restore status register */
	RESTORE_C0_SR(s6)	
#endif	RB3125

	j	s0		# return
	nop
2:
	j	s0		# error return
	or	v0,zero,1
	.set	reorder
END(cmd_fill)




/*
 * int cmd_dump(char *env_nvptr, int parm1)
 *
 * dump memory or scache
 *
 * command syntax: "dump -[s|w|h|b] <addr> [count]"
 * options: 
 *	-s	dump scache
 *	-w	dump memory in words
 *	-h	dump memory in half words
 *	-b	dump memory in bytes
 *	count   - number of units to dump (default=1)
 *
 * registers used:
 *	a0 - nv string ptr
 * 	s0 - saved ra
 *	s1 - start address
 *	s2 - number of units then end address
 *	s3 - data read
 *	s4 - dump mode: 'w'-word, 'h'-half word, 'b'-byte
 *	s5 - Isolate cache=1 0=regular memory read
 *	s6 - SAved status register
 *	s7 - OR'd status register with Isolate bit
 */
LEAF(cmd_dump)
	.set	noreorder
	move	s0,ra		# save ra in s0

	/*
	 * get WORD SIZE parameter from the command line
	 */
	jal	getopt_nv	# get the option: default=="-w"
	nop
	move	s4,v0		# s4 is the option: 0==default= WORD access

#if	defined (RB3125) || defined (R3030)
	bne	s4,char_C,1f	# branch if not -c option
	li	s5,1		# (BDSLOT) set cache option flag
	li	s4,char_W	# set size to word
	b	2f
	nop
1:
	/*
	 * get CACHE parameter from the command line
	 */
	li	a1, char_C	# Check for cache option -c
	jal	chkopt_nv	# get the option: default=="-c"
	nop
	move	s5,v0		# if zero, cache option not selected
2:
#endif	RB3125

	/*
	 * get ADDRESS to READ
	 */
	jal	aton_nv		# get addr
	nop
	bne	v1,zero,2f	# v1!=0 if error
	move	s1,v0		# s1 is the address

	/*
	 * Get number of WORDS to READ
	 */
	or	s3,zero,1	# set default 
	jal	aton_nv		# words to read
	nop
	bne	v1,zero,1f	# if v1!=0 (error), use default
	nop
	bne	v0,zero,1f
	move	s3,v0		# (BDSLOT) s3 is the unit count 
	li	s3,1		# count of 0 is illegal
1:
	subu	s3,1		# subtract count by 1
	sll	s3,2		# shift to word offset
	beq	s4,zero,1f	# zero -> option==default
	nop
	beq	s4,char_W,1f	# "-w"
	nop
	srl	s3,1		# shift back to half-word offset
	beq	s4,char_H,1f	# "-h"
	nop
	srl	s3,1		# shift back to byte offset
	bne	s4,char_B,2f	# error if illegal option given
	nop
1:
	addu	s3,s1		# Determine last address

	PRINT("\r\n\ndump 0x")
	PUTHEX(s1)
	PRINT(" to 0x")
	PUTHEX(s3)

_dump_mem:

	beq	s4,zero,11f	# if no option, the default is word
	nop
	bne	s4,char_W,1f	# fill memory in words
	nop
11:

#if	defined (RB3125) || defined (R3030)
	ISOLATECACHE(s6,s7,s5)
#endif	RB3125

	lw	s2,(s1)

#if	defined (RB3125) || defined (R3030)
	RESTORE_C0_SR(s6)
#endif	RB3125

	PUTS(_crlf)
	PUTHEX(s1)
	PRINT(": ")
	PUTHEX(s2)

	blt	s1,s3,11b
	addi	s1, 4
	b	32f		# return
	move	v0,zero
1:
	bne	s4,char_H,1f	# fill memory in half-words 
	nop
11:
#if	defined (RB3125) || defined (R3030)
	ISOLATECACHE(s6,s7,s5)
#endif	RB3125

	lhu	s2,(s1)

#if	defined (RB3125) || defined (R3030)
	RESTORE_C0_SR(s6)
#endif	RB3125

	PUTS(_crlf)
	PUTHEX(s1)
	PRINT(": ")
	#PUTHEX(s2)
	move	a0, s2
	jal	putbcd
	li	a1, 2
	blt	s1,s3,11b
	addi	s1, 2
	b	32f		# return
	move	v0,zero
1:
#if	defined (RB3125) || defined (R3030)
	ISOLATECACHE(s6,s7,s5)
#endif	RB3125

	lbu	s2,(s1)		# fill memory in bytes

#if	defined (RB3125) || defined (R3030)
	RESTORE_C0_SR(s6)
#endif	RB3125

	PUTS(_crlf)
	PUTHEX(s1)
	PRINT(": ")
	#PUTHEX(s2)
	move	a0, s2
	jal	putbcd
	li	a1, 2
	blt	s1,s3,1b
	addi	s1, 1
	move	v0,zero

32:
#if	defined (RB3125) || defined (R3030)
	RESTORE_C0_SR(s6)
#endif	RB3125
	j	s0		# return
	nop
2:
#if	defined (RB3125) || defined (R3030)
	RESTORE_C0_SR(s6)
#endif	RB3125
	j	s0		# error return
	or	v0,zero,1
	.set	reorder
END(cmd_dump)



#define	CHAR_NL		0xa
#define	CHAR_Q		0x51
#define	CHAR_Y		0x59

/*
 * int cmd_date(char *env_nvptr, int parm1)
 *
 * Display or set tod value (date) in nvram
 *
 * command syntax: "date [-s]"
 * options:
 *		-s	-- set new date interactively
 * 
 * Register usage:
 * 	t1  scratch register
 *	s0 - saved ra
 *	s1 - option char
 *      s2 - year/month/date/day 
 * 	s3 - hour/minute/second       
 *
 */
LEAF(cmd_date)
	move	s0, ra			# save return address.

	/*
	 * get parameters from the command line
	 */
	jal	getopt_nv		# get the option if any 
	and	s1, v0, 0x5F		# s1 is the option

	# PRINT("\n\nCurrent date is ")
	PUTS(_crlf)
	jal	date

	bne	s1, char_S, _date_done
	PRINT("\n\nSet date in nvram/tod chip: ")

set_loop:
	move	s2, zero
	move	s3, zero

read_year:
	PRINT("\n  Enter year  [0-99] > 0\b")
	jal	getbcd
	blt	v0, zero, read_year
	bgt	v0, 0x99, read_year
	/* Indy uses 1970 as its base year so subtract 70 from
	 * the year entered
	 */
#if	defined(IP24) | defined (IP22)
	addi	v0, -0x70
#endif
	sll	s2, v0, 24

read_month:
	PRINT("\n  Enter month [1-12] > ")
	jal	getbcd
	blt	v0, 1, read_month
	bgt	v0, 0x12, read_month
	sll	v0, 16
	or	s2, v0

read_date:
	PRINT("\n  Enter date [1-31] > ")
	jal	getbcd
	blt	v0, 1, read_date
	bgt	v0, 0x31, read_date
	sll	v0, 8
	or	s2, v0

read_day:
	PRINT("\n  Enter day (Sun=1) [1-7] > ")
	jal	getbcd
	blt	v0, 1, read_day
	bgt	v0, 7, read_day
	or	s2, v0

read_hour:
	PRINT("\n  Enter hour  [0-23] > 0\b")
	jal	getbcd
	blt	v0, zero, read_hour
	bgt	v0, 0x23, read_hour
	sll	v0, 16
	or	s3, v0
    
read_minute:
	PRINT("\n  Enter mins  [0-59] > 0\b")
	jal	getbcd
	blt	v0, zero, read_minute
	bgt	v0, 0x59, read_minute
	sll	v0, 8
	or	s3, v0

read_secs:
	PRINT("\n  Enter secs  [0-59] > 0\b")
	jal	getbcd
	blt	v0, zero, read_secs
	bgt	v0, 0x59, read_secs
	or	s3, v0

prompt_user:
#ifdef DEBUG1
	PRINT("\nSet TODC routine is called with : 0x")
	PUTHEX(s2);
	PRINT(" 0x");
	PUTHEX(s3);
#endif DEBUG1

call_set_tod:
	move	a0, s2		# call set tod routine.
	move	a1, s3
	jal	set_tod

read_tod:
	PRINT("\nNew date is ")
	jal	date

_date_done:
	move	v0, zero	# set normal return value
	j	s0
END(cmd_date)


#if	defined (RB3125) || defined (R3030)
/*
 * Read modify write of the status register. Toggles between swapped
 * and normal cache settings.
 */
LEAF(cmd_SwapCache)

		move	s0, ra			/* Save return address */

                .set noreorder
                mfc0    s1,C0_SR
                nop
                nop
                and     s2, s1, 0x20000         /* check swap cache bit */
                bne     s2, zero, 1f            /* swap already ? */
                or      s2,v0,0x20000           /* No, set swap bit */
		PRINT("\r\nCaches Swapped\r\n")
		nop
                b       2f
		nop
1:
                and     s2, v0, 0xfffdffff      /* clear swap bit */
		PRINT("\r\nCaches back to normal\r\n")
		nop
2:
                mtc0    s2,C0_SR                /* Set Clear bit */
                nop
                .set reorder
		move	v0, zero

                j       s0
END(cmd_SwapCache)
#endif	RB3125


#define PRINTERR(x, y, z) \
		/* Display address */\
		la	a0, _msg1; \
		jal	puts; \
		nop; \
		move	a0,x; \
		jal	puthex; \
		nop; \
		/* Display Expected data */\
		la	a0, _msg2; \
		jal	puts; \
		nop; \
		move	a0,y; \
		jal	puthex; \
		nop; \
		/* Display Actual */\
		la	a0, _msg3; \
		jal	puts; \
		nop; \
		move	a0,z; \
		jal	puthex; \
		nop; \
		la	a0, _crlf; \
		jal	puts; \
		nop;

/*
 * cmd_KH(nv_env *cmdptr, int cached)
 *
 * Run KH test either on memory or on cache.
 *
 * Command syntax: kh [-c] <start> <end> [loopcnt]
 *
 * s5 - cache-flag: set to run the test on cache.
 * s3 - start address
 * s4 - end address
 * s7 - loopcount: loop forever if loopcount==0
 */
LEAF(cmd_KH)
	move	s0, a1		# save cached flag

#if	defined (RB3125) || defined (R3030)
	/*
	 * get CACHE parameter from the command line
	 */
	li	a1, char_C	# Check for cache option -c
	jal	chkopt_nv	# get the option: default=="-c"
	move	s5,v0		# if zero, cache option not selected
#endif	RB3125

	/*
	 * get STARTING ADDRESS
	 */
	jal	aton_nv			# get addr
	bne	v1,zero, cmd_KH_end 	# v1!=0 if error
	move	s3,v0			# s3 is the starting address
	/*
	 * get ENDING ADDRESS
	 */
	jal	aton_nv			# get addr
	nop
	bne	v1,zero, cmd_KH_end 	# v1!=0 if error
	move	s4,v0			# s4 is the ending address

	/*
	 * Get LOOPCOUNT
	 */
	or	s7,zero,1	# set default loop count
	jal	aton_nv		# get loop count
	bne	v1,zero,1f	# if v1!=0 (error), use default
	move	s7,v0		# s7 is the unit count 
1:
	PRINT("\r\n\nMemory Knaizuk Hartmann Test ")

#if	defined (RB3125) || defined (R3030)
	beq	s5, zero, 1f	# branch if testing memory
	PRINT("\rData Cache Knaizuk Hartmann Test ")
#endif	RB3125

1:
	beq	s0, UNCACHED, 2f
	PRINT("< Cache I-Fetch >")
	la	s0, 2f
	and	s0, 0x9FFFFFFF
	j	s0		# cache i-fetch
2:
	PRINT("\r\nstart-addr=0x")
	PUTHEX(s3)
	PRINT(" end-addr=0x")
	PUTHEX(s4)
	PRINT(" loop-count= ")
	PUTUDEC(s7)
	PUTS(_crlf)

		sub	s4,8			# t4 has last address + 4,
						#   one word < last address to
						#   avoid bounds check
		
		move	t8,zero			# initialize error count
		move	t9,zero			# initialize pass count

		#PUSH2(t3, t4)			# save the test parameters
KH_loop:
		move	a0, s3
		jal	push_nvram
		move	a0, s4			
		jal	push_nvram

		#POP2(t3, t4)			# get the test parameters
		# Moved to the bottom
		#PUSH2(t3, t4)			# save the test parameters
						#    again
/*
 * Set partitions 1 and 2 to 0's.
 */
4:
		move	s1,zero			# set expected data
		addu	s2,s3,4			# initialize address pointer to
						#   partition 1
#if	defined (RB3125) || defined (R3030)
		.set	noreorder
		ISOLATECACHE(s6,v0,s5)		# s6 is saved sr value.
		.set	reorder
#endif	RB3125

$32:
		sw	s1,0(s2)		# write partition 1 data
		sw	s1,4(s2)		# write partition 2 data
		addu	s2,s2,12		# bump address 3 words to
						#   partition 1
		bleu	s2,s4,$32		# done yet?

/*
 * Set partition 0 to ones.
 */
		li	s1,-1			# set expected data
		move	s2,s3			# re-initialize pointer
$35:
		sw	s1,0(s2)		# write partition 0 data
		addu	s2,s2,12		# bump pointer address up 3
						#   words to partition 0 again
		bleu	s2,s4,$35		# done yet?

/*
 * Verify partition 1 is still 0's.
 */
		move	s1,zero			# set expected data
		addu	s2,s3,4			# re-initialize pointer to
						#   partition 1
$37:
		lw	s0,0(s2)		# read data
		beq	s0,s1,$38		# is it still 0?

		la	v0,3f
		j	v0
3:

#if	defined (RB3125) || defined (R3030)
		/*
		 * Restore status reg or we can't print if cache is 
		 * isolated
		 */
		.set	noreorder
		RESTORE_C0_SR(s6)
		.set	reorder
#endif	RB3125

		PRINTERR(s2, s1, s0)		# print address, expected,
						#   actual, and Xor
		b	$70
$38:
		addu	s2,s2,12		# bump address pointer 3 words
						#   to partition 1 again
		bleu	s2,s4,$37		# done yet?

/*
 * Set partition 1 to 1's.
 */
		li	s1,-1			# set expected data
		addu	s2,s3,4			# set address pointer to
						#   partition 1 start
$40:
		sw	s1,0(s2)		# write ones
		addu	s2,s2,12		# bump address pointer 3 words
						#   to partition 1 again
		bleu	s2,s4,$40		# done yet?

/*
 * Verify that partition 2 is 0's.
 */
		move	s1,zero			# set expected data
		addu	s2,s3,8			# re-initialize address pointer
						#   to partition 2
$42:
		lw	s0,0(s2)		# read location
		beq	s0,s1,$43		# is it still 0?

		la	v0,3f
		j	v0
3:

#if	defined (RB3125) || defined (R3030)
		/*
		 * Restore status reg or we can't print if cache is 
		 * isolated
		 */
		.set	noreorder
		RESTORE_C0_SR(s6)
		.set	reorder
#endif	RB3125

		PRINTERR(s2, s1, s0)		# print address, expected,
						#   actual, and xor
		b	$70
$43:
		addu	s2,s2,12		# bump address pointer 3 words
						#   to partition 2 again
		bleu	s2,s4,$42		# done yet?

/*
 * Verify that partitions 0 and 1 are still 1's.
 */
		li	s1,-1			# set expected data
		subu	s2,s2,4			# drop address pointer back to
						#   partition 1
		addu	s4,s4,4			# point reference address to
						#   "last" address
		bgtu	s2,s4,$131		# is partition 1 address >
						#   "last address"?

		sw	s1,0(s2)		# no! then write ones to
						#   partition 1 address
$131:
		subu	s4,s4,4			# put reference address back
		move	s2,s3			# re-init address pointer to
						#   partition 0
$45:
		lw	s0,0(s2)		# read partition 0 data
		bne	s0,s1,$46		# still 1's?

		lw	s0,4(s2)		# read partition 1 data
		beq	s0,s1,$47		# still 1's?

		addu	s2,s2,4			# bump address pointer 1 word
						#   for error
$46:
		la	v0,3f
		j	v0
3:

#if	defined (RB3125) || defined (R3030)
		/*
		 * Restore status reg or we can't print if cache is 
		 * isolated
		 */
		.set	noreorder
		RESTORE_C0_SR(s6)
		.set	reorder
#endif	RB3125

		PRINTERR(s2, s1, s0)		# print address, expected,
						#   actual, and xor
		b	$70
$47:
		addu	s2,s2,12		# bump address pointer 3 words
						#   to partition 0 again
		bleu	s2,s4,$45		# done yet?

/*
 * Set partition 0 to 0's.
 */
		move	s1,zero			# set expected data
		move	s2,s3			# re-initialize pointer to
						#   partition 0
$49:
		sw	s1,0(s2)		# write partition 0 data
		addu	s2,s2,12		# bump address pointer 3 words
						#   to partition 0 again
		bleu	s2,s4,$49		# done yet?

/*
 * Check partition 0 for 0's.
 */
		move	s2,s3			# re-initialize address pointer
						#   to partition 0
$51:
		lw	s0,0(s2)		# read partition 0 data
		beq	s0,s1,$52		# is it still 0?

		la	v0,3f
		j	v0
3:

#if	defined (RB3125) || defined (R3030)
		/*
		 * Restore status reg or we can't print if cache is 
		 * isolated
		 */
		.set	noreorder
		RESTORE_C0_SR(s6)
		.set	reorder
#endif	RB3125

		PRINTERR(s2, s1, s0)		# print address, expected,
						#   actual, and xor
		b	$70
$52:
		addu	s2,s2,12		# bump address pointer 3 words
						#   to partition 0 again
		bleu	s2,s4,$51		# done yet?
/*
 * Set partition 2 to 1's
 */
$53:
		li	s1,-1			# set data pattern
		addu	s2,s3,8			# re-initialize address pointer
						#   to partition 2
$54:
		sw	s1,0(s2)		# write partition 2 data
		addu	s2,s2,12		# bump address pointer 3 words
						#   to partition 2 again
		bleu	s2,s4,$54		# done yet?

/*
 * Check partition 2 for 1's.
 */
		addu	s2,s3,8			# re-initialize address pointer
						#   to partition 2
$56:
		lw	s0,0(s2)		# read partition 2 data
		beq	s0,s1,$57		# is it still 1's?

		la	v0,3f
		j	v0
3:

#if	defined (RB3125) || defined (R3030)
		/*
		 * Restore status reg or we can't print if cache is 
		 * isolated
		 */
		.set	noreorder
		RESTORE_C0_SR(s6)
		.set	reorder
#endif	RB3125

		PRINTERR(s2, s1, s0)		# print address, expected,
						#   actual, and xor
		b	$70
$57:
		addu	s2,s2,12		# bump address pointer 3 words
						#   to partition 2 again
		bleu	s2,s4,$56		# done yet?

/*
 * Check the last location
 */
		addu	s4,s4,4			# put last address back where
						#   it should be
		li	s1,0x55555555		# set data pattern
		sw	s1,0(s4)		# write it
		lw	s0,0(s4)		# read it
		beq	s0,s1,$60		# correct?
$59:
		la	v0,3f
		j	v0
3:

#if	defined (RB3125) || defined (R3030)
		/*
		 * Restore status reg or we can't print if cache is 
		 * isolated
		 */
		.set	noreorder
		RESTORE_C0_SR(s6)
		.set	reorder
#endif	RB3125

		PRINTERR(s4, s1, s0)		# print address, expected,
						#   actual, and xor
		b	$70
$60:
		li	s1,0xaaaaaaaa		# set data pattern
		sw	s1,0(s4)		# write it
		lw	s0,0(s4)		# read it
		bne	s0,s1,$59		# incorrect?

		move	s1,zero			# set last location to zero
		sw	s1,0(s4)		# write it

/*
 * Successful.
 */
		la	a0,success_msg
		b	1f
/*
 * Failed.
 */
$70:
		addu	t8,t8,1			# increment error count
		la	a0,xfailed_msg
1:

#if	defined (RB3125) || defined (R3030)
		/*
		 * Restore status reg or we can't print if cache is 
		 * isolated
		 */
		.set	noreorder
		RESTORE_C0_SR(s6)
		.set	reorder
#endif	RB3125

		jal	puts
		addu	t9,t9,1			# increment pass count
		move	a0,t9
		jal	puthex
		la	a0,err_count_msg
		jal	puts
		move	a0,t8
		jal	puthex
		PUTS(_crlf)

		jal	pop_nvram		# restore test parameters
		nop
		move	s4, v0
		jal	pop_nvram
		nop
		move	s3, v0
	/*
	 * loopcount
	 */
	beq	s7, zero, KH_loop	# loop forever if zero
	beq	s7, 1, cmd_KH_end
	subu	s7, 1
	b	KH_loop			# repeat test

cmd_KH_end:
	move	v0, zero
	j	warm_reset
END(cmd_KH)

/*
 * cmd_aina(nv_env *cmdptr, int cached)
 *
 * addr-in-addr memory test
 *
 * command syntax: aina [-c] <start> <end> [loopcount]
 *
 * option:
 *	c	- isolate the data cache (to test d-cache).
 *
 * Loop forever if loopcount==0.
 *
 * register usage:
 *	s0 - start address
 *	s1 - end address
 *	s2 - loop count
 *	s3 - "cached" flag
 *	s4 - option c
 *	s6 - saved ra
 */
LEAF(cmd_aina)
	move	s6, ra
	move	s3, a1		# save "cached" flag

#if	defined (RB3125) || defined (R3030)
	/*
	 * check for CACHE option from the command line
	 */
	li	a1, char_C	# Check for cache option -c
	jal	chkopt_nv	# get the option: default=="-c"
	move	s4,v0		# if zero, cache option not selected
#endif	RB3125

	/*
	 * get STARTING ADDRESS
	 */
	jal	aton_nv			# get addr
	bne	v1,zero, cmd_aina_end 	# v1!=0 if error
	move	s0,v0			# s0 is the start address
	/*
	 * get ENDING ADDRESS
	 */
	jal	aton_nv			# get addr
	nop
	bne	v1,zero, cmd_aina_end 	# v1!=0 if error
	move	s1,v0			# s1 is the end address
	/*
	 * Get LOOPCOUNT
	 */
	or	s2,zero,1	# set default loop count
	jal	aton_nv		# get loop count
	bne	v1,zero,1f	# if v1!=0 (error), use default
	move	s2,v0		# s2 is the unit count 
1:
	PRINT("\r\n\nMemory Address-in-Address Test ")

#if	defined (RB3125) || defined (R3030)
	beq	s4, zero, 1f	# branch if testing memory
	PRINT("\rData Cache Address-in-Address Test ")
#endif	RB3125

1:
	beq	s3, UNCACHED, 1f
	PRINT("< Cache I-Fetch >")
	la	s3, 1f
	and	s3, 0x9FFFFFFF
	j	s3			# cache i-fetch
1:
	PRINT("\r\nstart-addr=0x")
	PUTHEX(s0)
	PRINT(" end-addr=0x")
	PUTHEX(s1)
	PRINT(" loop-count= ")
	PUTUDEC(s2)

#if	defined (RB3125) || defined (R3030)
	.set	noreorder
	ISOLATECACHE(v0,v1,s4)
	.set	reorder
#endif	RB3125

	move	a0, s0		# pass start-addr
	move	a1, s1		# pass end-addr
	move	a2, s2		# pass loopcnt
	jal	aina_test

#if	defined (RB3125) || defined (R3030)
	.set	noreorder
	mfc0	v0, C0_SR	# clear SR_ISC if it was set.
	nop
	and	v0, ~SR_ISC
	mtc0	v0, C0_SR
	nop
	.set	reorder
#endif	RB3125

	move	v0, zero
	j	s6

cmd_aina_end:
	li	v0, 1		# cmd error
	j	s6
END(cmd_aina)


#ifdef	RB3125
/*
 * int cmd_memconfig()
 *
 * Re-configure the memory boards.
 */
LEAF(cmd_memconfig)
	move	s7, ra		# save ra
	PRINT("\nRe-configuring memory boards... ")
	jal	config_mem	# returns size in v1, number of boards in v0.
	move	s0, v0
	srl	v1, 20		# shift to meg-bytes
	PUTUDEC(v1)
	PRINT(" MB & ")
	PUTUDEC(s0)
	PRINT(" memory board(s)")
	move	v0, zero
	j	s7
END(cmd_memconfig)

#endif	RB3125

/*
 * int cmd_find(nv_env *cmdptr)
 *
 * Find all memory locations that contains the data specified.
 *
 * Command syntax:
 *	find [-v] <data> [<start_addr> <end_addr>]
 *
 * Option: v	- Search from the VME side assuming VME address base is
 *		is 0x10000000.
 *
 * If no address range is given, search all memory locations.
 *
 * register usage:
 *	s0	- start address
 *	s1	- end address
 *	s2	- data to search for
 *	s4	- vme option
 *	s7	- return address
 */
LEAF(cmd_find)
	move	s3, a0
	move	a0, ra
	jal	push_nvram
	move	a0, s3
	#move	s7, ra
	move	s3, a1		# save the "all_memory" flag

#ifdef	RB3125
	/*
	 * get VME option from the command line
	 */
	li	a1, char_V	# Check for cache option -c
	jal	chkopt_nv	# get the option: default=="-c"
	move	s4,v0		# if zero, vme side option not selected
#endif	RB3125

	/*
	 * get data to search for
	 */
	jal	aton_nv		# get data
	bne	v1,zero,99f	# v1!=0 if error
	move	s2,v0		# s1 is the data

	/*
	 * get start address
	 */
	jal	aton_nv		# get data
	bne	v1,zero,1f	# if v1!=0 then search all locations
	move	s0,v0		# s0 is the start address

	/*
	 * get end address
	 */
	jal	aton_nv		# get data
	bne	v1,zero,99f	# v1!=0 if error
	move	s1,v0		# s1 is the end address
	bgtu	s0, s1, 99f	# sanity check: start must be less than end.
	j	2f
1:
	jal	SizeMemory	# find the memory size
	li	s0, K1BASE	# first address of memory
	or	s1, s0, v0	# last address of memory
2:
#ifdef	RB3125
	beq	s4, zero, 1f	# branch if not vme side
	/*
	 * assuming MEM_ADDR on all memory boards have been set
	 * to select 0x10000000 as the vme base address.
	 */
	or	s0, (1<<28)	# set address bit 28
	or	s1, (1<<28)	# set address bit 28
#endif	RB3125

1:
	PRINT("\nFind 0x")
	PUTHEX(s2)
	PRINT(" between 0x")
	PUTHEX(s0)
	PRINT(" and 0x")
	PUTHEX(s1)
	PRINT("\nAddresses found:\n")
1:
	lw	a0, (s0)	# read word
	addu	s0, 4
	bne	a0, s2, 2f

	subu	v0, s0, 4
	PUTHEX(v0)		# found
	PUTS(_crlf)
2:
	blt	s0, s1, 1b

	jal	pop_nvram
	move	s0, v0
	move	v0, zero

	j	s0
99:
	jal	pop_nvram
	move	s0, v0
	li	v0, 1		# syntax error
	j	s0
END(find)


/*
 * int cmd_walk(char *env_nvptr, int parm1)
 *
 * write walking 1 or walking zero to location specified. Then reread
 * location and compare the data. The menu passes a zero for a walking
 * 1 and a one for a walking 0.
 *
 * command syntax: "walk1 -[whb] <addr> [-m mask] [loopcount]"
 * command syntax: "walk0 -[whb] <addr> [-m mask] [loopcount]"
 *
 * options: 
 *	loopcount - default=1, 0-loop forever
 *	transfer size- w=word, h=halfword, b=byte
 *	mask - mask used before the write.
 *
 * registers used:
 *	a0 - nv string ptr
 * 	s0 - saved ra
 *	s1 - address
 *	s2 - data mask
 *	s3 - loopcount (default = 1)
 *	s4 - size of write (word, halfword, byte)
 *	s5 - passed from table, 0=walking 1, 1=walking zero and loop count
 *	s6 - walking pattern
 *	s7 - actual data read
 *	t7 - data pattern generated from s6
 */
LEAF(cmd_walk)
	.set	noreorder
	move	s0,ra		# save ra in s0
	move	s5,a1		# Save which test we are running

	/*
	 * get WORD SIZE parameter from the command line
	 */
	jal	getopt_nv	# get the option: default=="-w"
	nop
	move	s4,v0		# s4 is the option: 0==default= WORD access

	/*
	 * get ADDRESS to write
	 */
	jal	aton_nv		# get addr
	nop
	bne	v1,zero,2f	# v1!=0 if error
	move	s1,v0		# (BDSLOT) s1 is the address

	/*
	 * Get MASK
	 */
	li	a1,char_M	# check for option -m
	jal	chkopt_nv	#
	not	s2,zero		# (BDSLOT) set default mask in s2
	beq	v0,zero,6f	# branch if no mask option specified
	nop                     #  ELSE
	jal	aton_nv		# get the user defined mask
	nop
	bne	v1,zero,2f	# v1!=0 if error
	move	s2,v0		# (BDSLOT) save mask in s2
6:
	/*
	 * Get LOOPCOUNT
	 */
	jal	aton_nv		# get loop count
	nop
	bne	v1,zero,1f	# if v1!=0 (error), use default
	or	s3,zero,1	# (BDSLOT) set default loop count
	move	s3,v0		# s3 is the unit count 
1:
	/*
	 * Determine initial bit pattern
	 * walking 1: 0x00000001  walking 0: 0xFFFFFFFE
	 */
	beq	s5,zero,1f	# s5==0 if walking 1
	li	s6,1		# (BDSLOT) walking 1 pattern
	not	s6		# walking 0 pattern
1:
	PRINT("\r\naddr 0x")
	PUTHEX(s1)
	PRINT(", bit pattern 0x")
	PUTHEX(s6)
	PRINT(", bit mask 0x")
	PUTHEX(s2)
	bne	s3, zero, 1f	# branch if not loop forever
	nop
	PRINT("\nLoop until system reset...")
1:
	/*
	 * Determine transfer size
	 */
	beq	s4,zero,wlk_word	# zero -> option==default
	nop
	beq	s4,char_W,wlk_word	# "-w"
	nop
	beq	s4,char_H,wlk_half	# "-h"
	and	s2,0x0000FFFF		# (BDSLOT) mask the upper half word
	beq	s4,char_B,wlk_byte	# "-b" 
	and	s2,0x00FF		# (BDSLOT) keep only the LSB
	b	2f			# branch if illegal option found
	nop

wlk_word:
	/*
	 * WORD MODE SELECTED
	 */
	move	s4, zero	# bit loop count
word_lp:
	and	t7, s6, s2	# mask out unwanted bits

#ifdef RICHDBG
	PRINT("\r\naddr 0x")
	PUTHEX(s1)
	PRINT(", data 0x")
	PUTHEX(t7)
	PRINT(", mask 0x")
	PUTHEX(s2)
	PRINT(", initial pattern ")
	PUTHEX(s6)
#endif RICHDBG

	sw	t7, (s1)	# store the data
	nop
	nop
	lw	s7, (s1)	# read it back
	nop
	beq	s7, t7, 64f	# check data, branch if ok
	nop
	/*
	 * DISPLAY ERROR DATA 
	 */
	PRINT("\r\naddr 0x")
	PUTHEX(s1)
	PRINT(", exp 0x")
	PUTHEX(t7)
	PRINT(", act 0x")
	PUTHEX(s7)
	PRINT(", xor 0x")
	xor	t0, t7, s7
	PUTHEX(t0)
64:
	rol	s6, s6, 1	# slide the bit
	add	s4, 1		# next bit
	blt	s4, 32, word_lp	# walk all 32 bits
	nop
	beq	s3, zero, wlk_word	# loop forever
	nop
	subu	s3, 1		# decr loop count
	bne	s3, zero, wlk_word
	nop
	j	s0		# return
	move	v0, zero	# (BDSLOT)

wlk_half:
	/*
	 * HALFWORD OPTION SELECTED
	 */
	move	s4, zero	# bit loop count
hword_lp:
	and	t7, s6, s2	# mask out unwanted bits
	sh	t7, (s1)	# store the data
	nop
	nop
	lhu	s7, (s1)	# read it back
	nop
	beq	s7, t7, 64f	# check data, branch if ok
	nop
	/*
	 * DISPLAY ERROR DATA 
	 */
	PRINT("\r\naddr 0x")
	PUTHEX(s1)
	PRINT(", exp 0x")
	PUTHEX(t7)
	PRINT(", act 0x")
	PUTHEX(s7)
	PRINT(", xor 0x")
	xor	t0, t7, s7
	PUTHEX(t0)
64:
	rol	s6, s6, 1	# slide the bit
	add	s4, 1		# next bit
	blt	s4, 16, hword_lp # walk all 16 bits
	nop
	ror	s6, 16		# shift back to initial bit pattern
	beq	s3, zero, wlk_half	# loop forever
	nop
	subu	s3, 1		# decr loop count
	bne	s3, zero, wlk_half
	nop
	j	s0		# return
	move	v0, zero	# (BDSLOT)

wlk_byte:
	/*
	 * BYTE MODE SELECTED
	 */
	move	s4, zero	# bit loop count
byte_lp:
	and	t7, s6, s2	# mask out unwanted bits
	sb	t7, (s1)	# store the data
	nop
	nop
	lbu	s7, (s1)	# read it back
	nop
	beq	s7, t7, 64f	# check data, branch if ok
	nop
	/*
	 * DISPLAY ERROR DATA 
	 */
	PRINT("\r\naddr 0x")
	PUTHEX(s1)
	PRINT(", exp 0x")
	PUTHEX(t7)
	PRINT(", act 0x")
	PUTHEX(s7)
	PRINT(", xor 0x")
	xor	t0, t7, s7
	PUTHEX(t0)
64:
	rol	s6, s6, 1	# slide the bit
	add	s4, 1		# next bit
	blt	s4, 8, byte_lp	# walk all 8 bits
	nop
	ror	s6, 8		# shift back to initial bit pattern
	beq	s3, zero, wlk_byte	# loop forever
	nop
	subu	s3,1		# decr loop count
	bne	s3, zero, wlk_byte
	nop
	j	s0		# return
	move	v0, zero	# (BDSLOT)
2:
	j	s0		# error return
	or	v0,zero,1
	.set	reorder
END(cmd_walk)



/*		d m o n _ m e m _ i n i t()
 *
 * dmon_mem_init()- Is a wrapper that calls the product specific
 * memory config routine. 
 */
LEAF(dmon_mem_init)

	/* Only used if we fall to the bottom */
	move	s4, ra	

#if	defined(IP24) || defined(IP22)

	/*
	 * We do a jump here so as not to use any more of the registers
	 */
	j	meminit_indy

#endif /* IP24 || IP22 */

	/* should never get here */
	PRINTF("dmon_mem_init- no function declared\n\r");
	j	s4

END(dmon_mem_init)

/* end */
