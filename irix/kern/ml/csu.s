/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1995, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.142 $"

#include "ml/ml.h"

#ifdef EVEREST
#include <sys/loaddrs.h>
#endif
#if IP19 || IP25 || (IP22 && _K64PROM32)
#include "sys/EVEREST/evaddrmacros.h"
#endif
#if defined(EVEREST) && defined(MULTIKERNEL)
#include "sys/EVEREST/evmp.h"
#endif	
	
/*
 * csu.s - early kernel start up and shutdown support
 *
 * This file contains the kernel's start point and the init process
 * bootstrap routine.
 */

/*
 * Kernel entry point
 *
 * since we can call cmn_err - which thinks it has 18 args, we
 * leave room for that many on our stacks!
 */

STARTLOCALSZ=	2			/* Save ra, old fp */
STARTFRM=	FRAMESZ((NARGSAVE+STARTLOCALSZ)*SZREG)
RAOFF=		STARTFRM-(1*SZREG)
FPOFF=		STARTFRM-(2*SZREG)

NESTED(start, STARTFRM, zero)
	/*
	 * Kernel initialization
	 *
	 * Now on prom stack; a0, a1, and a2 contain argc, argv, and environ
	 * from boot.
	 */
	LA	gp,_gp			# don't forget gp!

#ifdef MAPPED_KERNEL
#ifdef SN0
	.set noreorder
	move	s0, a0			# Save a0 value
	move	s1, a1			# Save a1 value
	GET_NASID_ASM(a0)		# Get master NASID
	move	a1, a0			# Map both text and data to our NASID

	jal	mapped_kernel_setup_tlb	# Set up kernel text and data TLB
					# entries.
	move	t0, zero	
	LA	t0, 1f			# Load the link address for label 1f
	jr	t0			# Jump to our link (mapped) address
	nop
1:
	nop
	move	a0, s0			# Restore a0 value
	move	a1, s1			# Restore a1 value
	.set reorder
#else /* !SN0 */
#if defined(IP19) || defined(IP25)
	/* This is for EVMK kernels intended for testing multi-kernel IP19 */

	.set noreorder
	move	s0, a0			# Save a0 value
	move	s1, a1			# Save a1 value
	move	s2, a2			# Save a2 value
	
	move	a0, zero		# Get master NASID (actually physaddr)
	move	a1, a0			# Map both text and data to our NASID

	jal	mapped_kernel_setup_tlb	# Set up kernel text and data TLB
					# entries.
	move	t0, zero	
	LA	t0, 1f			# Load the link address for label 1f
	jr	t0			# Jump to our link (mapped) address
	nop
1:
	nop
	move	a0, s0			# Restore a0 value
	move	a1, s1			# Restore a1 value
	move	a2, s2
	.set reorder
#else /* !IP19 */	
	Bomb!	Mapped kernels are currently only supported on SN0.
#endif /* !IP19 */	
#endif /* !SN0 */
#endif /* MAPPED_KERNEL */

#if defined(EVEREST) && defined(MULTIKERNEL)
	jal	evmk_multicell_start
#endif /* EVEREST && MULTIKERNEL */	
#if TFP
	/* Initialize wired register so HW can check valid bits on first
	 * ktlbmiss (RTL simulator didn't like uninitialized values --
	 * not a bad idea to init it anyway).
	 */
	.set	noreorder
	DMFC0(k0, C0_CONFIG)
	and	k0, ~CONFIG_ICE	# Turn off "Inhibit Count during Exception" bit
	DMTC0(k0, C0_CONFIG)

	DMTC0( zero, C0_WIRED )
	DMTC0( zero, C0_COUNT )
	DMTC0( zero, C0_CAUSE )	/* clear potential left-over VCI/BE */
#if EVEREST
	LI	k0,EV_CERTOIP
	li	k1,0x3fff	/* clear pending timeouts */
	sd	k1,0(k0)
	
	/* mdeneroff says we should run IP21 at low3v */
	
	li	t0, EV_VCR_LOW3V
	LI	t1, EV_VOLTAGE_CTRL
	sd	t0, (t1)
