#ident	"IP30prom/csu.s:  $Revision: 1.93 $"

/*
 * csu.s -- prom startup code
 */

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/RACER/gda.h>
#include <sys/RACER/racermp.h>
#include <sys/RACER/IP30addrs.h>
#include <sys/ds1687clk.h>
#include <fault.h>
#include <ml.h>

#if ENETBOOT
#include <sys/RACER/racermp.h>
#endif /* ENETBOOT */	
	
#if ENETBOOT
#define ENET_SR_BEV		0
#else
#define ENET_SR_BEV		SR_BEV
#endif

#define BSSMAGIC		0x0deadbed
#define	SCACHE_LINESIZE		128
#define	PROM_SIZE		0x100000
#define	PROM_STACK_SIZE_SHFT	15		/* 32K PROM stack */

#define	SET_STACK_POINTER_K0(scratch)				\
	LI	sp,K1_TO_K0(PROM_STACK)-EXSTKSZ-(NARGSAVE*SZREG);	\
	LI	scratch,PHYS_TO_COMPATK1(HEART_PRID);		\
	ld	scratch,0(scratch);				\
	dsll	scratch,PROM_STACK_SIZE_SHFT;			\
	dsubu	sp,scratch
#define	SET_STACK_POINTER_K1(scratch)				\
	LI	sp,PROM_STACK-EXSTKSZ-(NARGSAVE*SZREG);		\
	LI	scratch,PHYS_TO_COMPATK1(HEART_PRID);		\
	ld	scratch,0(scratch);				\
	dsll	scratch,PROM_STACK_SIZE_SHFT;			\
	dsubu	sp,scratch

#ifdef VERBOSE
#define	VPRINTF(msg)					\
	LA	a0,9f;					\
	jal	pon_puts;				\
	.data;						\
9:	.asciiz	msg;					\
	.text
#ifdef IP30_RPROM
#define RPROM_VPRINTF(msg)	VPRINTF(msg)
#else
#define RPROM_VPRINTF(msg)
#endif
#else 
#define	VPRINTF(msg)
#define RPROM_VPRINTF(msg)
#endif

#if defined(MFG_DBGPROM)
#define MFG_VPRINTF(msg)	VPRINTF(msg)
#else
#define MFG_VPRINTF(msg)
#endif

#ifndef IP30_RPROM
/*
 * BEGIN: prom entry point table
 */
LEAF(start)
	.set	noat		# so NMI preserves AT
				/* 0x000 offset */
	j	realstart
	j	bev_nmi		/* 0x008 offset hook for rprom NMI */
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

	/* ARCS dirty environment hooks.  Implement here (0x400) so they
	 * are consistant across flashes from UNIX, and halt/reboot work
	 * continue to work.
	 */
XLEAF(Halt)
	j	_Halt
XLEAF(PowerDown)
	j	_PowerDown
XLEAF(Restart)
	j	_Restart
XLEAF(Reboot)
	j	_Reboot
XLEAF(EnterInteractiveMode)
	j	_EnterInteractiveMode

/*
 * END: prom entry point table
 */

realstart:
#else	/* !IP30_RPROM */
LEAF(realstart)
#endif	/* IP30_RPROM */
	.set	noreorder

	mfc0	k0,C0_SR 		# load c0 status register
	srl	k0,16			# only touch k0 for now
	andi	k0,(SR_SR|SR_NMI)>>16
	sub	k0,(SR_SR|SR_NMI)>>16
	beqz	k0,bev_nmi		# jump to handler if so
	nop				# BDSLOT
	.set	at

#if SYMMON
#define	STARTFRM EXSTKSZ
	/* Load up symmon for dprom.DBG */

	move	v0,sp	
	LA	sp,dpromdbg_stack		# bottom stack address

	PTR_ADDU sp,(SKSTKSZ-4*BPREG)		# start stack @ top - argsaves
	sreg	sp,_fault_sp			# fault stack is top
	sreg	zero,-BPREG(sp)			# keep debuggers happy
	sreg	ra,-(2*BPREG)(sp)		# with top of fault stack
	sreg	v0,-(3*BPREG)(sp)		# set-up
	sreg	a0,0(sp)			# argsave: argc
	sreg	a1,BPREG(sp)			# argsave: argv
	sreg	a2,2*BPREG(sp)			# argsave: envp
	PTR_SUBU sp,STARTFRM			# regular stack starts here
	sreg	sp,initial_stack		# tuck stack pointer away

	/* Copy env and argv to the client's memory from the caller's
	 * memory space, resulting new values are returned on the stack.
	 */
	PTR_ADDU a0,sp,STARTFRM+(2*BPREG)	# address of envp
	jal	initenv				# initenv(char ***)
	nop					# BDSLOT
	PTR_ADDU a0,sp,STARTFRM			# address of argc
	PTR_ADDU a1,sp,STARTFRM+BPREG		# address of argv
	jal	initargv			# initargv(long *, char ***)
	nop					# BDSLOT

	/* set-up initial libsc_private for _check_dbg */
	LA	a0,ip30_tmp_libsc_private
	PTR_S	a0,_libsc_private

	/* call _check_dbg which loads symmon ... */
	lreg	a0,STARTFRM(sp)			# reload argc, argv, environ
	lreg	a1,STARTFRM+BPREG(sp)
	lreg	a2,STARTFRM+(2*BPREG)(sp)
	LA	a3,_dbgstart			# where to symmon starts client
	jal	_check_dbg			# load the debugger
	nop					# BDSLOT
	b	_nodbgmon
	nop					# BDSLOT
	
