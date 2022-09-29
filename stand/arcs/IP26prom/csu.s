#ident	"IP26prom/csu.s:  $Revision: 1.80 $"

/*
 * csu.s -- prom startup code
 */

#include <fault.h>
#include <asm.h>
#include <regdef.h>
#include <sys/cpu.h>
#include <sys/ds1286.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#include <sys/i8254clock.h>
#include <sys/hal2.h>

#define BSSMAGIC		0x0deadbed

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

LEAF(start)				# 0x900000001fc00000
	/*  The IP22 PAL doesn't work in power-on mode with the teton module
	 * and the MC thinks the HPC3 is a 32-bit device.  This causes us
	 * to see even instruction twice.  To get around that we reset the
	 * machine by writing to CPUCTRL0 we skip the jump to skip the
	 * hack.  The machine seems to work correctly after reset (aka the
	 * R4400 hack sees new life).
	 *
	 *  The code to initialize C0 early is questionably legal since it
	 * issues dmtc0's back-to-back.
	 */
	.set	noreorder
	.set	noat
	dmtc0	zero,C0_CAUSE			# clear interrupts
	ssnop					# odd
	dmtc0	zero,C0_SR			# disable interrupts
	ssnop					# odd
	nop					# seen 2x on bad system
	b	1f				# not seen on bad system
	lui	t0,0x9000			# 32 bits short of K1BASE
	nop					# odd
	dsll	t0,16				# 16 bits short of K1BASE
	nop					# odd
	lui	t1,0x1fa0			# MCBASE
	nop					# odd
	or	t0,t1				# PHYS_TO_K1(0x1fa00000)
	nop					# odd
	lui	t2,0x0001			# MC default +SIN bit
	nop					# odd
	ori	t2,0x020a
	nop					# odd
	sw	t2,0x04(t0)			# write to BE CPUCTRL0
	nop					# odd
	sw	t2,0x0(t0)			# write to LE CPUCTRL0
	nop					# odd
	ssnop
	ssnop
	.set	at

1:	LI	t0, SR_CU0|SR_CU1|SR_PAGESIZE
	dmtc0	t0, C0_SR
	ssnop

	LA	t0,_trap_table		# init TrapBase register (uncached)
	li	t1,0x3fffffff
	and	t0,t1
	LI	t1,K1BASE
	or	t0,t1
	dmtc0	t0,C0_TRAPBASE
	ssnop
	LA	t0,pon_handler		# load power-on BEV hander
	dmtc0	t0,TETON_BEV
	ssnop
	ssnop
	dmtc0	zero,TETON_GERROR	# zero error count
	.set	reorder

	ctc1	zero,$31		# clear floating point status register

	jal	check_scachesize	# calculate scache tag ram size
	LI	t0,PHYS_TO_K1(IP26_GCACHE_SIZE_ADDR)
	sb	v0,0(t0)		# save gcache size

	/* Some CPU initialization.
	 */
	jal	init_C0			# Initialize tfp C0
	jal	_flush_tlb		# Clear out the TLB
	jal	pon_invalidate_IDcaches	# Invalidate I&D caches
	jal	flush_SAQueue		# Initialize/flush SAQs

	/* Jump to Chandra mode for early pon [GCache tests depend on it].
	 */
	LA	t0,1f			# target of jump
	lui	t1,0x7fff		# physical address mask
	ori	t1,0xffff
	and	t0,t1			# get physical address of 1f
	li	t1,0x80			# chandra mode
	dsll32	t1,24			# shift into position
	or	t0,t1			# finally chandra 1f
	jr	t0			# jump to chandra mode
 	.align	3			# make sure we are on a new quad
1:	LI	t0,PHYS_TO_K1(TCC_FIFO)	# mysterious hardware WAR from IP21
	ld	zero,0(t0)
	ld	zero,0(t0)
	jal	inval_gcache_tags	# Invalidate G-cache

	/* Start MC configuration
	 */
	LI	t0,PHYS_TO_K1(CPUCTRL0)			# MC base
	sw	zero,CPU_ERR_STAT-CPUCTRL0(t0)		# clear CPU bus error
	sw	zero,GIO_ERR_STAT-CPUCTRL0(t0)		# clear GIO bus error
	lw	zero,PHYS_TO_K1(HPC3_INTSTAT_ADDR)	# clear HPC3 bus error

	lw	zero,0(t0)				# WBFLUSHM

#define	CPUCTRL0_REFS_4_LINES	0x00000002
#define	CPUCTRL0_REFS_8_LINES	0x00000004

	/*  Set up CPUCTRL0.  Set MUX_HWM to 12 words.  Temporarily set
	 * refresh to 8 lines.
	 */
	li	t1,0x3d402010|CPUCTRL0_REFS_8_LINES
	sw	t1,0(t0)				# CPUCTRL0
	lw	zero,0(t0)				# WBFLUSHM

	/* set MC_HWM to 0x6, enable GIO bus time out
	 */
	li	t1,CPUCTRL1_ABORT_EN|0x6
	sw	t1,CPUCTRL1-CPUCTRL0(t0)		# CPUCTRL1
	lw	zero,0(t0)				# WBFLUSHM

	/* Teton always has a 007+ board, which uses the 004 GIO set-up.
	 */
	lw	t1,_gio64_arb_004
	sw	t1,GIO64_ARB-CPUCTRL0(t0)		# GIO64_ARB
	lw	zero,0(t0)				# WBFLUSHM

