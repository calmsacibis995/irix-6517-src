/*****************************************************************************
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 ****************************************************************************/

#include <asm.h>
#include <regdef.h>
#include <sys/fpu.h>
#include "nanothread.h"
#include "uswtch.h"

/* #define YIELDCOUNT */

#if (_MIPS_SIM == _ABIO32)
<<< BOMB >>> Code has only been implemented for ABI64,N32
#endif

#if (PADDED_RSA_SIZE != 640)
<<< BOMB >>> Software multiplies assume 640.
#endif

	.set noat
	.set noreorder
	.set notransform

	/* align to cacheline boundary to minimize number of cachelines
	 * required by this code (performance issue only)
	 */
 # uint_t
 # resume_any_nid(nid_t oldnid, void *kusp) in C until I get to optimize
 #

	.align 7
 # uint_t
 # resume_nid(nid_t oldnid, kusharena_t *kusp, nid_t newnid)
 #
 # Returns 0 if the resume is unsuccessful, 1 otherwise.
 #
 # You enter resume via a function call, so the registers that must be
 # stored to the rsa's eframe are:
 #
 # ABI64 = s0->s7, gp, sp, fp, ra, epc
 # ABI64 = fs0->fs7, fpc_csr
 #
 #
 # The context being resumed was interrupted so the register that must be
 # restored from the rsa's eframe are:
 #
 # ABI64 = AT, v0, v1, a0->a7, t0->t3, s0->s7, t8, t9, k0, k1, gp, sp, fp, ra
 #		mdlo, mdhi (not k0, k1)
 # ABI64 = fv0, fv1, ft0->ft13, fa0->fa7, fs0->fs7, fpc_csr

/* #define		AT */
/* #define		v0 */
/* #define		v1 */
#define oldnid		a0
#define kusp		a1
#define newnid		a2
#define contextp	a4 /* ta0 */
#define one		a5 /* ta1 */
#define mask		a6 /* ta2 */
#define	bitv		a7 /* ta3 */
#define	rbitp		t8

LEAF(resume_nid)

	.set noreorder
	li		t0, PRDA
#if NULL_NID == 0
	INT_S		zero, PRDA_NID(t0)
#else
	li		t1, NULL_NID
	INT_S		t1, PRDA_NID(t0)		# No longer resumeable
#endif
	.set reorder
	sll		t0, oldnid, 1			# = 2*oldnid
	PTR_ADD		t0, kusp			# = kusp + 2*oldnid
	lhu		t0, KUS_NIDTORSA(t0)		# = rsaid
	bltz		t0, resume_nid_dynamic
	sll		t1, t0, 9			# = 512*rsaid
	sll		t0, 7				# = 128*rsaid
	add		t0, t1				# = 640*rsaid
	PTR_ADD		contextp, t0, kusp		# = kusp + 640*rsaid

 # Save yieldor's integer registers

#ifndef NDEBUG
	REG_L		t0, KUS_RSA+RSA_R0(contextp)
	tne		t0, zero
#endif

	li		one, 1
	REG_S		contextp, KUS_RSA+RSA_V0(contextp)
	RSA_SAVE_SREGS(contextp, t0)
	REG_S		ra, KUS_RSA+RSA_RA(contextp)
	srl		rbitp, newnid, 6
	andi		bitv, newnid, 0x3f
	sw		t0, KUS_RSA+RSA_FPC_CSR(contextp)

	sll		rbitp, 3			# = 8*(newnid/64)
	PTR_ADD		rbitp, kusp			# = kusp +8*(newnid/64)
	dsll		bitv, one, bitv			# = 1 << newnid % 64
	not		mask, bitv			# = ~(1<<(newnid%64))


	.set noreorder
1:	lld		t0, KUS_RBITS(rbitp)
	and		t1, t0, mask
	scd		t1, KUS_RBITS(rbitp)
	beql		t1, 0, 1b
	nada
	.set reorder
	
	and		t0, bitv			# BDSLOT 

	/* LL/SC loop succeeded, now see if newnid's bit was set.
	 * a3 = original state of newnid's rbit from "lld"
	 */
	
	beq		t0, 0, resume_backout
	nada

 # Complete save, by making oldnid resumeable, and increment yield counter.

	REG_S		one, KUS_RSA+RSA_V0(contextp)

