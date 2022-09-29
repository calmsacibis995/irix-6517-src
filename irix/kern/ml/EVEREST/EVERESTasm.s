/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * Everest specific assembly routines
 */

#ident	"$Revision: 1.172 $"

#include <ml/ml.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/gda.h>
#include <sys/EVEREST/evmp.h>
#include <sys/loaddrs.h>
#include <sys/dump.h>
#ifdef MAPPED_KERNEL
#include <sys/mapped_kernel.h>
#endif /* MAPPED_KERNEL */

/* stubs that just return */
LEAF(dummy_func)
XLEAF(apsfail)
XLEAF(sys_setled)
XLEAF(dcache_wb)		/* Everest has coherent I/O - NOP */
XLEAF(dcache_wbinval)
XLEAF(dki_dcache_wb)
XLEAF(dki_dcache_wbinval)
XLEAF(dki_dcache_inval)
	j ra
END(dummy_func)

/* stubs that return 0 */
LEAF(dummyret0_func)
	move	v0,zero
	j ra
END(dummyret0_func)

/* stubs that return 1 */
LEAF(dummyret1_func)
	li	v0, 1
	j ra
END(dummyret1_func)


#ifdef SPLMETER
/* atomic store if greater-than.
 * store a1 in 0(a0) if a1 is greater than current value
 */
LEAF(store_if_greater)
1:
	move	t0,a1
	ll	v0,0(a0)
	slt	v1,v0,a1
	beq	v1,zero,2f
	sc	t0,0(a0)
#ifdef R10K_LLSC_WAR
	.set	noreorder
	beql	t0,zero,1b
	nop
	.set	reorder
#else
	beq	t0,zero,1b
#endif
2:
	j	ra
END(store_if_greater)
#endif

/*
 * all slave processors come here - assumed on boot stack
 */
BOOTFRM=	FRAMESZ((NARGSAVE+2)*SZREG)	# arg save + 5th/6th args to tlbwired
NESTED(bootstrap, BOOTFRM, zero)
	/* do some one-time only initialization */
	PTR_SUBU sp,BOOTFRM
	.set	noreorder
	li	t0,SR_KADDR
	MTC0(t0,C0_SR)
#ifdef MAPPED_KERNEL
	move	s0, a0			# Save a0 value

	/* We extract the cellid from the cpu_virtid for the current cpu */
	
	lbu	a1, MP_VIRTID(s0)	/* a1 = cpu_virtid from MPCONF */
	LI	t0, ECFGINFO_ADDR+ECFG_CELLMASK
	lbu	t0, (t0)		/* load cellmask for this cpu */
	and	a1, t0			/* a1 = cell id for this cpu */
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
	move	a1, a0			/* read-write data NASID */
	jal	mapped_kernel_setup_tlb	/* setup kernel text & data TLB */
	nada

	LA	t0, 1f			/* load link address for label "1f" */
	jr	t0			/* jump to our mapped address */
	nada
1:		
	move	a0, s0			# Restore a0 value
#endif /* MAPPED_KERNEL */	

	/*
	 * Some of the calls below (size_2nd_cache on TFP for example) end up
	 * calling C code, so we need to init gp here.
	 */
	LA	gp,_gp

#if IP19 || IP25

	/* a0 - poiner to mpconf entry */
	
	lw	v0,MP_SCACHESZ(a0)	# Pick up secondary cache size
	li	s0,1
	sll	s0,v0			# 2 ^^ MPCACHESZ

#else /* IP19 */

	# Need to run size_2nd_cache in uncached space
	# except for IP21 which can't fetch uncached instructions
	LA	v0, 1f
#if !TFP
	KPHYSTOK1(v0);
#endif
	j	v0
	nop
1:	jal	size_2nd_cache
	nop
	LA	t0, 1f
	j	t0
	nop
	.set	reorder
1:
	# back to cached space.
	move	s0, v0
#endif /* IP19 */

#if R4000
	.set	noreorder
#if !IP19
	LI	a0, K0BASE
	move	a1, s0
	jal	rmi_cacheflush
	nop
#endif
	mtc0	zero,C0_WATCHLO		# No watch point exceptions.
	nop
	mtc0	zero,C0_WATCHHI
	li	a3,TLBPGMASK_MASK
	mtc0	a3,C0_PGMASK		# Set up pgmask register
	li	a3,NWIREDENTRIES
	mtc0	a3,C0_TLBWIRED		# Number of wired tlb entries
#if _MIPS_SIM == _ABI64
	# pairs of 4-byte ptes
	LI	v0,KPTEBASE<<(KPTE_TLBPRESHIFT+PGSHFTFCTR)
	DMTC0(v0,C0_CTXT)
#else
	# pairs of 4-byte ptes (use dli so ragnarok assembler doesn't complain)
	dli	v0,-((-KPTEBASE)<<(KPTE_TLBPRESHIFT+PGSHFTFCTR))
	mtc0	v0,C0_CTXT
#endif
	NOP_0_4
	.set    reorder
#endif
#if R10000
	.set	noreorder
#if	0
	LI	a0, K0BASE
	move	a1, s0
	jal	rmi_cacheflush
	nop
#endif
	mtc0	zero,C0_WATCHLO		# No watch point exceptions.
	nop
	mtc0	zero,C0_WATCHHI
	li	a3,TLBPGMASK_MASK
	mtc0	a3,C0_PGMASK		# Set up pgmask register
	li	a3,NWIREDENTRIES
	mtc0	a3,C0_TLBWIRED		# Number of wired tlb entries
#if _MIPS_SIM == _ABI64
	# pairs of 4-byte ptes
	LI	v0,KPTEBASE<<(KPTE_TLBPRESHIFT+PGSHFTFCTR)
	DMTC0(v0,C0_EXTCTXT)
#else
	# pairs of 4-byte ptes (use dli so ragnarok assembler doesn't complain)
	dli	v0,-((-KPTEBASE)<<(KPTE_TLBPRESHIFT+PGSHFTFCTR))
	mtc0	v0,C0_CTXT
#endif
	LI	v0,FRAMEMASK_MASK
	DMTC0(v0,C0_FMMASK)
	.set    reorder
#endif 
	.set	noreorder
	DMTC0(zero,C0_TLBHI)
