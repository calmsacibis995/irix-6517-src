#if TFP

#ident	"libsk/ml/tfpasm.s:  $Revision: 1.27 $"

/*
 * TFP specific generic assembler functions
 */
#include "ml.h"
#include <asm.h>
#include <sys/immu.h>
#include <sys/sbd.h>
#include <regdef.h>

#if EVEREST
#include <sys/EVEREST/IP21.h>
#endif

#if IP26
#include <sys/cpu.h>
#endif

#define NOINTS(x,y)			\
	li	x,(SR_PAGESIZE>>32);	\
	dsll	x,32;			\
	MTC0_NO_HAZ(x,C0_SR);		\
	NOP_MTC0_HAZ

LEAF(tfp_get_trapbase)
	.set	noreorder
	DMFC0(v0,C0_TRAPBASE)
	.set	reorder
	j	ra
	END(tfp_get_trapbase)

LEAF(tfp_set_trapbase)
	.set	noreorder
#if IP26			/* need to clear TETON_BEV handler */
	DMTC0	(zero,TETON_BEV)
#endif
	DMTC0(a0,C0_TRAPBASE)
	.set	reorder
	j	ra
	END(tfp_set_trapbase)

LEAF(tfp_getticker)
	.set	noreorder
	DMFC0(v0,C0_COUNT)
	.set	reorder
	j	ra
	END(tfp_getticker)

#if 0					/* currently unused */
LEAF(tfp_setticker)
	.set	noreorder
	DMTC0(a0,C0_COUNT)
	.set	reorder
	j	ra
	END(tfp_setticker)
#endif

LEAF(tfp_getconfig)
	.set	noreorder
	DMFC0(v0, C0_CONFIG)
	nop
	.set	reorder
	j	ra
	END(tfp_getconfig)

LEAF(tfp_getprid)
	.set	noreorder
	DMFC0(v0, C0_PRID)
	nop
	nop
	andi	t0, v0, 0xff
	bne	t0, zero, 9f	# if revision non-zero, go report it
	nop
	
	/* Revision field is zero, use bits in IC field of CONFIG register
	 * to correctly determine revision.
	 */
	DMFC0(t0,C0_CONFIG)
	and	t1, t0, CONFIG_IC
	dsrl	t1, CONFIG_IC_SHFT	# IC field now isolated
	lb	t2, revtab(t1)		# load minor revision
	daddu	v0, t2			# add into implementation
9:
	.set	reorder
	j	ra
	.rdata
	/* Sequence of minor revisions from IC field of CONFIG register:
	 * 	IC field:  0 1 2 3 4 5 6 7
	 *  	major rev  0 0 0 1 2 3 2 1
	 *      minor rev  0 0 0 1 2 0 1 2
	 */
revtab:
	.word	0x00000011
	.word	0x22302112
	.text
	END(tfp_getprid)

/*
 * If we have a config read  (from physical addr 18008000 to 1800ffff)
 * Disable interrupts, then delay in order to allow any pending
 * writebacks to complete (and we must insure that no additional
 * writebacks are initiated).  Then read the config register until
 * we get the same data twice.
 */
#if TFP_CC_REGISTER_READ_WAR && EVEREST
#define CC_READ_WAR			\
	.set	noreorder;		\
	dli	t0, EV_SYSCONFIG;	\
	beq	a0, t0, 7f;		\
	nop;				\
	dli	t0, EV_CONFIGREG_BASE;	\
	dsubu	t0, a0, t0;		\
	bltz	t0, 9f;			\
	nop;				\
	dsubu	t0, 0x08000;		\
	bgez	t0, 9f;			\
	nop;				\
	.align	7;			\
7:					\
	DMFC0(t1, C0_SR);		\
	ori	t0, t1, SR_IE;		\
	xori	t0, SR_IE;		\
	DMTC0(t0, C0_SR);		\
					\
1:	li	t3, 4000;		\
2:	daddi	t3, -1;			\
	bgez	t3, 2b;			\
	NOP_SSNOP;			\
					\
	ld	v0, 0(a0);		\
	dli	t2, EV_RTC;		\
					\
	li	t3, 4000;		\
2:	daddi	t3, -1;			\
	bgez	t3, 2b;			\
	NOP_SSNOP;			\
					\
	ld	t2, (t2);		\
					\
	li	t3, 4000;		\
2:	daddi	t3, -1;			\
	bgez	t3, 2b;			\
	NOP_SSNOP;			\
					\
	ld	t0, 0(a0);		\
					\
	li	t3, 4000;		\
2:	daddi	t3, -1;			\
	bgez	t3, 2b;			\
	NOP_SSNOP;			\
					\
	beq	t0, t2, 1b;		\
	nop;				\
	bne	v0, t0, 1b;		\
	nop;				\
					\
	DMTC0(t1, C0_SR);		\
	j	ra;			\
	nop;				\
	.set	reorder;		\
9:
#else
#define CC_READ_WAR
#endif /* TFP_CC_REGISTER_READ_WAR */