#endif
	LI	k0,SR_DEFAULT
	MTC0(k0,C0_SR)		/* set pg size, clear EXL so ktlbmiss works */
#if IP26
	lw	k0,ip26_sr_mask
	DMTC0( k0, C0_WORK1 )	/* IP26 spl mask */
#endif
	.set	reorder
#elif _MIPS_SIM == _ABI64 /* and !TFP */
	.set	noreorder
	/* Make sure the KX bit is on. */
	MFC0(k0,C0_SR)
	LI	k1, SR_KERN_SET
	or	k0, k1
	MTC0(k0,C0_SR)
	NOP_1_4

	/* Now modify PC so we're running out of K0 space */
	LA	k1, 1f		/* We may have been loaded from a
				 * 32-bit prom.  If so, we're running in
				 * compatibility space (0x80000000).
				 */
	nop
	j	k1
	NOP_1_2
1:
	NOP_0_4
	.set	reorder
#endif /* TFP */
#if defined(_RUN_UNCACHED) && !defined(TFP) 
	.set	noreorder
	mfc0	a0,C0_CONFIG
	NOP_0_4
	and	a0,~CONFIG_K0
	ori	a0,CONFIG_UNCACHED
	mtc0	a0,C0_CONFIG
	NOP_0_4
	.set	reorder
#endif /* _RUN_UNCACHED */

#if defined(SABLE) && defined(SN)
	.set	noreorder
	# Only let one processor run the master code.
	LA	k0, sable_mastercounter

XLEAF(sable_increment_counter)

	# Increment the counter
2:	ll	a0, (k0)
        add     k1, a0, 1
        sc      k1, (k0)
        beqz    k1, 2b;
        nop

	beqz	a0, 3f
	nop

	# We're a slave.
	LA	k0, slave_loop
	jr	k0
	nop
	.set	reorder
3:

#endif

	.globl eintstack		/* end of interrupt stack  */
	PTR_L	sp,eintstack
#if SABLE_RTL
	/* RTL simulator does not init the register file.
	 * Init a0 through a3 for environment info from "prom".
	 * Init "saved" registers in case they're accessed before we
	 * actually use them.
	 */
	move	a0,zero
	move	a1,zero
	move	a2,zero
	move	a3,zero
	move	s0,zero
	move	s1,zero
	move	s2,zero
	move	s3,zero
	move	s4,zero
	move	s5,zero
	move	s6,zero
	move	s7,zero
	move	s8,zero
	PTR_L	sp,eintstack		# make sure stack set up
#endif	/* SABLE_RTL */
	sw	a0,_argc

	PTR_S	a1,_argv
	PTR_S	a2,_envirn

#if !SABLE_RTL || SABLE_SYMMON
#if _K64PROM32
	#transform the SPB to 64-bit;  this must be done
	#very early, so that exception vector can be stored in SPB
	jal	swizzle_SPB
#endif
#if IP19
	jal	ip19_cache_init
#endif
#ifdef IP28
	/* Ensure K0 compat is cached (for now?).
	 */
#if !defined(_RUN_UNCACHED)
	.set	noreorder
	mfc0	a0,C0_CONFIG
	and	a0,~CONFIG_K0
	or	a0,CONFIG_NONCOHRNT
	mtc0	a0,C0_CONFIG
	.set	reorder
#endif

	/* Initialize IP28 ECC springboard.
	 */
	LA	a0,ecc_springboard
	LI	a1,K0_RAMBASE|ECC_SPRING_BOARD
	li	a2,0x400		# max size (much less now)
	jal	bcopy
	LI	a1,K0_RAMBASE|ECC_SPRING_BOARD
	.set	noreorder
	cache	CACH_SD|C_HWBINV,0*CACHE_SLINE_SIZE(a1)
	cache	CACH_SD|C_HWBINV,1*CACHE_SLINE_SIZE(a1)
	cache	CACH_SD|C_HWBINV,2*CACHE_SLINE_SIZE(a1)
	cache	CACH_SD|C_HWBINV,3*CACHE_SLINE_SIZE(a1)
	cache	CACH_SD|C_HWBINV,4*CACHE_SLINE_SIZE(a1)
	cache	CACH_SD|C_HWBINV,5*CACHE_SLINE_SIZE(a1)
	cache	CACH_SD|C_HWBINV,6*CACHE_SLINE_SIZE(a1)
	cache	CACH_SD|C_HWBINV,7*CACHE_SLINE_SIZE(a1)
	.set	reorder
