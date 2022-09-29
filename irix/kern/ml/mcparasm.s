/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1994, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.12 $"

/*
 * IP20/IP22 parity error detection and recovery support
 */

#if defined(IP20) || defined(IP22) || defined(IPMHSIM)

#include "ml/ml.h"

#ifdef _MEM_PARITY_WAR
/*
 * k_machreg_t 
 * tlbgetlo(tlbpid, vaddr) -- find a tlb entry corresponding to vaddr
 * 	same as tlbfind() but doesn't modify SR
 * a0 -- tlbpid -- tlbcontext number to use (0..TLBHI_NPID-1)
 * a1 -- vaddr -- virtual address to map. Can contain offset bits
 *
 * Returns -1 if not in tlb else TLBLO 0 or 1 register
 */
LEAF(tlbgetlo)
	.set	noreorder
	DMFC0(t1,C0_TLBHI)		# save current pid
	.set 	reorder
	sll	a0,TLBHI_PIDSHIFT	# align pid bits for entryhi
	andi	v1,a1,NBPP		# Save low bit of vpn
	and	a1,TLBHI_VPNMASK	# chop any offset bits from vaddr
	or	t2,a0,a1		# vpn/pid ready for entryhi
	.set	noreorder
	DMTC0(t2,C0_TLBHI)		# vpn and pid of new entry
	NOP_1_3				# let tlbhi get through pipe
#if defined(PROBE_WAR)
	LA	ta1,1f
	or	ta1,SEXT_K1BASE
	LA	ta0,2f
	j	ta1
	NOP_0_1				# BDSLOT executed cached.
	NOP_0_4				# not executed, req. for inst. alignment
1:
	j	ta0			# execute PROBE uncached in BDSLOT
#endif
	c0	C0_PROBE		# probe for address
#if defined(PROBE_WAR)
	NOP_0_4				# not executed, req. for inst. alignment
2:
#endif
	NOP_1_4				# let probe write index reg
	MFC0(t3,C0_INX)	
	NOP_1_1				# wait for t3 to be filled
	bltz	t3,1f			# not found
	li	v0,-1			# BDSLOT

	/* found it - return ENTRYLo */
	c0	C0_READI		# read slot
	NOP_1_3				# let readi fill tlblo reg
	beqz	v1,lo0
	nop
	DMFC0(v0,C0_TLBLO_1)
	b	1f
	nop
lo0:
	DMFC0(v0,C0_TLBLO_0)

1:
	DMTC0(t1,C0_TLBHI)		# restore TLBPID
	j	ra
	nop				# BDSLOT
	.set 	reorder
	END(tlbgetlo)

/*
 * These u*_* functions are just like the ones in ml/usercopy.s,
 * except they don't touch u.u_anything.
 */

/*
 * int u_uload_word(caddr_t addr, k_machreg_t *pword)
 */
LEAF(u_uload_word)
	.set noreorder
	ulw	v1,0(a0)
	.set reorder
	sreg	v1,0(a1)
	move	v0,zero
	j	ra
	END(u_uload_word)

/*
 * int u_uload_half(caddr_t addr, k_machreg_t *pword)
 */
LEAF(u_uload_half)
	.set	noreorder
	# 3.3 as gets this wrong (no nop); so expand it# ulh	v1,0(a0)
	.set noat
	lb	v1,0(a0)
	lbu	AT,1(a0)
	sll	v1,v1,8
	or	v1,v1,AT
	.set at
	# end expansion of ulh
	.set	reorder
	sreg	v1,0(a1)
	move	v0,zero
	j	ra
	END(u_uload_half)

/*
 * int u_uload_uhalf(caddr_t addr, k_machreg_t *pword)
 */
