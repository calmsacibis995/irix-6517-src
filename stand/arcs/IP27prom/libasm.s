/***********************************************************************\
*       File:           libasm.s                                        *
*                                                                       *
*       Contains subroutines which allow C functions to manipulate      *
*       the machine's low-level state.  In some cases, these            *
*       functions do little more than provide a C interface to          *
*       the machine's native assembly language.                         *
*                                                                       *
\***********************************************************************/

#ident "$Revision: 1.39 $"

#include <asm.h>

#include <sys/mips_addrspace.h>
#include <sys/regdef.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/sbd.h>
#include <sys/fpu.h>

#include <sys/SN/SN0/IP27.h>

#include "ip27prom.h"
#include "pod.h"
#include "libasm.h"

/*
 * Function:	parity
 * Purpose:	Compute parity of 64-bit value
 * Parameters:	a0 - 64-bit value to compute parity on
 * Returns:	v0 - parity bit in LSB
 * Notes:	uses a0,v0
 */

	.set	reorder

LEAF(parity)
	dsrl	v0, a0, 32
	xor	a0, v0
	dsrl	v0, a0, 16
	xor	a0, v0
	dsrl	v0, a0, 8
	xor	a0, v0
	dsrl	v0, a0, 4
	xor	a0, v0
	dsrl	v0, a0, 2
	xor	a0, v0
	dsrl	v0, a0, 1
	xor	a0, v0
	and	v0, a0, 1
	j	ra
	END(parity)

/*
 * ulong bitcount(ulong x)
 *
 *   Uses v0, v1, a0
 */

LEAF(bitcount)
	li	v0, 0
1:	beqz	a0, 2f
	dsubu	v1, a0, 1
	and	a0, v1
	add	v0, 1
	b	1b
2:	j	ra
	END(bitcount)

/*
 * ulong firstone(ulong x)
 *
 *   Uses v0, v1, a0, a1
 *   Returns 64 if no bits are set.
 */

LEAF(firstone)
	li	v0, 0
	li	v1, 1
1:	and	a1, a0, v1
	bnez	a1, 2f
	dsll	v1, 1
	add	v0, 1
	blt	v0, 64, 1b
2:	j	ra
	END(firstone)

/*
 * qs128	Special function for libc's memsum routine.
 *		The compiler was generating unfortunate code.
 */

LEAF(qs128)
	dli	a2, 0x00ff00ff00ff00ff
	li	a3, 128
	li	v0, 0
1:	ld	t0, 0(a0)
	daddiu	a0, 8
	addiu	a3, -1
	and	t1, t0, a2
	daddu	v0, t1
	dsrl	t1, t0, 8
	and	t1, a2
	daddu	v0, t1
	bnez	a3, 1b
	j	ra
	END(qs128)

/*
 * Function:	gcd
 * Purpose:	Compute gcd of two 64-bit unsigned values
 * Parameters:	a0, a1
 * Returns:	v0
 * Notes:	uses a0,a1,v0
 */

LEAF(gcd)
	bnez	a0, 1f
	move	v0, a1
	j	ra
1:	bnez	a1, 1f
	move	v0, a0
	j	ra
1:	bge	a0, a1, 1f
	ddivu	zero, a1, a0
	mfhi	a1
	b	gcd
1:	ddivu	zero, a0, a1
	move	a0, a1
	mfhi	a1
	b	gcd
	END(gcd)

	.set	noreorder

LEAF(get_cpu_irr)
	MFC0(v0, C0_PRID)
	j	ra
	 nop
	END(get_cpu_irr)

LEAF(get_fpc_irr)
	cfc1	v0,fpc_irr
	nop
	j	ra
	nop
	END(get_fpc_irr)

LEAF(set_BSR)
	DMTBR(a0, BR_BSR)
	j	ra
	 nop
	END(set_BSR)

LEAF(get_BSR)
	DMFBR(v0, BR_BSR)
	j	ra
	 nop
	END(get_BSR)

LEAF(set_libc_device)
	DMTBR(a0, BR_LIBC_DEVICE)
	j	ra
	 nop
	END(set_libc_device)

LEAF(get_libc_device)
	DMFBR(v0, BR_LIBC_DEVICE)
	j	ra
	 nop
	END(get_libc_device)

LEAF(set_libc_verbosity)
	DMFBR(v0, BR_BSR)
	and	v0, ~BSR_VERBOSE
	beqz	a0, 1f
	 nop
	or	v0, BSR_VERBOSE
1:	DMTBR(v0, BR_BSR)
	j	ra
	 nop
	END(set_libc_verbosity)