#define	GIO_MEMACC_PREIP22E	0x00034322

#if _MC_MEMORY_PARITY
/* no ECC baseboard - run memory system @ full speed */
#define	CPU_MEMACC_DEFAULT	0x11453433
#define	GIO_MEMACC_DEFAULT	GIO_MEMACC_PREIP22E
#else
/* ECC baseboard in 'slow' mode, which allows normal uncached access to
 * memory.  The kernel later turns on fast mode when it is ready not
 * access memory uncached.
 */
#define	CPU_MEMACC_DEFAULT_NORMAL	CPU_MEMACC_NORMAL
#define CPU_MEMACC_DEFAULT		CPU_MEMACC_SLOW
#define	GIO_MEMACC_DEFAULT		0x00034336
#define	ECC_MEMCFG_DEFAULT		(0x2000|(ECC_CTRL_BASE>>24))
#endif

	/*  Set up CPU_MEMACC and GIO_MEMACC.  Assume we have a IP26 (ECC)
	 * board for now -- we will later if need be.
	 */
	li	t1,CPU_MEMACC_DEFAULT
	sw	t1,CPU_MEMACC-CPUCTRL0(t0)		# CPU_MEMACC
	li	t1,GIO_MEMACC_DEFAULT			# IP26 (ECC)
	sw	t1,GIO_MEMACC-CPUCTRL0(t0)		# GIO_MEMACC

	# set up as 100ns counter for 50Mhz MC
	li	t1,0x0104
	sw	t1,RPSS_DIVIDER-CPUCTRL0(t0)		# RPSS_DIVIDER

	/* Must refresh SIMMs for atleast 8 lines before it is safe to
	 * use them.  This is ~62.5us.
	 */
	lw	t1,RPSS_CTR-CPUCTRL0(t0)		# current RPSS_CTR
	li	t2,625					# 62.5us
3:	lw	t3,RPSS_CTR-CPUCTRL0(t0)		# RPSS_CTR
	subu	t3,t3,t1
	blt	t3,t2,3b				# loop till delta>=625

	/* Do some TCC initialization:
	 *	- turn on TCC bus error and machine check interrupts
	 *	- clear error register
	 *	- enable even parity checking
	 */
	LI	t0,PHYS_TO_K1(TCC_BASE)
	li	t1,INTR_BUSERROR_EN|INTR_MACH_CHECK_EN
	sd	t1,TCC_INTR-TCC_BASE(t0)		# TCC_INTR
	sd	zero,TCC_ERROR-TCC_BASE(t0)		# TCC_ERROR
	li	t1,PAR_TDB_EN|PAR_TCC_EN|PAR_DATA_TDB_EN
	sd	t1,TCC_PARITY-TCC_BASE(t0)		# TCC_PARITY

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

	/*  Check if we have a pre-IP26 baseboard we need to set the
	 * GIO_MEMACC settings back to the fast settings to avoid the
	 * VINO bug.  We would like to do this when originally setting
	 * up the MC, but we need to read the SYS_ID register, and we
	 * have to init HPC3 first.
	 */
	LI	t3,PHYS_TO_K1(HPC3_SYS_ID)	# board revision info
	lw	t3,0(t3)
	andi	t3,BOARD_REV_MASK		# isolate board rev
	sub	t3,IP26_ECCSYSID		# first ecc bd = 0x18
	bgez	t3,1f				# skip setting if IP26
	LI	t2,PHYS_TO_K1(CPUCTRL0)		# MC
	li	t3,GIO_MEMACC_PREIP22E		# old timing
	sw	t3,GIO_MEMACC-CPUCTRL0(t2)	# store new (old) GIO_MEMACC
	lw	zero,0(t2)			# flushbus
1:

	jal	pon_initio		# re-initialize duart
        jal     StartAesTx		# Get the AES tx going early 
	jal	init_scsi		# init onboard SCSI interface
	jal	init_int2		# initialize INT2
	jal	init_hpc3_scsi		# initialize scsi hpc3 settings

	/*  Run GCache diagnostics.
	 */
	VPRINTF("\r\nRunning scache_tag...\r\n");
	jal	scache_tag
	jal	scache_tag2
	jal	scache_tag3
	VPRINTF("Running scache_data...\r\n");
	jal	scache_data
	jal	scache_data2

	jal	inval_gcache_tags		# re-initialize tags

	/*  Check TETON_GERROR to see how the GCache diagnostics have done:
	 *	- if only some tests failed disable those and continue,
	 *	  jumping to final cache mode.
	 *	- if all sets look bad, force set3 and try to run in
	 *	  the current (chandra uncached) mode as long as possible.
	 *	- in all cases, we now turn on writebacks.
	 */
	dli	t0,PHYS_TO_K1(TCC_GCACHE)	# GCache control
	.set	noreorder
	dmfc0	t2,TETON_GERROR			# get error counts
	ssnop
	.set	reorder
	dsrl	t2,GERROR_SET_SHIFT		# get failing sets
	not	t2				# enable good sets
	andi	t2,0xf				# mask off unused stuff
	bnez	t2,1f				# all sets bad -> force set 3
	li	t2,GCACHE_FORCE_SET3		# force set 3, all sets off
