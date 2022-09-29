/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.5 $"

#include "ml/IP32_ml.h"
#include <sys/sbd.h>
#if	IP19
#include "sys/EVEREST/evmp.h"		/* Needed for MPCONF definitions */
#include "sys/loaddrs.h"
#endif
#ifdef R10000
#include "R10kcache.h"
#endif /* R10000 */

/* 
 * _R4600SC_LAZY is defined to assure that a0 and a1
 * are unmodified in all of the code paths which result in a branch to
 * _r4600sc_index_inval() or _r4600sc_inval_cache().  During this time
 * the original values of a0 and a1 will be stored in a2 and a3 and the
 * called routines will expect their two arguments to be passed in a2
 * and a3.
 */
/* #define _R4600SC_LAZY */
/* #define R4600SC_STATS */
	
/*
 * Cache flushing, cache sizing routines.
 *
 * The globals icache_size, dcache_size no longer refer to
 * the size of the primary i/d caches, but are the size
 * of the largest cache that must be written back/invalidated:
 * e.g. clean_dcache(K0BASE, dcache_size) for IP17 needs to refer
 * to the size of the 2nd cache. The same call for IP6 needs refer
 * to the size of the primary dcache. This allows code in other
 * directories to just use icache_size, dcache_size.
 *
 * Sizes of specific caches are computed and stored in variables such
 * as picache_size, boot_sdcache_size etc. These sizes are used in the
 * add_to_inventory() calls
 */

#ifdef _STANDALONE

BSS(picache_size, 4)            # bytes of icache
BSS(pdcache_size, 4)            # bytes of dcache
BSS(icache_size, 4)            # bytes of icache
BSS(dcache_size, 4)            # bytes of dcache

BSS(_icache_size, 4)            # bytes of icache
BSS(_dcache_size, 4)            # bytes of dcache
BSS(cachewrback, 4)             # writeback secondary cache?

BSS(boot_sidcache_size, 4)                  # bytes of secondary cache
BSS(_sidcache_size, 4)                  # bytes of secondary cache
BSS(_scache_linesize, 4)                # secondary cache line size
BSS(_scache_linemask, 4)                # secondary cache line mask
BSS(scache_linemask, 4)                # secondary cache line mask

BSS(_icache_linesize, 4)                # primary I cache line size
BSS(_icache_linemask, 4)                # primary I cache line mask

BSS(_dcache_linesize, 4)                # primary D cache line size
BSS(_dcache_linemask, 4)                # primary D cache line mask
BSS(cachecolorsize,4)
BSS(cachecolormask,4)

#ifdef R4600
BSS(_r4600sc_cache_on, 4)               # 0 if not present or off, 1 if on
BSS(_two_set_pcaches, 4)        # 0 if one-set; set offset if two-set
BSS(two_set_pcaches, 4)        # 0 if one-set; set offset if two-set
#ifdef R4600SC	
BSS(_r4600sc_sidcache_size, 4)          # for r4600sc hinv
#endif /* R4600SC */
BSS(_r5000_sidcache_size, 4)            # bytes of secondary cache calculate

#endif /* R4600 */
#endif /* _STANDALONE */


#if R4000 || R10000

#if IP17 || IP19 || IP20 || IP22 || IP25 || IP28 || IP30 || IP32 || IPMHSIM
#if R10000 && (IP32 || IPMHSIM)
#define	SCACHE_LINESIZE		(16*4)
#define SCACHE_LINEMASK		((16*4)-1)
#else /* R10000 && (IP32 || IPMHSIM) */	
#define	SCACHE_LINESIZE		(32*4)
#define SCACHE_LINEMASK		((32*4)-1)
#endif /* R10000 && (IP32 || IPMHSIM) */	

#ifdef BLOCKSIZE_8WORDS
#define	DCACHE_LINESIZE		(8*4)
#define	ICACHE_LINESIZE		(8*4)
#define DCACHE_LINEMASK		((8*4)-1)
#define ICACHE_LINEMASK		((8*4)-1)
#elif R10000
#define	DCACHE_LINESIZE		(8*4)
#define	ICACHE_LINESIZE		(16*4)
#define DCACHE_LINEMASK		((8*4)-1)
#define ICACHE_LINEMASK		((16*4)-1)
#else
#define	DCACHE_LINESIZE		(4*4)
#define	ICACHE_LINESIZE		(4*4)
#define DCACHE_LINEMASK		((4*4)-1)
#define ICACHE_LINEMASK		((4*4)-1)
#endif	/* BLOCKSIZE_8WORDS */

#ifdef R4600
#define	R4600_DCACHE_LINESIZE		(8*4)
#define	R4600_ICACHE_LINESIZE		(8*4)
#define R4600_DCACHE_LINEMASK		((8*4)-1)
#define R4600_ICACHE_LINEMASK		((8*4)-1)
#endif /* R4600 */

#ifdef R4600SC
#define	R4600_SCACHE_LINESIZE	(8*4)
#define R4600_SCACHE_LINEMASK	((8*4)-1)
#define R4600_CCC_BASE		0x80000000
#define R4600_PHYSMASK		0x1fffffff	# convert k0 & k1 to physical
#define R4600_SCACHE_SIZE	0x80000
#define XKPHYS_UNCACHED_BASE	0x9000
#define XKPHYS_UNCACHED_SHIFT	0x20
#define	EEROM			0xbfa00034	# stolen from sys/mc.h
#define CACHESZ_REG		0x11		# where the size is stored
#endif
	
#else
#define SCACHE_LINEMASK		0
#endif /* IP17 || IP19 || IP20 || IP22 || IP25 || IP28 || IP30 || IP32 || IPMHSIM */

#if IP20 || IP22 || IP28 || IP30 || IP32 || IPMHSIM
#define K0_CACHEFLUSHBASE K0_RAMBASE
#else
#define K0_CACHEFLUSHBASE K0BASE
#endif

#ifdef R4600SC
/*
 * _r4600sc_disable_cache()
 *
 * disable the secondary cache on an R4600SC
 * returns 0 if the cache was not previously disabled, 1 if it was.
 *
 * NB:	since this code might be called from the ECC exception handler
 *      it may run either cached or uncached.  Because of this we extract
 *      the virtual address space selector bits from the return address
 *      and or them into the virtual address space selector bits of any
 *      addresses which we must access in order to ensure that we do not
 *      touch the caches if we are running in kseg1.
 */
LEAF(_r4600sc_disable_scache)
	li	a0,0
#ifndef _K64U64
	lui	a1,0xE000
#else
	LI	a1,0xf800000000000000	# assume not in compat space (hack)
#endif
	and	a1,ra			# a1 now contains the VAS selector bits
#ifdef TRITON	
	.set noreorder
	mfc0	t2,C0_PRID
	NOP_0_4
	.set reorder
	andi	t2,C0_IMPMASK
	subu	t2,(C0_IMP_TRITON << C0_IMPSHIFT)
	beqz    t2,4f
        subu    t2,((C0_IMP_RM5271 << C0_IMPSHIFT) - (C0_IMP_TRITON << C0_IMPSHIFT))
        bnez    t2,5f
4:

	.set noreorder
	mfc0	t2,C0_CONFIG
	NOP_0_4
#ifdef TRITON_INDYSC	
	and	t1,t2,CONFIG_TR_SC
	bnez	t1,5f
#endif /* TRITON_INDYSC */
	and	t2,~CONFIG_TR_SE
	mtc0	t2,C0_CONFIG
	NOP_0_4
	.set reorder	
	b	6f		
5:	
#endif /* TRITON */
#if IP22
	LA	v0,_r4600sc_cache_index
	or	v0,a1			# make sure we call in same seg we came
					# from (could be kseg0 or kseg1)
	.set noreorder
	jalr	t9,v0
	nop
	mfc0	t0,C0_SR
	nop
#ifndef _K64U64
	mtc0	zero,C0_SR
#else
	and	ta0,t0,SR_KX
	mtc0	ta0,C0_SR
#endif
	LI	a2,K1BASE
	beql	a1,a2,1f
	li	t1,SR_KX		# BDSLOT
	LA	t1,1f
	cache	CACH_PI|C_FILL, 0(t1)
	nop
	cache	CACH_PI|C_FILL, 32(t1)
	j	1f
	li	t1,SR_KX
	nop
1:	
	.align 6
	mtc0	t1,C0_SR
	nop
	nop
	nop
	nop
	sh	zero, 0(v0)
#ifndef _K64U64
	mtc0	zero,C0_SR
#endif
	nop
	nop
	nop
	nop	
	mtc0	t0,C0_SR
	nop
	.set reorder
#endif /* IP22 */
6:	
	LA	a0,_r4600sc_scache_disabled
	or	a0,a1
	lw	v0, 0(a0)
	li	t0,1
	sw	t0,0(a0)
	j	ra
	END(_r4600sc_disable_scache)
	
#ifdef _MEM_PARITY_WAR	
LEAF(_r4600sc_disable_scache_erl)
	li	a0,0
#ifndef _K64U64
	lui	a1,0xE000
#else
	LI	a1,0xf800000000000000	# assume not in compat space (hack)
#endif
	and	a1,ra			# a1 now contains the VAS selector bits
	LA	v0,_r4600sc_cache_index
	or	v0,a1			# make sure we call in same seg we came
					# from (could be kseg0 or kseg1)
	.set noreorder
	jalr	t9,v0
	nop
	mfc0	t0,C0_SR
	nop
	or	t1,t0,SR_KX
	mtc0	t1,C0_SR
	nop
	nop
	nop
	nop
	sh	zero, 0(v0)
	nop
	nop
	nop
	nop	
	mtc0	t0,C0_SR
	nop
	.set reorder
	lw	v0,_r4600sc_scache_disabled
	li	t0,1
	sw	t0,_r4600sc_scache_disabled
	j	ra
	END(_r4600sc_disable_scache_erl)
#endif /* _MEM_PARITY_WAR */

/*
 * _r4600sc_enable_cache()
 *
 * enable and invalidate the secondary cache on an R4600SC
 *
 * NB:	since this code might be called from the ECC exception handler
 *      it may run either cached or uncached.  Because of this we extract
 *      the virtual address space selector bits from the return address
 *      and or them into the virtual address space selector bits of any
 *      addresses which we must access in order to ensure that we do not
 *      touch the caches if we are running in kseg1.
 */
LEAF(_r4600sc_enable_scache)
	li	a0,0
#ifndef _K64U64
	lui	a1,0xE000
#else
	LI	a1,0xf800000000000000	# assume not in compat space (hack)
#endif
	and	a1,ra			# a1 now contains the VAS selector bits
#ifdef TRITON	
	.set noreorder
	mfc0	t2,C0_PRID
	NOP_0_4
	.set reorder
	andi	t2,C0_IMPMASK
	subu	t2,(C0_IMP_TRITON << C0_IMPSHIFT)
	beqz    t2,4f
        subu    t2,((C0_IMP_RM5271 << C0_IMPSHIFT) - (C0_IMP_TRITON << C0_IMPSHIFT))
        bnez    t2,5f
4:

	.set noreorder
	mfc0	t2,C0_CONFIG
	NOP_0_4
	.set reorder
#ifdef TRITON_INDYSC
	and	ta0,t2,CONFIG_TR_SC
	bnez	ta0,5f
#endif /* TRITON_INDYSC *//
	lw	v1,boot_sidcache_size
	LA	ta0,K0BASE
	and	t3,t2,CONFIG_TR_SE
	bnez	t3,6f
	or	t2,CONFIG_TR_SE
	LA	v0,11f
	or	v0,K1BASE
	LA	t9,6f
	or	t9,a1			# stay in same segment on return
	j	v0			# be sure we are uncached
	
	.set noreorder
11:	
	mfc0	ta1,C0_SR		# disable interrupts
	nop	
	mtc0	zero,C0_SR
	lw	t3,_triton_use_invall	# CACH_SD|C_INVALL available?
	bnez	t3,12f			# --> CACH_SD|C_INVALL is available
	mtc0	t2,C0_CONFIG		# enable scache
	mfc0	ta1,C0_SR
	nop
	mtc0	zero,C0_SR
	nop
	mfc0	ta2,C0_TAGLO
	NOP_0_4
	mtc0	zero,C0_TAGLO
	add	v1,ta0
	add	ta0,0x1000
13:	
	cache	CACH_SD|C_INVPAGE,-0x1000(ta0)
	bne	ta0,v1,13b
	add	ta0,0x1000
	mtc0	ta2,C0_TAGLO
	b	14f
	mtc0	ta1,C0_SR
	
12:		
	NOP_0_4
	cache	CACH_SD|C_INVALL,0(ta0)	# invalidate scache
14:	
	j	t9			# back to previous space
	mtc0	ta1,C0_SR		# with interrupts restored
	.set reorder	

5:	
#endif /* TRITON */
#if IP22
	LA	v0,_r4600sc_cache_index
	or	v0,a1			# make sure we call in same seg we came
					# from (could be kseg0 or kseg1)
	.set noreorder
	jalr	t9,v0
	nop
	mfc0	t0,C0_SR
	nop
#ifndef _K64U64
	mtc0	zero,C0_SR
#else
	li	ta0,SR_KX
	mtc0	ta0,C0_SR
#endif
	LI	a2,K1BASE
	beql	a1,a2,1f
	li	t1,SR_KX		# BDSLOT
	LA	t1,1f
	cache	CACH_PI|C_FILL, 0(t1)
	nop
	cache	CACH_PI|C_FILL, 32(t1)
	j	1f
	li	t1,SR_KX
	nop
1:	
	.align 6
	mtc0	t1,C0_SR
	nop
	nop
	nop
	nop
	sb	zero, 0(v0)
#ifndef _K64U64
	mtc0	zero,C0_SR
#endif
	nop
	nop
	nop
	nop	
	mtc0	t0,C0_SR
	nop
	.set reorder
#endif /* IP22 */
6:	
	LA	a0,_r4600sc_scache_disabled
	or	a0,a1
	sw	zero, 0(a0)
	j	ra
	END(_r4600sc_enable_scache)
	
#ifdef _MEM_PARITY_WAR	
LEAF(_r4600sc_enable_scache_erl)
	li	a0,0
#ifndef _K64U64
	lui	a1,0xE000
#else
	LI	a1,0xf800000000000000	# assume not in compat space (hack)
#endif

	and	a1,ra			# a1 now contains the VAS selector bits
	LA	v0,_r4600sc_cache_index
	or	v0,a1			# make sure we call in same seg we came
					# from (could be kseg0 or kseg1)
	.set noreorder
	jalr	t9,v0
	nop
	mfc0	t0,C0_SR
	nop
	or	t1,t0,SR_KX	
	mtc0	t1,C0_SR
	nop
	nop
	nop
	nop
	sb	zero, 0(v0)
	nop
	nop
	nop
	nop	
	mtc0	t0,C0_SR
	nop
	.set reorder
	lw	v0,_r4600sc_scache_disabled
	sw	zero,_r4600sc_scache_disabled
	j	ra
	END(_r4600sc_enable_scache_erl)
#endif /* _MEM_PARITY_WAR */

/*
 * _r4600sc_vtop(vaddr)
 *
 * a0 has the virtual address on which we wish to do the translation
 * on return v0 contains the 32 bit physical address we will use.
 * t9 contains the return address
 * XXX:  this routine should always be called w/interrupts disabled.
 */
LEAF(_r4600sc_vtop)
	sll	ta1,a0,1
	bgez	a0,3f		# bit 31 == 0 --> KUSEG use TLB
	bltz	ta1,3f		# bit 30 == 0 --> K2SEG use TLB

	# this address is in unmapped space 
	# get rid of the va selector bits 29-31
	sll	v0,a0,3
	srl	v0,v0,3
	j	t9		# to complete the translation

	.set noreorder
3:	dmfc0	ta3,C0_TLBHI		# save current pid
	.set 	reorder
	and	ta2,ta3,TLBHI_PIDMASK

	dsrl32	ta1,ta3,8		# get rid of the old VPN
	dsll32	ta1,8

	and	t3,a0,TLBHI_VPNMASK	# chop any offset bits from vaddr
	or	v0,ta1,ta2		# vpn/pid ready for entryhi
	or	v0,t3,v0

	.set noreorder
	dmtc0	v0,C0_TLBHI		# vpn and pid of new entry
	nop 				# tlbhi get through pipe
	c0	C0_PROBE
	nop
	mfc0	t3,C0_INX	
	nop				# wait for t3 to be filled
	bltz	t3,1f			# not found
	li	v0,-1			# BDSLOT

	c0	C0_READI		# read slot
	nop
	srl	t8,a0,BPCSHIFT		# even or odd page?
	andi	ta2,t8,1
	bnez	ta2,2f

	dmfc0	v0, C0_TLBLO_0		# even page, get EntryLo0
	j	3f
	nop

2:	dmfc0	v0, C0_TLBLO_1		# odd page, get EntryLo1
	nop
	.set reorder

3:	dsrl	v0, 6			# strip off EntryLo bits
	dsll	v0, 12			# shift to align for offset
	li	t3, 0xFFF		# mask for page offset
	and	t3, a0, t3		# get the page offset from vaddr
	or	v0, t3			# add in the offset bits
	
	.set noreorder
1:	dmtc0	ta3,C0_TLBHI		# restore TLBPID
	j	t9
	nop				# BDSLOT
	.set 	reorder
	END(_r4600sc_vtop)

/*
 * _r4600sc_cache_index(paddr)
 *
 * calculate the address to write to flush the line containing
 * the physical address "paddr".
 * note that since bits 4:0 are ignored, the index is contained
 * in bits 18:5
 *
 * The CCC only looks at bits 18:5 when determining which line to
 * invalidate, so we don't have to extract the index bits from
 * the address here -- the hardware is nice to us.
 */
