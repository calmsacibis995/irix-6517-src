#ident	"$Id: handler.s,v 1.1 1994/07/21 00:18:09 davidl Exp $"
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
#include "sys/fpregdef.h"
#include "sys/asm.h"
#include "sys/sbd.h"
#include "saioctl.h"

#include "pdiag.h"
#include "monitor.h"

	.globl	warm_reset
	.globl _test_return

	.text
	.align	2

/*
 * void InstallHandler(int *HandlerPtr())
 *
 * Install the user exception handler for all types of exception.
 * 
 * Note: The exception vector is always reset to zero on exception.
 */
LEAF(InstallHandler)
	move	VectorReg,a0	# Install the vector in VectorReg
	j	ra
END(InstallHandler)


/*
 * void RemoveHandler(void)
 *
 * Remove (clear) the global exception vector.
 */
LEAF(RemoveHandler)
	move	VectorReg,zero
	j	ra
END(RemoveHandler)


/*------------------------------------------------------------------------+
| Routine  : void _bev_handler (int excpt_type)                           |
| Function : This is  a general exception vector in PROM space.  UTLB and |
|   genenratl exception vector to here when the BEV bit in the status reg |
|   is set to 1.                                                          |
+------------------------------------------------------------------------*/

LEAF(_bev_handler)

	.set	noreorder
	.set	noat
	la	Bevhndlr, _bev_handler	# always capture Bev exception

	beq	VectorReg, zero, 1f	# any user handler installed?
	nop
	j	VectorReg
	move	VectorReg, zero		# (BDSLOT) clear vector on exception
	.set	at
1:
	/*
	 * Immediately read C0/BC states into s0-s6 so that they are close 
	 * to the true states in which the exception occured.
	 */
	mfc0	s0, C0_SR
	mfc0	s1, C0_EPC
	mfc0	s2, C0_CAUSE
	move	s3, ra
	mfc0	s5, C0_BADVADDR
#ifndef SABLE
	mfc0	s4, C0_ECC
#endif
	mfc0	s6, C0_CACHE_ERR
	mfc0	s7, C0_ERROR_EPC
	.set	reorder
	
_disp_imsg:
	bne	ExceptReg, EXCEPT_UTLB, 1f # type of exception (may be bogus)
	la	a0, utlb_msg
	j	_disp_code
1:
	bne	ExceptReg, EXCEPT_XTLB, 1f
	la	a0, xtlb_msg
	j	_disp_code
1:
	bne	ExceptReg, EXCEPT_CACHE, 1f
	la	a0, cache_err_msg
	jal	puts
	j	_disp_regs
1:
	la	a0, excpt_msg		# general exception msg

_disp_code:
	jal	puts			# display header message

	move	t0, s2			# determine the cause and
	andi	t0, 0x03c		#  display the type of exception.
	la	a0, excpt_code
	addu	a0, t0
	lw	a0, (a0)
	jal	puts

_disp_regs:
	PRINT("\n  STATUS      : 0x")
	PUTHEX(s0)
	PRINT("  EPC         : 0x")
	PUTHEX(s1)
	PRINT("\n  CAUSE       : 0x")
	PUTHEX(s2)
	PRINT("  R31/RA      : 0x")
	PUTHEX(s3)
	PRINT("\n  ECC         : 0x")
	PUTHEX(s4)
	PRINT("  CACHE_ERR   : 0x")
	PUTHEX(s6)
	PRINT("\n  ERROR EPC   : 0x")
	PUTHEX(s7)
	PRINT("\n  PRID        : 0x")
	.set	noreorder
	mfc0	t0, C0_PRID
	nop
	.set	reorder
	PUTHEX(t0)
	PRINT("  BADVADDR    : 0x")
	PUTHEX(s5)

	/*
	 * PRINT FPA status registers if SR_CU1 set
	 */
	and	t0, s0, SR_CU1
	beq	t0, zero, 1f		# SR_CU1 set?

	PRINT("\n  FPA/REV:      0x")
	.set	noreorder
	cfc1	t0, zero
	nop
	.set	reorder
	PUTHEX(t0)

	PRINT("  FPA/CSR     : 0x")
	.set	noreorder
	cfc1	t0, fcr31
	nop
	.set	reorder
	PUTHEX(t0)
1:
	/*
	 * Clear some error/status bits in C0_SR
	 */
	and	t0, s0, SR_CU1|SR_BEV
	or	t0, SR_BEV		# enable BEV vector

	.set	noreorder
	mtc0	t0, C0_SR
	mtc0	zero, C0_CAUSE		# clear cause register
        nop
	.set	reorder


	la	a0, _crlf
	jal	puts
	la	a0, _crlf
	jal	puts
	jal	date			# print current time 

   	jal	_pause			# pause until a key entered

   	li	a0, _CNFGSTATUS
   	jal	lw_nvram
   	and	t0, v0, TAFLAG_MASK	# check if any test active

   	beq  	t0, zero, 1f		# if NO, do warm reset

   	li	v0,1			# set v0 to 1 as error indicator
	.set	noreorder
   	j	_test_return
	c0	C0_RFE
	.set	reorder