LEAF(get_libc_verbosity)
	DMFBR(v0, BR_BSR)
	dsrl	v0, BSR_VERBOSE_SHFT
	j	ra
	 and	v0, 1
	END(get_libc_verbosity)

LEAF(set_ioc3uart_base)
	DMTBR(a0, BR_IOC3UART_BASE)
	j	ra
	 nop
	END(set_ioc3uart_base)

LEAF(get_ioc3uart_base)
	DMFBR(v0, BR_IOC3UART_BASE)
	j	ra
	 nop
	END(get_ioc3uart_base)

LEAF(get_ioc3uart_baud)
	li	v0, 9600
	j	ra
	 nop
	END(get_ioc3uart_baud)

LEAF(set_elsc)
	DMTBR(a0, BR_ELSC_SPACE)
	j	ra
	 nop
	END(set_elsc)

LEAF(get_elsc)
	DMFBR(v0, BR_ELSC_SPACE)
	j	ra
	 nop
	END(get_elsc)

LEAF(set_sp)
	j	ra
	 move	sp, a0
	END(set_sp)

LEAF(get_sp)
	j	ra
	 move	v0, sp
	END(get_sp)

LEAF(set_gp)
	j	ra
	 move	gp, a0
	END(set_gp)

LEAF(get_gp)
	j	ra
	 move	v0, gp
	END(get_gp)

LEAF(get_pc)
	j	ra
	 move	v0, ra
	END(get_pc)

LEAF(set_pi_eint_pend)
	DMTBR(a0, BR_ERR_PI_EINT_PEND)
	j	ra
	 nop
	END(set_pi_eint_pend)

LEAF(set_pi_err_sts0_a)
	DMTBR(a0, BR_ERR_PI_ESTS0_A)
	j	ra
	 nop
	END(set_pi_err_sts0_a)

LEAF(set_pi_err_sts0_b)
	DMTBR(a0, BR_ERR_PI_ESTS0_B)
	j	ra
	 nop
	END(set_pi_err_sts0_b)

LEAF(set_pi_err_sts1_a)
	DMTBR(a0, BR_ERR_PI_ESTS1_A)
	j	ra
	 nop
	END(set_pi_err_sts1_a)

LEAF(set_pi_err_sts1_b)
	DMTBR(a0, BR_ERR_PI_ESTS1_B)
	j	ra
	 nop
	END(set_pi_err_sts1_b)

LEAF(set_md_dir_error)
	DMTBR(a0, BR_ERR_MD_DIR_ERROR)
	j	ra
	 nop
	END(set_md_dir_error)

LEAF(set_md_mem_error)
	DMTBR(a0, BR_ERR_MD_MEM_ERROR)
	j	ra
	 nop
	END(set_md_mem_error)

LEAF(set_md_proto_error)
	DMTBR(a0, BR_ERR_MD_PROTO_ERROR)
	j	ra
	 nop
	END(set_md_proto_error)

LEAF(set_md_misc_error)
	DMTBR(a0, BR_ERR_MD_MISC_ERROR)
	j	ra
	 nop
	END(set_md_misc_error)


LEAF(get_pi_eint_pend)
	DMFBR(v0, BR_ERR_PI_EINT_PEND)
	j	ra
	 nop
	END(get_pi_eint_pend)

LEAF(get_pi_err_sts0_a)
	DMFBR(v0, BR_ERR_PI_ESTS0_A)
	j	ra
	 nop
	END(get_pi_err_sts0_a)

LEAF(get_pi_err_sts0_b)
	DMFBR(v0, BR_ERR_PI_ESTS0_B)
	j	ra
	 nop
	END(get_pi_err_sts0_b)

LEAF(get_pi_err_sts1_a)
	DMFBR(v0, BR_ERR_PI_ESTS1_A)
	j	ra
	 nop
	END(get_pi_err_sts1_a)

LEAF(get_pi_err_sts1_b)
	DMFBR(v0, BR_ERR_PI_ESTS1_B)
	j	ra
	 nop
	END(get_pi_err_sts1_b)

LEAF(get_md_dir_error)
	DMFBR(v0, BR_ERR_MD_DIR_ERROR)
	j	ra
	 nop
	END(get_md_dir_error)

LEAF(get_md_mem_error)
	DMFBR(v0, BR_ERR_MD_MEM_ERROR)
	j	ra
	 nop
	END(get_md_mem_error)

LEAF(get_md_proto_error)
	DMFBR(v0, BR_ERR_MD_PROTO_ERROR)
	j	ra
	 nop
	END(get_md_proto_error)

