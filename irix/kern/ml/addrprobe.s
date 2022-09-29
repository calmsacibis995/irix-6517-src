/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1996, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include "ml/ml.h"

#ifdef _MEM_PARITY_WAR

ABS(mem_parity_nofault,0xa00002F0)

#define set_mem_parity_nofault(x) \
	sw	x,mem_parity_nofault

#else
#define set_mem_parity_nofault(x) ;
#endif


BADADDRFRM=	FRAMESZ((NARGSAVE+1)*SZREG)	# 4 arg saves plus a ra
RAOFF=		BADADDRFRM-(1*SZREG)
#if !EVEREST && !SN
/*
 * badaddr(addr, len)
 *	check for bus error on read access to addr
 *	len is length of access (1=byte, 2=short, 4=long)
 * On some machines this can generate a bus error interrupt - so we treat
 * it just like a write
 */
NESTED(badaddr, BADADDRFRM, zero)
	PTR_SUBU sp,BADADDRFRM
	REG_S	ra,RAOFF(sp)
	.set 	noreorder
	MFC0_NO_HAZ(t0,C0_SR)
	NOP_0_4
	NOINTS(t1,C0_SR)
	NOP_0_4
	.set	reorder
	clearbuserrm			/* clear bus error */
	.set	noreorder
	AUTO_CACHE_BARRIERS_DISABLE	# t2 is dependent
        PTR_L   t2,VPDA_CURKTHREAD(zero)
	li	v0,NF_BADADDR
        sh      v0,K_NOFAULT(t2)
	AUTO_CACHE_BARRIERS_ENABLE
	set_mem_parity_nofault(v0)

	bne	a1,1,1f
	nop				# BDSLOT
	lb	v0,0(a0)
	b	5f
	nop				# BDSLOT

1:	bne	a1,2,2f
	nop				# BDSLOT
	lh	v0,0(a0)
	b	5f
	nop				# BDSLOT

2:	bne	a1,4,3f
	nop				# BDSLOT
	lw	v0,0(a0)
	b	5f
	nop				# BDSLOT

3:	bne	a1,8,4f
	nop				# BDSLOT
	ld	v0,0(a0)
	b	5f
	nop				# BDSLOT

	.set	reorder

4:	PANIC("baddaddr")
	/* NOTREACHED */

5:
	wbflushm			/* wait for any bus errors */
	.set	noreorder
#if IP32
	ld	t1,CRM_HARDINT|K1BASE
#else
	MFC0_NO_HAZ(t1,C0_CAUSE)
	NOP_1_1
#endif
	and	t1,CAUSE_BERRINTR
	bne	t1,zero,baerror
	nop				# BDSLOT
	.set	reorder
	checkbuserrm(t1,baerror)
	.set	noreorder
	AUTO_CACHE_BARRIERS_DISABLE	# t2 is dependent
        PTR_L   t2,VPDA_CURKTHREAD(zero)
        sh      zero,K_NOFAULT(t2)
	AUTO_CACHE_BARRIERS_ENABLE
	set_mem_parity_nofault(zero)
	MTC0_NO_HAZ(t0,C0_SR)
	NOP_0_4
	REG_L	ra,RAOFF(sp)
	PTR_ADDU sp,BADADDRFRM
	j	ra
	move	v0,zero
	.set	reorder
	END(badaddr)
#endif /* !EVEREST && !SN */

/*
 * badaddr_val(addr, len, pointer)
 * Just like badaddr, but returns the value loaded in *value
 *
 *	check for bus error on read access to addr
 *	len is length of access (1=byte, 2=short, 4=long)
 * On some machines this can generate a bus error interrupt - so we treat
 * it just like a write
 */