LEAF(_r4600sc_cache_index)
	li	ta3,0x1				# create R4600_CCC_BASE
	dsll	ta3,31				# ...
	or	v0,a0,ta3			# physical address to write

	lui	ta3,XKPHYS_UNCACHED_BASE
	dsll32	ta3,XKPHYS_UNCACHED_SHIFT-0x20	# create 64 bit base address
	or	v0,ta3				# xkphys address to write
	j	t9
	END(_r4600sc_cache_index)


/*
 * _r4600sc_cache_on_test()
 *
 * determine if the r4600sc cache is on and set cache state global variable
 *
 * Assumes code and stack are in cached K0 space, the stack is used for a
 * scratch memory while testing the cache.
 *
 */
SCONTSTFRM=     FRAMESZ(8*SZREG)                # 8 words per cache line
LEAF(_r4600sc_cache_on_test)
        .set    noreorder
        PTR_SUBU sp,SCONTSTFRM          # use stack for cache testing address
        sw      zero,_r4600sc_cache_on
        lw      ta3,_r4600sc_sidcache_size
        nop
        beqz    ta3,99f                 # no scache--state = off?
        nop
#ifndef _K64U64
        lui     t3,0x2000               # address bits mask for K1->K0
        xor     t2,t3,sp
        and     t2,t2,sp                # make sure it pts to K0 space
        srl     t2,6
        sll     t2,6                    # align ptr to cache line boundry
#else
        li      ta0,0x1fffffff          # to phys mask
        and     t2,ta0,sp
        or      t2,K0BASE               # cached
        dsrl    t2,6
        dsll    t2,6                    # align ptr to cache line boundry
        LI      t3,0x1000000000000000   # uncached bit (except compat)
#endif

        mfc0    t0,C0_SR
        nop
#ifdef _K64U64
        li      ta0,SR_KX
        mtc0    ta0,C0_SR
#else
        mtc0    zero,C0_SR
#endif
        nop
        LA      t1,1f
        and     ta0,t3,t1               # check for addr in K0
        bne     ta0,zero,1f             # K1 space, dont fill PI cache
        nop
        cache   CACH_PI|C_FILL, 0(t1)
        nop
        cache   CACH_PI|C_FILL, 32(t1)
        j       1f
        nop
        nop
1:        .align 6
#ifndef _K64U64
        or      ta0,t2,K1BASE           # ptr to same addr in K1 space
#else
        and     ta0,t2,0x1fffffff       # to phys
        or      ta0,K1BASE              # ptr to same addr in K1 space
#endif
        lw      ta1,0(t2)               # read from K0 space to fill caches
        cache   CACH_PD|C_HINV,0(t2)    # invalidate primary
        xori    ta2,ta1,0xffff          # complement lower halfword of data
        sw      ta2,0(ta0)              # write it back to memory thru K1 space
        nop
        lw      ta1,0(t2)               # read addr again
        nop
        beq     ta2,ta1,2f              # if eql got it from memory, cache off
        nop
        li      ta1,0x01
        sw      ta1,_r4600sc_cache_on   # else the cache is on
2:
        nop
        mtc0    t0,C0_SR                # restore status register
        nop

99:

        PTR_ADDU sp,SCONTSTFRM          # restore stack ptr
        .set reorder
        j       ra
        END(_r4600sc_cache_on_test)


/*
 * _r4600sc_limit(current, end)
 *	current -- the next address to be operated on
 *	end     -- the ending address for this entire set of
 *	           cache operations.
 *
 * on exit v0 contains the page offset in bytes which is the 
 * limit for the next chunk of invalidates.
 *
 * if (current page == end page)
 *	// we will do the rest in a singe operation
 *	v0 = (end address - current address)
 * else
 *	// we will do lines up to the end of the current page  
 *	v0 = ((current address + NBPP) & 0xFFFFF000) - current address
 * v0 &= 0xFFFFFFE0
 */
LEAF(_r4600sc_limit)
	srl	v0,a0,BPCSHIFT
	srl	v1,a1,BPCSHIFT
	bne	v0,v1,1f	# are both addresses on the same page?
	PTR_SUBU v0,a1,a0	# yes --> use remaining count
	and	v0, R4600_SCACHE_LINEMASK
	j	t9		# return the count

1:	PTR_ADDU v0,a0,NBPP	# get count to end of page
	srl	v0,BPCSHIFT
	sll	v0,BPCSHIFT	# get rid of offset bits
	PTR_SUBU v0,v0,a0	# count to end of page
	and	v0,R4600_SCACHE_LINEMASK
	j	t9
	END(_r4600sc_limit)

/*
 * _r4600sc_inval_cache(void)
 *
 * invalidate the entire secondary cache if the request is the same size
 * as the secondary cache or larger -- otherwise do a line by line invalidate.
 */
LEAF(_r4600sc_inval_cache)
	lw	t0,_sidcache_size
	beqz	t0,99f			# no secondary cache, --> exit

#ifdef _R4600SC_LAZY
	move	a1,a3			# a1 is the length of this request
#endif

	bltu	a1,t0,_r4600sc_index_inval

	#
	# check to see if the cache is disabled, if so
	# we don't want to do anything since the sb in
	# this routine would have the effect of turning
	# on the scache.
	#
	lw	t0,_r4600sc_scache_disabled 
	bnez	t0,99f

#ifdef TRITON	
	.set noreorder
	mfc0	t2,C0_PRID
	NOP_0_4
	.set reorder
	andi	t2,C0_IMPMASK
	subu	t2,(C0_IMP_TRITON << C0_IMPSHIFT)
	beqz    t2,4f
        subu    t2,((C0_IMP_RM5271 << C0_IMPSHIFT) - (C0_IMP_TRITON << C0_IMPSHIFT))
        bnez    t2,5f
4:

#ifdef TRITON_INDYSC
	.set noreorder
	mfc0	t2,C0_CONFIG
	NOP_0_4
	.set reorder
	and	v1,t2,CONFIG_TR_SC
	bnez	v1,5f
#endif /* TRITON_INDYSC */
	lw	t3,_triton_use_invall	# CACH_SD|C_INVALL available?
	LA	t2,K0BASE
	bnez	t3,12f			# --> CACH_SD|C_INVALL is available
	lw	v1,boot_sidcache_size
	add	v1,t2
	add	t2,0x1000
	.set	noreorder
	mfc0	t1,C0_SR
	nop
	mtc0	zero,C0_SR
	nop
	mfc0	t3,C0_TAGLO
	nop
	mtc0	zero,C0_TAGLO
	NOP_0_4
13:	
	cache	CACH_SD|C_INVPAGE,-0x1000(t2)
	bne	t2,v1,13b
	add	t2,0x1000
	mtc0	t3,C0_TAGLO
	b	99f
	mtc0	t1,C0_SR
	.set	reorder
	
12:	
	.set noreorder
	cache	CACH_SD|C_INVALL,0(t2)
	.set reorder	
	b	99f		
5:	
#endif /* TRITON */
#if IP22
	.set noreorder
	mfc0	ta0,C0_SR
	nop
#ifndef _K64U64
	mtc0	zero,C0_SR
#else
	li	ta0,SR_KX
	mtc0	ta0,C0_SR
#endif
	.set reorder
#ifdef R4600SC_STATS
	lw	t0,_r4600sc_nuke
	addu	t0,1
	sw	t0,_r4600sc_nuke
#endif
	li	a0,0
	LA	v0,_r4600sc_cache_index
	jalr	t9,v0			# get xkphys address to write

	.set noreorder
	LA	t9,1f			# fill icache to try and prevent
	cache	CACH_PI|C_FILL,0(t9)	# taking exceptions while in 
	nop				# KX mode
	cache	CACH_PI|C_FILL,32(t9)
	nop
	j	1f
	li	t9,SR_KX
	.align 6
1:	mtc0	t9,C0_SR
	nop
	nop
	nop
	nop
	sb	zero, 0(v0)
#ifndef _K64U64
	mtc0	zero,C0_SR
#endif
	nop
	nop
	nop
	nop	
	mtc0	ta0, C0_SR
	nop
	nop
	nop
	nop
	.set	reorder
#endif /* IP22 */
99:
	j	ra
	END(_r4600sc_inval_cache)

/*
 * _r4600sc_index_inval(addr, len)
 *
 * arguments arrive in a2 and a3 if _R4600SC_LAZY is defined.
 *
 * register usage:
 *	a0	-- starting address and argument passing
 *	a1	-- length and argument passing
 *	a2	-- value of SR_KX bit
 *	a3	-- scratch
 *	t0	-- virtual starting address (used to get xlation)
 *	t1	-- virtual ending address
 *	t2	-- saved value of C0_SR
 *	t3	-- scratch (used by _r4600sc_vtop as scratch)
 *	ta0	-- scratch 
 *	ta1	-- scratch (used by subroutines)
 *	ta2	-- scratch (used by subroutines)
 *	ta3	-- scratch (used by subroutines)
 *	t8	-- scratch (used by subroutines)
 *	t9	-- return address for subroutine calls
 *	v0	-- address to call & function return values
 *	v0	-- function return values
 *
 * XXX: if the length to be invalidated is a multiple of the linesize
 *	then an extra line is invalidated -- we will check for this
 *	condition and decrement the length by the linesize to prevent
 *	the extra line from being invalidated.
 */
LEAF(_r4600sc_index_inval)
	lw	t0,_sidcache_size
	beqz	t0,99f

	lw	t2,_r4600sc_scache_disabled
	bnez	t2,99f

#ifdef _R4600SC_LAZY
	move	a0,a2
	move	a1,a3
#endif

#ifdef TRITON	
	.set noreorder
	mfc0	t2,C0_PRID
	NOP_0_4
	.set reorder
	andi	t2,C0_IMPMASK
	subu	t2,(C0_IMP_TRITON << C0_IMPSHIFT)
	beqz    t2,4f
        subu    t2,((C0_IMP_RM5271 << C0_IMPSHIFT) - (C0_IMP_TRITON << C0_IMPSHIFT))
        bnez    t2,5f
4:
	
#ifdef TRITON_INDYSC
	.set noreorder
	mfc0	t2,C0_CONFIG
	NOP_0_4
	.set reorder
	and	t0,t2,CONFIG_TR_SC
	bnez	t0,5f
#endif /* TRITON_INDYSC */	
	sltu	t2, t0, a1		# size > scache_size?
	bnez	t2, 6f			# yes-->invalidate all
	move	t0,a1
	addu	t0,a0
	move	t1,a0
	li	t3,(0x1000>>1)
	sltu	t2, t3, a1		# size > 1/2 page size?
	bnez	t2, 9f			# yes-->invalidate page
	and	t1,~0x1F
	addu	t0,0x1F
	and	t0,~0x1F
	addu	t1,0x20
	bgtu	t1,t0,99f
	.set	noreorder
	mfc0	ta1,C0_SR
	nop
	mtc0	zero,C0_SR
	nop
	mfc0	ta2,C0_TAGLO
	NOP_0_4
	mtc0	zero,C0_TAGLO
8:	
	cache	CACH_SD|C_IST,-0x20(t1)
	bne	t1,t0,8b
	addu	t1,0x20
	mtc0	ta2,C0_TAGLO
	b	99f
	mtc0	ta1,C0_SR
	.set	reorder
9:	
	and	t1,~0xFFF
	addu	t0,0xFFF
	and	t0,~0xFFF
	addu	t1,0x1000
	bgtu	t1,t0,99f

	.set	noreorder
	mfc0	ta1,C0_SR
	nop
	mtc0	zero,C0_SR
	nop
	mfc0	ta2,C0_TAGLO
	NOP_0_4
	mtc0	zero,C0_TAGLO
	NOP_0_4
7:		
	cache	CACH_SD|C_INVPAGE,-0x1000(t1)
	bne	t1,t0,7b
	addu	t1,0x1000
	mtc0	ta2,C0_TAGLO
	b	99f
	mtc0	ta1,C0_SR
	.set reorder
6:	
	lw	t3,_triton_use_invall	# CACH_SD|C_INVALL available?
	LA	t1,K0BASE
	bnez	t3,12f			# --> CACH_SD|C_INVALL is available
	lw	v1,boot_sidcache_size
	add	v1,t1
	add	t1,0x1000
	.set	noreorder
	mfc0	ta1,C0_SR
	nop
	mtc0	zero,C0_SR
	nop
	mfc0	ta2,C0_TAGLO
	NOP_0_4
	mtc0	zero,C0_TAGLO
13:	
	cache	CACH_SD|C_INVPAGE,-0x1000(t1)
	bne	t1,v1,13b
	add	t1,0x1000
	mtc0	ta2,C0_TAGLO
	b	99f
	mtc0	ta1,C0_SR
	.set	reorder
	
12:	
	.set noreorder
	cache	CACH_SD|C_INVALL,0(t1)
	.set reorder	
	b	99f
5:	
#endif /* TRITON */
#if IP22
#ifdef R4600SC_STATS
	#
	# we are going to collect some stats here for
	# analyzing performance of this routine.
	# we want counts of the following:
	#	counts over a page
	#	counts under a page
	#	start + len cross a page -- len < 0x1000.
	# 	mapped starting addresses
	# 	unmapped starting addresses
	#	mapped addresses whose ending address is in a different page
	#
	li	t2,0
	.set noreorder
	mfc0	t3,C0_SR
	nop
#ifndef _K64U64
	mtc0	zero,C0_SR
#else
	li	t0,SR_KX
	mtc0	t0,C0_SR
#endif
	.set reorder
	bgtu	a1,0x1000,10f
	lw	t0,_r4600sc_short
	addu	t0,1
	PTR_ADDU t1,a0,a1
	srl	v0,a0,BPCSHIFT
	srl	v1,t1,BPCSHIFT
	bne	v0,v1,1f	# are both addresses on the same page?
	sw	t0,_r4600sc_short
	j	100f

1:	lw	t0,_r4600sc_cross
	addu	t0,1
	sw	t0,_r4600sc_cross
	addu	t2,1	
	j	100f

10:	lw	t0,_r4600sc_long
	addu	t0,1
	sw	t0,_r4600sc_long
	addu	t2,1

100:	sll	ta1,a0,1
	bgez	a0,3f		# bit 31 == 0 --> KUSEG use TLB
	bltz	ta1,3f		# bit 30 == 0 --> K2SEG use TLB

	lw	t0,_r4600sc_unmapped
	addu	t0,1
	sw	t0,_r4600sc_unmapped
	j	1f

3:	lw	t0,_r4600sc_mapped
	addu	t0,1
	sw	t0,_r4600sc_mapped
	beqz	t2,1f
	lw	t0,_r4600sc_mandc
	addu	t0,1
	sw	t0,_r4600sc_mandc

	.set noreorder
1:	mtc0	t3,C0_SR
	nop
	.set reorder
#endif /* R4600SC_STATS */
	
	and	t0,a0,~R4600_SCACHE_LINEMASK
	addu	t1,a0,a1		
	and	t3,t1,~R4600_SCACHE_LINEMASK

	beq	t3,t1,1f	# is the ending address cache line aligned?

	#
	# no --> round up to the start of the first cache line which
	# should *not* be invalidated.
	# 
	PTR_ADDU t1,t3,R4600_SCACHE_LINESIZE

1:	
	li	a2,SR_KX
	.set noreorder
	mfc0	t2,C0_SR
	nop

10:	
	nop				# CP0 hazard for mfc0 at end of loop
	mtc0	zero,C0_SR		# turn off interrupts
	move	a0,t0			# start address for next inner loop
	cache	CACH_PD|C_ILT,0(t0)	# force an address translation
	.set reorder			# w/o doing anything to the line

	LA	v0,_r4600sc_vtop	# get the virtual -> physical
					# translation for this address
	jalr	t9,v0
	move	a3,v0			# save physical address

	move	a1,t1			# limit for entire operation
	LA	v0,_r4600sc_limit	# get limit (byte count) for the
	jalr	t9,v0			# inner loop in this interation of
					# the outer loop

	move	v1,v0			# save the count
	move	a0,a3			# get physical address
	move	a3,v1			# save the count again
	LA	v0,_r4600sc_cache_index	# get the address in xkphys
	jalr	t9,v0			# to write to
	#
	# calculate limit address in xkphys
	# we decrement the ending address by R4600_SCACHE_LINESIZE
	# to ensure that the inner loop will execute only the specified
	# number of times.  In order for this to work, register t1 --
	# the ending address -- must point to the first line *not* to
	# be invalidated.
	#
	daddu	v1,v0			
	dsubu	v1,R4600_SCACHE_LINESIZE

	#
	# v0 --> starting xkphys address
	# v1 --> ending xkphys address
	# t0 --> starting virtual address
	# t1 --> ending virtual address
	# a3 --> count of bytes done in this iteration
	#
	# we now need to prepare for the change to 64 bit addressing
	# mode by filling the cache with the code to be executed with
	# KX addressing mode turned on.
	#
	LA	t3,1f
	.set noreorder
	cache	CACH_PI|C_FILL, 0(t3)
	cache	CACH_PI|C_FILL, 32(t3)
	j	1f
	nop
	.align 6
1:	mtc0	a2,C0_SR		# put the CPU in 64 bit mode
	nop				# we should play with the number
2:	sw	zero, 0(v0)		# invalidate the line
	bltu	v0,v1,2b
	daddu	v0,R4600_SCACHE_LINESIZE 
	mtc0	zero,C0_SR		# leave 64 bit mode
	nop
	PTR_ADDU t0,a3			# we've done this much
	#
	# XXX:	 could this be changed to a "bne t0,t1,10b"?
	#
	bltu	t0,t1,10b		# are we done?
	mtc0	t2,C0_SR
	.set reorder