LEAF(get_md_misc_error)
	DMFBR(v0, BR_ERR_MD_MISC_ERROR)
	j	ra
	 nop
	END(get_md_misc_error)

LEAF(get_cop0)
	dsll	v0, a0, 3
	JUMP_TABLE(v0)
	j	ra;	dmfc0	v0, $0
	j	ra;	dmfc0	v0, $1
	j	ra;	dmfc0	v0, $2
	j	ra;	dmfc0	v0, $3
	j	ra;	dmfc0	v0, $4
	j	ra;	dmfc0	v0, $5
	j	ra;	dmfc0	v0, $6
	j	ra;	dmfc0	v0, $7
	j	ra;	dmfc0	v0, $8
	j	ra;	dmfc0	v0, $9
	j	ra;	dmfc0	v0, $10
	j	ra;	dmfc0	v0, $11
	j	ra;	dmfc0	v0, $12
	j	ra;	dmfc0	v0, $13
	j	ra;	dmfc0	v0, $14
	j	ra;	dmfc0	v0, $15
	j	ra;	dmfc0	v0, $16
	j	ra;	dmfc0	v0, $17
	j	ra;	dmfc0	v0, $18
	j	ra;	dmfc0	v0, $19
	j	ra;	dmfc0	v0, $20
	j	ra;	dmfc0	v0, $21
	j	ra;	dmfc0	v0, $22
	j	ra;	dmfc0	v0, $23
	j	ra;	dmfc0	v0, $24
	j	ra;	dmfc0	v0, $25
	j	ra;	dmfc0	v0, $26
	j	ra;	dmfc0	v0, $27
	j	ra;	dmfc0	v0, $28
	j	ra;	dmfc0	v0, $29
	j	ra;	dmfc0	v0, $30
	j	ra;	dmfc0	v0, $31
	END(get_cop0)

LEAF(set_cop0)
	dsll	v0, a0, 3
	JUMP_TABLE(v0)
	j	ra;	dmtc0	a1, $0
	j	ra;	dmtc0	a1, $1
	j	ra;	dmtc0	a1, $2
	j	ra;	dmtc0	a1, $3
	j	ra;	dmtc0	a1, $4
	j	ra;	dmtc0	a1, $5
	j	ra;	dmtc0	a1, $6
	j	ra;	dmtc0	a1, $7
	j	ra;	dmtc0	a1, $8
	j	ra;	dmtc0	a1, $9
	j	ra;	dmtc0	a1, $10
	j	ra;	dmtc0	a1, $11
	j	ra;	dmtc0	a1, $12
	j	ra;	dmtc0	a1, $13
	j	ra;	dmtc0	a1, $14
	j	ra;	dmtc0	a1, $15
	j	ra;	dmtc0	a1, $16
	j	ra;	dmtc0	a1, $17
	j	ra;	dmtc0	a1, $18
	j	ra;	dmtc0	a1, $19
	j	ra;	dmtc0	a1, $20
	j	ra;	dmtc0	a1, $21
	j	ra;	dmtc0	a1, $22
	j	ra;	dmtc0	a1, $23
	j	ra;	dmtc0	a1, $24
	j	ra;	dmtc0	a1, $25
	j	ra;	dmtc0	a1, $26
	j	ra;	dmtc0	a1, $27
	j	ra;	dmtc0	a1, $28
	j	ra;	dmtc0	a1, $29
	j	ra;	dmtc0	a1, $30
	j	ra;	dmtc0	a1, $31
	END(set_cop0)

LEAF(get_cop1)
	dsll	v0, a0, 3
	JUMP_TABLE(v0)
	j	ra;	dmfc1	v0, $f0
	j	ra;	dmfc1	v0, $f1
	j	ra;	dmfc1	v0, $f2
	j	ra;	dmfc1	v0, $f3
	j	ra;	dmfc1	v0, $f4
	j	ra;	dmfc1	v0, $f5
	j	ra;	dmfc1	v0, $f6
	j	ra;	dmfc1	v0, $f7
	j	ra;	dmfc1	v0, $f8
	j	ra;	dmfc1	v0, $f9
	j	ra;	dmfc1	v0, $f10
	j	ra;	dmfc1	v0, $f11
	j	ra;	dmfc1	v0, $f12
	j	ra;	dmfc1	v0, $f13
	j	ra;	dmfc1	v0, $f14
	j	ra;	dmfc1	v0, $f15
	j	ra;	dmfc1	v0, $f16
	j	ra;	dmfc1	v0, $f17
	j	ra;	dmfc1	v0, $f18
	j	ra;	dmfc1	v0, $f19
	j	ra;	dmfc1	v0, $f20
	j	ra;	dmfc1	v0, $f21
	j	ra;	dmfc1	v0, $f22
	j	ra;	dmfc1	v0, $f23
	j	ra;	dmfc1	v0, $f24
	j	ra;	dmfc1	v0, $f25
	j	ra;	dmfc1	v0, $f26
	j	ra;	dmfc1	v0, $f27
	j	ra;	dmfc1	v0, $f28
	j	ra;	dmfc1	v0, $f29
	j	ra;	dmfc1	v0, $f30
	j	ra;	dmfc1	v0, $f31
	END(get_cop1)

