/* pon_io.s - register-based power-on diagnostic output primitives */

#include "asm.h"
#include "sys/sbd.h"
#include "sys/PCI/PCI_defs.h"
#include "sys/cpu.h"
#include "sys/ns16550.h"
#include "early_regdef.h"

#define	STORE(reg,value)	li T01,value; sb T01,reg(T00)
LEAF(pon_initio)

	LI	T00,MIPS_SER0_BASE

	STORE(LINE_CNTRL_REG, LCR_DLAB)
	STORE(DIVISOR_LATCH_MSB_REG, SER_DIVISOR(9600)>>8)
	STORE(DIVISOR_LATCH_LSB_REG, SER_DIVISOR(9600)&0xff)
	STORE(LINE_CNTRL_REG, LCR_8_BITS_CHAR|LCR_1_STOP_BITS)
	STORE(INTR_ENABLE_REG, 0x0)
	STORE(FIFO_CNTRL_REG, FCR_ENABLE_FIFO)
	STORE(FIFO_CNTRL_REG, FCR_ENABLE_FIFO|FCR_RCV_FIFO_RESET|FCR_RCV_FIFO_RESET)

	j	ra
	END(pon_initio)

LEAF(pon_puthex)
	li	T10,28		# Amount to shift to get the 1st digit
	b	print_hex

XLEAF(pon_puthex64)
	li	T10,60		# Amount to shift to get the 1st digit

print_hex:
	move	T11,a0
	move	RA1,ra

	/* print leading '0x' */
	li	a0,0x30		# Hex code for '0'
	jal	pon_putc
	li	a0,0x78		# Hex code for 'x'
	jal	pon_putc
1:
	dsrlv	a0,T11,T10	# Isolate digit 
	and	a0,0xf
	lb	a0,hexdigit(a0) # Convert it to hexidecimal
	jal	pon_putc	# Print it
	sub	T10,4		# Set up next nibble
	bge	T10,zero,1b

	j	RA1		# Yes - done
	END(pon_puthex)

	.data
hexdigit:
	.ascii  "0123456789abcdef"

	.text
LEAF(pon_puts)
	move	RA1,ra
	move	T10,a0
	b	2f		# jump to test for end-of-string
1:
	daddu	T10,1		# Bump to next character
	jal	pon_putc	# No - Display character
2:
	lb	a0,0(T10)	# Get character to display
	bne	a0,zero,1b	# End of string?

	j	RA1
	END(pon_puts)

/*
 * putc subroutine
 */
#define	DELAY_COUNT		0x8000

LEAF(pon_putc)
	LI	T00,MIPS_SER0_BASE
 	li	T01,DELAY_COUNT
1:
	lb	T02,LINE_STATUS_REG(T00)

	and	T02,LSR_XMIT_BUF_EMPTY	# transmitter ready ?
	bne	T02,zero,9f		# if not empty, delay
 	sub	T01,1			# decrement the count
 	beq	T01,zero,2f		# spin till it be 0
	b	1b			# just spin till ready
2:
 	j	ra			# no output, just return
9:
	sb	a0,XMIT_BUF_REG(T00)

 	j	ra			# and return
	END(pon_putc)

LEAF(pon_memerr)
	/*  a0 - bad address
	 *  a1 - data expected from read
	 *  a2 - actual data obtained
	 */
	move	RA2,ra
	move	T20,a0		# save bad address
	move	T21,a1		# save expected value
	move	T22,a2		# save actual value

	li	a0,LED_AMBER
	jal	pon_led

	dla	a0,failed
	jal	pon_puts

	dla	a0,address
	jal	pon_puts
	move	a0,T20		# address where error occured
	jal	pon_puthex64

	dla	a0,expected
	jal	pon_puts
	move	a0,T21		# data expected
	jal	pon_puthex64

	dla	a0,received
	jal	pon_puts
	move	a0,T22		# actual value read
	jal	pon_puthex64

	dla	a0,crlf		# new line
	jal	pon_puts

	j	RA2
	END(pon_memerr)


#define	WAITCOUNT	200000
LEAF(pon_flash_led)
1:	b	1b
	END(pon_flash_led)

/* modify LED state according to a0 */
LEAF(pon_led)

	j	ra
	END(pon_led)

	.data
address:	.asciiz	"Address: "
expected:	.asciiz	", Expected: "
received:	.asciiz	", Received: "
failed:		.asciiz "       *FAILED*\r\n"

EXPORT(crlf)
		.asciiz	"\r\n"