#endif	/* IP28 */

	lw	a0,_argc
	PTR_L	a1,_argv
	PTR_L	a2,_envirn
	LA	a3,dbg_start		# location dbgmon will jump to


	jal	_check_dbg
	/*
	 * return here after possibly going to dbgmon and back.
	 */
dbg_start:
	
#if (EVEREST && !SABLE) || (IP30 && MP)
	/*
	 * Start the slaves in the kernel slave loop(stackless) as
	 * soon as possible.
	 */
	jal	start_slave_loop
#endif /* EVEREST */
	LA	gp,_gp			# bug in symmon? need to do this again!
	
	PTR_L	a1,_argv
	PTR_L	a2,_envirn
	PTR_L	sp,eintstack		# make sure stack set up
	lw	v0,dbgmon_loadstop	# check for stopping if dbgmon
	beq	zero,v0,dont_stop	# really loaded
	LA	a0,_quiet
	jal	debug
#endif /* !SABLE_RTL || SABLE_SYMMON */
dont_stop:
	lw	a0,_argc
	REG_S	zero,RAOFF(sp)		# zero old ra for debuggers
	REG_S	zero,FPOFF(sp)		# zero old fp for debuggers (???)
	.set 	noreorder
#if TFP
	DMTC0(zero,C0_TLBHI)
	NOINTS(a3,C0_SR)		# paranoid
	DMTC0(zero,C0_ICACHE)		# init IASID
	/* Here we load a value into C0_WIRED which will allow us to wire
	 * the PDA, U area, and kernel stack.  Assumes that they are all
	 * in distinct congruence classes.
	 */
	LI	a3,(((PDAPAGE>>PNUMSHFT)&WIRED_MASK)|WIRED_VMASK)<<WIRED_SHIFT0
	LI	v0,((((KSTACKPAGE)>>PNUMSHFT)&WIRED_MASK)|WIRED_VMASK)<<WIRED_SHIFT2
	or	a3,v0
	DMTC0(a3,C0_WIRED)

	LI	a3,KPTEBASE
	DMTC0(a3,C0_KPTEBASE)		# For quick reference by utlbmiss

#if EVEREST
	LI	a3,EV_CEL
	DMTC0(a3,C0_CEL)		# For quick reference by exception code
#endif
#endif	/* TFP */
#if R4000 || R10000
	MTC0(zero, C0_CAUSE)
	DMTC0_NO_HAZ(zero,C0_TLBHI)
	NOINTS(a3,C0_SR)		# paranoid

#ifdef SN0
	MFC0(k0,C0_SR)
	LI	k1, SR_BEV
	or	k0, k1
	MTC0(k0,C0_SR)
#endif

#ifdef R4600
	nop
	mfc0	a3,C0_PRID
	NOP_0_4
	.set	reorder
	and	a3,C0_IMPMASK
	subu	a3,(C0_IMP_R4600 << C0_IMPSHIFT)
	beqz	a3,98f
	subu	a3,((C0_IMP_R4700 << C0_IMPSHIFT) - (C0_IMP_R4600 << C0_IMPSHIFT))
	beqz	a3,98f
#ifdef TRITON	
	subu	a3,((C0_IMP_TRITON << C0_IMPSHIFT) - (C0_IMP_R4700 << C0_IMPSHIFT))
	beqz	a3,98f
	subu	a3,((C0_IMP_RM5271 << C0_IMPSHIFT) - (C0_IMP_TRITON << C0_IMPSHIFT))
	beqz	a3,98f
	nop
#endif /* TRITON */
	.set	noreorder
#endif
	mtc0	zero,C0_WATCHLO		# No watch point exceptions.
	mtc0	zero,C0_WATCHHI
