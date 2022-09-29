#ident	"IP22prom/csu.s:  $Revision: 1.75 $"

/*
 * csu.s -- prom startup code
 */

#include <fault.h>
#include <ml.h>
#include <sys/cpu.h>
#include <sys/ds1286.h>
#include <sys/sbd.h>
#include <sys/i8254clock.h>
#include <sys/hal2.h>

#ifndef IP24			/* Indigo2 doesn't support R4000PC */
#undef	R4000PC
#endif

#if ENETBOOT
#define ENET_SR_BEV		0
#else
#define ENET_SR_BEV		SR_BEV
#endif

#define BSSMAGIC		0x0deadbed

#define WBFLUSHM					\
	.set	noreorder; 				\
	.set	noat;					\
	LI	AT,PHYS_TO_K1(CPUCTRL0);		\
	lw	AT,0(AT);				\
	.set	at; 					\
	.set	reorder

#ifdef VERBOSE
#define	VPRINTF(msg)					\
	LA	a0,9f;					\
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
	li	v0,SR_PROMBASE|ENET_SR_BEV|SR_DE
	.set	noreorder
	mtc0	v0,C0_SR		# state unknown on reset
	mtc0	zero,C0_CAUSE		# clear software interrupts
	mtc0	zero,C0_PGMASK		# init tlb pgmask
	mtc0	zero,C0_WATCHLO         # clear watchpoint interrupt
	mtc0	zero,C0_WATCHHI

#ifdef BLOCKSIZE_8WORDS
	li	v0,CONFIG_IB|CONFIG_DB|CONFIG_UNCACHED
#else
	li	v0,CONFIG_UNCACHED
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
#endif	/* ndef ENETBOOT */

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
	LI	t0,PHYS_TO_K1(CPUCTRL0)

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
#ifdef R4600
1:	and	a2,a0,CONFIG_IB|CONFIG_DB
	li	a1,32
	bnez	a2,2f
	li	a1,16
#else /* R4600 */
#ifdef	BLOCKSIZE_8WORDS
1:	li	a1,32
#else
1:	li	a1,16
#endif
#endif /* R4600 */

2:	srl	a1,3			# divided by 8
	add	a1,4			# add 4
	sll	a1,20			# shift into correct position
	or	t1,a1,CPUCTRL0_DEFAULT|CPUCTRL0_REFS_8_LINES
	sw	t1,0(t0)
	WBFLUSHM

	/* set MC_HWM to 0x6, enable GIO bus time out */
	LI	t0,PHYS_TO_K1(CPUCTRL1)
	li	t1,CPUCTRL1_ABORT_EN|0x6
	sw	t1,0(t0)
	WBFLUSHM

#if !IP24
	/* fullhouse: Set GIO64_ARB depending on Board rev: rev-003 and
	 * before uses EXP1, while rev-004 will use EISA.
	 */
        LI      t0,PHYS_TO_K1(HPC3_SYS_ID)	# get board rev bits
        lw      t1,0(t0)
        andi    t2,t1,BOARD_REV_MASK
        srl     t3,t2,BOARD_REV_SHIFT
	sub	t3,t3,1				# rev-004 an on will be >= 0

	LI	t0,PHYS_TO_K1(GIO64_ARB)

	bge	t3,zero,1f			# fullhouse rev >= 2 (-004)

	/* rev-003 or less */
	lw	t1,_gio64_arb_003
	b	2f
1:
	/* rev-004 and beyond */
	lw	t1,_gio64_arb_004
#else
	/* guinness */
	li	t0,PHYS_TO_K1(GIO64_ARB)
	lw	t1,_gio64_arb_gns
#endif
2:
	sw	t1,0(t0)
	WBFLUSHM

#define	CPU_MEMACC_DEFAULT	0x11453433
#define	GIO_MEMACC_DEFAULT	0x34322

	/* set up CPU_MEMACC and GIO_MEMACC */
	LI	t0,PHYS_TO_K1(CPU_MEMACC)
	li	t1,CPU_MEMACC_DEFAULT
	sw	t1,0(t0)

	LI	t0,PHYS_TO_K1(GIO_MEMACC)
	li	t1,GIO_MEMACC_DEFAULT
	sw	t1,0(t0)

	# set up as 100ns counter for 50Mhz MC
	LI	t0,PHYS_TO_K1(RPSS_DIVIDER)
	li	t1,0x0104
	sw	t1,0(t0)

	LI	t0,PHYS_TO_K1(RPSS_CTR)	# RPSS counter
	lw	t1,0(t0)		# current RPSS counter
	li	t2,625			# must refresh SIMMs for at least 8