_dbgstart:				
	lreg	sp,initial_stack		# reload stack pointer
	jal	idbg_init			# load symmon
	nop					# BDSLOT
	INT_L	v0,dbg_loadstop
	beq	zero,v0,1f
	nop					# BDSLOT
	jal	quiet_debug
	nop					# BDSLOT
1:
	jal	config_cache
	nop					# BDSLOT
	jal	flush_cache
	nop					# BDSLOT
_nodbgmon:
#endif /* SYMMON */	

#if ENETBOOT
	#
	# for dprom, we need to re-launch the slave
	# processors cause they were originally put them
	# to sleep by the master processor in the prom
	#

	# Launch any disabled processors so dprom is similar to the prom
	# MP bootstrap.
	LI	t0,PHYS_TO_COMPATK1(HEART_MODE)
	ld	t2,0(t0)
	or	t2,HM_PROC_DISABLE_MSK
	xor	t2,HM_PROC_DISABLE_MSK
	sd	t2,0(t0)
	
	# put id of processor in t0
	ld	t0,PHYS_TO_COMPATK1(HEART_PRID)

	# set up a processor id mask in s0
	# note:	need s0 cause we want s0 to survive a function call
	#       ok here cause we're at the very beginning
	li	s0,0x1
	dsllv	s0,t0

	# set up a processors active mask in t1
	ld	t1,PHYS_TO_COMPATK1(HEART_STATUS)
	and	t1,HEART_STAT_PROC_ACTIVE_MSK	# active processors mask
	dsrl	t1,HEART_STAT_PROC_ACTIVE_SHFT

	# xor processor id mask with processors active mask into s0
	xor	s0, s0, t1

	# loop thru bits of the to-launch mask
	LI	s1, 0x0		# s1 is the processor counter
	# get launch field in mpconf_blk for polling
	LI	s2, MPCONF_ADDR+MP_LAUNCHOFF	
				# note:	need s1,s2 cause we want them to 
				#       survive a function call - ok here 
				#       cause we're at the very beginning
	
launch_loop:
	dsrlv	t2, s0, s1	# get processor bit from to-launch mask
	and	t2, 0x1		# if its a 1, we launch
	beq	t2, zero, next_processor	# not 1,  so goto next
	nop			# BDSLOT

	#
	# call launch_slave for each processor
	# 
	move	a0, s1          # processor id 
	LA	a1, done	# address to start executing
	move	a2, zero	# no parameters
	move	a3, zero	# no rendezvous function
	move	a4, zero	# no rendezvous parameters
	move	a5, zero	# no stack
	jal	launch_slave
	nop			# BDSLOT
	
	# loop until slave launched
launch_not_done:
	ld	t3, 0(s2)
	bne	t3, zero, launch_not_done
	nop			# BDSLOT
			
next_processor:
	daddu	s2, MPCONF_SIZE	# next mpconf_blk	
	add	s1, 0x1		# increment counter
	blt	s1, MAXCPU, launch_loop
	nop			# BDSLOT		
done:		
#endif /* ENETBOOT */		
	
	li	k0,CONFIG_UNCACHED	# set up cache algorithm for
	mtc0	k0,C0_CONFIG		# compat K0 space

	LA	k0,1f			# start executing at 0x9fc...
	j	k0			# instead of at 0xbfc..
	nop				# BDSLOT
1:

	li	v0,SR_PROMBASE|ENET_SR_BEV|SR_DE
	mtc0	v0,C0_SR		# state unknown on reset
	mtc0	zero,C0_CAUSE		# clear software interrupts
#if !SYMMON				/* for dbgstop bring-up debug */
	mtc0	zero,C0_WATCHLO		# clear/disable watchpoint interrupt
	mtc0	zero,C0_WATCHHI