1:	li	t1,GCACHE_RR_FULL|GCACHE_WB_RESTART
	or	t1,t2
	sd	t1,0(t0)
	ld	zero,16(t0)			# flush it out
	
#if TEXTADDR != 0x800000001fc00000		/* !Chandra PROM */
	andi	t2,GCACHE_FORCE_SET3		# skip cache mode change if
	bnez	t2,2f				# cache is bad (force set3 set)

	LA	t0,71f
	jr	t0
	.align	3			# make sure we are on another quad
#endif	/* !Chandra PROM */
71:	jal	pon_invalidate_IDcaches	# re-invalidate I&D caches
2:

	/* Ready to run with TrapBase cached */
	.set	noreorder
	LA	t0,_trap_table
	dmtc0	t0,C0_TRAPBASE
	ssnop
	.set	reorder

	/* Set up the ECC hardware
	 * The tricky part is that there are 2 IDT49C466 chips, sitting
	 * on different DMUX's, but they have to be written on the same
	 * cycle.  The only way to guarantee this is by doing the writes to
	 * cache, then flushing.
	 */
	/* Check for board rev indicating presence of ECC baseboard */
	LI	t0,PHYS_TO_K1(HPC3_SYS_ID)	# board revision info
	lw	t0,0(t0)
	andi	t0,BOARD_REV_MASK		# isolate board rev
	sub	t0,0x18				# first ecc bd = 0x18
	bltz	t0,no_ecc			# skip setting if no ECC

	/* set_up_bank3() -- Stole code from set_mem_conf in lmem_conf.s */
	li	a0,3				# Bank 3
	li	a1,ECC_MEMCFG_DEFAULT		# Enabled, at 0x60000000
	LI	t0,PHYS_TO_K1(MEMCFG1)		# addr of MEMCFG1
	lw	t1,0(t0)		# content of mem config register
	lui	t2,0xffff		# load mask for lower half word
	and	t1,t2			# mask out the old content
	or	t1,a1			# or in the new content
	sw	t1,0(t0)		# save content of config reg
	lw	zero,CPUCTRL0-MEMCFG1(t0)	# WBFLUSHM

	/*disable_errors_in_ctrl_reg()*/
	LI	t0,PHYS_TO_K1(ECC_CTRL_REG)
	LI	t2,PHYS_TO_K1(CPUCTRL0)
	li	t1,ECC_CTRL_DISABLE		# Disable ECC reporting.
	sd	t1,0(t0)			# ""
	lw	zero,0(t2)			# Flush bus
	li	t1,ECC_CTRL_CLR_INT		# Clear small write errors.
	sd	t1,0(t0)			# ""
	lw	zero,0(t2)			# Flush bus
	
	/* Figure out where we're running, then write to a cached address
	 * where we know that code fetches won't interleave with our
	 * cached writes.  This is okay, since the ECC registers are
	 * replicated on every cache line throughout bank 3.
	 * Each cache set is 512KB.  Devide this into 4 quadrants, and
	 * pick the quadrant opposite the one where we are executing.
	 */
	LA	t3,2f
2:	lui	t1,0x0006			# Mask for quadrants in 512K
	lui	t2,0x0004			# 256K
	xor	t3,t2				# Move to opposite quadrant
	and	t3,t1				# Opposite quadrant now in t3
	
	/*cached_write_chip0()*/
	/*cached_write_chip1_and_ctrl()*/
	LI	t0,PHYS_TO_K0(ECC_CTRL_BASE)
	daddu	t0,t3				# Shift to quadrant from above
	LI	t1,ECC_DEFAULT			# Defalut is w/checking off.
	sd	t1,0(t0)			# Write 49C466 0
	sd	t1,8(t0)			# Write 49C466 1
	sd	t1,16(t0)			# Replicate the above
	sd	t1,24(t0)			#  writes throughout the
	sd	t1,32(t0)			#  cache line.  Each quad-
	sd	t1,40(t0)			#  word actually writes
	sd	t1,48(t0)			#  the ECC parts!
	sd	t1,56(t0)			# 
	sd	t1,64(t0)			# 
	sd	t1,72(t0)			# 
	sd	t1,80(t0)			# 
	sd	t1,88(t0)			# 
	sd	t1,96(t0)			# 
	sd	t1,104(t0)			# 
	sd	t1,112(t0)			# 
	sd	t1,120(t0)			# 

	/*wb_cache()*/
	LI	t3,PHYS_TO_K1(TCC_CACHE_OP|TCC_DIRTY_WB|TCC_INVALIDATE)
	and	t0,TCC_PHYSADDR
	or	t3,t0
	ld	zero,0(t3)
	
	/* set_up_ctrl_reg()
	 * Already done, since we want to leave it disabled here.
	 */

no_ecc:
	/*  Clear all parity errors that may have been generated during or
	 * around scache diagnostics.
	 */
	.set	noreorder
	dmtc0	zero,C0_CAUSE		# clear parity GCache errors from pon
	.set	reorder
	LI	t0,PHYS_TO_K1(TCC_INTR)	# clear TCC bus errors/machine checks
	ld	t1,0(t0)
	sd	t1,0(t0)