3:	lw	t3,0(t0)		# lines before it's safe to use them,
	subu	t3,t3,t1		# which is approximately ~62.5us
	blt	t3,t2,3b

#if _MIPS_SIM == _ABI64				/* go form compat K1 to 64-bit K1 */
	.set	noreorder
	LA	k0,1f
	jr	k0
	nop
1:	nop
	.set	reorder
#endif

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
	jal	init_hpc3		# initialize HPC3 chip
        jal     StartAesTx		# Get the AES tx going early 
					# so the reciever pll can sync.
	jal	init_scsi		# init onboard SCSI interface
	jal	init_intX		# initialize INT2/INT3 chip

	LA	fp,pon_handler		# set BEV handler
	jal	pon_initio		# initialize duart for I/O

#ifdef	VERBOSE
	LA	a0,9f
	jal	pon_puts
	.set	noreorder
	mfc0	a0,C0_CONFIG
	.set	reorder
	jal	pon_puthex
	LA	a0,crlf
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
	LI	t0,PHYS_TO_K1(MEMCFG0)

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

	LI	t0,PHYS_TO_K1(CPUCTRL0)
	lw	t1,0(t0)
	and	t1,~CPUCTRL0_REFS_MASK
	or	t1,CPUCTRL0_REFS_4_LINES
	sw	t1,0(t0)

1:
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
#define ARCS_POWERDOWN	2		/* unused */
#define ARCS_RESTART	3
#define ARCS_REBOOT	4
#define ARCS_IMODE	5

LEAF(Halt)
	li	s0,ARCS_HALT
	j	arcs_common
	END(Halt)

LEAF(PowerDown)
#ifdef IP24
	jal	clear_prom_ram		# clear_prom_ram initializes sp
	li	a0,1
	jal	play_hello_tune		# play shutdown tune
	jal	wait_hello_tune
#endif
	/* immediately power down */
	j	powerdown
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
	LA	t0,_ftext
	LA	t1,_etext
	li	k0, 0x1fffffff
	and	t0,t0,k0		# KDM_TO_PHYS
	and	t1,t1,k0		# KDM_TO_PHYS
	and	ra,ra,k0		# KDM_TO_PHYS
	sgeu	t0,ra,t0
	sleu	t1,ra,t1
	and	t0,t0,t1
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
	LI	sp,PROM_STACK-(4*4)	# reset the stack
	sreg	sp,_fault_sp
	PTR_SUBU	sp,EXSTKSZ	# top 1/4 of stack for fault handling
	jal	main
	# if main fails fall through and re-init
1:
	# go whole hog
	li	s0,ARCS_IMODE
	j	arcs_common
	END(EnterInteractiveMode)

LEAF(arcs_common)
	.set	noreorder
	li	a0,SR_PROMBASE|ENET_SR_BEV
#ifdef ENETBOOT
	LA	fp,pon_handler		# set BEV handler
#endif
	mtc0	a0,C0_SR		# no interrupts
	.set	reorder
	jal	kill_hpc3_dma		# does what it says
        jal     StartAesTx              # Start up the AES TX early so reciver
                                        # can sync its PLL
	jal	clear_prom_ram		# clear_prom_ram initializes sp
	jal	gfxreset		# reset graphics
	move	a0,zero
	jal	init_prom_soft		# initialize saio and everything else

	bne	s0,ARCS_RESTART,1f
	jal	restart
	j	never			# never gets here

1:
	# restart cannot reset the soft power controls since it will
	# overwrite the alarm information for auto power on.
	jal	ip22_power_switch_on

	bne	s0,ARCS_IMODE,1f
	jal	main
	j	never			# never gets here
1:
	bne	s0,ARCS_HALT,1f
	jal	halt
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
#ifdef ENETBOOT
	LA	fp,pon_handler
#endif
	li	a0,SR_PROMBASE|ENET_SR_BEV
	mtc0	a0,C0_SR		# back to a known sr
	mtc0	zero,C0_CAUSE		# clear software interrupts
	.set	reorder

	lw	v0,bssmagic		# Check to see if bss valid
	bne	v0,BSSMAGIC,_init	# If not, do a hard init

	LI	sp,PROM_STACK-(4*4)	# reset the stack
	sreg	sp,_fault_sp
	PTR_SUBU	sp,EXSTKSZ	# top 1/4 of stack for fault handling
	jal	config_cache		# determine cache sizes
	jal	flush_cache		# flush cache
	jal	enter_imode