#endif

	.set	reorder

	/* initialize cpu registers before using them to workaround T5 bug */
	.set	noat
			 move $1,zero;    move $2,zero;    move $3,zero
	move $4,zero;    move $5,zero;    move $6,zero;    move $7,zero
	move $8,zero;    move $9,zero;    move $10,zero;   move $11,zero
	move $12,zero;   move $13,zero;   move $14,zero;   move $15,zero
	move $16,zero;   move $17,zero;   move $18,zero;   move $19,zero
	move $20,zero;   move $21,zero;   move $22,zero;   move $23,zero
	move $24,zero;   move $25,zero;   move $26,zero;   move $27,zero
	move $28,zero;   move $29,zero;   move $30,zero;   move $31,zero
	.set	at

	dmtc1 zero,$f0;  dmtc1 zero,$f1;  dmtc1 zero,$f2;  dmtc1 zero,$f3
	dmtc1 zero,$f4;  dmtc1 zero,$f5;  dmtc1 zero,$f6;  dmtc1 zero,$f7
	dmtc1 zero,$f8;  dmtc1 zero,$f9;  dmtc1 zero,$f10; dmtc1 zero,$f11
	dmtc1 zero,$f12; dmtc1 zero,$f13; dmtc1 zero,$f14; dmtc1 zero,$f15
	dmtc1 zero,$f16; dmtc1 zero,$f17; dmtc1 zero,$f18; dmtc1 zero,$f19
	dmtc1 zero,$f20; dmtc1 zero,$f21; dmtc1 zero,$f22; dmtc1 zero,$f23
	dmtc1 zero,$f24; dmtc1 zero,$f25; dmtc1 zero,$f26; dmtc1 zero,$f27
	dmtc1 zero,$f28; dmtc1 zero,$f29; dmtc1 zero,$f30; dmtc1 zero,$f31
	ctc1  zero,$31		# fp control & status register

	.set	noreorder
	MTPS(zero,PRFCRT0);	MTPS(zero,PRFCRT1);	# clear hwperf control
	MTPC(zero,PRFCNT0);	MTPC(zero,PRFCNT1);	# clear hwperf counts
	.set	reorder

	/* determine which is the master processor, one with the smallest id */
	ld	a0,PHYS_TO_COMPATK1(HEART_PRID)		# heart processor id

	dadd	v0,a0,HEART_STAT_PROC_ACTIVE_SHFT
	LI	v1,1
	dsllv	a1,v1,v0		# my own active bit
	dsub	v0,a1,1			# mask for processors with smaller id

	ld	v1,PHYS_TO_COMPATK1(HEART_STATUS)
	and	v1,HEART_STAT_PROC_ACTIVE_MSK	# active processors mask

	and	v0,v1
	beq	v0,zero,master_continue

	/* slave processor */
	SET_STACK_POINTER_K1(v0)

	/* inter-processor interrupt status bit for this processor */
	dadd	v1,a0,HEART_INT_L2SHIFT+HEART_INT_IPISHFT
	li	a1,1
	dsllv	v1,a1,v1

	/* poll for the interrupt */
	LI	v0,PHYS_TO_COMPATK1(HEART_ISR)
1:
	ld	a1,0(v0)
	and	a1,v1
	beq	a1,zero,1b

#ifdef IP30_RPROM
	/* May start-up in rprom or fprom */
	PTR_L	a0,PHYS_TO_K1(IP30_RPROM_SLAVE)	# addr stored in NMI buffer
	beqz	a0,1f
	sd	v1,PHYS_TO_COMPATK1(HEART_CLR_ISR)	# clear IPI
	jr	a0
1:
#endif
	j	sysinit

master_continue:
	/*
	 * a0 = own processor id
	 * a1 = own active bit
	 * v1 = processors active mask
	 */
#ifndef ENETBOOT	/* confuses already disabled processors */
	xor	a1,v1
	dsll	a1,HM_PROC_DISABLE_SHFT-HEART_STAT_PROC_ACTIVE_SHFT

	/* disable slave processors */
	LI	a0,PHYS_TO_COMPATK1(HEART_MODE)
	ld	a2,0(a0)
	or	a1,a2
	sd	a1,0(a0)
#endif

	/* Power on diagnostics to check paths to HEART, XBOW, and BRIDGE.
	 * We should test IOC3 before pon_initio, but it requires pon_initio
	 * to run.  On failure we try to pon_initio, print a message, then
	 * flash the led and spin.  So given a failure, we may very well
	 * just hang.
	 */
	jal	pon_heart
	jal	pon_xbow
	jal	pon_bridge

	LA	fp,pon_handler		# set BEV handler

#if !defined(ENETBOOT) || defined(MFG_IO6)
	/*
	 * DPROM doesn't want to re-initialize the IOC3 mapping since
	 * it may conflict with the mapping set up by the PROM earlier,
	 * i.e. it may map to address space occupied by the Qlogic SCSI
	 * controller.
	 */
	jal	pon_initio		# initialize duart for I/O
#if defined(MFG_IO6)
	VPRINTF("\r\nIO6 Slot 10 - Initialized\r\n")
#endif	/* MFG_IO6 */
#endif	/* !ENETBOOT */

#if !defined(ENETBOOT)
	/* basic setup for HEART memory subsystem, assert DQM */
	LI	a1,IOC3_PCI_DEVIO_K1PTR
	li	v1,1			# make sure DQM != 0
	sw	v1,IOC3_GPPR(4)(a1)	# assert DQM
	li	v1,GPCR_DIR_MEM_DQM
	sw	v1,IOC3_GPCR_S(a1)	# enable ioc3 to drive the signal
	li	v1,1			# make sure DQM != 0, paranoid
	sw	v1,IOC3_GPPR(4)(a1)	# assert DQM

	LI	v0,PHYS_TO_COMPATK1(HEART_PIU_BASE)

	/*
	 * set up the mode register in SDRAMs, see HEART spec section 7.9
	 * for detail.  MAX_PHY_BANKS must be modified for systems that
	 * support more than 4 DIMM banks
	 */
#define	MAX_PHY_BANKS		8
#define	SDRAM_PHY_BS_SHFT	16
#define	SDRAM_MR_SELECT		0x0000
#define	SDRAM_LATENCY_MODE	0x0020
#define	SDRAM_WRAP_TYPE		0x0000
#define	SDRAM_BURST_LENGTH	0x0001
#define	SDRAM_MODE_DEFAULT	(SDRAM_MR_SELECT |	\
				 SDRAM_LATENCY_MODE |	\
				 SDRAM_WRAP_TYPE |	\
				 SDRAM_BURST_LENGTH)

	li	a0,MAX_PHY_BANKS
