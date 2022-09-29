/***********************************************************************\
*	File:		ccio.s						*
*									*
*	This file contains the IP21 PROM Utility Library routines for	*
*	manipulating the CC chip's LEDs and UART.			*
*									*
\***********************************************************************/

#ident "$Revision: 1.19 $"

#include "ml.h"
#include <sys/regdef.h>
#include <asm.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/i8251uart.h>
#include "ip21prom.h"
#include "pod_failure.h"

	.text
	.set 	noreorder
	.set	noat

/*
 * Routine set_cc_leds
 * Arguments:
 *	a0 -- Value to display on CC leds (0 - 255)
 * Returns:
 *	Nothing.
 * Uses:
 *	a0, v0.
 */

LEAF(set_cc_leds)
	dli	v0, EV_LED_BASE
	j	ra	
	sd	a0, 0(v0)
	END(set_cc_leds)


/*
 * Routine flash_cc_leds
 *	Causes a specific value to be flashed on the CC leds.
 *	Note that this routine NEVER RETURNS.
 *
 * Arguments:
 *	a0 -- value to be flashed.
 * Never returns.
 */

#ifdef	SABLE
#define	DELAY_COUNTER	1
#else
#define	DELAY_COUNTER	15000000
#endif

LEAF(flash_cc_leds)
	jal	ccuart_flush		# Get crap out of the CC UART port
	nop				# (BD)

	move	s0, a0
1:					# LOOP FOREVER 
	jal	set_cc_leds		#   Turn off the LEDS
	move	a0, zero		#   (BD)
	li	a0,DELAY_COUNTER	#    
2: 	subu	a0, 1			#   Decrement loop count	
	nop				# Don't want the branch in the
	nop				# same quad as the target for
	nop				# predictable branch cache behavior
	bne	a0,zero,2b		#   Delay 
	nop

	jal	set_cc_leds		#   Turn on value in LEDS
	move	a0, s0			#   (BD)	
	li	a0, DELAY_COUNTER
3:	subu	a0, 1			#
	nop				# Don't want the branch in the
	nop				# same quad as the target for
	nop				# predictable branch cache behavior
	bne	a0, zero, 3b		#   Delay
	nop

        jal     ccuart_poll        	# See if a character's waiting
        nop                             # (BD)

        beqz    v0, 1b                  # Was one?
        nop                             # (BD)

        jal     ccuart_getc             # Get it
        nop                             # (BD)

        li      v1, 16                  # ^P
        bne     v1, v0, 1b		# No, don't go into pod mode
        nop
	
	dla	a0, debug_mode_prompt	
	jal	pod_handler
	li	a1, EVDIAG_DEBUG	# (BD)
	END(flash_cc_leds)


/*
 * Routine ccuart_init
 *	Initializes the CC chip UART by writing three NULLS to
 *	get it into a known state, doing a soft reset, and then
 *	bringing it into ASYNCHRONOUS mode.  
 * 
 * Arguments:
 *	None.
 * Returns:
 * 	Nothing.
 * Uses:
 *	a0, a1.
 */
#define		CMD	0x0
#define 	DATA	0x8

LEAF(ccuart_init) 
	dli	a1, EV_UART_BASE
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
	j	ra 
	sd	a0, CMD(a1)		# (BD)
	END(ccuart_init)


/*
 * Routine ccuart_poll 
 *	Poll the CC UART to see if a charactre is available
 *
 * Arguments:
 * 	None.
 * Returns:
 *	v0 -- Nonzero if a character is available, zero otherwise
 * Uses:
 *	v0.
 */

LEAF(ccuart_poll) 
	dli	v0, EV_UART_BASE
	ld	v0, CMD(v0) 		#   Get status bit
	nop
	j	ra
	andi	v0, I8251_RXRDY
	END(ccuart_poll)


/*
 * Routine ccuart_getc
 *	Reads a single character from the CC UART.
 *
 * Arguments:
 * 	None.
 * Returns:
 *	v0 -- The character read.  
 * Uses:
 *	a0, a1, a2, v0, v1.
 */

LEAF(ccuart_getc)
	dli	v1, EV_UART_BASE
	li	a1, 1
	move	a2, ra
1:
	li	a0, 140000
2:					# 
	ld	v0, CMD(v1) 		# DO 
	nop
	andi	v0, I8251_RXRDY			
	bne	v0, zero, 20f		# WHILE (not ready && 
	nop				# (BD)
	bnez	a0, 2b			# 	 i-- > 0)
	sub	a0, 1			# (BD)
	
	jal	set_cc_leds
	xor	a0, a1, 3		# (BD) 
	j	1b			# Jump back to the get char loop
	xor	a1, 3				# (BD)
