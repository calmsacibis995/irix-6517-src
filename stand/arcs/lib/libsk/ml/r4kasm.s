#if R4000 || R10000
#ident	"libsk/ml/r4kasm.s:  $Revision: 1.31 $"

/*
 * Doubleword Shift by more than 32 is broken in the current version
 * of R4000.  It's expected to be fixed in the R4000A, so this flag
 * can be removed.
 */
#define DS32_WAR 1

/*
 * R4000 specific generic assembler functions
 */
#include	"ml.h"
#include <asm.h>
#include <sys/sbd.h>
#include <regdef.h>
#include <sys/fpu.h>

LEAF(r4k_getticker)
	.set	noreorder
	mfc0	v0,C0_COUNT
	.set	reorder
	j	ra
	END(r4k_getticker)

LEAF(r4k_setticker)
	.set	noreorder
	mtc0	a0,C0_COUNT
	.set	reorder
	j	ra
	END(r4k_setticker)

LEAF(r4k_setcompare)
	.set	noreorder
	mtc0	a0,C0_COMPARE
	.set	reorder
	j	ra
	END(r4k_setcompare)

LEAF(r4k_getconfig)
	.set	noreorder
	mfc0	v0, C0_CONFIG
	nop
	.set	reorder
	j	ra
	END(r4k_getconfig)

LEAF(r4k_getprid)
XLEAF(get_cpu_irr)
	.set	noreorder
	mfc0	v0, C0_PRID
	nop
	nop
	.set	reorder
	j	ra
	END(r4k_getprid)

LEAF(r4k_get_fpirr)
XLEAF(get_fpc_irr)
	.set    noreorder
	cfc1	v0, fpc_irr
	nop
	nop
	.set	reorder
	j	ra
	END(r4k_get_fpirr)

/*
 * long long load_double(long long *)
 *
 * Implicitly assumes that address is doubleword aligned!
 *
 * On entry:
 *	a0 -- the address from which to load double
 */
LEAF(load_double)
XLEAF(load_double_nowar)
	ld	v0,0(a0)
#if _MIPS_SIM == _ABIO32
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
#endif
	j	ra
	END(load_double)

#if (_MIPS_SIM == _ABIO32)
/*
 * void store_double(long long *, long long)
 *
 * On entry: 
 *	a0 -- the address into which to store double.
 *	a1 -- the 8 byte value to store.
 */
LEAF(store_double)
XLEAF(store_double_rtn)
#if _MIPS_SIM == _ABIO32
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
#else /* ! _ABIO32 */
	sd	a1,0(a0)
#endif
	j	ra
	END(store_double)

#endif /* _MIPS_SIM == _ABIO32 */	


/* r4k supports hardware watchpoints */
LEAF(get_watchlo)
	.set	noreorder
#ifdef R4600
	mfc0	v0,C0_PRID
	NOP_1_4
	andi	v0,0xFF00
	sub	v0,0x2000		# r4600
	beqz	v0,96f
	nop
	sub	v0,(0x2100-0x2000)	# check for R4700
	beqz	v0,96f
	sub 	v0,(0x2300-0x2100)	# check for r5000
	beqz    v0,96f
        sub     v0,(0x2800-0x2300)      # check for rm5271
	bnez	v0,97f
	nop
96:	j	ra
	nop
97:
#endif
	mfc0	v0,C0_WATCHLO
	nop
	j	ra
	nop
	.set	reorder
	END(get_watchlo)

LEAF(get_watchhi)
	.set	noreorder
#ifdef R4600
	mfc0	v0,C0_PRID
	NOP_1_4
	andi	v0,0xFF00
	sub	v0,0x2000
	beqz	v0,96f
	nop
	sub	v0,(0x2100-0x2000)	# check for R4700
	beqz	v0,96f
	sub 	v0,(0x2300-0x2100)	# check for r5000
	beqz    v0,96f
        sub     v0,(0x2800-0x2300)      # check for rm5271
	bnez	v0,97f
	nop
96:	j	ra
	nop
97:
#endif
	mfc0	v0,C0_WATCHHI
	nop
	j	ra
	nop
	.set	reorder
	END(get_watchhi)

/* 
 * write C0_WATCHLO after clearing reserved bit.  Caller
 * must set WATCHLO_WTRAP or WATCHLO_RTRAP bits in paddr
 * returns the previous value of C0_WATCHLO.
 */
LEAF(set_watchlo)
	.set	noreorder