#if EVEREST || SN
NESTED(_badaddr_val, BADADDRFRM, zero)
#else
NESTED(badaddr_val, BADADDRFRM, zero)
#endif	/* EVEREST || SN */
	PTR_SUBU sp,BADADDRFRM
	REG_S	ra,RAOFF(sp)
	.set 	noreorder
	MFC0(t0,C0_SR)
	NOP_0_4
	NOINTS(t1,C0_SR)
	NOP_0_4
	.set	reorder
	clearbuserrm			/* clear bus error */
	.set	noreorder
	AUTO_CACHE_BARRIERS_DISABLE	# t2 is dependent
        PTR_L   t2,VPDA_CURKTHREAD(zero)
	beq	t2, zero, 10f	
        li      v0,NF_BADADDR		#BDSLOT
        sh      v0,K_NOFAULT(t2)
	b	11f
	nop				#BDSLOT
	
10:	sw	v0, VPDA_NOFAULT(zero)
11:				
	AUTO_CACHE_BARRIERS_ENABLE
	set_mem_parity_nofault(v0)

	bne	a1,1,1f
	nop				# BDSLOT
	CACHE_BARRIER			# protect store @ a2
	lb	v0,0(a0)
	nop				# LDSLOT
	sb	v0,0(a2)
	b	5f
	nop				# BDSLOT

1:	bne	a1,2,2f
	nop				# BDSLOT
	CACHE_BARRIER			# protect store @ a2
	lh	v0,0(a0)
	nop				# LDSLOT
	sh	v0,0(a2)
	b	5f
	nop				# BDSLOT

2:	bne	a1,4,3f
	nop				# BDSLOT
	CACHE_BARRIER			# protect store @ a2
	lw	v0,0(a0)
	nop				# LDSLOT
	sw	v0,0(a2)
	b	5f
	nop				# BDSLOT

3:	bne	a1,8,4f
	nop				# BDSLOT
	CACHE_BARRIER			# protect store @ a2
	ld	v0,0(a0)
	nop				# LDSLOT
	sd	v0,0(a2)
	b	5f
	nop				# BDSLOT
	.set	reorder

4:	PANIC("baddaddr_val")
	/* NOTREACHED */

5:
	wbflushm			/* wait for any bus errors */
	.set	noreorder
#if IP32
	ld	t1,CRM_HARDINT|K1BASE
#else
	MFC0(t1,C0_CAUSE)
	NOP_1_1
#endif
	and	t1,CAUSE_BERRINTR
	bne	t1,zero,baerror
	nop				# BDSLOT
	.set	reorder
	checkbuserrm(t1,baerror)
	.set	noreorder
	AUTO_CACHE_BARRIERS_DISABLE		# t2 is dependent
        PTR_L   t2,VPDA_CURKTHREAD(zero)
	beq	t2, zero, 20f
	nop				#BDSLOT
	sh	zero,K_NOFAULT(t2)
	b	21f
	nop				#BDSLOT
	
20:	sw	zero,VPDA_NOFAULT(zero)
21:		
	AUTO_CACHE_BARRIERS_ENABLE
	set_mem_parity_nofault(zero)
	MTC0(t0,C0_SR)
	NOP_0_4
	REG_L	ra,RAOFF(sp)
	PTR_ADDU sp,BADADDRFRM
	j	ra
	move	v0,zero
	.set	reorder
#if EVEREST || SN
	END(_badaddr_val)
#else
	END(badaddr_val)
#endif

#if !EVEREST && !SN
/*
 * wbadaddr(addr, len)
 *	check for bus error on write access to addr
 *	len is length of access (1=byte, 2=short, 4=long)
 */
NESTED(wbadaddr, BADADDRFRM, zero)
	PTR_SUBU sp,BADADDRFRM
	REG_S	ra,RAOFF(sp)
	.set 	noreorder
	MFC0_NO_HAZ(t0,C0_SR)
	NOP_0_4
	NOINTS(t1,C0_SR)
	NOP_0_4
	.set 	reorder
	clearbuserrm			/* clear bus error */
	.set	noreorder
	AUTO_CACHE_BARRIERS_DISABLE	# t2 is dependent
        PTR_L   t2,VPDA_CURKTHREAD(zero)
        li      v0,NF_BADADDR
        sh      v0,K_NOFAULT(t2)
	AUTO_CACHE_BARRIERS_ENABLE
	set_mem_parity_nofault(v0)

	bne	a1,1,1f
	nop				# BDSLOT
	CACHE_BARRIER			# protect store to a0
	sb	zero,0(a0)
	b	5f
	nop				# BDSLOT

