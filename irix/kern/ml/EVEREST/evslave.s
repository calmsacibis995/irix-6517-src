#include <sys/EVEREST/evmp.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/regdef.h>
#include <asm.h>

#if defined(MULTIKERNEL)	
#include <sys/EVEREST/evintr.h>
#endif
/*
 * void slave_loop(mpconf_t *)
 *	slave_loop watches this CPU's MPCONF launch address and jumps
 *		to any address stored into it.
 */
LEAF(slave_loop)
	.set noreorder

	move	s0,a0
#if IP21
	/* mdeneroff says we should run IP21 at low3v */
	li	t0, EV_VCR_LOW3V
	LI	t1, EV_VOLTAGE_CTRL
	sd	t0, (t1)
#endif	
#if IP19
	jal	ip19_cache_init		/* Set up the caches */
	nop
#endif
	K32TOKPHYS(s0, t0, t1)		/* Turn into real 64 bit pointer */
	move	a0,s0
	/*
	 * Indicate we are ready ....
	 */
	KPHYSTOK1(a0);			/* MPCONF_ADDR is in K1 */
	lbu	t0,MP_VIRTID(s0)
	LA	t1,slave_loop_ready
	PTR_ADDU t0,t1			/* Address of byte to set */
	li	t1,1
#ifdef MAPPED_KERNEL
	KPHYSTOK0(t0)			/* can't use virtual (K2) address */
#endif /* MAPPED_KERNEL */	
	sb	t1,0(t0)		/* Bang - were ready! */
1:
#if defined(MULTIKERNEL)
	/* make sure no stale debug interrupts when we start up */
	LA	t0, EV_CIPL0
	LI	t1, EVINTR_LEVEL_DEBUG
	sd	t1, (t0)
#endif	
	li	t0,0xffffff

	# Avoid hogging bus while CPU0 is clearing memory by
	# spinning in a tight cached loop.
2:	bne	t0,zero,2b
	subu	t0,1			# (BD)

	lw	t0, MP_LAUNCHOFF(a0)
	beqz	t0, 1b
	nop				# (BD)

	lw	t1, MP_MAGICOFF(a0)
	# form a 32 bit sign-extended constant to match
	# the sign-extended result of the lw above.
	# (The Ragnarok compiler may not treat the 'li' macro
	# properly, so make it explicit.
	lui	t2, (MPCONF_MAGIC >> 16)
	ori	t2, (MPCONF_MAGIC & 0xffff)
	
	beq	t1, t2, 2f
	nop

	LI	t1, EV_PROM_FLASHLEDS	# Go away forever.
	j	t1
	nop

2:
	# Load the real stack pointer.
	PTR_L	sp, MP_REAL_SP(a0)
	sw	zero, MP_LAUNCHOFF(a0)	# Clear the launch address
	lw	a0, MP_LPARM(a0)	# Load pointer to MPCONF
	K32TOKPHYS(a0, t1, t2)		# Convert to 64-bit address 

	# Create a 64-bit address if necessary.  Use t1, t2 as temp regs.
	K32TOKPHYS(t0, t1, t2)

	jal	t0
	nop				# (BD)

	# Should never return
	LI	t1, EV_PROM_FLASHLEDS	# Go away forever.
	j	t1
	nop

	END(slave_loop)

	.align 7
LEAF(slave_entry)
	LA	t0,slave_loop
#ifdef MAPPED_KERNEL
	KPHYSTOK0(t0)			/* can't use virtual (K2) address */
#endif /* MAPPED_KERNEL */	
	j	t0
	nop
	END(slave_entry)
	.align 7

LEAF(start_slave_loop)
	.set	noreorder
	/* 
	 *Launch slaves into kernel slave loop, in a stackless function.
	 */
	LA	t2,slave_entry
	
	KPHYSTO32K1(t2);

	LI	t0,MPCONF_ADDR
	li	t1,EV_MAX_CPUS
1:
	/*
	 * Zero out stuff that should be zero (for saftey).
	 */
	sw	zero,MP_STACKADDR(t0)
#if _MIPS_SIM == _ABI64
	PTR_S	zero, MP_REAL_SP(t0)
#else
	sw	zero,FILLER(t0)
	sw	zero,FILLER+4(t0)
#endif
	move	t3,t0			/* MPCONF pointer */
	KPHYSTO32K1(t3)
	sw	t3,MP_LPARM(t0)
	sw	t2,MP_LAUNCHOFF(t0)

	sub	t1,1
	bgtz	t1,1b
	PTR_ADDU t0,MPCONF_SIZE

	/* Wait a while to let them get going .... */

	li	t0,0x3fffffc
2:	bgtz	t0,2b
	sub	t0,1

	j	ra
	nop
	END(start_slave_loop)