#if TFP
	DMFC0(k0, C0_CONFIG)
	and	k0, ~CONFIG_ICE		# Turn off "Inhibit Count during Exception" bit
	DMTC0(k0, C0_CONFIG)

	/* Initialize wired register so HW can check valid bits on first
	 * ktlbmiss (RTL simulator didn't like uninitialized values --
	 * not a bad idea to init it anyway).
	 */
	DMTC0( zero, C0_WIRED )
	DMTC0( zero, C0_COUNT )
	DMTC0(zero,C0_CAUSE)	/* clear potential left-over VCI/BE */
	LI	k0,EV_CERTOIP
	li	k1,0x3fff	/* clear pending timeouts */
	sd	k1,0(k0)
	LI	k0,SR_DEFAULT
	DMTC0(k0,C0_SR)		/* set pg size, clear EXL so ktlbmiss works */
	NOINTS(a3,C0_SR)		# paranoid
	DMTC0(zero,C0_ICACHE)		# init IASID
	/* Here we load a value into C0_WIRED which will allow us to wire
	 * the PDA and kernel stack.  Assumes that they are all
	 * in distinct congruence classes.
	 */
	LI	a3,(((PDAPAGE>>PNUMSHFT)&WIRED_MASK)|WIRED_VMASK)<<WIRED_SHIFT0
	LI	v0,((((KSTACKPAGE)>>PNUMSHFT)&WIRED_MASK)|WIRED_VMASK)<<WIRED_SHIFT2
	or	a3,v0
	DMTC0(a3,C0_WIRED)

	LI	a3,KPTEBASE
	DMTC0(a3,C0_KPTEBASE)		# For quick reference by utlbmiss

	LI	a3,EV_CEL
	DMTC0(a3,C0_CEL)		# For quick reference by exception code

	jal	init_interrupts
	nop
#endif	/* TFP */

	.set	reorder
	/*
	 * Set up pda as wired entry
	 * wirepda(pdaindr[id].pda);
	 */
	jal	getcpuid		# grab cpuid from hardware 
	li	v1,PDAINDRSZ
	mul	v0,v0,v1
	LA	v1,pdaindr
	PTR_ADDU v0,v0,v1
	PTR_L	a0,PDAOFF(v0)		# fetch addr of pda
					# (set up in initmasterpda)
	# mask and shift pfn suitably.
#if _MIPS_SIM == _ABI64
	and	a0,0x7ffff000		# mask PFN suitably
#else
	and	a0,0x1ffff000		# mask and shift PFN into place
#endif	/* _ABI64 */

	srl	a0,PNUMSHFT - PTE_PFNSHFT	# align for proper pde
	or	a1,a0,PG_M|PG_G|PG_VR|PG_COHRNT_EXLWR
	li	a3,PG_G
	move	s1,a1			# save for later
#if TFP
	move	a3,a1			# only one pte
#if defined(_COMPILER_VERSION) && (_COMPILER_VERSION>620) && !defined(PTE_64BIT)
        /*
	 * For earlier revisions of ragnarok, PTEs are passed in
	 * low half of register instead of high half where they
	 * belong -- in newer compilers, we must move them there:
         */
        dsll    a3,a3,32                # 4th arg - 1st pte
#endif
#else
#if (_MIPS_SIM == _ABIO32)
	sw	a1,16(sp)		# even pte for tlbwired
	li	t0, TLBPGMASK_MASK
	sw	t0,20(sp)		# size mask
#else
	move	a4,a1
	li	a5, TLBPGMASK_MASK
#if defined(_COMPILER_VERSION) && (_COMPILER_VERSION>620) && !defined(PTE_64BIT)
        /*
	 * For earlier revisions of ragnarok, PTEs are passed in
	 * low half of register instead of high half where they
	 * belong -- in newer compilers, we must move them there:
         */
        dsll    a3,a3,32                # 4th arg - 1st pte
        dsll    a4,a4,32                # 5th arg - 1st pte
#endif
#endif  /* ABI32 */
#endif	/* TFP */

	move	a0,zero
	move	a1,zero
	LI	a2,PDAPAGE
	jal	tlbwired

	# In PTE_64BIT Mode, pte sizes are 64 bits!!. 
	# So, use PTE_S instead of INT_S, which scales properly.
	# PTE_S is defined in ml/ml.h
	PTE_S	s1,VPDA_PDALO(zero)	# for later

	li	v0,PDA_CURIDLSTK
	sb	v0,VPDA_KSTACK(zero)	# running on boot/idle stack
	PTR_L	sp,VPDA_LBOOTSTACK(zero)

	# store secondary cache size in pda
	sw	s0, VPDA_PSCACHESIZE(zero)
	j	cboot
	END(bootstrap)

/*
 * Routine to set count and compare system coprocessor registers,
 * used for the scheduling clock.
 *
 * We mess around with the count register, so it can't be used for
 * other timing purposes.
 */
LEAF(resetcounter)
	.set	noreorder
#if R4000 || R10000
	mfc0	t1,C0_SR
	NOP_0_4
	li	t0,SR_KADDR
	mtc0	t0,C0_SR		# disable ints while we muck around
	NOP_0_4
	mfc0	t0,C0_COUNT
	mfc0	t2,C0_COMPARE
	li	t3,1000			# "too close for comfort" value
	NOP_0_3				# (be safe)
	sltu	ta1,t0,t2		# if COUNT < COMPARE
	bne	ta1,zero,1f		#  then COUNT has wrapped around
	subu	ta0,t0,t2		#   and we're way way past!
	sltu	ta1,a0,ta0		# if (COUNT-COMPARE)
	bne	ta1,zero,1f		#  exceeds interval, then way past!
	subu	a0,ta0			# decrement overage from next interval
	sltu	ta1,a0,t3		# if next COMPARE
	beq	ta1,zero,2f		#  is really soon, then...
	nop
1:
	move	a0,t3			# go with an "immediate" interrupt
2:
	mtc0	zero,C0_COUNT		# finally, start COUNT back at zero,
	NOP_0_4				#  (be safe)
	mtc0	a0,C0_COMPARE		# and set up COMPARE for next interval
	NOP_0_4				#  (be safe)
	mtc0	t1,C0_SR		# reenable ints (don't need NOPs after)
#endif

#if TFP
#if SABLE_FASTCLKINT
	/* Run clock at faster frequency (i.e. time is accelerated)
	 */
	dsrl	a0,4			# run clock at 16 times "real-time"
