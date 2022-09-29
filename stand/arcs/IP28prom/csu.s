#ident	"IP28prom/csu.s:  $Revision: 1.8 $"

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
#include <sys/RACER/gda.h>	/* share with racer for good measure */

#if ENETBOOT
#define ENET_SR_BEV		0
#else
#define ENET_SR_BEV		SR_BEV
#endif

#define BSSMAGIC		0x0deadbed

#define WBFLUSHM					\
	.set	noreorder; 				\
	lw	zero,PHYS_TO_COMPATK1(CPUCTRL0);	\
	sync;						\
	lw	zero,PHYS_TO_COMPATK1(CPUCTRL0);	\
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
	/*  The IP22 PAL doesn't work in power-on mode with the teton module
	 * and the MC thinks the HPC3 is a 32-bit device.  This causes us
	 * to see even instruction twice.  To get around that we reset the
	 * machine by writing to CPUCTRL0 we skip the jump to skip the
	 * hack.  The machine seems to work correctly after reset.
	 */
	AUTO_CACHE_BARRIERS_DISABLE
	.set	noreorder
	.set	noat
	nop				# see 2x on bad system		0x00
	j	realstart		# real prom (skip on bad sys)	0x04
	lui	k0,0xbfa0		# PHYS_TO_COMPATK1(CPUCTRL0)	0x08
	nop				# odd				0x0c
	lui	k1,0x0001		# MC default + SIN bit		0x10
	nop				# odd				0x14
	ori	k1,0x020a		# rest of MC default		0x18
	nop				# odd				0x1c
	sw	k1,4(k0)		# write to BE CPUCTRL0		0x20
	nop				# odd				0x24
	sw	k1,0(k0)		# write to LE CPUCTRL0		0x28
	nop				# odd				0x2c
	lw	zero,0(k0)		# flush				0x30
	nop				# odd				0x34
	lw	zero,0(k0)		# flush				0x38
	nop				# odd				0x3c
	.set	at
	.set	reorder
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
	j	bev_ip26_cacherror
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
	j	notimplemented

/*
 * END: prom entry point table
 */

realstart:
	.set	noreorder
	.set	noat
	li	k0,CONFIG_UNCACHED	# turn off the cache via compat K0
	mtc0	k0,C0_CONFIG

	LA	k0,1f			# move to propper address space
	j	k0
1:

	mtc0	zero,C0_WATCHLO         # clear/disable watchpoint interrupt
	mtc0	zero,C0_WATCHHI

	/* check for NMI */
	mfc0	k0,C0_SR		# get status register
	li	k1,SR_SR		# check soft reset
	and	k1,k0,k1
	beqz	k1,1f
	li	k1,SR_NMI		# BDSLOT: check for NMI
	and	k0,k1
	bnez	k0,bev_nmi
	nop				# BDSLOT
	.set	at
1:

#define PROM_SR SR_CU0|SR_CU1|SR_FR|SR_KX|ENET_SR_BEV|SR_DE
	li	v0,PROM_SR
	mtc0	v0,C0_SR		# state unknown on reset
	mtc0	zero,C0_CAUSE		# clear software interrupts

	.set	reorder

	/* Clear CPU and GIO errors on MC and HPC3 errors */
	sw	zero,PHYS_TO_COMPATK1(CPU_ERR_STAT)
	sw	zero,PHYS_TO_COMPATK1(GIO_ERR_STAT)
	lw	zero,PHYS_TO_COMPATK1(HPC3_INTSTAT_ADDR)
	WBFLUSHM

	jal	init_registers		# initialize T5 integer regsiters

	LA	fp,pon_handler		# set BEV handler early

#define	CPUCTRL0_DEFAULT	0x3c002010