#endif /* IP22 */
99:
	j	ra
	END(_r4600sc_index_inval)
#endif /* R4600SC */

#if !IP25
/*
 * __dcache_inval(addr, len)
 * invalidate data cache for range of physical addr to addr+len-1
 *
 * We use the HIT_INVALIDATE cache op. Therefore, this requires
 * that 'addr'..'len' be the actual addresses which we want to
 * invalidate in the caches.
 * addr/len are not necessarily cache line aligned. Any lines
 * partially covered by addr/len are written out with the
 * HIT_WRITEBACK_INVALIDATE op.
 *
 * Invalidate lines in both the primary and secondary cache.
 *
 * XXX Assumes 32 word line size for secondary cache.
 * XXX Assumes 32 byte line size for primary data cache.
 * XXX Assumes 4 primary lines per secondary line
 * XXX Assumes that primary linesize == secondary linesize for 4600SC
 */

LEAF(__dcache_inval)
#if IP17
	j	rmi_cacheflush
#else
#ifdef IP20
        lw      t0,mc_rev_level
        bne     t0,zero,1f
        j       rmi_cacheflush
1:
#endif

#ifdef defined(R4600SC) && defined(_R4600SC_LAZY)
	move	a2,a0
	move	a3,a1
#endif
	blez	a1,41f			# if length <= 0, we are done
	lw	ta3,_sidcache_size
#ifdef R4000PC
	beqz	ta3,99f			# no scache--do primary only
#endif

#ifdef R4600SC
	lw	t0, two_set_pcaches	# are we on a 4600SC?
	bnez	t0, 99f
#endif

	sltu	t2, ta3, a1		# size > scache_size?
	bnez	t2, 102f		# yes --> use indexed invalidate

	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid
	PTR_SUBU t1,SCACHE_LINESIZE

	# check and align starting and ending addresses.
	and	t2,t0,SCACHE_LINEMASK
	beq	t2,zero,1f		# starting addr is aligned

	# align the address to the start of the line
	PTR_SUBU t0,t2

	# 'hit writeback invalidate' the secondary line indicated
	# by 'addr'. This also does writeback/invalidates in the primary
	# I and D caches as necessary.
	.set	noreorder
	cache	CACH_SD|C_HWBINV,0(t0)	# writeback + inval

	bgeu	t0,t1,3f		# Out if we are done
	# align addr to the next line
	PTR_ADDU t0,SCACHE_LINESIZE	# BDSLOT
1:
	# check if ending address is cache-line aligned.
	and	t2,t1,SCACHE_LINEMASK
	beq	t2,zero,1f		# ending address is full cache line
	PTR_SUBU t1,t2			# BDSLOT align to start of line
	cache	CACH_SD|C_HWBINV,SCACHE_LINESIZE(t1) # writeback + inval
	PTR_ADDU t1,1
	bgeu	t0,t1,3f
	nop				# BDSLOT

1:	# top of loop
	cache	CACH_SD|C_HINV,0(t0)	# invalidate secondary and primary

	bltu	t0,t1,1b
	PTR_ADDU t0,SCACHE_LINESIZE	# BDSLOT
	.set	reorder
3:
#ifdef R4600SC_DEBUG
	lw	t9, _r4600sc_2nd_flush_error
	or	t9, 1
	sw	t9, _r4600sc_2nd_flush_error
#endif

#if R10000 && !R10000_SPECULATION_WAR
	/* ensure previous cache instructions have graduated */
	.set	noreorder
	cache	CACH_BARRIER,-SCACHE_LINESIZE(t0)
	.set	reorder
#endif

	j	ra

	/* clean scache using index invalidate */
102:	LI	t0, K0_CACHEFLUSHBASE	# set up current address
#ifdef R10000
	srl	ta3,1			# cache is 2 way set associative
#endif
	PTR_ADDU t1, t0, ta3		# set up limit address
	PTR_SUBU t1, SCACHE_LINESIZE	# (base + scache_size - scache_linesize)

	.set	noreorder
3:	cache	CACH_SD|C_IWBINV, 0(t0)	# Invalidate cache line
#ifdef R10000
	cache	CACH_SD|C_IWBINV, 1(t0)	# Invalidate cache line (way 1)
#endif	
	bltu	t0, t1, 3b
	PTR_ADDU t0, SCACHE_LINESIZE	# BDSLOT increase invalidate address
	.set	reorder

#ifdef R4600SC_DEBUG
	lw	t9, _r4600sc_2nd_flush_error
	or	t9, 2
	sw	t9, _r4600sc_2nd_flush_error
#endif

#if R10000 && !R10000_SPECULATION_WAR
	/* ensure previous cache instructions have graduated */
	.set	noreorder
	cache	CACH_BARRIER,-SCACHE_LINESIZE(t0)
	.set	reorder
#endif

	j	ra

#ifdef R4000PC
99:	
	lw	ta3,pdcache_size
	beqz	ta3, 3f			# no dcache --> done

#ifdef R4600
	lw	t3,two_set_pcaches
#endif
	sltu	t2, ta3, a1		# size > dcache_size?
	bnez	t2, 102f		# yes --> use indexed invalidate
#ifdef R4600
	beqz	t3,17f

	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid
	PTR_SUBU t1,R4600_DCACHE_LINESIZE

	PD_REFILL_WAIT(cacheops_refill_0)
		
	# check and align starting and ending addresses.
	and	t2,t0,R4600_DCACHE_LINEMASK
	beq	t2,zero,1f		# starting addr is aligned

	# align the address to the start of the line
	PTR_SUBU t0,t2

	# 'hit writeback invalidate' the primary data line indicated
	# by 'addr'.
	.set	noreorder
	cache	CACH_PD|C_HWBINV,0(t0) # writeback + inval

	bgeu	t0,t1,3f		# Out if we are done
	# align addr to the next line
	PTR_ADDU t0,R4600_DCACHE_LINESIZE	# BDSLOT
1:
	# check if ending address is cache-line aligned.
	and	t2,t1,R4600_DCACHE_LINEMASK
	beq	t2,zero,1f		# ending address is full cache line
	PTR_SUBU t1,t2			# BDSLOT align to start of line
	cache	CACH_PD|C_HWBINV,R4600_DCACHE_LINESIZE(t1) # writeback + inval
	PTR_ADDU t1,1
	bgeu	t0,t1,3f
	nop				# BDSLOT

1:	# top of loop
#ifdef TWO_SET_HINV_WAR
	cache	CACH_PD|C_HWBINV,0(t0)	# invalidate primary
#else
	cache	CACH_PD|C_HINV,0(t0)	# invalidate primary
#endif

	bltu	t0,t1,1b
	PTR_ADDU t0,R4600_DCACHE_LINESIZE
	.set	reorder
3:
#ifdef R4600SC
	b	_r4600sc_index_inval
#else
	j	ra
#endif
	

17:
#endif /* R4600 */
	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid
	PTR_SUBU t1,DCACHE_LINESIZE

	# check and align starting and ending addresses.
	and	t2,t0,DCACHE_LINEMASK
	beq	t2,zero,1f		# starting addr is aligned

	# align the address to the start of the line
	PTR_SUBU t0,t2

	# 'hit writeback invalidate' the primary data line indicated
	# by 'addr'.
	.set	noreorder
	cache	CACH_PD|C_HWBINV,0(t0) # writeback + inval

	bgeu	t0,t1,3f		# Out if we are done
	# align addr to the next line
	PTR_ADDU t0,DCACHE_LINESIZE	# BDSLOT
1:
	# check if ending address is cache-line aligned.
	and	t2,t1,DCACHE_LINEMASK
	beq	t2,zero,1f		# ending address is full cache line
	PTR_SUBU t1,t2			# BDSLOT align to start of line
	cache	CACH_PD|C_HWBINV,DCACHE_LINESIZE(t1) # writeback + inval
	PTR_ADDU t1,1
	bgeu	t0,t1,3f
	nop				# BDSLOT

1:	# top of loop
	cache	CACH_PD|C_HINV,0(t0)	# invalidate primary

	bltu	t0,t1,1b
	PTR_ADDU t0,DCACHE_LINESIZE
	.set	reorder
3:
#ifdef R4600SC_DEBUG
	lw	t9, _r4600sc_2nd_flush_error
	or	t9, (1<<2)
	sw	t9, _r4600sc_2nd_flush_error
#endif

	j	ra

	/* clean dcache using index invalidate */
102:	LI	t0, K0_CACHEFLUSHBASE	# set up current address
#ifdef R4600
	beqz	t3,17f
	PTR_ADDU t1, t0, t3		# set up limit address
	PTR_SUBU t1, R4600_DCACHE_LINESIZE	# (base + scache_size - scache_linesize)
	.set	noreorder
#ifdef _R4600_CACHEOP_WAR
	mfc0	ta0, C0_SR
#endif
4:
#ifdef _R4600_CACHEOP_WAR
	mtc0	zero, C0_SR
	nop
#endif
	cache	CACH_PD|C_IWBINV, 0(t0)	# Invalidate cache line
	xor	t2,t0,t3
	cache	CACH_PD|C_IWBINV, 0(t2)	# Invalidate second line in set
#ifdef _R4600_CACHEOP_WAR
	mtc0	ta0, C0_SR
#endif
	bltu	t0, t1, 4b
	PTR_ADDU t0, R4600_DCACHE_LINESIZE	# BDSLOT increase invalidate address
	.set	reorder
#ifdef R4600SC
	b	_r4600sc_inval_cache
#else
	j	ra
#endif

17:
#endif
	PTR_ADDU t1, t0, ta3		# set up limit address
	PTR_SUBU t1, DCACHE_LINESIZE	# (base + scache_size - scache_linesize)

	.set	noreorder
3:	cache	CACH_PD|C_IWBINV, 0(t0)	# Invalidate cache line
	bltu	t0, t1, 3b
	PTR_ADDU t0, DCACHE_LINESIZE	# BDSLOT increase invalidate address
	.set	reorder

#ifdef R4600SC_DEBUG
	lw	t9, _r4600sc_2nd_flush_error
	or	t9, (1<<3)
	sw	t9, _r4600sc_2nd_flush_error
#endif

	j	ra
#endif /* R4000PC */
#endif	/* IP17 || IP19 */
	END(__dcache_inval)
	
/*
 * __dcache_wb(addr,len)
 * write back data from the primary data and secondary cache.
 *
 * XXX Assumes 32 word line size for secondary cache.
 * XXX Assumes 32 byte line size for primary data cache.
 * XXX Assumes 4 primary lines per secondary line
 * XXX note that since 2ndary cache on 4600SC is write through
 *     nothing needs to be done since data is already written
 *     to memory.  
 */

LEAF(__dcache_wb)
#if IP17
	j	rmi_cacheflush
#else
#if IP20
        lw      t0,mc_rev_level
        bne     t0,zero,1f
        j       rmi_cacheflush
1:
#endif

	blez	a1,41f			# if length <= 0, we are done
	lw	ta3,_sidcache_size
#ifdef R4000PC
#ifdef R4600SC
	lw	t0,two_set_pcaches
	bnez	t0,99f			# scache is writethru--do primary only
#endif
	beqz	ta3,99f			# no scache--do primary only
#endif

	sltu	t2, ta3, a1		# size > scache_size?
	bnez	t2, 102f		# yes --> use indexed invalidate

	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid
	PTR_SUBU t1,SCACHE_LINESIZE

	# align the starting address.
	and	t2,t0,SCACHE_LINEMASK
	PTR_SUBU t0,t2

	.set	noreorder
1:	# top of loop
#ifdef R10000
	cache	CACH_SD|C_HWBINV,0(t0)	# writeback secondary
	nop
	cache	CACH_SD|C_HWBINV,1(t0)	# writeback secondary
#else /* R10000 */
	cache	CACH_SD|C_HWB,0(t0)	# writeback secondary
#endif /* R10000 */

	bltu	t0,t1,1b
	PTR_ADDU t0,SCACHE_LINESIZE
	.set	reorder
3:
#ifdef R4600SC_DEBUG
	lw	t9, _r4600sc_2nd_flush_error
	or	t9, (1<<4)
	sw	t9, _r4600sc_2nd_flush_error
#endif

#if R10000 && !R10000_SPECULATION_WAR
	/* ensure previous cache instructions have graduated */
	.set	noreorder
	cache	CACH_BARRIER,-SCACHE_LINESIZE(t0)
	.set	reorder
#endif

	j	ra

	/* clean scache using index invalidate */
102:	LI	t0, K0_CACHEFLUSHBASE	# set up current address
#ifdef R10000
	srl	ta3,1			# cache is 2 way set associative
#endif
	PTR_ADDU t1, t0, ta3		# set up limit address
	PTR_SUBU t1, SCACHE_LINESIZE	# (base + scache_size - scache_linesize)

	.set	noreorder
3:	cache	CACH_SD|C_IWBINV, 0(t0)	# Invalidate cache line
#ifdef R10000	
	cache	CACH_SD|C_IWBINV, 1(t0)	# Invalidate cache line (way 1)
#endif	
	bltu	t0, t1, 3b
	PTR_ADDU t0, SCACHE_LINESIZE	# BDSLOT increase invalidate address
	.set	reorder

#ifdef R4600SC_DEBUG
	lw	t9, _r4600sc_2nd_flush_error
	or	t9, (1<<5)
	sw	t9, _r4600sc_2nd_flush_error
#endif

#if R10000 && !R10000_SPECULATION_WAR
	/* ensure previous cache instructions have graduated */
	.set	noreorder
	cache	CACH_BARRIER,-SCACHE_LINESIZE(t0)
	.set	reorder
#endif

	j	ra

#ifdef R4000PC
99:	
	lw	ta3,pdcache_size
	beqz	ta3, 3f			# no dcache --> done

#ifdef R4600
	lw	t3,two_set_pcaches
#endif
	sltu	t2, ta3, a1		# size > dcache_size?
	bnez	t2, 102f		# yes --> use indexed invalidate
#ifdef R4600
	beqz	t3,17f

	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid
	PTR_SUBU t1,R4600_DCACHE_LINESIZE

	# check and align starting and ending addresses.
	and	t2,t0,R4600_DCACHE_LINEMASK
	# align the address to the start of the line
	PTR_SUBU t0,t2

	PD_REFILL_WAIT(cacheops_refill_1)
		
	# 'hit writeback' the primary data line indicated
	# by 'addr'.
	.set	noreorder
1:	# top of loop
	cache	CACH_PD|C_HWB,0(t0)	# invalidate primary

	bltu	t0,t1,1b
	PTR_ADDU t0,R4600_DCACHE_LINESIZE
	.set	reorder
3:

	j	ra
17:
#endif /* R4600 */
	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid
	PTR_SUBU t1,DCACHE_LINESIZE

	# check and align starting and ending addresses.
	and	t2,t0,DCACHE_LINEMASK
	# align the address to the start of the line
	PTR_SUBU t0,t2

	# 'hit writeback' the primary data line indicated
	# by 'addr'.
	.set	noreorder
1:	# top of loop
	cache	CACH_PD|C_HWB,0(t0)	# invalidate primary

	bltu	t0,t1,1b
	PTR_ADDU t0,DCACHE_LINESIZE
	.set	reorder
3:
	j	ra

	/* clean dcache using index invalidate */
102:	LI	t0, K0_CACHEFLUSHBASE	# set up current address
#ifdef R4600
	beqz	t3,17f
	PTR_ADDU t1, t0, t3		# set up limit address
	PTR_SUBU t1, R4600_DCACHE_LINESIZE	# (base + scache_size - scache_linesize)
	.set	noreorder
#ifdef _R4600_CACHEOP_WAR
	mfc0	ta0, C0_SR
#endif
3:
#ifdef _R4600_CACHEOP_WAR
	mtc0	zero, C0_SR
	nop
#endif
	cache	CACH_PD|C_IWBINV, 0(t0)	# Invalidate cache line
	xor	t2,t0,t3
	cache	CACH_PD|C_IWBINV, 0(t2)	# Invalidate second line in set
#ifdef _R4600_CACHEOP_WAR
	mtc0	ta0, C0_SR
#endif
	bltu	t0, t1, 3b
	PTR_ADDU t0, R4600_DCACHE_LINESIZE	# BDSLOT increase invalidate address
	.set	reorder

	j	ra

17:
#endif /* R4600 */
	PTR_ADDU t1, t0, ta3		# set up limit address
	PTR_SUBU t1, DCACHE_LINESIZE	# (base + scache_size - scache_linesize)

	.set	noreorder
3:	cache	CACH_PD|C_IWBINV, 0(t0)	# Invalidate cache line
	bltu	t0, t1, 3b
	PTR_ADDU t0, DCACHE_LINESIZE	# BDSLOT increase invalidate address
	.set	reorder

	j	ra
#endif /* R4000PC */
#endif	/* IP17 || IP19 */
	END(__dcache_wb)

/*
 * __dcache_wb_inval(addr, len)
 * writeback-invalidate data cache for range of physical addr to addr+len-1
 *
 * XXX Assumes 32 word line size for secondary cache.
 * XXX Assumes 32 byte line size for primary data cache.
 * XXX Assumes 4 primary lines per secondary line
 * XXX Assumes that primary linesize == secondary linesize for 4600SC
 */

LEAF(__dcache_wb_inval)
#if IP17
	j	rmi_cacheflush
#else
#if IP20
        lw      t0,mc_rev_level
        bne     t0,zero,1f
        j       rmi_cacheflush
1:
#endif

#ifdef defined(R4600SC) && defined(_R4600SC_LAZY)
	move	a2,a0
	move	a3,a1
#endif
	blez	a1,41f			# if length <= 0, we are done
	lw	ta3,_sidcache_size
#ifdef R4000PC
	beqz	ta3,99f			# no scache--do primary only
