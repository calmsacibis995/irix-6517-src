/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1995, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.6 $"

#include "ml/ml.h"

#if R4000 && (! defined(_NO_R4000))

#define	M_EXCEPT	0xfbffffff /* all registers except k0 are saved. */

/*
 * This is the size of a VCE log entry.  It must divide evenly 
 * into a page in order to wrap around nicely.
 */
#define VCELOG_ENTRY_SIZE	0x20

#define VCELOG_EPC		0x0
#define VCELOG_RA		(VCELOG_EPC+(_MIPS_SZPTR/8))
#define VCELOG_BADVADDR 	(VCELOG_RA+(_MIPS_SZPTR/8))
#define VCELOG_SR		(VCELOG_BADVADDR+(_MIPS_SZPTR/8))
#define VCELOG_CAUSE 		(VCELOG_SR+(_MIPS_SZINT/8))

/*
** On R4000 MP systems (EVEREST), we can't use primary cache hit operations, 
** due to an R4000 MP bug.
**
** On IP20 with MC rev level 0, we can't do ???.
*/

	.align	7		/* for performance, minimize # of cachelines */
LEAF(handle_vce)
EXL_EXPORT(locore_exl_12)
	/*
	 * k1 == 0 VCED
	 *    != 0 VCEI
	 */
	.set 	noreorder
	
/*	XLEAF(handle_vcei)	*/
	
	dmfc0	k0,C0_BADVADDR	# use "dmfc0" in case addr was 64-bit addr
	NOP_0_4
	bne	k1,zero,vce_common
	nop

/*	XLEAF(handle_vced)	*/
	
	LI	k1,~3		# word align
	and	k0,k1
    	/* On a VCED STORE, R4000 turns on dirty bit in primary line 
	 * corresponding to the virtual address which we're trying to use.  
	 * We need to invalidate this line in the primary without causing a 
	 * writeback, so we blast the cache tags.
	 */
	MTC0(zero,C0_TAGLO)
	NOP_0_4
	cache	CACH_PD|C_IST,0(k0)
	NOP_0_4
	/* FALL THROUGH */
XLEAF(vce_common)
#ifdef IP20
	lw	k1,mc_rev_level
	nop
	beq	k1,zero,rmi_vcehandler
	nop
#endif	/* IP20 */
	cache	CACH_SD|C_HWBINV,0(k0)
	NOP_0_2
	b	do_vcelog
	nop

#if defined(IP20)
XLEAF(rmi_vcehandler)
	sreg	AT,VPDA_ATSAVE(zero)	# AT used below
	/*
	** RMI is broken in a way that prevents us from doing
	** a hit writeback invalidate.  Therefore, we'll use
	** the standard workaround:  In order to flush the
	** offending cacheline out, we'll read a cache line
	** that hits at the same secondary index.  To obtain 
	** the address that will kick out the offender:
	**
	**	if BADVADDR is a K0SEG address, simply flip
	**	the 4MEG bit in the address and load a byte.
	**	(This works as long as our secondary cache 
	**	size is <= 4Meg, and this is architecturally
	**	guaranteed :-).
	**	
	**	
	**	if BADVADDR is a KUSEG or K2SEG address, probe
	**	tlb and extract the PFN.  Then add on the page
	**	offset bits from BADVADDR and turn it into a
	**	K0SEG address, which can be treated as above.
	**
	**	This must be an in-line handler in order to
	**	avoid kicking out the tlb entry which contains
	**	the information we need, so we don't have many
	**	registers with which to play.
	*/

	mfc0	k0,C0_BADVADDR
	NOP_0_4

	LI	k1,K0BASE
	and	k1,k0
	beq	k1,zero,1f	# Is KUSEG?
	nop

#define SEGMASK 0x60000000
	li	k1,SEGMASK
	and	k1,k0
	beq	k1,zero,havek0	# Is K0SEG?
	nop

1:
	#
	# Faulted on a K2SEG or KUSEG address.
	# Let's look up the physical frame number, in order to
	# convert this address to a K0SEG equivalent.  Note that
	# the address is guaranteed to have a translation, since
	# we have not written over any tlb entries since the VCE
	# error occured.
	#
	# We must avoid changing the process PID out from under 
	# this process.
	#
	DMFC0(k1,C0_TLBHI)		# load badvaddr to TLBHI
	NOP_0_4
	and	k0,TLBHI_VPNMASK	# in order to probe tlb
	and	k1,TLBHI_PIDMASK
	or	k1,k0
	DMTC0(k1,C0_TLBHI)
	NOP_0_4
#ifdef PROBE_WAR
	# The workaround requires that the probe instruction
	# never get into the i-cache. So we execute the instruction
	# uncached, and also surround the instruction with nops
	# that are not executed, which ensures that a cache miss
	# to a nearby instruction will not bring the probe into
	# the i-cache.
	la	k0,1f
	or	k0,SEXT_K1BASE
	.set	noat
	la	AT,2f
	j	k0
	NOP_0_1				# BDSLOT executed cached.
	NOP_0_4				# not executed, req. for inst. alignment
1:
	j	AT			# PROBE in delay
	.set	at
#endif
	c0	C0_PROBE		# probe for address
