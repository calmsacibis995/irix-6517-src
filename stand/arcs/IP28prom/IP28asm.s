
#ident "$Revision: 1.4 $"

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <asm.h>
#include <regdef.h>

/* init from prom as T5 sometimes seems to turn these into cache misses,
 * and memory may not be initalized yet.  This is good for larger caches
 * even though the prom is only 512K.  It alias the prom for 2MB.
 */
#define	CACHE_INIT_BASE		PHYS_TO_K0(0x1fc00000)

LEAF(init_icache)
	move	v1,a0
	dsrl	v1,1				# 2 ways
	dli	v0,CACHE_INIT_BASE
	PTR_ADDU	v1,v0

	.set	noreorder
	mtc0	zero,C0_TAGLO
	mtc0	zero,C0_TAGHI
	mtc0	zero,C0_ECC
1:	
	PTR_ADDU	v0,CACHE_ILINE_SIZE
	cache	CACH_PI|C_ISD,-64(v0)		# initialize data array
	cache	CACH_PI|C_ISD,-63(v0)
	cache	CACH_PI|C_ISD,-60(v0)
	cache	CACH_PI|C_ISD,-59(v0)
	cache	CACH_PI|C_ISD,-56(v0)
	cache	CACH_PI|C_ISD,-55(v0)
	cache	CACH_PI|C_ISD,-52(v0)
	cache	CACH_PI|C_ISD,-51(v0)
	cache	CACH_PI|C_ISD,-48(v0)
	cache	CACH_PI|C_ISD,-47(v0)
	cache	CACH_PI|C_ISD,-44(v0)
	cache	CACH_PI|C_ISD,-43(v0)
	cache	CACH_PI|C_ISD,-40(v0)
	cache	CACH_PI|C_ISD,-39(v0)
	cache	CACH_PI|C_ISD,-36(v0)
	cache	CACH_PI|C_ISD,-35(v0)
	cache	CACH_PI|C_ISD,-32(v0)
	cache	CACH_PI|C_ISD,-31(v0)
	cache	CACH_PI|C_ISD,-28(v0)
	cache	CACH_PI|C_ISD,-27(v0)
	cache	CACH_PI|C_ISD,-24(v0)
	cache	CACH_PI|C_ISD,-23(v0)
	cache	CACH_PI|C_ISD,-20(v0)
	cache	CACH_PI|C_ISD,-19(v0)
	cache	CACH_PI|C_ISD,-16(v0)
	cache	CACH_PI|C_ISD,-15(v0)
	cache	CACH_PI|C_ISD,-12(v0)
	cache	CACH_PI|C_ISD,-11(v0)
	cache	CACH_PI|C_ISD,-8(v0)
	cache	CACH_PI|C_ISD,-7(v0)
	cache	CACH_PI|C_ISD,-4(v0)
	bltu	v0,v1,1b
	cache	CACH_PI|C_ISD,-3(v0)		# BDSLOT
	.set	reorder

	j	ra
	END(init_icache)

LEAF(init_dcache)
	move	v1,a0
	dsrl	v1,1				# 2 ways
	dli	v0,CACHE_INIT_BASE
	PTR_ADDU	v1,v0

	.set	noreorder
	mtc0	zero,C0_TAGLO
	mtc0	zero,C0_TAGHI
	mtc0	zero,C0_ECC
1:	
	PTR_ADDU	v0,CACHE_DLINE_SIZE
	cache	CACH_PD|C_ISD,-32(v0)		# initialize data array
	cache	CACH_PD|C_ISD,-31(v0)
	cache	CACH_PD|C_ISD,-28(v0)
	cache	CACH_PD|C_ISD,-27(v0)
	cache	CACH_PD|C_ISD,-24(v0)
	cache	CACH_PD|C_ISD,-23(v0)
	cache	CACH_PD|C_ISD,-20(v0)
	cache	CACH_PD|C_ISD,-19(v0)
	cache	CACH_PD|C_ISD,-16(v0)
	cache	CACH_PD|C_ISD,-15(v0)
	cache	CACH_PD|C_ISD,-12(v0)
	cache	CACH_PD|C_ISD,-11(v0)
	cache	CACH_PD|C_ISD,-8(v0)
	cache	CACH_PD|C_ISD,-7(v0)
	cache	CACH_PD|C_ISD,-4(v0)
	bltu	v0,v1,1b
	cache	CACH_PD|C_ISD,-3(v0)		# BDSLOT
	.set	reorder

	j	ra
	END(init_dcache)

LEAF(init_scache)
	move	v1,a0
	dsrl	v1,1				# 2 ways
	dli	v0,CACHE_INIT_BASE
	PTR_ADDU	v1,v0

	.set	noreorder
	mtc0	zero,C0_TAGLO
	mtc0	zero,C0_TAGHI
	mtc0	zero,C0_ECC
1:	
	PTR_ADDU	v0,CACHE_SLINE_SIZE
#if (CACHE_SLINE_SIZE == 128)
	cache	CACH_SD|C_ISD,-128(v0)		# inialize data array
	cache	CACH_SD|C_ISD,-127(v0)
	cache	CACH_SD|C_ISD,-112(v0)
	cache	CACH_SD|C_ISD,-111(v0)
	cache	CACH_SD|C_ISD,-96(v0)
	cache	CACH_SD|C_ISD,-95(v0)
	cache	CACH_SD|C_ISD,-80(v0)
	cache	CACH_SD|C_ISD,-79(v0)
#endif	/* CACHE_SLINE_SIZE == 128 */
	cache	CACH_SD|C_ISD,-64(v0)
	cache	CACH_SD|C_ISD,-63(v0)
	cache	CACH_SD|C_ISD,-48(v0)
	cache	CACH_SD|C_ISD,-47(v0)
	cache	CACH_SD|C_ISD,-32(v0)
	cache	CACH_SD|C_ISD,-31(v0)
	cache	CACH_SD|C_ISD,-16(v0)
	bltu	v0,v1,1b
	cache	CACH_SD|C_ISD,-15(v0)		# BDSLOT
	.set	reorder

	j	ra
	END(init_scache)

/* Invalidate all tlb entries */
LEAF(init_tlb)
	.set	noreorder
	dmtc0	zero,C0_TLBLO_0
	dmtc0	zero,C0_TLBLO_1
	move	v1,zero			# C0_TLBHI

	li	v0,TLBPGMASK_MASK
	mtc0	v0,C0_PGMASK

	mtc0	zero,C0_FMMASK

	li	a0,_PAGESZ*2		# VPN increment
	li	v0,NTLBENTRIES-1
1:
	dmtc0	v1,C0_TLBHI
	mtc0	v0,C0_INX
	nop
	c0	C0_WRITEI
	daddu	v1,a0			# next VPN

	bne	v0,zero,1b
	sub	v0,1

	j	ra
	nop                             # BDSLOT
	.set	reorder
	END(init_tlb)