#ifdef R4600
98:
#endif
	li	a3,TLBPGMASK_MASK
	mtc0	a3,C0_PGMASK
	li	a3,NWIREDENTRIES
	mtc0	a3,C0_TLBWIRED		# Number of wired tlb entries
#if _MIPS_SIM == _ABI64
	# pairs of 4-byte ptes
	LI	v0,KPTEBASE<<(KPTE_TLBPRESHIFT+PGSHFTFCTR)
#if R10000
	DMTC0(v0,C0_EXTCTXT)
	LI	v0,FRAMEMASK_MASK
	DMTC0(v0,C0_FMMASK)
#else
	DMTC0(v0,C0_CTXT)
#endif /* R10000 */
#else
	# pairs of 4-byte ptes (use dli so ragnarok assembler doesn't complain)
	dli	v0,-((-KPTEBASE)<<(KPTE_TLBPRESHIFT+PGSHFTFCTR))
	mtc0	v0,C0_CTXT
#endif
#endif	/* R4000 || R10000 */

/* BEAST */

#if defined (BEAST)
	NOINTS( a3, C0_SR )			# paranoid
	DMTC0( zero, C0_ICACHE )		# init IASID
	DMTC0( zero, C0_TLBHI )
	DMTC0( zero, C0_COUNT )
	DMTC0( zero, C0_CAUSE )
	DMTC0( zero, C0_ECC )

	LI	v0, DEFAULT_PAGE_SIZE		# default tlb page size
	DMTC0( v0, C0_KPS)			# per set.
	DMTC0( v0, C0_UPS)
#endif /* BEAST */

	
#if EVEREST && TFP
	jal	init_interrupts
	nop
#endif
#if IP19
	/* On second-NMI we're supposed to enter POD mode.  But the IP19
	 * cpu proms assume that FP register 3 points to the epcuart.
	 * In order to make this work, we save the prom's FP register 3
	 * so that it can be reloaded on NMI.
	 */
	mfc0	v0, C0_SR
	li	v1, (SR_CU1|SR_FR)
	or	v1,v0
	mtc0	v1, C0_SR
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
	mtc0	v0, C0_SR
#endif		
	.set 	reorder
	LA	gp,_gp
	jal	mlsetup			# called mlsetup(argc, argv, environ)

	move	sp,v0
	li	v0,PDA_CURKERSTK
	sb	v0,VPDA_KSTACK(zero)	# running on kernel stack
	jal	main
	/* NOTREACHED */
	/*
	 * We return here from proc1 and
	 * should be set up and running as tlbpid 1.  The
	 * icode should already be copied into the user data space starting
	 * at USRDATA.  Initial register values don't matter, the GP won't
	 * be referenced.  The following should enter user mode for the first
	 * time.
	 */
EXPORT(proc1)
#if IP32
	# Set the spl subsystem to level 0
	# load CRIME interrupt mask register with mask for level 0
	sw	zero,VPDA_CURSPL(zero)
	ld	k1,VPDA_SPLMASKS(zero)
	li	v0,CRM_INTMASK|K1BASE
	sd	k1,0(v0)
#endif
	# Set the base mode to user, and turn on interrupt enable.
	# Raise the exception level, for 2 reasons: to retain
	# usability of cp0 and retain access to the kernel address
	# space. Note that EXL high also disables interrupts.
#if TFP
	LI	v0,SR_IMASK0|SR_KSU_USR|SR_IE|SR_EXL|SR_UXADDR|SR_DEFAULT
	and	v0,~SR_CU1		# make sure no FP
#if IP26
	.set	noreorder
	DMFC0(v1,C0_WORK1)		# get local SR_MASK value
	.set	reorder
	and	v0,v1
#endif
#endif
#if R4000 || R10000
	li	v0,SR_IMASK0|SR_KSU_USR|SR_IE|SR_EXL|SR_KADDR|SR_UXADDR
#endif
	.set 	noreorder
	MTC0(v0,C0_SR)
	NOP_1_4
	.set	reorder
	li	k1,USRCODE		# jump to first word of text
	sb	zero,VPDA_KSTACK(zero)	# switching to user stack
	.set	noreorder
	DMTC0(k1, C0_EPC)		# Set up for return to user.
	NOP_1_4
	eret
	.set	reorder
	END(start)