LEAF(u_uload_uhalf)
	.set	noreorder
	# 3.3 as gets this wrong (no nop); so expand it# ulhu	v1,0(a0)
	.set noat
	lbu	v1,0(a0)
	lbu	AT,1(a0)
	sll	v1,v1,8
	or	v1,v1,AT
	.set at
	# end expansion of ulhu
	.set	reorder
	sreg	v1,0(a1)
	move	v0,zero
	j	ra
	END(u_uload_uhalf)


/*
 * int u_uload_uword(caddr_t addr, k_machreg_t *pword)
 */
LEAF(u_uload_uword)
	.set noreorder
	ulwu	v1,0(a0)
	.set reorder
	sreg	v1,0(a1)
	move	v0,zero
	j	ra
	END(u_uload_uword)

/*
 * int u_uload_double(caddr_t addr, k_machreg_t *pword)
 */
LEAF(u_uload_double)
	.set	noreorder
	uld	v1,0(a0)
	.set	reorder
	sd	v1,0(a1)
	move	v0,zero
	j	ra
	END(u_uload_double)

/*
 * int u_ustore_double(caddr_t addr, k_machreg_t value)
 */
LEAF(u_ustore_double)
	.set noreorder
#if _MIPS_SIM == _ABIO32
	/* 'value' is a long long, and _ABIO32 passes
	 * long longs in 2 registers. The register pair starts on
	 * an even register, hence the low order word is in reg a3.
	 */
	dsll32	a2,0
	or	a1,a2,a3
#endif

	usd	a1,0(a0)
	.set reorder
	move	v0,zero
	j	ra
	END(u_ustore_double)

/*
 * int u_ustore_word(caddr_t addr, k_machreg_t value)
 */
LEAF(u_ustore_word)
	.set noreorder
#if _MIPS_SIM == _ABIO32
	/* 'value' is a long long, and _ABIO32 passes
	 * long longs in 2 registers. The register pair starts on
	 * an even register, hence the low order word is in reg a3.
	 */
	usw	a3,0(a0)
#else
	usw	a1,0(a0)
#endif
	.set reorder
	move	v0,zero
	j	ra
	END(u_ustore_word)

/*
 * int u_ustore_half(caddr_t addr, k_machreg_t value)
 */
LEAF(u_ustore_half)
	.set noreorder
#if _MIPS_SIM == _ABIO32
	/* 'value' is a long long, and _ABIO32 passes
	 * long longs in 2 registers. The register pair starts on
	 * an even register, hence the low order word is in reg a3.
	 */
	ush	a3,0(a0)
#else
	ush	a1,0(a0)
#endif
	.set reorder
	move	v0,zero
	j	ra
	END(u_ustore_half)

/*
 * int u_uload_word_left(caddr_t addr, k_machreg_t *pword, k_machreg_t reg)
 */
LEAF(u_uload_word_left)
	.set noreorder
#if _MIPS_SIM == _ABIO32
	/* 'value' is a long long, and _ABIO32 passes
	 * long longs in 2 registers. The register pair starts on
	 * an even register, hence the low order word is in reg a3.
	 */
	lwl	a3,0(a0)
	sreg	a3,0(a1)
#else
	lwl	a2,0(a0)
	sreg	a2,0(a1)
#endif
	.set reorder
	move	v0,zero
	j	ra
	END(u_uload_word_left)

/*
 * int u_uload_word_right(caddr_t addr, k_machreg_t *pword, k_machreg_t reg)
 */
LEAF(u_uload_word_right)
	.set noreorder
#if _MIPS_SIM == _ABIO32
	/* 'value' is a long long, and _ABIO32 passes
	 * long longs in 2 registers. The register pair starts on
	 * an even register, hence the low order word is in reg a3.
	 */
	lwr	a3,0(a0)
	sreg	a3,0(a1)
#else
	lwr	a2,0(a0)
	sreg	a2,0(a1)
#endif
	.set reorder
	move	v0,zero
	j	ra
	END(u_uload_word_right)

/*
 * int u_uload_double_left(caddr_t addr, k_machreg_t *pword, k_machreg_t reg)
 */
