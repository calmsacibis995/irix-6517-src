/*
 * pon_io.s - register-based power-on diagnostic output primitives
 *
 */
#include "asm.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/z8530.h"
#include "early_regdef.h"

#if	defined(SIMULATE) || defined(PiE_EMULATOR)
#define DELAY
#else
#define	DELAY	li v0,100; 99: sub v0,1; bnez v0,99b
#endif

#define	STORE(offset,value)	DELAY; li T01,offset; sb T01,0(T00); DELAY; li T01,value; sb T01,0(T00)

LEAF(pon_initio)
	move	RA3,ra

	li	T00,PHYS_TO_K1(DUART1B_CNTRL)

	STORE(WR9, WR9_HW_RST)		# hardware reset
	STORE(WR4, WR4_X16_CLK|WR4_1_STOP)	# 16x clock, 1 stop bit
	STORE(WR3, WR3_RX_8BIT)		# 8 bit Rx character
	STORE(WR5, WR5_TX_8BIT)		# 8 bit Tx character
	STORE(WR11, WR11_RCLK_BRG|WR11_TCLK_BRG)	# clock source from BRG
	STORE(WR12, 0x0a)		# BRG time constant, lower byte, 9600
	STORE(WR13, 0x00)		# BRG time constant, upper byte, 9600
	STORE(WR14, WR14_BRG_ENBL)	# enable BRG
	STORE(WR3, WR3_RX_8BIT|WR3_RX_ENBL)	# enable receiver
	STORE(WR5, WR5_TX_8BIT|WR5_TX_ENBL)	# enable transmitter

	/* do a hardware reset on all the other DUARTs */
	li	T00,PHYS_TO_K1(DUART0A_CNTRL)
	STORE(WR9, WR9_HW_RST)
	STORE(WR5, WR5_RTS)		# leave HP1 led set to yellow

	li	T00,PHYS_TO_K1(DUART2A_CNTRL)
	STORE(WR9, WR9_HW_RST)
	STORE(WR5, 0x00)		# make sure RTS is not set because it
					# may confuse APPLETALK network

	j	RA3
	END(pon_initio)

LEAF(pon_puthex)
	move	RA1,ra
	li	T10,8		# Number of digits to display
1:
	srl	v1,a0,28	# Isolate digit 
	lb	v1,hexdigit(v1) # Convert it to hexidecimal
	jal	pon_putc	# Print it
	sll	a0,4		# Set up next nibble
	sub	T10,1		# Entire number printed?
	bne	T10,zero,1b
	j	RA1		# Yes - done
	END(pon_puthex)

hexdigit:
	.ascii  "0123456789ABCDEF"

LEAF(pon_puts)
	move	RA1,ra
	b	2f		# jump to test for end-of-string
1:
	addu	a0,1		# Bump to next character
	jal	pon_putc	# No - Display character
2:
	lb	v1,0(a0)	# Get character to display
	bne	v1,zero,1b	# End of string?
	j	RA1
	END(pon_puts)

/*
 * putc subroutine - v1 contains byte to be printed
 */
#if	defined(SIMULATE) || defined(PiE_EMULATOR)
#define DELAY_COUNT		1
#else
#define	DELAY_COUNT		0x8000
#endif

LEAF(pon_putc)
	li	T00,PHYS_TO_K1(DUART1B_CNTRL)
#ifndef	SIMULATE
 	li	T01,DELAY_COUNT
1:
	DELAY
	li	T02,RR0			# read status register
	sb	T02,0(T00)
	DELAY
	lb	T02,0(T00)

	and	T02,RR0_TX_EMPTY	# transmitter ready ?
	bne	T02,zero,9f		# if not empty, delay
 	sub	T01,1			# decrement the count
 	beq	T01,zero,2f		# spin till it be 0
	b	1b			# just spin till ready
2:
 	j	ra			# and just return
9:
	DELAY
#endif	/* SIMULATE */
	li	T02,WR8
	sb	T02,0(T00)
	DELAY
	sb	v1,0(T00)
	.set	noreorder
	nop
	nop
	nop
	.set	reorder
 	j	ra			# and return
	END(pon_putc)

LEAF(pon_memerr)
	/*  a0 - bad address
	 *  a1 - data expected from read
	 *  a2 - actual data obtained
	 */
	move	RA2,ra
	move	T20,a0		# save bad address
	la	a0,failed
	jal	pon_puts

	move	T21,a1		# save expected value
	move	T22,a2		# save actual value
	la	a0,address
	jal	pon_puts
	move	a0,T20		# address where error occured
	jal	pon_puthex
	la	a0,expected
	jal	pon_puts
	move	a0,T21		# data expected
	jal	pon_puthex
	la	a0,received
	jal	pon_puts
	move	a0,T22		# actual value read
	jal	pon_puthex
	la	a0,crlf		# new line
	jal	pon_puts
	j	RA2
	END(pon_memerr)

address:	.asciiz	"Address: "
expected:	.asciiz	", Expected: "
received:	.asciiz	", Received: "
EXPORT(addr_msg)
		.asciiz	"Memory address test\t\t    "
EXPORT(walk_msg)
		.asciiz	"Memory walking bit test\t\t    "
EXPORT(crlf) 	
		.asciiz	"\r\n"
EXPORT(failed)
		.asciiz "       *FAILED*\r\n"
EXPORT(passed)
		.asciiz "        PASSED\r\n"
