#ident	"IP20prom/csu.s:  $Revision: 1.39 $"

/*
 * csu.s -- prom startup code
 */

#include <fault.h>
#include <asm.h>
#include <regdef.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/i8254clock.h>

#define BSSMAGIC		0xfeeddead

#define WBFLUSHM					\
	.set	noreorder; 				\
	.set	noat;					\
	li	AT,PHYS_TO_K1(CPUCTRL0);		\
	lw	AT,0(AT);				\
	.set	at; 					\
	.set	reorder

#ifdef VERBOSE
#define	VPRINTF(msg)					\
	la	a0,9f;					\
	jal	pon_puts;				\
	.data;						\
9:	.asciiz	msg;					\
	.text
#else 
#define	VPRINTF(msg)
#endif

/* the following is lifted from sys/z8530.h */
#define WR5		5       /* Tx parameters and control */
#define WR5_TX_8BIT	0x60    /* 8 bits per Tx character */
#define WR5_TX_ENBL	0x08    /* Tx enable */
#define WR5_RTS		0x02    /* request to send */

/*
 * BEGIN: prom entry point table
 */
LEAF(start)
				/* 0x000 offset */
	j	realstart	/* prom entry point */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x040 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x080 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x0c0 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x100 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x140 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x180 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x1c0 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x200 offset */
	j	bev_utlbmiss	/* utlbmiss boot exception vector, 32 bits */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x240 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x280 offset */
	j	bev_xutlbmiss	/* utlbmiss boot exception vector, 64 bits */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x2c0 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x300 offset */
	j	bev_cacheerror
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x340 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x380 offset */
	j	bev_general
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented

/*
 * END: prom entry point table
 */

realstart:
	li	v0,SR_BEV|SR_DE
	.set	noreorder
	mtc0	v0,C0_SR		# state unknown on reset
	mtc0	zero,C0_CAUSE		# clear software interrupts
	mtc0	zero,C0_PGMASK		# init tlb pgmask
	mtc0	zero,C0_WATCHLO         # clear watchpoint interrupt
	mtc0	zero,C0_WATCHHI

#ifdef BLOCKSIZE_8WORDS
	li	v0,CONFIG_IB|CONFIG_DB|CONFIG_NONCOHRNT
#else
	li	v0,CONFIG_NONCOHRNT
#endif
	mtc0	v0,C0_CONFIG
	.set	reorder

#ifndef ENETBOOT
	.set noreorder
	mfc0	v0,C0_CONFIG
	nop
	.set reorder

	and	v0,CONFIG_BE

#ifdef	_MIPSEL
	beq	v0,zero,1f
	jal	switch_endian
#else
	bne	v0,zero,1f
	jal	switch_endian
#endif

1:
	li	v0,PHYS_TO_K1(HPC_ENDIAN&~3)
	lw	v1,0(v0)		# note: this's a word access
	and	v1,v1,HPC_CPU_LITTLE

	/* If we're little endian, the bit should be 1, otherwise
	 * set to 1.  Opposite for big endian.
	 */
#ifdef	_MIPSEL
	bnez	v1,endian_ok
	li	v1,HPC_CPU_LITTLE
#else
	beqz	v1,endian_ok
	li	v1,0
#endif
	sw	v1,0(v0)

	/*
	 * according to james tornes, we don't need to reset the system
	 * after changing the endianness of the machine since MC and the R4000
	 * already know it
	 */

endian_ok:
#endif	/* ndef ENETBOOT */

	sw	zero,PHYS_TO_K1(CPU_ERR_STAT)	# clear CPU and GIO bus error
	sw	zero,PHYS_TO_K1(GIO_ERR_STAT)
	WBFLUSHM

#ifndef	PiE_EMULATOR
#ifdef	_MIPSEL
#define	CPUCTRL0_DEFAULT	0x3c042010
#else
#define	CPUCTRL0_DEFAULT	0x3c002010
#endif

#else	/* ndef PiE_EMULATOR */

#ifdef	_MIPSEL
#define	CPUCTRL0_DEFAULT	0x3dd42010
#else
#define	CPUCTRL0_DEFAULT	0x3dd02010
#endif
#endif	/* ndef PiE_EMULATOR */

#define	CPUCTRL0_REFS_4_LINES	0x00000002
#define	CPUCTRL0_REFS_8_LINES	0x00000004

	/* set up CPU control register 0, CPUCTRL0 */
	li	t0,PHYS_TO_K1(CPUCTRL0)
