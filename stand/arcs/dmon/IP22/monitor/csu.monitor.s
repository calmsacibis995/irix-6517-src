#ident	"$Id: csu.monitor.s,v 1.1 1994/07/21 00:18:00 davidl Exp $"
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
#include "mips/dregdef.h"
#include "mips/cpu.h"
#include "mips/cpu_board.h"
*/
#include "sys/regdef.h"
#include "sys/fpregdef.h"
#include "sys/asm.h"
#include "saioctl.h"
#include "sys/sbd.h"
#include "sys/hpc3.h"
#include "sys/IP22.h"
#include "sys/i8254clock.h"
#include "pdiag.h"
#include "monitor.h"
#include "sys/mc.h"

/*
 * NOTE: Define MACHINE_TYPE to be the BRDTYPE_xxxxx of the machine
 *	 you want this monitor to work on. In the boot prom, he value of 
 *	 MACHINE_TYPE is compared with the return value of get_machine_type() 
 *	 for a match.
 */

#define MACHINE_TYPE	12
#define GREEN	0x10
#define RED	0x20
#define AMBER	0x0
#define LEDOFF	0x30

#define WBFLUSHM                                        \
        .set    noreorder;                              \
        .set    noat;                                   \
        li      AT,PHYS_TO_K1(CPUCTRL0);                \
        lw      AT,0(AT);                               \
        .set    at;                                     \
        .set    reorder


/*------------------------------------------------------------------------+
| define bss variables							  |
+------------------------------------------------------------------------*/
BSS(environ, 4)


	.text

        .globl  main
	
/*========================================================================+
| Starting point of all diagnostic code.                                  |
+========================================================================*/


LEAF(_prom_start)
/*
 *	prom entry point table:
 *
 *	entry point		-> corresponding system prom entry <-
 */
        j       _realstart	# "realstart"
        j       _realstart	# "_promexec"
        j       _realstart	# "_exit" - reenter command loop w/t init
        j       _realstart	# "_init" - reenter monitor and reinit
        j       _realstart	# "reboot" - perform power-up boot (if set)
END(_prom_start)

/*------------------------------------------------------------------------+
| Routine  : UTLB_excpt                                                   |
| Function : UTLB exception handler. UTLB exception vector here if the    |
|   BEV bit in the status register  is set to 1,  which is the case on    |
|   system reset.                                                         |
|									  |
| Use VectorReg as a vector for user installed handler.			  |
+------------------------------------------------------------------------*/
#ifdef R3030
	.align	8			# align it to 0xbfc00100
#endif R3030

#ifdef R4230
	.align	9			# align pc to 0xbfc00200
#endif R4230

LEAF(UTLB_excpt)
	.set	noreorder
	beq	Bevhndlr, zero, 1f	# use default handler if zero 
	nop
	j	Bevhndlr
	move	Bevhndlr, zero		# (BDSLOT) reset Bevhndlr on exc
1:
	.set	noat
	move	VectorReg, zero		# reset user fault handler vector
	la	Bevhndlr, _bev_handler	# use default BEV exception handler
	j	Bevhndlr		# go there
	li	ExceptReg, EXCEPT_UTLB	# (BDSLOT) pass exception type 
	.set	at
	.set	reorder
END(UTLB_excpt)


#ifdef R4230
	.align	7			# advance pc to 0xbfc00280

LEAF(XTLB_excpt)
	.set	noreorder
	li	ExceptReg, EXCEPT_XTLB	# pass exception type 
	beq	Bevhndlr, zero, 1f	# use default handler if zero 
	nop
	j	Bevhndlr
	move	Bevhndlr, zero		# (BDSLOT) reset Bevhndlr on exc
1:
	.set	noat
	move	VectorReg, zero		# reset user fault handler vector
	la	Bevhndlr, _bev_handler	# use default BEV exception handler
	j	Bevhndlr		# go there
	nop
	.set	at
	.set	reorder
END(XTLB_excpt)

	.align	8			# advance pc to 0xbfc00300

LEAF(Cache_excpt)
	.set	noreorder
	li	ExceptReg, EXCEPT_CACHE	# pass exception type 
	beq	Bevhndlr, zero, 1f	# use default handler if zero 
	nop
	j	Bevhndlr
	move	Bevhndlr, zero		# (BDSLOT) reset Bevhndlr on exc