1:
	.set	noreorder
   	j	warm_reset		# else return to RunTests()
	c0	C0_RFE
	.set	reorder
END(_bev_handler)

/*
 * void _intr_jmp(void)
 *
 * Ctrl-C is hard wired to here from putchar()
 */
LEAF(_intr_jmp)
   	li	a0, _CNFGSTATUS
   	jal	lw_nvram
   	and	t0, v0, TAFLAG_MASK	# check if any test active

   	beq  	t0, zero, 1f		# if NO, do warm reset

	la	a0, _crlf
	jal	puts
	la	a0, _crlf
	jal	puts
	li	a0, _MODNAME		# print current module name
	jal	lw_nvram
	move	a0, v0
	jal	puts
	PRINT(" - ABORTED by Ctl-C. Laps = ")
	li	a0, _PASSCNT		# print pass count
	jal	lw_nvram
	PUTUDEC(v0)
	PRINT("  Total Errors = ") 
	li	a0, _ERRORCNT
	jal	lw_nvram 		# get previous error count
	PUTUDEC(v0)

	la	a0, _crlf
	jal	puts
1:
   	j	warm_reset		# just do warm_reset
END(_intr_jmp)


/*------------------------------------------------------------------------+
| test messages                                                           |
+------------------------------------------------------------------------*/

	.data
	.align	2

	.globl	excpt_code

excpt_code:
	.word	ext_int
	.word	mod_int
	.word	tlbl_int
	.word	tlbs_int
	.word	adel_int
	.word	ades_int
	.word	ibe_int
	.word	dbe_int
	.word	sys_int
	.word	bp_int
	.word	ri_int
	.word	cpu_int
	.word	ovf_int
	.word	trap_int
	.word	vcei_int
	.word	fpe_int
	.word	excpt_16
	.word	excpt_17
	.word	excpt_18
	.word	excpt_19
	.word	excpt_20
	.word	excpt_21
	.word	excpt_22
	.word	watch_int
	.word	excpt_24
	.word	excpt_25
	.word	excpt_26
	.word	excpt_27
	.word	excpt_28
	.word	excpt_29
	.word	excpt_30
	.word	vced_int

	.align	2

utlb_msg:	.asciiz	"\n\nUTLB-Miss Exception: "
xtlb_msg:	.asciiz "\n\nXTLB_Miss Exception: "
cache_err_msg:	.asciiz "\n\nCACHE ERROR Exception: "
excpt_msg:	.asciiz	"\n\nUnexpected Exception: "
ext_int:	.asciiz	"0 - INT: External Interrupt"
mod_int:	.asciiz	"1 - MOD: TLB Modification Exception"
tlbl_int:	.asciiz	"2 - RMISS: TLB Miss Exception (load/I-fetch)"
tlbs_int:	.asciiz	"3 - WMISS: TLB Miss Exception (store)"
adel_int:	.asciiz	"4 - RADE: Address Error Exception (load/I-fetch)"
ades_int:	.asciiz	"5 - WADE: Address Error Exception (store)"
ibe_int:	.asciiz	"6 - IBE: Bus Error Exception (I-fetch)"
dbe_int:	.asciiz	"7 - DBE: Bus Error Exception (data load/store)"
sys_int:	.asciiz	"8 - SYSCALL: SysCall Exception"
bp_int:		.asciiz	"9 - BREAK: Break Point Exception"
ri_int:		.asciiz	"10 - RI: Reserved Instruction Exception"
cpu_int:	.asciiz	"11 - CpU: Coprocessor Unusable"
ovf_int:	.asciiz	"12 - OV: Arithmetic Overflow"
trap_int:	.asciiz	"13 - TRAP: Trap Exception (MIPS II)"
vcei_int:	.asciiz	"14 - VCEI: Virtual Coherency Exception Instruction"
fpe_int:	.asciiz	"15 - FPE: Floating Point exception (R4000)" 
excpt_16:	.asciiz	"16 - Reserved Exception Code" 
excpt_17:	.asciiz	"17 - Reserved Exception Code" 
excpt_18:	.asciiz	"18 - Reserved Exception Code" 
excpt_19:	.asciiz	"19 - Reserved Exception Code" 
excpt_20:	.asciiz	"20 - Reserved Exception Code" 
excpt_21:	.asciiz	"21 - Reserved Exception Code" 
excpt_22:	.asciiz	"22 - Reserved Exception Code" 
watch_int:	.asciiz	"23 - WATCH: WatchHi/WatchLo address referenced"
excpt_24:	.asciiz	"24 - Reserved Exception Code" 
excpt_25:	.asciiz	"25 - Reserved Exception Code" 
excpt_26:	.asciiz	"26 - Reserved Exception Code" 
excpt_27:	.asciiz	"27 - Reserved Exception Code" 
excpt_28:	.asciiz	"28 - Reserved Exception Code" 
excpt_29:	.asciiz	"29 - Reserved Exception Code" 
excpt_30:	.asciiz	"30 - Reserved Exception Code" 
vced_int:	.asciiz	"31 - Virtual Coherency Exception Data(R4000)"