#else	/* !SABLE_FASTCLKINT */
#if SABLE_RTL
	/* Normal mode for RTL simulation is NO clock interrupts
 	 */
	MTC0_AND_JR(zero, C0_COUNT, ra)
#endif	/* SABLE_RTL */
#endif	/* !SABLE_FASTCLK */	
	MFC0(t1, C0_SR)
	ori	t2,t1,SR_IE
	xori	t2,SR_IE		# IE bit now off
	MTC0(t2, C0_SR)			# disable ints while we muck around
	DMFC0(t0, C0_COUNT)
	dsll	t3,t0,32
	bgez	t3,3f	 		# if no interrupt pending, then init
	dsubu	t0,a0			# t0 = current time - # cycles
	DMTC0(t0, C0_COUNT)
	MTC0(t1,C0_SR)			# reenable ints (don't need NOPs after)
	j	ra
	nop

	/* initialize the counter, assume the time to be set is <= 31 bits
	 */
3:
	dli	t0,0x080000000
	dsubu	t0,a0
	DMTC0(t0, C0_COUNT)
	MTC0(t1,C0_SR)		# reenable ints (don't need NOPs after)
	j	ra
	nop
#endif	/* TFP */
	j	ra
	nop
	.set	reorder
	END(resetcounter)

#if SABLE
/* speedup_counter
 * Intended to be called from idle() on SABLE in order to cause the next
 * clock tick more quickly.
 */
LEAF(speedup_counter)
	.set	noreorder
	dli	t0,0x07ffffff0	# 16 cycles until next clock interrupt
	DMTC0(t0, C0_COUNT)
	j	ra
	nop
	.set	reorder
	END(speedup_counter)
#endif /* SABLE */

LEAF(delaycounter)
	.set	noreorder
#if R4000 || R10000	
	mfc0	t1,C0_SR
	NOP_0_4
	li	t0,SR_KADDR	
	mtc0	t0,C0_SR		# disable ints while we muck around
	NOP_0_4
	mtc0	zero,C0_COUNT		# start COUNT back at zero,
	NOP_0_4
	li 	t0,0xF0000000
	mtc0	t0,C0_COMPARE		# and set up COMPARE for long interval
	NOP_0_4				# (be safe)
	mtc0	t1,C0_SR		# reenable ints (don't need NOPs after)
#endif
#if TFP
	DMTC0(zero, C0_COUNT)		# make the count 0 for longest delay
#endif /* TFP */
	j	ra
	nop
	.set	reorder
	END(delaycounter)

LEAF(initcounter)
        .set    noreorder
#if R4000 || R10000	
        mfc0    t1,C0_SR
        NOP_0_4
        li      t0,SR_KADDR
        mtc0    t0,C0_SR                # disable ints while we muck around
        NOP_0_4
	mtc0    zero,C0_COUNT           # start COUNT back at zero,
        NOP_0_4                         # (be safe)
        mtc0    a0,C0_COMPARE           # set up COMPARE for next interval
        NOP_0_4                         # (be safe)
        mtc0    t1,C0_SR                # reenable ints (don't need NOPs after)
#endif	
#if TFP
	DMTC0(a0, C0_COUNT)		# set count
#endif				
	j	ra
	nop
        .set    reorder
        END(initcounter)
			
/*
 * Function:	get_mpconf
 * Purpose:	To recover the mpconf structure for this CPU.
 * Returns:	Pointer to MPconf for this CPU or 0.
 * Notes:	This FUnction ONLY KILLS T registers.
 */
LEAF(get_mpconf)
	.set	noreorder
	
	LI	t0,EV_SPNUM
	ld	t0,0(t0)
	and	t0,EV_SPNUM_MASK
	
	LI	t2,MPCONF_SIZE*EV_MAX_CPUS+MPCONF_ADDR
	LI	t1,MPCONF_ADDR

	lui	v0,(MPCONF_MAGIC >> 16); 
	ori	v0,(MPCONF_MAGIC & 0xffff)

1:
	lw	v1,MP_MAGICOFF(t1)	/* Magic # */
	bne	v1,v0,2f
	nop
	lbu	v1,MP_PHYSID(t1)
	beq	v1,t0,3f
	nop
2:	
	PTR_ADD	t1,MPCONF_SIZE
	bltu	t1,t2,1b
	nop
	move	t1,zero			/* Not found */
3:
	move	v0,t1
	j	ra
	nop

	.set	reorder
	END(get_mpconf)

/*
 * Doubleword Shift by more than 32 is broken in the current version
 * of R4000.  It's expected to be fixed in the R4000A.
 */
#if R4000
#define DS32_WAR 1
#endif

/*
 * long long load_double(long long *)
 *
 * Implicitly assumes that address is doubleword aligned!
 *
 * On entry:
 *	a0 -- the address from which to load double
 */
LEAF(load_double)
#if TFP_CC_REGISTER_READ_WAR
	dli	t0, EV_SYSCONFIG	# sysconfig register special
	beq	a0, t0, do_config_rd
	dli	t0, EV_CONFIGREG_BASE	# now check general range
	dsubu	t0, a0, t0
	bltz	t0, 9f
	dsubu	t0, 0x08000
	bgez	t0, 9f

	/* we have a config read  (from physical addr 18008000 to 1800ffff)
	 * Disable interrupts, then delay in order to allow any pending
	 * writebacks to complete (and we must insure that no additional
	 * writebacks are initiated).  Then read the config register until
	 * we get the same data twice.
	 */
do_config_rd:
	.set	noreorder
	.align	7
	DMFC0(t1, C0_SR)
	ori	t0, t1, SR_IE
	xori	t0, SR_IE		# disable interrupts
	DMTC0(t0, C0_SR)

1:	li	t3, 100		# delay for writeback
2:	daddi	t3, -1
	bgez	t3, 2b
	NOP_SSNOP

	ld	v0, 0(a0)		# do first config read
	dli	t2, EV_RTC

	li	t3, 100
2:	daddi	t3, -1			# delay for writeback
	bgez	t3, 2b
	NOP_SSNOP

	ld	t2, (t2)		# get some other data into DB

	li	t3, 100
2:	daddi	t3, -1			# delay for writeback
	bgez	t3, 2b
	NOP_SSNOP

	ld	t0, 0(a0)		# second config read

	li	t3, 100