#endif

#ifdef R4600SC
	lw	t0, two_set_pcaches	# are we on a 4600?
	bnez	t0, 99f
#endif

	sltu	t2, ta3, a1		# size > scache_size?
	bnez	t2, 102f		# yes --> use indexed invalidate

	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid
	PTR_SUBU t1,SCACHE_LINESIZE

	# align the starting address.
	and	t2,t0,SCACHE_LINEMASK
	PTR_SUBU t0,t2
	
	.set	noreorder
1:	# top of loop
	cache	CACH_SD|C_HWBINV,0(t0)	# writeback secondary
	bltu	t0,t1,1b
	PTR_ADDU t0,SCACHE_LINESIZE
	.set	reorder
3:
#ifdef R4600SC_DEBUG
	lw	t9, _r4600sc_2nd_flush_error
	or	t9, (1<<10)
	sw	t9, _r4600sc_2nd_flush_error
#endif

#if R10000 && !R10000_SPECULATION_WAR
	/* ensure previous cache instructions have graduated */
	.set	noreorder
	cache	CACH_BARRIER,-SCACHE_LINESIZE(t0)
	.set	reorder
#endif

	j	ra

	/* clean scache using index invalidate */
102:	LI	t0, K0_CACHEFLUSHBASE	# set up current address
#ifdef R10000
	srl	ta3,1			# cache is 2 way set associative
#endif
	PTR_ADDU t1, t0, ta3		# set up limit address
	PTR_SUBU t1, SCACHE_LINESIZE	# (base + scache_size - scache_linesize)

	.set	noreorder
3:	cache	CACH_SD|C_IWBINV, 0(t0)	# Invalidate cache line
#ifdef R10000	
	cache	CACH_SD|C_IWBINV, 1(t0)	# Invalidate cache line (way 1)
#endif	
	bltu	t0, t1, 3b
	PTR_ADDU t0, SCACHE_LINESIZE	# BDSLOT increase invalidate address
	.set	reorder

#ifdef R4600SC_DEBUG
	lw	t9, _r4600sc_2nd_flush_error
	or	t9, (1<<11)
	sw	t9, _r4600sc_2nd_flush_error
#endif

#if R10000 && !R10000_SPECULATION_WAR
	/* ensure previous cache instructions have graduated */
	.set	noreorder
	cache	CACH_BARRIER,-SCACHE_LINESIZE(t0)
	.set	reorder
#endif

	j	ra

#ifdef R4000PC
99:	
	lw	ta3,pdcache_size
	beqz	ta3, 3f			# no dcache --> done

#ifdef R4600
	lw	t3,two_set_pcaches
#endif
	sltu	t2, ta3, a1		# size > dcache_size?
	bnez	t2, 102f		# yes --> use indexed invalidate
#ifdef R4600
	beqz	t3,17f
	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid
	beqz	t0,3f			# for prom DEBUG - XXX
	PTR_SUBU t1,R4600_DCACHE_LINESIZE

	# check and align starting and ending addresses.
	and	t2,t0,R4600_DCACHE_LINEMASK
	# align the address to the start of the line
	PTR_SUBU t0,t2

	PD_REFILL_WAIT(cacheops_refill_2)
		
	# 'hit writeback' the primary data line indicated
	# by 'addr'.
	.set	noreorder
1:	# top of loop
	cache	CACH_PD|C_HWBINV,0(t0)	# invalidate primary

	bltu	t0,t1,1b
	PTR_ADDU t0,R4600_DCACHE_LINESIZE
	.set	reorder
3:
#ifdef R4600SC
	b	_r4600sc_index_inval
#else
	j	ra
#endif
17:
#endif /* R4600 */
	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid
	PTR_SUBU t1,DCACHE_LINESIZE

	# check and align starting and ending addresses.
	and	t2,t0,DCACHE_LINEMASK
	# align the address to the start of the line
	PTR_SUBU t0,t2

	# 'hit writeback' the primary data line indicated
	# by 'addr'.
	.set	noreorder
1:	# top of loop
	cache	CACH_PD|C_HWBINV,0(t0)	# invalidate primary

	bltu	t0,t1,1b
	PTR_ADDU t0,DCACHE_LINESIZE
	.set	reorder
3:
#ifdef R4600SC_DEBUG
	lw	t9, _r4600sc_2nd_flush_error
	or	t9, (1<<12)
	sw	t9, _r4600sc_2nd_flush_error
#endif

	j	ra

	/* clean dcache using index invalidate */
102:	LI	t0, K0_CACHEFLUSHBASE	# set up current address
#ifdef R4600
	beqz	t3,17f
	PTR_ADDU t1, t0, t3		# set up limit address
	PTR_SUBU t1, R4600_DCACHE_LINESIZE	# (base + scache_size - scache_linesize)

	.set	noreorder
#ifdef _R4600_CACHEOP_WAR
	mfc0	ta0, C0_SR
#endif
3:
#ifdef _R4600_CACHEOP_WAR
	mtc0	zero, C0_SR
	nop
#endif
	cache	CACH_PD|C_IWBINV, 0(t0)	# Invalidate cache line
	xor	t2,t0,t3
	cache	CACH_PD|C_IWBINV, 0(t2)	# Invalidate cache line
#ifdef _R4600_CACHEOP_WAR
	mtc0	ta0, C0_SR
#endif
	bltu	t0, t1, 3b
	PTR_ADDU t0, R4600_DCACHE_LINESIZE	# BDSLOT increase invalidate address
	.set	reorder

#ifdef R4600SC
	b	_r4600sc_inval_cache
#else
	j	ra
#endif

17:
#endif /* R4600 */
	PTR_ADDU t1, t0, ta3		# set up limit address
	PTR_SUBU t1, DCACHE_LINESIZE	# (base + scache_size - scache_linesize)

	.set	noreorder
3:	cache	CACH_PD|C_IWBINV, 0(t0)	# Invalidate cache line
	bltu	t0, t1, 3b
	PTR_ADDU t0, DCACHE_LINESIZE	# BDSLOT increase invalidate address
	.set	reorder

#ifdef R4600SC_DEBUG
	lw	t9, _r4600sc_2nd_flush_error
	or	t9, (1<<13)
	sw	t9, _r4600sc_2nd_flush_error
#endif

	j	ra
#endif /* R4000PC */
#endif	/* IP17 || IP19 */
	END(__dcache_wb_inval)

/*
 * __icache_inval(addr, len)
 * invalidate instruction cache for range of physical addr to addr+len-1
 *
 * We use the HIT_INVALIDATE cache op. Therefore, this requires
 * that 'addr'..'len' be the actual addresses which we want to
 * invalidate in the caches. (i.e. won't work for user virtual
 * addresses that the kernel converts to K0 before calling.)
 * addr/len are not necessarily cache line aligned.
 *
 * Invalidate lines in both the primary and secondary cache.
 *
 * XXX Assumes 32 word line size for secondary cache.
 * XXX Assumes 32 byte line size for primary instruction cache.
 * XXX Assumes 4 primary lines per secondary line
 */

LEAF(__icache_inval)
#if IP17
	j	rmi_cacheflush
#else
#if IP20
        lw      t0,mc_rev_level
        bne     t0,zero,1f
        j       rmi_cacheflush
1:
#endif

#ifdef defined(R4600SC) && defined(_R4600SC_LAZY)
	move	a2,a0
	move	a3,a1
#endif
	blez	a1,41f			# if length <= 0, we are done
	lw	ta3,_sidcache_size
#ifdef R4000PC
	beqz	ta3,99f			# no scache--do primary only
#endif

#ifdef R4600SC
	lw	t0, two_set_pcaches	# are we on a 4600SC?
	bnez	t0, 99f
#endif

	sltu	t2, ta3, a1		# size > scache_size?
	bnez	t2, 102f		# yes --> use indexed invalidate

	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid
	PTR_SUBU t1,SCACHE_LINESIZE

	# align the starting address.
	and	t2,t0,SCACHE_LINEMASK
	PTR_SUBU t0,t2

	.set	noreorder
1:	# top of loop
	cache	CACH_SD|C_HWBINV,0(t0)	# writeback secondary

	bltu	t0,t1,1b
	PTR_ADDU t0,SCACHE_LINESIZE
	.set	reorder
3:
#ifdef R4600SC_DEBUG
	lw	t9, _r4600sc_2nd_flush_error
	or	t9, (1<<14)
	sw	t9, _r4600sc_2nd_flush_error
#endif

#if R10000 && !R10000_SPECULATION_WAR
	/* ensure previous cache instructions have graduated */
	.set	noreorder
	cache	CACH_BARRIER,-SCACHE_LINESIZE(t0)
	.set	reorder
#endif

41:
	j	ra

	/* clean scache using index invalidate */
102:	LI	t0, K0_CACHEFLUSHBASE	# set up current address
#ifdef R10000
	srl	ta3,1			# cache is 2 way set associative
#endif
	PTR_ADDU t1, t0, ta3		# set up limit address
	PTR_SUBU t1, SCACHE_LINESIZE	# (base + scache_size - scache_linesize)

	.set	noreorder
3:	cache	CACH_SD|C_IWBINV, 0(t0)	# Invalidate cache line
#ifdef R10000	
	cache	CACH_SD|C_IWBINV, 1(t0)	# Invalidate cache line (way 1)
#endif	
	bltu	t0, t1, 3b
	PTR_ADDU t0, SCACHE_LINESIZE	# BDSLOT increase invalidate address
	.set	reorder

#ifdef R4600SC_DEBUG
	lw	t9, _r4600sc_2nd_flush_error
	or	t9, (1<<15)
	sw	t9, _r4600sc_2nd_flush_error
#endif

#if R10000 && !R10000_SPECULATION_WAR
	/* ensure previous cache instructions have graduated */
	.set	noreorder
	cache	CACH_BARRIER,-SCACHE_LINESIZE(t0)
	.set	reorder
#endif

	j	ra

#ifdef R4000PC
	/* for R4000PC, we clean both caches, as assumed by higher levels */
99:	
	lw	ta3,pdcache_size
	beqz	ta3, 3f			# no dcache --> done

#ifdef R4600
	lw	t3,two_set_pcaches
#endif
	sltu	t2, ta3, a1		# size > dcache_size?
	bnez	t2, 102f		# yes --> use indexed invalidate
#ifdef R4600
	beqz	t3,17f
	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,103f		# paranoid
	PTR_SUBU t1,R4600_DCACHE_LINESIZE

	# check and align starting and ending addresses.
	and	t2,t0,R4600_DCACHE_LINEMASK
	# align the address to the start of the line
	PTR_SUBU t0,t2

	PD_REFILL_WAIT(cacheops_refill_3)
		
	# 'hit writeback invalidate' the primary data line indicated
	# by 'addr'.
	.set	noreorder
1:	# top of loop
	cache	CACH_PD|C_HWBINV,0(t0)	# invalidate primary

	bltu	t0,t1,1b
	PTR_ADDU t0,R4600_DCACHE_LINESIZE
	.set	reorder
3:
	b	103f
	

	/* clean dcache using index invalidate */
102:	LI	t0, K0_CACHEFLUSHBASE	# set up current address
	PTR_ADDU t1, t0, t3		# set up limit address
	PTR_SUBU t1, R4600_DCACHE_LINESIZE	# (base + dcache_size - dcache_linesize)

	.set	noreorder
#ifdef _R4600_CACHEOP_WAR
	mfc0	ta0, C0_SR
#endif
3:
#ifdef _R4600_CACHEOP_WAR
	mtc0	zero, C0_SR
	nop
#endif
	cache	CACH_PD|C_IWBINV, 0(t0)	# Invalidate cache line
	xor	t2,t0,t3
	cache	CACH_PD|C_IWBINV, 0(t2)	# Invalidate cache line
#ifdef _R4600_CACHEOP_WAR
	mtc0	ta0, C0_SR
#endif
	bltu	t0, t1, 3b
	PTR_ADDU t0, R4600_DCACHE_LINESIZE	# BDSLOT increase invalidate address
	.set	reorder

103:
	lw	ta3,picache_size
	beqz	ta3, 3f			# no icache --> done

	sltu	t2, ta3, a1		# size > icache_size?
	bnez	t2, 102f		# yes --> use indexed invalidate

	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid
	PTR_SUBU t1,R4600_ICACHE_LINESIZE

	# check and align starting and ending addresses.
	and	t2,t0,R4600_ICACHE_LINEMASK
	# align the address to the start of the line
	PTR_SUBU t0,t2

	# 'hit invalidate' the primary data line indicated
	# by 'addr'.
	.set	noreorder
1:	# top of loop
	cache	CACH_PI|C_HINV,0(t0)	# invalidate primary
	bltu	t0,t1,1b
	PTR_ADDU t0,R4600_ICACHE_LINESIZE
	.set	reorder
3:
#ifdef R4600SC
	b	_r4600sc_index_inval
#else
	j	ra
#endif

	/* clean icache using index invalidate */
102:	LI	t0, K0_CACHEFLUSHBASE	# set up current address
	PTR_ADDU t1, t0, t3		# set up limit address
	PTR_SUBU t1, R4600_ICACHE_LINESIZE	# (base + icache_size - icache_linesize)

	.set	noreorder
#ifdef _R4600_CACHEOP_WAR
	mfc0	ta0, C0_SR
	mtc0	zero, C0_SR
	nop
#endif
3:	cache	CACH_PI|C_IINV,0(t0)	# Invalidate cache line
	xor	t2,t0,t3
	cache	CACH_PI|C_IINV,0(t2)	# Invalidate cache line
	bltu	t0, t1, 3b
	PTR_ADDU t0, R4600_ICACHE_LINESIZE	# BDSLOT increase invalidate address
#ifdef _R4600_CACHEOP_WAR
	mtc0	ta0, C0_SR
	nop
#endif
	.set	reorder

#ifdef R4600SC
	b	_r4600sc_inval_cache
#else
	j	ra
#endif
17:
#endif /* R4600 */
	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,103f		# paranoid
	PTR_SUBU t1,DCACHE_LINESIZE

	# check and align starting and ending addresses.
	and	t2,t0,DCACHE_LINEMASK
	# align the address to the start of the line
	PTR_SUBU t0,t2

	# 'hit writeback invalidate' the primary data line indicated
	# by 'addr'.
	.set	noreorder
1:	# top of loop
	cache	CACH_PD|C_HWBINV,0(t0)	# invalidate primary

	bltu	t0,t1,1b
	PTR_ADDU t0,DCACHE_LINESIZE
	.set	reorder
3:
	b	103f
	

	/* clean dcache using index invalidate */
102:	LI	t0, K0_CACHEFLUSHBASE	# set up current address
	PTR_ADDU t1, t0, ta3		# set up limit address
	PTR_SUBU t1, DCACHE_LINESIZE	# (base + dcache_size - dcache_linesize)

	.set	noreorder
3:	cache	CACH_PD|C_IWBINV, 0(t0)	# Invalidate cache line
	bltu	t0, t1, 3b
	PTR_ADDU t0, DCACHE_LINESIZE	# BDSLOT increase invalidate address
	.set	reorder

103:
	lw	ta3,picache_size
	beqz	ta3, 3f			# no icache --> done

	sltu	t2, ta3, a1		# size > icache_size?
	bnez	t2, 102f		# yes --> use indexed invalidate

	move	t0,a0
	PTR_ADDU t1,a0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid
	PTR_SUBU t1,ICACHE_LINESIZE

	# check and align starting and ending addresses.
	and	t2,t0,ICACHE_LINEMASK
	# align the address to the start of the line
	PTR_SUBU t0,t2

	# 'hit invalidate' the primary data line indicated
	# by 'addr'.
	.set	noreorder
1:	# top of loop
	cache	CACH_PI|C_HINV,0(t0)	# invalidate primary

	bltu	t0,t1,1b
	PTR_ADDU t0,ICACHE_LINESIZE
	.set	reorder
3:
#ifdef R4600SC_DEBUG
	lw	t9, _r4600sc_2nd_flush_error
	or	t9, (1<<16)
	sw	t9, _r4600sc_2nd_flush_error
#endif

	j	ra

	/* clean icache using index invalidate */
102:	LI	t0, K0_CACHEFLUSHBASE	# set up current address
	PTR_ADDU t1, t0, ta3		# set up limit address
	PTR_SUBU t1, ICACHE_LINESIZE	# (base + icache_size - icache_linesize)

	.set	noreorder
3:	cache	CACH_PI|C_IINV,0(t0)	# Invalidate cache line
	bltu	t0, t1, 3b
	PTR_ADDU t0, ICACHE_LINESIZE	# BDSLOT increase invalidate address
	.set	reorder

#ifdef R4600SC_DEBUG
	lw	t9, _r4600sc_2nd_flush_error
	or	t9, (1<<17)
	sw	t9, _r4600sc_2nd_flush_error
#endif

	j	ra
#endif /* R4000PC */
#endif /* IP17 */
	END(__icache_inval)
/*
 * __cache_wb_inval(addr, len)
 * 
 * Uses the INDEX_INVALIDATE and INDEX_WRITEBACK_INVALIDATE
 * cacheops. Should be called with K0 addresses, to avoid
 * the tlb translation (and tlb miss fault) overhead.
 *
 * XXX Assumes 32 word line size for secondary cache.
 * XXX Assumes 32 byte line size for primary I/D caches.
 */
LEAF(__cache_wb_inval)
XLEAF(clear_cache)
#if IP17
	j	rmi_cacheflush
#else
#if IP20
        lw      t0,mc_rev_level
        bne     t0,zero,1f
        j       rmi_cacheflush
1:
#endif

#ifdef defined(R4600SC) && defined(_R4600SC_LAZY)
	move	a2,a0
	move	a3,a1