#define	CPUCTRL0_REFS_4_LINES	0x00000002
#define	CPUCTRL0_REFS_8_LINES	0x00000004

	/* set up CPU control register 0, CPUCTRL0 */
	CLI	t0,PHYS_TO_COMPATK1(CPUCTRL0)

	/* set up the MUX_HWM according to the cache line size */
	.set	noreorder
	mfc0	a0,C0_CONFIG
	.set	reorder
	and	a1,a0,CONFIG_SB
	srl	a1,CONFIG_SB_SHFT	# 1 == 32W, 0 == 16W
	add	a1,6
	li	a2,1
	sll	a1,a2,a1		# bytes in cache line

	srl	a1,3			# divided by 8
	add	a1,8			# add 8 (different from r4000!)
	sll	a1,20			# shift into correct position
	or	t1,a1,CPUCTRL0_DEFAULT|CPUCTRL0_REFS_4_LINES
	sw	t1,0(t0)
	lw	zero,0(t0)		# WBFLUSHM
	sync
	lw	zero,0(t0)		# WBFLUSHM continued

	/* set MC_HWM to 0x6, enable GIO bus time out */
	CLI	t0,PHYS_TO_COMPATK1(CPUCTRL1)
	li	t1,CPUCTRL1_ABORT_EN|0x6
	sw	t1,0(t0)
	lw	zero,0(t0)		# WBFLUSHM
	sync
	lw	zero,0(t0)		# WBFLUSHM continued

	/* ensure we are re-set from the EISA hang */
	li	a1,0x0c30
	sw	a1,PHYS_TO_COMPATK1(CTRLD)

	/* Assume recent baseboard */
	CLI	t0,PHYS_TO_COMPATK1(GIO64_ARB)
	lw	t1,_gio64_arb_004
	sw	t1,0(t0)
	lw	zero,0(t0)		# WBFLUSHM
	sync
	lw	zero,0(t0)		# WBFLUSHM continued

/* ECC baseboard in 'slow' mode, which allows normal uncached access to
 * memory.  The kernel later turns on fast mode when it is ready not
 * access memory uncached.
 */
#define	GIO_MEMACC_DEFAULT		0x00034336

	/* set up CPU_MEMACC and GIO_MEMACC */
	CLI	t0,PHYS_TO_COMPATK1(CPU_MEMACC)
	li	t1,CPU_MEMACC_SLOW
	CLI	t2,PHYS_TO_COMPATK1(HPC3_SYS_ID)# board revision info
	lw	t2,0(t2)
	andi	t2,BOARD_REV_MASK		# isolate board rev
	sub	t2,IP26_ECCSYSID		# IP26 ecc bd = 0x18
	bnez	t2,1f				# skip if on IP26+/IP28 bd
	li	t1,CPU_MEMACC_SLOW_IP26		# slow IP26 memory setting
1:	sw	t1,0(t0)

	CLI	t0,PHYS_TO_COMPATK1(GIO_MEMACC)
	li	t1,GIO_MEMACC_DEFAULT
	sw	t1,0(t0)

	# Set up as 100ns counter for 50Mhz MC (not true on IP28 but changed
	# kernel changes this anyway).
	CLI	t0,PHYS_TO_COMPATK1(RPSS_DIVIDER)
	li	t1,0x0104
	sw	t1,0(t0)

	CLI	t0,PHYS_TO_COMPATK1(RPSS_CTR)	# RPSS counter
	lw	t1,0(t0)		# current RPSS counter
	li	t2,625			# must refresh SIMMs for at least 8
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

	LI	sp,K0BASE		# so auto cache barriers are ok

	/* initialize cpu board hardware
	 */
	jal	init_hpc3		# initialize HPC3 chip
        jal     StartAesTx		# Get the AES tx going early 
					# so the reciever pll can sync.
	jal	init_scsi		# init onboard SCSI interface
	jal	init_int2		# initialize INT2 chip

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

	/* Must initialize cache before writing to ECC parts */
	VPRINTF("Invalidating caches.\r\n")
	jal	iCacheSize			# get instruction cache size
	move	a0,v0
	jal	invalidate_icache
	jal	dCacheSize			# get data cache size
	move	a0,v0
	jal	invalidate_dcache
	jal	sCacheSize			# get secondary cache size
	move	a0,v0
	jal	invalidate_scache

	VPRINTF("Initializing caches.\r\n")
	jal	sCacheSize			# get secondary cache size
	move	a0,v0
	jal	init_scache
	jal	iCacheSize			# get instruction cache size
	move	a0,v0
	jal	init_icache
	jal	dCacheSize			# get data cache size
	move	a0,v0
	jal	init_dcache

	/* Set up the ECC hardware
	 * The tricky part is that there are 2 IDT49C466 chips, sitting
	 * on different DMUX's, but they have to be written on the same
	 * cycle.  The only way to guarantee this is by doing the writes to
	 * cache, then flushing.
	 */
	VPRINTF("Initializing ECC hardware.\r\n")
	li	a0,ECC_CTRL_DISABLE		# Turn on slow mode
	jal	ip28_write_pal
	li	a0,ECC_CTRL_CLR_INT		# Clear any errors
	jal	ip28_write_pal

	/* On IP28 also turn off ecc error reporting */
	CLI	t3,PHYS_TO_COMPATK1(HPC3_SYS_ID)# board revision info
	lw	t3,0(t3)
	andi	t3,BOARD_REV_MASK		# isolate board rev
	sub	t3,IP26_ECCSYSID		# IP26 ecc bd = 0x18
	blez	t3,1f				# skip if not on IP28 board
	li	a0,ECC_CTRL_CHK_OFF
	jal	ip28_write_pal