#ifdef R4600
	mfc0	v0,C0_PRID
	NOP_1_4
	andi	v0,0xFF00
	sub	v0,0x2000
	beqz	v0,96f
	nop
	sub	v0,(0x2100-0x2000)	# check for R4700
	beqz	v0,96f
	sub 	v0,(0x2300-0x2100)	# check for r5000
	beqz    v0,96f
        sub     v0,(0x2800-0x2300)      # check for rm5271
	bnez	v0,97f
	nop
96:	j	ra
	nop
97:
#endif
	mfc0	v0, C0_WATCHLO
	nop
	nop
	.set	reorder
	and	a0, a0, WATCHLO_VALIDMASK
	.set	noreorder
	mtc0	a0,C0_WATCHLO
	nop
	nop
	nop
	nop
	j	ra
	nop
	.set	reorder
	END(set_watchlo)

/* 
 * write C0_WATCHHI after clearing reserved bits.
 * returns the previous value of C0_WATCHHI.
 */
LEAF(set_watchhi)
	.set	noreorder
#ifdef R4600
	mfc0	v0,C0_PRID
	NOP_1_4
	andi	v0,0xFF00
	sub	v0,0x2000
	beqz	v0,96f
	nop
	sub	v0,(0x2100-0x2000)	# check for R4700
	beqz	v0,96f
	sub 	v0,(0x2300-0x2100)	# check for r5000
	beqz    v0,96f
        sub     v0,(0x2800-0x2300)      # check for rm5271
	bnez	v0,97f
	nop
96:	j	ra
	nop
97:
#endif
	mfc0	v0, C0_WATCHHI
	nop
	nop
	.set	reorder
	and	a0, a0, WATCHHI_VALIDMASK
	.set	noreorder
	mtc0	a0,C0_WATCHHI
	nop
	nop
	nop
	nop
	j	ra
	nop
	.set	reorder
	END(set_watchhi)

#if _K64PROM32
LEAF(k64p32_callback_0)
.set	noreorder
	MFC0	(k0,C0_SR)			# jump back to 32 bit mode
	li	k1,~SR_KX
	and	k0,k0,k1
	MTC0	(k0,C0_SR)
	jalr	a0
	MTC0	(zero,C0_PGMASK)		# back to 4k pages
.set	reorder
	END(k64p32_callback_0)

#endif

/*
 * Implementation of fetch-and-add... stolen from kern/ml/atomic_ops.s
 * a0 - address of 32-bit integer to operate on
 * a1 - value to add to integer
 * new value returned in v0
 */
LEAF(atomicAddInt)
XLEAF(atomicAddUint)
#if (_MIPS_SZLONG == 32)
XLEAF(atomicAddLong)
XLEAF(atomicAddUlong)
#endif
	.set 	noreorder

	# Load the old value
1:
	ll	t2,0(a0)
	addu	t1,t2,a1

	# Try to set the new one
	sc	t1,0(a0)	
#ifdef R10000	/* up till 2.6 at least have trouble with some sc sequences */
	nop;nop;nop;nop;nop;nop;nop;nop
	nop;nop;nop;nop;nop;nop;nop;nop
	nop;nop;nop;nop;nop;nop;nop;nop
	nop;nop;nop;nop;nop;nop;nop;nop
#endif
	beq	t1,zero,1b

	# set return value in branch-delay slot
	addu	v0,t2,a1

	# Return to caller
	j	ra
	nop
	.set	reorder
END(atomicAddInt)

#if (_MIPS_SZLONG == 64)
/*
 * Implementation of long-word fetch-and-add.
 * a0 - address of 32- or64-bit integer to operate on
 * a1 - value to add to integer
 * new value returned in v0
 */
LEAF(atomicAddLong)
XLEAF(atomicAddUlong)
	.set 	noreorder

	CACHE_BARRIER			# ensure a0 is good
	AUTO_CACHE_BARRIERS_DISABLE

	# Load the old value
1:
	LONG_LL	t2,0(a0)
	LONG_ADDU t1,t2,a1

	# Try to set the new one
	LONG_SC	t1,0(a0)	
#ifdef R10000	/* up till 2.6 at least have trouble with some sc sequences */
	nop;nop;nop;nop;nop;nop;nop;nop
	nop;nop;nop;nop;nop;nop;nop;nop
	nop;nop;nop;nop;nop;nop;nop;nop
	nop;nop;nop;nop;nop;nop;nop;nop
#endif
	beq	t1,zero,1b

	# set return value in branch-delay slot
	LONG_ADDU v0,t2,a1

	# Return to caller
	j	ra
	nop
	AUTO_CACHE_BARRIERS_ENABLE
	.set	reorder
END(atomicAddLong)
#endif

#endif /* R4000 || R10000 */