LEAF(u_uload_double_left)
	.set noreorder
#if _MIPS_SIM == _ABIO32
	/* 'value' is a long long, and _ABIO32 passes
	 * long longs in 2 registers. The register pair starts on
	 * an even register, hence the low order word is in reg a3.
	 */
	dsll32	a2,0
	or	a2,a3
#endif
	ldl	a2,0(a0)
	.set reorder
	sreg	a2,0(a1)
	move	v0,zero
	j	ra
	END(u_uload_double_left)

/*
 * int u_uload_double_right(caddr_t addr, k_machreg_t *pword, k_machreg_t reg)
 */
LEAF(u_uload_double_right)
	.set noreorder
#if _MIPS_SIM == _ABIO32
	/* 'value' is a long long, and _ABIO32 passes
	 * long longs in 2 registers. The register pair starts on
	 * an even register, hence the low order word is in reg a3.
	 */
	dsll32	a2,0
	or	a2,a3
#endif
	ldr	a2,0(a0)
	.set reorder
	sreg	a2,0(a1)
	move	v0,zero
	j	ra
	END(u_uload_double_right)

/*
 * int u_ustore_word_left(caddr_t addr, k_machreg_t value)
 */
LEAF(u_ustore_word_left)
	.set noreorder
#if _MIPS_SIM == _ABIO32
	/* 'value' is a long long, and _ABIO32 passes
	 * long longs in 2 registers. The register pair starts on
	 * an even register, hence the low order word is in reg a3.
	 */
	swl	a3,0(a0)
#else
	swl	a1,0(a0)
#endif
	.set reorder
	move	v0,zero
	j	ra
	END(u_ustore_word_left)

/*
 * int u_ustore_word_right(caddr_t addr, k_machreg_t value)
 */
LEAF(u_ustore_word_right)
	.set noreorder
#if _MIPS_SIM == _ABIO32
	/* 'value' is a long long, and _ABIO32 passes
	 * long longs in 2 registers. The register pair starts on
	 * an even register, hence the low order word is in reg a3.
	 */
	swr	a3,0(a0)
#else
	swr	a1,0(a0)
#endif
	.set reorder
	move	v0,zero
	j	ra
	END(u_ustore_word_right)

/*
 * int u_ustore_double_left(caddr_t addr, k_machreg_t value)
 */
LEAF(u_ustore_double_left)
	.set noreorder
#if _MIPS_SIM == _ABIO32
	/* 'value' is a long long, and _ABIO32 passes
	 * long longs in 2 registers. The register pair starts on
	 * an even register, hence the low order word is in reg a3.
	 */
	dsll32	a2,0
	or	a1,a2,a3
#endif
	sdl	a1,0(a0)
	.set reorder
	move	v0,zero
	j	ra
	END(u_ustore_double_left)

/*
 * int u_ustore_double_right(caddr_t addr, k_machreg_t value)
 */
LEAF(u_ustore_double_right)
	.set noreorder
#if _MIPS_SIM == _ABIO32
	/* 'value' is a long long, and _ABIO32 passes
	 * long longs in 2 registers. The register pair starts on
	 * an even register, hence the low order word is in reg a3.
	 */
	dsll32	a2,0
	or	a1,a2,a3
#endif
	sdr	a1,0(a0)
	.set reorder
	move	v0,zero
	j	ra
	END(u_ustore_double_right)

/*
 * emulate the lwc1 instruction
 * a0 - register number
 * a1 - uncached memory address
 */
#define	LOAD_WORD_FROM_COP1(reg)	jr ra; lwc1 $f/**/reg,0(a1)
LEAF(emulate_lwc1)
	.set	noreorder
	LA	t0,1f
	sll	t1,a0,3
	addu	t0,t1
	j	t0
	nop