/*
 * create refs for symbols needed by final ld.
 */
	.extern sysent
	.extern biomove
	.extern uuid_create
	.extern setmask
	.extern reclock
	.extern ddi_lock
LEAF(mlclosure)
	lw	t0, sysent
	lw	t0, biomove
	lw	t0, uuid_create
	lw	t0, setmask
	lw	t0, reclock
	lw	t0, ddi_lock
	END(mlclosure)

	.data
EXPORT(_quiet)
	.asciiz	"quiet"

/*
 * Local storage for arguments
 */
BSS(_argc, 4)		/* XXX -- back to LBSS when cmplr works */

#if (_MIPS_SZPTR == 32)
BSS(_argv, 4)		/* XXX -- back to LBSS when cmplr works */
BSS(_envirn, 4)		/* XXX -- back to LBSS when cmplr works */
#endif
#if (_MIPS_SZPTR == 64)
BSS(_argv, 8)		/* XXX -- back to LBSS when cmplr works */
BSS(_envirn, 8)		/* XXX -- back to LBSS when cmplr works */
#endif

	.sdata
EXPORT(dbgmon_loadstop)
	.word	0
	.text

/*
   Turn off all the audio on the system.  This routine is called from
   icmn_err during CE_PANIC processing to quiet the audio system before
   falling into the debugger
*/

LEAF(silence_all_audio)
#if IP22 || IP26 || IP28
	/* nuke ALL the DMA channels on the hpc3...parallel port,
	 * and all audio.
	 */
	CLI	t0,PHYS_TO_COMPATK1(HPC3_PBUS_CONTROL_BASE) # pbus control base
	li	t1,0x20				# ch_act = 0/ch_act_ld = 1
	li	t2,7				# # of pbus channels (0-7)
1:	sw	t1,0(t0)			# reset control
	PTR_ADD	t0,HPC3_PBUS_CONTROL_OFFSET	# get next address
	addi	t2,-1
	bgez	t2,1b
#endif
XLEAF(silence_bell)
#if IP22 || IP26 || IP28
	/* turn off keyboard bell if EIU present.
	 */
#ifdef IP22	/* Indy does not have this bell */
	lw	t0,bellok
	beq	t0,zero,1f
#endif
	CLI	t0,PHYS_TO_COMPATK1(NMIStatus)
	lb	t1,0(t0)
	andi	t1,t1,(~EISA_SPK_ENB)&0xff
	sb	t1,0(t0)
1:
#endif
#if IP30
	/* turn off the keyboard bell for octane lx */
	lw	t0,is_octane_lx
	beq	t0,zero,1f
	LI	t0,IOC3_PCI_DEVIO_K1PTR+IOC3_GPCR_C
	li	t1,GPCR_INT_OUT_EN
	sw	t1,0(t0)

1:
	j	ip30_rad_halt
#endif
        j       ra
        END(silence_all_audio)

#if EVEREST && TFP
LEAF(init_interrupts)
	.set	noreorder
#if TFP_ADE_EBUS_WBACK_WAR
	/*
	 * The logic in the CC that reports address errors on EBus
	 * writeback channel is broken. Turn off the bit in the diag
	 * register. Note that the sense of the bits is reversed from
	 * the spec (only when we write the register)!
	 * For now, just slam a hard coded constant in there. If we
	 * ever need to touch the register for some other reason,
	 * we'll add #define's.
	 * The ip21prom writes the same value, but we do it here in case
	 * the system has old proms.
	 */
	LI	v0, EV_DIAGREG
	LI	v1, 0x40b00000		# disable error bit 4 and
	sd	v1, 0(v0)		# change "in queue almost full" (b)
#endif
	LI	v0, EV_CIPL0
	li	v1, 127			# Highest interrupt number
clear_ints:
	sd	v1, 0(v0)		# Clear interrupt (BD)
	bne	v1, zero, clear_ints
	addi	v1, -1			# Decrement counter (BD)

	j	ra
	nop
	.set	reorder
	END(init_interrupts)