1:
	sub	a0,1		# bank number
	dsll	v1,a0,SDRAM_PHY_BS_SHFT
	or	v1,SDRAM_MODE_DEFAULT
	sd	v1,HEART_PIU_SDRAM_MODE(v0)

	/*
	 * must wait at least 70 MEMCLK cycles before proceeding to the
	 * next physical bank.  since the PROM is running uncached at this
	 * point and each instruction fetch takes ~1us, 8 NOPs is more
	 * than enough to cover the necessary delay
	 */
	.set	noreorder
	nop; nop; nop; nop
	nop; nop; nop;
	bne	a0,zero,1b
	nop				# BDSLOT
	.set	reorder

	/* deassert DQM */
	sw	zero,IOC3_GPPR(4)(a1)

	/* set up for memory initialization */
	LI	v1,HEART_MEMREF_VAL
	sd	v1,HEART_PIU_MEM_REF(v0)
	ld	v1,HEART_PIU_MODE(v0)
	and	v1,~HM_CACHED_PROM_EN
	or	v1,HM_REF_EN|HM_MEM_FORCE_WR|HM_DATA_ELMNT_ERE
	sd	v1,HEART_PIU_MODE(v0)
#endif	/* !defined(ENETBOOT) */

	MFG_VPRINTF("Setting LED -> GREEN\r\n")

	/* Enable LED outputs and turn LED green.  We would like to turn
	 * before early pon diags are run, but it does not work.
	 */
	LI	v0,IOC3_PCI_DEVIO_K1PTR
	li	v1,GPCR_DIR_LED1|GPCR_DIR_LED0
	sw	v1,IOC3_GPCR_S(v0)
	sw	zero,IOC3_GPPR(1)(v0)		# amber off
	li	v1,1
	sw	v1,IOC3_GPPR(0)(v0)		# green on

	MFG_VPRINTF("Setting Voltages -> NOMINAL\r\n")

	/* set voltage to nominal (after IOC3 is set-up) */
	LI	a0,IP30_VOLTAGE_CTRL
	li	a1,PWR_SUPPLY_MARGIN_LO_NORMAL|PWR_SUPPLY_MARGIN_HI_NORMAL
	sb	a1,0(a0)

	MFG_VPRINTF("Keep power on\r\n")

	jal	ip30_power_switch_on	# keep power on always till C code

#ifdef VERBOSE
#ifndef ENETBOOT
	and	a0,v0,RTC_KICKSTART_INTR
	beqz	a0,1f
	RPROM_VPRINTF("\r\nIP30 powered on by power switch.")
	b	2f
1:	and	a0,v0,RTC_WAKEUP_ALARM_INTR
	beqz	a0,1f
	RPROM_VPRINTF("\r\nIP30 powered on by wakeupat.")
	b	2f
1:	RPROM_VPRINTF("\r\nIP30 reset or AC cycled.")
#endif	/* ENETBOOT */
2:
#ifdef IP30_RPROM
	VPRINTF("\r\nI am alive: C0_CONFIG=")
	.set	noreorder
	mfc0	a0,C0_CONFIG
	.set	reorder
	jal	pon_puthex
	VPRINTF(", C0_PRID=")
	.set	noreorder
	mfc0	a0,C0_PRID
	.set	reorder
	jal	pon_puthex
	VPRINTF("\r\n")
#else
	VPRINTF("FPROM is alive.\r\n");
#endif
	ld	a0,prom_version
	jal	pon_puts
	VPRINTF("\r\n")
#endif	/* VERBOSE */

	MFG_VPRINTF("Testing IOC3 Data lines ...\r\n")

	jal	pon_ioc3

	MFG_VPRINTF("Testing RTC Data lines ...\r\n")

	jal	pon_rtc			# test RTC path

	VPRINTF("Sizing memory and running memory diagnostic.\r\n")
	jal	init_memconfig		# initialize memory configuration

	LI	v0,PHYS_TO_COMPATK1(HEART_PIU_BASE)
	ld	v1,HEART_MODE-HEART_PIU_BASE(v0)
	and	v1,~HM_MEM_FORCE_WR
	sd	v1,HEART_MODE-HEART_PIU_BASE(v0)

	VPRINTF("Initializing PROM stack.\r\n")
	jal	init_prom_ram		# init stack and bss

	VPRINTF("Start executing C code.\r\n")
	j	sysinit			# goodbye

#ifndef IP30_RPROM
	END(start)
#else
	END(realstart)
#endif /* IP30_RPROM */

/* bev_nmi: If memory is initialized, and GDA has been set-up, jump there,
 * otherwise dump as much information as we can (depending on if memory is
 * valid).
 *
 * The RPROM sets SR, saves k1, debounces the switch before we get here.
 */