/*
 * long long load_double(long long *)
 *
 * Implicitly assumes that address is doubleword aligned!
 *
 * On entry:
 *	a0 -- the address from which to load double
 */
LEAF(load_double)
	CC_READ_WAR
XLEAF(load_double_nowar)
	ld	v0,0(a0)
	j	ra
	END(load_double)

/*
 * void store_double(long long *, long long)
 *
 * On entry: 
 *	a0 -- the address into which to store double.
 *	a2/a3 -- the 8 byte value to store.
 */
LEAF(store_double)
XLEAF(store_double_rtn)
	sd	a1,0(a0)
	j	ra
	END(store_double)

#if EVEREST
/* not supported for tfp since it doesn't support multiple page sizes */
LEAF(get_pgmask)
	.set	noreorder
	j	ra
	nop
	.set	reorder
	END(get_pgmask)

/* not supported for tfp since it doesn't support multiple page sizes */
LEAF(set_pgmask)
	.set	noreorder
	j	ra
	nop
	.set	reorder
	END(set_pgmask)
#endif

/*
 * invaltlb(i,j): Invalidate the ith ITLB entry in set j.
 * Called whenever a specific TLB entry needs to be invalidated.
 * Used by clock based routine to slowly flush all tlbs, and
 * also used in DEBUG to unmap uarea & kstack.
 */
LEAF(invaltlb_in_set)
	.set	noreorder
	DMFC0(t0,C0_TLBHI)		# save current TLBHI
	DMFC0(v0,C0_SR)			# save SR and disable interrupts
	NOINTS(t1,C0_SR)
	DMTC0(zero,C0_TLBLO)

	/* TFP HW requires that the virtual addresses in each set at a
	 * given index be unique.  So we create an invalid entry which
	 * has the set number included as a field in the VA.
	 * Also, place the bogus addresses in a portion of KV0 space
	 * which isn't used to avoid duplicate entries.
	 */
	dsll	t1,a1,8+PNUMSHFT	# set number shifted above index
	dsll	a0,PNUMSHFT		# convert index into virtual address
	dadd	a0,t1
	LI	t1, (KV0BASE>>48)	# Kernel private address space
	dsll	t1, 48
	dadd	a0, t1
	DMTC0(a0,C0_BADVADDR)		# select correct index
	DMTC0(a0,C0_TLBHI)		# select correct index
	DMTC0(a1,C0_TLBSET)		# select correct set

	TLB_WRITER
	DMTC0(t0,C0_TLBHI)		# restore TLBPID
	DMTC0(v0,C0_SR)			# restore SR
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(invaltlb_in_set)
/*
 * tlbwired(indx, tlbpid, vaddr, pte) -- setup wired ITLB entry
 * a0 -- indx -- tlb entry index
 * a1 -- tlbpid -- context number to use (0..TLBHI_NPID-1)
 * a2 -- vaddr -- virtual address (could have offset bits)
 * a3 -- pte -- contents of pte
 */
LEAF(tlbwired)
	.set	noreorder
	DMFC0(t0,C0_TLBHI)		# save current TLBPID
	DMFC0(v0,C0_SR)			# save SR and disable interrupts
	NOINTS(t1,C0_SR)
	DMTC0(a2,C0_BADVADDR)
	.set	reorder
	dsll	a3,TLBLO_HWBITSHIFT	# shift pfn field to correct position
	sll	a1,TLBHI_PIDSHIFT	# line up pid bits
	dsrl	a2,PNUMSHFT		# chop offset bits
	dsll	a2,PNUMSHFT
	or	a1,a2			# formatted tlbhi entry
	.set	noreorder
	DMTC0(a1,C0_TLBHI)		# set VPN and TLBPID
	DMTC0(a3,C0_TLBLO)		# set PPN and access bits
	DMTC0(zero,C0_TLBSET)		# wired entries are in Set 0
	TLB_WRITER			# drop it in
	DMTC0(t0,C0_TLBHI)		# restore TLBPID
	DMTC0(v0,C0_SR)			# restore SR
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(tlbwired)

/*
 * get_tlblo(index, set) -- return entry low contents of tlb entry "index"
 */
LEAF(get_tlblo0)
XLEAF(get_tlblo)
	.set	noreorder
	DMFC0(t0,C0_SR)			# save sr
	and	t1,t0,~SR_IE
	DMTC0(t1,C0_SR)			# disable interrupts
	nop
	DMFC0(t1,C0_TLBHI)		# save current pid
	dsll	a0,PNUMSHFT		# convert index into virtual address
	DMTC0(zero,C0_TLBHI)		# clear pid
	DMTC0(a0,C0_BADVADDR)		# select correct index
	DMTC0(a1,C0_TLBSET)		# select correct set
	TLB_READ			# read entry to entry hi/lo
	DMFC0(v0,C0_TLBLO)		# to return value
	DMTC0(t1,C0_TLBHI)		# restore current pid
	DMTC0(t0,C0_SR)			# restore sr
	j	ra
	nop
	.set	reorder
	END(get_tlblo0)