1:	b	1b			# never gets here
	END(_exit)

/*
 * notimplemented -- deal with calls to unimplemented prom services
 */
NOTIMPFRM=(4*SZREG)
NESTED(notimplemented, NOTIMPFRM, zero)
	subu	sp,NOTIMPFRM
	beqz	fp,1f
	move	a0,fp
	move	fp,zero
	j	a0
1:
	LA	a0,notimp_msg
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
	LA	fp,pon_handler
	li	t0,SR_PROMBASE|ENET_SR_BEV
	mtc0	t0,C0_SR
	nop
	li	t0, CONFIG_NONCOHRNT
	mtc0	t0,C0_CONFIG
	.set	reorder
	jal	clear_prom_ram		# clear_prom_ram initializes sp
	move 	a0,zero
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
INITMEMFRM=(4*SZREG)
NESTED(clear_prom_ram, INITMEMFRM, zero)
 	LA	a0,_fbss		# clear locore, bss, and stack
#ifndef ENETBOOT
 	LI	a1,PROM_STACK-4		# end of prom stack
#else
 	LA	a1,end			# clear bss only
#endif

	and	a0,0x9fffffff		# turn into cached addresses
	and	a1,0x9fffffff

#if _MIPS_SIM == _ABI64
	/* Last and really isolated phys addr, now do PHYS_TO_K0
	 */
	LI	v0,K0BASE
	or	a0,v0
	or	a1,v0
#endif

#ifdef R4000PC
	.set	noreorder
	mfc0	v0,C0_CONFIG
	nop
	nop
	.set	reorder
	and	v0,CONFIG_SC
	bnez	v0,2f
#endif
	move	v0,a0

	/* hit invalidate to prevent VCE during memory clear */
 	.set	noreorder
	LI	v1,0x400000|K0_RAMBASE	# maximum possible cache size
	LI	v0,K0_RAMBASE
1:	PTR_ADD	v0,256
	cache	CACH_SD|C_IINV,-256(v0)
	cache	CACH_SD|C_IINV,-192(v0)
	cache	CACH_SD|C_IINV,-128(v0)
	bltu	v0,v1,1b
	cache	CACH_SD|C_IINV,-64(v0)
	.set	reorder

#ifdef R4000PC
	b	3f

2:
	.set	noreorder
	mfc0	a2,C0_CONFIG

	LI	v1,0x1000		# cache unit size, 4K
	LI	v0,K0_RAMBASE		# cache base
	and	a3,a2,CONFIG_DC		# primary date cache size
	srl	a3,CONFIG_DC_SHFT
	sllv	v1,a3
	or	v1,v0

4:	PTR_ADD	v0,64
	cache	CACH_PD|C_IINV,-64(v0)
	cache	CACH_PD|C_IINV,-48(v0)
	cache	CACH_PD|C_IINV,-32(v0)
	bltu	v0,v1,4b
	cache	CACH_PD|C_IINV,-16(v0)

	LI	v1,0x1000		# cache unit size, 4K
	LI	v0,K0_RAMBASE		# cache base
	and	a3,a2,CONFIG_IC		# primary instruction cache size
	srl	a3,CONFIG_IC_SHFT
	sllv	v1,a3
	or	v1,v0

4:	PTR_ADD	v0,64
	cache	CACH_PI|C_IINV,-64(v0)
	cache	CACH_PI|C_IINV,-48(v0)
	cache	CACH_PI|C_IINV,-32(v0)
	bltu	v0,v1,4b
	cache	CACH_PI|C_IINV,-16(v0)
	.set	reorder

3:
#endif
	move	v0,a0
	move	v1,a1

 	.set	noreorder
1:	PTR_ADD	v0,64			# clear 64 bytes at a time
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

#ifdef R4000PC
	.set	noreorder
	mfc0	v0,C0_CONFIG
	nop
	nop
	.set	reorder
	and	v0,CONFIG_SC
	bnez	v0,2f
#endif

	/* hit writeback invalidate */
 	.set	noreorder
	LI	v1,0x400000|K0_RAMBASE	# maximum possible cache size
	LI	v0,K0_RAMBASE