#ifdef YIELDCOUNT
	PTR_L		t0, KUS_YIELDP(kusp)
	sll		t1, oldnid, 2
	PTR_ADD		t0, t1
	INT_L		t1, 0(t0)
	INT_ADD		t1, 1				# yields[nid]++
	INT_S		t1, 0(t0)
#endif

	sll		t2, newnid, 1			# = 2*nid
	PTR_ADD		t2, kusp			# = kusp + 2*nid
	lhu		t0, KUS_NIDTORSA(t2)		# = rsaid

	tlt		t0, zero
	sll		t1, t0, 9			# = 512*rsaid
	sll		t0, 7				# = 128*rsaid
	add		t0, t1				# = 640*rsaid
	PTR_ADD		contextp, t0, kusp		# = kusp + 640*rsaid
	REG_L		sp, KUS_RSA+RSA_SP(contextp)		# $29
	teq		sp, zero

	srl		rbitp, oldnid, 6		# BDSLOT a4 = nid / 64
	andi		bitv, oldnid, 0x3f		# = oldnid % 64
	sll		rbitp, 3			# = 8*(oldnid/64)
	PTR_ADD		rbitp, kusp			# = kusp +8*(oldnid/64)
	dsll		bitv, one, bitv			# = 1 << oldnid % 64

	.set noreorder
1:	lld		t0, KUS_RBITS(rbitp)
	or		t1, bitv, t0
	scd		t1, KUS_RBITS(rbitp)
	beql		t1, zero, 1b
	nada
	.set reorder

XLEAF(_run_nid)

 # Determine newnid's RSA
	
#ifndef NDEBUG
	REG_L		t0, KUS_RSA+RSA_R0(contextp)
	tne		t0, zero
#endif

#if (_MIPS_SIM == _ABI64)
	ldc1		fv0, KUS_RSA+RSA_FV0(contextp)		# $f0
	ldc1		ft12, KUS_RSA+RSA_FT12(contextp)	# $f1
	ldc1		fv1, KUS_RSA+RSA_FV1(contextp)		# $f2
	ldc1		ft13, KUS_RSA+RSA_FT13(contextp)	# $f3
	ldc1		ft0, KUS_RSA+RSA_FT0(contextp)		# $f4
	ldc1		ft1, KUS_RSA+RSA_FT1(contextp)		# $f5
	ldc1		ft2, KUS_RSA+RSA_FT2(contextp)		# $f6
	ldc1		ft3, KUS_RSA+RSA_FT3(contextp)		# $f7
	ldc1		ft4, KUS_RSA+RSA_FT4(contextp)		# $f8
	ldc1		ft5, KUS_RSA+RSA_FT5(contextp)		# $f9
	ldc1		ft6, KUS_RSA+RSA_FT6(contextp)		# $f10
	ldc1		ft7, KUS_RSA+RSA_FT7(contextp)		# $f11
	ldc1		fa0, KUS_RSA+RSA_FA0(contextp)		# $f12
	ldc1		fa1, KUS_RSA+RSA_FA1(contextp)		# $f13
	ldc1		fa2, KUS_RSA+RSA_FA2(contextp)		# $f14
	ldc1		fa3, KUS_RSA+RSA_FA3(contextp)		# $f15
	ldc1		fa4, KUS_RSA+RSA_FA4(contextp)		# $f16
	ldc1		fa5, KUS_RSA+RSA_FA5(contextp)		# $f17
	ldc1		fa6, KUS_RSA+RSA_FA6(contextp)		# $f18
	ldc1		fa7, KUS_RSA+RSA_FA7(contextp)		# $f19
	ldc1		ft8, KUS_RSA+RSA_FT8(contextp)		# $f20
	ldc1		ft9, KUS_RSA+RSA_FT9(contextp)		# $f21
	ldc1		ft10, KUS_RSA+RSA_FT10(contextp)	# $f22
	ldc1		ft11, KUS_RSA+RSA_FT11(contextp)	# $f23
	ldc1		fs0, KUS_RSA+RSA_FS0(contextp)		# $f24
	ldc1		fs1, KUS_RSA+RSA_FS1(contextp)		# $f25
	ldc1		fs2, KUS_RSA+RSA_FS2(contextp)		# $f26
	ldc1		fs3, KUS_RSA+RSA_FS3(contextp)		# $f27
	ldc1		fs4, KUS_RSA+RSA_FS4(contextp)		# $f28
	ldc1		fs5, KUS_RSA+RSA_FS5(contextp)		# $f29
	ldc1		fs6, KUS_RSA+RSA_FS6(contextp)		# $f30
	ldc1		fs7, KUS_RSA+RSA_FS7(contextp)		# $f31