1:
	.set	noat
	move	VectorReg, zero		# reset user fault handler vector
	la	Bevhndlr, _bev_handler	# use default BEV exception handler
	j	Bevhndlr		# go there
	nop
	.set	at
	.set	reorder
END(Cache_excpt)

#endif R4230


/*------------------------------------------------------------------------+
| Routine  : Gen_excpt                                                    |
| Function : This is  a general exception vector  in PROM space.  The cpu |
|   branch to here upon receiving exception and the BEV bit in the status |
|   register  is set to 1,  which is the case on system reset.            |
|									  |
| Use VectorReg as a vector for user installed handler.			  |
+------------------------------------------------------------------------*/
	.align	7			# align pc to 0xbfc00180 (for R3030)
					# align pc to 0xbfc00380 (for R4230)

LEAF(Gen_excpt)
	.set	noreorder
	beq	Bevhndlr, zero, 1f	# use default handler if zero 
	nop
	j	Bevhndlr
	move	Bevhndlr, zero		# (BD) reset Bevhndlr on exception
1:
	.set	noat
	move	VectorReg, zero		# reset user fault handler vector
	la	Bevhndlr, _bev_handler	# use default BEV exception handler
	j	Bevhndlr		# go there
	li	ExceptReg, EXCEPT_NORM	# (BDSLOT) pass exception type 
	.set	at
	.set	reorder
END(Gen_excpt)

/*
 * Diag Monitor ID string
 *
 * This ID string is used to identify the existence of a diags monitor
 * in the boot prom, and also the correctness of the monitor for
 * the type of machine in operation.
 *
 * The id string is accessed by adding 0x200 to the start address of the
 * monitor. To get this right, the start address must be aligned on Kb
 * address boundary.
 */
/*
 * For Aftershock, the id string is pushed to 0x400 from the starting
 * address of the debug monitor.
 */
	.align 8

LEAF(DiagMon_ID)
	.ascii	"SGI"			# id for existence identification
	.word	MACHINE_TYPE		# machine id for type match
	.ascii	"Copyright (c) 1993 "
	.ascii	"Silicon Graphics Inc. "
	.asciiz	"All Rights Reserved "
	.word	0
END(DiagMon_ID)

	.align	6

	.globl	main

LEAF(_realstart)

	.set	noreorder
	or	a0, zero, SR_BEV	# use prom boot strap.
	mtc0	a0, C0_SR
	mtc0	zero, C0_CAUSE		# clear c0_cause
	.set	reorder

	li	a0, LEDOFF
	li	a1, 0x1
	#jal set_led_ip22

	/*
	 * Initialize the Memory Control and other registers
	 */
	sw	zero,PHYS_TO_K1(CPU_ERR_STAT)	# clear CPU and GIO bus error
	sw	zero,PHYS_TO_K1(GIO_ERR_STAT)
	lw	zero,PHYS_TO_K1(HPC3_INTSTAT_ADDR)	# clear HPC3 bus error
	WBFLUSHM

#ifdef	_MIPSEL
#define	CPUCTRL0_DEFAULT	0x3c042010
#else
#define	CPUCTRL0_DEFAULT	0x3c002010
#endif

#define	CPUCTRL0_REFS_4_LINES	0x00000002
#define	CPUCTRL0_REFS_8_LINES	0x00000004

	/* set up CPU control register 0, CPUCTRL0 */
	li	t0,PHYS_TO_K1(CPUCTRL0)

	/* set up the MUX_HWM according to the cache line size */
	.set	noreorder
	mfc0	a0,C0_CONFIG
	.set	reorder

	and	a1,a0,CONFIG_SC
	bne	a1,zero,1f		# no secondary cache

	and	a1,a0,CONFIG_SB
	srl	a1,CONFIG_SB_SHFT	# 0/1/2/3=4/8/16/32 words
	add	a1,4
	li	a2,1
	sll	a1,a2,a1		# bytes in cache line
	b	2f

	/* use primary data cache line if there's no secondary cache */
1:	li	a1,32