1:	PTR_ADD	v0,256
	cache	CACH_SD|C_IWBINV,-256(v0)
	cache	CACH_SD|C_IWBINV,-192(v0)
	cache	CACH_SD|C_IWBINV,-128(v0)
	bltu	v0,v1,1b
	cache	CACH_SD|C_IWBINV,-64(v0)
	.set	reorder

#ifdef R4000PC
	b	3f

2:
	.set	noreorder
	mfc0	a2,C0_CONFIG

	LI	v1,0x1000		# cache unit size, 4K
	LI	v0,K0_RAMBASE		# cache base
	and	a3,a2,CONFIG_DC		# primary date cache size
	srl	a3,CONFIG_DC_SHFT
	sllv	v1,a3
	or	v1,v0

4:	PTR_ADD	v0,64
	cache	CACH_PD|C_IWBINV,-64(v0)
	cache	CACH_PD|C_IWBINV,-48(v0)
	cache	CACH_PD|C_IWBINV,-32(v0)
	bltu	v0,v1,4b
	cache	CACH_PD|C_IWBINV,-16(v0)
	.set	reorder
3:
#endif

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
        LI      sp,PROM_STACK-(4*SZREG)
        sreg    sp,_fault_sp            # base of fault stack
        PTR_SUBU        sp,EXSTKSZ      # top 1/4 of stack for fault handling
        PTR_SUBU        sp,INITMEMFRM
        sreg    ra,INITMEMFRM-SZREG(sp)

	jal	szmem			# determine main memory size
	sw	v0,memsize
        lreg    ra,INITMEMFRM-SZREG(sp)
        PTR_ADDU        sp,INITMEMFRM
	j	ra
	END(init_prom_ram)

/* 
 * initialize HPC configuration
 */
LEAF(init_hpc3)
/*		Set endianness
*/
        LI      t0,PHYS_TO_K1(HPC3_MISC_ADDR)
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
	LI      t0,PHYS_TO_K1(HPC3_EXT_IO_ADDR)
	li	t2,EXTIO_GIO_33MHZ
#endif
	lw	t1,0(t0)
	and	t2,t1
	beq	t2,zero,1f

#ifndef	IP24
	LI	t0,PHYS_TO_K1(HPC3_SCSI_PIOCFG1)	# set scsi 1 to 33MHZ
#ifdef IP22_WD95
	li	t1,WD95_PIO_CFG_33MHZ        
#else
	li	t1,WD93_PIO_CFG_33MHZ        
#endif
	sw	t1,0(t0)
#endif

	LI	t0,PHYS_TO_K1(HPC3_SCSI_PIOCFG0)	# set scsi 0 to 33MHZ
#ifdef IP22_WD95
	li	t1,WD95_PIO_CFG_33MHZ        
#else
	li	t1,WD93_PIO_CFG_33MHZ        
#endif
	sw	t1,0(t0)

	b	2f
1:
#ifndef	IP24
        LI      t0,PHYS_TO_K1(HPC3_SCSI_PIOCFG1)       # set scsi chan1 to 25MHZ
#ifdef IP22_WD95
        li      t1,WD95_PIO_CFG_25MHZ	
#else
        li      t1,WD93_PIO_CFG_25MHZ	
#endif
        sw      t1,0(t0)
#endif

        LI      t0,PHYS_TO_K1(HPC3_SCSI_PIOCFG0)       # set scsi chan0 to 25MHZ
#ifdef IP22_WD95
        li      t1,WD95_PIO_CFG_25MHZ	
#else
        li      t1,WD93_PIO_CFG_25MHZ	
#endif
        sw      t1,0(t0)

2:

/* 	Configure INT2 (IP22 only)
 *      (This is an ISDN channel on IP24)
*/
#ifndef	IP24
	LI	t0,PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(4)) 
        li      t1,0x4e27
        sw      t1,0(t0)
#endif


/*	Configure IO channel
*/
	LI	t0,PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(6))
	beq	t2,zero,1f		# t2 still has valid gio speed
        li      t1,0x988d		# setup 33mhz gio value for c6
	b	2f
	
1:
        li      t1,0x946d		# setup 25mhz gio value for c6
2:
        sw      t1,0(t0)
	LI	t0,PHYS_TO_K1(HPC3_WRITE1)	/* reset hi */
#if	IP24
        li      t1,(LED_RED_OFF|PAR_RESET|KBD_MS_RESET)
#else
        li      t1,(LED_GREEN|PAR_RESET|KBD_MS_RESET|EISA_RESET)
#endif
        sw      t1,0(t0)

