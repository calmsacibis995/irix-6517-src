
#ident "$Revision: 1.5 $"

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
	li	t1,K0_RAMBASE
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
	li	t1,K0_RAMBASE
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
	li	t1,K0_RAMBASE
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
	li	v0,K0BASE&TLBHI_VPNMASK
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
