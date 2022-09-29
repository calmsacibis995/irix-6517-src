#include <regdef.h>
#include <asm.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/i8251uart.h>

#define		CMD	0x0
#define 	DATA	0x8

LEAF(ENTRY)
	# Initialize the CC UART

	.set noreorder
	li	a1, EV_UART_BASE
	sd	zero, CMD(a1)		# Clear state by writing 3 zero's
	nop
	sd	zero, CMD(a1)
	nop
	sd	zero, CMD(a1)
	li	a0, I8251_RESET		# Soft reset
	sd	a0, CMD(a1)
	li	a0, I8251_ASYNC16X | I8251_NOPAR | I8251_8BITS | I8251_STOPB1
	sd	a0, CMD(a1)
	li	a0, I8251_TXENB | I8251_RXENB | I8251_RESETERR
	sd	a0, CMD(a1)

	# Enable CP1 for EPCUART stuff.

        mfc0    k0, C0_SR               # Make sure FR and CU1 bits are on.
        li      k1, SR_CU1|SR_FR
        or      k0, k1
        mtc0    k0, C0_SR

	.set reorder

	li	sp, 0x80020000

	move	s0, ra

	jal	run_vector		# Run a vector of Niblet tests

	move	ra, s0

	j	ra

	END(ENTRY)