1:	bne	a1,2,2f
	nop				# BDSLOT
	CACHE_BARRIER			# protect store to a0
	sh	zero,0(a0)
	b	5f
	nop				# BDSLOT

2:	bne	a1,4,3f
	nop				# BDSLOT
	CACHE_BARRIER			# protect store to a0
	sw	zero,0(a0)
	b	5f
	nop				# BDSLOT

3:	bne	a1,8,4f
	nop				# BDSLOT
	CACHE_BARRIER			# protect store to a0
	sd	zero,0(a0)
	b	5f
	nop				# BDSLOT

	.set	reorder

4:	PANIC("wbaddaddr")
	/* NOTREACHED */

5:
	wbflushm			/* wait for any bus errors */
	wbflushm			/* MC systems need more on write */
	/* since we're doing a write, the VMEbus could time out after
	 * some LONG time. It appears that practically though, we need
	 * only wait for a few mem references (needed on IP4)
	 */
	lw	t2, K1BASE
	.set	noreorder
	nop				# let previous load complete
#if IP32
	ld	t1,CRM_HARDINT|K1BASE
#else
	MFC0_NO_HAZ(t1,C0_CAUSE)
	NOP_1_1
#endif
	and	t1,CAUSE_BERRINTR
	bne	t1,zero,baerror
	nop				# BDSLOT
	.set	reorder
	checkbuserrm(t1,baerror)
	.set	noreorder
	AUTO_CACHE_BARRIERS_DISABLE	# t2 is dependent
        PTR_L   t2,VPDA_CURKTHREAD(zero)
        sh      zero,K_NOFAULT(t2)
	AUTO_CACHE_BARRIERS_ENABLE
	set_mem_parity_nofault(zero)
	MTC0_NO_HAZ(t0,C0_SR)
	NOP_0_4
	REG_L	ra,RAOFF(sp)
	PTR_ADDU sp,BADADDRFRM		# LDSLOT
	j	ra
	move	v0,zero
	.set 	reorder
	END(wbadaddr)

#endif	/* !EVEREST && !SN */
/*
 * wbadaddr_val(addr, len, value)
 *	check for bus error on write access to addr
 *	len is length of access (1=byte, 2=short, 4=long)
 */
#if EVEREST || SN
NESTED(_wbadaddr_val, BADADDRFRM, zero)
#else
NESTED(wbadaddr_val, BADADDRFRM, zero)
#endif	/* EVEREST || SN */
	PTR_SUBU sp,BADADDRFRM
	REG_S	ra,RAOFF(sp)
	.set 	noreorder
	MFC0(t0,C0_SR)
	NOP_0_4
	NOINTS(t1,C0_SR)
	NOP_0_4
	.set 	reorder
	clearbuserrm			/* clear bus error */
	.set	noreorder
	AUTO_CACHE_BARRIERS_DISABLE	# t2 is dependent
        PTR_L   t2,VPDA_CURKTHREAD(zero)
        li      v0,NF_BADADDR
#if SN
	/*	
	 * If we're not being called from a thread, we're being called
	 * from the error handling code and interrupts are disabled.
	 */
	bnez	t2, 1f
	 nop
	sw	v0,VPDA_NOFAULT(zero)
	b	2f
	 nop	
1:	sh      v0,K_NOFAULT(t2)
2:	
#else
        sh      v0,K_NOFAULT(t2)
#endif
	AUTO_CACHE_BARRIERS_ENABLE
	set_mem_parity_nofault(v0)

	bne	a1,1,1f
	nop				# BDSLOT
	CACHE_BARRIER			# protect a0 store
	lb	v0,0(a2)
	nop				# LDSLOT
	sb	v0,0(a0)
	b	5f
	nop				# BDSLOT

