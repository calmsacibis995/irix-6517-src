/***********************************************************************\
*	File:		mtest_asm.s					*
*									*
*	This file contains memory routines in handcoded			*
*	assembly so that they run as fast as possible.			*
*									*
*	Would it be possible to use the BTE to further speed them up?	*
*									*
\***********************************************************************/

#ident	"$Revision: 1.3 $"

#include <asm.h>

#include <sys/regdef.h>
#include <sys/sbd.h>

#include <sys/SN/agent.h>

#include "mtest.h"

	.text
	.set	at
	.set	reorder

#define T_S4D(r, A, B, C, D)			\
	.set	noreorder;			\
	sd	A, 0(r);			\
	sd	B, 8(r);			\
	sd	C, 16(r);			\
	sd	D, 24(r);			\
	.set	reorder

#define T_L4D(r, A, B, C, D)			\
	.set	noreorder;			\
	ld	A, 0(r);			\
	ld	B, 8(r);			\
	ld	C, 16(r);			\
	ld	D, 24(r);			\
	.set	reorder

/*
 * mtest_compare_prom
 *
 *   Compares PROM data with data elsewhere in memory
 *
 *   a0  Source address in PROM (word-aligned)
 *   a1  Source address in RAM (word-aligned)
 *   a2  Length in bytes (will be rounded up to a multiple of 64)
 *   a3  Pointer to mtest_fail_t structure to receive failure info
 *
 *   Returns 0 for success, -1 for failure.
 */

#define MCP_CHECK(exp, act, off) \
	beq	exp, act, 1f;		\
	move	t0, exp;		\
	move	ta0, act;		\
	li	v0, off;		\
	b	9f;			\
1:

LEAF(mtest_compare_prom)
	beqz	a2, 10f			# Done if NULL compare

	daddu	a2, 63			# Round up to a multiple of 64
	and	a2, ~63

1:
	T_L4D(a0, t0, t1, t2, t3)
	T_L4D(a1, ta0, ta1, ta2, ta3)

	MCP_CHECK(t0, ta0,  0)
	MCP_CHECK(t1, ta1,  8)
	MCP_CHECK(t2, ta2, 16)
	MCP_CHECK(t3, ta3, 24)

	daddu	a0, 32
	daddu	a1, 32

	dsubu	a2, 32
	bnez	a2, 1b

10:
	li	v0, 0			# Return success
	j	ra

9:	daddu	a1, v0			# Failed address adjustment

	ld	v0, 0(a1)
	sd	v0,  MTEST_FAIL_OFF_REREAD(a3)
	sd	a1,  MTEST_FAIL_OFF_ADDR(a3)
	li	v0, -1
	sd	v0,  MTEST_FAIL_OFF_MASK(a3)
	sd	t0,  MTEST_FAIL_OFF_EXPECT(a3)
	sd	ta0, MTEST_FAIL_OFF_ACTUAL(a3)
	ld	v0, LOCAL_HUB(MD_MEM_ERROR_CLR)
	sd	v0,  MTEST_FAIL_OFF_MEM_ERR(a3)
	ld	v0, LOCAL_HUB(MD_DIR_ERROR_CLR)
	sd	v0,  MTEST_FAIL_OFF_DIR_ERR(a3)
	li	v0, -1			# Return failure
	j	ra
	END(mtest_compare_prom)