#endif
	blez	a1, 41f			# if we don't have anything return
#ifdef R4600
	lw	ta2,two_set_pcaches	# are we on a 4600?
	bnez	ta2,1f			# yes --> we may have a secondary
					# cache, but we will deal with that
					# later, after the primaries have
					# been invalidated.
#endif
	lw	ta0,_sidcache_size

#ifdef R4000PC
	/* do we have an scache? */
	bnez	ta0,101f		# yes --> just use scache operations 

#ifdef R4600
1:
#endif
	lw	ta3, pdcache_size
	lw	v0, picache_size
#ifdef R4600
	beqz	ta2,17f

	/* do dcache */
	beqz	ta3, 31f		# no dcache --> check icache

	/* clean dcache using indexed invalidate */
	move	t0, a0			# set up current address
 
	PTR_ADDU t1, t0, a1		# set up limit address
#ifdef R4600SC
	bleu	a1,ta2,21f
	PTR_ADDU t1, t0, ta2
21:	
#endif /* R4600SC */
	PTR_SUBU t1, R4600_DCACHE_LINESIZE	# (base + count - R4600_DCACHE_LINESIZE)

	and t3, t0, R4600_DCACHE_LINEMASK	# align start to dcache line
	PTR_SUBU t0, t3			# pull off extra

	.set	noreorder
#ifdef _R4600_CACHEOP_WAR
	mfc0	ta0, C0_SR
#endif
2:
#ifdef _R4600_CACHEOP_WAR
	mtc0	zero, C0_SR
	nop
#endif
	cache	CACH_PD|C_IWBINV, 0(t0)		# Invalidate cache line
	xor	ta1,t0,ta2
	cache	CACH_PD|C_IWBINV, 0(ta1)	# Invalidate cache line
#ifdef _R4600_CACHEOP_WAR
	mtc0	ta0, C0_SR
#endif
	bltu	t0, t1, 2b
	PTR_ADDU t0, R4600_DCACHE_LINESIZE	# BDSLOT
	.set	reorder

	/* now do icache */
31:	beqz	v0, 51f

	/* clean icache using indexed invalidate */
	move	t0, a0			# set up current address

	PTR_ADDU t1, t0, a1		# set up target address
#ifdef R4600SC
	bleu	a1,ta2,22f
	PTR_ADDU t1, t0, ta2
22:	
#endif /* R4600SC */
	PTR_SUBU t1, R4600_ICACHE_LINESIZE	# (base + size - R4600_ICACHE_LINESIZE)

	and t3, t0, R4600_ICACHE_LINEMASK # align start to icache line
	PTR_SUBU t0, t3			# pull off extra

	and	t3, t1, R4600_ICACHE_LINEMASK	# align ending to icache line
	PTR_SUBU t1, t3

	.set	noreorder
#ifdef _R4600_CACHEOP_WAR
	mfc0	ta0, C0_SR
	mtc0	zero, C0_SR
	nop
#endif
4:	cache	CACH_PI|C_IINV, 0(t0) # Invalidate cache line
	xor	ta1,t0,ta2
	cache	CACH_PI|C_IINV, 0(ta1) # Invalidate cache line
	bltu	t0, t1, 4b
	PTR_ADDU t0, R4600_ICACHE_LINESIZE	# BDSLOT
#ifdef _R4600_CACHEOP_WAR
	mtc0	ta0, C0_SR
	nop
#endif
	.set	reorder

#ifdef R4600SC
51:	b	_r4600sc_index_inval
#else
51:	j	ra
#endif

17:
#endif /* R4600 */
	/* do dcache */
	beqz	ta3, 31f		# no dcache --> check icache

	/* clean dcache using indexed invalidate */
	move	t0, a0			# set up current address
 
	PTR_ADDU t1, t0, a1		# set up limit address
	PTR_SUBU t1, DCACHE_LINESIZE	# (base + count - DCACHE_LINESIZE)

	and t3, t0, DCACHE_LINEMASK	# align start to dcache line
	PTR_SUBU t0, t3			# pull off extra

	.set	noreorder
2:	cache	CACH_PD|C_IWBINV, 0(t0)	# Invalidate cache line
	bltu	t0, t1, 2b
	PTR_ADDU t0, DCACHE_LINESIZE	# BDSLOT
	.set	reorder

	/* now do icache */
31:	beqz	v0, 41f

	/* clean icache using indexed invalidate */
	move	t0, a0			# set up current address

	PTR_ADDU t1, t0, a1		# set up target address
	PTR_SUBU t1, ICACHE_LINESIZE	# (base + size - ICACHE_LINESIZE)

	and t3, t0, ICACHE_LINEMASK # align start to icache line
	PTR_SUBU t0, t3			# pull off extra

	and	t3, t1, ICACHE_LINEMASK	# align ending to icache line
	PTR_SUBU t1, t3

	.set	noreorder
4:	cache	CACH_PI|C_IINV, 0(t0) # Invalidate cache line
	bltu	t0, t1, 4b
	PTR_ADDU t0, ICACHE_LINESIZE	# BDSLOT
	.set	reorder

41:
#ifdef R4600SC_DEBUG
	lw	t9, _r4600sc_2nd_flush_error
	or	t9, (1<<18)
	sw	t9, _r4600sc_2nd_flush_error
#endif

	j	ra
#endif /* R4000PC */

101:	/* do scache */
#ifdef R10000
	srl	ta0,1			# take into 2 way assoc cache
#endif

	move	t0,a0			# starting address

#ifdef R10000
	move	ta1,ta0			# save s$ size for use below
#endif
	# align the starting address to a 2nd cache line
	and	t2,t0,SCACHE_LINEMASK
	bltu	ta0,a1,1f		# 2nd cache is smaller than count
	move	ta0,a1
1:
	PTR_ADDU ta0,t0			# ending addr + 1
	PTR_SUBU t0,t2			# line-align
	PTR_SUBU ta0,SCACHE_LINESIZE

#ifdef R10000
	/* R10000 only checks ptag for primary caches, so also hit them
	 * explicitly as this is an indexed operation.
	 */
	bleu	ta1,a1,3f		# skip primary if flushing full cache

	move	ta1,a0			# base addr
	lw	t2,pdcache_size		# cache size (isize == dsize)
	move	ta2,a1			# base + length
	srl	t2,1			# 2 way assoc
	PTR_ADDU t2,ta1			# addr + cache size
	bltu	ta2,t2,2f		# len > cachesize?
	move	ta2,t2			# cap primary loop @ cachesize

	.set	noreorder
2:
	PTR_ADDU	ta1,CACHE_ILINE_SIZE
	cache	CACH_PD|C_IWBINV,-CACHE_DLINE_SIZE(ta1)		# dcache 1 way 0
	cache	CACH_PD|C_IWBINV,-CACHE_DLINE_SIZE+1(ta1)	# dcache 1 way 1
	cache	CACH_PD|C_IWBINV,-(2*CACHE_DLINE_SIZE)(ta1)	# dcache 2 way 0
	cache	CACH_PD|C_IWBINV,-(2*CACHE_DLINE_SIZE)+1(ta1)	# dcache 2 way 1
	cache	CACH_PI|C_IINV,-CACHE_ILINE_SIZE(ta1)		# icache way 0
	bltu	ta1,ta2,2b
	cache	CACH_PI|C_IINV,-CACHE_ILINE_SIZE+1(ta1)		# BDSLOT way 1
	.set	reorder
3:
#if CACHE_ILINE_SIZE != (2*CACHE_DLINE_SIZE)
#error R10000 CACHE_ILINE_SIZE != (2*CACHE_DLINE_SIZE)
#endif
#endif /* R10000 */

	.set	noreorder
1:
	cache	CACH_SD|C_IWBINV,0(t0)	# writeback + inval 2nd cache lines
#ifdef R10000
	cache	CACH_SD|C_IWBINV,1(t0)	# writeback + inval 2nd (way 1) lines
#endif

	bltu	t0,ta0,1b
	PTR_ADDU t0,SCACHE_LINESIZE	# BDSLOT
	.set	reorder

#if R10000 && !R10000_SPECULATION_WAR
	/* ensure previous cache instructions have graduated */
	.set	noreorder
	cache	CACH_BARRIER,-SCACHE_LINESIZE(t0)
	.set	reorder
#endif

41:
#ifdef R4600SC_DEBUG
	lw	t9, _r4600sc_2nd_flush_error
	or	t9, (1<<19)
	sw	t9, _r4600sc_2nd_flush_error
#endif

	j	ra			# restores cached mode.
#endif	/* IP17 || IP19 */
	END(__cache_wb_inval)

#endif /* !IP25 */
	

#if IP19 || IP25 || IP30
/*
 * __cache_hwb_inval_pfn(poffset, len, pfn)
 * Uses the HIT_WRITEBACK_INVALIDATE
 * cacheops. Should be called with xkphys addresses, to avoid
 * the tlb translation (and tlb miss fault) overhead.
 *
*/
LEAF(__cache_hwb_inval_pfn)
#if _K32U64
	.set	noreorder
	MFC0(t3,C0_SR)
	.set	reorder
#endif
	dsll	a2,PNUMSHFT
	lui	t0,0xa800		/* cacheable, coh. excl on write */
	dsll	t0,32			/* t0 = 0xa800000000000000 */
	dadd	a2,t0
	dadd	t0,a0,a2		/* t0 = 64 bit starting	address */
	lw	ta0,_sidcache_size
#if _K32U64
	/* Not normally running in 64-bit mode, so turn it on.
	 * Don't need to restore original state since no real harm in
	 * running the kernel in 64-bit mode.
	 */
	ori	t3,SR_KX		/* enable 64 bit addressing */
	.set	noreorder
	MTC0(t3,C0_SR)
	.set	reorder
#endif

	# align the starting address to a 2nd cache line
#if IP25
	and	t2,t0,ICACHE_LINEMASK
#else
	and	t2,t0,SCACHE_LINEMASK
#endif
	bltu	ta0,a1,1f		# 2nd cache is smaller than count
	move	ta0,a1
1:
	daddu	ta0,t0			# ending addr + 1
	dsubu	t0,t2			# line-align
1:
	.set	noreorder
#if IP25
#if ICACHE_LINESIZE != (2*DCACHE_LINESIZE)
#       error "Icache line size assumed 2 * dcache line size"
#endif
	/*
	 * SCC (< REV E) can hang if we invalidate an scache line. So we
	 * work on the primary only.
	 */
	ICACHE(C_HINV, 0(t0))
	DCACHE(C_HWBINV, 0(t0))
	DCACHE(C_HWBINV, DCACHE_LINESIZE(t0))
	.set	reorder
	daddu	t0,ICACHE_LINESIZE
#else /* IP19 || IP30 */
	cache	CACH_SD|C_HWBINV,0(t0)	# writeback + inval 2nd cache lines
	.set	reorder
	daddu	t0,SCACHE_LINESIZE
#endif	
	bltu	t0,ta0,1b

	j	ra			# restores cached mode.
	END(__cache_hwb_inval_pfn)
#endif /* IP19 || IP25 || IP30 */

/*
 * Config_cache() -- determine sizes of i and d caches
 * Sizes stored in globals picache_size, pdcache_size, icache_size
 * and dcache_size.
 * For R4000, 2nd cache size stored in global boot_sidcache_size.
 * Determines size of secondary cache - assumes 2nd cache is
 * data or unified I+D.
 * Can be extended to look for 2nd instruction
 * cache by reading the config register. By definition, if 2nd
 * cache is present and 'split', then both secondary caches are
 * the same size.
 */

#define	MIN_CACH_POW2	12

CFGLOCALSZ=	4			# Save ra, s0, s1, s1
CFGFRM=		FRAMESZ((NARGSAVE+CFGLOCALSZ)*SZREG)
CFGRAOFF=	CFGFRM-(1*SZREG)
CFGS0OFF=	CFGFRM-(2*SZREG)
CFGS1OFF=	CFGFRM-(3*SZREG)
CFGS2OFF=	CFGFRM-(4*SZREG)

NESTED(config_cache, CFGFRM, zero)
	PTR_SUBU sp,CFGFRM
	REG_S	ra,CFGRAOFF(sp)
	REG_S	s0,CFGS0OFF(sp)	# save s0 on stack
	REG_S	s1,CFGS1OFF(sp)	# save s1 on stack
	REG_S	s2,CFGS2OFF(sp)	# save s2 on stack
	.set    noreorder
	MFC0(s0,C0_SR)		# save SR
	NOP_1_1				# get thru pipe before we zero it
	# NOINTS(t0,C0_SR)		# disable interrupts

	# Size primary instruction cache.
	NOP_0_1				# required between mtco/mfc0
#ifdef R10000	
	nop
#endif /* R10000 */
	mfc0	t0,C0_CONFIG
	.set reorder
#if R4000 && R10000
	.set	noreorder
	mfc0	t3,C0_PRID
	NOP_0_4
	.set reorder
	andi	t3,C0_IMPMASK
	subu	t3,(C0_IMP_R5000 << C0_IMPSHIFT)
	beqz	t3,72f
	subu    t3,((C0_IMP_RM5271 << C0_IMPSHIFT) - (C0_IMP_R5000 << C0_IMPSHIFT))
	beqz    t3,72f
	
	and	t1,t0,CONFIG_R10K_IC
	srl	t1,CONFIG_R10K_IC_SHFT
	addi	t1,MIN_CACH_POW2
	li	t2,1
	sll	t2,t1
	sw	t2,picache_size

	# Size primary data cache.
	and	t1,t0,CONFIG_R10K_DC
	srl	t1,CONFIG_R10K_DC_SHFT
	addi	t1,MIN_CACH_POW2
	b	73f

#endif 
72:	
	and	t1,t0,CONFIG_IC
	srl	t1,CONFIG_IC_SHFT
	addi	t1,MIN_CACH_POW2
	li	t2,1
	sll	t2,t1
	sw	t2,picache_size
	sw	t2,_icache_size
	sw	t2,icache_size

	# Size primary data cache.
	and	t1,t0,CONFIG_DC
	srl	t1,CONFIG_DC_SHFT
	addi	t1,MIN_CACH_POW2
73:	
	li	t2,1
	sll	t2,t1
	sw	t2,pdcache_size
	sw	t2,_dcache_size
	sw	t2,dcache_size

#ifdef R4600	
	li	t2,R4600_ICACHE_LINEMASK
#else /* R4600 */
	li	t2,ICACHE_LINEMASK
#endif /* R4600 */
	sw	t2,_icache_linemask
#ifdef R4600	
	li	t2,R4600_DCACHE_LINEMASK
#else /* R4600 */
	li	t2,DCACHE_LINEMASK
#endif /* R4600 */
	sw	t2,_dcache_linemask

#ifdef R4600	
	li	t2,R4600_ICACHE_LINESIZE
#else /* R4600 */
	li	t2,ICACHE_LINESIZE
#endif /* R4600 */
	sw	t2,_icache_linesize
#ifdef R4600	
	li	t2,R4600_DCACHE_LINESIZE
#else /* R4600 */
	li	t2,DCACHE_LINESIZE
#endif /* R4600 */
	sw	t2,_dcache_linesize

#ifdef R4600	
	li	t2,R4600_SCACHE_LINEMASK
#else /* R4600 */
	li	t2,SCACHE_LINEMASK
#endif /* R4600 */
	sw	t2,scache_linemask
	sw	t2,_scache_linemask
	LONG_S	t2,dmabuf_linemask

#if	IP19
	/* Determine CPU # and pick up cache size from our MPCONF block */
	jal	get_mpconf
	lw	t0,MP_SCACHESZ(v0)	# cache size (sorta)
	li	v0,1
	sll	v0,t0			# Cache size in bytes
	
	.set	noreorder
#endif /* IP19 */
	
#if (! (defined(R10000) && (! defined(R4000))))
#if 0
#ifdef R10000
	bnez	t3,74f
#endif /* R10000 */		
#endif 
#ifndef	IP19
	LA	v0,1f
#ifdef _K64U64
	xor	v0,K0BASE
#endif
	or	v0,K1BASE
	.set	noreorder
	j	v0			# run uncached
	nop

1:	jal	size_2nd_cache
	nop
#endif	/* IP19 */
	# v0 has the size of the secondary (data or unified) cache.
	sw	v0,boot_sidcache_size
	sw	v0,_sidcache_size
	.set reorder
#ifdef R4000PC
	bnez	v0, 3f
	lw	t0,picache_size
	lw	t1,pdcache_size
	sw	t0,icache_size
	sw	t0,_icache_size
	sw	t1,dcache_size
	sw	t1,_dcache_size
	srl	t3,t1,PNUMSHFT
#ifdef R4600
	.set noreorder
	mfc0	t2,C0_PRID
	NOP_0_4
	.set reorder
	andi	t2,C0_IMPMASK
	subu	t2,(C0_IMP_R4600 << C0_IMPSHIFT)
	beqz	t2,4f
	subu	t2,((C0_IMP_R4700 << C0_IMPSHIFT)-(C0_IMP_R4600 << C0_IMPSHIFT))
#ifdef TRITON	
	beqz	t2,4f
	subu	t2,((C0_IMP_TRITON << C0_IMPSHIFT)-(C0_IMP_R4700 << C0_IMPSHIFT))
	beqz    t2,4f
        subu    t2,((C0_IMP_RM5271 << C0_IMPSHIFT)-(C0_IMP_TRITON << C0_IMPSHIFT))
#endif /* TRITON */
	bnez	t2,5f
4:		
	srl	t2,t1,1
	sw	t2,two_set_pcaches
	srl	t3,t2,PNUMSHFT
5:	
#endif
	sw	t3,cachecolorsize
	subu	t3,1
	sw	t3,cachecolormask
	j	2f

