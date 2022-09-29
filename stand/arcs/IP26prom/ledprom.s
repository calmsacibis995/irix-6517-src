/* Minimum neccessary required code is used to JUST FLASH LED.
 */

#ident	"$Revision: 1.6 $"

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <regdef.h>
#include <asm.h>

#if PiE_EMULATOR
#define WAITCOUNT	10000
#else
#define WAITCOUNT	200000
#endif

LEAF(start)
	.set noreorder

#ifdef R4000			/* force 64bit mode and intrs off */
	li	a0, SR_KX
	MTC0	(a0, C0_SR)
#endif

#ifdef TFP			/* make sure interrupts are cleared and off */
	li	v0, SR_CU1|SR_FR
	DMTC0	(v0, C0_SR)
	DMTC0	(zero, C0_CAUSE)
#endif

#if PiE_EMULATOR
	LI	t0, PHYS_TO_K1(CPUCTRL0)
	lw	t2, 0(t0)
	li	t3,~CPUCTRL0_RFE
	and	t2,t2,t3
	sw	t2, 0(t0)
#endif

	LI	t0, PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(6))
	li	t3, 0xd46b
	nop
	sw	t3, 0(t0)

	LI	t0, PHYS_TO_K1(HPC3_WRITE1)	# channel 6, offset 0x1c

repeat:
	li	t3, 0
	nop
	sw	t3, 0(t0)

	li	t2, WAITCOUNT			# delay
2:	bnez	t2, 2b
	subu	t2,1

	li	t3, LED_GREEN
	nop
	sw	t3, 0(t0)

	li	t2, WAITCOUNT			# delay
3:	bnez	t2, 3b
	subu	t2, 1

	li	t3, LED_AMBER
	nop
	sw	t3, 0(t0)

	li	t2, WAITCOUNT    		# delay
4:	bnez	t2, 4b
	subu	t2, 1

b	repeat
	nop

	j	ra
	nop

	.set	reorder
	END(start)
