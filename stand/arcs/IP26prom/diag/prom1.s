/* Minimum neccessary required code is used to just turn the LED green,
 * only fetching instructions from even side of the SysAD/T-bus.
 *
 * This is step1 of the diag PROM series.
 */

#ident	"$Revision: 1.3 $"

#include <sys/sbd.h>
#include <regdef.h>
#include <asm.h>
#include "blink.h"

#define DATAADDR_SEG	0x1fc0

/* change DATAADDR_OFF and the .align at the end at the same time */
#define DATAADDR_OFF	0x2000


LEAF(start)
	.set noreorder
	.set noat

.align 2
#ifdef ODD_INST
	nop
#endif

	li	v0,SR_CU1|SR_FR
	nop					# zero
	dmtc0	v0, C0_SR
	nop					# zero
	ssnop
	nop					# zero
	ssnop
	nop					# zero
	ssnop
	nop					# zero
	dmtc0	zero, C0_CAUSE
	nop					# zero
	ssnop
	nop					# zero
	ssnop
	nop					# zero
	ssnop
	nop					# zero

	/* Write 0xd46b to HPC3_PBUS_CFG_ADDR(6)) */
	lui	t0, 0x9000			# K1BASE
	nop					# zero
	dsll	t0, 16
	nop					# zero
	dsll	t0, 16
	nop					# zero
	lui	t1, 0x1fbd
	nop					# zero
	ori	t1, 0xd600
	nop					# zero
	or	t1, t0, t1			# final address
	nop					# zero
	li	t2, 0xd46b			# data
	nop					# zero
	sw	t2, 0(t1)
	nop					# zero

	/* blink green led three times, hold last one */
	move	s0, zero
	nop
	ori	s1, zero, WAITCOUNT_FAST
	nop
	jal	blink2
	nop

	/* now lets see if there is any data on the other side (to see if
         * there are stuck bits on the non execution side */
	lui	t8, 0x9000			# read in address
	nop
	dsll	t8, 16
	nop
	dsll	t8, 16
	nop
	lui	t9, DATAADDR_SEG
	nop
	ori	t9, t9, DATAADDR_OFF
	nop
	or	t8, t9, t8
	nop
	ld	t2, 0(t8)
	nop
	
	/* t2 should now contain all 5's */
	ori	a0, zero, 1
	nop
	jal	check
	nop

	/* blink green light three times to signify next dw check */
	move	s0, zero
	nop
	ori	s1, zero, WAITCOUNT_FAST
	nop
	jal	blink2
	nop

	/* load next address in data seg */
	ld	t2, 8(t8)
	nop

	/* t2 should now contain all a's */
	move	a0, zero
	nop
	jal	check
	nop

end:
	j	end
	nop

	.set	reorder
	.set	at
	END(start)


/* include code for check and blink */
#include "blink.s"


.align 13
.rdata
.dword	0x5555555555555555
.dword	0xaaaaaaaaaaaaaaaa