#ifndef ENETBOOT
	VPRINTF("Running TCC/MC/HPC data path diagnostic.\r\n")
	jal	pon_mc			# test path to MC
#endif
	jal	pon_regs		# test HPC data paths

	VPRINTF("Sizing memory and running memory diagnostic.\r\n")
	jal	init_memconfig		# initialize memory configuration

	/*  Refresh 4 lines if not using 256Kx36 or 512Kx36 SIMMs.  Skip
	 * check on rev D MC as these SIMMS are no longer supported.  We
	 * always refresh 4 lines.
	 */
	LI	t0,PHYS_TO_K1(CPUCTRL0)

	lw	t1,SYSID-CPUCTRL0(t0)			# get SYSID
	andi	t1,SYSID_CHIP_REV_MASK			# mask off rev
	addi	t1,-5					# rev D == 5
	bgez	t1,2f					# skip check if D+

	lw	t1,MEMCFG0-CPUCTRL0(t0)			# MEMCFG0
	and	t2,t1,MEMCFG_VLD|0x0200
	beq	t2,MEMCFG_VLD,1f
	srl	t1,16
	and	t2,t1,MEMCFG_VLD|0x0200
	beq	t2,MEMCFG_VLD,1f

	lw	t1,MEMCFG1-CPUCTRL0(t0)			# MEMCFG1
#if 0	/* No bank 3 on Indigo2, and potentially dangerous w/ECC HW in bank3 */
	and	t2,t1,MEMCFG_VLD|0x0200
	beq	t2,MEMCFG_VLD,1f
#endif
	srl	t1,16
	and	t2,t1,MEMCFG_VLD|0x0200
	beq	t2,MEMCFG_VLD,1f

2:	lw	t1,0(t0)				# CPUCTRL0
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
#ifdef GOOD_COMPILER			/* why is this always broken? */
	LA	t0, _ftext
	LA	t1, _etext
#else
	LA	t0, start		# start of the prom
	daddu	t1,t0,0x80000		# 512K prom (max)
#endif
	li	t2, 0x1fffffff		# Only 29 bits of phys addr on teton
	and	t0,t0,t2		# KDM_TO_PHYS
	and	t1,t1,t2		# KDM_TO_PHYS
	and	ra,ra,t2		# KDM_TO_PHYS
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
	LI	sp,PROM_STACK-(4*SZREG)	# reset the stack
	sd	sp,_fault_sp
	dsubu	sp,EXSTKSZ		# top 1/4 of stack for fault handling
	jal	main
	# if main fails fall through and re-init
1:
	# go whole hog
	li	s0,ARCS_IMODE
	j	arcs_common
	END(EnterInteractiveMode)

LEAF(arcs_common)
	.set	noreorder
	LA	t0,pon_handler		# load power-on BEV hander
	dmtc0	t0,TETON_BEV
	ssnop
	LA	t0,_trap_table		# init TrapBase register
	dmtc0	t0,C0_TRAPBASE

	LI	v0, SR_PROMBASE		# no interrupts enabled
	dmtc0   v0, C0_SR
	DMTC0	(s0,C0_PBASE)		# save s0 for sure (well pretty sure)
	.set	reorder

	jal	ip26_enable_ucmem	# enable uncached memory

	LA	a0,1f			# come back to native mode
	jr	a0
	ssnop
 	.align	3			# make sure we are on a new quad

1:	jal	kill_hpc3_dma		# does what it says
        jal     StartAesTx              # Start up the AES TX early so reciver
                                        # can sync its PLL
#ifdef ENETBOOT
	jal	clear_prom_ram		# clear_prom_ram initializes sp
#else
	li	v0,0x2000000		# hardcode a 2MB cache for flush
	sw	v0,_sidcache_size
	jal	__dcache_inval_all	# invalidate the cache
	jal	szmem			# set-up which memory to clear
	jal	clear_all_ram		# clear all ram to scrub ECC
	jal	init_prom_ram		# set-up stack + cookies
	jal	wait_clear_seg1		# wait for seg1 memory to clear
#endif
	jal	gfxreset		# reset graphics
	move	a0,zero
	jal	init_prom_soft		# initialize saio and everything else

	.set	noreorder
	DMFC0	(s0,C0_PBASE)		# make sure S0 is ok
	.set	reorder

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
	dmtc0	zero,TETON_BEV		# clear BEV handler
	ssnop
	LA	t0,_trap_table		# init TrapBase register
	dmtc0	t0,C0_TRAPBASE
	ssnop
	LI	t0, SR_PROMBASE		# no interrupts enabled
	dmtc0	t0, C0_SR
	ssnop
	dmtc0	zero,C0_CAUSE		# clear software interrupts
	ssnop
	.set	reorder

	lw	v0,bssmagic		# Check to see if bss valid
	bne	v0,BSSMAGIC,_init	# If not, do a hard init

	LI	sp,PROM_STACK-(4*SZREG)	# reset the stack
	sd	sp,_fault_sp
	dsubu	sp,EXSTKSZ		# top 1/4 of stack for fault handling
	jal	config_cache		# determine cache sizes
	jal	flush_cache		# flush cache
	jal	enter_imode
