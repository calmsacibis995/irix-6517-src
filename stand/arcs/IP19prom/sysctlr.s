/***********************************************************************\
*	File:		sysctlr.s					*
*									*
*	Contains code for performing simple system controller 		*
*	exchanges, like sending messages, reading the debug		*
*	switches, reading the current time, etc.			*
*	These functions are really just aspects of the 			*
*	Sysctlr/CC-uart	communications protocol.  This package 		*
*	encapsulates this protocol with a functional wrapper.		*
*									*
\***********************************************************************/

#include <regdef.h>
#include <asm.h>
#include <sys/EVEREST/sysctlr.h>
#include "ip19prom.h"

	.text
	.set	noreorder


/*
 * sysctlr_message
 * 	Display a message on the system controller LCD.
 *
 * Parameters:
 *	a0 -- Pointer to message to be displayed (better be less than 
 *	      20 characters in length, or the SysCtlr will freak).
 * Returns:
 *	Not much.
 * Uses:
 *	a0, a1, v0, v1, t5, t6, t7, t8, t9
 */

LEAF(sysctlr_message)
	move	t5, ra			# Save our return address
	move	t6, a0			# Save message to be written

	jal	ccuart_putc		# Write the ESCAPE character
	ori	a0, zero, SC_ESCAPE	# (BD)_ Load escape character
	jal	ccuart_putc		# Write a character
	ori	a0, zero, SC_SET	# (BD) Set command
	jal	ccuart_putc		# Write a character
	ori	a0, zero, SC_MESSAGE	# Send a message

	jal	ccuart_puts		# Write out the actual message
	move	a0, t6			# (BD) pointer to message

	jal	ccuart_putc		# Write message terminator
	ori	a0, zero, SC_TERM	# (BD) Load term into a0 

	# Spin for awhile to give the Sysctlr time to write out the message
	# What we're doing here is kind of lame, since it is processor 
	# speed dependent.  
	# 
	li	a0, 75000
2:	bnez	a0, 2b
	sub	a0, 1

	j	t5
	nop
	END(sysctlr_message)


/*
 * sysctlr_getdebug
 *	Reads the magic debugging DIP switches from the system controller
 *	and returns them in v0.
 *
 * Returns:
 *	v0 -- current setting of magic DIP switches.
 * Uses:
 *	a0, a1, v0, v1, t5, t6, t7, t8, t9
 */

LEAF(sysctlr_getdebug)
	move 	t5, ra			# Save our return address

	jal	ccuart_flush		# Flush out the garbage
	nop

	jal	ccuart_putc
	ori	a0, zero, SC_ESCAPE	# (BD) Load the ESCAPE character
	jal	ccuart_putc		# Write command
	ori	a0, zero, SC_GET	# (BD) Get
	jal	ccuart_putc		# Write sequence byte
	ori	a0, zero, SC_DEBUG	# (BD) Use request type as sequence 	
	jal	ccuart_putc		# Write debug
	ori	a0, zero, SC_DEBUG	# (BD) Debug value
	jal	ccuart_putc		# Write terminator
	ori	a0, zero, SC_TERM	# (BD) send term char. 

	# Now spin waiting for a reply with the proper sequence
 	# number to come back to us.  Since we don't expect to
	# have outstanding requests in the prom, we just throw
	# away responses, alarms or warnings we don't care about.
	#
	li	t6, 50000 
1:	
	jal	ccuart_poll		# read a character
	nop
	sub	t6, 1			# (BD)
	beqz	t6, 9f			# If timeout expires goto end
	nop				# (BD)
	beqz	v0, 1b			# If no character loop
	nop				# (BD) 

	# Check to see if the first character is
 	# part of response.
	#
	jal	ccuart_getc		# Read character
	li	t7, SC_RESP		# Load response character
	bne	v0, t7, 9f		# Got the response character
	move	t6, zero		# (BD)  Make sure we fail cleanly

	# If we get to this point, we're getting a response.
	# Check to see if the sequence number matches.
3:	#
	jal	ccuart_getc		# Get sequence number
	li	t6, SC_DEBUG		# (BD) Load comparison value
	bne	v0, t6, 9f		# Return if values don't match 
	move	t6, zero		# (BD)

	jal	ccuart_getc		# Get response type
	li	t6, SC_DEBUG		# (BD) 
	bne	t6, v0, 9f		#  Return if values don't match 
	move	t6, zero		# (BD) Return zero on error


	# Read in four bytes of data from system controller
	#
	li	t7, 3
	move	t6, zero