3:
#endif
	lw	v0,picache_size
	sw	v0,icache_size
	sw	v0,_icache_size
	lw	v0,pdcache_size
	sw	v0,dcache_size
	sw	v0,_dcache_size

#ifdef R4600SC
	.set noreorder
	mfc0	t2,C0_PRID
	NOP_0_4
	.set	reorder
	andi	t2,C0_IMPMASK
	subu	t2,(C0_IMP_R4600 << C0_IMPSHIFT)
	beqz	t2,41f
	subu	t2,((C0_IMP_R4700 << C0_IMPSHIFT)-(C0_IMP_R4600 << C0_IMPSHIFT))
#ifdef TRITON	
	beqz	t2,41f
	subu	t2,((C0_IMP_TRITON << C0_IMPSHIFT)-(C0_IMP_R4700 << C0_IMPSHIFT))
	beqz    t2,41f
        subu    t2,((C0_IMP_RM5271 << C0_IMPSHIFT)-(C0_IMP_TRITON << C0_IMPSHIFT))
#endif /* TRITON */
	bnez	t2,2f
41:	
#if 0
	jal	_r4600sc_enable_scache
#endif
	lw	t1,pdcache_size
	li	t3,R4600_SCACHE_LINEMASK
	sw	t3,scache_linemask
	sw	t3,_scache_linemask
	LONG_S	t3,dmabuf_linemask
	li	t3,R4600_SCACHE_LINESIZE
	sw	t3,_scache_linesize
	b	4b
#endif

#ifdef R4000PC
2:
#endif
	.set noreorder
#ifndef	EVEREST
	LA	t0,1f
	j	t0			# back to cached mode
	nop
#endif
#else	/* R10000 */
#ifdef _STANDALONE
	jal	size_2nd_cache
	sw	v0,boot_sidcache_size
	sw	v0,_sidcache_size
	sw	v0,icache_size
	sw	v0,dcache_size
#endif /* _STANDALONE */
	.set	noreorder
#endif	/* R10000 */

74:		
1:	MTC0(s0,C0_SR)		# restore SR
	.set	reorder
	REG_L	s2,CFGS2OFF(sp)	# restore old s2
	REG_L	s1,CFGS1OFF(sp)	# restore old s1
	REG_L	s0,CFGS0OFF(sp)	# restore old s0
	REG_L	ra,CFGRAOFF(sp)
	PTR_ADDU sp,CFGFRM
	j	ra
	END(config_cache)

#if	!IP19
/*
 * size_2nd_cache()
 * return size of current data cache
 * called while running in uncached space.
 *
 * sizing secondary cache: (assumes running uncached, 2nd cache is
 * a data or unified cache)
 * 1: Load up the cache in powers of 2, from MINCACHE to MAXCACHE. Then
 * each of these lines guarenteed to be valid (may cycle around the cache.)
 * 2. do an index store tag zero to line 0 - this tag is guaranteed to be
 * invalid for any address we use.
 * 3. Starting at MINCACHE, do an index load tag. Continue until find the
 * zero tag.
 *
 * XXX:	for R4600 we read the cache size from the EEROM.  If it is an R4600PC
 *      register 17 in the EEROM will contain zeros, otherwise it will contain
 *      the cache size in pages for an R4600SC.
 */
LEAF(size_2nd_cache)
#ifdef R4600SC
	.set	noreorder
	mfc0	t0,C0_PRID		# get processor ID
	NOP_0_4
	.set	reorder
	and	t0,C0_IMPMASK
	subu	t0,(C0_IMP_R4600 << C0_IMPSHIFT)
	beqz	t0,6f
	subu	t0,((C0_IMP_R4700 << C0_IMPSHIFT)-(C0_IMP_R4600 << C0_IMPSHIFT))
#ifdef TRITON	
	beqz	t0,6f
	subu	t0,((C0_IMP_TRITON << C0_IMPSHIFT)-(C0_IMP_R4700 << C0_IMPSHIFT))
	beqz    t0,4f
        subu    t0,((C0_IMP_RM5271 << C0_IMPSHIFT)-(C0_IMP_TRITON << C0_IMPSHIFT))
        bnez    t0,1f
4:
	.set	noreorder
	mfc0	t0,C0_CONFIG
	nop
	and	v0,t0,CONFIG_TR_SC
#ifdef TRITON_INDYSC
	bnez	v0,6f
#else /* TRITON_INDYSC */
	bnez	v0,3f
#endif /* TRITON_INDYSC */
	and	v0,zero
	.set reorder
	and	t0,CONFIG_TR_SS
	srl	t0,CONFIG_TR_SS_SHFT
	li	v0,512*1024
	sll	v0,t0
	sw	v0,_r5000_sidcache_size
#if 0
	.set	noreorder
	mfc0	t0,C0_CONFIG
	nop
	or	v0,t0,CONFIG_TR_SE
	mtc0	v0,C0_CONFIG
	NOP_0_4
	cache	CACH_SD|C_INVALL,0(ta0)	# invalidate scache
	.set	reorder
#endif
	b	3f
#else /* TRITON	*/
	bnez	t0,1f
#endif /* TRITON */
6:	
#if IP22
	li	t0,1
	sw	t0,early_delay_flag
	li	t0,EEROM		# yes --> get cache size from EEROM
	lw	s1,cpu_auxctl
	move	s2,ra
	sw	t0,cpu_auxctl
	li	a0,CACHESZ_REG		# we want the 18th halfword
	jal	get_nvreg		# get cache size from EEROM, 4K page
	li	t0,0
	sw	t0,early_delay_flag
	sw	s1,cpu_auxctl
	sll	v0,BPCSHIFT			# cache size in bytes
	move	ra,s2			# restore old ra
#else /* IP22 */
	move	v0,zero
#endif /* IP22 */
	b	3f
#endif /* R4600SC */
	
1:	
#ifdef R10000
	# R10000 always has a secondary cache.
	j	sCacheSize
#else /* R10000 */
	.set noreorder

#ifndef R10000
	# R10000 always has a secondary cache.
1:	
	mfc0	t0,C0_CONFIG
	.set	reorder
	and	t0,CONFIG_SC		# 0 -> 2nd cache present
	beq	t0,zero,1f
	move	v0,zero			# No second cache
	j	ra
#endif
1:
	.set	reorder
	li	v0,MINCACHE		# If MINCACHE == MAXCACHE, just
	li	t0,MAXCACHE		# return that
	bne	t0,v0,1f
	j	ra
1:
	# Load valid tags at each cache boundary up to MAXCACHE.
	# v0 has MINCACHE value.
1:
	LI	t2,K0_CACHEFLUSHBASE
	PTR_ADDU t2,v0
	lw	zero,0(t2)
	PTR_SLL	v0,1
	ble	v0,MAXCACHE,1b

	# Invalidate cache tag at 0th index in all caches.
	# Invalidating the primaries insures that we do not
	# create a non-subset of the secondary cache in
	# either of the primary caches.
	.set	noreorder
	mtc0	zero,C0_TAGLO
	mtc0	zero,C0_TAGHI
	LI	t0,K0_CACHEFLUSHBASE		# LDSLOT
	li	v0,MINCACHE		# LDSLOT
	cache	CACH_PI|C_IST,0(t0)
	cache	CACH_PD|C_IST,0(t0)
	cache	CACH_SD|C_IST,0(t0)
	.set	reorder

	# Now loop until that tag is found
	# v0 - MINCACHE
1:
	LI	t0,K0_CACHEFLUSHBASE	# Reload K0_CACHEFLUSHBASE for each iteration.
	PTR_ADDU t0,v0
	.set	noreorder
	cache	CACH_SD|C_ILT,0(t0)
	NOP_0_3
	mfc0	t1,C0_TAGLO
	.set	reorder
	beq	t1,zero,2f		# Found the marker
	PTR_SLL	v0,1			# Next power of 2
	blt	v0,MAXCACHE,1b		# Iterate until MAXCACHE
2:
#endif /* R10000 */
#ifdef R10000
	sll	v0,1			# double the size as there are 2 sets
#endif
3:
	j	ra
	END(size_2nd_cache)
#endif

#if IP17 || IP20
/*
 * rmi_cacheflush(addr,len)
 */
LEAF(rmi_cacheflush)
#define RMI_CACHEMASK	0xfffff		# Mask for 1 meg cache
#define RMI_LOWLIMIT	0x8000		# 32k
	/* start = megmask(start) */
	li	t0,RMI_CACHEMASK
	and	t0,a0
	#---------------------------------------------------
	/* Here is the algorithm:
	 * if (start >= 32k)
	 *	flush(start, end)
	 *	return
	 * else if (start < 32k && start + count > 32k)
	 *	count = count - (32k - start)
	 *	end = PHYS_TO_K0(32k) + 1meg
	 *	start = PHYS_TO_K0(start) + 1meg
	 *	flush(start, end)
	 *	flush(K0(32k), K0(count))
	 *	return
	 * else ( start < 32k && start + count <= 32k)
	 *	start = K0(start) + 1meg
	 *	end = K0(end) + 1meg
	 *	flush(start, end)
	 *	return
	 */
	/* if (start >= 32k) goto normal_flush; */
	move	ta1,zero			# only 1 iteration
	li	ta0,RMI_LOWLIMIT
	bge	t0,ta0,normal_flush

	/* else if (start < 32k && start + count > 32k) goto splitflush */

	addu	ta1,t0,a1		# ta1 = start + count
	bgt	ta1,ta0,splitflush

	/* it must be true that start < 32k && start + count <= 32k */
	move	ta1,zero			# only 1 iteration
	addu	t0,0x100000
	b	normal_flush

splitflush:
	/* start < 32k && start + count > 32k */
	/* Flush from start to 32k */
	subu	ta2,ta0,t0		# 32k - start
	subu	ta2,a1,ta2		# save modified count in ta2
	li	ta1,1			# 2 iterations
	li	a1,RMI_LOWLIMIT		# flush from start to 32k
	addu	t0,0x100000
	b	normal_flush
comebackhere:
	/* Now flush from 32k to ending address */
	li	t0,RMI_LOWLIMIT
	move	a1,ta2
	move	ta1,zero			# no more passes

normal_flush:
	#---------------------------------------------------
	or	t0,K0_CACHEFLUSHBASE

	PTR_ADDU t1,t0,a1		# ending address + 1
	bgeu	t0,t1,3f		# paranoid

	# check and align starting address
	and	t2,t0,SCACHE_LINEMASK
	PTR_SUBU t0,t2

1:	# top of loop
	lw	zero,0(t0)		# wb secondary line
	PTR_ADDU t0,SCACHE_LINESIZE
	bltu	t0,t1,1b

	#---------------------------------------------------
	bne	ta1,zero,comebackhere	# another iteration?
	#---------------------------------------------------

	j	ra
3:
	PTR_L	sp,VPDA_LBOOTSTACK(zero)
	ASMASSFAIL("Bad args to rmi_cacheflush");
	/* NOTREACHED */
	END(rmi_cacheflush)
#endif	/* IP17 || IP20 */


/* _c_hwb(which_cache, k0addr): hit writeback specified cache.
 * a0: which_cache: CACH_PI, CACH_PD, or CACH_SD (CACH_SI illegal).
 * a1: K0 address of line to writeback.
 * if called for CACH_SI return -1, if for 2ndary data cache
 * return SR_CH bit, else return 0.
 */
LEAF(_c_hwb)
	move	v0, zero
	li	t0, CACH_PD
	bltu	a0, t0, 3f		# writeback primary i-cache tag
	beq	a0, t0, 4f		# primary d-cache tag
	li	t0, CACH_SI		# 2nd i-cache illegal for hwb
	beq	a0, t0, 2f		# return error
	.set	noreorder
#ifdef R10000
	cache	CACH_SD|C_HWBINV, 0(a1)	# inval secondary (I|D) cache tag
#else /* R10000 */
	cache	CACH_SD|C_HWB, 0(a1)	# inval secondary (I|D) cache tag
#endif /* R10000 */
	NOP_0_4
	MFC0(v0, C0_SR)			# check if we hit the 2ndary cache
	NOP_0_4
	and	v0, SR_CH
	.set	reorder

1:	# hwbout:
	j	ra

2:	# hwbsi:
	li	v0, -1
	b	1b

3:	# hwbpi:
	.set	noreorder
#ifdef R10000
	cache	CACH_PI|C_HWBINV, 0(a1)	# writeback primary icache tag if hit
#else /* R10000 */
	cache	CACH_PI|C_HWB, 0(a1)	# writeback primary icache tag if hit
#endif /* R10000 */
	.set	reorder
	b	1b

4:	# hwbpd:
#ifdef R4600
	PD_REFILL_WAIT(cacheops_refill_4)
#endif		
	.set	noreorder
#ifdef R10000
	cache	CACH_PD|C_HWBINV, 0(a1)	# primary data cache tag
#else /* R10000 */
	cache	CACH_PD|C_HWB, 0(a1)	# primary data cache tag
#endif /* R10000 */
	.set	reorder
	b	1b
	END(_c_hwb)


/* NOTE: this routine is called only by force_ecc, which needs it to
 * force the desired data ecc/parity value into the cache line.  It
 * isn't used by the ecc handler, so any ecc errors caused by the RMI
 * write bug are acceptable.  Please don't change it to use the 
 * RMI workaround method (flush by reading addr one cache-size away) */

/* _c_hwbinv(which_cache, k0addr): hit writeback invalidate specified cache.
 * a0: which_cache: CACH_PD or CACH_SD (icaches illegal).
 * a1: K0 address of line to writeback invalidate.
 * if called for CACH_SI or CACH_PI return -1, if for 2ndary data cache
 * return SR_CH bit, else return 0.
 */
LEAF(_c_hwbinv)
	move	v0, zero
	li	t0, CACH_PD
	bltu	a0, t0, 2f		# wb primary i-cache tag: illegal
	beq	a0, t0, 4f		# primary d-cache tag
	li	t0, CACH_SI		# 2nd i-cache illegal for hwb
	beq	a0, t0, 3f		# return error
	.set	noreorder
	cache	CACH_SD|C_HWBINV, 0(a1)
	NOP_0_4
	MFC0(v0, C0_SR)			# check if we hit the 2ndary cache
	NOP_0_4
	and	v0, SR_CH
	.set	reorder

1:	# hwbiout:
	j	ra

2:	# hwbipi:
3:	# hwbisi:
	li	v0, -1
	b	1b

4:	# hwbipd:
#ifdef R4600
	PD_REFILL_WAIT(cacheops_refill_5)
#endif		
	.set	noreorder
	cache	CACH_PD|C_HWBINV, 0(a1)
	.set	reorder
	b	1b
	END(_c_hwbinv)


/* _c_hinv(which_cache, k0addr): hit invalidate specified cache.
 * a0: which_cache: CACH_PI, CACH_PD, CACH_SI, or CACH_SD.
 * a1: K0 address of line to invalidate.
 * if called for 2ndary, return SR_CH bit, else return 0.
 */
LEAF(_c_hinv)
	move	v0, zero
	li	t0, CACH_PD
	bltu	a0, t0, 2f		# invalidate primary i-cache tag
	beq	a0, t0, 3f		# primary d-cache tag
	.set	noreorder
	cache	CACH_SD|C_HINV, 0(a1)	# inval secondary (I|D) cache tag
	NOP_0_4
	MFC0(v0, C0_SR)			# check if we hit the 2ndary cache
	NOP_0_4
	and	v0, SR_CH
	.set	reorder

1:	# hiout:
	j	ra

2:	# hiprimi:
	.set	noreorder
	cache	CACH_PI|C_HINV, 0(a1)	# fetch primary instruction cache tag
	.set	reorder
	b	1b

3:	# hiprimd:
#ifdef R4600
	PD_REFILL_WAIT(cacheops_refill_6)
#endif		
	.set	noreorder
	cache	CACH_PD|C_HINV, 0(a1)	# fetch primary data cache tag
	.set	reorder
	b	1b
	END(_c_hinv)


/* _c_ilt_n_ecc(cache,k0addr,tagptr,&ecc):
 *     load tag & data ecc from specified cache.
 * a0: which_cache: CACH_PI, CACH_PD, CACH_SI, or CACH_SD.
 * a1: K0 address of line.
 * a2: ptr to 2 uints (taglo [0] and taghi [1]).
 * a3: ptr to uint (data ecc).
 */
LEAF(_c_ilt_n_ecc)
	move	v0, zero
	li	t0, CACH_PD
	bltu	a0, t0, 2f		# fetch primary i-cache tag
	beq	a0, t0, 3f		# primary d-cache tag
	.set	noreorder
	cache	CACH_SD|C_ILT, 0(a1)	# secondary (I|D) cache tag
	.set	reorder

1:	# iltnout:
	.set	noreorder
	NOP_0_4
	mfc0	t0, C0_TAGLO
	NOP_0_4
	mfc0	t1, C0_ECC
	NOP_0_4
	.set	reorder
	sw	t0, 0(a2)
	sw	t1, 0(a3)
	j	ra

2:	# iltnpi:
	.set	noreorder
	cache	CACH_PI|C_ILT, 0(a1)
	.set	reorder
	b	1b

3:	# iltnpd:
	.set	noreorder
	cache	CACH_PD|C_ILT, 0(a1)
	.set	reorder
	b	1b
	END(_c_ilt_n_ecc)


/* _c_ilt(cache,k0addr,tagptr): load tag from specified cache-line.
 * a0: which_cache: CACH_PI, CACH_PD, CACH_SI, or CACH_SD.
 * a1: K0 address of line.
 * a2: ptr to 2 uints (taglo [0] and taghi [1]).
 */