1:	b	1b			# never gets here
	END(_exit)

/*
 * _init -- reinitialize prom and reenter command parser
 */
LEAF(_init)
	.set	noreorder
	dmtc0	zero,TETON_BEV		# clear bev handler
	ssnop
	LA	t0,_trap_table		# init TrapBase register
	dmtc0	t0,C0_TRAPBASE
	ssnop
	LI	t0, SR_PROMBASE		# no interrupts enabled
	dmtc0	t0, C0_SR
	ssnop
	ssnop
	.set	reorder

	jal	clear_prom_ram		# clear_prom_ram initializes sp
	move 	a0,zero
	jal	init_prom_soft
	jal	main
	jal	EnterInteractiveMode	# shouldn't get here
	END(_init)

/*
 * clear_prom_ram -- clear prom bss, clear memory between _fbss and
 * PROM_STACK-4 inclusive (for dprom, clear between _fbss and end),
 * clears and initializes stack.
 *
 * We assume good alignment for prom case, while dprom does not.
 */
LEAF(clear_prom_ram)
#if ENETBOOT
#ifdef GOOD_COMPILER
	LA	a0,_fbss
	LA	a1,end
#else
	LI	a0,DBSSADDR		# dprom BSS link address
	daddu	a1,a0,0x100000		# assume 1MB BSS
#endif
1:	PTR_ADD	a0,8
	sdl	zero,-8(a0)		# stupid compiler...
	sdr	zero,-8(a0)
	bltu	a0,a1,1b
#else
#ifdef BSSADDR
 	LI	a0,BSSADDR		# work around cmplr bug
#else
	LA	a0,_fbss		# clear bss and stack
#endif
 	LI	a1,PROM_STACK		# end of prom stack

	/* make sure start/end are cached.
	 */
	LI	v0,TO_PHYS_MASK		# KDM_TO_PHYS
	and	a0,a0,v0
	and	a1,a1,v0
	LI	v0,K0BASE		# PHYS_TO_K0
	or	a0,a0,v0
	or	a1,a1,v0

	LI	v0,K1BASE|TCC_CACHE_OP|TCC_DIRTY_WB|TCC_INVALIDATE
	li	v1,TCC_PHYSADDR

 	.set	noreorder
1:	and	a2,a0,v1		# get phys addr to hit on cache op
	PTR_ADD	a0,TCC_LINESIZE		# clear 128 bytes (cache line)
	sd	zero,-8(a0)
	sd	zero,-16(a0)
	sd	zero,-24(a0)
	sd	zero,-32(a0)
	sd	zero,-40(a0)
	sd	zero,-48(a0)
	sd	zero,-56(a0)
	sd	zero,-64(a0)
	sd	zero,-72(a0)
	sd	zero,-80(a0)
	sd	zero,-88(a0)
	sd	zero,-96(a0)
	sd	zero,-104(a0)
	sd	zero,-112(a0)
	or	a2,v0			# calculate cache op addr
	sd	zero,-120(a0)
	sd	zero,-128(a0)
 	bltu	a0,a1,1b
	lw	zero,0(a2)		# BDSLOT: wb_inval this line
 	.set	reorder
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
	LI	sp,PROM_STACK-(4*SZREG)
	sd	sp,_fault_sp		# base of fault stack
	dsubu	sp,EXSTKSZ		# top 1/4 of stack for fault handling
	j	ra
	END(clear_prom_ram)

/* 
 * initialize HPC configuration
 */
#ifdef IP22_WD95
#define PIO_CFG_33MHZ		WD95_PIO_CFG_33MHZ
#define PIO_CFG_25MHZ		WD95_PIO_CFG_25MHZ	
#else
#define PIO_CFG_33MHZ		SC_PIO_CFG_33MHZ
#define PIO_CFG_25MHZ		SC_PIO_CFG_25MHZ	
#endif
LEAF(init_hpc3)
	/* Set endianness
	 */
	LI	t0,PHYS_TO_K1(HPC3_MISC_ADDR)
	lw	t1,0(t0)

	and	t1,~HPC3_DES_ENDIAN	# DMA descriptor fetch is big endian
	or	t1,HPC3_EN_REAL_TIME
	sw	t1,0(t0)

	/* Configure extended register 
	 */
	LI	a7,PHYS_TO_K1(HPC3_PBUS_CFGPIO_BASE)	# base address
        li      t1,0x44e27
        sw      t1,5*HPC3_PBUS_CFGPIO_OFFSET(a7)
	lw	zero,5*HPC3_PBUS_CFGPIO_OFFSET(a7)	# flush bus

	/* Initialize write1 -- release reset and turn on green LED.
	 */
	LI	t0,PHYS_TO_K1(HPC3_WRITE1)
	li	t1,(LED_GREEN|PAR_RESET|KBD_MS_RESET|EISA_RESET)
	sw	t1,0(t0)

	/* Configure INT2
	 */
        li      t1,0x4e27
        sw      t1,4*HPC3_PBUS_CFGPIO_OFFSET(a7)

	/* Configure PIO channels for audio devices
	 */
	/* Channel 0 = HAL2 chip */
	li	t1,0x48a45
	sw	t1,0*HPC3_PBUS_CFGPIO_OFFSET(a7)

	/* Channel 1 = AES chips */
	li	t1,0x88e47
	sw	t1,1*HPC3_PBUS_CFGPIO_OFFSET(a7)

	/* Channel 2 = MDAC volume control */
	li	t1,0x11289
	sw	t1,2*HPC3_PBUS_CFGPIO_OFFSET(a7)

	/* Channel 3 = synthesizer chip */
	li	t1,0xc4a25
	sw	t1,3*HPC3_PBUS_CFGPIO_OFFSET(a7)

	/*  Configure PBUS DMA channels to the best of our ability.
	 * Audio uses PBUS DMA channels 1-4, parallel port using
	 * channel 0.
	 */
	LI	t0,PHYS_TO_K1(HPC3_PBUS_CFGDAM_BASE)
	li	t1,0x8248844

	sw	t1,1*HPC3_PBUS_CFGDMA_OFFSET(t0)
	sw	t1,2*HPC3_PBUS_CFGDMA_OFFSET(t0)
	sw	t1,3*HPC3_PBUS_CFGDMA_OFFSET(t0)
	sw	t1,4*HPC3_PBUS_CFGDMA_OFFSET(t0)