2:	srl	a1,3			# divided by 8
	add	a1,4			# add 4
	sll	a1,20			# shift into correct position
	or	t1,a1,CPUCTRL0_DEFAULT|CPUCTRL0_REFS_8_LINES
	sw	t1,0(t0)
	WBFLUSHM

	/* set MC_HWM to 0x6, enable GIO bus time out */
	li	t0,PHYS_TO_K1(CPUCTRL1)
	li	t1,CPUCTRL1_ABORT_EN|0x6
	sw	t1,0(t0)
	WBFLUSHM

#if !IP24
	/* fullhouse: Set GIO64_ARB depending on Board rev: rev-003 and
	 * before uses EXP1, while rev-004 will use EISA.
	 */
        li      t0,PHYS_TO_K1(HPC3_SYS_ID)	# get board rev bits
        lw      t1,0(t0)
        andi    t2,t1,BOARD_REV_MASK
        srl     t3,t2,BOARD_REV_SHIFT
	sub	t3,t3,1				# rev-004 an on will be >= 0

	li	t0,PHYS_TO_K1(GIO64_ARB)

	bge	t3,zero,1f			# fullhouse rev >= 2 (-004)

	/* rev-003 or less */
	li	t1,  GIO64_ARB_HPC_SIZE_64   | GIO64_ARB_HPC_EXP_SIZE_64 | GIO64_ARB_1_GIO         | GIO64_ARB_EXP0_PIPED | GIO64_ARB_EXP1_MST      | GIO64_ARB_EXP1_RT
	b	2f
1:
	/* rev-004 and beyond */
	li	t1,GIO64_ARB_HPC_SIZE_64   | GIO64_ARB_HPC_EXP_SIZE_64 | GIO64_ARB_1_GIO         | GIO64_ARB_EXP0_PIPED 
#else
	/* guinness */
	li	t0,PHYS_TO_K1(GIO64_ARB)
	li	t1, GIO64_ARB_HPC_SIZE_64 | GIO64_ARB_1_GIO
#endif
2:
	sw	t1,0(t0)
	WBFLUSHM

#define	CPU_MEMACC_DEFAULT	0x11453433
#define	GIO_MEMACC_DEFAULT	0x34322

	/* set up CPU_MEMACC and GIO_MEMACC */
	li	t0,PHYS_TO_K1(CPU_MEMACC)
	li	t1,CPU_MEMACC_DEFAULT
	sw	t1,0(t0)

	li	t0,PHYS_TO_K1(GIO_MEMACC)
	li	t1,GIO_MEMACC_DEFAULT
	sw	t1,0(t0)

	# set up as 100ns counter for 50Mhz MC
	li	t0,PHYS_TO_K1(RPSS_DIVIDER)
	li	t1,0x0104
	sw	t1,0(t0)

	li	t0,PHYS_TO_K1(RPSS_CTR)	# RPSS counter
	lw	t1,0(t0)		# current RPSS counter
	li	t2,625			# must refresh SIMMs for at least 8
3:	lw	t3,0(t0)		# lines before it's safe to use them,
	subu	t3,t3,t1		# which is approximately ~62.5us
	blt	t3,t2,3b
	#jal	init_mc
	nop

	li	a0, GREEN
	li	a1, 0x1
	#jal set_led_ip22
	/*
	 * Initialize the HPC3 registers
	 */
	jal	init_hpc3
	nop
	li	a0, GREEN
	li	a1, 0x1
	#jal set_led_ip22
	nop
	jal	init_intX
	nop
/*
#ifdef IP24
	li	a0, 0x23200000		# set default mem config to 16mb
	sw	a0, PHYS_TO_K1(MEMCFG0)
	sw	zero, PHYS_TO_K1(MEMCFG1)
#endif IP24	
*/
	li	a0, GREEN
	li	a1, 0x1
	#jal set_led_ip22

	INSTALLVECTOR(1f)		# set vector to loop on exception
1:
#if	!defined(IP24) 
	jal	pon_initio	# initialize the scc ports
	#jal	scc_init

#else
	jal	pon_initio	# initialize the scc ports
	PRINT("\nYo")