1:	bne	a1,2,2f
	nop				# BDSLOT
	CACHE_BARRIER			# protect a0 store
	lh	v0,0(a2)
	nop				# LDSLOT
	sh	v0,0(a0)
	b	5f
	nop				# BDSLOT

2:	bne	a1,4,3f
	nop				# BDSLOT
	CACHE_BARRIER			# protect a0 store
	lw	v0,0(a2)
	nop				# LDSLOT
	sw	v0,0(a0)
	b	5f
	nop				# BDSLOT

3:	bne	a1,8,4f
	nop				# BDSLOT
	CACHE_BARRIER			# protect a0 store
	ld	v0,0(a2)
	nop				# LDSLOT
	sd	v0,0(a0)
	b	5f
	nop				# BDSLOT

	.set	reorder

4:	PANIC("wbaddaddr_val")
	/* NOTREACHED */

5:
	wbflushm			/* wait for any bus errors */
	wbflushm			/* MC systems need more on write */
	/* since we're doing a write, the VMEbus could time out after
	 * some LONG time. It appears that practically though, we need
	 * only wait for a few mem references (needed on IP4)
	 */
	lw	t2, K1BASE
	.set	noreorder
#if IP32
	ld	t1,CRM_HARDINT|K1BASE
#else
	MFC0(t1,C0_CAUSE)
	NOP_1_1
#endif
	and	t1,CAUSE_BERRINTR
	bne	t1,zero,baerror
	nop				# BDSLOT
	.set	reorder
	checkbuserrm(t1,baerror)
	.set	noreorder
	AUTO_CACHE_BARRIERS_DISABLE	# t2 is dependent
        PTR_L   t2,VPDA_CURKTHREAD(zero)
#if SN
	/*	
	 * If we're not being called from a thread, we're being called
	 * from the error handling code and interrupts are disabled.
	 */
	bnez	t2, 1f
	 nop
	sw	zero,VPDA_NOFAULT(zero)
	b	2f
	 nop
1:	sh      zero,K_NOFAULT(t2)
2:
#else
        sh      zero,K_NOFAULT(t2)
#endif
	AUTO_CACHE_BARRIERS_ENABLE
	set_mem_parity_nofault(zero)
	MTC0(t0,C0_SR)
	NOP_0_4
	REG_L	ra,RAOFF(sp)
	PTR_ADDU sp,BADADDRFRM		# LDSLOT
	j	ra
	move	v0,zero
	.set 	reorder
#if EVEREST || SN
	END(_wbadaddr_val)
#else
	END(wbadaddr_val)
#endif

#if !EVEREST
/*
 * earlybadaddr(addr, len)
 *	check for bus error on read access to addr
 *	len is length of access (1=byte, 2=short, 4=long)
 *
 * This routine is exactly the same as badaddr, except it
 * doesn't assume that the u area's been set up yet, so it
 * can be called very early on.
 */
NESTED(earlybadaddr, BADADDRFRM, zero)
	PTR_SUBU sp,BADADDRFRM
	REG_S	ra,RAOFF(sp)
	.set 	noreorder
	MFC0(t0,C0_SR)
	NOP_0_4
	NOINTS(t1,C0_SR)
	NOP_0_4
	.set	reorder
	clearbuserrm			/* clear bus error */
	.set	noreorder
	CACHE_BARRIER			# protect nofault store
	li	v0,NF_EARLYBADADDR
	sw	v0,nofault
	set_mem_parity_nofault(v0)

	bne	a1,1,1f
	nop				# BDSLOT
	lb	v0,0(a0)
	b	4f
	nop				# BDSLOT

1:	bne	a1,2,2f
	nop				# BDSLOT
	lh	v0,0(a0)
	b	4f
	nop				# BDSLOT

2:	bne	a1,4,3f
	nop				# BDSLOT
	lw	v0,0(a0)
	b	4f
	nop				# BDSLOT
	.set	reorder