#endif	/* TFP */

#if defined(EVEREST) && defined(MULTIKERNEL)
NESTED(startothercell, STARTFRM, zero)
	/*
	 * cell initialization
	 *
	 * a0 => MPCONF structure for this cpu
	 */
	LA	gp,_gp			# don't forget gp!

	.set noreorder
	/* We extract the cellid from the cpu_virtid for the current cpu */

	/* a0 => MPCONF
	 * Load cpu_virtid from MPCONF structure then apply the cellmask
	 * to determine which cell we're in.
	 */
	lb	a1, MP_VIRTID(a0)
	
	LI	a2, ECFGINFO_ADDR+ECFG_CELLMASK
	lb	a2, (a2)
	and	a1, a2			/* a1 = cellid */

	/* now determine the memory base address of this cell */
	
	LI	t0, ECFGINFO_ADDR+ECFG_CELL
1:	
	beq	a1, zero, 2f
	lw	a0, ECFG_CELL_MEMBASE(t0) /* BDSLOT - base address in BLOCs */
	addi	a1, -1
	b	1b
	PTR_ADDI t0, SIZEOF_CELLINFO	/* BDSLOT - goto next cellinfo */
2:
	/* a0 = BLOCs of membase for the cell corresponding to this cpu */
	
	PTR_SLL	a0, 8			/* physical byte address of cell 1 */
	move	s7, a0			/* s7 = physical address of cell 1 */	
	move	a1, a0			/* read-write data NASID */
	jal	mapped_kernel_setup_tlb	/* setup kernel text & data TLB */
	nada
		
#if _MIPS_SIM == _ABI64
	/* Make sure the KX bit is on. */
	MFC0(k0,C0_SR)
	LI	k1, SR_KERN_SET
	or	k0, k1
	MTC0(k0,C0_SR)
	NOP_1_4
#endif /* _ABI64 */
	
	LA	t0, 1f			# Load the link address for label 1f
	jr	t0			# Jump to our link (mapped) address
	nop
1:
	nop
	.set reorder
	
	PTR_L	sp,eintstack		# make sure stack set up
#if IP19
	jal	ip19_cache_init
#endif
	LA	gp,_gp			# bug in symmon? need to do this again!
	
	PTR_L	sp,eintstack		# make sure stack set up

	/* We need to "wire" a temporary PDA into the tlb so that we can execute
	 * enough of the exception code to enter the debugger. Not a problem
	 * for the "golden cell" since symmon intercepts all exceptions until
	 * the kernel "hooks" the exceptions.  But for slave cells the exceptions
	 * have already been "hooked" by the golden cell and we need a pda.
	 */
	
	/* first build even-page/odd-page pdes in a3/a4 */
	LA	a4, evmk_temp_pda
	PTR_ADD	a4, s7			/* relocate into this cell's memory */
	PTR_ADDI a4, 16384		/* map the "odd" page (avoid VCEs) */
	and	a4, 0x7ffff000
	PTR_SRL	a4, PNUMSHFT-PTE_PFNSHFT
	or	a4, PG_M|PG_G|PG_VR|PG_COHRNT_EXLWR
	li	a3, PG_G
	
	li	a0, PDATLBINDEX
	move	a1, zero		/* tlbpid */
	LI	a2, PDAPAGE

	LI	a5, TLBPGMASK_MASK
	jal	tlbwired

	/* We now have a PDA "wired" so that we can take exceptions.
	 * Now setup the debugger's exception handler as the default
	 * handler until rest of initialization is complete (where we
	 * would normally "hook" exceptions).
	 */
	
	.set	noat
	LI	AT, SPB_DEBUGADDR
	PTR_L	AT, 0(AT)
	beq	AT, zero, 1f
	PTR_L	AT, DB_EXCOFF(AT)
	beq	AT, zero, 1f
	PTR_S	AT, VPDA_EXCNORM

	/* Now check if we should enter symmon before rest of initialization */
	
	lw	v0,dbgmon_loadstop	# check for stopping if dbgmon
	beq	zero,v0,1f		# really loaded
	
	LA	a0,_ring
	jal	debug
	.set	at