1:
	LOAD_WORD_FROM_COP1(0)
	LOAD_WORD_FROM_COP1(1)
	LOAD_WORD_FROM_COP1(2)
	LOAD_WORD_FROM_COP1(3)
	LOAD_WORD_FROM_COP1(4)
	LOAD_WORD_FROM_COP1(5)
	LOAD_WORD_FROM_COP1(6)
	LOAD_WORD_FROM_COP1(7)
	LOAD_WORD_FROM_COP1(8)
	LOAD_WORD_FROM_COP1(9)
	LOAD_WORD_FROM_COP1(10)
	LOAD_WORD_FROM_COP1(11)
	LOAD_WORD_FROM_COP1(12)
	LOAD_WORD_FROM_COP1(13)
	LOAD_WORD_FROM_COP1(14)
	LOAD_WORD_FROM_COP1(15)
	LOAD_WORD_FROM_COP1(16)
	LOAD_WORD_FROM_COP1(17)
	LOAD_WORD_FROM_COP1(18)
	LOAD_WORD_FROM_COP1(19)
	LOAD_WORD_FROM_COP1(20)
	LOAD_WORD_FROM_COP1(21)
	LOAD_WORD_FROM_COP1(22)
	LOAD_WORD_FROM_COP1(23)
	LOAD_WORD_FROM_COP1(24)
	LOAD_WORD_FROM_COP1(25)
	LOAD_WORD_FROM_COP1(26)
	LOAD_WORD_FROM_COP1(27)
	LOAD_WORD_FROM_COP1(28)
	LOAD_WORD_FROM_COP1(29)
	LOAD_WORD_FROM_COP1(30)
	LOAD_WORD_FROM_COP1(31)
	.set	reorder
	END(emulate_lwc1)

/*
 * emulate the ldc1 instruction
 * a0 - register number
 * a1 - uncached memory address
 */
#define	LOAD_DOUBLEWORD_FROM_COP1(reg)	jr ra; ldc1 $f/**/reg,0(a1)
LEAF(emulate_ldc1)
	.set	noreorder
	LA	t0,1f
	sll	t1,a0,2
	addu	t0,t1
	j	t0
	nop

1:
	LOAD_DOUBLEWORD_FROM_COP1(0)
	LOAD_DOUBLEWORD_FROM_COP1(2)
	LOAD_DOUBLEWORD_FROM_COP1(4)
	LOAD_DOUBLEWORD_FROM_COP1(6)
	LOAD_DOUBLEWORD_FROM_COP1(8)
	LOAD_DOUBLEWORD_FROM_COP1(10)
	LOAD_DOUBLEWORD_FROM_COP1(12)
	LOAD_DOUBLEWORD_FROM_COP1(14)
	LOAD_DOUBLEWORD_FROM_COP1(16)
	LOAD_DOUBLEWORD_FROM_COP1(18)
	LOAD_DOUBLEWORD_FROM_COP1(20)
	LOAD_DOUBLEWORD_FROM_COP1(22)
	LOAD_DOUBLEWORD_FROM_COP1(24)
	LOAD_DOUBLEWORD_FROM_COP1(26)
	LOAD_DOUBLEWORD_FROM_COP1(28)
	LOAD_DOUBLEWORD_FROM_COP1(30)
	.set	reorder
	END(emulate_ldc1)

/*
 * emulate the swc1 instruction
 * a0 - register number
 * a1 - uncached memory address
 */
#define	STORE_WORD_TO_COP1(reg)	jr ra; swc1 $f/**/reg,0(a1)
LEAF(emulate_swc1)
	.set	noreorder
	LA	t0,1f
	sll	t1,a0,3
	addu	t0,t1
	j	t0
	nop