XLEAF(init_hpc3_scsi)					# ext io no ok early
	/* Configure SCSI 
	 */
	LI	t0,PHYS_TO_K1(HPC3_EXT_IO_ADDR)
	lw	t1,0(t0)
	andi	t2,t1,EXTIO_GIO_33MHZ			# mask 33MHZ bit
	LI	t0,PHYS_TO_K1(HPC3_SCSI_PIOCFG0)	# PIO 0 address
	li	t1,WD93_PIO_CFG_25MHZ			# assume 25Mhz
	beq	t2,zero,1f
	li	t1,WD93_PIO_CFG_33MHZ        		# nope, 33Mhz
1:	sw	t1,HPC3_SCSI_PIOCFG1-HPC3_SCSI_PIOCFG0(t0)	# PIO 1
	sw	t1,0(t0)					# PIO 0

	/* Configure IO channel
	 */
	LI	a7,PHYS_TO_K1(HPC3_PBUS_CFGPIO_BASE)	# base address
	li	t1,0x946d			# setup 25mhz gio value for c6
	beq	t2,zero,1f			# t2 still has gio bit.
        li      t1,0x988d			# setup 33mhz gio value for c6
1:	sw	t1,6*HPC3_PBUS_CFGPIO_OFFSET(a7)

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

	LI	t0,PHYS_TO_K1(HPC3_PBUS_CONTROL_BASE)
	li	t1,0x20
	sw	t1,0*HPC3_PBUS_CONTROL_OFFSET(t0)	# channel 0
	sw	t1,1*HPC3_PBUS_CONTROL_OFFSET(t0)	# channel 1
	sw	t1,2*HPC3_PBUS_CONTROL_OFFSET(t0)	# channel 2
	daddiu	t0,3*HPC3_PBUS_CONTROL_OFFSET		# next chunk
	sw	t1,0*HPC3_PBUS_CONTROL_OFFSET(t0)	# channel 3
	sw	t1,1*HPC3_PBUS_CONTROL_OFFSET(t0)	# channel 4
	sw	t1,2*HPC3_PBUS_CONTROL_OFFSET(t0)	# channel 5
	daddiu	t0,3*HPC3_PBUS_CONTROL_OFFSET		# next chunk
	sw	t1,0*HPC3_PBUS_CONTROL_OFFSET(t0)	# channel 6
	sw	t1,1*HPC3_PBUS_CONTROL_OFFSET(t0)	# channel 7

	j	ra
	END(kill_hpc3_dma)

/*
 * Start the AES Transmitter going very early on so that equipment
 * downstream can lock to it before the boot tune plays.
 */
#define HAL2_ISR_OFFSET		(HAL2_ISR-HAL2_ADDR)
#define HAL2_REV_OFFSET		(HAL2_REV-HAL2_ADDR)
#define HAL2_IAR_OFFSET		(HAL2_IAR-HAL2_ADDR)
#define HAL2_IDR0_OFFSET	(HAL2_IDR0-HAL2_ADDR)
#define HAL2_IDR1_OFFSET	(HAL2_IDR1-HAL2_ADDR)
#define AESTX_CR1_OFFSET	(AESTX_CR1-HAL2_ADDR)
#define AESTX_CR2_OFFSET	(AESTX_CR2-HAL2_ADDR)
#define AESTX_CR3_OFFSET	(AESTX_CR3-HAL2_ADDR)
#define AESTX_CS0_OFFSET	(AESTX_CS0-HAL2_ADDR)
#define AESTX_CS1_OFFSET	(AESTX_CS1-HAL2_ADDR)
#define AESTX_DMA_CHAN		2
LEAF(StartAesTx)
	LI	t1,PHYS_TO_K1(HAL2_ADDR)		# hal2 base address

	lw	t0,HAL2_REV_OFFSET(t1)
	and	t0,t0,0x8000
	bnez	t0,no_audio

	/* Take the HAL2 out of reset
	 */
	li	t0, HAL2_ISR_CODEC_RESET_N|HAL2_ISR_GLOBAL_RESET_N;
	sw	t0, HAL2_ISR_OFFSET(t1)

	/* Set up AES TX
	 */
	li	t0, AESTX_DMA_CHAN+(1 /*bres clock 1*/ << 3)+(1/*mono*/ << 8);
	sw	t0, HAL2_IDR0_OFFSET(t1)
	li	t0, HAL2_AESTX_CTRL_W;
	sw	t0, HAL2_IAR_OFFSET(t1)