/*	Configure extended register 
*/
	LI	t0,PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(5)) 
        li      t1,0x44e27
        sw      t1,0(t0)

/*	Configure PIO channels for audio devices
*/
	/* Channel 0 = HAL2 chip */
	LI	t0,PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(0))
	li	t1,0x48a45
	sw	t1,0(t0)

	/* Channel 1 = AES chips */
	LI	t0,PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(1))
	li	t1,0x88e47
	sw	t1,0(t0)

	/* Channel 2 = MDAC volume control */
	LI	t0,PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(2))
	li	t1,0x11289
	sw	t1,0(t0)

	/* Channel 3 = synthesizer chip */
	LI	t0,PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(3))
	li	t1,0xc4a25
	sw	t1,0(t0)

/*	Configure PBUS DMA channels to the best of our ability.
	Audio uses PBUS DMA channels 1-4, parallel port using
	channel 0.
	On IP24, we use 0-3 for audio, 4-5 for ISDN, 6 for plp.

*/
	LI	t0,PHYS_TO_K1(HPC3_PBUS_CFGDMA_ADDR(1))
	li	t1,0x8248844
	sw	t1,0(t0)

	LI	t0,PHYS_TO_K1(HPC3_PBUS_CFGDMA_ADDR(2))
	sw	t1,0(t0)

	LI	t0,PHYS_TO_K1(HPC3_PBUS_CFGDMA_ADDR(3))
	sw	t1,0(t0)

#ifndef	IP24
	LI	t0,PHYS_TO_K1(HPC3_PBUS_CFGDMA_ADDR(4))
	sw	t1,0(t0)
#endif

	j	ra
	END(init_hpc3)

/*
 * nuke ALL the DMA channels on the hpc3...parallel port and all audio.
 * this allows us to use the hpc3 DMA for the shutdown tune.
 */
LEAF(kill_hpc3_dma)
	LI	t0,PHYS_TO_K1(HPC3_ETHER_MISC_ADDR)
	li	t1,0x1		# put enet controller in a reset state
	sw	t1,0(t0)

	LI	t0,PHYS_TO_K1(HPC3_PBUS_CONTROL_ADDR(0))
	li	t1,0x20
	sw	t1,0(t0)

	LI	t0,PHYS_TO_K1(HPC3_PBUS_CONTROL_ADDR(1))
	sw	t1,0(t0)

	LI	t0,PHYS_TO_K1(HPC3_PBUS_CONTROL_ADDR(2))
	sw	t1,0(t0)

	LI	t0,PHYS_TO_K1(HPC3_PBUS_CONTROL_ADDR(3))
	sw	t1,0(t0)

	LI	t0,PHYS_TO_K1(HPC3_PBUS_CONTROL_ADDR(4))
	sw	t1,0(t0)

	LI	t0,PHYS_TO_K1(HPC3_PBUS_CONTROL_ADDR(5))
	sw	t1,0(t0)

	LI	t0,PHYS_TO_K1(HPC3_PBUS_CONTROL_ADDR(6))
	sw	t1,0(t0)

	LI	t0,PHYS_TO_K1(HPC3_PBUS_CONTROL_ADDR(7))
	sw	t1,0(t0)

	j	ra

	END(kill_hpc3_dma)
/*
 * Start the AES Transmitter going very early on so that equipment
 * downstream can lock to it before the boot tune plays.
 */
#define AESTX_DMA_CHAN  2

LEAF(StartAesTx)

/* Skip AES initialization if no audio board is installed */

	LI	t1, PHYS_TO_K1(HAL2_REV)
	lw	t0,0(t1)
	and	t0,t0,0x8000
	bnez	t0,no_audio

	/* Take the HAL2 out of reset */

	li	t0, HAL2_ISR_CODEC_RESET_N|HAL2_ISR_GLOBAL_RESET_N;
	LI	t1, PHYS_TO_K1(HAL2_ISR)
	sw	t0, 0(t1)

	/* Set up AES TX */

	li	t0, AESTX_DMA_CHAN+(1 /*bres clock 1*/ << 3)+(1/*mono*/ << 8);
	LI	t1, PHYS_TO_K1(HAL2_IDR0)
	sw	t0, 0(t1)
	li	t0, HAL2_AESTX_CTRL_W;
	LI	t1, PHYS_TO_K1(HAL2_IAR)
	sw	t0, 0(t1)