2:	daddi	t3, -1			# delay for writeback
	bgez	t3, 2b
	NOP_SSNOP

	beq	t0, t2, 1b		# make sure RTC <> config data
	nop
	bne	v0, t0, 1b		# and make sure bit config reads same
	nop

	DMTC0(t1, C0_SR)		# restore SR value
	.set	reorder
	j	ra

9:

#endif /* TFP_CC_REGISTER_READ_WAR */

	ld	v0,0(a0)
#if (_MIPS_SIM == _ABIO32)
	move	v1,v0
#if DS32_WAR
	dsra	v0,16
	dsra	v0,16

	# sign extend v1
	dsll	v1,16
	dsll	v1,16
	dsra	v1,16
	dsra	v1,16
#else
	dsra	v0,32

	# sign extend v1
	dsll	v1,32
	dsra	v1,32
#endif /* DS32_WAR */
#endif /* (_MIPS_SIM == _ABIO32) */
	j	ra
	END(load_double)

LEAF(load_double_nowar)
	ld	v0,0(a0)
	j	ra
	END(load_double_nowar)
	
#if	IP25
	.align	7
LEAF(load_double_scc)
	/*
	 * SCC bug can not handle a block read being issued while a
	 * config read/sense config read are outstanding. To avoid this
	 * we cause the next secondary cache line to be in the cache
	 * by accessing it - otherwise, the R10K "may" issue a block read
	 * for it while the config read is outstanding.
	 *
	 * With Rev "B" SCC's or greater, this is not a problem EXCEPT for
	 * sense config reads.
	 */
	.set	noreorder
	LA	v0,1f
	lw	zero, CACHE_SLINE_SIZE(v0)
	lw	zero, 0(ra)
	DMFC0(zero, C0_SR)
1:	ld	v0,0(a0)
	DMFC0(zero, C0_SR)
	j	ra
	nop
	.set	reorder
	END(load_double_scc)
	.align	7
#endif

/*
 * void store_double(long long *, long long)
 *
 * On entry: 
 *	a0 -- the address into which to store double.
 *	a2/a3 -- the 8 byte value to store.
 */
#if _MIPS_SIM != _ABI64		/* macro is 64 bit mode */
LEAF(store_double)
#if (_MIPS_SIM == _ABIO32)
#if DS32_WAR
	dsll	a2,16
	dsll	a2,16
	dsll	a3,16
	dsll	a3,16
	dsrl	a3,16
	dsrl	a3,16
#else
	dsll	a2,32
	dsll	a3,32		# strip off high 32 bits
	dsrl	a3,32
#endif /* DS32_WAR */
	or	a2,a3
	sd	a2,0(a0)
#else
	sd	a1,0(a0)
#endif
	j	ra
	END(store_double)
#endif

/*
 * void store_half(short *, short)
 *
 * On entry: 
 *	a0 -- the address into which to store half.
 *	a1 -- the 2 byte value to store.
 */
LEAF(store_half)
	sh	a1, 0(a0)
	j	ra
	END(store_half)

LEAF(get_config)
	.set	noreorder
	DMFC0(v0, C0_CONFIG)
	NOP_0_4
	.set	reorder
	j	ra
	END(get_config)

#if !TFP
LEAF(_get_compare)
	/* TBD -- what to do for TFP */
	.set	noreorder
	mfc0	v0, C0_COMPARE
	NOP_0_4
	.set	reorder
	j	ra
	END(_get_compare)
#endif /* !TFP */

LEAF(_get_count)
	.set	noreorder
#if TFP
	DMFC0(v0, C0_COUNT)
#else
	mfc0	v0, C0_COUNT		/* must read R4000 COUNT with mfc0 */
#endif
	NOP_0_4
	.set	reorder
	j	ra
	END(_get_count)

LEAF(_set_count)
	.set	noreorder
	DMTC0(a0, C0_COUNT)
	.set	reorder
	j	ra
	END(_set_count)

LEAF(load_double_lo)
	.set	noreorder
	ld	v0,0(a0)
	j	ra
	nop
	.set	reorder
	END(load_double_lo)

LEAF(load_double_hi)
	.set	noreorder
	ld	v0,0(a0)
	dsrl	v0,16
	dsrl	v0,16
	j	ra
	nop
	.set	reorder
	END(load_double_hi)

#if TFP
LEAF(get_trapbase)
	.set	noreorder
	DMFC0(v0,C0_TRAPBASE)
	.set	reorder
	j	ra
	END(get_trapbase)

LEAF(set_trapbase)
	.set	noreorder
	DMTC0(a0,C0_TRAPBASE)
	.set	reorder
	j	ra
	END(set_trapbase)
#endif /* TFP */

#if R4000 || R10000
#define	NMI_ERREPC	0
#endif
#if TFP
#define	NMI_CAUSE	0
#endif
#define	NMI_EPC		8
#define	NMI_SP		16
#define	NMI_RA		24
#define	NMI_SAVE_REGS	4

#define	NMI_SAVESZ_SHFT	5

LEAF(nmi_dump)
	.set noreorder

	LI	k0,SR_KADDR|SR_DEFAULT	# make sure page size bits
	MTC0(k0, C0_SR)			# are on
	nop
	nop
	nop
	nop
	nop
	nop
	nop

	LI	k0, GDA_ADDR		# Load address of global data area
	lw	k0, G_MASTEROFF(k0)	# Get the maste processor's SPNUM
	LI	k1, EV_SPNUM		# Load the SPNUM resource addr
	ld	k1, 0(k1)		# Read our spnum
					# (LD)
	andi	k1, EV_SPNUM_MASK
	bne	k0, k1, 1f		# Skip forward if we're not master
	nop				# (BD)

	# Save Registers for Master cpu at offset 0 in nmi_saveregs.
	# Use uncached address.
	
	LA	k0, nmi_saveregs
	LI	k1, TO_PHYS_MASK
	and	k0, k1
	LI	k1, K1BASE
	or	k0, k1			# store into uncached addresses
#if R4000 || R10000
	DMFC0(k1, C0_ERROR_EPC)
	nop
	sd	k1, NMI_ERREPC(k0)
	nop
#endif
#if TFP
	DMFC0(k1, C0_CAUSE)
	nop
	sd	k1, NMI_CAUSE(k0)
	nop
#endif
	DMFC0(k1, C0_EPC)
	nop
	sd	k1, NMI_EPC(k0)
	nop
	sd	sp, NMI_SP(k0)
	nop
	sd	ra, NMI_RA(k0)
	nop