#endif
#if (_MIPS_SIM == _ABIN32)
	ldc1		fv0, KUS_RSA+RSA_FV0(contextp)		# $f0
	ldc1		ft14, KUS_RSA+RSA_FT14(contextp)	# $f1
	ldc1		fv1, KUS_RSA+RSA_FV1(contextp)		# $f2
	ldc1		ft15, KUS_RSA+RSA_FT15(contextp)	# $f3
	ldc1		ft0, KUS_RSA+RSA_FT0(contextp)		# $f4
	ldc1		ft1, KUS_RSA+RSA_FT1(contextp)		# $f5
	ldc1		ft2, KUS_RSA+RSA_FT2(contextp)		# $f6
	ldc1		ft3, KUS_RSA+RSA_FT3(contextp)		# $f7
	ldc1		ft4, KUS_RSA+RSA_FT4(contextp)		# $f8
	ldc1		ft5, KUS_RSA+RSA_FT5(contextp)		# $f9
	ldc1		ft6, KUS_RSA+RSA_FT6(contextp)		# $f10
	ldc1		ft7, KUS_RSA+RSA_FT7(contextp)		# $f11
	ldc1		fa0, KUS_RSA+RSA_FA0(contextp)		# $f12
	ldc1		fa1, KUS_RSA+RSA_FA1(contextp)		# $f13
	ldc1		fa2, KUS_RSA+RSA_FA2(contextp)		# $f14
	ldc1		fa3, KUS_RSA+RSA_FA3(contextp)		# $f15
	ldc1		fa4, KUS_RSA+RSA_FA4(contextp)		# $f16
	ldc1		fa5, KUS_RSA+RSA_FA5(contextp)		# $f17
	ldc1		fa6, KUS_RSA+RSA_FA6(contextp)		# $f18
	ldc1		fa7, KUS_RSA+RSA_FA7(contextp)		# $f19
	ldc1		fs0, KUS_RSA+RSA_FS0(contextp)		# $f20
	ldc1		ft8, KUS_RSA+RSA_FT8(contextp)		# $f21
	ldc1		fs1, KUS_RSA+RSA_FS1(contextp)		# $f22
	ldc1		ft9, KUS_RSA+RSA_FT9(contextp)		# $f23
	ldc1		fs2, KUS_RSA+RSA_FS2(contextp)		# $f24
	ldc1		ft10, KUS_RSA+RSA_FT10(contextp)	# $f25
	ldc1		fs3, KUS_RSA+RSA_FS3(contextp)		# $f26
	ldc1		ft11, KUS_RSA+RSA_FT11(contextp)	# $f27
	ldc1		fs4, KUS_RSA+RSA_FS4(contextp)		# $f28
	ldc1		ft12, KUS_RSA+RSA_FT12(contextp)	# $f29
	ldc1		fs5, KUS_RSA+RSA_FS5(contextp)		# $f30
	ldc1		ft13, KUS_RSA+RSA_FT13(contextp)	# $f31