spinloop1:
	LI	t1, PHYS_TO_K1(HAL2_ISR)
	lw	t0, 0(t1)
	and	t0, t0, 1
	bnez	t0, spinloop1

	/* Set up BRES CLOCK 1 source */

	li	t0, 1   /* 1 = 44.1kHz master */
	LI	t1, PHYS_TO_K1(HAL2_IDR0)
	sw	t0, 0(t1)
	li	t0, HAL2_BRES1_CTRL1_W
	LI	t1, PHYS_TO_K1(HAL2_IAR)
	sw	t0, 0(t1)
spinloop2:
	LI	t1, PHYS_TO_K1(HAL2_ISR)
	lw	t0, 0(t1)
	and	t0, t0, 1
	bnez	t0, spinloop2

	/* Set up BRES CLOCK 1 inc and modctrl */

	li	t0, 1				/* idr0=inc=1 */
	LI	t1, PHYS_TO_K1(HAL2_IDR0)
	sw	t0, 0(t1)
	li	t0, 0xffff			/* idr1=modctrl=0xffff */
	LI	t1, PHYS_TO_K1(HAL2_IDR1)
	sw	t0, 0(t1)
	li	t0, HAL2_BRES1_CTRL2_W		/* iar=bres1 ctrl reg 2 */
	LI	t1, PHYS_TO_K1(HAL2_IAR)
	sw	t0, 0(t1)
spinloop3:
	LI	t1, PHYS_TO_K1(HAL2_ISR)
	lw	t0, 0(t1)
	and	t0, t0, 1
	bnez	t0, spinloop3

        /* Configure the channel status bits on the AES Tx Chip */
	LI	t1,PHYS_TO_K1(AESTX_CS0)
	li	t0,0x4;
	sw	t0,0(t1)
	LI	t1,PHYS_TO_K1(AESTX_CS1)
	sw	zero,1*4(t1)
	sw	zero,2*4(t1)
	sw	zero,3*4(t1)
	sw	zero,4*4(t1)
	sw	zero,5*4(t1)
	sw	zero,6*4(t1)
	sw	zero,7*4(t1)
	sw	zero,8*4(t1)
	sw	zero,9*4(t1)
	sw	zero,10*4(t1)
	sw	zero,11*4(t1)
	sw	zero,12*4(t1)
	sw	zero,13*4(t1)
	sw	zero,14*4(t1)
	sw	zero,15*4(t1)
	sw	zero,16*4(t1)
	sw	zero,17*4(t1)
	sw	zero,18*4(t1)
	sw	zero,19*4(t1)
	sw	zero,20*4(t1)
	sw	zero,21*4(t1)
	sw	zero,22*4(t1)
	sw	zero,23*4(t1)

        /* Activate the TX Chip */
	LI	t1,PHYS_TO_K1(AESTX_CR3);
	li	t0,0x9;
	sw	t0,0(t1)
	LI	t1,PHYS_TO_K1(AESTX_CR1);
	sw	zero,0(t1)
	LI	t1,PHYS_TO_K1(AESTX_CR2);
	li	t0,0x83;
	sw	t0,0(t1)
no_audio:
	j	ra	
	END(StartAesTx)

/* 
 * initialize INT2/INT3 configuration
 *
 * NBI, maintain HPC_INT_ADDR in (t2)
 */
LEAF(init_intX)
#ifdef	IP24
	li	t2,PHYS_TO_K1(HPC3_INT3_ADDR)	# IOC1/INT3
#else
	LI	t2,PHYS_TO_K1(HPC3_INT3_ADDR)	# assume IOC1/INT3
	IS_IOC1(t0)
	bnez	t0, 1f				# branch if IOC1/INT3
	LI	t2,PHYS_TO_K1(HPC3_INT2_ADDR)	# use INT2
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

	LI	t0,PHYS_TO_K1(HPC3_WRITE2)	# configure duart clocks
	li	t1,UART1_ARC_MODE|UART0_ARC_MODE# low=RS422 MAC mode,hi=RS232
	sw	t1,0(t0)

#ifdef IP24
	li	t1,(POWER_SUP_INHIBIT|POWER_INT|PANEL_VOLUME_UP_ACTIVE|PANEL_VOLUME_UP_INT|PANEL_VOLUME_DOWN_ACTIVE|PANEL_VOLUME_DOWN_INT)
#else
	li	t1,((POWER_SUP_INHIBIT|POWER_INT)&0xff)