#if IP19	
	/* Need to restore EPCUART pointer into FP register 3 for POD */
	
	LI	k0, SR_KADDR|SR_DEFAULT|SR_CU1|SR_FR
	mtc0	k0, C0_SR
	nop
	nop
	nop
	nop
	ld	k1, pod_nmi_fp3
	mtc1	k1, $f3
	nop
	nop
	nop
	nop

	LI	k0,SR_KADDR|SR_DEFAULT	# make sure page size bits
	MTC0(k0, C0_SR)			# are on
	nop
	nop
	nop
	nop
	nop
	
	/* FP3 restored */
#endif /* IP19 */	
	
	LA	sp, dumpstack
	LI	gp, DUMP_STACK_SIZE - 16
	PTR_ADD	sp, gp			# Set our stack pointer.
	LA	gp, _gp
	j	cont_nmi_dump
	nop				# (BD)

1:
	# k1 now has the contents of EV_SPNUM
	# This code assumes logical_cpuid is defined as
	# cpuid_t logical_cpuid[EV_MAX_SLOTS][EV_CPU_PER_BOARD];
	# in init.c
	# Since EV_SPNUM = ( slot << 2) + cpunum
	# EV_SPNUM itself could be used as an index into this array
	# to get cpuid.
	.set	noat
	LA	k0, logical_cpuid
#if TFP
	PTR_SRL	k1,1			# 2 slices per board for tfp
#endif
	PTR_ADDU k0, k1
	lb	k1, 0(k0)		# k1 now has logical cpuid
	PTR_SLL	k1, NMI_SAVESZ_SHFT	# Get right offset into k1
	LA	k0, nmi_saveregs	# Starting address of save array
	PTR_ADDU k0, k1			# k0 now has proper starting address
	LI	k1, TO_PHYS_MASK
	and	k0, k1
	LI	k1, K1BASE
	or	k0, k1			# store into uncached addresses

#if R4000 || R10000
	DMFC0(k1, C0_ERROR_EPC)
	nop
	sd	k1, NMI_ERREPC(k0)
	nop
#endif
#if TFP
	DMFC0(k1, C0_CAUSE)
	nop
	sd	k1, NMI_CAUSE(k0)
	nop
#endif
	DMFC0(k1, C0_EPC)
	nop
	sd	k1, NMI_EPC(k0)
	nop
	sd	sp, NMI_SP(k0)
	nop
	sd	ra, NMI_RA(k0)
	nop
2:
	b	2b			# Loop forever.
	nop

	END(nmi_dump)



/*
 * io4_doflush_cache
 * a0: address of the IO4_CONF_CACHETAG0L register
 *     	Cache tags are a pair (CACHETAG0L giving physical bits 38:7  
 *	and CACHETAG0U gives bit 39 of physical address.
 *	In addition CACHETAG0U bit 1 indicates if the tag is valid or not.
 *
 *	There are 4 such register pairs.
 *
 * Routine firstreads the upper tag register to check if data is valid, and
 * then reads the lower tag register to get the actual cache tag.
 *
 * IP21 gets a separate C routine, and the io4_config_read assembly routine.
 */

#if defined(IP19) || defined(IP25)

#define	TAG0L_OFFS		0
#define	TAG0U_OFFS		8
#define	TAG1L_OFFS		16
#define	TAG1U_OFFS		24
#define	TAG2L_OFFS		32
#define	TAG2U_OFFS		40
#define	TAG3L_OFFS		48
#define	TAG3U_OFFS		56

#define	CACHETAG_VALID		0x2
#define	CACHELINE_SIZE_SHFT	7

/*
 * t0 has the cached address used to pull the line in all cases, and
 * t1 has the suitable K0 base address.
 * t2 will hold the mask value.
 * This routine assumes kernel running in 64 bit mode.
 */

LEAF(io4_doflush_cache)
	
	LI	t1, (K0BASE)
	LI	t2, 0xFFFFFFFF

	.set	noreorder
	# Pull line sitting in CACHETAG0

	PTR_L	t0, TAG0L_OFFS(a0)

	# If invalid tag, skip flushing this.
	beq	t0, zero, 1f
	and	t0, t0, t2		# BD Slot

	PTR_SLL t0, CACHELINE_SIZE_SHFT
	PTR_ADDU t0, t1			# t0 has proper K0 address to use
	PTR_L	zero, 0(t0)		# Read a word from there.

#ifndef IP25
	# don't need the HWBINVs for R10000. Moreover, using
	# cacheops exposes an IP25 bug. Hence skip this for IP25.
	# t0 has the cache aligned K0 address that can be used to 
	# do writeback invalidate.
	cache   CACH_SD|C_HWBINV,0(t0)  # writeback + inval
#endif

1:
	# Pull line sitting in CACHETAG1
	PTR_L	t0, TAG1L_OFFS(a0)

	# If invalid tag, skip flushing this.
	beq	t0, zero, 2f
	and	t0, t0, t2		# BD Slot

	PTR_SLL t0, CACHELINE_SIZE_SHFT
	PTR_ADDU t0, t1			# t1 has K0 address for pulling TAG1L 
	PTR_L	zero, 0(t0)		# Pull and discard data

#ifndef IP25
	# writeback invalidate t0
	cache	CACH_SD|C_HWBINV,0(t0)  # writeback + inval
#endif

2:
	# Pull line sitting in CACHETAG2
	PTR_L	t0, TAG2L_OFFS(a0)

	# If invalid tag, skip flushing this.
	beq	t0, zero, 3f
	and	t0, t0, t2		# BD Slot

	PTR_SLL	t0, CACHELINE_SIZE_SHFT
	PTR_ADDU t0, t1                 # t0 has K0 address for pulling TAG2L
	PTR_L   zero, 0(t0)             # Pull and discard data

#ifndef IP25
	# writeback invalidate t0
	cache   CACH_SD|C_HWBINV,0(t0)  # writeback + inval
#endif

3:
	# Pull line sitting in CACHETAG3
	PTR_L   t0, TAG3L_OFFS(a0)

	# If invalid tag, skip flushing this.
	beq	t0, zero, 4f
	and	t0, t0, t2		# BD Slot

	PTR_SLL t0, CACHELINE_SIZE_SHFT
	PTR_ADDU t0, t1                 # t3 has K0 address for pulling TAG3L
	PTR_L	zero, 0(t0) 		# Pull and discard data

