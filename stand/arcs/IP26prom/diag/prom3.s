/* Fetch As and 5s from PROM to make sure all bits from SysAD -> Tbus
 * work (even though they probably have to to allow the code to
 * run).  Light LED to green if it looks good or amber on a failure.
 *
 * Keep looping after test is done
 *
 * This is step3 of the diag PROM series.
 */

#ident	"$Revision: 1.1 $"

#define IP26	1

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <regdef.h>
#include <asm.h>

LEAF(start)
	.set noreorder
	li	v0,SR_CU1|SR_FR
	dmtc0	v0, C0_SR
	ssnop
	ssnop
	ssnop
	dmtc0	zero, C0_CAUSE
	ssnop
	ssnop
	ssnop

	/* Write 0xd46b to PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(6)) */
	LI	t0, PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(6))
	li	t1, 0xd46b			# data
	sw	t1, 0(t0)

	LA	t3,stuff
	ld	a1,0(t3)
	ld	a2,8(t3)
	daddu	a1,a2				# all f's
	daddi	a1,1				# -1 + 1 == 0

	LI	t0,PHYS_TO_K1(HPC3_WRITE1)
	li	t1, 0x20			# amber LED on
	li	t2, 0x10			# green LED on
	movn	t2,t1,a1			# pick color
	sw	t2, 0(t0)			# led on

	/* Spin for-ever */
1:	ld	zero,0(t3)			# 5s
	ld	zero,8(t3)			# As
	b	1b				# spin
	nop
	.set	reorder
	END(start)

	.data
stuff:
.dword	0x5555555555555555
.dword	0xaaaaaaaaaaaaaaaa