20:
	j	a2			# Return 
	ld	v0, DATA(v1)		# (BD) Read character from register
END(ccuart_getc)


/*
 * Routine ccuart_putc
 * 	Writes a single character to the CC UART.
 *
 * Arguments:
 *	a0 -- ASCII value of character to be written
 * Returns:
 *	v0 -- If non-zero, putc succeeded.
 * Uses:
 *	a0, a1, v0, v1	
 */

LEAF(ccuart_putc)
	dli	a1, EV_UART_BASE
	li	v0, 1000000
1:					# DO
	ld	v1, CMD(a1)		#   Get status register
	nop
	andi	v1, I8251_TXRDY		# UNTIL ready to transmit 
	bne	v1, zero, 2f	 	# OR
	subu	v0, 1			# (BD) (--cnt == 0)
	bne	v0, zero, 1b
	nop
	j	ra			# paranoid return without storing
	nop
2:
	sd	a0, DATA(a1)		# (BD) Write char into xmit buf
	j	ra
	nop
	END(ccuart_putc)


/*
 * Routine ccuart_flush
 *	Flushes the input buffer of the UART.
 *
 * Arguments:
 *	None.
 * Returns:
 *	Nothing.
 * Uses:
 *	v0, v1.
 */

LEAF(ccuart_flush)
	dli	v0, EV_UART_BASE	
	ld	v1, CMD(v0)		# Read status bits
	and	v1, I8251_RXRDY		# IF character available
	beq	v1, zero, 1f		# THEN
	nop
	ld	zero, DATA(v0)		#   read character
	nop
1:					# ENDIF
	j	ra
	nop 
	END(ccuart_flush)

	
/* 
 * Routine ccuart_puthex
 *	Writes a 64-bit hexadecimal value to the CC UART.
 *
 * Arguments:
 *	a0 -- the 64-bit hex number to write.
 * Returns:
 *	v0 -- Success.  Non-zero indicates write successful.
 * Uses:
 *	a0, a1, v0, v1, ta1, ta2, ta3, t8, t9	
 */

LEAF(ccuart_puthex)
	move    t9, ra
	jal	getendian
	li      t8, 16		# Number of digits to display (BD)
	move	ta2, v0		# Copy endianness to ta2
	move	ta1, a0		# Copy argument to ta1
2:
	dsrl	a0,ta1,30	# Rotate right 60 bits ($@!&*# assembler
	dsrl	a0,a0,30 	# 	won't assemble srl32 a0,ta1,28)
				# This isolates the rightmost nibble
	dla	ta3,hexdigit
	beq     ta2,zero,3f	# 0 = big endian
	daddu	a0,ta3		# (BD)
	xor     a0,3		# assumes hexdigit burned in prom in eb order
3:
	jal	get_char 
	nop			# (BD)
	jal     ccuart_putc	# Print it
	move	a0, v0		# (BD)

	beqz	v0, 4f		# If putc fails, give up on the number.
	sub     t8,1		# (BD) Entire number printed?

	bne     t8,zero,2b
	dsll	ta1,4		# Set up next nibble
4:
	j       t9		# Yes - done
	nop
	END(ccuart_puthex)


/*
 * Routine ccuart_puts
 *	Writes a null-terminated string to the CC UART.
 *
 * Arguments:
 *	a0 -- the address of the first character of the string.
 * Returns:
 *	v0 -- If non-zero, puts (probably) succeeded.  If zero,
 *	      puts definitely failed. 
 * Uses:
 *	a0, a1, v0, v1, ta2, ta3, t8, t9
 */

LEAF(ccuart_puts)
	.set	noreorder
	move	t9, ra			# Save the return address
	move	ta3, a0			# Save the argument register
	jal	getendian		# Get endianess
	nop	
	move	t8, v0			# Put endianess in t8
1:
        beq     t8, zero, 2f		# Skip to write char if big-endian 
        move    ta2, ta3  		# (BD) Copy next_char to curr_char
        xor     ta2, 3			# If EL, swizzle the bits
2:     
	jal	get_char 
	move	a0, ta2
	move	a0, v0
        daddi	ta3, 1			# Increment next char. 
        beq     a0, zero, 3f		# If char == zero, return
        nop				# (BD)
        jal     ccuart_putc		# 
        nop				# (BD)

	beqz	v0, 3f			# If putc fails, give up on the string
	nop				# (BD)

        b       1b			# Loop back up
        nop				# (BD)
3:
        j       t9			# Return
        nop				# (BD)
	END(ccuart_puts)

	.data
debug_mode_prompt:
	.asciiz	"\r\nEntering CPU board debug mode\r\n"

