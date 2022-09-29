
#include <asm.h>
#include <regdef.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <ml.h>

	AUTO_CACHE_BARRIERS_DISABLE

/* b2bdiaga: sw uc mem; sw gio, lw mc */
LEAF(b2bdiaga)
	ORD_CACHE_BARRIER_AT(0,sp)	# ensure destination regs defined
	LI	t0,PHYS_TO_K1(CPUCTRL0)
	move	t2,t0
	li	a2,0xff
	sw	zero,0(a0)
	sync
	sw	a2,0(a1)
	sync
	lw	zero,0(t0)
	sync

	sw	zero,0(a0)		# mem write
	sw	zero,0(a1)		# gio write
	lw	zero,0(t2)		# mc read

	sync				# flushbus
	lw	zero,0(t0)		# flushbus
	sync				# flushbus
	lw	zero,0(t0)		# flushbus
	lw	v0,0(a1)
	j	ra
	END(b2bdiaga)

/* b2bdiagb: sw uc mem; sw gio, lw mem */
LEAF(b2bdiagb)
	ORD_CACHE_BARRIER_AT(0,sp)	# ensure destination regs defined
	LI	t0,PHYS_TO_K1(CPUCTRL0)
	daddu	t2,a0,8
	li	a2,0xff
	sw	zero,0(a0)
	sync
	sw	a2,0(a1)
	sync
	lw	zero,0(t0)
	sync

	sw	zero,0(a0)		# mem write
	sw	zero,0(a1)		# gio write
	lw	zero,0(t2)		# mem read

	sync				# flushbus
	lw	zero,0(t0)		# flushbus
	sync				# flushbus
	lw	zero,0(t0)		# flushbus
	lw	v0,0(a1)
	j	ra
	END(b2bdiagb)

/* b2bdiagc: cache wb; sw gio, lw mc */
LEAF(b2bdiagc)
	ORD_CACHE_BARRIER_AT(0,sp)	# ensure destination regs defined
	LI	t0,PHYS_TO_K1(CPUCTRL0)
	LI	t2,PHYS_TO_K0_RAM(0x00c00080)
	sw	zero,0(t2)		# dirty line
	li	a2,0xff
	sw	zero,0(a0)
	sync
	sw	a2,0(a1)
	sync
	lw	zero,0(t0)
	sync

	.set	noreorder
	cache	CACH_SD|C_HWBINV,0(t2)	# writeback
	.set	reorder
	sw	zero,0(a1)		# gio write
	lw	zero,0(t0)		# mc load

	sync				# flushbus
	lw	zero,0(t0)		# flushbus
	sync				# flushbus
	lw	zero,0(t0)		# flushbus
	lw	v0,0(a1)
	j	ra
	END(b2bdiagc)

/* b2bdiagd: sw gio, sw gio, lw mc */
LEAF(b2bdiagd)
	ORD_CACHE_BARRIER_AT(0,sp)	# ensure destination regs defined
	LI	t0,PHYS_TO_K1(CPUCTRL0)
	daddu	t2,a0,8
	li	a2,0xff
	sw	zero,0(a0)
	sync
	sw	a2,0(a1)
	sync
	lw	zero,0(t0)
	sync

	sw	zero,8(a1)		# gio write
	sw	zero,0(a1)		# gio write
	lw	zero,0(t2)		# mem read

	sync				# flushbus
	lw	zero,0(t0)		# flushbus
	sync				# flushbus
	lw	zero,0(t0)		# flushbus
	lw	v0,0(a1)
	j	ra
	END(b2bdiagd)