#endif
	lw		t0, KUS_RSA+RSA_FPC_CSR(contextp)
	ctc1		t0, fpc_csr     # $fpc_csr

 #	ld		t1, KUS_PAD(kusp)
 #	tne		t1, jp
	
 # Restore yieldee's integer registers
	
	REG_L		AT, KUS_RSA+RSA_AT(contextp)		# $1
	REG_L		v0, KUS_RSA+RSA_V0(contextp)		# $2
	REG_L		v1, KUS_RSA+RSA_V1(contextp)		# $3
	REG_L		a0, KUS_RSA+RSA_A0(contextp)		# $4
	REG_L		a1, KUS_RSA+RSA_A1(contextp)		# $5
 # Postpone loading a2 (newnid)
	REG_L		a3, KUS_RSA+RSA_A3(contextp)		# $7
 # Postpone loading a4(ta0) (contextp)
	REG_L		a5, KUS_RSA+RSA_A5(contextp)		# $9
	REG_L		a6, KUS_RSA+RSA_A6(contextp)		# $10
	REG_L		a7, KUS_RSA+RSA_A7(contextp)		# $11
 # Postpone loading t0, need a temporary
 # Postpone loading t1, need a temporary
	REG_L		t2, KUS_RSA+RSA_T2(contextp)		# $14
	REG_L		t3, KUS_RSA+RSA_T3(contextp)		# $15
	REG_L		s0, KUS_RSA+RSA_S0(contextp)		# $16
	REG_L		s1, KUS_RSA+RSA_S1(contextp)		# $17
	REG_L		s2, KUS_RSA+RSA_S2(contextp)		# $18
	REG_L		s3, KUS_RSA+RSA_S3(contextp)		# $19
	REG_L		s4, KUS_RSA+RSA_S4(contextp)		# $20
	REG_L		s5, KUS_RSA+RSA_S5(contextp)		# $21
	REG_L		s6, KUS_RSA+RSA_S6(contextp)		# $22
	REG_L		s7, KUS_RSA+RSA_S7(contextp)		# $23
	REG_L		t8, KUS_RSA+RSA_T8(contextp)		# $24
 # Postpone loading of t9(jp) for nested resume check
	REG_L		gp, KUS_RSA+RSA_GP(contextp)		# $28
	REG_L		fp, KUS_RSA+RSA_FP(contextp)		# $30
	REG_L		ra, KUS_RSA+RSA_RA(contextp)		# $31
	REG_L		t0, KUS_RSA+RSA_MDLO(contextp)
	mtlo		t0					# $Mulitply Low
	REG_L		t0, KUS_RSA+RSA_MDHI(contextp)
	mthi		t0					# $Multiply Hi
	REG_L		t0, KUS_RSA+RSA_EPC(contextp)

 #	teq		zero, zero
1:	PTR_SUBU	t1, t0, jp
	PTR_ADDIU	t1, -neststrt
	.set noreorder
	bltz		t1, 1f
	PTR_ADDIU	t1, -0x10
	bltz		t1, 2f
	REG_L		t0, KUS_RSA+RSA_T0(contextp)
	.set reorder

	REG_L		t0, KUS_RSA+RSA_EPC(contextp)
1:	REG_L		t1, KUS_RSA+RSA_T1(contextp)
	REG_S		t1, -24(sp)
	REG_L		t1, KUS_RSA+RSA_A2(contextp)
	REG_S		t1, -16(sp)
	REG_L		t1, KUS_RSA+RSA_T0(contextp)
	REG_S		t1, -8(sp)

2:	REG_L		t9, KUS_RSA+RSA_T9(contextp)	# $25, jp
	REG_L		a4, KUS_RSA+RSA_A4(contextp)

	li		t1, PRDA
	.set noreorder
#ifndef NDEBUG
	teq		t0, zero
	teq		sp, zero
#endif
	INT_S		newnid, PRDA_NID(t1)
XLEAF(_resume_nid_nest)
	REG_L		t1, -24(sp)
	REG_L		a2, -16(sp)
	jr		t0			# User's PC
	REG_L		t0, -8(sp)		# $31
	.set reorder

resume_backout:

#ifdef YIELDCOUNT
	PTR_L		t0, KUS_YIELDP(kusp)
	sll		t1, oldnid, 2
	PTR_ADDIU	t0, FYIELD_OFFSET
	PTR_ADDU	t0, t1
	INT_L		t1, 0(t0)
	INT_ADD		t1, 1
	INT_S		t1, 0(t0)
#endif
	li		t1, PRDA
	li		v0, 0
	.set noreorder
	jr		ra
	INT_S		oldnid, PRDA_NID(t1)
	.set reorder
END(resume_nid)

LEAF(resume_nid_dynamic)
	teq		zero, zero
END(resume_nid_dynamic)

LEAF(run_nid)
	.set noreorder
	move		jp, a0
#ifndef NDEBUG
 #	dli		t0, 0x4000200000
 #	tne		a1, t0
#endif
	sll		t0, a2, 9
	sll		t1, a2, 7
	add		t0, t1
	b		_run_nid
	PTR_ADD		contextp, kusp, t0
	.set reorder
END(run_nid)

/*
 * int
 * save_rsa(  rsa_t *rsap )
 *
 * Saves current register set into the RSA entry indiced by the first argument.
 * Intended to be used mainly to setup upcall RSA, but written in a manner such that
 * it could be used in order to unload a NID (if you take care to manage the rbits
 * and nid_to_rsa table).
 *
 * Returns a ZERO when invoked, will return a "1" when RSA 0 is resumed since
 * this code stores a "1" into the V0 restore slot.
 */