#ifdef PROBE_WAR
	NOP_0_4				# not executed, req. for inst. alignment
2:
#endif
	NOP_0_4
#if DEBUG
	mfc0	k0,C0_INX
	NOP_0_4
	bge	k0,zero,1f
	nop
	jal	debug
	nop
1:
#endif
	c0	C0_READI
	NOP_0_4

	# 
	# TLBHI may have been corrupted, if we matched on a GLOBAL
	# TLB entry with a different pid.  We need to restore TLBHI.
	#
	DMTC0(k1,C0_TLBHI)
	NOP_0_4

	#
	# In order to properly handle the dual entry R4000 tlb, we
	# need to know if this was an even or odd page.
	#
	mfc0	k0,C0_BADVADDR
	NOP_0_4	
	andi	k1,k0,NBPP
	beq	k1,zero,1f
	nop

	# Odd page number, so look in TLBLO1.
	mfc0	k1,C0_TLBLO_1
	NOP_0_4
	srl	k1,TLBLO_PFNSHIFT	# extract PFN
	sll	k1,MIN_PNUMSHFT
#ifdef IP20
	and	k1,0x7fffff		# get cache address
	or	k1,0x08000000		# add offset to base of memory
#endif
	or	k1,SEXT_K0BASE		# turn into K0SEG addr
	andi	k0,POFFMASK		# grab page offset
	j	havek0
	or	k0,k1			# delay slot

1:
	# Even page number, so look in TLBLO0.
	mfc0	k1,C0_TLBLO_0
	NOP_0_4
	srl	k1,TLBLO_PFNSHIFT	# extract PFN
	sll	k1,MIN_PNUMSHFT
#ifdef IP20
	and	k1,0x7fffff		# get cache address
	or	k1,0x08000000		# add offset to base of memory
#endif
	or	k1,SEXT_K0BASE		# turn into K0SEG addr
	andi	k0,POFFMASK		# grab page offset
	j	havek0
	or	k0,k1			# delay slot
	

havek0:
	/*
	** We now have a K0SEG address corresponding to the
	** faulting address.  Flip the "companion bit" in the K0
	** address in order to evict the cache line that's
	** causing problems.
	**
	** Once we've kicked out the old line, we should
	** invalidate the line we read in.  Otherwise, drivers
	** with assumptions about coherency might get confused.
	*/
#define COMPANION_BIT 0x400000
	li	k1,COMPANION_BIT
	xor	k0,k1,k0

	lb	zero,(k0)	# flush primary and secondary
	nop

	mfc0	k0,C0_BADVADDR
	NOP_0_4
	lb	zero,(k0)	# flush companion
	nop
	lreg	AT,VPDA_ATSAVE(zero)
#endif	/* IP20 */

XLEAF(do_vcelog)
#ifdef DEBUG
	/*
	** Save some registers into VCE log.  Carefully avoid stomping 
	** over AT, since some platforms don't save this register.
	*/
	.set	noat
	NOP_0_4
	LI	k1,VPDA_VCELOG_OFFSET
	lw	k0,0(k1)
	nop
	addiu	k0,k0,VCELOG_ENTRY_SIZE
	and	k0,k0,NBPP-1
	sw	k0,0(k1)
	LI	k1,VPDA_VCELOG
	PTR_L	k1,0(k1)
	beq	k1,zero,nolog
	PTR_ADDU k1,k0
	nop
	DMFC0(k0,C0_EPC)
	NOP_0_4
	PTR_S	k0,VCELOG_EPC(k1)
	PTR_S	ra,VCELOG_RA(k1)
	NOP_0_1
	MFC0(k0,C0_SR)
	NOP_0_4
	INT_S	k0,VCELOG_SR(k1)
	NOP_0_1
	MFC0(k0,C0_CAUSE)
	NOP_0_4
	INT_S	k0,VCELOG_CAUSE(k1)
	DMFC0(k0,C0_BADVADDR)
	NOP_0_4
	PTR_S	k0,VCELOG_BADVADDR(k1)
	nop
nolog:
	/* The following code checks for VCE errors which occured while
	 * accessing a kernel stack address.  Without this we end up getting
	 * a report of a stack overflow which is more difficult to anaylze.
	 */
	DMFC0(k0,C0_BADVADDR)
	NOP_0_4
	LI	k1,KSTACKPAGE
	PTR_SRL	k0,PNUMSHFT
	PTR_SLL	k0,PNUMSHFT
	bne	k0,k1,1f
	NOP_0_4
	DMFC0(a2,C0_BADVADDR)
	NOP_0_4
	DMFC0(a3,C0_EPC)
	SPPANIC("VCE error on kernel stack address: 0x%x, epc: 0x%x")
1:	
#endif /* DEBUG */

#define COUNT_VCES 1
#if COUNT_VCES
XLEAF(vce_count)
	lw	k0,VPDA_VCECOUNT(zero)
	addiu	k0,k0,1
	sw	k0,VPDA_VCECOUNT(zero)
#endif
XLEAF(vce_backtouser)
	NOP_0_4
	eret
EXL_EXPORT(elocore_exl_12)	
	NOP_0_4
	.set 	reorder
	.set	at
	END(handle_vce)
#endif	/* R4000  && (! defined(_NO_R4000)) */