#endif
	LI	t0,PHYS_TO_K1(HPC3_PANEL)	# power on and clear intr
	sw	t1,0(t0)

	j	ra
	nop
	.set 	reorder
	END(init_intX)

/*
 * Do NOT reset the bus.  A reset is done by the act of powering up
 * or resetting.  It shouldn't be needed from software.  If there is an
 * interrupt, clear it, but don't wait for one.
 */
LEAF(init_scsi)
	.set 	noreorder
	/* clear the scsi reset line; HPC latches it on reset/powerup */
#ifndef IP24
	LI	t0,PHYS_TO_K1(SCSI1_CTRL_ADDR)
	sw	zero,0(t0)
#endif
	LI	t0,PHYS_TO_K1(SCSI0_CTRL_ADDR)
	sw	zero,0(t0)


	/* clear any SCSI interrupts by reading the status register on the
	   Western Digital SCSI controller chip
	   Then issue a reset command.  This is needed because a chip reset
	   doesn't clear the registers, and we now look at some of them
	   on startup, to handle kernel to PROM transitions.  CMD register
	   follows status, and register # is auto-incremented, so don't
	   need to write the register # (RESET is cmd 0). Need 7 usec between
	   status register read and cmd; we execute about 1 cmd/usec in IP22 
	   PROM.
	*/

	/* RESET SCSI0 */
	li	t1,0x17				# SCSI reg 0x17 - status
	LI	t0,PHYS_TO_K1(SCSI0A_ADDR)
	sb	t1,0(t0)
	LI	t0,PHYS_TO_K1(SCSI0D_ADDR)
	lb	zero,0(t0)
#if 1
	nop; nop; nop; nop; nop; nop; nop; nop
#else
	.set	noreorder
	li	t2, 1000
1:	beqz	t2, 1b
	sub	t2, 1
	.set	reorder
#endif
	sb	zero,0(t0) # reset the chip; clears sync register, etc. 
#if 1
	nop; nop; nop; nop; nop; nop; nop; nop
#else
	.set	noreorder
	li	t2, 1000
1:	beqz	t2, 1b
	sub	t2, 1
	.set	reorder
#endif

#if 0
/* try a "hard" reset of the sync register */
	li	t1,WD93SYNC
	li	t0,PHYS_TO_K1(SCSI0A_ADDR+0x3)
	sb	t1,0(t0)
	li	t0,PHYS_TO_K1(SCSI0D_ADDR+0x3)
	sb	zero,0(t0)
#endif

#ifndef IP24
	/* RESET SCSI1 */
	li	t1,0x17				# SCSI reg 0x17 - status
	LI	t0,PHYS_TO_K1(SCSI1A_ADDR)
	sb	t1,0(t0)
	LI	t0,PHYS_TO_K1(SCSI1D_ADDR)
	lb	zero,0(t0)
	nop; nop; nop; nop; nop; nop; nop; nop
	sb	zero,0(t0) # reset the chip; clears sync register, etc. 
#endif

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
	/* Check lcl1 interrupt for power switch press -- don't touch RAM! */
#ifdef IP24
	li	k1,PHYS_TO_K1(HPC3_INT3_ADDR)	# IOC1/INT3
#else
	LI	k1,PHYS_TO_K1(HPC3_INT3_ADDR)	# assume IOC1/INT3
	IS_IOC1(k0)
	bnez	k0, 1f				# branch if IOC1/INT3
	LI	k1,PHYS_TO_K1(HPC3_INT2_ADDR)	# use INT2
1:
#endif

	/* check if we have a power interrupt */
	lb	k0,LIO_1_ISR_OFFSET(k1)
	andi	k0,LIO_POWER
	beqz	k0,3f				# branch if NOT a button

#ifdef IP24	/* on guinness check which button was pressed */
	li	k1,PHYS_TO_K1(HPC3_PANEL)
	lw	k0,0(k1)
	andi	k0,POWER_INT			# check if power or volume
	beqz	k0,powerdown			# branch if power

	/* must have rec'd a volume button interrupt, clear it here */
	li	k0,POWER_ON	
	sb	k0,0(k1)			# clear interrupt(s)
	.set	noreorder
	eret					# resume execution
	nop
	.set	reorder
#endif

	/* Switch hit, turn off watchdog timer -- since we are gonna
	 * kill power anyway, feel free to use more than just k[01].
	 */