#ifndef IP25
	# writeback invalidate t0
	cache   CACH_SD|C_HWBINV,0(t0)  # writeback + inval
#endif

4:
	# We are all done. 
	# head back.
	j	ra
	nop
	.set    reorder
	END(io4_doflush_cache)

#endif	/* defined(IP19) */

LEAF(symmon_dump)
	.set noreorder
	LI	k0,SR_KADDR|SR_DEFAULT	# make sure page size bits
	DMTC0(k0, C0_SR)		# are on
	nop
	nop
	nop
	nop
	nop
	nop
	nop
 
	LA      sp, dumpstack
	LI	gp, DUMP_STACK_SIZE - 16
	PTR_ADD sp, gp			# Set our stack pointer.
	LA      gp, _gp
	j       _symmon_dump
	nop                             # (BD)
	END(symmon_dump)
	
#ifdef MC3_CFG_READ_WAR

LOCALSPINSZ=	4			# ra, s0, s1, s2
SPINSPLFRM=	FRAMESZ((NARGSAVE+LOCALSPINSZ)*SZREG)
RA_SPINOFF=		SPINSPLFRM-(1*SZREG)
S0_SPINOFF=		SPINSPLFRM-(2*SZREG)
S1_SPINOFF=		SPINSPLFRM-(3*SZREG)
S2_SPINOFF=		SPINSPLFRM-(4*SZREG)
/*
 * int ospl = special_trylock(lock_t *lock, splfunc_t);
 * return -1 if not successful.
 */
NESTED(special_trylock, SPINSPLFRM, zero)
	.set	noreorder
	PTR_SUBU sp,SPINSPLFRM
	REG_S	ra,RA_SPINOFF(sp)
	REG_S	s1,S1_SPINOFF(sp)
	move	s1,a1		# remember spl routine address
	REG_S	s0,S0_SPINOFF(sp)
	move	s0,a0

	# call spl routine
	jal	s1
	nop

	# See if lock is busy.
1:	LOCK_LL	ta0,0(s0)
	andi	ta1,ta0,1
	bne	ta1,zero,3f
	or	ta0,1

	# Try to mark lock busy
	LOCK_SC	ta0,0(s0)	
#ifdef R10000_LLSC_WAR
	beql	ta0,zero,1b
#else
	beq	ta0,zero,1b
#endif
	nop

	# We have the lock.
2:	REG_L	ra,RA_SPINOFF(sp)
	REG_L	s0,S0_SPINOFF(sp)
	REG_L	s1,S1_SPINOFF(sp)
	j	ra
	PTR_ADDU sp,SPINSPLFRM

3: 	# Lock is busy.  
	jal	splx
	move	a0,v0
	b	2b
	daddiu	v0,zero,-1

	.set	reorder
	END(special_trylock)

#endif /* MC3_CFG_READ_WAR */


#ifdef	TFP
/*
 * special routine to read IO4 config register.
 * Input:
 *	a0 = starting address of the config registers.
 *	a1 = lower limit on data read
 *	a2 = upper limit on data read.
 */
LEAF(io4_config_read)

#ifdef	DEBUG
	move	ta1, zero	# Number of loops through this routine.
#endif
	# load EV_RTC address in ta0.
	LI	t9, EV_RTC
	LI	ta0, 10000000

1:
	addi	ta0, -1
	beq	ta0, zero, io4err

#ifdef	DEBUG
	add	ta1, 1
#endif	/* DEBUG */


	.set	noreorder
	#
	# Disable Interrupts 
	# Read config register four times with 
	# reading EV_RTC value in between.
	#

	DMFC0(t8, C0_SR)
	ori	t0, t8, SR_IE
	xori	t0, SR_IE		# disable interrupts
	DMTC0(t0, C0_SR)

	ld	t0, 0(a0)
	NOP_SSNOP
	ld	t1, 0(a0)
	NOP_SSNOP

	#
	# intersperse an RTC Read.
	#
	ld	v0, 0(t9)
	NOP_SSNOP
	#

	ld	t2, 0(a0)
	NOP_SSNOP
	ld	t3, 0(a0)
	NOP_SSNOP

	DMTC0(t8, C0_SR)
	.set	reorder

	#
	# If any of these are same as value read for RTC,
	# then we are getting bogus values. Get back.
	# Since we anyway check if t0 == t1 == t2 == t3,
	# just check if any one of these is equal to EV_RTC value,
	# and  loop back if true.

	beq	t0, v0, 1b


	# Make sure that t0 == t1 == t2 == t3
	bne	t0, t1, 1b
	bne	t0, t2, 1b
	bne	t0, t3, 1b

	#
	# Register read successful. 
	# Check for limits.
	#
	# Clip t0 to be 32 bits, 
	# and massage bit properly
	#
	LI	t1, 0xFFFFFFFF
	and	v0, t0, t1
	PTR_SLL	v0, 7

	#
	# At boot time, you are going to get zero!!.
	#
	beq	v0, zero, 2f

	#
	# If v0, is outside <a1..a2>, redo it all over.
	#
	.set	at
	blt	v0, a1, 1b
	bgt	v0, a2, 1b
	.set	noat

#ifdef	DEBUG
	# Bump count of register read
	# stats_count[0] == number of registers read.
	LA	t0, stats_count
	lw	t1, 0(t0)
	addi	t1, 1
	sw	t1, 0(t0)

	addi	ta1, -1

	# stats_count[1] = num loops done
	lw	t1, 4(t0)
	add	t1, ta1
	sw	t1, 4(t0)

	# stats_count[2] = max value of loops done.
	lw	t1, 8(t0)
	.set	at
	bgt	t1, ta1, 2f
	.set	noat
	sw	ta1, 8(t0)
#endif	/* DEBUG */

2:
	jr	ra

	/* loop count exhausted - we tried 1000000 times */
io4err:	
	li	v0, -1		/* error flag */
	jr	ra
	END(io4_config_read)
#endif	/* TFP */	

#ifdef IP19
	/* These conversion routines exists mainly to trick the compiler.
	 * Using standard PHYS_TO_K0 macro, the compiler generates 64-bit
	 * constants and loads them globally, either through "gp" register
	 * or using a global K0 address.  This allows ecc_handler and friends
	 * to call an external function to perform the conversion and avoid
	 * the cached access, which could alter the state of the cache
	 * while we're trying to examine it.
	 */