1:
	STORE_WORD_TO_COP1(0)
	STORE_WORD_TO_COP1(1)
	STORE_WORD_TO_COP1(2)
	STORE_WORD_TO_COP1(3)
	STORE_WORD_TO_COP1(4)
	STORE_WORD_TO_COP1(5)
	STORE_WORD_TO_COP1(6)
	STORE_WORD_TO_COP1(7)
	STORE_WORD_TO_COP1(8)
	STORE_WORD_TO_COP1(9)
	STORE_WORD_TO_COP1(10)
	STORE_WORD_TO_COP1(11)
	STORE_WORD_TO_COP1(12)
	STORE_WORD_TO_COP1(13)
	STORE_WORD_TO_COP1(14)
	STORE_WORD_TO_COP1(15)
	STORE_WORD_TO_COP1(16)
	STORE_WORD_TO_COP1(17)
	STORE_WORD_TO_COP1(18)
	STORE_WORD_TO_COP1(19)
	STORE_WORD_TO_COP1(20)
	STORE_WORD_TO_COP1(21)
	STORE_WORD_TO_COP1(22)
	STORE_WORD_TO_COP1(23)
	STORE_WORD_TO_COP1(24)
	STORE_WORD_TO_COP1(25)
	STORE_WORD_TO_COP1(26)
	STORE_WORD_TO_COP1(27)
	STORE_WORD_TO_COP1(28)
	STORE_WORD_TO_COP1(29)
	STORE_WORD_TO_COP1(30)
	STORE_WORD_TO_COP1(31)
	.set	reorder
	END(emulate_swc1)

/*
 * emulate the sdc1 instruction
 * a0 - register number
 * a1 - uncached memory address
 */
#define	STORE_DOUBLEWORD_TO_COP1(reg)	jr ra; sdc1 $f/**/reg,0(a1)
LEAF(emulate_sdc1)
	.set	noreorder
	LA	t0,1f
	sll	t1,a0,2
	addu	t0,t1
	j	t0
	nop

1:
	STORE_DOUBLEWORD_TO_COP1(0)
	STORE_DOUBLEWORD_TO_COP1(2)
	STORE_DOUBLEWORD_TO_COP1(4)
	STORE_DOUBLEWORD_TO_COP1(6)
	STORE_DOUBLEWORD_TO_COP1(8)
	STORE_DOUBLEWORD_TO_COP1(10)
	STORE_DOUBLEWORD_TO_COP1(12)
	STORE_DOUBLEWORD_TO_COP1(14)
	STORE_DOUBLEWORD_TO_COP1(16)
	STORE_DOUBLEWORD_TO_COP1(18)
	STORE_DOUBLEWORD_TO_COP1(20)
	STORE_DOUBLEWORD_TO_COP1(22)
	STORE_DOUBLEWORD_TO_COP1(24)
	STORE_DOUBLEWORD_TO_COP1(26)
	STORE_DOUBLEWORD_TO_COP1(28)
	STORE_DOUBLEWORD_TO_COP1(30)
	.set	reorder
	END(emulate_sdc1)

/* parwbad --
 *	write a byte/halfword/word with bad parity
 *	a0 - address to be written
 *	a1 - value to be written
 *	a2 - size of value to be written
 *	This is a level 0 routine (C-callable).
 *
 *	void parwbad(addr,value,size);
 */

#define CPUCTRL0	0x1fa00004
#define CTRL0_BAD_PAR	0x02000000
#define CTRL0_RFE	0x00000010
#define	MEMLOCK		0x1fa0010c
#define	K1_BASE		_S_EXT_(0xa0000000)
#define	TO_PHYS_MASK	0x1fffffff	/* won't work if kernel text is in */
					/* high memory */

	.text
LEAF(parwbad)
	.set	noreorder
	MFC0(a3,C0_SR)		/* disable all interrupts */
	NOP_0_1
	and	ta1,a3,SR_EXL|SR_ERL
	bnez	ta1,3f
	nop
	MTC0(zero,C0_SR)
	NOP_0_1
3:
	.set	reorder
	LA	t0,1f			/* run uncached */
	and	t0,t0,TO_PHYS_MASK
	or	t0,t0,K1_BASE
	j	t0
