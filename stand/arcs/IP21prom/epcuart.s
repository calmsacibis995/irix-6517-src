/***********************************************************************\
*	File:		epcuart.s					*
*									*
*	Contains simple read and write routines for the IO4 EPC	UART.	*
*	These routines are written in assembler so that they can 	*
*	easily be called even when we have no stack.			*
*									*
* WARNING:								*
*	init_epc_uart() must be called before these routines will work. *
*									*
\***********************************************************************/

#include "ml.h"
#include <sys/regdef.h>
#include <asm.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/EVEREST/epc.h>
#include <sys/z8530.h>
#include "ip21prom.h"

#define CNTRL   0x0	
#define DATA	0x10	

/*
 * Routine epcuart_getc
 * 	Read a character from the EPC uart.  This routine will never return
 * 	unless a character is available.  Use epcuart_poll to check whether
 * 	a character is available.
 * Arguments:
 *	a0 -- Selected channel 
 * Returns:
 *	v0 -- the character read.
 * Uses registers:
 *	a0, a1, v0, v1.
 */
 
LEAF(epcuart_getc)
	.set	noreorder
	GET_DUARTBASE(a1)
	dadd	a0, a1			# Add 
2:
	lb	a1, CNTRL(a0) 		# Read the EPC UART control reg

	andi	v0, a1, RR0_RX_CHR	# Check to see if chr was recvd.
	beqz	v0, 2b			# If not received, jump back to top 
	nop				# (BD)
3:
	li	a1, RR1
	sb	a1, CNTRL(a0) 		# Tell uart we want the RR1 register
	nop
	lb	a1, CNTRL(a0) 		# Actually perform the read
	nop
	
	lb	v0, DATA(a0)		# Read the data register.

	andi	a1, (RR1_RX_ORUN_ERR | RR1_FRAMING_ERR | RR1_PARITY_ERR)
	beq	a1, zero, 9f		# If an ERROR occurred THEN
	nop				# (BD)
	move	v0, zero		# Put a zero into the return value
9:
	j	ra			# Return
	nop				# (BD)
	END(epcuart_getc)

/*
 * epcuart_flush
 *	Flushes the input buffer of the UART.
 * Arguments:
 *	a0 -- Selected channel number. 
 * Uses:
 *	a0, v0 
 */

LEAF(epcuart_flush)
	.set	noreorder
	GET_DUARTBASE(v0)
	dadd	a0, v0			# Add in the channel selector

	lb	v0, CNTRL(a0)		# Read the control reg.
	andi	v0, RR0_RX_CHR		# Check to see if char was recvd.
	beqz	v0, 3f			# Goto end if no character.
	nop				# 

	lb	v0, DATA(a0)		# Read the character
3:
	j	ra			# Return 
	nop				# 
	END(epcuart_flush)
		
/*
 * Routine epcuart_putc
 *	Writes a single character to the EPC UART.
 *
 * Arguments:
 *	a0 -- the byte to write. 
 *	a1 -- the channel to write to.
 * Returns:
 *	nothing.
 * Uses registers:
 *	a0, a1, v0, v1.	
 */

LEAF(epcuart_putc)
	.set	noreorder
	GET_DUARTBASE(v0)
	dadd	a1, v0			# Add in the channel selector

	li	v0, 10000		# Number of retry attempts
1: 	
	lb	v1, CNTRL(a1)		#   Read the control register
	nop
	andi	v1, RR0_TX_EMPTY	# UNTIL ready to transmit
	bnez	v1, 2f			# OR
	subu	v0, 1			# (BD) (--cnt == 0)
	bnez	v0, 1b			#  
	nop				# (BD)
	j	ra
	nop				# (BD)
2:
	j	ra			# Return
	sb	a0, DATA(a1)		# Write the character out		
	END(epcuart_putc)


/*
 * Routine epcuart_poll
 *	Checks to see whether a character is available for reading.
 *	If a character is available, v0 will be non-zero.
 * Arguments
 *	a0 -- Channel to poll on.
 * Returns:
 *	v0 -- Non-zero if a character is available, zero otherwise.
 * Uses registers:
 *	a0, v0.
 */

LEAF(epcuart_poll)
	.set	noreorder
	GET_DUARTBASE(v0)
	dadd	v0, a0			# Add in the channel selector
1:		
	lb	v0, CNTRL(v0)		# Read	
	nop
	j	ra			# Return
	andi	v0, RR0_RX_CHR
	END(epcuart_poll)


/* 
 * Routine epc_puthex
 *	Writes a 64-bit hex number to EPC UART.
 *
 * Arguments:
 *	a0 -- the 64-bit hex number to write.
 *	a1 -- Channel to write to.
 * Returns:
 *	v0 -- Success.  Non-zero indicates write successful.
 * Uses registers:
 *	a0, a1, a2, v0, v1, ta1, ta2, ta3, t8, t9
 */

LEAF(epcuart_puthex)
	.set	noreorder
	move	t9, ra
	jal	getendian
	li	t8, 16
	move    ta2, v0
	move    ta1, a0	  		# Copy argument to ta1
	move	a2, a1			# Save channel value
2:
	dsrl    a0,ta1,30		# Shift right 60 bits ($@!&*# assembler
	dsrl    a0,a0,30		#       won't assemble srl32 a0,ta1,28)
					# This isolates the rightmost nibble
	dla	ta3,hexdigit
	beq     ta2,zero,3f      	# 0 = big endian
	daddu	a0,ta3	   		# (BD)
	xor     a0,3	    		# assumes hexdigit burned in eb order
3:
	jal     get_char 
	nop		     		# (BD)

	move	a1, a2			# Reload channel information
	jal     epcuart_putc     	# Print it
	move    a0, v0	  		# (BD) Load character to be written

	beqz	v0, 4f			# If putc fails, give up on the number
	sub     t8,1	    		# Entire number printed?
	bne     t8,zero,2b
	dsll    ta1,4	    		# Set up next nibble
4:
	j       t9	      		# Yes - done
	nop
	END(epcuart_puthex)


/*
 * Routine epcuart_puts
 *	Writes a null-terminates string to the EPC UART.
 *
 * Arguments:
 *	a0 -- the address of the first character of the string.
 *	a1 -- The channel to write to.
 * Returns:
 *	v0 -- If non-zero, puts (probably) succeeeded. Otherwise, it failed.
 * Uses:
 *	a0, a1, a2, v0, v1, ta2, ta3, t8, t9
 */

LEAF(epcuart_puts)
	.set    noreorder
        move    t9, ra                  # Save the return address
        move    ta3, a0                  # Save the argument register
	move	a2, a1			# Save the channel number
        jal     getendian               # Get endianess
        nop     
        move    t8, v0                  # Put endianess in t8
1:
        beq     t8, zero, 2f            # Skip to write char if big-endian 
        move    ta2, ta3                  # (BD) Copy next_char to curr_char
        xor     ta2, 3                   # If EL, swizzle the bits
2:     
        jal     get_char 
        move    a0, ta2
        move    a0, v0
        dadd	ta3, 1			# Increment next char. 
        beq     a0, zero, 3f            # If char == zero, return
        nop                             # (BD)
        jal     epcuart_putc            # 
        move	a1, a2 			# (BD) Reload channel number

	beqz	v0, 3f       		# If putc fails, give up on the string
	nop				# (BD)
 
        b       1b                      # Loop back up
        nop                             # (BD)
3:
        j       t9                      # Return
        nop                             # (BD)
	END(epcuart_puts)