#ifdef	PiE_EMULATOR
	li	t1,CPUCTRL0_DEFAULT|CPUCTRL0_REFS_4_LINES
	li	t4,PHYS_TO_K1(CTRLD)	# refresh counter preload
	li	t5,128			# refresh 4 lines every 128 cycles
	sw	t5,0(t4)
#else
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
#ifdef	BLOCKSIZE_8WORDS
1:	li	a1,32
#else
1:	li	a1,16
#endif

2:	srl	a1,3			# divided by 8
	add	a1,4			# add 4
	sll	a1,20			# shift into correct position
	or	t1,a1,CPUCTRL0_DEFAULT|CPUCTRL0_REFS_8_LINES
#endif	/* PiE_EMULATOR */
	sw	t1,0(t0)
	WBFLUSHM

	/* set MC_HWM to 0x6, enable GIO bus time out */
	li	t0,PHYS_TO_K1(CPUCTRL1)
#ifdef	_MIPSEL
	li	t1,CPUCTRL1_ABORT_EN|CPUCTRL1_HPC_FX|CPUCTRL1_HPC_LITTLE|0x6
#else
	li	t1,CPUCTRL1_ABORT_EN|CPUCTRL1_HPC_FX|0x6
#endif
	sw	t1,0(t0)
	WBFLUSHM

#define	CPU_MEMACC_DEFAULT	0x11453433
#define	GIO_MEMACC_DEFAULT	0x34332

	/* set up CPU_MEMACC and GIO_MEMACC */
	li	t0,PHYS_TO_K1(CPU_MEMACC)
	li	t1,CPU_MEMACC_DEFAULT
	sw	t1,0(t0)

	li	t0,PHYS_TO_K1(GIO_MEMACC)
	li	t1,GIO_MEMACC_DEFAULT
	sw	t1,0(t0)

#ifdef	PiE_EMULATOR
	/* set up RPSS counter as a usec counter */
	li	t0,PHYS_TO_K1(RPSS_DIVIDER)
	li	t1,0x0100		# use this as a usec counter, 1MHz clock
	sw	t1,0(t0)
#else
	/* set up RPSS counter as a 100ns counter (for 50Mhz MC) */
	li	t0,PHYS_TO_K1(RPSS_DIVIDER)
	li	t1,0x0104
	sw	t1,0(t0)
#endif

	li	t0,PHYS_TO_K1(RPSS_CTR)	# RPSS counter
	lw	t1,0(t0)		# current RPSS counter
#ifdef	PiE_EMULATOR
	li	t2,16000
#else
	li	t2,625			# must refresh SIMMs for at least 8
#endif
3:	lw	t3,0(t0)		# lines before it's safe to use them,
	subu	t3,t3,t1		# which is approximately ~62.5us
	blt	t3,t2,3b

	/* 
	 * Now have most basic hw initialized
	 *	interrupts off
	 *	parity errors cleared
	 *	memory refresh on
	 *	parity checking on
	 *	GIO bus protocol set
	 */

	/* initialize cpu board hardware
	 */
	jal	init_hpc		# initialize HPC1 chip
	jal	init_scsi		# init onboard SCSI interface
	jal	init_int2		# initialize INT2 chip

	la	fp,pon_handler		# set BEV handler
	jal	pon_initio		# initialize duart for I/O

#ifdef	VERBOSE
	la	a0,9f
	jal	pon_puts
	.set	noreorder
	mfc0	a0,C0_CONFIG
	.set	reorder
	jal	pon_puthex
	la	a0,crlf
	jal	pon_puts
	.data
9:	.asciiz	"\r\nI am alive: "
	.text
#endif

#ifndef ENETBOOT
	VPRINTF("Running MC/HPC data path diagnostic.\r\n")
	jal	pon_mc			# test path to MC
#endif
	jal	pon_regs		# test HPC data paths

	VPRINTF("Sizing memory and running memory diagnostic.\r\n")
	jal	init_memconfig		# initialize memory configuration

	/* refresh 4 lines if not using 256Kx36 or 512Kx36 SIMMs */