LEAF(set_cop1)
	dsll	v0, a0, 3
	JUMP_TABLE(v0)
	j	ra;	dmtc1	a1, $f0
	j	ra;	dmtc1	a1, $f1
	j	ra;	dmtc1	a1, $f2
	j	ra;	dmtc1	a1, $f3
	j	ra;	dmtc1	a1, $f4
	j	ra;	dmtc1	a1, $f5
	j	ra;	dmtc1	a1, $f6
	j	ra;	dmtc1	a1, $f7
	j	ra;	dmtc1	a1, $f8
	j	ra;	dmtc1	a1, $f9
	j	ra;	dmtc1	a1, $f10
	j	ra;	dmtc1	a1, $f11
	j	ra;	dmtc1	a1, $f12
	j	ra;	dmtc1	a1, $f13
	j	ra;	dmtc1	a1, $f14
	j	ra;	dmtc1	a1, $f15
	j	ra;	dmtc1	a1, $f16
	j	ra;	dmtc1	a1, $f17
	j	ra;	dmtc1	a1, $f18
	j	ra;	dmtc1	a1, $f19
	j	ra;	dmtc1	a1, $f20
	j	ra;	dmtc1	a1, $f21
	j	ra;	dmtc1	a1, $f22
	j	ra;	dmtc1	a1, $f23
	j	ra;	dmtc1	a1, $f24
	j	ra;	dmtc1	a1, $f25
	j	ra;	dmtc1	a1, $f26
	j	ra;	dmtc1	a1, $f27
	j	ra;	dmtc1	a1, $f28
	j	ra;	dmtc1	a1, $f29
	j	ra;	dmtc1	a1, $f30
	j	ra;	dmtc1	a1, $f31
	END(set_cop1)

/*
 * Delay uses the smallest possible loop to loop a specified number of times.
 * For accurate micro-second delays based on the hub clock, see rtc.s.
 */

LEAF(delay)
1:
	bnez	a0, 1b
	 daddu	a0, -1
	j	ra
	 nop
	END(delay)

LEAF(halt)
#ifdef SABLE
	dmtc0	zero, $7		# Magic clue for Sable to exit
#endif /* SABLE */
1:
	b	1b
	 nop
	END(halt)

LEAF(sync_instr)
	sync
	j	ra
	 nop
	END(sync_instr)

/* Get the endianness of this cpu from the config register */
LEAF(get_endian)
	MFC0(v0, C0_CONFIG)
	dsrl	v0, CONFIG_BE_SHFT	# shift BE to bit position 0
	and	v0, 1
	xor	v0, 1			# big = 0, little = 1
	j	ra
	 nop
	END(get_endian)

/*
 * jump_inval
 *
 *  Jumps to an address, possibly invalidating selected caches and/or
 *  switching the stack pointer.
 *
 *  a0: address to jump to
 *  a1: first parameter to pass
 *  a2: second parameter to pass
 *  a3: which caches to invalidate (combination of JINV_* flags)
 *  a4: new stack pointer, may be 0 to leave it unchanged
 *
 *  Return value:
 *	jump_inval is sneaky.  In order to avoid saving its return address, it
 *	does a jump rather than a jump and link to the routine called.  This
 *	means that the return value is whatever the called function leaves in
 *	v0.
 */

LEAF(jump_inval)
	move	s3, a0			# Save args
	move	s4, a1			# Kludge: this code knows what regs
	move	s5, a2			# are not trashed by cache code
	move	s6, a3
	move	s7, a4
	move	s0, ra			# Save the ra

	and	a0, s6, JINV_ICACHE	# Possibly invalidate I-cache
	beq	a0, zero, 1f
	 nop
	jal	cache_inval_i
	 nop
1:
	and	a0, s6, JINV_DCACHE	# Possibly invalidate D-cache
	beq	a0, zero, 1f
	 nop
	jal	cache_inval_d
	 nop
1:
	and	a0, s6, JINV_SCACHE	# Possibly invalidate S-cache
	beq	a0, zero, 1f
	 nop
	jal	cache_inval_s
	 nop