LEAF(_c_ilt)
	move	v0, zero
	li	t0, CACH_PD
	bltu	a0, t0, 2f		# fetch primary i-cache tag
	beq	a0, t0, 3f		# primary d-cache tag
	.set	noreorder
	cache	CACH_SD|C_ILT, 0(a1)	# secondary (I|D) cache tag
	.set	reorder

1:	# iltout:
	.set	noreorder
	NOP_0_4
	mfc0	t0, C0_TAGLO
	NOP_0_4
	sw	t0, 0(a2)
	.set	reorder
	j	ra

2:	# iltpi:
	.set	noreorder
	cache	CACH_PI|C_ILT, 0(a1)
	.set	reorder
	b	1b

3:	# iltpd:
	.set	noreorder
	cache	CACH_PD|C_ILT, 0(a1)
	.set	reorder
	b	1b
	END(_c_ilt)


/* _c_ist(which_cache,k0addr,tagptr): store tag to specified cache.
 * a0: which_cache: CACH_PI, CACH_PD, CACH_SI, or CACH_SD.
 * a1: K0 address of line.
 * a2: ptr to 2 uints (taglo and taghi).
 * tagptr[0] = taglo, uintptr[1] = taghi
 */
LEAF(_c_ist)
	move	v0, zero
	lw	t1, 0(a2)
	.set	noreorder
	NOP_0_4
	mtc0	t1, C0_TAGLO
	.set	reorder
	li	t0, CACH_PD
	bltu	a0, t0, 1f		# store primary i-cache tag
	beq	a0, t0, 2f		# primary d-cache tag
	.set	noreorder
	NOP_0_2
	cache	CACH_SD|C_IST, 0(a1)	# store secondary (I|D) cache tag
	.set	reorder
	j	ra

1:	# istpi:
	.set	noreorder
	NOP_0_2
	cache	CACH_PI|C_IST, 0(a1)
	.set	reorder
	j	ra

2:	# istpd:
	.set	noreorder
	NOP_0_2
	cache	CACH_PD|C_IST, 0(a1)
	.set	reorder
	j	ra
	END(_c_ist)


#ifndef R10000
/* pi_fill(vaddr): load P icache with instruction line 
 * a0: vaddr
 */
LEAF(_pi_fill)
	.set	noreorder
	cache	CACH_PI|C_FILL, 0(a0)
	NOP_0_2
	.set	reorder
	j	ra
	END(_pi_fill)
#endif


LEAF(get_cache_err)
        .set    noreorder
        mfc0    v0,C0_CACHE_ERR
        .set    reorder
        j       ra
        END(get_cache_err)

LEAF(get_error_epc)
        .set    noreorder
        mfc0    v0,C0_ERROR_EPC
        .set    reorder
        j       ra
        END(get_error_epc)



LEAF(get_ecc)
	.set    noreorder
	mfc0    v0,C0_ECC		# get the current ECC value 
	.set    reorder
	j       ra
	END(get_ecc)

LEAF(set_ecc)
	.set	noreorder
	mtc0	a0,C0_ECC		# set the ECC register
	.set	reorder
	j	ra
	END(set_ecc)


#if _K32U32 || _K32U64
LEAF(runcached)
	PTR_SUBU	v0,ra,K0SIZE
1:	j	v0
	END(runcached)

LEAF(uncached)
	PTR_ADDU	v0,ra,K0SIZE
1:	j	v0
	END(uncached)
#else
LEAF(runcached)
	dsll	v0,ra,5		/* First shift off region/cache bits */
	dsrl	v0,5
	PTR_ADDU v0,K0BASE	/* then add K0BASE */
1:	j	v0
	END(runcached)

LEAF(uncached)
	dsll	v0,ra,5		/* First shift off region/cache bits */
	dsrl	v0,5
	PTR_ADDU v0,K1BASE	/* then add K1BASE */
1:	j	v0
	END(uncached)
#endif


/* _munge_decc(c_addr, status_reg): force the contents of the ECC register
 * to be used in generating the checkbits for the double word of data
 * at c_addr.
 * a0: K0addr at which to force the bad checkbits
 * a1: status register to use during the forced-write (must include SR_CE
 * and NOT include SR_DE: the CE bit does not work when DE is set).  In
 * order to avoid unintentionally munging some i-cache checkbits interrupts
 * had better be disabled and the kernel should be running uncached also
 * (just during this munging operation).
 */
LEAF(_munge_decc)
#ifdef R4600
	PD_REFILL_WAIT(cacheops_refill_7)
#endif		
	.set	noreorder
	MFC0(t1, C0_SR)		# save current status register
	NOP_0_4
	MTC0(a1, C0_SR)		# swap in specified sr
	NOP_0_4
	NOP_0_4
	cache	CACH_PD|C_HWBINV, 0(a0)	# with CE bit set, this uses ECC reg
	NOP_0_4
	NOP_0_4
	MTC0(t1, C0_SR)		# restore orig status reg
	NOP_0_4
	NOP_0_4
	.set	reorder
	j	ra
	END(_munge_decc)


/* _read_tag(WhichCache, address, &destination)
 * WhichCache == CACH_PI, CACH_PD, CACH_SI, or CACH_SD.
 * address may be in KUSER or K0SEG space.
 * destination is a pointer to two uints.
 * a0: WhichCache
 * a1: address
 * a2: destination ptr
 */
LEAF(_read_tag)
	move	v0, zero		# success by default
	li	t0, CACH_PD
	bltu	a0, t0, 2f	# fetch primary i-cache tag
	beq	a0, t0, 3f	# primary d-cache tag
	.set	noreorder
	cache	CACH_SD|C_ILT, 0(a1)	# fetch secondary (I|D) cache tag
	.set	reorder

1:	# getout:
	.set	noreorder
	nop
	nop
	mfc0	t0, C0_TAGLO
	# DO NOT READ C0_TAGHI IN CURRENT IMPLEMENTATION OF R4K!
	nop
	nop
	sw	t0, 0(a2)	# taglo is 1st uint in tag struct
	# sw	t1, 4(a2)	# taghi is 2nd
	.set	reorder
	j	ra

2:	# rprimi:
	.set	noreorder
	cache	CACH_PI|C_ILT, 0(a1)	# fetch primary instruction cache tag
	.set	reorder
	b	1b

3:	# rprimd:
	.set	noreorder
	cache	CACH_PD|C_ILT, 0(a1)	# fetch primary data cache tag
	.set	reorder
	b	1b

	END(_read_tag)


/* _write_tag(WhichCache, address, &source)
 * WhichCache == CACH_PI, CACH_PD, CACH_SI, or CACH_SD.
 * address may be in KUSER or K0SEG space.
 * source is a pointer to two uints.
 * a0: WhichCache
 * a1: address
 * a2: source ptr
 */
LEAF(_write_tag)
	lw	t0, 0(a2)
	.set	noreorder
	mtc0	t0, C0_TAGLO	# just set taglo
	NOP_0_2
	.set	reorder
	li	t0, CACH_PD
	bltu	a0, t0, 2f	# fetch primary i-cache tag
	beq	a0, t0, 3f	# primary d-cache tag
	.set	noreorder
	cache	CACH_SD|C_IST, 0(a1)	# set secondary (I|D) cache tag
	.set	reorder

1:	# setout:
	j	ra

2:	# wprimi:
	.set	noreorder
	cache	CACH_PI|C_IST, 0(a1)	# set primary instruction cache tag
	.set	reorder
	b	1b

3:	# wprimd:
	.set	noreorder
	cache	CACH_PD|C_IST, 0(a1)	# set primary data cache tag
	.set	reorder
	b	1b

	END(_write_tag)

LOCALSZ=	1			# save ra
CACHEFRM=	FRAMESZ((NARGSAVE+LOCALSZ)*SZREG)
RAOFF=		CACHEFRM-(1*SZREG)

/*
 * flush_cache()
 * flush entire I & D cache
 */
NESTED(flush_cache, CACHEFRM, zero)
#ifndef EVEREST                                 # ev_flush_caches on EVEREST
XLEAF(FlushAllCaches)
#endif
	PTR_SUBU sp,CACHEFRM
	REG_S	ra,RAOFF(sp)
	LI	a0, K0_CACHEFLUSHBASE
	lw	a1,_sidcache_size
#ifdef R4000PC
	bnez	a1, 1f
	lw	a1, pdcache_size
#ifdef R4600
	lw	a2, two_set_pcaches
	beqz	a2,1f
	move	a1,a2
#endif /* R4600 */	
1:
#endif /* R4000PC */
	jal	__cache_wb_inval
	REG_L	ra,RAOFF(sp)
	PTR_ADDU sp,CACHEFRM
	j	ra
	END(flush_cache)

#endif /* R4000  || R10000 */
	

#if IP19
LEAF(ip19_cache_init)
/*
 * Routine:	ip19_cache_init
 * Purpose:	Initialize the cache tags to a known "legal" state.
 * Parameters:	None
 * Returns:	Nothing
 * Notes:	This routine accomplishes its task in
 *		the following steps, and uses the CONFIG register and the
 *		MPCONF structure to determine the cache sizes.
 *
 *		1) Index write-back, invalidate on all secondary cache lines.
 *		   This is required if symmon is running cached to be sure
 *		   we don't lose any data.  This in itself *COULD* cause a 
 *		   problem if the caches are not inited properly, but it 
 *		   appears to usually work - so if an older prom is installed,
 *		   this makes things more likely to work.
 *		2) Set all Primary data/instruction tags to contain a valid
 *		   address but set the line invalid.
 *		3) Set all secondary cache tags to contain a valid address,
 *		   such that the primary cache tags are a subset of the
 *		   secondary tags even though all are invalid.
 *
 */
	/* Get secondary cache size */

        move	a0,ra
        jal	get_mpconf		/* ONLY KILLS T registers +v0/v1 */
        move	ra,a0
	.set	noreorder

	lw	t2,MP_SCACHESZ(v0)	/* Secondary cache size - sorta */

	LI	v0,1
	sll	v0,t2			/* Size in bytes */

	LI	a0,K0BASE
	PTR_ADDU a1,a0,v0		/* address of byte after last line */

	LA	t1,1f			/* Set up to run uncached */
	LI	t2,TO_PHYS_MASK
	and	t1,t2
	LI	t2,K1BASE
	or	t1,t2
	j	t1			/* Bang - uncached */
	nop

	.align	7
        .set	noreorder
	/* Flush cache */

1:	
	cache	CACH_SD+C_IWBINV,0(a0)
	bltu	a0,a1,1b
	PTR_ADDU a0,SCACHE_LINESIZE	/* BDSLOT */

	/* Write primary tags */

	mfc0	t0,C0_CONFIG
	and	t0,CONFIG_IC
	srl	t0,CONFIG_IC_SHFT
	add	t0,12
	li	a1,1
	sll	a1,t0			/* IC size in a1 */
	
	move	a0,zero			/* Start Address in a0 */
	mtc0	zero,C0_TAGHI
1:
	
	PTR_SRL	a2,a0,12
	PTR_SLL	a2,8
	mtc0	a2,C0_TAGLO
	or	a2,a0,K0BASE
	cache	CACH_PI+C_IST,0(a2)
	PTR_ADD	a0,ICACHE_LINESIZE	/* Next cache line */
	bltu	a0,a1,1b
	nop

	mfc0	t0,C0_CONFIG
	and	t0,CONFIG_DC
	srl	t0,CONFIG_DC_SHFT
	add	t0,12
	li	a1,1
	sll	a1,t0			/* IC size in a1 */
	
	move	a0,zero			/* Start Address in a0 */
	mtc0	zero,C0_TAGHI
1:
	
	PTR_SRL	a2,a0,12
	PTR_SLL	a2,8
	mtc0	a2,C0_TAGLO
	or	a2,a0,K0BASE
	cache	CACH_PD+C_IST,0(a2)
	PTR_ADD	a0,DCACHE_LINESIZE	/* Next cache line */
	bltu	a0,a1,1b
	nop

	/* Write secondary tags */

	move	a0,zero
	move	a1,v0
1:
	PTR_SRL a2,a0,17
	PTR_SLL	a2,13
	PTR_SRL	a3,a0,12
	and	a3,7
	PTR_SLL	a3,7
	or	a3,a2
	mtc0	a3,C0_TAGLO
	or	a3,a0,K0BASE
	cache	CACH_SD+C_IST,0(a3)
	PTR_ADDI a0,SCACHE_LINESIZE
	bltu	a0,a1,1b
	nop

	j	ra			/* Back to cached space */
	nop

	.align	7
	
	END(ip19_cache_init)
#endif	/* IP19 */

#if IP25

LEAF(__cache_exclusive)
/*
 * Routine:	__cache_exclusive
 * Purpose:	Perform opration to assure the cache line containing
 *		the passed address is owned exclusivly by calling CPU.
 *		This has the side affect of flushing any dirty data out
 *		of all other CPU's data caches.
 * Parameters:	a0 - address (arbitrary alignment).
 *		a1 - length (in bytes)
 * Returns:	nothing
 */
	.set	reorder
	PTR_ADDU a1,a0				/* Ending line address */
	and	a0,CACHE_SLINE_MASK		/* Starting line */
1:	
	ll	a2,0(a0)
	move	a3,a2
	sc	a2,0(a0)
	beqz	a2,1b
	PTR_ADDU a0,CACHE_SLINE_SIZE		/* Next line please */
	bltu	a0,a1,1b

	j	ra
	END(__cache_exclusive)

LEAF(__dcache_wb)
/*
 * Routine:	__dcache_wb
 * Purpose:	Write back the specified addresses from the primary
 *		dcache. Does not operate on secondary cache.
 * Parameters:	addr, len
 * Returns:	nothing
 */
	PTR_ADDU a1,a0				/* ending line */
	and	a0, CACHE_DLINE_MASK		/* Starting line */
	
1:	.set	noreorder
	DCACHE(C_HWB, 0(a0))
	.set	reorder
	PTR_ADDU a0, CACHE_DLINE_SIZE
	bltu	a0,a1,1b
	j	ra
	END(__dcache_wb)

LEAF(__cache_wb_inval)
/*
 * Routine:	__cache_wb_inval
 * Purpose:	Flush and invalidate the specified memory from the primary
 *		caches.
 * Parameters:	a0 - address to flush/invalidate
 *		a1 - number of bytes
 * Returns:	nothing
 */
	PTR_ADDU a1,a0				/* ending line */
	and	a0, CACHE_ILINE_MASK		/* Starting line */
	
1:	.set	noreorder
	ICACHE(C_HINV, 0(a0))
	DCACHE(C_HWBINV, 0(a0))
	DCACHE(C_HWBINV, CACHE_DLINE_SIZE(a0))
	.set	reorder
	PTR_ADDU a0, CACHE_ILINE_SIZE
	bltu	a0,a1,1b
	j	ra
	
	END(__cache_wb_inval)

LEAF(__dcache_inval)
XLEAF(__dcache_wb_inval)
/*
 * Routine:	__dcache_inval
 * Purpose:	Flush and invalidate the specified region from the
 *		primary Dcache.
 * Parameters:	a0 - actuial virtual address to flush
 *		a1 - length in bytes.
 * Returns:	nothing.
 */
	PTR_ADDU a1,a0				/* ending line */
	and	a0, CACHE_DLINE_MASK		/* Starting line */
1:	.set	noreorder
	DCACHE(C_HWBINV, 0(a0))
	.set	reorder
	PTR_ADDU a0, CACHE_DLINE_SIZE
	bltu	a0,a1,1b
	j	ra
	END(__dcache_inval)
	
#endif /* IP25 */
	
#ifdef IP32
LEAF(invalidate_sc_page)
#if R10000 && TRITON
	.set	noreorder
	MFC0(t0,C0_PRID)
	nop
	.set	reorder
	and	t0,C0_IMPMASK
	subu	t0,(C0_IMP_R5000<<C0_IMPSHIFT)
	beqz	t0,55f
	subu    t0,((C0_IMP_RM5271 << C0_IMPSHIFT) - (C0_IMP_R5000 << C0_IMPSHIFT))
        beqz    t0,55f
#endif /* R10000 && TRITON */
#ifdef R10000
        .set    noreorder
        MFC0(t0,C0_CONFIG)
        nop
        .set    reorder
	and	t0,CONFIG_SB
	srl	t0,CONFIG_SB_SHFT
	li	t1,64	
	sll	t1,t0
	addu	t0,a0,_PAGESZ
11:		
	.set	noreorder
	cache	CACH_SD|C_IWBINV,0(t0)	
	subu	t0,t1
	.set	reorder
	bgeu	t0,a0,11b
	j	ra
#endif /* R10000 */
#ifdef TRITON
55:		
        # 2nd cache not guaranteed to exist - just return if not present
        .set    noreorder
        MFC0(t0,C0_CONFIG)
        nop
        nop
        .set    reorder
        and     t0,CONFIG_SC            # 0 -> 2nd cache present
        beq     t0,zero,1f
        j       ra                      # No second cache
1:
        .set    noreorder
        mtc0    zero,C0_TAGLO
        mtc0    zero,C0_TAGHI
        nop
        nop
        .set    reorder
        .set    noreorder
        cache   CACH_SD|C_INVPAGE,0(a0)
        nop; nop; nop; nop; nop;
        cache   CACH_SD|C_ILT,0(a0)
        .set    reorder
        j       ra
#endif /* TRITON */	
        END(invalidate_sc_page)

LEAF(invalidate_sc_line)
#if R10000 && TRITON
	.set	noreorder
	MFC0(t0,C0_PRID)
	nop
	.set	reorder
	and	t0,C0_IMPMASK
	sub	t0,(C0_IMP_R5000<<C0_IMPSHIFT)
	beqz	t0,55f
	subu    t0,((C0_IMP_RM5271 << C0_IMPSHIFT) - (C0_IMP_R5000 << C0_IMPSHIFT))
        beqz    t0,55f