#ifndef	PiE_EMULATOR
	li	t0,PHYS_TO_K1(MEMCFG0)

	lw	t1,0(t0)
	and	t2,t1,MEMCFG_VLD|0x0200
	beq	t2,MEMCFG_VLD,1f
	srl	t1,16
	and	t2,t1,MEMCFG_VLD|0x0200
	beq	t2,MEMCFG_VLD,1f

	lw	t1,8(t0)
	and	t2,t1,MEMCFG_VLD|0x0200
	beq	t2,MEMCFG_VLD,1f
	srl	t1,16
	and	t2,t1,MEMCFG_VLD|0x0200
	beq	t2,MEMCFG_VLD,1f

	li	t0,PHYS_TO_K1(CPUCTRL0)
	lw	t1,0(t0)
	and	t1,~CPUCTRL0_REFS_MASK
	or	t1,CPUCTRL0_REFS_4_LINES
	sw	t1,0(t0)

1:
#endif	/* ndef PiE_EMULATOR */

	VPRINTF("Initializing PROM stack.\r\n")
	jal	init_prom_ram		# init stack and bss
					# ah... finally we have data area
	VPRINTF("Start executing C code.\r\n")
	j	sysinit			# goodbye
	END(start)

/*******************************************************************/
/* 
 * ARCS Prom entry points
 */

#define ARCS_HALT	1
#define ARCS_POWERDOWN	2
#define ARCS_RESTART	3
#define ARCS_REBOOT	4
#define ARCS_IMODE	5

LEAF(Halt)
	li	s0,ARCS_HALT
	j	arcs_common
	END(Halt)

LEAF(PowerDown)
	li	s0,ARCS_POWERDOWN
	j	arcs_common
	END(PowerDown)

LEAF(Restart)
	li	s0,ARCS_RESTART
	j	arcs_common
	END(Restart)

LEAF(Reboot)
	li	s0,ARCS_REBOOT
	j	arcs_common
	END(Reboot)

/*  EnterInteractive mode goes to prom menu.  If called from outside the
 * prom or bssmagic, or envdirty is true, do full clean-up, else just
 * re-wind the stack to keep the prom responsive.
 */
LEAF(EnterInteractiveMode)
	la	t0, _ftext
	la	t1, _etext
	sgeu	t2,ra,t0
	sleu	t3,ra,t1
	and	t0,t2,t3
	beq	t0,zero,1f


	# check if bssmagic was trashed
	lw	v0,bssmagic		# Check to see if bss valid
	bne	v0,BSSMAGIC,1f		# If not, do full init

	# check if envdirty true (program was loaded)
	lw	v0,envdirty
	beq	v0,zero,2f

	jal	close_noncons
	b	1f

2:
	# light clean-up
	li	sp,PROM_STACK-(4*4)	# reset the stack
	sw	sp,_fault_sp
	subu	sp,EXSTKSZ		# top 1/4 of stack for fault handling
	jal	main
	# if main fails fall through and re-init
1:
	# go whole hog
	li	s0,ARCS_IMODE
	j	arcs_common
	END(EnterInteractiveMode)

LEAF(arcs_common)
	.set	noreorder
	mtc0	zero,C0_SR		# no interrupts
	.set	reorder
	jal	clear_prom_ram		# clear_prom_ram initializes sp
	move	a0,zero
	jal	init_prom_soft		# initialize saio and everything else

	bne	s0,ARCS_IMODE,1f
	jal	main
	j	never			# never gets here
1:
	bne	s0,ARCS_HALT,1f
	jal	halt
	j	never			# never gets here
1:
	bne	s0,ARCS_POWERDOWN,1f
	jal	powerdown
	j	never			# never gets here
1:
	bne	s0,ARCS_RESTART,1f
	jal	restart
	j	never			# never gets here
1:
	bne	s0,ARCS_REBOOT,never
	jal	reboot

never:	j	_exit			# never gets here
	END(arcs_common)

/*
 * _exit()
 *
 * Re-enter prom command loop without initialization
 * Attempts to clean-up things as well as possible while
 * still maintaining things like current enabled consoles, uart baud
 * rates, and environment variables.
 */
LEAF(_exit)
	.set	noreorder
	mtc0	zero,C0_SR		# back to a known sr
	mtc0	zero,C0_CAUSE		# clear software interrupts
	.set	reorder

	lw	v0,bssmagic		# Check to see if bss valid
	bne	v0,BSSMAGIC,_init	# If not, do a hard init

	li	sp,PROM_STACK-(4*4)	# reset the stack
	sw	sp,_fault_sp
	subu	sp,EXSTKSZ		# top 1/4 of stack for fault handling
	jal	config_cache		# determine cache sizes
	jal	flush_cache		# flush cache
	jal	enter_imode