1:
	li	t0,K1_BASE|CPUCTRL0
	lw	t1,0(t0)
	or	t2,t1,CTRL0_BAD_PAR
	and	t2,~CTRL0_RFE
	sw	t2,0(t0)
	li	t3,K1_BASE|MEMLOCK
	li	ta0,1
	sw	zero,0(t3)
	wbflushm

	.set	noreorder
	MFC0(ta1,C0_SR)		/* run kernel in 64-bit mode temporary */
	NOP_0_1
	or	ta1,SR_KX
	MTC0(ta1,C0_SR)
	.set	reorder

	li	ta1,0x9
	dsll32	ta1,28		/* 0x9000000000000000 */
	or	a0,ta1

	bne	a2,1,1f
	wbflushm
	sb	a1,0(a0)
	j	baddone

1: 	bne	a2,2,2f
	wbflushm
	sh	a1,0(a0)
	j	baddone

2:	wbflushm
	sw	a1,0(a0)
	j	baddone

baddone:
	sw	ta0,0(t3)
	sw	t1,0(t0)
	wbflushm
	.set	noreorder
	MTC0(a3,C0_SR)
	NOP_0_1
	.set	reorder
	j	ra
	END(parwbad)

LEAF(touch_bad_cacheline)
	lb	t0,0(a0)
	.set noreorder
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	.set reorder
	lb	t0,16(a0)
	j	ra
	END(touch_bad_cacheline)

LEAF(execute_bad_cacheline)
	.set noreorder
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	.set reorder
	j ra
	END(execute_bad_cacheline)

/*
 * read the location specified by the physical address through uncached
 * space.  do it in 64-bit mode since the specified address may be >= 512M
 */
LEAF(r_phys_word)
	.set	noreorder
	MFC0(a1,C0_SR)
	or	a2,a1,SR_KX		/* run in 64-bit mode temporary */
	MTC0(a2,C0_SR)
	.set	reorder

	li	a2,0x9
	dsll32	a2,28			/* K1BASE != 0x9000000000000000 */
	or	a2,a0
	lwu	v0,0(a2)

	.set	noreorder
	MTC0(a1,C0_SR)
	.set	reorder
	j	ra
	END(r_phys_word)

#define	CPUCTRL0_MPR		0x00000040
#define	CPUCTRL0_R4K_CHK_PAR_N	0x04000000
/*
 * read the location specified by the physical address through uncached
 * space.  do it in 64-bit mode since the specified address may be >= 512M.
 * no hardware error checking
 */
LEAF(r_phys_byte_nofault)
	.set	noreorder
	MFC0(a1,C0_SR)
	or	a2,a1,SR_KX		/* run in 64-bit mode temporary */
	MTC0(a2,C0_SR)
	.set	reorder

	li	a2,0x9
	dsll32	a2,28			/* K1_BASE != 0x9000000000000000 */
	or	a2,a0

	/* disable error checking by MC and the R4K */
	li	t0,K1_BASE|CPUCTRL0
	lw	t1,0(t0)
	and	t2,t1,~CPUCTRL0_MPR		/* no error checking in MC */
	or	t2,CPUCTRL0_R4K_CHK_PAR_N	/* no error checking in R4K */
	sw	t2,0(t0)
	lbu	v0,0(a2)
	sw	t1,0(t0)

	.set	noreorder
	MTC0(a1,C0_SR)
	.set	reorder
	j	ra
	END(r_phys_byte_nofault)