/* b2bdiage: sw mem, sw mem, lw mc */
LEAF(b2bdiage)
	ORD_CACHE_BARRIER_AT(0,sp)	# ensure destination regs defined
	LI	t0,PHYS_TO_K1(CPUCTRL0)
	daddu	t2,a0,8
	li	a2,0xff
	sw	zero,0(a0)
	sync
	daddiu	a1,a0,8			# mem alias is second target
	sw	a2,0(a1)
	sync
	lw	zero,0(t0)
	sync

	sw	zero,0(a0)		# mem write
	sw	zero,0(a1)		# mem write #2
	lw	zero,0(t2)		# mem read

	sync				# flushbus
	lw	zero,0(t0)		# flushbus
	sync				# flushbus
	lw	zero,0(t0)		# flushbus
	lw	v0,0(a1)
	j	ra
	END(b2bdiage)

/* b2bdiagf: sw mem; sw gio, cache ld */
LEAF(b2bdiagf)
	ORD_CACHE_BARRIER_AT(0,sp)	# ensure destination regs defined
	LI	t0,PHYS_TO_K1(CPUCTRL0)
	LI	t2,PHYS_TO_K0_RAM(0x00c00080)
	li	a2,0xff
	.set	noreorder
	cache	CACH_SD|C_HWBINV,0(t2)	# ensure not in cache
	.set	reorder
	sw	zero,0(a0)
	sync
	sw	a2,0(a1)
	sync
	lw	zero,0(t0)
	sync

	.set	noreorder
	sw	zero,0(a0)		# mem write
	sw	zero,0(a1)		# gio write
	nop;nop
	lw	zero,0(t2)		# cache miss
	.set	reorder

	sync				# flushbus
	lw	zero,0(t0)		# flushbus
	sync				# flushbus
	lw	zero,0(t0)		# flushbus
	lw	v0,0(a1)
	j	ra
	END(b2bdiagf)

/* b2bdiagas: sw uc mem; sw gio, sync, lw mc */
LEAF(b2bdiagas)
	LI	t0,PHYS_TO_K1(CPUCTRL0)
	move	t2,t0
	li	a2,0xff
	sw	zero,0(a0)
	sync
	sw	a2,0(a1)
	sync
	lw	zero,0(t0)
	sync

	sw	zero,0(a0)		# mem write
	sw	zero,0(a1)		# gio write
	sync
	lw	zero,0(t2)		# mc read

	sync				# flushbus
	lw	zero,0(t0)		# flushbus
	sync				# flushbus
	lw	zero,0(t0)		# flushbus
	lw	v0,0(a1)
	j	ra
	END(b2bdiagas)

/* b2bdiagg: sw uc mem; sw gio, lw prom */
LEAF(b2bdiagg)
	ORD_CACHE_BARRIER_AT(0,sp)	# ensure destination regs defined
	LI	t0,PHYS_TO_K1(0x1fc00000)
	move	t2,t0
	li	a2,0xff
	sw	zero,0(a0)
	sync
	sw	a2,0(a1)
	sync
	lw	zero,0(t0)
	sync

	sw	zero,0(a0)		# mem write
	sw	zero,0(a1)		# gio write
	lw	zero,0(t2)		# mc read

	sync				# flushbus
	lw	zero,0(t0)		# flushbus
	sync				# flushbus
	lw	zero,0(t0)		# flushbus
	lw	v0,0(a1)
	j	ra
	END(b2bdiagg)

/* b2bdiagh: sw uc mem; sw gio, sw mem */
LEAF(b2bdiagh)
	ORD_CACHE_BARRIER_AT(0,sp)	# ensure destination regs defined
	LI	t0,PHYS_TO_K1(CPUCTRL0)
	move	t2,t0
	li	a2,0xff
	sw	zero,0(a0)
	sync
	sw	a2,0(a1)
	sync
	lw	zero,0(t0)
	sync

	sw	zero,0(a0)		# mem write
	sw	zero,0(a1)		# gio write
	sw	zero,8(a0)		# mem write

	sync				# flushbus
	lw	zero,0(t0)		# flushbus
	sync				# flushbus
	lw	zero,0(t0)		# flushbus
	lw	v0,0(a1)
	j	ra
	END(b2bdiagh)

	AUTO_CACHE_BARRIERS_ENABLE