1:	b	1b			# never gets here
	END(_exit)

/*
 * notimplemented -- deal with calls to unimplemented prom services
 */
NOTIMPFRM=(4*4)+8
NESTED(notimplemented, NOTIMPFRM, zero)
	subu	sp,NOTIMPFRM
	beqz	fp,1f
	move	a0,fp
	move	fp,zero
	j	a0
1:
	la	a0,notimp_msg
	jal	printf
	jal	EnterInteractiveMode
	END(notimplemented)

	.data
notimp_msg:
	.asciiz	"ERROR: call to unimplemented prom routine\n"
	.text

/*******************************************************************/
/*
 * Subroutines
 *
 */

/*
 * _init -- reinitialize prom and reenter command parser
 */
LEAF(_init)
	.set	noreorder
	mtc0	zero,C0_SR
	.set	reorder
	jal	clear_prom_ram		# clear_prom_ram initializes sp
	move	a0,zero
	jal	init_prom_soft
	jal	main
	jal	EnterInteractiveMode	# shouldn't get here
	END(_init)

/*
 * clear_prom_ram -- config and init cache, clear prom bss,
 * clear memory between _fbss and PROM_STACK-4 inclusive
 * (for dprom, clear between _fbss and end), clears and
 * initializes stack
 */
INITMEMFRM=(4*4)+8
NESTED(clear_prom_ram, INITMEMFRM, zero)
 	la	a0,_fbss		# clear locore, bss, and stack
#ifndef ENETBOOT
 	li	a1,PROM_STACK-4		# end of prom stack
#else
 	la	a1,end			# clear bss only
#endif

	and	a0,0x9fffffff		# turn into cached addresses
	and	a1,0x9fffffff

	move	v0,a0
	move	v1,a1

	/* hit invalidate to prevent VCE during memory clear */
 	.set	noreorder
1:	add	v0,256
	cache	CACH_SD|C_HINV,-256(v0)
	cache	CACH_SD|C_HINV,-192(v0)
	cache	CACH_SD|C_HINV,-128(v0)
	bltu	v0,v1,1b
	cache	CACH_SD|C_HINV,-64(v0)
	.set	reorder

	move	v0,a0

 	.set	noreorder
1:	add	v0,64			# clear 64 bytes at a time
	sw	zero,-4(v0)
	sw	zero,-8(v0)
	sw	zero,-12(v0)
	sw	zero,-16(v0)
	sw	zero,-20(v0)
	sw	zero,-24(v0)
	sw	zero,-28(v0)
	sw	zero,-32(v0)
	sw	zero,-36(v0)
	sw	zero,-40(v0)
	sw	zero,-44(v0)
	sw	zero,-48(v0)
	sw	zero,-52(v0)
	sw	zero,-56(v0)
	sw	zero,-60(v0)
 	bltu	v0,v1,1b
	sw	zero,-64(v0)		# BDSLOT
 	.set	reorder

	xor	v0,a0,0x400000		# flush out of the zeroes; +/- 4M
	xor	v1,a1,0x400000		# such that it will work even with the
					# the largest 2nd cache
 	.set	noreorder
1:	sw	zero,0(v0)
 	bltu	v0,v1,1b
 	add	v0,64			# BDSLOT: touch every cache block
 	.set	reorder

XLEAF(init_prom_ram)
	/*
	 * Remember that bss is now okay
	 */
	li	v0,BSSMAGIC
	sw	v0,bssmagic

	/*
	 * retrieve bad simm flags from mult-hi register
	 */
	mfhi	v0
	sw	v0,simmerrs

	/*
	 * Initialize stack
	 */
	li	sp,PROM_STACK-(4*4)
	sw	sp,_fault_sp		# base of fault stack
	subu	sp,EXSTKSZ		# top 1/4 of stack for fault handling
	subu	sp,INITMEMFRM
	sw	ra,INITMEMFRM-4(sp)
	jal	szmem			# determine main memory size
	sw	v0,memsize
	lw	ra,INITMEMFRM-4(sp)
	addu	sp,INITMEMFRM
	j	ra
	END(clear_prom_ram)

/* 
 * initialize HPC configuration
 */
