/***************************************************************************
 *   arcsasm.s -							   *
 * 	Contains assembly routines needed by systems with ARCS proms and   *
 *	64-bit kernels.							   *
 ***************************************************************************/

#if defined(_K64PROM32) || defined(_KN32PROM32)

#include "ml.h"

/*
 * int call_prom_cached(int parm 1, int parm2, int parm3, int parm4,
 *	                int *function()
 *
 * call_prom_cached() makes a call into a cached copy of the 32-bit ARCS prom
 * from a 64-bit kernel.
 */
LEAF(call_prom_cached)
	.set	noreorder
	LA	t0, real_return_address
	daddi	t0, 7
	li	t1, ~7
	and	t0, t1
	sd	ra, 0x00(t0)		# Save our ra.
	sd	s0, 0x08(t0)
	sd	s1, 0x10(t0)
	sd	s2, 0x18(t0)
	sd	s3, 0x20(t0)
	sd	s4, 0x28(t0)
	sd	s5, 0x30(t0)
	sd	s6, 0x38(t0)
	sd	s7, 0x40(t0)
	sd	s8, 0x48(t0)
	sd	sp, 0x50(t0)

	lui	t1, 0x8000

	# Generate a 32-bit return address
	LA	ra, 1f			# Get what _will_ be our ra.
	dsll	ra, 35			# Strip the top bits.
	dsrl	ra, 35			# Restore the physical address
	or	ra, t1			# Make it a 32-bit K0 address.

	# Convert the stack pointer to a 32-bit pointer
	# Save space for 32 bit arg space
	daddiu	sp, -64			# more than enough
	dsll	sp, 35			# Strip the top bits.
	dsrl	sp, 35			# Restore the physical address
	or	sp, t1			# Make it a 32-bit K0 address.
	j	a4
	nop

1:	# Make sure the KX bit is still on.	

	DMFC0(t0, C0_SR)
	NOP_0_4
	ori	t0, SR_KX
	NOP_0_2
	DMTC0(t0, C0_SR)
	NOP_0_4

	LA	t0, real_return_address
	daddi	t0, 7
	li	t1, ~7
	and	t0, t1
	ld	ra, 0x00(t0)
	ld	s0, 0x08(t0)
	ld	s1, 0x10(t0)
	ld	s2, 0x18(t0)
	ld	s3, 0x20(t0)
	ld	s4, 0x28(t0)
	ld	s5, 0x30(t0)
	ld	s6, 0x38(t0)
	ld	s7, 0x40(t0)
	ld	s8, 0x48(t0)
	ld	sp, 0x50(t0)
	nop
	j	ra
	nop
	END(call_prom_cached)

	.comm real_return_address, 8 * 33

#endif /* _K64PROM32 || _KN32PROM32 */
