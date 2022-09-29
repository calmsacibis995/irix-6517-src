
#ident "$Revision: 1.9 $"

#include <asm.h>
#include <regdef.h>
#include <sys/cpu.h>
#include <sys/sbd.h>

/* Initialize the primary instruction cache from the power-up state */
LEAF(init_load_icache)
	lw	a0,_icache_size
	lw	v0,_icache_linesize

	# to initialize cache, we have to load every line up to
	# cache size.
	# if the scache exists, it has already been 'loaded' in
	# this way.
	# use the 'fill' cache op to fill from scache/memory.
	#
	# a0: cache size, v0: line size
	LI	t1,K0_RAMBASE
	addu	a0,t1			# a0 = K0base + cache size
1:
	.set	noreorder
	cache	CACH_PI|C_FILL,0(t1)
	.set	reorder
	addu	t1,v0			# step the address by line size
	bltu	t1,a0,1b

	j	ra
	END(init_load_icache)

/* Initialize the primary data cache from the power-up state */
LEAF(init_load_dcache)
	lw	a0,_dcache_size
	lw	v0,_dcache_linesize

	# to initialize cache, we have to load every line up to
	# cache size.
	# if the scache exists, it has already been 'loaded' in
	# this way.
	#
	# a0: cache size, v0: line size
	LI	t1,K0_RAMBASE
	addu	a0,t1			# a0 = K0base + cache size
1:
	lw	zero,0(t1)
	addu	t1,v0			# step the address by line size
	bltu	t1,a0,1b

	j	ra
	END(init_load_dcache)

/* Initialize the secondary cache from the power-up state */
LEAF(init_load_scache)
	lw	a0,_sidcache_size
	lw	v0,_scache_linesize

	# to initialize cache, we have to load every line up to
	# cache size
	#
	# a0: cache size, v0: line size
	LI	t1,K0_RAMBASE
	addu	a0,t1			# a0 = K0base + cache size
1:
	lw	zero,0(t1)
	addu	t1,v0			# step the address by line size
	bltu	t1,a0,1b

	j	ra
	END(init_load_scache)

/* Invalidate all tlb entries */
LEAF(init_tlb)
	.set	noreorder
	mtc0	zero,C0_TLBLO_0
	mtc0	zero,C0_TLBLO_1
	mtc0	zero, C0_PGMASK		# set 4K page mask
	LI	v0,K0BASE&TLBHI_VPNMASK
	mtc0	v0,C0_TLBHI             # invalidate entry
	move	v0,zero
1:
	mtc0	v0,C0_INX
	NOP_0_3
	c0	C0_WRITEI

	addiu	v0,1
	blt	v0,NTLBENTRIES,1b
	nop

	j	ra
	nop                             # BDSLOT
	.set	reorder
	END(init_tlb)

/*  r4400_hack_clear_DE should be just before clearing SR_DE for real.  It
 * works around a floating bit inside the r4400 that may cause cache ecc
 * errors when DE is cleared.  Since there is no external interface to
 * the problem bits, we must take the exception and reset the cpu via
 * MC since there appears to be no way to clear the problem.
 *
 *  Use rt clock nvram to make sure we do not get stuck in an infinite reset
 * loop.
 */
#ifndef ENETBOOT

#define	MC_RESET_FLAG_ADDR	(RT_CLOCK_ADDR+DS1286_RAMOFFSET+MC_IN_RESET*4)

LEAF(r4400_hack_handler)
	.set	noreorder
	nop ; nop ; nop ; nop

#ifdef DEBUG
	LA	a0,9f			# no need to save ra since we reset cpu
	jal	printf
	nop
#endif
	LI	a0,MC_RESET_FLAG_ADDR
	LI	a1,1
	sw	a1,0(a0)

	LI	k1,PHYS_TO_K1(CPUCTRL0)
	lw	k0,0(k1)
	nop
	LI	a1,CPUCTRL0_SIN		# reset the system
	or	k0,k0,a1
1:	sw	k0,0(k1)		# reset CPU
	nop
	lw	zero,0(k1)		# flush bus
	j	1b			# keep going till machine resets
	nop
	.set	reorder
	END(r4400_hack_handler)

LEAF(r4400_hack_clear_DE)
	.set	noreorder

	/* If MC_IN_RESET is set, we are comming up after the forced
	 * reset and don't want to set-up to take the exception.
	 */
	LI	v0,MC_RESET_FLAG_ADDR
	lw	v0,0(v0)		# read and set our retry flag
	nop
	beq	zero,v0,1f		# if unset go ahead with hack
	nop
#ifdef DEBUG
	move	k0,ra			# stow ra
	LA	a0,10f
	nop
	jal	printf
	nop
	move	ra,k0			# restore ra
#endif
	LI	v0,MC_RESET_FLAG_ADDR
	sw	zero,0(v0)

	j	ra			# return if this is second try
	nop

1:
#ifdef DEBUG
	move	k0,ra			# stow ra
	LA	a0,11f
	nop
	jal	printf
	nop
	move	ra,k0			# restore ra
#endif

	/* Set SR_BEV first to make sure its ready.
	 */
	MFC0	(v0,C0_SR)		# get SR
	li	v1,SR_BEV		# make sure we have early handler
	or	v0,v0,v1
	MTC0	(v0,C0_SR)		# enable early handler

	/* Now clear SR_DE and set-up our hack handler.
	 */
	li	v1,~SR_DE		# clear cache error prevention flag
	and	v0,v0,v1
	LA	fp,r4400_hack_handler	# set-up our hack handler
	nop				# addr of exception on bad power-up
	MTC0	(v0,C0_SR)		# enable cache errors
	NOP_0_4

	/*  If we hit the problem it would have happend on last mtc0, so
	 * if we are still going, we are home free -- clear SR_BEV.
	 */
	MFC0	(v0,C0_SR)		# turn off early handler
	li	k1,~SR_BEV
	and	v0,v0,k1
	MTC0	(v0,C0_SR)

	move	fp,zero			# erase bev handler pointer

	j	ra			# all done
	nop
	.set	reorder
	END(r4400_hack_clear_DE)
#else
LEAF(r4400_hack_clear_DE)
	j	ra
	END(r4400_hack_clear_DE)
#endif

#ifdef DEBUG
        .data
9:	.asciiz "\r\nr4400 ecc problem -- resetting cpu!\r\n"
10:	.asciiz "\r\nskip r4400 ecc problem.\r\n"
11:	.asciiz "\r\ntry r4400 ecc workaround.\r\n"
#endif