LEAF(bev_nmi)
	.set	noat
	/*  If it looks like memory is not configured, just try to dump
	 * init the IO and dump what we can.  It's not 100% clear this
	 * will help, but it might for early hangs, especially after we
	 * are out of the lab.
	 */
	li	k1,HEART_MEMCFG_VLD
	LI	k0,PHYS_TO_COMPATK1(HEART_MEMCFG(0))
	ld	k0,0(k0)
	and	k0,k1
	bnez	k0,1f
	LI	k0,PHYS_TO_COMPATK1(HEART_MEMCFG(1))
	ld	k0,0(k0)
	and	k0,k1
	bnez	k0,1f
	LI	k0,PHYS_TO_COMPATK1(HEART_MEMCFG(2))
	ld	k0,0(k0)
	and	k0,k1
	bnez	k0,1f
	LI	k0,PHYS_TO_COMPATK1(HEART_MEMCFG(3))
	ld	k0,0(k0)
	and	k0,k1
	bnez	k0,1f

	/* Just spin processor 1 here. */
	LI	k1,PHYS_TO_COMPATK1(HEART_PRID)
	ld	k1,0(k1)		# this processor's id
	beqz	k1,23f			# proc zero dumps

99:	b	99b			# spin processor 1

	/* No memory initalized, just give it a try (processor 0) */
23:	jal	pon_initio		# give it a try.
	li	a0,EXCEPT_NMI
	j	pon_handler
	/* NOTREACHED */

	/* Try loading the NMI vector from main memory */
1:	LI	k0,GDA_ADDR		# address of the GDA
	lw	k0,G_MAGICOFF(k0)	# read GDA magic number
	li	k1,GDA_MAGIC		# the correct GDA magic number
	bne	k0,k1,2f		# no match, GDA not initialized

	/* Increment G_COUNT on the master processor */
	LI	k0,GDA_ADDR		# addresss of the GDA
	LONG_L	k0,G_MASTEROFF(k0)	# read master processor id
	ld	k1,PHYS_TO_COMPATK1(HEART_PRID)	# running processors id
	bne	k1,k0,1f		# not the master processor
	LI	k0,GDA_ADDR		# addresss of the GDA
	ld	k1,G_COUNT(k0)		# increment the count.
	daddiu	k1,1
	sd	k1,G_COUNT(k0)

1:	LI	k0,GDA_ADDR		# address of the GDA
	LONG_L	k0,G_NMIVECOFF(k0)	# NMI handler address
	beq	k0,zero,1f		# must not be zero

	dmfc1	k1,$f1			# restore k1 for NMI handler

	j	k0			# jump to the NMI handler

1:
	LI	k0,GDA_ADDR		# address of the GDA
	LONG_L	k0,G_MASTEROFF(k0)	# read master processor id
	b	3f
2:	move	k0,zero			# no gda let CPU 0 go first
3:	ld	k1,PHYS_TO_COMPATK1(HEART_PRID)	# running processors id
	bne	k1,k0,1f		# not the master processor

	/* Slightly higher than low level handler */
	j	pon_nmi_handler		# display info & loop forever

	/* Spin for a 2 seconds, then dump second CPU -- only use kx regs */
1:	LI	k0,PHYS_TO_COMPATK1(HEART_COUNT)
	ld	k1,0(k0)
	li	k0,2*HEART_COUNT_RATE	# 2 seconds
	daddu	k1,k0			# base + 2 seconds
1:	LI	k0,PHYS_TO_COMPATK1(HEART_COUNT)
	ld	k0,0(k0)
	dsub	k0,k1,k0
	bgtz	k0,1b

	j	pon_nmi_slave_handler	# display slave info & loop forever
	.set	at
	END(bev_nmi)

#ifndef IP30_RPROM
/*******************************************************************/
/* 
 * ARCS Prom entry points
 */

#define ARCS_HALT	1
#define ARCS_POWERDOWN	2		/* unused */
#define ARCS_RESTART	3
#define ARCS_REBOOT	4
#define ARCS_IMODE	5

LEAF(_Halt)
	li	s0,ARCS_HALT
	j	arcs_common
	END(_Halt)

LEAF(_PowerDown)
1:	j	bev_poweroff
	END(_PowerDown)

LEAF(_Restart)
	li	s0,ARCS_RESTART
	j	arcs_common
	END(_Restart)

LEAF(_Reboot)
	li	s0,ARCS_REBOOT
	j	arcs_common
	END(_Reboot)

/* EnterInteractive mode goes to prom menu.  If called from outside the
 * prom or bssmagic, or envdirty is true, do full clean-up, else just
 * re-wind the stack to keep the prom responsive.
 */
LEAF(_EnterInteractiveMode)
	.set	noreorder
	/* just in case came here via the ECC exception handler */
	li	t0,CONFIG_NONCOHRNT
	mtc0	t0,C0_CONFIG
	.set	reorder

	LA	t0,start
	PTR_ADDU	t1,t0,PROM_SIZE
	sgeu	t0,ra,t0
	sltu	t1,ra,t1
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
	# light clean-up, reset the stack
	LI	sp,K1_TO_K0(PROM_STACK)-(NARGSAVE*SZREG)
	sd	sp,_fault_sp
	SET_STACK_POINTER_K0(v0)
	jal	main
	# if main fails fall through and re-init
1:
	# go whole hog
	li	s0,ARCS_IMODE

	/* FALLTHROUGH */

	END(_EnterInteractiveMode)

LEAF(arcs_common)
	.set	noreorder
#if !SYMMON
	mtc0	zero,C0_WATCHLO		# make sure watchpoints are clear
	mtc0	zero,C0_WATCHHI
#endif
	li	a0,SR_PROMBASE|ENET_SR_BEV
#ifndef ENETBOOT
	LA	fp,pon_handler		# set BEV handler