LEAF(ecc_phys_to_k0)
	LI	v0, TO_PHYS_MASK
	and	a0, v0
	LI	v0, K0BASE
	or	v0, a0
	jr	ra
	END(ecc_phys_to_k0)
	
LEAF(ecc_k0_to_phys)
	LI	v0, TO_PHYS_MASK
	and	v0, a0
	jr	ra
	END(ecc_k0_to_phys)	
#endif /* IP19 */		
	.data
inv_scachsz: .asciiz "CPU %d scache size 0x%x different from bootmaster size 0x%x\n"

EXPORT(pod_nmi_fp3)
	.dword	0
	/* Add pad so that nmi_saveregs will not have other data on same
	 * cacheline since we use uncached operations to write registers.
	 * (NOTE: Dump code will use cached access).
	 */ 
	.align 7
EXPORT(nmi_cpad1)
	.dword  0: (16)         
	
EXPORT(nmi_saveregs)
	.dword	0: (EV_MAX_CPUS * NMI_SAVE_REGS)
EXPORT(nmi_cpad2)
	.dword  0: (16)         

#if	defined(DEBUG) && defined(TFP)
EXPORT(stats_count)
	.word 	0: 4
#endif	/* DEBUG */

#if R4000	
/* Following routine needed for old R4400 chips which have a bug preventing
 * use of primary cache operations.
 */


#define ICACHE_LINE_CODE				\
	ld	a1, (a0)	;	/* 0 */ 	\
	addi	a0, 8		;	/* 4 */		\
	ld	a1, (a0)	;	/* 8 */ 	\
	addi	a0, 8		;	/* 12 */	\
	ld	a1, (a0)	;	/* 16 */	\
	addi	a0, 8		;	/* 20 */	\
	ld	a1, (a0)	;	/* 24 */ 	\
	addi	a0, 8
	

/* Four icache lines per gcache line (128 bytes) */
#define FOUR_LINES_CODE		\
	ICACHE_LINE_CODE;	\
	ICACHE_LINE_CODE;	\
	ICACHE_LINE_CODE;	\
	ICACHE_LINE_CODE	\

/* 32 icache lines per 1k */
#define THIRTYTWO_LINES_CODE			\
	FOUR_LINES_CODE; FOUR_LINES_CODE;	\
	FOUR_LINES_CODE; FOUR_LINES_CODE;	\
	FOUR_LINES_CODE; FOUR_LINES_CODE;	\
	FOUR_LINES_CODE; FOUR_LINES_CODE	\

/* 
 * void indirect_flush_primary_caches
 * Flushes both primary caches (assumed to be 16K in size) by loading
 * itself into the dcache.
 */
	.text
LEAF(indirect_flush_primary_caches)
	LA	a0, code_start
	jr	a0
	/*
	 * This is 16k of code that basically empties both the primary data
	 * and primary icache by executing 16K worth of instructions which
	 * load themselves into the dcache.
	 */

	.set noreorder
	nop

	.align 7
code_start:
	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 2k */
	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 4k */
	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 6k */
	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 8k */
	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 10k */
	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 12k */
	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 14k */
	THIRTYTWO_LINES_CODE; THIRTYTWO_LINES_CODE	/* 16k */
	.set reorder
get_out:
	j	ra
	END(indirect_flush_primary_caches)
#endif /* R4000 */	

#ifdef MAPPED_KERNEL
#ifdef MULTIKERNEL
#if EVMK_NUMCELLS == 2
#elif EVMK_NUMCELLS == 4
#elif EVMK_NUMCELLS == 8
#else
	<< BOMB - EVMK_NUMCELLS must be defined as either 2, 4, or 8 >>
#endif
	
	.text
LEAF(evmk_multicell_start)
	.set	at
	move	s0, a0			# Save a0 value
	move	s1, a1			# Save a1 value
	move	s2, a2			# Save a2 value
	move	s8, ra			# save ra value

	LA	a0, evmk		/* a0 = addr(evmk) */
	
	LI	t0, ECFGINFO_ADDR
	li	t1, EVMK_NUMCELLS

reduce_numcells:			
	sb	t1, ECFG_NUMCELLS(t0)	# setup numcells in (uncached) config
	sb	t1, EVMK_NCELLS(a0)	# setup cell cnt in kernel cached space

	/* First we divide memory for the cells.  Since the tlb uses 16MB
	 * tlb entries, the memory for each cell must be on a 16 MB boundary.
	 * For simplicity we trunceate memory size to 16MB multiple, then
	 * give each cell the same amount of memory.
	 * NOTE: Total memory used will be either a multiple of 32MB (2 cell)
	 * or 64MB (4 cell).
	 */
	addi	t2, t1, -1		# t2 = cell mask (from cpu_vpid)
	sb	t2, ECFG_CELLMASK(t0)	# uncached access (symmon, etc)
	sb	t2, EVMK_CELLMASK(a0)	# cached access for kernel
	
	lw	t1, ECFG_MEMSIZE(t0)	# nbr of 256-byte memory blocks
	srl	t2, 1
	move	s3, t2
	srl	t1, 17			# round-off to multiple of 32 MB
	beq	t2, zero, 1f

	srl	t2, 1
	srl	t1, 1			# 4 cells, roundoff to 64 MB
	beq	t2, zero, 1f
	
	srl	t1, 1			# 8 cells, roundoff to 128 MB
			
1:	
	sll	t1, 16

	/* Make sure we have at least 64 MB per cell */
	li	a1, 0x00040000		# 64 MB (nbr of 256-byte blocks)
	sub	a1, t1, a1
	bgez	a1,1f
	lb	t1, ECFG_NUMCELLS(t0)
	srl	t1, 1			# half the number of cells
	bgt	t1, zero, reduce_numcells

	/* only way to fall through here is less than 64MB, so we have 1 cell */
	