LEAF(ecc_restore_specials)
	lreg	a0,EF_TAGLO(k1)
	lreg	a1,EF_TAGHI(k1)
	lreg	a3,EF_ECC(k1)
	.set	noreorder
	mtc0	a0,C0_TAGLO
	mtc0	a1,C0_TAGHI
	mtc0	a3,C0_ECC
	.set reorder
	lreg	a0,EF_BADVADDR(k1)
	lreg	a1,EF_TLBHI(k1)
	lreg	a2,EF_TLBLO_0(k1)
	lreg	a3,EF_TLBLO_1(k1)
	.set	noreorder
	DMTC0(a0,C0_BADVADDR)
	DMTC0(a1,C0_TLBHI)
	TLBLO_FIX_250MHz(C0_TLBLO_0)	# 250MHz R4K workaround
	DMTC0(a2,C0_TLBLO_0)
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	DMTC0(a3,C0_TLBLO_1)
	.set reorder
	lreg	a1,EF_ERROR_EPC(k1)
	lreg	a2,EF_CAUSE(k1)
	lreg	a3,EF_EPC(k1)
	.set	noreorder
	DMTC0(a1,C0_ERROR_EPC)
	mtc0	a2,C0_CAUSE
	DMTC0(a3,C0_EPC)
	.set	reorder
	j	ra
	END(ecc_restore_specials)

LEAF(ecc_map_uarea)
	.set	noreorder
	.set	noat
	/*
	 * wire in process kernel stack
	 */
	SVSLOTS
#if TLBKSLOTS == 1
	LI	k0, KSTACKPAGE		# BDSLOT
	DMFC0(AT, C0_TLBHI)		# save pid
	DMTC0(k0, C0_TLBHI)		# set virtual page number
	li	k0, UKSTKTLBINDEX	# base of wired entries
	mtc0	k0, C0_INX		# set index to wired entry
	# lw	k0, VPDA_UPGLO(zero)
	# nop
	li	k0, TLBLO_G
	/* TLBLO_FIX_HWBITS(k0) */
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	mtc0	k0, C0_TLBLO_1		# LDSLOT set pfn and access bits

	# and	k0, TLBLO_V		# NOP SLOT check valid bit 
	# beq	k0, zero, jmp_baduk	# panic if not valid

	/* now do kernel stack */
	lw	k0, VPDA_UKSTKLO(zero)
	nop
	TLBLO_FIX_HWBITS(k0)
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	mtc0	k0, C0_TLBLO		# LDSLOT set pfn and access bits
	and	k0, TLBLO_V		# NOP SLOT check valid bit 
	c0	C0_WRITEI		# write entry
	beq	k0, zero, jmp_baduk	# panic if not valid
	DMTC0(AT, C0_TLBHI)		# BDSLOT restore pid
#elif TLBKSLOTS == 0
	/* KERNELSTACK and PDA share a TLB entry */
	LI	k0, KSTACKPAGE		# BDSLOT
	DMFC0(AT, C0_TLBHI)		# save pid
	DMTC0(k0, C0_TLBHI)		# set virtual page number
	li	k0, PDATLBINDEX		# base of wired entries
	mtc0	k0, C0_INX		# set index to wired entry
	lw	k0, VPDA_UKSTKLO(zero)
	TLBLO_FIX_HWBITS(k0)
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	mtc0	k0, C0_TLBLO		# LDSLOT set pfn and access bits
	and	k0, TLBLO_V		# NOP SLOT check valid bit 
	beq	k0, zero, jmp_baduk	# panic if not valid

	/* now do PDA page */
	lw	k0, VPDA_PDALO(zero)
	TLBLO_FIX_HWBITS(k0)
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	mtc0	k0, C0_TLBLO_1		# set pfn and access bits
	and	k0, TLBLO_V		# NOP SLOT check valid bit 
	c0	C0_WRITEI		# write entry
	beq	k0, zero, jmp_baduk	# panic if not valid
	DMTC0(AT, C0_TLBHI)		# BDSLOT restore pid
#else
<<bomb -- must be 0 or 1 >>
#endif	/* TLBKSLOTS == 1 || TLBKSLOTS == 0 */
	.set	at
	.set	reorder
	j	ra

jmp_baduk:
	j	_baduk
	/* NOTREACHED */
	END(ecc_map_uarea)

#endif /* _MEM_PARITY_WAR */

#endif /* IP20 || IP22 */