LEAF(save_rsa)
	.set	noreorder
	move		a3, a0
	
	REG_S		s0, KUS_RSA+RSA_S0(a3)		# $16
	REG_S		s1, KUS_RSA+RSA_S1(a3)		# $17
	REG_S		s2, KUS_RSA+RSA_S2(a3)		# $18
	REG_S		s3, KUS_RSA+RSA_S3(a3)		# $19
	REG_S		s4, KUS_RSA+RSA_S4(a3)		# $20
	REG_S		s5, KUS_RSA+RSA_S5(a3)		# $21
	REG_S		s6, KUS_RSA+RSA_S6(a3)		# $22
	REG_S		s7, KUS_RSA+RSA_S7(a3)		# $23
	REG_S		t9, KUS_RSA+RSA_T9(a3)		# $25
	REG_S		gp, KUS_RSA+RSA_GP(a3)		# $28
	REG_S		sp, KUS_RSA+RSA_SP(a3)		# $29
	REG_S		fp, KUS_RSA+RSA_FP(a3)		# $30
	REG_S		ra, KUS_RSA+RSA_RA(a3)		# $31
	REG_S		ra, KUS_RSA+RSA_EPC(a3)		# User's PC
	li		a7, 1
	REG_S		a7, KUS_RSA+RSA_V0(a3)		# yield return value

	sdc1		fs0, KUS_RSA+RSA_FS0(a3)	# $f24
	sdc1		fs1, KUS_RSA+RSA_FS1(a3)	# $f25
	sdc1		fs2, KUS_RSA+RSA_FS2(a3)	# $f26
	sdc1		fs3, KUS_RSA+RSA_FS3(a3)	# $f27
	sdc1		fs4, KUS_RSA+RSA_FS4(a3)	# $f28
	sdc1		fs5, KUS_RSA+RSA_FS5(a3)	# $f29
#if (_MIPS_SIM == _ABI64)
	sdc1		fs6, KUS_RSA+RSA_FS6(a3)	# $f30
	sdc1		fs7, KUS_RSA+RSA_FS7(a3)	# $f31
#endif
	cfc1		a6, fpc_csr
	nada
	sw		a6, KUS_RSA+RSA_FPC_CSR(a3)	# fpc_csr
	jr		ra
	move		v0, zero
	.set	reorder
	END(save_rsa)

/* _restart_nid will:
 *	load register values from RSA
 *	set fbits using address & mask for RSA
 *	set correct nid value (resume)
 *	clear nid_to_rsaid[resume]
 */

#if 0

