/***********************************************************************\
*	File:		ccuart_io.s					*
*									*
*	This file contains the IP19 PROM Utility Library routines for	*
*	manipulating the CC chip's LEDs and UART.			*
*									*
\***********************************************************************/

#ident "$Revision: 1.6 $"

#include <regdef.h>
#include <asm.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/i8251uart.h>
#include <ml.h>

#define         CMD     0x0
#define         DATA    0x8

	.text
	.set 	noreorder
	.set	noat


/*
 * Routine ccuart_poll 
 *	Poll the CC UART to see if a charactre is available
 *
 * Arguments:
 * 	None.
 * Returns:
 *	v0 -- Nonzero if a character is available, zero otherwise
 */

LEAF(ccuart_poll) 
	LI	v1, EV_UART_BASE
	ld	v0, CMD(v1) 		#   Get status bit
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
 */

LEAF(ccuart_getc)
	LI	v1, EV_UART_BASE
1:					# 
	ld	v0, CMD(v1) 		# DO 
	nop
	andi	v0, I8251_RXRDY			
	beqz	v0, 1b			# WHILE (not ready && 
	nop				# (BD)
	
	j	ra			# Return 
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
 */

LEAF(ccuart_putc)
	LI	a1, EV_UART_BASE
#ifndef SABLE
	LI	v0, 1000000
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
#endif
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
 */

LEAF(ccuart_flush)
	LI	v0, EV_UART_BASE	
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
 */

LEAF(ccuart_puthex)
	move    t9, ra
	LI	t8, 8 		# Number of digits to display (BD)
	LI	ta2, 0 
	move	ta1, a0		# Copy argument to ta1
2:
	dsrl	a0,ta1,30	# Rotate right 60 bits ($@!&*# assembler
	dsrl	a0,a0,30 	# 	won't assemble srl32 a0,ta1,28)
				# This isolates the rightmost nibble
	LA	ta3,hexdigit
	PTR_ADDU ta3,a0		# Calculate address of hex byte 
	jal     ccuart_putc	# Print it
	lb	a0, 0(ta3)	# (BD) Read byte	
	sub     t8,1		# Entire number printed?
	bne     t8,zero,2b	# 
	dsll	ta1,4		# (BD) Set up next nibble

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
 */

LEAF(ccuart_puts)
	.set	noreorder
	move	t9, ra			# Save the return address
	move	t8, a0
1:
	lb	a0, 0(t8)		# Load the char to be printed
	nop				# 
        beq     a0, zero, 3f		# If char == zero, return
        nop				# (BD)
        jal     ccuart_putc		# 
        nop				# (BD)

        b       1b			# Loop back up
       	PTR_ADDU t8, 1			# (BD) Increment the string pointer
3:
        j       t9			# Return
        nop				# (BD)
	END(ccuart_puts)

	.data
hexdigit:
	.asciiz	"0123456789abcdef"