1:

	dli	a0,ECC_FLOWTHRU			# come up flowthru for diag
	jal	ip28_write_ecc
	
#ifndef ENETBOOT
	VPRINTF("Running MC/HPC data path diagnostic.\r\n")
	jal	pon_mc			# test path to MC
#endif
	jal	pon_regs		# test HPC data paths

	VPRINTF("Sizing memory and running memory diagnostic.\r\n")
	jal	init_memconfig		# initialize memory configuration

	/* sp trashed in init_mem_config */
	LI	sp,K0BASE		# so auto cache barriers are ok

	/* Turn ECC correction back on.  Error reporting turned on later */
	VPRINTF("Enable ECC correction.\r\n")
	dli	a0,ECC_DEFAULT		# turn correction on
	jal	ip28_write_ecc

	VPRINTF("Initializing PROM stack.\r\n")
	jal	init_prom_ram		# init stack and bss
					# ah... finally we have data area
	VPRINTF("Start executing C code.\r\n")
	j	sysinit			# goodbye
	AUTO_CACHE_BARRIERS_ENABLE
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
#ifdef GOOD_COMPILER
	LA	t0,_ftext
	LA	t1,_etext
#else
	LA	t0,start		# start of the prom
	daddu	t1,t0,0x80000		# 512K prom (max)
#endif
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
	LI	sp,PROM_STACK-(4*SZREG)	# reset the stack
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
	mtc0	zero,C0_WATCHLO		# make sure watchpoints are clear
	mtc0	zero,C0_WATCHHI
	li	a0,SR_PROMBASE|ENET_SR_BEV
	LA	fp,pon_handler		# set BEV handler (for hard prom)
	mtc0	a0,C0_SR		# no interrupts
	.set	reorder

	jal	ip26_enable_ucmem	# enable uncached memory
	jal	cache_K0		# make sure cached prom is on

	LI	a0,GDA_ADDR		# clear GDA vec 4 now (re-hooked later)
	sd	zero,G_NMIVECOFF(a0)

	jal	kill_hpc3_dma		# does what it says
        jal     StartAesTx              # Start up the AES TX early so reciver
                                        # can sync its PLL
	jal	_gfxreset		# reset graphics
	jal	clear_prom_ram		# clear_prom_ram initializes sp
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

	LI	sp,PROM_STACK-(4*SZREG)	# reset the stack
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
	li	t0,CONFIG_NONCOHRNT
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
	/* If GOOD_COMPILER ever becomes true with mongoose fix this */
#ifdef ENETBOOT
	LI	a0,DBSSADDR		# just clear BSS
	daddu	a1,a0,0x100000		# assume 1MB BSS -- larger than real
#else
	LI	a0,BSSADDR		# clear BSS and stack
	LI	a1,PROM_STACK
#endif
	LI	v0,TO_PHYS_MASK		# KDM_TO_PHYS
	and	a0,v0
	and	a1,v0

	/* Last and really isolated phys addr, now do PHYS_TO_K0
	 */
	LI	v0,K0BASE
	or	a0,v0
	or	a1,v0

	move	v0,a0
	move	v1,a1

	.set	noreorder