powerdown:
	LI	v0,PHYS_TO_K1(HPC3_PBUS_RTC_1286)
	lw	k0,0x2c(v0)			# read command
	ori	k0,k0,WTR_DEAC_WAM		# deactivate watchdog
	sw	k0,0x2c(v0)			# store command
	WBFLUSHM
	sw	zero,0x30(v0)			# zero hundreths
	WBFLUSHM
	sw	zero,0x34(v0)			# zero seconds
	WBFLUSHM

#ifndef IP24		/* needs to be ported to Indy/INT3 */
	/* james tornes and brad reger: don't change this */
3:
	/* wait for power switch to be released */
	LI	k0,PHYS_TO_K1(HPC3_PANEL)
	li	k1,POWER_ON
	sw	k1,0(k0)
	WBFLUSHM

	/* delay ~5ms */
	.set	noreorder
	li	k0,1000000
1:	bne	k0,zero,1b
	sub	k0,1
	.set	reorder

	/* wait for interrupt to go away */
	LI	k0,PHYS_TO_K1(HPC3_INT2_ADDR+LIO_1_ISR_OFFSET)
	lb	k1,0(k0)
	and	k1,LIO_POWER
        bne	k1,zero,2b
#endif

	/* Now go ahead and power down */
hitpow:	li	k0,~POWER_SUP_INHIBIT
	LI	k1,PHYS_TO_K1(HPC3_PANEL)
	sw	k0,0(k1)
	WBFLUSHM

delay:	li	v1,10				# wait ~10us
	lw	k0,0x2c(v0)			# read command register
	addi	v1,-1
	bgt	zero,v1,delay

	/* If power not out, maybe "wakeupat" time hit the window.
	 */
	andi	v1,k0,WTR_TDF			# k0 holds command from delay
	beq	v1,zero,hitpow

	/* Disable and clear alarm.
	 */
	ori	k0,k0,WTR_DEAC_TDM		# k0 holds command from delay
	sw	k0,0x2c(v0)			# deactive timer
	WBFLUSHM
	lw	zero,0x14(v0)			# read hour to clear alarm

	j hitpow				# try power again

2:	li	a0,1
	b	bev_common
	END(bev_general)

LEAF(bev_cacheerror)
	li	a0,2
	b	bev_common
	END(bev_cacheerror)

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
1:

#if IP24
	.set	noreorder
/* write sentinel and various cpu regs to fixed address for LA debugging */
	sw	a0,0xbfbd98e0

	li	v0,0xf1f1
	sw	v0,0xbfbd98e0

	mfc0	t0,C0_EPC
	mfc0	t1,C0_CAUSE
	sw	t0,0xbfbd98e0
	sw	t1,0xbfbd98e0

	mfc0	t0,C0_CTXT
	mfc0	t1,C0_BADVADDR
	sw	t0,0xbfbd98e0
	sw	t1,0xbfbd98e0

	mfc0	t0,C0_TLBLO
	mfc0	t1,C0_TLBLO_1
	sw	t0,0xbfbd98e0
	sw	t1,0xbfbd98e0

	mfc0	t0,C0_TLBHI
	lw	t1,CPU_ERR_ADDR
	sw	t0,0xbfbd98e0
	sw	t1,0xbfbd98e0

	lw	t0,CPU_ERR_STAT
	sw	t0,0xbfbd98e0
	lw	t0,GIO_ERR_ADDR
	sw	t0,0xbfbd98e0

	lw	t0,GIO_ERR_STAT
	sw	t0,0xbfbd98e0
	lw	t0,DMA_CAUSE
	sw	t0,0xbfbd98e0

	lw	t0,HPC3_BUSERR_STAT_ADDR
	sw	t0,0xbfbd98e0
	lw	t0,HPC3_INTSTAT_ADDR
	sw	t0,0xbfbd98e0

	lw	t0,HPC3_BUSERR_STAT_ADDR
	sw	t0,0xbfbd98e0
	lw	t0,HPC3_INT3_ADDR+LIO_0_ISR_OFFSET
	sw	t0,0xbfbd98e0

	lw	t0,HPC3_INT3_ADDR+LIO_1_ISR_OFFSET
	sw	t0,0xbfbd98e0
	lw	t0,HPC3_INT3_ADDR+LIO_2_3_ISR_OFFSET
	sw	t0,0xbfbd98e0
	.set	reorder
#endif

2:	b	2b
	END(bev_common)

/*******************************************************************/
/* 
 * Data area
 *
 */
	BSS(bssmagic,4)
	BSS(simmerrs,4)