#endif /* R10000 && TRITON */
#ifdef R10000
	.set	noreorder
	cache	CACH_SD|C_IINV,0(a0)	
	nop
	cache	CACH_SD|C_IINV,1(a0)	
	.set	reorder	
	j	ra
#endif /* R10000 */
#ifdef TRITON	
55:		
        # 2nd cache not guaranteed to exist - just return if not present
        .set    noreorder
        MFC0(t0,C0_CONFIG)
        nop
        nop
        .set    reorder
        and     t0,CONFIG_SC            # 0 -> 2nd cache present
        beq     t0,zero,1f
        j       ra                      # No second cache
1:
        .set    noreorder
        mtc0    zero,C0_TAGLO
        mtc0    zero,C0_TAGHI
        nop
        nop
        .set    reorder
        .set    noreorder
        cache   CACH_SD|C_IST,0(a0)
        .set    reorder
        j       ra
#endif /* TRITON */
        END(invalidate_sc_line)
#endif /* IP32 */

/*
 * Clobber all of the caches.
 * Assumes the variables _icache_size, _dcache_size, _sidcache_size
 * are valid. Stack must be uncached (as in IDE).
 */
INVALFRAME=     FRAMESZ((4+1)*SZREG)    # 4 arg saves, ra
NESTED(invalidate_caches, INVALFRAME, zero)
	PTR_SUBU sp,INVALFRAME
	REG_S   ra,INVALFRAME-SZREG(sp)

	lw      a0, _icache_size
	lw      a1, _icache_linesize
	jal     invalidate_icache
	lw      a0, _dcache_size
	lw      a1, _dcache_linesize
	jal     invalidate_dcache

	lw      a0, _sidcache_size
	lw      a1, _scache_linesize
	jal     invalidate_scache

	REG_L   ra,INVALFRAME-SZREG(sp)
	PTR_ADDU sp,INVALFRAME
	j       ra
	END(invalidate_caches)


/*
 * Invalidate the cache in question.
 * Contents of the cache is invalidated - no writebacks occur.
 * These do not assume memory is working, or stack pointer valid.
 *
 * These invalidate without causing writebacks. They
 * are useful for cleaning up the random power-on state.
 *
 * a0: cache size
 * a1: cache line size
 */
LEAF(invalidate_icache)
	.set	noreorder
	mfc0	t3,C0_PRID
	NOP_0_4
	.set reorder
	andi	t3,C0_IMPMASK
	subu	t3,(C0_IMP_R5000 << C0_IMPSHIFT)
	.set    noreorder
	mtc0    zero,C0_TAGLO
	mtc0    zero,C0_TAGHI
	LI      t0,K0_RAMBASE
	PTR_ADDU a0,t0          # a0: cache size + K0base for loop termination
	NOP_0_1
	beqz	t3,2f
	subu    t3,((C0_IMP_RM5271 << C0_IMPSHIFT) - (C0_IMP_R5000 << C0_IMPSHIFT))
        beqz    t3,2f
	nop
1:
	cache	CACH_PI|C_IINV, 0(t0)
	nop
	cache	CACH_PI|C_IINV, 1(t0)
	.set    reorder
	PTR_ADDU t0,a1
	bltu    t0,a0,1b                # a0 is termination count

	j       ra

2:
	.set    noreorder
	cache   CACH_PI|C_IST,0(t0)
	.set    reorder
	PTR_ADDU t0,a1
	bltu    t0,a0,2b                # a0 is termination count

	j       ra
	END(invalidate_icache)

LEAF(invalidate_dcache)
	.set	noreorder
	mfc0	t3,C0_PRID
	NOP_0_4
	.set reorder
	andi	t3,C0_IMPMASK
	subu	t3,(C0_IMP_R5000 << C0_IMPSHIFT)
	.set    noreorder
	mtc0    zero,C0_TAGLO
	mtc0    zero,C0_TAGHI
	LI      t0,K0_RAMBASE
	PTR_ADDU a0,t0          # a0: cache size + K0base for loop termination
	NOP_0_1
	beqz	t3,2f
	subu    t3,((C0_IMP_RM5271 << C0_IMPSHIFT) - (C0_IMP_R5000 << C0_IMPSHIFT))
        beqz    t3,2f
	nop
1:
	cache	CACH_PD|C_IINV, 0(t0)
	nop
	cache	CACH_PD|C_IINV, 1(t0)
	.set    reorder
	PTR_ADDU t0,a1
	bltu    t0,a0,1b                # a0 is termination count

	j       ra

2:
	.set    noreorder
	cache   CACH_PD|C_IST,0(t0)
	.set    reorder
	PTR_ADDU t0,a1
	bltu    t0,a0,2b                # a0 is termination count

	j       ra
	END(invalidate_dcache)

LEAF(invalidate_scache)
	.set	noreorder
	mfc0	t3,C0_PRID
	NOP_0_4
	.set reorder
	andi	t3,C0_IMPMASK
	subu	t3,(C0_IMP_R5000 << C0_IMPSHIFT)
	# 2nd cache not guaranteed to exist - just return if not present
	bne	a0,zero,1f
	j       ra                      # No second cache
1:
	.set    noreorder
	mtc0    zero,C0_TAGLO
	mtc0    zero,C0_TAGHI
	LI      t0,K0_RAMBASE
	PTR_ADDU a0,t0          # a0: cache size + K0base for loop termination
	NOP_0_1
	beq	t3,zero,2f
	subu    t3,((C0_IMP_RM5271 << C0_IMPSHIFT) - (C0_IMP_R5000 << C0_IMPSHIFT))
        beq     t3,zero,2f
	nop
1:
	cache	CACH_SD|C_IST, 0(t0)
	nop
	cache	CACH_SD|C_IST, 1(t0)
	.set    reorder
	PTR_ADDU t0,a1
	bltu    t0,a0,1b                # a0 is termination count

	LI      t0,K0_RAMBASE
	PTR_ADDU a0,t0          # a0: cache size + K0base for loop termination
	.set	noreorder
1:	cache	CACH_SD|C_IINV, 0(t0)
	nop
	cache	CACH_SD|C_IINV, 1(t0)
	.set    reorder
	PTR_ADDU t0,a1
	bltu    t0,a0,1b                # a0 is termination count

	j       ra





2:
	.set    noreorder
	cache   CACH_SD|C_IST,0(t0)
	.set    reorder
	PTR_ADDU t0,a1
	bltu    t0,a0,2b                # a0 is termination count

	j       ra
	END(invalidate_scache)

#if R10000
/*
 * Function:	sCacheSize
 * Purpose:	determine the size of the secondary cache
 * Returns:	secondary cache size in bytes in v0
 */
LEAF(sCacheSize)
        .set	noreorder
	mfc0	v1,C0_CONFIG
#ifdef R4000
	and	v1,CONFIG_R10K_SS
	dsrl	v1,CONFIG_R10K_SS_SHFT
#else /* R4000 */
	and	v1,CONFIG_SS
	dsrl	v1,CONFIG_SS_SHFT
#endif /* R4000 */
	dadd	v1,CONFIG_SCACHE_POW2_BASE
	li	v0,1
	li	t3,SCACHE_LINESIZE
	sw	t3,_scache_linesize
	li	t3,SCACHE_LINEMASK
	sw	t3,_scache_linemask
	j	ra
	dsll	v0,v1			# cache size in byte
        .set	reorder
        END(sCacheSize)



/*
 * Function:	iCacheSize
 * Purpose:	determine the size of the primary instruction cache
 * Returns:	primary cache size in bytes in v0
 */
LEAF(iCacheSize)
	.set	noreorder
	mfc0	v1,C0_CONFIG
	and	v1,CONFIG_IC
	dsrl	v1,CONFIG_IC_SHFT
	dadd	v1,CONFIG_PCACHE_POW2_BASE
	li	v0,1
	j	ra
	dsll	v0,v1			# cache size in byte
        .set	reorder
        END(iCacheSize)



/*
 * Function:	dCacheSize
 * Purpose:	determine the size of the primary data cache
 * Returns:	primary data cache size in bytes in v0
 */
LEAF(dCacheSize)
        .set	noreorder
	mfc0	v1,C0_CONFIG
	and	v1,CONFIG_DC
	dsrl	v1,CONFIG_DC_SHFT	
	dadd	v1,CONFIG_PCACHE_POW2_BASE
	li	v0,1
	j	ra
	dsll	v0,v1			# cache size in byte
        .set	reorder
        END(dCacheSize)


/*
 * Function:	iLine
 * Purpose:	Fetch an instruction line from the icache.
 * Parameters:	a0 - vaddr (low order bit signals way)
 *		a1 - ptr to il_t buffer
 * Returns:	a1-> filled in.
 */
LEAF(iLine)
	.set	noreorder

	ICACHE(C_ILT, 0(a0))		/* Store Tag */
	MFC0(v0, C0_TAGLO)
	MFC0(v1, C0_TAGHI)
	dsll	v1,32
	or	v0,v1
	sd	v0,IL_TAG(a1)

	daddiu	t0,a1,IL_DATA		/* Data pointer   */
	daddiu	t1,a1,IL_PARITY		/* Parity pointer */

	daddiu	a2,a0,CACHE_ILINE_SIZE
1:		
	ICACHE(C_ILD, 0(a0))
	MFC0(v0, C0_TAGLO)
	MFC0(v1, C0_TAGHI)
	dsll	v1,32
	or	v0,v1
	sd	v0,0(t0)
	
	MFC0(v0, C0_ECC)
	sb	v0,0(t1)
	daddiu	t0,8			/* 8-bytes stored */
	daddiu	a0,4
	blt	a0, a2, 1b
	daddiu	t1,1			/* 1-bit of parity */
	
	j	ra
	nop
	.set	reorder
	END(iLine)

/*
 * Function:	dLine
 * Purpose:	Fetch primary data cache line.
 * Parameters:	a0 - vaddr (low order bit signals way)
 *		a1 - ptr to dl_t
 * Returns:	a1--> filled in.
 */
LEAF(dLine)
	.set	noreorder
	
	DCACHE(C_ILT, 0(a0))		/* Read and store tag */
	MFC0(v0, C0_TAGHI)
	MFC0(v1, C0_TAGLO)
	dsll	v0,32
	or	v0,v1
	sd	v0,DL_TAG(a1)

	daddiu	a2,a0,CACHE_DLINE_SIZE
	daddiu	t0,a1,DL_DATA		/* Data pointer */
	daddiu	t1,a1,DL_ECC		/* ECC pointer */

1:	
	DCACHE(C_ILD, 0(a0))
	MFC0(v0, C0_TAGLO)
	sw	v0,0(t0)
	MFC0(v0, C0_ECC)
	sb	v0,0(t1)
	daddiu	a0,4			/* Next word - vaddr */
	daddiu	t0,4			/* Next word - buffer */
	blt	a0,a2,1b
	daddiu	t1,1			/* DELAY: Next ECC field */

	j	ra
	nop
	
	.set	reorder
	END(dLine)
	
/*
 * Function:	sLine
 * Purpose:	Fetch a cache line from the secondary cache.
 * Parameters:	a0 - vaddr (low order bit signals way)
 *		a1 - pointer to sl_t area.
 * Returns:	nothing
 */
LEAF(sLine)

	.set	noreorder

	SCACHE(C_ILT, 0(a0))		/* Pick up T5 TAG */
	MFC0(v0, C0_TAGHI)
	MFC0(v1, C0_TAGLO)
	dsll	v0,32
	dsll	v1,32			/* Clear high order 32 bits */
	dsrl	v1,32
	or	v0,v1
	sd	v0, SL_TAG(a1)
	
	move	t0,ra			/* Pick up CC tag */
	jal	sCacheSize
	nop
	move	ra,t0

#ifdef EVEREST
	srl	v0,1			/* div by 2 for # ways */
	sub	v0,1			/* scachesize/2 - 1 */
	and	v0,a0,v0		/* V0 - normalizes virtual address */

	divu	v0,CACHE_SLINE_SIZE	/* Compute index */
	sll	v0,4			/* Index into duplicate tags */
	and	v1,a0,1			/* Look at way */
	sll	v1,3			/* add 8-bytes for way 1 */
	daddu	v1,v0			/* Offset into TAGS */
	daddu	v1,EV_BTRAM_BASE	/* address of TAG */
#if SABLE
	move	v0,zero
#else
	ld	v0,0(v1)		/* duplicate tag */
#endif
	sd	v0,SL_CCTAG(a1)		/* and store for the caller */
#endif	/* EVEREST */

	/* OK - lets get the data */

	li	v0,CACHE_SLINE_SIZE/16	/* # fetches */
	daddiu	t0,a1,SL_DATA
	daddiu	t1,a1,SL_ECC
2:
	SCACHE(C_ILD, 0(a0))
	MFC0(v1, C0_TAGLO)		/* Store 8-bytes of data */
	sw	v1,0(t0)
	MFC0(v1, C0_TAGHI)
	sw	v1,4(t0)
	MFC0(v1, C0_ECC)
	sh	v1,0(t1)		/* Store ECC for this 8 bytes */
	
	SCACHE(C_ILD, 8(a0))
	MFC0(v1, C0_TAGLO)		/* Store 8-bytes of data */
	sw	v1,8(t0)
	MFC0(v1, C0_TAGHI)
	sw	v1,12(t0)
	
	sub	v0,1
	daddu	a0,16			/* Increment address */
	daddu	t0,16			/* Increment data array. */
	bgt	v0,zero,2b
	daddiu	t1,2			/* DELAY: Increment ECC array */
1:
	/* All done ... */
	j	ra
	nop
	.set	reorder
	END(sLine)



LEAF(cacheOP)
/*
 * Routine:	cacheOP
 * Purpose:	Perform a cache operation
 * Parameters:	a0 - cacheop_t * - pointer to cache op structure.
 * Returns:	v0 - undefined, for Load data, taghi, taglo, and ECC
 *		     values filled in.
 */
	.set	noreorder
	lw	v0,COP_TAGLO(a0)		/* First setup CP0 */
	MTC0(v0, C0_TAGLO)
	lw	v0,COP_TAGHI(a0)
	MTC0(v0, C0_TAGHI)
	lw	v0,COP_ECC(a0)
	MTC0(v0, C0_ECC)
	
	ld	a1,COP_ADDRESS(a0)		/* Address to use */
	lw	v1,COP_OP(a0)			/* Operation */
	dla	v0,cacheOP_table
	sll	v1,3				/* Index into table. */
	daddu	v0,v1
	jalr	a2,v0
	nop
	MFC0(v0, C0_TAGLO)
	sw	v0,COP_TAGLO(a0)
	MFC0(v0, C0_TAGHI)
	sw	v0,COP_TAGHI(a0)
	MFC0(v0, C0_ECC)
	.set	reorder
	sw	v0,COP_ECC(a0)
	j	ra

        /*
         * There are 32 possible operations to perform, each is
	 * defined in the table below, and uses 2 instructions (8 bytes).
         */

cacheOP_table:
	.set	noreorder
	jr	a2
	cache	0, 0(a1)
	jr	a2
	cache	1, 0(a1)
	jr	a2
	cache	2, 0(a1)
	jr	a2
	cache	3, 0(a1)
	jr	a2
	cache	4, 0(a1)
	jr	a2
	cache	5, 0(a1)
	jr	a2
	cache	6, 0(a1)
	jr	a2
	cache	7, 0(a1)
	jr	a2
	cache	8, 0(a1)
	jr	a2
	cache	9, 0(a1)
	jr	a2
	cache	10, 0(a1)
	jr	a2
	cache	11, 0(a1)
	jr	a2
	cache	12, 0(a1)
	jr	a2
	cache	13, 0(a1)
	jr	a2
	cache	14, 0(a1)
	jr	a2
	cache	15, 0(a1)
	jr	a2
	cache	16, 0(a1)
	jr	a2
	cache	17, 0(a1)
	jr	a2
	cache	18, 0(a1)
	jr	a2
	cache	19, 0(a1)
	jr	a2
	cache	20, 0(a1)
	jr	a2
	cache	21, 0(a1)
	jr	a2
	cache	22, 0(a1)
	jr	a2
	cache	23, 0(a1)
	jr	a2
	cache	24, 0(a1)
	jr	a2
	cache	25, 0(a1)
	jr	a2
	cache	26, 0(a1)
	jr	a2
	cache	27, 0(a1)
	jr	a2
	cache	28, 0(a1)
	jr	a2
	cache	29, 0(a1)
	jr	a2
	cache	30, 0(a1)
	jr	a2
	cache	31, 0(a1)
	.set	reorder
	END(cacheOP)
#endif
	

	.data
lmsg:	.asciiz	"cache.s"
#ifdef R4600SC
#ifdef _MEM_PARITY_WAR	
	.globl	_r4600sc_scache_disabled
#endif /* _MEM_PARITY_WAR */
_r4600sc_scache_disabled:	.word	1
#endif
#ifdef R4600SC_DEBUG
_r4600sc_2nd_flush_error:	.word	0
#endif
#ifdef R4600SC_STATS
_r4600sc_mapped:		.word	0	# number using mapped addresses
_r4600sc_unmapped:		.word	0	# number using unmapped 
_r4600sc_short:			.word	0	# number len lt 1 page
_r4600sc_long:			.word	0	# number len gt 1 page
_r4600sc_cross:			.word	0	# number len lt 1 page but 
						# cross page boundry
_r4600sc_nuke:			.word	0	# times cache is nuked
_r4600sc_mandc:			.word	0	# mapped and cross
#endif
#ifdef TRITON
	.sdata
	.globl	_triton_use_invall
_triton_use_invall:		.word	0	# enable C_INVALL
#endif /* TRITON */