1:	PTR_ADD	v0,128			# clear one cacheline at a time
	CACHE_BARRIER			# ensure loop stops
	sd	zero,-8(v0)
	sd	zero,-16(v0)
	sd	zero,-24(v0)
	sd	zero,-32(v0)
	sd	zero,-40(v0)
	sd	zero,-48(v0)
	sd	zero,-56(v0)
	sd	zero,-64(v0)
	sd	zero,-72(v0)
	sd	zero,-80(v0)
	sd	zero,-88(v0)
	sd	zero,-96(v0)
	sd	zero,-104(v0)
	sd	zero,-112(v0)
	sd	zero,-120(v0)
	bltu	v0,v1,1b
        sd	zero,-128(v0)			# BDSLOT
	.set	reorder

	/* index writeback invalidate */
 	.set	noreorder
	LI	v0,K0_RAMBASE
	mfc0	t0,C0_CONFIG			# config register
	and	t0,CONFIG_SS			# mask cache size bits
	dsrl	t0,CONFIG_SS_SHFT		# right justify
	dadd	t0,CONFIG_SCACHE_POW2_BASE	# unit justify
	li	v1,1				# size is 1 << v0
	dsllv	v1,t0				# cache size in bytes
	dsrl	v1,1				# cache is 2-way associative
	daddu	v1,v0,v1			# base + size
	/* NOTE: depends on 128 byte line size */
1:	daddiu	v0,256
	cache	CACH_SD|C_IWBINV,-256(v0)
	cache	CACH_SD|C_IWBINV,-256+1(v0)
	cache	CACH_SD|C_IWBINV,-128(v0)
	bltu	v0,v1,1b
	cache	CACH_SD|C_IWBINV,-128+1(v0)	# BDSLOT
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
	END(clear_prom_ram)

/* 
 * initialize HPC configuration
 */
LEAF(init_hpc3)
	AUTO_CACHE_BARRIERS_DISABLE	# run before memory set-up
	/* Set endianness
	 */
	CLI	t0,PHYS_TO_COMPATK1(HPC3_MISC_ADDR)
	lw	t1,0(t0)

	and	t1,~HPC3_DES_ENDIAN	# DMA descriptor fetch is big endian
	or	t1,HPC3_EN_REAL_TIME
	sw	t1,0(t0)

	/* Configure extended register 
	 */
	CLI	a7,PHYS_TO_COMPATK1(HPC3_PBUS_CFGPIO_BASE)
        li      t1,0x44e27
        sw      t1,5*HPC3_PBUS_CFGPIO_OFFSET(a7)
	lw	zero,5*HPC3_PBUS_CFGPIO_OFFSET(a7)	# flushbus
	sync
	lw	zero,5*HPC3_PBUS_CFGPIO_OFFSET(a7)	# flushbus continued

	/* Initialize write1 -- release reset and turn on green LED.
	 */
	CLI	t0,PHYS_TO_COMPATK1(HPC3_WRITE1)
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
	CLI	t0,PHYS_TO_COMPATK1(HPC3_PBUS_CFGDAM_BASE)
	li	t1,0x8248844

	sw	t1,1*HPC3_PBUS_CFGDMA_OFFSET(t0)
	sw	t1,2*HPC3_PBUS_CFGDMA_OFFSET(t0)
	sw	t1,3*HPC3_PBUS_CFGDMA_OFFSET(t0)
	sw	t1,4*HPC3_PBUS_CFGDMA_OFFSET(t0)

XLEAF(init_hpc3_scsi)					# ext io no ok early
	/* Configure SCSI 
	 */
	CLI	t0,PHYS_TO_COMPATK1(HPC3_EXT_IO_ADDR)
	lw	t1,0(t0)
	andi	t2,t1,EXTIO_GIO_33MHZ			# mask 33MHZ bit
	CLI	t0,PHYS_TO_COMPATK1(HPC3_SCSI_PIOCFG0)	# PIO 0 address
	li	t1,WD93_PIO_CFG_25MHZ			# assume 25Mhz
	beq	t2,zero,1f
	li	t1,WD93_PIO_CFG_33MHZ        		# nope, 33Mhz
