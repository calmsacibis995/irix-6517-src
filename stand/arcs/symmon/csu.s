#ident "symmon/csu.s: $Revision: 1.58 $"
/*
 *
 * Copyright 1985 by MIPS Computer Systems, Inc.
 *
 * csu.s -- dbgmon startup code (independently loaded version)
 */
#include <regdef.h>
#include <sys/sbd.h>
#include <asm.h>
#include "dbgmon.h"
#include "mp.h"
#include <sys/loaddrs.h>

#if EVEREST && SABLE
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/EVEREST/gda.h>
#include <sys/i8251uart.h>
#endif

#if SN0 && SABLE
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/SN/gda.h>
#include <sys/i8251uart.h>
#endif

#if IP30
#include <sys/RACER/gda.h>
#endif


/*
 * dbgmon entry point
 * dbgmon expects to be called:
 *	dbgmon(argc, argv, environ)
 * where argc, and argv are the arguments passed to the process
 * being debugged.  environ is a pointer to the current environment.
 * dbgmon assumes that the stack it is called on is the stack to use
 * for the process being debugged and that the gp register on entry
 * is the clients gp.  dbgmon itself runs on a stack above its bss.
 */
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
LOCAL_SIZE=	2				# sp, gp
STARTFRM=	FRAMESZ((NARGSAVE+LOCAL_SIZE)*SZREG)
ARGC_OFFSET=	STARTFRM
ARGV_OFFSET=	STARTFRM+(1*SZREG)
ENVP_OFFSET=	STARTFRM+(2*SZREG)
SP_OFFSET=	STARTFRM-(1*SZREG)
GP_OFFSET=	STARTFRM-(2*SZREG)
#elif (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _ABIN32)
LOCAL_SIZE=6					# a0..a3, sp, gp
STARTFRM=	FRAMESZ((NARGSAVE+LOCAL_SIZE)*SZREG)
SP_OFFSET=	STARTFRM-(1*SZREG)
GP_OFFSET=	STARTFRM-(2*SZREG)
ARGC_OFFSET=	STARTFRM-(3*SZREG)
ARGV_OFFSET=	STARTFRM-(4*SZREG)
ENVP_OFFSET=	STARTFRM-(5*SZREG)
#endif
#if IP32
BSS(_ip32_config_reg, 4)        
BSS(_ip32_config_reg2, 4)        
#endif

	.globl	_dbgstack
	.text
	.align 9
NESTED(start, STARTFRM, zero)
	.set noreorder
#if SN0 && SABLE
	b	1f			# normal symmon start
	li	t0,1			# non-zero instruction used as flag
	b	sable_symmon_slave_loop
	nop
	b	sable_symmon_slave_launch
	nop
1:		
#endif /* SABLE */	
	LI	t0,SYMMON_SR
#if SN0
	or	t0,SR_BEV
#endif
	MTC0(t0,C0_SR)
	nop
	.set reorder
	move	v0,gp
	LA	gp,_gp
#ifdef IP19
	/* t0 == current C0_SR */	

	/* On second-NMI we're supposed to enter POD mode.  But the IP19
	 * cpu proms assume that FP register 3 points to the epcuart.
	 * In order to make this work, we save the prom's FP register 3
	 * so that it can be reloaded on NMI.
	 */

	.set noreorder
	LI	v1, (SR_CU1|SR_FR)
	or	v1,t0
	MTC0(v1,C0_SR)
	nop
	nop
	nop
	nop
	mfc1	v1, $f3
	nop
	nop
	nop
	nop
	sd	v1, pod_nmi_fp3
	
	MTC0(t0,C0_SR)
	nop
	.set reorder
#endif /* IP19 */		
	/* switch stacks */
	PTR_L	s0,_dbgstack		# boot processor's stack
	PTR_SUBU s0,SYMMON_STACK_SIZE/2	# 1/2 of stack is for fault handler
	/* save sp, (gp in v0), a0-3 on stack */
	PTR_S	sp,-SZREG(s0)
	move	sp,s0
	PTR_SUBU sp,STARTFRM		# old sp is now on STARTFRM-4(sp)
	PTR_S	v0,GP_OFFSET(sp)	# old gp
	PTR_S	a0,ARGC_OFFSET(sp)	# argc
	PTR_S	a1,ARGV_OFFSET(sp)	# argv
	PTR_S	a2,ENVP_OFFSET(sp)	# environ
#if _K64PROM32
	jal	swizzle_SPB
#endif
	jal	config_cache