1:
	move	ra, s0			# Restore ra
	move	a0, s4			# Set up parameters
	move	a1, s5
	# switch sp, if s7 is not zero.
	beq	s7, zero, 1f
	nop
	move	sp, s7
1:
	j	s3
	 nop
	# Never returns
	END(jump_inval)

/*
 * save_and_call
 *
 * a0 = sregs addr
 * a1 = function to call
 * a2 = paramater
 *
 * Returns the return value of function called
 */

LEAF(save_and_call)
	sd	s0, 0(a0)
	sd	s1, 8(a0)
	sd	s2, 16(a0)
	sd	s3, 24(a0)
	sd	s4, 32(a0)
	sd	s5, 40(a0)
	sd	s6, 48(a0)
	sd	s7, 56(a0)
	sd	fp, 64(a0)	# fp == s8
	sd	ra, 72(a0)

	move	s7, a0

	jal	a1
	 move	a0, a2

	move	a0, s7
	ld	s0, 0(a0)
	ld	s1, 8(a0)
	ld	s2, 16(a0)
	ld	s3, 24(a0)
	ld	s4, 32(a0)
	ld	s5, 40(a0)
	ld	s6, 48(a0)
	ld	s7, 56(a0)
	ld	fp, 64(a0)	# fp == s8
	ld	ra, 72(a0)

	j	ra
	 nop

	END(save_and_call)

LEAF(save_sregs)

	sd	s0, 0(a0)
	sd	s1, 8(a0)
	sd	s2, 16(a0)
	sd	s3, 24(a0)
	sd	s4, 32(a0)
	sd	s5, 40(a0)
	sd	s6, 48(a0)
	sd	s7, 56(a0)
	sd	fp, 64(a0)	# fp == s8

	j	ra
	 nop

	END(save_sregs)

LEAF(restore_sregs)

	ld	s0, 0(a0)
	ld	s1, 8(a0)
	ld	s2, 16(a0)
	ld	s3, 24(a0)
	ld	s4, 32(a0)
	ld	s5, 40(a0)
	ld	s6, 48(a0)
	ld	s7, 56(a0)
	ld	fp, 64(a0)	# fp == s8

	j	ra
	 nop

	END(restore_sregs)

LEAF(save_gprs)
	sd	AT, R1_OFF(a0)
	sd	v0, R2_OFF(a0)
	sd	v1, R3_OFF(a0)
	/* No point in saving a0 */
	sd	a1, R5_OFF(a0)
	sd	a2, R6_OFF(a0)
	sd	a3, R7_OFF(a0)
	sd	ta0, R8_OFF(a0)
	sd	ta1, R9_OFF(a0)
	sd	ta2, R10_OFF(a0)
	sd	ta3, R11_OFF(a0)
	sd	t0, R12_OFF(a0)
	sd	t1, R13_OFF(a0)
	sd	t2, R14_OFF(a0)
	sd	t3, R15_OFF(a0)
	sd	s0, R16_OFF(a0)
	sd	s1, R17_OFF(a0)
	sd	s2, R18_OFF(a0)
	sd	s3, R19_OFF(a0)
	sd	s4, R20_OFF(a0)
	sd	s5, R21_OFF(a0)
	sd	s6, R22_OFF(a0)
	sd	s7, R23_OFF(a0)
	sd	t8, R24_OFF(a0)
	sd	t9, R25_OFF(a0)
	sd	k0, R26_OFF(a0)
	sd	k1, R27_OFF(a0)
	sd	gp, R28_OFF(a0)
	sd	sp, R29_OFF(a0)
	sd	fp, R30_OFF(a0)
	/* No point in saving ra */
	j	ra
	 nop

	END(save_gprs)

/*
 * Pseudo-random number generator using various bits from various
 * counters in the IP27.  Unfortunately, this is not so random because
 * the system comes up synchronized so well!
 */

	.set	noreorder

LEAF(randnum)
	/* Get 12 bits from refresh counter */

	ld	v0, LOCAL_HUB(MD_REFRESH_CONTROL)
	dsrl	v0, MRC_COUNTER_SHFT
	and	v0, (MRC_COUNTER_MASK >> MRC_COUNTER_SHFT)

	/* Get 6 bits from T5 TLB random register */

	MFC0(v1, C0_RAND)
	and	v1, 63
	dsll	v1, 12
	or	v0, v1

	/* XOR with PI_RT_COUNT */

	ld	v1, LOCAL_HUB(PI_RT_COUNT)
	j	ra
	 xor	v0, v1

	END(randnum)