1:	sw	t1,HPC3_SCSI_PIOCFG1-HPC3_SCSI_PIOCFG0(t0)	# PIO 1
	sw	t1,0(t0)					# PIO 0

	/* Configure IO channel
	 */
	CLI	a7,PHYS_TO_COMPATK1(HPC3_PBUS_CFGPIO_BASE)# base address
	li	t1,0x946d			# setup 25mhz gio value for c6
	beq	t2,zero,1f			# t2 still has gio bit.
        li      t1,0x988d			# setup 33mhz gio value for c6
1:	sw	t1,6*HPC3_PBUS_CFGPIO_OFFSET(a7)
	AUTO_CACHE_BARRIERS_ENABLE

	j	ra
	END(init_hpc3)

/*
 * nuke ALL the DMA channels on the hpc3...parallel port and all audio.
 * this allows us to use the hpc3 DMA for the shutdown tune.
 */
LEAF(kill_hpc3_dma)
	CLI	t0,PHYS_TO_COMPATK1(HPC3_ETHER_MISC_ADDR)
	li	t1,0x1		# put enet controller in a reset state
	sw	t1,0(t0)

	CLI	t0,PHYS_TO_COMPATK1(HPC3_PBUS_CONTROL_BASE)
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
	CLI	t1,PHYS_TO_COMPATK1(HAL2_ADDR)		# hal2 base address

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
	CLI	t2,PHYS_TO_COMPATK1(HPC3_INT2_ADDR)
	.set 	noreorder
	# deassert reset/init retrace/select S0-DMA-SYNC
	li	t1,(PCON_CLR_SG_RETRACE_N|PCON_CLR_S0_RETRACE_N|PCON_SG_RESET_N|PCON_S0_RESET_N)
	AUTO_CACHE_BARRIERS_DISABLE		# addr constructed w/!branches
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
	CLI	t0,PHYS_TO_COMPATK1(HPC3_WRITE2) # configure duart clocks
	li	t1,UART1_ARC_MODE|UART0_ARC_MODE # low=RS422 MAC mode,hi=RS232
	sw	t1,0(t0)

	li	t1,((POWER_SUP_INHIBIT|POWER_INT)&0xff)
	CLI	t0,PHYS_TO_COMPATK1(HPC3_PANEL)	# power on and clear intr
	sw	t1,0(t0)
	AUTO_CACHE_BARRIERS_ENABLE

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
	AUTO_CACHE_BARRIERS_DISABLE			# called before mem
	.set 	noreorder
	/* clear the scsi reset line; HPC latches it on reset/powerup */
	CLI	t0,PHYS_TO_COMPATK1(SCSI0_CTRL_ADDR)
	CLI	t2,PHYS_TO_COMPATK1(0x1fc00000)
	sw	zero,SCSI1_CTRL_ADDR-SCSI0_CTRL_ADDR(t0)# SCSI1_CTRL_ADDR
	sw	zero,0(t0)				# SCSI0_CTRL_ADDR
	lw	zero,0(t2)				# flushbus
	sync
	lw	zero,0(t2)				# flushbus continued

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
	CLI	t0,PHYS_TO_COMPATK1(SCSI0A_ADDR)
	sb	t1,0(t0)
	lw	zero,0(t2)				# flushbus
	sync
	lw	zero,0(t2)				# flushbus continued
	lb	zero,SCSI0D_ADDR-SCSI0A_ADDR(t0)	# SCSI0D_ADDR
	SCSI_WAIT(t2)

	/* reset the chip; clears sync register, etc. */
	sb	zero,SCSI0D_ADDR-SCSI0A_ADDR(t0) 

	SCSI_WAIT(t2)

	/* RESET SCSI1 */
	li	t1,0x17				# SCSI reg 0x17 - status
	CLI	t0,PHYS_TO_COMPATK1(SCSI1A_ADDR)
	sb	t1,0(t0)
	lw	zero,0(t2)				# flushbus
	sync
	lw	zero,0(t2)				# flushbus continued
	lb	zero,SCSI1D_ADDR-SCSI1A_ADDR(t0)	# SCSI1D_ADDR
	SCSI_WAIT(t2)

	/* reset the chip; clears sync register, etc. */
	sb	zero,SCSI1D_ADDR-SCSI1A_ADDR(t0) 
	lw	zero,0(t2)				# flushbus
	sync
	lw	zero,0(t2)				# flushbus continued

	j	ra
	nop						# BDSLOT
	AUTO_CACHE_BARRIERS_ENABLE
	.set 	reorder
	END(init_scsi)