#ifdef IP22
	.set noreorder
	jal	_r4600sc_cache_on_test	# find out if SC cache on and set global
	nop
	lw	t0,_r4600sc_cache_on	# read global
	beq     t0,zero,1f
	nop
	jal	_r4600sc_disable_scache # make sure its off now
	nop
	.set reorder
1:
#endif
#if IP32
	jal	_ip32_disable_serial_dma # and save state 
	.set	noreorder
	mfc0	t0,C0_PRID
	NOP_0_4
	.set reorder
	andi	t0,C0_IMPMASK
	subu	t0,(C0_IMP_R5000 << C0_IMPSHIFT)
	beq	t0,zero,2f
	nop
	subu    t0,((C0_IMP_RM5271 << C0_IMPSHIFT) - (C0_IMP_R5000 << C0_IMPSHIFT))
	beq	t0,zero,2f
	nop
	.set noreorder
	mfc0	t0,C0_CONFIG
	NOP_0_4
	.set reorder
	sw	t0,_ip32_config_reg
	andi	t0,CONFIG_K0
	subu	t0,CONFIG_NONCOHRNT
	blez	t0,2f			# if K0 uncached or cached non-coherent leave alone
	lw	t0,_ip32_config_reg
	li	t1,~CONFIG_K0
	and	t0,t1
	ori	t0,CONFIG_NONCOHRNT	# else set K0 to cached non-coherent

	.set noreorder
	mtc0	t0,C0_CONFIG
	NOP_0_4
	.set reorder

2:
#endif
#ifndef SABLE
	jal	flush_cache		# just to be sure
#endif
#if (EVEREST || SN0) && SABLE
#if EVEREST
	jal	ccuart_init
#endif
#if SN0
	jal	hubuart_init
#endif
	jal	sysinit
	/* setup some reasonable values... */
#if IP19 || IP25
#ifdef IP19
	LI	k0, EV_BE_LOC
	LI	k1, EAROM_BE_MASK	# Big endian
	sd	k1, 0(k0)
#endif
	LI	k0, EV_CACHE_SZ_LOC
	LI	k1, 22			# log2(4mb) = 22
	sd	k1, 0(k0)
#endif

#if !defined(SN0) && !defined(SN0S)
	LI	t0, GDA_ADDR
	LI	t1, GDA_MAGIC		# Load actual magic number
	sw	t1, G_MAGICOFF(t0)	# Store magic number
	sw	zero, G_PROMOPOFF(t0)	# Set actual operation
#endif /* !SN0 */

	LI	t0, 1
	LA	t1, sk_sable		# set sable flag for libsk
	sw	t0, 0(t1)

	LA	t1, sc_sable		# set sable flag for libsc
	sw	t0, 0(t1)
#endif	/* (EVEREST || SN0) && SABLE */
#if (IP30 && SABLE)
	LI	t0, GDA_ADDR
	LI	t1, GDA_MAGIC		# Load actual magic number
	sw	t1, G_MAGICOFF(t0)	# Store magic number
	sw	zero, G_PROMOPOFF(t0)	# Set actual operation
#endif

	move	a0,sp			# so mpsetup can set up private._regs
	jal	mpsetup			# wired pda, set exception vector	
	move	a0,zero			# hack to inval PDA entry
	jal	invaltlb

#if IP19 || IP25 || IP27
	.globl	_prom
	LA	a0, _prom		# Tells symmon to initialize ARCS stuff
	li	a1, 1
	sw	a1, 0(a0)		# Turn it on
#endif

#if _K64PROM32
	.set	noreorder
	LA	a0,1f			# jump out of compatability space
	jr	a0
	nop
	nop
	.set	reorder
1:
#define initenv		k64p32_initenv
#define initargv	k64p32_initargv
#endif

	/* copyin argv and environ
	 */
	PTR_ADDU a0,sp,ENVP_OFFSET	# address of envp
	jal	initenv
	PTR_ADDU a0,sp,ARGC_OFFSET	# address of argc
	PTR_ADDU a1,sp,ARGV_OFFSET	# address of argv
	jal	initargv

	/* Some system (like Everest) always want to run the debugger
	 * on the console serial port.  Init_consenv knows that if no
	 * graphics devices are configured, it should just use the serial
	 * port.  In order for this to work, though, we need to explicitly
	 * re-initialize the console environment variables, thus...
	 */
	move	a0, zero
	jal	init_consenv

#if (IP30 && SABLE)
	jal	sableinit		# initialize environment