#endif

	li	a0, GREEN
	li	a1, 0x1
	#jal set_led_ip22

	PRINT("\n- SCC initialized")

	li	a0, GREEN
	li	a1, 0x1
	#jal set_led_ip22
	/*
	 * test the section of nvram used by the debug monitor
	 */

	INSTALLVECTOR(1f)		# in case unexpected exception

	PRINT("\n- Test nvram (byte 0x")
	li	a0, NVDIAG_BASE
	jal	puthex
	PRINT(" to 0x")
	li	a0, NVDIAG_END
	jal	puthex
	PRINT("):")

	PUTS(nvram_AddrUMsg)
	li	a0, NVDIAG_BASE
	li	a1, NVDIAG_END
	jal	nvram_addru_test

	bne	v0, zero, 1f
	PUTS(word_pass)

	j	2f
1:
	PUTS(word_fail)

	/*
	 * nvram debug routine for a failing nvram.
	 */
	jal	nvram_debug

2:
	li	a0, GREEN
	li	a1, 0x1
	#jal set_led_ip22
	/*
	 * Init nvram diag section 
 	 */

	RESETVECTOR()

	PRINT("\n- Clear diagmon section of nvram ...")
	jal	init_nvram_var		# clear diag var section of nvram
	PUTS(word_done)

#ifdef RICHARD
/*
	INSTALLVECTOR(1f)		# don't let memory failure stop
					# bring up the debug monitor

	PRINT("\n- Clearing first 1MB of memory...")
	jal	init_mem
	PUTS(word_done)

	j	sys_setup
1:
	PUTS(word_fail)
*/
#endif

	#+----------------------------------------------------------------+
	#| turn off parity detection, clear error reg, and set up stack   |
	#+----------------------------------------------------------------+
sys_setup:

	li	sp, PROM_STACK		# set sp to MONITOR stack

	/*
	 * Set SR_CU1 bit to make FPA usable
	 */
	jal	_setup_fp
	nop
	li	CnfgStatusReg, CUMask	# Set FPU usability bit in StatusReg

	li	a0, GREEN
	li	a1, 0x1
	#jal set_led_ip22
	nop

	j	main
END(_realstart)


/*
 * void init_nvram_var(void)
 *
 * initialize the poriton of nvram used by the prom monitor
 * to zero.
 */
LEAF(init_nvram_var)
	move	t2, ra			# save ra in t2

	li	a0, NVDIAG_BASE
	jal	addr_nvram		# return K1seg addr
	move	t0, v0			# save start addr in t0

	li	a0, NVDIAG_END
	jal	addr_nvram		# return K1seg addr
	move	t1, v0			# save end addr in t1
1:
	sb	zero, (t0)
	addu	t0, 4
	bltu	t0, t1, 1b

	j	t2
END(init_nvram_var)

/* 
 * initialize HPC configuration
 */

LEAF(init_hpc3)
/*		Set endianness
*/
        li      t0,PHYS_TO_K1(HPC3_MISC_ADDR)
        lw      t1,0(t0)

	and	t1,~HPC3_DES_ENDIAN	# DMA descriptor fetch is big endian
	or	t1,HPC3_EN_REAL_TIME
        sw      t1,0(t0)

/*		Configure SCSI 
*/
#ifdef IP24
	li      t0,PHYS_TO_K1(HPC3_GEN_CONTROL)
	li	t2,GC_GIO_33MHZ
#else
	li      t0,PHYS_TO_K1(HPC3_EXT_IO_ADDR)
	li	t2,EXTIO_GIO_33MHZ
#endif
	lw	t1,0(t0)
	and	t2,t1
	beq	t2,zero,1f

#ifndef	IP24
	li	t0,PHYS_TO_K1(HPC3_SCSI_PIOCFG1)	# set scsi 1 to 33MHZ
	li	t1,WD93_PIO_CFG_33MHZ        
	sw	t1,0(t0)
#endif

	li	t0,PHYS_TO_K1(HPC3_SCSI_PIOCFG0)	# set scsi 0 to 33MHZ
	li	t1,WD93_PIO_CFG_33MHZ        
	sw	t1,0(t0)

	b	2f