3:	PANIC("earlybaddaddr")
	/* NOTREACHED */

4:
	wbflushm			/* wait for any bus errors */
	.set	noreorder
#if IP32
	ld	t1,CRM_HARDINT|K1BASE
#else
	MFC0(t1,C0_CAUSE)
	NOP_1_1
#endif
	and	t1,CAUSE_BERRINTR
	bne	t1,zero,earlybaerror
	nop				# BDSLOT
	.set	reorder
	checkbuserrm(t1,earlybaerror)
	.set	noreorder
	CACHE_BARRIER			# protect nofault store
	sw	zero,nofault
	set_mem_parity_nofault(zero)
	MTC0(t0,C0_SR)
	NOP_0_4
	REG_L	ra,RAOFF(sp)
	PTR_ADDU sp,BADADDRFRM
	.set	reorder
	move	v0,zero
	j	ra
	END(earlybadaddr)

/*
 * wbadmemaddr(addr)
 *	check for address error on word access to addr
 *	Assumes addr points to RAM since trap is generated by read-back
 *
 * ***NOTE***
 *	Because this is used during startup, before a user structure
 *	exists, we must use the global "nofault" instead of u+U_NOFAULT
 */
NESTED(wbadmemaddr, BADADDRFRM, zero)
	.set 	noreorder
	MFC0_NO_HAZ(t0,C0_SR)
	NOP_0_4
	NOINTS(t1,C0_SR)
	NOP_0_4
	.set 	reorder
	PTR_SUBU sp,BADADDRFRM
	REG_S	ra,RAOFF(sp)
	.set	noreorder
	CACHE_BARRIER			# protect nofault store
	li	v0,NF_EARLYBADADDR
	sw	v0,nofault
	set_mem_parity_nofault(v0)
	sw	zero,0(a0)		# store first to generate ECC
	lw	v0,0(a0)		# load can cause sync DBE
	.set	reorder
	checkbuserrm(t1,baerror)
	.set	noreorder
	CACHE_BARRIER			# protect nofault store
	sw	zero,nofault
	set_mem_parity_nofault(zero)
	REG_L	ra,RAOFF(sp)
	PTR_ADDU sp,BADADDRFRM
	MTC0_NO_HAZ(t0,C0_SR)
	NOP_0_4
	j	ra
	move	v0,zero			# BDSLOT
	.set 	reorder
	END(wbadmemaddr)
#endif	/* !EVEREST */


/*
 * trap() nofault code comes here on bus errors when nofault == NF_BADADDR
 */
NESTED(baerror, BADADDRFRM, zero)
	clearbuserrm			/* clear bus error */
	.set 	noreorder
	MTC0(t0,C0_SR)
	NOP_0_4
	.set 	noreorder
        PTR_L   t2,VPDA_CURKTHREAD(zero)
	beq	t2, zero, 1f
        li      v0,1			# BDSLOT
	AUTO_CACHE_BARRIERS_DISABLE	# t2 is either zero or a ptr (ok)
        sh      zero,K_NOFAULT(t2)
	AUTO_CACHE_BARRIERS_ENABLE
	b	2f
	nop				# BDSLOT
1:
	sw	zero,VPDA_NOFAULT(zero)
2:			
	.set	reorder
	set_mem_parity_nofault(zero)
	REG_L	ra,RAOFF(sp)
	PTR_ADDU sp,BADADDRFRM
	j	ra
	END(baerror)


/*
 * trap() nofault code comes here on bus errors when nofault == NF_EARLYBADADDR
 * used before u area is set up
 */
NESTED(earlybaerror, BADADDRFRM, zero)
	clearbuserrm			/* clear bus error */
	.set noreorder
	MTC0(t0,C0_SR)
	NOP_0_4
	.set reorder
	li	v0,1
	sw	zero,nofault
	set_mem_parity_nofault(zero)
	REG_L	ra,RAOFF(sp)
	PTR_ADDU sp,BADADDRFRM
	j	ra
	END(earlybaerror)