#endif
	mtc0	a0,C0_SR		# no interrupts
	.set	reorder

	LI	v0,GDA_ADDR
	ld	v0,G_MASTEROFF(v0)	# get master processor number
	LI	v1,PHYS_TO_COMPATK1(HEART_PRID)
	ld	v1,0(v1)		# processor id
	beq	v0,v1,1f

	SET_STACK_POINTER_K0(v0)

	li	v0,HEART_INT_L2SHIFT+HEART_INT_IPISHFT
	daddu	v0,v1
	li	v1,1
	dsllv	v0,v1,v0		# IPI status in heart ISR for this cpu

	LI	a1,PHYS_TO_COMPATK1(HEART_ISR)
	LI	t0,PHYS_TO_COMPATK1(HEART_COUNT)
	ld	t2,0(t0)
	daddu	t2,75000000		# 6 seconds timeout (2 was a bit short)

	/*
	 * wait for interprocessor interrupt from the master processor,
	 * which indicates bss has been set up and is ready to be used
	 */
99:
	ld	t1,0(t0)
	bgt	t1,t2,100f
	ld	a0,0(a1)
	and	a0,v0
	beq	a0,zero,99b

	move	a0,zero
	jal	init_prom_soft

	/* timeout, reset the system */
100:
	VPRINTF("slave reset!\r\n");
	LI	a0,PHYS_TO_COMPATK1(HEART_MODE)
	ld	a1,0(a0)
	or	a1,HM_SW_RST
	sd	a1,0(a0)
	ld	zero,0(a0)		# flush the bus
	b	100b			# make sure we do not keep going

	/* NOTREACHED */

	/* for master processor only */
1:
	li	a0,MAXCPU
	LI	a1,MPCONF_ADDR

2:	/* check whether the slave processor is in spin loop */
	lw	v0,MP_IDLEFLAG(a1)
	beq	v0,zero,4f		# branch if not

	/*
	 * if the slave processor is in spin loop, tell it to jump to
	 * arcs_common
	 */
	LA	v0,arcs_common
	sd	zero,MP_STACKADDR(a1)
	sd	v0,MP_LAUNCHOFF(a1)

3:	/*
	 * wait until the slave processor get the message since slave_loop
	 * play with variables in bss
	 */
	lw	v0,MP_IDLEFLAG(a1)
	bne	v0,zero,3b

4:
	dsubu	a0,1
	beq	a0,zero,5f		# branch if done with all processors

	/* next mpconf block */
	daddu	a1,MPCONF_SIZE
	b	2b

5:
	/* Can take a few ms for each slave to wake up, make sure they
	 * make it to the prom cached before we reset xtalk.
	 */
	LI	t0,PHYS_TO_COMPATK1(HEART_COUNT)
	ld	a0,0(t0)		# initial count
	daddu	a0,625000		# 50 ms
6:	ld	a1,0(t0)		# resample count
	bltu	a1,a0,6b

#ifndef ENETBOOT
	/* warm reset will reset baseio, causing the IOC3
	 * DQM signal to inhibit memory access until we
	 * de-assert it
	 */
	jal	warm_reset_ip30_xio	# reset all xbow links
#endif
	jal	pon_initio		# initialize duart for I/O

	/* de-assert DQM caused by warm_reset_ip30_xio which reset IOC3
	 */
	LI	a1,IOC3_PCI_DEVIO_K1PTR
	sw	zero,IOC3_GPPR(4)(a1)	# de-assert DQM
	li	t0,GPCR_DIR_MEM_DQM	# ioc3 enable to GPPR[4]
	sw	t0,IOC3_GPCR_S(a1)	# drive the signal
	sw	zero,IOC3_GPPR(4)(a1)	# one more time paranoid
	lw	zero,IOC3_GPPR(4)(a1)	# wbflush

#if SYMMON
	jal	save_symmon_db
#endif
	jal	clear_prom_ram		# clear_prom_ram, initializes sp
#if SYMMON
	jal	restore_symmon_db
#endif
	move	a0,zero			# flag to re-initialize environment
	jal	init_prom_soft		# initialize saio and everything else

	bne	s0,ARCS_RESTART,1f
	jal	restart
	j	never			# never gets here
1:
	# restart cannot reset the soft power controls since it will
	# overwrite the alarm information for auto power on.
	jal	ip30_power_switch_on

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
#ifndef ENETBOOT
	LA	fp,pon_handler
#endif
	li	a0,SR_PROMBASE|ENET_SR_BEV
	mtc0	a0,C0_SR		# back to a known sr
	mtc0	zero,C0_CAUSE		# clear software interrupts
	.set	reorder

	lw	v0,bssmagic		# Check to see if bss valid
	bne	v0,BSSMAGIC,_init	# If not, do a hard init

	/* reset the stack */
	LI	sp,K1_TO_K0(PROM_STACK)-(NARGSAVE*SZREG)
	sd	sp,_fault_sp
	SET_STACK_POINTER_K0(v0)
	jal	config_cache		# determine cache sizes
	jal	flush_cache		# flush cache
	jal	enter_imode
1:	b	1b			# never gets here
	END(_exit)
#endif /* !IP30_RPROM */

/*
 * notimplemented -- deal with calls to unimplemented prom services
 */