1:
#ifndef	IP24
        li      t0,PHYS_TO_K1(HPC3_SCSI_PIOCFG1)       # set scsi chan1 to 25MHZ
        li      t1,WD93_PIO_CFG_25MHZ	
        sw      t1,0(t0)
#endif

        li      t0,PHYS_TO_K1(HPC3_SCSI_PIOCFG0)       # set scsi chan0 to 25MHZ
        li      t1,WD93_PIO_CFG_25MHZ	
        sw      t1,0(t0)

2:

/* 	Configure INT2/INT3
*/
	li	t0,PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(4)) 
        li      t1,0x4e27
        sw      t1,0(t0)


/*	Configure IO channel
*/
	li	t0,PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(6))
	beq	t2,zero,1f		# t2 still has valid gio speed
        li      t1,0x988d		# setup 33mhz gio value for c6
	b	2f
	
1:
        li      t1,0x946d		# setup 25mhz gio value for c6
2:
        sw      t1,0(t0)
	li	t0,PHYS_TO_K1(HPC3_WRITE1)	/* reset hi */
#if	IP24
        li      t1,(LED_RED_OFF|PAR_RESET|KBD_MS_RESET)
#else
        li      t1,(LED_GREEN|PAR_RESET|KBD_MS_RESET|EISA_RESET)
#endif
        sw      t1,0(t0)

/*	Configure extended register 
*/
	li	t0,PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(5)) 
        li      t1,0x44e27
        sw      t1,0(t0)

/*	Configure PIO channels for audio devices
*/
	/* Channel 0 = HAL2 chip */
	li	t0,PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(0))
	li	t1,0x48a45
	sw	t1,0(t0)

	/* Channel 1 = AES chips */
	li	t0,PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(1))
	li	t1,0x88e47
	sw	t1,0(t0)

	/* Channel 2 = MDAC volume control */
	li	t0,PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(2))
	li	t1,0x11289
	sw	t1,0(t0)

	/* Channel 3 = synthesizer chip */
	li	t0,PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(3))
	li	t1,0xc4a25
	sw	t1,0(t0)

/*	Configure PBUS DMA channels to the best of our ability.
	Audio uses PBUS DMA channels 1-4, parallel port using
	channel 0.
*/
	li	t0,PHYS_TO_K1(HPC3_PBUS_CFGDMA_ADDR(1))
	li	t1,0x8248844
	sw	t1,0(t0)

	li	t0,PHYS_TO_K1(HPC3_PBUS_CFGDMA_ADDR(2))
	sw	t1,0(t0)

	li	t0,PHYS_TO_K1(HPC3_PBUS_CFGDMA_ADDR(3))
	sw	t1,0(t0)

	li	t0,PHYS_TO_K1(HPC3_PBUS_CFGDMA_ADDR(4))
	sw	t1,0(t0)

	j	ra
	END(init_hpc3)


/*
 * set_led_ip22() - Set the led color based on the value passed in a0.
 * Not 'C' callable folks....
 *
 * Register a0 = 
 *	LED_AMBER		0x0    Set led to amber or a lesser red for
 *					you color blind people 
 *	LED_GREEN               0x10   Set led to green 
 *	LED_RED			0x20   Set led to bright red 
 * 	LED_OFF			0x30   turn led off (guinness)
 */

#define WAITCOUNT	0x90000
LEAF(set_led_ip22)
        .set    noreorder

         li     t0,PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(6))
         li     t3,0xd46b
	 nop
         sw     t3,0(t0)

         li     t0,PHYS_TO_K1(HPC3_WRITE1); 	/* channel 6 ,offset 0x1c */

repeat:
	 /* Shut the led off. Do a read/modify/write */
	 lw	t1, 0(t0)
         li     t3, 0x30
	 nop
	 or	t1, t3
         sw     t1,0(t0)

         li     t2,WAITCOUNT    		# delay
2:       bnez   t2,2b
         subu   t2,1

	 /* Set the led to the color of your choice baby.. */
	 lw	t1, 0(t0)
	 li	t2, 0xf
	 and	t1, t2
	 or	t1, a0
         sw     t1,0(t0)

         li     t2,WAITCOUNT    		# delay