#endif

	jal	_init_saio

	PTR_L	a0,ARGC_OFFSET(sp)	# argc
	PTR_L	a1,ARGV_OFFSET(sp)	# argv
	PTR_L	a2,ENVP_OFFSET(sp)	# environ
	jal	_dbgmon			# enter debug monitor
	/* shouldn't return, but just in case */
	j	_quit
	END(start)

#if EVEREST && SABLE
/*
 * Routine ccuart_init
 *	Initializes the CC chip UART by writing three NULLS to
 *	get it into a known state, doing a soft reset, and then
 *	bringing it into ASYNCHRONOUS mode.  
 * 
 * Arguments:
 *	None.
 * Returns:
 * 	Nothing.
 * Uses:
 *	a0, a1.
 */
#define		CMD	0x0
#define 	DATA	0x8

LEAF(ccuart_init) 
	.set	noreorder
	LI	a1, EV_UART_BASE
	sd	zero, CMD(a1)		# Clear state by writing 3 zero's
	nop
	sd	zero, CMD(a1)
	nop
	sd	zero, CMD(a1)
	LI	a0, I8251_RESET		# Soft reset
	sd	a0, CMD(a1)
	LI	a0, I8251_ASYNC16X | I8251_NOPAR | I8251_8BITS | I8251_STOPB1
	sd	a0, CMD(a1)
	LI	a0, I8251_TXENB | I8251_RXENB | I8251_RESETERR
	j	ra 
	sd	a0, CMD(a1)		# (BD)
	.set	reorder
	END(ccuart_init)

#endif	/* EVEREST && SABLE */


#if SN0 && SABLE
/*
 * Routine hubuart_init
 *	Initializes the hub chip UART by writing three NULLS to
 *	get it into a known state, doing a soft reset, and then
 *	bringing it into ASYNCHRONOUS mode.  
 * 
 * Arguments:
 *	None.
 * Returns:
 * 	Nothing.
 * Uses:
 *	a0, a1.
 */
#define		CMD	0x0
#define 	DATA	0x8

LEAF(hubuart_init) 
	.set	noreorder
	LI	a1, KL_UART_BASE
	sd	zero, CMD(a1)		# Clear state by writing 3 zero's
	nop
	sd	zero, CMD(a1)
	nop
	sd	zero, CMD(a1)
	LI	a0, I8251_RESET		# Soft reset
	sd	a0, CMD(a1)
	LI	a0, I8251_ASYNC16X | I8251_NOPAR | I8251_8BITS | I8251_STOPB1
	sd	a0, CMD(a1)
	LI	a0, I8251_TXENB | I8251_RXENB | I8251_RESETERR
	j	ra 
	sd	a0, CMD(a1)		# (BD)
	.set	reorder
	END(hubuart_init)

	/* Since there is no prom waiting to receive launch commands, the
	 * slaves come here and go to an idle loop in symmon waiting for
	 * launch commands.
	 * Caller (kernel) has setup a valid "sp" for us to use.
	 */
LEAF(sable_symmon_slave_loop)
	LA	a0, sable_symmon_hwinit
1:	lw	t0, (a0)
	beq	t0, zero, 1b
	LA	t0, sabsym_slave_loop		# this is OK for now
	jr	t0
	END(sable_symmon_slave_loop)
	
LEAF(sable_symmon_slave_launch)
	LA	t0, sable_symmon_launch_kernel
	jr	t0
	END(sable_symmon_slave_launch)

/*
 * HACK - This lets us use hubuartprintf before we really define such
 * a thing in the real code.
 */
LEAF(hubprintf)
	j	printf
	END(hubprintf)
			
#endif	/* SN0 && SABLE */

#if MULTIPROCESSOR
NESTED(start_slave, STARTFRM, zero)
#ifdef SN0
	PTR_SUBU sp,STARTFRM
	PTR_S	ra,0(sp)
#endif		
	.set noreorder
	LI	t0,SYMMON_SR
#if SN0
	or	t0,SR_BEV
#endif
	MTC0(t0,C0_SR)
	nop
	LA	gp,_gp
	.set reorder
	jal	flush_cache
	jal	slave_boot
#ifdef SN0
	PTR_L	ra,0(sp)
	PTR_ADDU sp,STARTFRM
	j	ra		# on sn0 systems we return to the IP27prom
#endif		
	END(start_slave)
#endif /* MULTIPROCESSOR */

LEAF(_quit)
	jal	EnterInteractiveMode	# needs to be jal for prom to work
					# since prom looks at $ra
	END(_quit)

/*
 * stub for symmon. real one only lives in IP27prom
 */
LEAF(db_printf)
      j       printf
      END(db_printf)


	BSS(_client_sp, _MIPS_SZPTR/8)