1:	/* we have at least 64 MB per cell */	
	
	PTR_ADDI t0, ECFG_CELL
	sw	t1, ECFG_CELL_MEMSIZE(t0)
	
	PTR_ADDI t0, SIZEOF_CELLINFO
	sw	t1, ECFG_CELL_MEMBASE(t0)
	sw	t1, ECFG_CELL_MEMSIZE(t0)
	beq	s3, zero, 1f

	/* >= 4 cells */

	addu	t2, t1, t1
	PTR_ADDI	t0, SIZEOF_CELLINFO
	sw	t2, ECFG_CELL_MEMBASE(t0)
	sw	t1, ECFG_CELL_MEMSIZE(t0)
	
	addu	t2, t1
	PTR_ADDI	t0, SIZEOF_CELLINFO
	sw	t2, ECFG_CELL_MEMBASE(t0)
	sw	t1, ECFG_CELL_MEMSIZE(t0)
	srl	s3, 1
	beq	s3, zero, 1f
	
	/* 8 cells */
	addu	t2, t1
	PTR_ADDI	t0, SIZEOF_CELLINFO
	sw	t2, ECFG_CELL_MEMBASE(t0)
	sw	t1, ECFG_CELL_MEMSIZE(t0)
	
	addu	t2, t1
	PTR_ADDI	t0, SIZEOF_CELLINFO
	sw	t2, ECFG_CELL_MEMBASE(t0)
	sw	t1, ECFG_CELL_MEMSIZE(t0)
	
	addu	t2, t1
	PTR_ADDI	t0, SIZEOF_CELLINFO
	sw	t2, ECFG_CELL_MEMBASE(t0)
	sw	t1, ECFG_CELL_MEMSIZE(t0)
	
	addu	t2, t1
	PTR_ADDI	t0, SIZEOF_CELLINFO
	sw	t2, ECFG_CELL_MEMBASE(t0)
	sw	t1, ECFG_CELL_MEMSIZE(t0)
1:
	
	/* Now we copy the entire kernel image into the other cells */

	LA	s3, __elf_header	/* s3 = source address */
	PTR_L	s4, kend
	PTR_SUB	s4, s4, s3		/* s4 = size to be copied */
	PTR_SLL s5, t1, 8		/* s5 = byte size of each cell */

	LI	t3, K0BASE
	
	PTR_S	t3, EVMK_CELL_K0BASE(a0)	/* cell_k0base for cell 0 */
	PTR_ADD	a1, s5, t3
	PTR_S	a1, 8+EVMK_CELL_K0BASE(a0)	/* cell_k0base for cell 1 */
	PTR_ADD	a1, s5
	PTR_S	a1, 16+EVMK_CELL_K0BASE(a0)	/* cell_k0base for cell 2 */
	PTR_ADD	a1, s5
	PTR_S	a1, 24+EVMK_CELL_K0BASE(a0)	/* cell_k0base for cell 3 */
	PTR_ADD	a1, s5
	PTR_S	a1, 32+EVMK_CELL_K0BASE(a0)	/* cell_k0base for cell 4 */
	PTR_ADD	a1, s5
	PTR_S	a1, 40+EVMK_CELL_K0BASE(a0)	/* cell_k0base for cell 5 */
	PTR_ADD	a1, s5
	PTR_S	a1, 48+EVMK_CELL_K0BASE(a0)	/* cell_k0base for cell 6 */
	PTR_ADD	a1, s5
	PTR_S	a1, 56+EVMK_CELL_K0BASE(a0)	/* cell_k0base for cell 7 */
	
	PTR_ADD s6, s5, t3		/* current cell base memory address */

	/* set s7 to cell mask (EVMK_NUMCELLS - 1) */
	
	LI	s7, ECFGINFO_ADDR
	lb	s7, ECFG_NUMCELLS(s7)
	addi	s7, -1

	/* This loop copies the kernel into each of the other cells */
1:
	and	t3, s3, 0x00ffffffff	/* source's offset from its' membase */
	PTR_ADD	a1, s6, t3		/* destination address */
	move	a0, s3			/* source address */
	move	a2, s4			/* size in bytes */
	addi	s7, -1
	jal	bcopy

	/* update the cell id in each memory cell */
	LA	a0, evmk		/* a0 = addr(evmk) */
	PTR_ADD	t3, a0, EVMK_CELLID
	and	t3, 0x00ffffffff	/* offset from base of evmk_cellid */
	PTR_ADD	t3, s6


	LI	t0, ECFGINFO_ADDR
	lb	t0, ECFG_NUMCELLS(t0)
	addi	t0, -1			/* t0 == cell mask (EVMK_NUMCELLS-1) */
	xor	t0, s7

	sh	t0, (t3)		/* store cellid into evmk_cellid */

#ifdef CELL	
	LA	t3, my_cellid
	and	t3, 0x00ffffffff	/* offset from base of my_cellid */
	PTR_ADD	t3, s6
	sh	t0, (t3)		/* store cellid into my_cellid */
#endif /* CELL */	
	
	PTR_ADD	s6, s5			/* next cell base address */
	bgtz	s7, 1b

	move	a0, s0			# Restore a0 value
	move	a1, s1			# Restore a1 value
	move	a2, s2
	move	ra, s8
	.set	noat
	j	ra		
	END(evmk_multicell_start)
#endif /* MULTIKERNEL */	
/*
 * mapped_kernel_setup_tlb(text_nasid, data_nasid)
 *
 * NOTE: For now the parameter is the starting physical address of the
 * cell's memory.
 */
LEAF(mapped_kernel_setup_tlb)
#ifndef R8000
	.set noreorder
	.set at
	and	a0,0x7ffff000		# mask PFN suitably
	srl	a0,PNUMSHFT - PTE_PFNSHFT	# align for proper pde
	or	a4,a0,PG_M|PG_G|PG_VR|PG_COHRNT_EXLWR
	move	a3,a4

	li	a5, MAPPED_KERN_TLBMASK
#if defined(_COMPILER_VERSION) && (_COMPILER_VERSION>620) && !defined(PTE_64BIT)
        /*
	 * For earlier revisions of ragnarok, PTEs are passed in
	 * low half of register instead of high half where they
	 * belong -- in newer compilers, we must move them there:
         */
        dsll    a3,a3,32                # 4th arg - 1st pte
        dsll    a4,a4,32                # 5th arg - 1st pte
#endif
	li	a0,1
	move	a1,zero
	dli	a2, 0xc000000000000000	# Virtual address base.
	j	tlbwired
	nada
	
#else
	Bomb!  Need to implement mapping code for other processors.
#endif /* R10000 */

	j	ra
	nada
	END(mapped_kernel_setup_tlb)
	
#endif /* MAPPED_KERNEL */