3:       bnez   t2,3b
         subu   t2,1

         nop
	 subu	a1, 1
	 bnez   a1, repeat
	 nop

         j      ra
         nop

        .set    reorder
END(set_led_ip22)
/* 
 * initialize INT2/INT3 configuration
 *
 * NBI, maintain HPC_INT_ADDR in (t2)
 */
LEAF(init_intX)
#ifdef	IP24
	li	t2,PHYS_TO_K1(HPC3_INT3_ADDR)	# IOC1/INT3
#else
	li	t2,PHYS_TO_K1(HPC3_INT3_ADDR)	# assume IOC1/INT3
	IS_IOC1(t0)
	bnez	t0, 1f				# branch if IOC1/INT3
	li	t2,PHYS_TO_K1(HPC3_INT2_ADDR)	# use INT2
1:
#endif

	.set 	noreorder
#ifndef	IP24	/* PORT_CONFIG does not exist on guinness */
	# deassert reset/init retrace/select S0-DMA-SYNC
	li	t1,(PCON_CLR_SG_RETRACE_N|PCON_CLR_S0_RETRACE_N|PCON_SG_RESET_N|PCON_S0_RESET_N)
	sb	t1,PORT_CONFIG_OFFSET(t2)
#endif

	sb	zero,LIO_0_MASK_OFFSET(t2)	# mask all external ints
	sb	zero,LIO_1_MASK_OFFSET(t2)
	sb	zero,LIO_2_MASK_OFFSET(t2)
	sb	zero,LIO_3_MASK_OFFSET(t2)

	# 8254 times are broken on IOC1, so leave them be on IOC1 machines
	.set	reorder
	IS_IOC1(t1)
	bnez	t1, 1f
	.set	noreorder
	li	t1,PTCW_SC(0)|PTCW_MODE(1)	# disable 8254 timer
	sb	t1,PT_CLOCK_OFFSET+PT_CONTROL(t2)
	li	t1,PTCW_SC(1)|PTCW_MODE(1)
	sb	t1,PT_CLOCK_OFFSET+PT_CONTROL(t2)
	li	t1,PTCW_SC(2)|PTCW_MODE(1)
	sb	t1,PT_CLOCK_OFFSET+PT_CONTROL(t2)
	li	t1,3				# clear timer ints
	sb	t1,TIMER_ACK_OFFSET(t2)
1:

	li	t0,PHYS_TO_K1(HPC3_WRITE2)	# configure duart clocks
	li	t1,UART1_ARC_MODE|UART0_ARC_MODE# low=RS422 MAC mode,hi=RS232
	sw	t1,0(t0)

#ifdef IP24
	li	t1,(POWER_SUP_INHIBIT|POWER_INT|PANEL_VOLUME_UP_ACTIVE|PANEL_VOLUME_UP_INT|PANEL_VOLUME_DOWN_ACTIVE|PANEL_VOLUME_DOWN_INT)
#else
	li	t1,((POWER_SUP_INHIBIT|POWER_INT)&0xff)
#endif
	li	t0,PHYS_TO_K1(HPC3_PANEL)	# power on and clear intr
	sw	t1,0(t0)

	j	ra
	nop
	.set 	reorder
	END(init_intX)


/*
 * This code sets up the FP Chip, so that the interrupt vector is
 * initialized.  Note, since we are not reading from the chip, this
 * should still work even if a chip isn't installed.
 *
 * Set FPU usable bit in C0_SR
 */
LEAF(_setup_fp)
	.set 	noreorder
	mfc0	v0, C0_SR	# get current SR (and save it in v0)
	li	v1, SR_CU1	# (LDSLOT)
	or	v1, v0
	mtc0	v1, C0_SR	# make FPU ops street-legal
	nop			# wait 2 cycles before Cp1 can be accessed
	nop

	ctc1	zero, fcr31	# move 0 into the fp status register
	nop

	j	ra		# exit
	# mtc0	v0,C0_SR	# (BDSLOT) restore SR, disallow Cp1 accesses
	nop
	.set 	reorder
END(_setup_fp)

	.align	2
	.data
word_pass:	.asciiz " PASSED"
word_fail:	.asciiz " FAILED"
word_done:	.asciiz " Done"