/*
 * boot exception vector handler
 *
 * this routine check fp for a current exception handler
 * if non-zero, jump to the appropriate handler else spin
 */
LEAF(bev_general)
	/* Check for speculative execution MC errors */
	.set	noreorder
	.set	noat
	mfc0	k1,C0_CONFIG			# if running uncached, we
	and	k1,CONFIG_K0			# won't get a speculative
	sub	k1,CONFIG_UNCACHED		# interrupt.
	beqz	k1,1f
	nop					# BDSLOT
	CLI	k1,PHYS_TO_COMPATK1(CPU_ERR_STAT)
	lw	k0,0(k1)
	and	k0,CPU_ERR_STAT_ADDR
	beqz	k0,1f
	nop					# BDSLOT
	AUTO_CACHE_BARRIERS_DISABLE		# k1 will always be valid here
	sw	zero,0(k1)			# clear error
	AUTO_CACHE_BARRIERS_ENABLE
	lw	zero,0(k1)			# flushbus
	sync
	lw	zero,0(k1)			# flushbus continued
	lw	zero,0(k1)			# wait a bit for intr to clear
	lw	zero,0(k1)			# wait a bit for intr to clear
	lw	zero,0(k1)			# wait a bit for intr to clear
	lw	zero,0(k1)			# wait a bit for intr to clear
	lw	zero,0(k1)			# wait a bit for intr to clear
	lw	zero,0(k1)			# wait a bit for intr to clear
	lw	zero,0(k1)			# wait a bit for intr to clear
	nop
	nop
	eret
	nop
1:
	.set	at
	.set	reorder

	/* Check lcl1 interrupt for power switch press -- don't touch RAM! */
	CLI	k1,PHYS_TO_COMPATK1(HPC3_INT2_ADDR)

	/* check if we have a power interrupt */
	lb	k0,LIO_1_ISR_OFFSET(k1)
	andi	k0,LIO_POWER
	beqz	k0,3f				# branch if NOT a button

	/* Switch hit, turn off watchdog timer -- since we are gonna
	 * kill power anyway, feel free to use more than just k[01].
	 */
powerdown:
	CLI	v0,PHYS_TO_COMPATK1(HPC3_PBUS_RTC_1286)
	lw	k0,0x2c(v0)			# read command
	ori	k0,k0,WTR_DEAC_WAM		# deactivate watchdog
	sw	k0,0x2c(v0)			# store command
	WBFLUSHM
	sw	zero,0x30(v0)			# zero hundreths
	WBFLUSHM
	sw	zero,0x34(v0)			# zero seconds
	WBFLUSHM

	/* james tornes and brad reger: don't change this */
2:
	/* wait for power switch to be released */
	CLI	k0,PHYS_TO_COMPATK1(HPC3_PANEL)
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
	CLI	k0,PHYS_TO_COMPATK1(HPC3_INT2_ADDR+LIO_1_ISR_OFFSET)
	lb	k1,0(k0)
	and	k1,LIO_POWER
        bne	k1,zero,2b

	/* Now go ahead and power down */
hitpow:	li	k0,~POWER_SUP_INHIBIT
	CLI	k1,PHYS_TO_COMPATK1(HPC3_PANEL)
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

3:	li	a0,EXCEPT_NORM
	b	bev_common
	END(bev_general)


/* special hook for kernel with IP26 boards to dump some state on ECC errors */
LEAF(bev_ip26_cacherror)
	LA	fp,pon_handler
XLEAF(bev_cacheerror)
	li	a0,EXCEPT_ECC
	b	bev_common
	END(bev_ip26_cacherror)

LEAF(bev_utlbmiss)
	li	a0,EXCEPT_UTLB
	b	bev_common
	END(bev_utlbmiss)