spinloop1:
	lw	t0, HAL2_ISR_OFFSET(t1)
	and	t0, 1
	bnez	t0, spinloop1

	/* Set up BRES CLOCK 1 source
	 */
	li	t0, 1   /* 1 = 44.1kHz master */
	sw	t0, HAL2_IDR0_OFFSET(t1)
	li	t0, HAL2_BRES1_CTRL1_W
	sw	t0, HAL2_IAR_OFFSET(t1)

spinloop2:
	lw	t0, HAL2_ISR_OFFSET(t1)
	and	t0, 1
	bnez	t0, spinloop2

	/* Set up BRES CLOCK 1 inc and modctrl
	 */
	li	t0, 1				/* idr0=inc=1 */
	sw	t0, HAL2_IDR0_OFFSET(t1)
	li	t0, 0xffff			/* idr1=modctrl=0xffff */
	sw	t0, HAL2_IDR1_OFFSET(t1)
	li	t0, HAL2_BRES1_CTRL2_W		/* iar=bres1 ctrl reg 2 */
	sw	t0, HAL2_IAR_OFFSET(t1)

spinloop3:
	lw	t0, HAL2_ISR_OFFSET(t1)
	and	t0, 1
	bnez	t0, spinloop3

        /* Configure the channel status bits on the AES Tx Chip
	 */
	li	t0,0x4;
	sw	t0,AESTX_CS0_OFFSET(t1)

	daddiu	t2,t1,AESTX_CS1_OFFSET		# start at CS1
	li	t0,1				# loop counter
	li	t3,24				# end of loop
1:	sw	zero,0(t2)			# store CSx (1-23)
	daddiu	t2,4				# incr CSx pointer
	addi	t0,1				# incr loop counter
	blt	t0,t3,1b			# loop till done

        /* Activate the TX Chip
	 */
	li	t0,0x9;
	sw	t0,AESTX_CR3_OFFSET(t1)
	sw	zero,AESTX_CR1_OFFSET(t1)
	li	t0,0x83;
	sw	t0,AESTX_CR2_OFFSET(t1)

no_audio:
	j	ra	
	END(StartAesTx)

/* 
 * initialize INT2 configuration
 *
 * NBI, maintain HPC_INT_ADDR in (t2)
 */
LEAF(init_int2)
	LI	t2,PHYS_TO_K1(HPC3_INT2_ADDR)	# use INT2
	.set 	noreorder
	# deassert reset/init retrace/select S0-DMA-SYNC
	li	t1,(PCON_CLR_SG_RETRACE_N|PCON_CLR_S0_RETRACE_N|PCON_SG_RESET_N|PCON_S0_RESET_N)
	sb	t1,PORT_CONFIG_OFFSET(t2)

	sb	zero,LIO_0_MASK_OFFSET(t2)	# mask all external ints
	sb	zero,LIO_1_MASK_OFFSET(t2)
	sb	zero,LIO_2_MASK_OFFSET(t2)
	sb	zero,LIO_3_MASK_OFFSET(t2)

	li	t1,PTCW_SC(0)|PTCW_MODE(1)	# disable 8254 timer
	sb	t1,PT_CLOCK_OFFSET+PT_CONTROL(t2)
	li	t1,PTCW_SC(1)|PTCW_MODE(1)
	sb	t1,PT_CLOCK_OFFSET+PT_CONTROL(t2)
	li	t1,PTCW_SC(2)|PTCW_MODE(1)
	sb	t1,PT_CLOCK_OFFSET+PT_CONTROL(t2)
	li	t1,3				# clear timer ints
	sb	t1,TIMER_ACK_OFFSET(t2)
1:
XLEAF(init_duart)
	LI	t0,PHYS_TO_K1(HPC3_WRITE2)	# configure duart clocks
	li	t1,UART1_ARC_MODE|UART0_ARC_MODE# low=RS422 MAC mode,hi=RS232
	sw	t1,0(t0)

	li	t1,((POWER_SUP_INHIBIT|POWER_INT)&0xff)
	LI	t0,PHYS_TO_K1(HPC3_PANEL)	# power on and clear intr
	sw	t1,0(t0)

	j	ra
	nop
	.set 	reorder
	END(init_int2)

/*
 * Do NOT reset the bus.  A reset is done by the act of powering up
 * or resetting.  It shouldn't be needed from software.  If there is an
 * interrupt, clear it, but don't wait for one.
 */

#define SCSI_WAIT(reg)		 \
	lw	zero,0(reg)	;\
	lw	zero,0(reg)	;\
	lw	zero,0(reg)	;\
	lw	zero,0(reg)	;\
	lw	zero,0(reg)	;\
	lw	zero,0(reg)	;\
	lw	zero,0(reg)	;\
	lw	zero,0(reg)