LEAF(_restart_nid)

 # Determine newnid's RSA

	.set	noreorder	
	sll		a3, a0, 1			# a3 = 2*nid
	PTR_ADD		a3, a1				# a3 = kusp + 2*nid
	lhu		a3, KUS_NIDTORSA(a3)		# a3 = rsaid

	sll		a4, a3, 9			# a4 = 512*rsaid
	sll		a3, a3, 7			# a3 = 128*rsaid
	add		a3, a4, a3			# a3 = 640*rsaid
	PTR_ADD		a3, a1, a3			# a3 = kusp + 640*rsaid


 # REGALLOC: a[0, 2, 4-7] are dead. (a0 = newnid, a3 = newrsa_p)
	
 # Restore yieldee's fp registers

	ldc1		fv0, KUS_RSA+RSA_FV0(a3)	# $f0
	ldc1		ft12, KUS_RSA+RSA_FT12(a3)	# $f1
	ldc1		fv1, KUS_RSA+RSA_FV1(a3)	# $f2
	ldc1		ft13, KUS_RSA+RSA_FT13(a3)	# $f3
	ldc1		ft0, KUS_RSA+RSA_FT0(a3)	# $f4
	ldc1		ft1, KUS_RSA+RSA_FT1(a3)	# $f5
	ldc1		ft2, KUS_RSA+RSA_FT2(a3)	# $f6
	ldc1		ft3, KUS_RSA+RSA_FT3(a3)	# $f7
	ldc1		ft4, KUS_RSA+RSA_FT4(a3)	# $f8
	ldc1		ft5, KUS_RSA+RSA_FT5(a3)	# $f9
	ldc1		ft6, KUS_RSA+RSA_FT6(a3)	# $f10
	ldc1		ft7, KUS_RSA+RSA_FT7(a3)	# $f11
	ldc1		fa0, KUS_RSA+RSA_FA0(a3)	# $f12
	ldc1		fa1, KUS_RSA+RSA_FA1(a3)	# $f13
	ldc1		fa2, KUS_RSA+RSA_FA2(a3)	# $f14
	ldc1		fa3, KUS_RSA+RSA_FA3(a3)	# $f15
	ldc1		fa4, KUS_RSA+RSA_FA4(a3)	# $f16
	ldc1		fa5, KUS_RSA+RSA_FA5(a3)	# $f17
	ldc1		fa6, KUS_RSA+RSA_FA6(a3)	# $f18
	ldc1		fa7, KUS_RSA+RSA_FA7(a3)	# $f19
	ldc1		ft8, KUS_RSA+RSA_FT8(a3)	# $f20
	ldc1		ft9, KUS_RSA+RSA_FT9(a3)	# $f21
	ldc1		ft10, KUS_RSA+RSA_FT10(a3)	# $f22
	ldc1		ft11, KUS_RSA+RSA_FT11(a3)	# $f23
	ldc1		fs0, KUS_RSA+RSA_FS0(a3)	# $f24
	ldc1		fs1, KUS_RSA+RSA_FS1(a3)	# $f25
	ldc1		fs2, KUS_RSA+RSA_FS2(a3)	# $f26
	ldc1		fs3, KUS_RSA+RSA_FS3(a3)	# $f27
	ldc1		fs4, KUS_RSA+RSA_FS4(a3)	# $f28
	ldc1		fs5, KUS_RSA+RSA_FS5(a3)	# $f29
	ldc1		fs6, KUS_RSA+RSA_FS6(a3)	# $f30
	ldc1		fs7, KUS_RSA+RSA_FS7(a3)	# $f31
	lw		v0, KUS_RSA+RSA_FPC_CSR(a3)
	ctc1		v0, fpc_csr	# $fpc_csr
	
 # Restore yieldee's integer registers
	
	REG_L		AT, KUS_RSA+RSA_AT(a3)		# $1
	REG_L		v0, KUS_RSA+RSA_V0(a3)		# $2
	REG_L		v1, KUS_RSA+RSA_V1(a3)		# $3
 # Postpone loading a0 (newnid)
 # Postpone loading a1 (kusp)
 # Postpone loading a2 (newrsa)
 # Postpone loading a3 (newrsa_p)
 # Postpone loading a4, temporary for loading pc
	REG_L		a5, KUS_RSA+RSA_A5(a3)		# $9
	REG_L		a6, KUS_RSA+RSA_A6(a3)		# $10
	REG_L		a7, KUS_RSA+RSA_A7(a3)		# $11
	REG_L		t0, KUS_RSA+RSA_T0(a3)		# $12
	REG_L		t1, KUS_RSA+RSA_T1(a3)		# $13
	REG_L		t2, KUS_RSA+RSA_T2(a3)		# $14
	REG_L		t3, KUS_RSA+RSA_T3(a3)		# $15
	REG_L		s0, KUS_RSA+RSA_S0(a3)		# $16
	REG_L		s1, KUS_RSA+RSA_S1(a3)		# $17
	REG_L		s2, KUS_RSA+RSA_S2(a3)		# $18
	REG_L		s3, KUS_RSA+RSA_S3(a3)		# $19
	REG_L		s4, KUS_RSA+RSA_S4(a3)		# $20
	REG_L		s5, KUS_RSA+RSA_S5(a3)		# $21
	REG_L		s6, KUS_RSA+RSA_S6(a3)		# $22
	REG_L		s7, KUS_RSA+RSA_S7(a3)		# $23
	REG_L		t8, KUS_RSA+RSA_T8(a3)		# $24
	REG_L		t9, KUS_RSA+RSA_T9(a3)		# $25
 # delay loading of gp, so we can check for nested resume
	REG_L		sp, KUS_RSA+RSA_SP(a3)		# $29
	REG_L		fp, KUS_RSA+RSA_FP(a3)		# $30
	REG_L		ra, KUS_RSA+RSA_RA(a3)		# $31
	REG_L		a4, KUS_RSA+RSA_MDLO(a3)
	mtlo		a4			# $Mulitply Low
	REG_L		a4, KUS_RSA+RSA_MDHI(a3)
	mthi		a4			# $Multiply Hi

 # REGALLOC: a0, a1, a2, a3 are live
 # a0, a1, a2, a3, a4, epc have not yet been loaded
 # gp has not yet been loaded since we have global address references 

	/* check to see if we're resuming a nanothread which was already
	 * executing in the region from _resume_restart_nid to
	 * _end_resume_restart_nid.  If so, then we skip moving certain
	 * registers from the RSA to the stack since the only valid
	 * copy of those registers is already on the stack.
	 */

	SETUP_GPX64(gp, a2);
	.set noreorder
	REG_L		a4, KUS_RSA+RSA_EPC(a3)
	LA		a2, _resume_restart_nid
	PTR_SUB		a2, a4, a2
	bltz		a2, move_rsa_to_stack	# EPC below code, so OK to move
	LA		a2, _end_resume_restart_nid
	PTR_SUB		a2, a4, a2
	bltz		a2, dont_move_rsa_to_stack
	nada
	