NOTIMPFRM=((NARGSAVE * SZREG) + ALSZ) & ALMASK
NESTED(notimplemented, NOTIMPFRM, zero)
	PTR_SUBU	sp,NOTIMPFRM
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

#ifndef IP30_RPROM
/*******************************************************************/
/*
 * Subroutines
 *
 */

/*
 * _init -- reinitialize prom and reenter command parser
 */
LEAF(_init)
	/* for master processor only */
	li	a0,MAXCPU
	LI	a1,MPCONF_ADDR

2:	/* check whether the slave processor is in spin loop */
	lw	v0,MP_IDLEFLAG(a1)
	beq	v0,zero,4f		# branch if not

	/*
	 * if the slave processor is in spin loop, tell it to jump to
	 * _init
	 */
	LA	v0,arcs_common
	sd	zero,MP_STACKADDR(a1)
	sd	v0,MP_LAUNCHOFF(a1)

3:	/*
	 * wait until the slave processor get the message since slave_loop
	 * play with variables in bss
	 */
	lw	v0,MP_IDLEFLAG(a1)
	bne	v0,zero,3b

4:
	dsubu	a0,1
	beq	a0,zero,5f		# branch if done with all processors

	/* next mpconf block */
	daddu	a1,MPCONF_SIZE
	b	2b

5:
#if SYMMON
	jal	save_symmon_db
#endif
	jal	clear_prom_ram		# clear_prom_ram, initializes sp
#if SYMMON
	jal	restore_symmon_db
#endif
	move	a0,zero			# flag to re-initialize environment
	jal	init_prom_soft
	jal	main
	jal	EnterInteractiveMode	# shouldn't get here
	END(_init)
#endif	/* !IP30_RPROM */

/*
 * clear_prom_ram -- clear prom/dprom bss, initialize stack pointer
 */
#define	STACK_BOTTOM	K1_TO_K0(PROM_STACK)-EXSTKSZ-(NARGSAVE*SZREG)- \
			MAXCPU*(0x1<<PROM_STACK_SIZE_SHFT)

INITMEM_LOCALSZ = 1			# save ra only
INITMEM_FRAMESZ =(((NARGSAVE + INITMEM_LOCALSZ) * SZREG) + ALSZ) & ALMASK
INITMEM_RAOFF = INITMEM_FRAMESZ - (1 * SZREG)

#ifndef IP30_RPROM
NESTED(clear_prom_ram, INITMEM_FRAMESZ, zero)
#ifdef ENETBOOT
	LI	a0,K1_TO_K0(DBSSADDR)
	daddu	a1,a0,0x100000
#else
	LI	a0,K1_TO_K0(BSSADDR)
	LI	a1,STACK_BOTTOM
#endif	/* ENETBOOT */

 	.set	noreorder
1:	PTR_ADDU	a0,SCACHE_LINESIZE	# clear 1 cacheline at a time
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
	sd	zero,-120(a0)
 	bltu	a0,a1,1b
	sd	zero,-128(a0)		# BDSLOT
 	.set	reorder

	/*
	 * writeback and invalidate the secondary cache just in case BSS is
	 * uncached
	 */
 	.set	noreorder
	mfc0	v0,C0_CONFIG
	and	v0,CONFIG_SS
	dsrl	v0,CONFIG_SS_SHFT
	dadd	v0,CONFIG_SCACHE_POW2_BASE
	li	v1,1
	dsllv	v1,v0			# cache size in byte
	dsrl	v1,1			# cache is 2-way associative

	LI	v0,K0_RAMBASE
	PTR_ADDU	v1,v0

1:	PTR_ADDU	v0,SCACHE_LINESIZE*2
	cache	CACH_SD|C_IWBINV,-2*SCACHE_LINESIZE(v0)
	cache	CACH_SD|C_IWBINV,-2*SCACHE_LINESIZE|1(v0)
	cache	CACH_SD|C_IWBINV,-1*SCACHE_LINESIZE|1(v0)
	bltu	v0,v1,1b
	cache	CACH_SD|C_IWBINV,-1*SCACHE_LINESIZE(v0)
	.set	reorder

	/* Remember that bss is now okay */
	li	v0,BSSMAGIC
	sw	v0,bssmagic

	/* Initialize stack */
	LI	sp,K1_TO_K0(PROM_STACK)-(NARGSAVE*SZREG)
	sd	sp,_fault_sp		# base of fault stack

	SET_STACK_POINTER_K0(v0)
	PTR_SUBU	sp,INITMEM_FRAMESZ
	sd	ra,INITMEM_RAOFF(sp)
	jal	szmem			# determine main memory size
	sd	v0,memsize

	ld	ra,INITMEM_RAOFF(sp)
	PTR_ADDU	sp,INITMEM_FRAMESZ
	j	ra
	END(clear_prom_ram)
#endif	/* !IP30_RPROM */