LEAF(init_hpc)
	li	t0,PHYS_TO_K1(HPC1MISCSR)	# Reset the DSP.
	li	t1,9
	sw	t1,0(t0)
	li	t0,PHYS_TO_K1(HEADPHONE_MDAC_L) # turn off the left headphone
	sb	zero,0(t0)
	li	t0,PHYS_TO_K1(HEADPHONE_MDAC_R) # turn off the right headphone
	sb	zero,0(t0)

	/* on a blackjack, turn the led on to amber
	 */
	li	t0,PHYS_TO_K1(CPU_AUX_CONTROL)
	lbu	t1,0(t0)
	or	t1,CONSOLE_LED
	sb	t1,0(t0)

	li	t0,PHYS_TO_K1(DUART0A_CNTRL)
	li	t1,WR5
	sb	t1,0(t0)
	li	t1,WR5_TX_8BIT|WR5_TX_ENBL|WR5_RTS
	sb	t1,0(t0)
	j	ra
	END(init_hpc)

/* 
 * initialize INT2 configuration
 */
LEAF(init_int2)
	.set 	noreorder
	li	t0,PHYS_TO_K1(LIO_0_MASK_ADDR)	# mask all external ints
	sb	zero,0(t0)
	li	t0,PHYS_TO_K1(LIO_1_MASK_ADDR)
	sb	zero,0(t0)
	li	t0,PHYS_TO_K1(VME_0_MASK_ADDR)
	sb	zero,0(t0)
	li	t0,PHYS_TO_K1(VME_1_MASK_ADDR)
	sb	zero,0(t0)

	li	t0,PHYS_TO_K1(PORT_CONFIG)	# configure duart clocks
	li	t1,PCON_SER0RTS|PCON_SER1RTS|PCON_POWER
	sb	t1,0(t0)

	li	t0,PT_CLOCK_ADDR		# disable 8254 timer
	li	t1,PTCW_SC(0)|PTCW_MODE(1)
	sb	t1,PT_CONTROL(t0)
	li	t1,PTCW_SC(1)|PTCW_MODE(1)
	sb	t1,PT_CONTROL(t0)
	li	t2,PTCW_SC(2)|PTCW_MODE(1)
	sb	t1,PT_CONTROL(t0)

	li	t0,TIMER_ACK_ADDR		# clear timer ints
	li	t1,3
	j	ra
	sb	t1,0(t0)			# BDSLOT
	.set 	reorder
	END(init_int2)

/*
 * Do NOT reset the bus.  A reset is done by the act of powering up
 * or resetting.  It shouldn't be needed from software.  If there is an
 * interrupt, clear it, but don't wait for one.
 */
LEAF(init_scsi)
	.set 	noreorder
	/* clear the scsi reset line; HPC latches it on reset/powerup */
	li	t0,PHYS_TO_K1(SCSI0_CTRL_ADDR)
	sw	zero,0(t0)

	/* clear any SCSI interrupts by reading the status register on the
	   Western Digital SCSI controller chip
	   Then issue a reset command.  This is needed because a chip reset
	   doesn't clear the registers, and we now look at some of them
	   on startup, to handle kernel to PROM transitions.  CMD register
	   follows status, and register # is auto-incremented, so don't
	   need to write the register # (RESET is cmd 0). Need 7 usec between
	   status register read and cmd; we execute about 1 cmd/usec in IP20 PROM.
	*/
	li	t1,0x17				# SCSI reg 0x17 - status
	li	t0,PHYS_TO_K1(SCSI0A_ADDR)
	sb	t1,0(t0)
	li	t0,PHYS_TO_K1(SCSI0D_ADDR)
	lb	zero,0(t0)
	nop; nop; nop; nop; nop; nop; nop; nop
	sb	zero,0(t0) # reset the chip; clears sync register, etc. 
	j	ra
	nop
	.set 	reorder
	END(init_scsi)

/*
 * boot exception vector handler
 *
 * this routine check fp for a current exception handler
 * if non-zero, jump to the appropriate handler else spin
 */
LEAF(bev_general)
	li	a0,1
	b	bev_common
	END(bev_general)

LEAF(bev_cacheerror)
	li	a0,2
	b	bev_common
	END(bev_cacherror)

LEAF(bev_utlbmiss)
	li	a0,3
	b	bev_common
	END(bev_utlbmiss)

LEAF(bev_xutlbmiss)
	li	a0,4
	b	bev_common
	END(bev_xutlbmiss)

LEAF(bev_common)
	beq	fp,zero,1f
	move	k0,fp
	move	fp,zero
	j	k0
1:	b	1b
	END(bev_common)

/*******************************************************************/
/* 
 * Data area
 *
 */
	BSS(bssmagic,4)
	BSS(simmerrs,4)