4:					# DO
	jal	ccuart_getc		#   Read byte of response
	nop				#   (BD)
	beqz	v0, 9f			# If we got the term char (NULL) return
	nop				# (BD)
	jal	ascii_to_hex		#   Convert byte to hex
	move	a0, v0			#   (BD)
	sll	t6, 4			#   Shift prev value over
	or	t6, v0			#   Merge in current byte
	bnez	t7, 4b			#   Loop back up to read
	sub	t7, 1			#   (BD) decrement counter
					# UNTIL t7 is zero
	jal	ccuart_getc		# Read terminating character
	nop				# (BD)

	# Return to caller
9:	#
	j	t5			# Return	
	move	v0, t6			# (BD) stuff return value into v0
	END(sysctlr_getdebug)


/*
 * sysctlr_getpanel
 *	Reads the magic debugging DIP switches from the system controller
 *	and returns them in v0.
 *
 * Returns:
 *	v0 -- 'E' (0x45) for the small panel and 'T' (0x54) for the large
 * Uses:
 *	a0, a1, v0, v1, t5, t6, t7, t8, t9
 */

LEAF(sysctlr_getpanel)
	move 	t5, ra			# Save our return address

	jal	ccuart_flush		# Flush out the garbage
	nop

	jal	ccuart_putc
	ori	a0, zero, SC_ESCAPE	# (BD) Load the ESCAPE character
	
	# Write out the get request
	#	
1:
	jal	ccuart_putc		# Write command
	ori	a0, zero, SC_GET	# (BD) Get
	jal	ccuart_putc		# Write sequence byte
	ori	a0, zero, SC_PANEL	# (BD) Use request type as sequence 	
	jal	ccuart_putc		# Write debug
	ori	a0, zero, SC_PANEL	# (BD) Panel value
	jal	ccuart_putc		# Write terminator
	ori	a0, zero, SC_TERM	# (BD) send term char. 

	# Now spin waiting for a reply with the proper sequence
 	# number to come back to us.  Since we don't expect to
	# have outstanding requests in the prom, we just throw
	# away responses, alarms or warnings we don't care about.
	#
	li	t7, 50000 
	li	t6, 0x45		# Default return value ('E')
1:	
	jal	ccuart_poll		# read a character
	nop
	sub	t7, 1			# (BD)
	beqz	t7, 9f			# If timeout expires goto end
	nop				# (BD)
	beqz	v0, 1b			# If no character loop
	nop				# (BD) 

	# Check to see if the first character is
 	# part of response.
	#
	jal	ccuart_getc		# Read character
	li	t7, SC_RESP		# Load response character
	bne	v0, t7, 9f		# Got the response character
	nop				# (BD)

	# If we get to this point, we're getting a response.
	# Check to see if the sequence number matches.
3:	#
	jal	ccuart_getc		# Get sequence number
	li	t7, SC_PANEL		# (BD) Load comparison value
	bne	v0, t7, 9f		# Return if values don't match 
	move	t7, zero		# (BD)

	jal	ccuart_getc		# Get response type
	li	t7, SC_PANEL		# (BD) 
	bne	t7, v0, 9f		#  Return if values don't match 
	move	t7, zero		# (BD) Return zero on error

	jal	ccuart_getc		# Read the actual response
	nop				# (BD)
	move	t6, v0			# Store the byte in t0

	jal	ccuart_getc		# Read the nice terminating NULL
	nop

	# Return to caller
9:	#
	j	t5			# Return	
	move	v0, t6			# (BD) stuff return value into v0
	END(sysctlr_getpanel)

/*
 * Routine ascii_to_hex
 *	Given an ascii character in a0, this routine
 *	converts it into a single integer value 
 *	between 0 and 15 and returns it in v0.
 */

LEAF(ascii_to_hex)
	move	t8, ra			# Save return address
	move	t9, a0			# Character being converted

	sub	v0, a0, 0x60		# Check to see if character is
	blez	v0, 1f			#   lower case 
	nop				# (BD)
	sub	t9, a0, 0x20		# If so, convert it to upper	
1:
	la	a0, hexstring		# Load the address of the hex string
2:
	jal	get_char		# Read the first hex character
	nop				# (BD)
	beq	v0, t9, 3f		# If characters match, exit loop 
	li	v1, 0x46		# (BD) Load capital F
	beq	v0, v1, 9f		# If comparison char is 'F' exit loop
	move	v0, zero		# (BD) Load zero

	b	2b			# LOOP and load next character
	add	a0, 1			# (BD) increment character pointer
3:
	la	v1, hexstring
	sub	v0, a0, v1		# Calculate offset
9:
	j	t8
	nop
	END(ascii_to_hex)

	.data
hexstring:
	.ascii	"0123456789ABCDEF"