LEAF(bev_xutlbmiss)
	li	a0,EXCEPT_XUT
	b	bev_common
	END(bev_xutlbmiss)

LEAF(bev_nmi)
	.set	noat

	.set	noreorder
	mfc0	k0,C0_SR
	li	k1,PROM_SR		# ensure 64-bit, etc
	or	k0,k1
	mtc0	k0,C0_SR
	.set	reorder

	/* if it looks like memory @ 0 is not configued skip check */
#define MEM0VAL	((SEG0_BASE>>24)|MEMCFG_VLD)
#define MEM0MSK	(MEMCFG_ADDR_MASK|MEMCFG_VLD)
	CLI	k0,PHYS_TO_COMPATK1(MEMCFG0)
	lw	k1,0(k0)			# MEMCFG0
	andi	k1,MEM0MSK			# mask mapping off (bank 1)
	addi	k1,-MEM0VAL			# mapped at SEG0_BASE?
	beqz	k1,1f				# found memory
	lw	k1,0(k0)			# MEMCFG0
	srl	k1,16				# bank 0
	andi	k1,MEM0MSK			# mask mapping (bank 0)
	addi	k1,-MEM0VAL			# mapped at SEG0_BASE
	beqz	k1,1f				# found memory
	lw	k1,8(k0)			# MEMCFG1
	srl	k1,16				# bank 2
	andi	k1,MEM0MSK			# mask mapping (bank 2)
	addi	k1,-MEM0VAL			# mapped at SEG0_BASE
	bnez	k1,9f				# no memory found!
1:

	/* Try loading the NMI Vector from main memory. */
	LI	k0,GDA_ADDR		# Load address of global data area
	lw	k0,G_MAGICOFF(k0)	# Snag the actual entry
	li	k1,GDA_MAGIC		# Load magic number
	bne	k0,k1,9f		# Didn't work. Just goto pon_handler

	LI	k0,GDA_ADDR		# Reload GDA address
	ld	k1,G_NMIVECOFF(k0)	# Load the vector
	beqz	k1,9f			# If vec is zero, just goto pon

	/*  Now must go to slow mode -- save and restore calle saved regisers
	 * around the call.
	 */
	LI	k0,K1_RAMBASE|0x0c80
	REG_S	ra,R_RA*BPREG(k0)
	REG_S	k1,R_K1*BPREG(k0)
	REG_S	v0,R_V0*BPREG(k0)

	REG_S	v0,R_V1*BPREG(k0)
	REG_S	a0,R_A0*BPREG(k0)
	REG_S	a1,R_A1*BPREG(k0)

	REG_S	a2,R_A2*BPREG(k0)
	REG_S	a3,R_A3*BPREG(k0)
	REG_S	a4,R_A4*BPREG(k0)

	REG_S	a5,R_A5*BPREG(k0)
	REG_S	a6,R_A6*BPREG(k0)
	REG_S	a7,R_A7*BPREG(k0)

	REG_S	t0,R_T0*BPREG(k0)
	REG_S	t1,R_T1*BPREG(k0)
	REG_S	t2,R_T2*BPREG(k0)

	REG_S	t3,R_T3*BPREG(k0)
	REG_S	t8,R_T8*BPREG(k0)
	REG_S	t9,R_T9*BPREG(k0)


	jal	ip26_enable_ucmem

	#now safe to store into memory
	sd	zero,G_NMIVECOFF(k0)	# 2nd,3rd,.. nmi will just go to pon

	LI	k0,K1_RAMBASE|0x0c80
	REG_L	ra,R_RA*BPREG(k0)
	REG_L	k1,R_K1*BPREG(k0)
	REG_L	v0,R_V0*BPREG(k0)

	REG_L	v0,R_V1*BPREG(k0)
	REG_L	a0,R_A0*BPREG(k0)
	REG_L	a1,R_A1*BPREG(k0)

	REG_L	a2,R_A2*BPREG(k0)
	REG_L	a3,R_A3*BPREG(k0)
	REG_L	a4,R_A4*BPREG(k0)

	REG_L	a5,R_A5*BPREG(k0)
	REG_L	a6,R_A6*BPREG(k0)
	REG_L	a7,R_A7*BPREG(k0)

	REG_L	t0,R_T0*BPREG(k0)
	REG_L	t1,R_T1*BPREG(k0)
	REG_L	t2,R_T2*BPREG(k0)

	REG_L	t3,R_T3*BPREG(k0)
	REG_L	t8,R_T8*BPREG(k0)
	REG_L	t9,R_T9*BPREG(k0)


	j	k1			# Jump to NMI vector (kernel or symmon)