LEAF(init_scsi)
	.set 	noreorder
	/* clear the scsi reset line; HPC latches it on reset/powerup */
	LI	t0,PHYS_TO_K1(SCSI0_CTRL_ADDR)
	LI	t2,PHYS_TO_K1(0x1fc00000)
	sw	zero,SCSI1_CTRL_ADDR-SCSI0_CTRL_ADDR(t0)# SCSI1_CTRL_ADDR
	sw	zero,0(t0)				# SCSI0_CTRL_ADDR
	lw	zero,0(t2)				# flushbus

	/* clear any SCSI interrupts by reading the status register on the
	   Western Digital SCSI controller chip
	   Then issue a reset command.  This is needed because a chip reset
	   doesn't clear the registers, and we now look at some of them
	   on startup, to handle kernel to PROM transitions.  CMD register
	   follows status, and register # is auto-incremented, so don't
	   need to write the register # (RESET is cmd 0). Need 7 usec between
	   status register read and cmd; Each lw from the PROM takes about
	   ~1 usec, so execute 8 of them.
	*/

	/* RESET SCSI0 */
	li	t1,0x17				# SCSI reg 0x17 - status
	LI	t0,PHYS_TO_K1(SCSI0A_ADDR)
	sb	t1,0(t0)
	lw	zero,0(t2)				# flushbus
	lb	zero,SCSI0D_ADDR-SCSI0A_ADDR(t0)	# SCSI0D_ADDR
	SCSI_WAIT(t2)

	/* reset the chip; clears sync register, etc. */
	sb	zero,SCSI0D_ADDR-SCSI0A_ADDR(t0) 

	SCSI_WAIT(t2)

	/* RESET SCSI1 */
	li	t1,0x17				# SCSI reg 0x17 - status
	LI	t0,PHYS_TO_K1(SCSI1A_ADDR)
	sb	t1,0(t0)
	lw	zero,0(t2)				# flushbus
	lw	zero,0(t2)				# flushbus
	lb	zero,SCSI1D_ADDR-SCSI1A_ADDR(t0)	# SCSI1D_ADDR
	SCSI_WAIT(t2)

	/* reset the chip; clears sync register, etc. */
	sb	zero,SCSI1D_ADDR-SCSI1A_ADDR(t0) 
	lw	zero,0(t2)				# flushbus

	j	ra
	nada
	.set 	reorder
	END(init_scsi)


/* init_C0: Initialize floating COP0 registers.
 */
LEAF(init_C0)
	.set	noreorder
	dmtc0	zero, C0_TLBSET
	ssnop
	dmtc0	zero, C0_TLBLO
	ssnop
	dmtc0	zero, C0_BADVADDR
	ssnop
	dmtc0	zero, C0_COUNT
	ssnop
	dmtc0	zero, C0_TLBHI
	ssnop
	dmtc0	zero, C0_CAUSE
	ssnop
	dmtc0	zero, C0_EPC
	ssnop
	dmtc0	zero, C0_UBASE
	ssnop
	dmtc0	zero, C0_PBASE
	ssnop
	dmtc0	zero, C0_GBASE
	ssnop
	dmtc0	zero, C0_WIRED
	ssnop
	dmtc0	zero, C0_DCACHE
	ssnop
	dmtc0	zero, C0_ICACHE
	ssnop
	.set	reorder

	j	ra			# Return to caller
	END(init_C0)


/* All entries are marked invalid.  The VPN written to each of
 * the 3 sets is unique to ensure no duplicate hits.
 * v0: set counter
 * v1: index counter
 * a0: VAddr (and EntryHI if doing all indeces)
 * If doing only 8 indeces:
 *      a1: EntryHi
 *      a2: value to increment EntryHi to change it from one set to another
 */
LEAF(_flush_tlb)
	.set    noreorder
        dmtc0	zero, C0_TLBLO		# clear all bits in EntryLo
	ssnop
        li      v0, NTLBSETS - 1        # v0 is set counter
        dli     a0, 0xC000000000000000  # Kernel Global region
1:
	dmtc0	a0, C0_TLBHI
	ssnop
	li      v1, NTLBENTRIES         # do all indeces

	dmtc0	v0, C0_TLBSET
	ssnop
        dmtc0	a0, C0_BADVADDR		# specify which TLB index
	ssnop
2:
        addi    v1, -1                  # decrement index counter
        tlbw
	NOP_COM_HAZ
        daddiu  a0, NBPP		# increment address to next TLB index
        dmtc0	a0, C0_BADVADDR		# specify which TLB index
	ssnop
	ssnop
        bgt	v1, zero, 2b		# are we done with all indeces?
        nop

        addi    v0, -1
        bgez    v0, 1b                  # are we done with all sets?
        nop
        j       ra
        nop
        .set    reorder
END(_flush_tlb)

LEAF(get_gerror)
	.set	noreorder
	dmfc0	v0,TETON_GERROR
	ssnop
	ssnop
	.set	reorder
	j	ra
	END(get_gerror)

/* 
 * Data area
 *
 */
	BSS(bssmagic,4)
	BSS(simmerrs,4)