1:
	REG_S	zero,RAOFF(sp)		# zero old ra for debuggers
	REG_S	zero,FPOFF(sp)		# zero old fp for debuggers (???)
	.set 	noreorder
	MTC0(zero, C0_CAUSE)
	DMTC0_NO_HAZ(zero,C0_TLBHI)
	NOINTS(a3,C0_SR)		# paranoid

	mtc0	zero,C0_WATCHLO		# No watch point exceptions.
	mtc0	zero,C0_WATCHHI

	li	a3,TLBPGMASK_MASK
	mtc0	a3,C0_PGMASK
	li	a3,NWIREDENTRIES
	mtc0	a3,C0_TLBWIRED		# Number of wired tlb entries
#if _MIPS_SIM == _ABI64
	# pairs of 4-byte ptes
	LI	v0,KPTEBASE<<(KPTE_TLBPRESHIFT+PGSHFTFCTR)
#if R10000
	DMTC0(v0,C0_EXTCTXT)
	LI	v0,FRAMEMASK_MASK
	DMTC0(v0,C0_FMMASK)
#else
	DMTC0(v0,C0_CTXT)
#endif /* R10000 */
#else
	# pairs of 4-byte ptes (use dli so ragnarok assembler doesn't complain)
	dli	v0,-((-KPTEBASE)<<(KPTE_TLBPRESHIFT+PGSHFTFCTR))
	mtc0	v0,C0_CTXT
#endif
	.set 	reorder
	LA	gp,_gp
	
	/* slave cells have no need to an environment.  The golden cell has
	 * already replicated all kernel arguments during its' getargs()
	 * processing.
	 */
	move	a0, zero
	move	a1, zero
	move	a2, zero
	jal	mlsetup			# called mlsetup(argc, argv, environ)

	move	sp,v0
	li	v0,PDA_CURKERSTK
	sb	v0,VPDA_KSTACK(zero)	# running on kernel stack
	jal	main
	/* NOTREACHED */
	END(startothercell)
#endif /* EVEREST && MULTIKERNEL */	
/*
 * Bootstrap program executed in user mode
 * to bring up the system.
 * Note: we use "sw" below since init runs as a 32 bit user program and
 * needs to set up its arguments using 32 bit instructions so that exec
 * can pick them up properly.
 */
	AUTO_CACHE_BARRIERS_DISABLE	# icode is user code
	.align	3			# needed for 64 bit PTR_S below
LEAF(icode)
	LA	a0,icode_file
	LA	v0,idata		# relocate for user space
	PTR_SUBU a0,v0
	PTR_ADDU a0,USRDATA
	LA	a1,icode_argv
	LA	v0,idata		# relocate for user space
	PTR_SUBU a1,v0
	PTR_ADDU a1,USRDATA
	sw	a0,0(a1)		# relocate vector in d space
	move	a2,zero
	li	v0,SYS_exece
	syscall
	move	a0,v0			# panic string gets 'what=errno'
	li	v0,SYS_exit
	syscall
1:	b	1b
EXPORT(eicode)
	END(icode)
	AUTO_CACHE_BARRIERS_ENABLE

	.align	4
	.data
EXPORT(idata)
EXPORT(icode_argv)
	PTR_WORD	0
	.space	10*4			# leave space for boot args

EXPORT(icode_file)
	.asciiz	"/etc/init"
	.space	32
argp:					# leave space for boot args
	.space	64

	.align	4

EXPORT(icode_args)
	PTR_WORD	argp

EXPORT(icode_argc)
	.word	1

EXPORT(eidata)
#if defined(EVEREST) && defined(MULTIKERNEL)
	/* page aligned page to be used as pseudo PDA for master cpu in
	 * secondary cell since exceptions have already been "hooked"
	 * to kernel exception handler.
	 * NOTE: align to a 32KB boundary so we can map the "odd" page
	 * into the temporaru PDA so we avoid potential problems with VCEs.
	 */
EXPORT(evmk_temp_pda)	
	.align	15
	.space	32768
EXPORT(_ring)
	.asciiz	"ring"
#endif /* EVEREST && MULTIKERNEL */