/* only gets called during power-on */
LEAF(init_prom_ram)
	/*
	 * Remember that bss is now okay
	 */
	li	v0,BSSMAGIC
	LA	v1,bssmagic
	and	v1,TO_PHYS_MASK
	or	v1,K1BASE
	sw	v0,0(v1)

	/*
	 * retrieve bad simm flags from mult-hi register
	 */
	mfhi	v0
	LA	v1,simmerrs
	and	v1,TO_PHYS_MASK
	or	v1,K1BASE
	sw	v0,0(v1)

	/*
	 * Initialize stack
	 */
	LI	sp,PROM_STACK-(NARGSAVE*SZREG)

	LA	v1,_fault_sp
	and	v1,TO_PHYS_MASK
	or	v1,K1BASE
	sd	sp,0(v1)		# base of fault stack

	SET_STACK_POINTER_K1(v0)
	PTR_SUBU	sp,INITMEM_FRAMESZ
	sd	ra,INITMEM_RAOFF(sp)
	jal	szmem			# determine main memory size

	LA	v1,memsize
	and	v1,TO_PHYS_MASK
	or	v1,K1BASE
	sd	v0,0(v1)

	ld	ra,INITMEM_RAOFF(sp)
	PTR_ADDU	sp,INITMEM_FRAMESZ
	j	ra
	END(init_prom_ram)

/*
 * boot exception vector handler
 *
 * this routine check fp for a current exception handler
 * if non-zero, jump to the appropriate handler else spin
 */
	.set	noat			# do not trash AT in bev routines
LEAF(bev_general)
	/* Check for power interrupt */
	lw	k0,PHYS_TO_COMPATK1(BRIDGE_BASE|BRIDGE_INT_STATUS)
	andi	k0,PWR_SWITCH_INTR
	beqz	k0,1f
	j	bev_poweroff

	.set	noreorder
1:	mfc0	a1,C0_CAUSE
	DMFC0(a2,C0_EPC)
	.set	reorder
	li	a0,EXCEPT_NORM
	b	bev_common
	END(bev_general)

LEAF(bev_cacheerror)
	.set	noreorder
	mfc0	a1,C0_CACHE_ERR
	DMFC0(a2,C0_ERROR_EPC)
	.set	reorder
	li	a0,EXCEPT_ECC
	b	bev_common
	END(bev_cacheerror)

LEAF(bev_utlbmiss)
	.set	noreorder
	mfc0	a1,C0_CAUSE
	DMFC0(a2,C0_EPC)
	.set	reorder
	li	a0,EXCEPT_UTLB
	b	bev_common
	END(bev_utlbmiss)

LEAF(bev_xutlbmiss)
	.set	noreorder
	mfc0	a1,C0_CAUSE
	DMFC0(a2,C0_EPC)
	.set	reorder
	li	a0,EXCEPT_XUT
	b	bev_common
	END(bev_xutlbmiss)

LEAF(bev_common)
	LI	v0,PHYS_TO_COMPATK1(HEART_TRIGGER)
	sd	a1,0(v0)
	sd	a2,0(v0)

	beq	fp,zero,1f
	move	k0,fp
	move	fp,zero
	j	k0

1:
	j	powerspin			/* poll for power intr */
	END(bev_common)
	.set	at

#ifdef SYMMON
#include <arcs/spb.h>
LEAF(save_symmon_db)
	LI	t0,SPB_DEBUGADDR	# SPB pointer
	lreg	t0,0(t0)		# actual pointer
	beqz	t0,1f
	LI	t1,PHYS_TO_K0(IP30_EARLY_NMI)	# cheat with early NMI buffer
	ld	t2,0x00(t0)		# approx size of debug_block
	sd	t2,0x00(t1)
	ld	t2,0x08(t0)
	sd	t2,0x08(t1)
	ld	t2,0x10(t0)
	sd	t2,0x10(t1)
	ld	t2,0x18(t0)
	sd	t2,0x18(t1)
	ld	t2,0x20(t0)
	sd	t2,0x20(t1)
	ld	t2,0x28(t0)
	sd	t2,0x28(t1)
	ld	t2,0x30(t0)
	sd	t2,0x30(t1)
	ld	t2,0x38(t0)
	sd	t2,0x38(t1)
	ld	t2,0x40(t0)
	sd	t2,0x40(t1)
	ld	t2,0x48(t0)
	sd	t2,0x48(t1)
1:	j	ra
	END(save_symmon_db)

LEAF(restore_symmon_db)
	LI	t1,SPB_DEBUGADDR	# SPB pointer
	lreg	t1,0(t1)		# actual pointer
	beqz	t1,1f
	LI	t0,PHYS_TO_K0(IP30_EARLY_NMI)	# cheat with early NMI buffer
	ld	t2,0x00(t0)		# approx size of debug_block
	sd	t2,0x00(t1)
	ld	t2,0x08(t0)
	sd	t2,0x08(t1)
	ld	t2,0x10(t0)
	sd	t2,0x10(t1)
	ld	t2,0x18(t0)
	sd	t2,0x18(t1)
	ld	t2,0x20(t0)
	sd	t2,0x20(t1)
	ld	t2,0x28(t0)
	sd	t2,0x28(t1)
	ld	t2,0x30(t0)
	sd	t2,0x30(t1)
	ld	t2,0x38(t0)
	sd	t2,0x38(t1)
	ld	t2,0x40(t0)
	sd	t2,0x40(t1)
	ld	t2,0x48(t0)
	sd	t2,0x48(t1)
1:	j	ra
	END(restore_symmon_db)
#endif

/*******************************************************************/
/* 
 * Data area
 *
 */
	BSS(bssmagic,4)
	BSS(simmerrs,4)
#if SYMMON
	BSS(dpromdbg_stack,SKSTKSZ)
	BSS(_argc, 4)
	BSS(_argv, 8)
#endif