move_rsa_to_stack:

	/* need to move final registers from RSA to below current SP */
	
	REG_L		a4, KUS_RSA+RSA_A2(a3)
	REG_S		a4, -8(sp)
	REG_L		a4, KUS_RSA+RSA_A4(a3)
	REG_S		a4, -16(sp)
	REG_L		a4, KUS_RSA+RSA_EPC(a3)
	REG_S		a4, -24(sp)
	REG_L		a4, KUS_RSA+RSA_A0(a3)
	REG_S		a4, -32(sp)
	REG_L		a4, KUS_RSA+RSA_A1(a3)
	REG_S		a4, -40(sp)

dont_move_rsa_to_stack:			
		
	REG_L		gp, KUS_RSA+RSA_GP(a3)		# $28
	REG_L		a3, KUS_RSA+RSA_A3(a3)

	/* set fbits */

	sll		a2, a0, 1			# a2 = 2*nid
	PTR_ADD		a2, a1				# a2 = kusp + 2*nid
	lhu		a4, KUS_NIDTORSA(a2)		# a4 = rsaid
	sh		zero, KUS_NIDTORSA(a2)

	dsrl		a2, a4, 6			# a2 = rsa / 64
	andi		a4, 0x3f			# a4 = rsa % 64
	dsll		a2, 3				# a2 = 8*(rsa/64)
	dadd		a2, a1				# a2 = kusp + 8*rsa/64
	li		a1, 1
	dsll		a1, a1, a4			# a1 = 1 << rsa % 64
	/*
	 * a0 == nid
	 * a1 == 1 << (rsa %64)
	 * a2 == & kusp->rbits[rsa/64]
	 * a4 == scratch
	 */
set_fbit:	
	lld		a4, KUS_FBITS(a2)
	or		a4, a1
	scd		a4, KUS_FBITS(a2)
	beql		a4, zero, set_fbit
	nada
	
	/* restore other registers
	 *
	 * NOTE that as soon as the PRDA_NID is restored we need to worry
	 * about this process being pre-empted since that causes the kernel to
	 * dump the registers into the RSA but correct values for
	 * a1, a2, a4, EPC are ONLY below (sp).  So we special case handling
	 * a _restart_nid if we're restarting a nanothread which was preempted
	 * while executing _resume_restart_nid to _end_resume_restart_nid.
	 * In this special case we don't re-stack these registers.
	 */

	li		a4, PRDA
	sw		a0, PRDA_NID(a4)
	
XLEAF(_resume_restart_nid)
	
	REG_L		a1, -40(sp)
	REG_L		a2, -8(sp)

	REG_L		a0, -32(sp)
	REG_L		a4, -24(sp)
	jr		a4			# User's PC
	REG_L		a4, -16(sp)		# $31
	.set	reorder
XLEAF(_end_resume_restart_nid)
END(_restart_nid)

#endif

LEAF(_restart_nid)
	teq		zero, zero
END(_restart_nid)

LEAF(test_clean_llbit)
	PTR_ADDI	sp, -16
	li		v0, 100
1:
	lld		a2, 8(sp)
	ld		a2, 8(sp)
	scd		a2, 8(sp)
	add		v0, -1
	beq		v0, zero, 1f
	beq		a2, zero, 1b
1:
	PTR_ADDI	sp, 16
END(test_clean_llbit)