9:
	LA	fp,pon_handler		# set BEV handler
	li	a0,EXCEPT_NMI
	b	bev_common
	END(bev_nmi)

LEAF(bev_common)
	beq	fp,zero,1f
	move	k0,fp
	move	fp,zero
	j	k0
1:	b	1b
	END(bev_common)

/* gfxreset() w/o need for stack/mem */
LEAF(_gfxreset)
	CLI	t0,PHYS_TO_COMPATK1(CPU_CONFIG)
	lw	t1,0(t0)
	or	t2,t1,CONFIG_GRRESET
	xor	t2,CONFIG_GRRESET			# bit off
	sw	t2,0(t0)				# set reset
	lw	zero,0(t0)				# flushbus
	sync
	lw	zero,0(t0)				# us_delay(1)
	lw	zero,0(t0)				# ...
	lw	zero,0(t0)				# us_delay(2)
	lw	zero,0(t0)				# ...
	lw	zero,0(t0)				# us_delay(3)
	lw	zero,0(t0)				# ...
	lw	zero,0(t0)				# us_delay(4)
	lw	zero,0(t0)				# ...
	lw	zero,0(t0)				# us_delay(5)
	lw	zero,0(t0)				# ...
	sw	t1,0(t0)				# out of reset
	lw	zero,0(t0)				# flushbus
	j	ra
	END(_gfxreset)

/*  The T5 needs to have registers written before reading.  Get them all
 * here.
 */
LEAF(init_registers)
	move	$1,zero; move	$2,zero; move	$3,zero; move	$4,zero
	move	$5,zero; move	$6,zero; move	$7,zero; move	$8,zero
	move	$9,zero; move	$10,zero;move	$11,zero;move	$12,zero
	move	$13,zero;move	$14,zero;move	$15,zero;move	$16,zero
	move	$17,zero;move	$18,zero;move	$19,zero;move	$21,zero
	move	$22,zero;move	$23,zero;move	$24,zero;move	$25,zero
	move	$26,zero;move	$27,zero;move	$28,zero;move	$29,zero
	move	$30,zero

	.set	noreorder
	MTPS(zero,PRFCRT0);	MTPS(zero,PRFCRT1);	# clear hwperf control
	MTPC(zero,PRFCNT0);	MTPC(zero,PRFCNT1);	# clear hwperf counts
	.set	reorder

	dmtc1	zero,$f0; dmtc1	zero,$f1; dmtc1	zero,$f2; dmtc1	zero,$f3
	dmtc1	zero,$f4; dmtc1	zero,$f5; dmtc1	zero,$f6; dmtc1	zero,$f7
	dmtc1	zero,$f8; dmtc1	zero,$f9; dmtc1	zero,$f10;dmtc1	zero,$f11
	dmtc1	zero,$f12;dmtc1	zero,$f13;dmtc1	zero,$f14;dmtc1	zero,$f15
	dmtc1	zero,$f16;dmtc1	zero,$f17;dmtc1	zero,$f18;dmtc1	zero,$f19
	dmtc1	zero,$f20;dmtc1	zero,$f21;dmtc1	zero,$f22;dmtc1	zero,$f23
	dmtc1	zero,$f24;dmtc1	zero,$f25;dmtc1	zero,$f26;dmtc1	zero,$f27
	dmtc1	zero,$f28;dmtc1	zero,$f29;dmtc1	zero,$f30;dmtc1	zero,$f31

	ctc1	zero,$31		# clear floating point status register

	j	ra			# ra cleared on jal
	END(init_registers)

/* 
 * Data area
 *
 */
	BSS(bssmagic,4)
	BSS(simmerrs,4)