/*
 * set_cp0_tlblo(tlblo)
 */
LEAF(set_cp0_tlblo)
	.set	noreorder
	DMTC0(a0,C0_TLBLO)
	j	ra
	nop
	END(set_cp0_tlblo)
/*
 * get_cp0_tlblo()
 */
LEAF(get_cp0_tlblo)
	DMFC0(v0,C0_TLBLO);
	j	ra
	nop
	END(get_cp0_tlblo)
/*
 * set_cp0_tlbhi(tlbhi)
 */
LEAF(set_cp0_tlbhi)
	DMTC0(a0,C0_TLBHI)
	j	ra
	nop
	END(set_cp0_tlbhi)
/*
 * get_cp0_tlbhi()
 */
LEAF(get_cp0_tlbhi)
	DMFC0(v0,C0_TLBHI)
	j	ra
	nop
	END(get_cp0_tlbhi)
/*
 * set_tlbset(tlbset)
 */
LEAF(set_cp0_tlbset)
	DMTC0(a0,C0_TLBSET)
	j	ra
	nop
	END(set_cp0_tlbset)
/*
 * get_cp0_tlbset()
 */
LEAF(get_cp0_tlbset)
	DMFC0(v0,C0_TLBSET)
	j	ra
	nop
	END(get_cp0_tlbset)
/*
 * set_cp0_badvaddr(badvaddr)
 */
LEAF(set_cp0_badvaddr)
	DMTC0(a0,C0_BADVADDR)
	j	ra
	nop
	END(set_cp0_badvaddr)
/*
 * get_cp0_badvaddr()
 */
LEAF(get_cp0_badvaddr)
	DMFC0(v0,C0_BADVADDR)
	j	ra
	nop
	END(get_cp0_badvaddr)
/*
 * cp0_tlb_write()
 */
LEAF(cp0_tlb_write)
	TLB_WRITER
	j	ra
	nop
	END(cp0_tlb_write)
/*
 * cp0_tlb_read()
 */
LEAF(cp0_tlb_read)
	TLB_READ
	j	ra
	nop
	END(cp0_tlb_read)
/*
 * cp0_tlb_probe()
 */
LEAF(cp0_tlb_probe)
	TLB_PROBE
	DMFC0(v0,C0_TLBSET)
	j	ra
	nop
	.set	reorder
	END(cp0_tlb_probe)

/*
 * get_tlbhi(index, set) -- return entry high contents of tlb entry "index"
 */
LEAF(get_tlbhi)
	.set	noreorder
	DMFC0(t0,C0_SR)			# save sr
	and	t1,t0,~SR_IE
	DMTC0(t1,C0_SR)			# disable interrupts
	nop
	DMFC0(t1,C0_TLBHI)		# save current pid
	dsll	a0,PNUMSHFT		# convert index into virtual address
	DMTC0(zero,C0_TLBHI)		# clear pid
	DMTC0(a0,C0_BADVADDR)		# select correct index
	DMTC0(a1,C0_TLBSET)		# select correct set
	TLB_READ			# read entry to entry hi/lo
	DMFC0(v0,C0_TLBHI)		# to return value
	DMTC0(t1,C0_TLBHI)		# restore current pid
	DMTC0(t0,C0_SR)			# restore sr
	j	ra
	nop
	.set	reorder
	END(get_tlbhi)

/*
 * invaltlb_in_set(i,j): Invalidate the ith ITLB entry in set j.
 * Called whenever a specific TLB entry needs to be invalidated.
 * Used by clock based routine to slowly flush all tlbs, and
 * also used in DEBUG to unmap uarea & kstack.
 */
LEAF(invaltlb_in_set)
	.set	noreorder
	DMFC0(t0,C0_TLBHI)		# save current TLBHI
	DMFC0(v0,C0_SR)			# save SR and disable interrupts
	NOINTS(t1,C0_SR)
	DMTC0(zero,C0_TLBLO)

	/* TFP HW requires that the virtual addresses in each set at a
	 * given index be unique.  So we create an invalid entry which
	 * has the set number included as a field in the VA.
	 * Also, place the bogus addresses in a portion of KV0 space
	 * which isn't used to avoid duplicate entries.
	 */
	dsll	t1,a1,8+PNUMSHFT	# set number shifted above index
	dsll	a0,PNUMSHFT		# convert index into virtual address
	dadd	a0,t1
	LI	t1, (KV0BASE>>48)	# Kernel private address space
	dsll	t1, 48
	dadd	a0, t1
	DMTC0(a0,C0_BADVADDR)		# select correct index
	DMTC0(a0,C0_TLBHI)		# select correct index
	DMTC0(a1,C0_TLBSET)		# select correct set

	TLB_WRITER
	DMTC0(t0,C0_TLBHI)		# restore TLBPID
	DMTC0(v0,C0_SR)			# restore SR
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(invaltlb_in_set)

#endif /* TFP */
