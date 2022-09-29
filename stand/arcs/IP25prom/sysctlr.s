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

#include <asm.h>
	
#include <sys/regdef.h>
#include <sys/mips_addrspace.h>
	
#include <sys/EVEREST/sysctlr.h>
#include <sys/EVEREST/IP25.h>
	
#include "ml.h"
#include "ip25prom.h"

	.text
	.set	noreorder

#define	INVERT(_r)	xori	_r,_r,0xff
#define	PUTC(_c)	jal	ccuart_putc; ori a0,(0xff ^ _c)

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
 *	a0, a1, v0, v1, ta1, ta2, ta3, t8, t9
 */

LEAF(sysctlr_message)
	move	t0, ra			# Save our return address
	move	t1, a0			# Save message to be written

	jal	ccuart_putc		# Write the ESCAPE character
	ori	a0, zero, SC_ESCAPE	# (BD)_ Load escape character
	jal	ccuart_putc		# Write a character
	ori	a0, zero, SC_SET	# (BD) Set command
	jal	ccuart_putc		# Write a character
	ori	a0, zero, SC_MESSAGE	# Send a message

	jal	ccuart_puts		# Write out the actual message
	move	a0, t1			# (BD) pointer to message

	jal	ccuart_putc		# Write message terminator
	ori	a0, zero, SC_TERM	# (BD) Load term into a0 
	j	t0
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
 *	a0, a1, v0, v1, ta1, ta2, ta3, t8, t9
 */

LEAF(sysctlr_getdebug)
	move 	ta1, ra			# Save our return address

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
#ifdef SABLE
	li	ta2, 1 
#else
	li	ta2, 50000
#endif
1:	
	jal	ccuart_poll		# read a character
	nop
	sub	ta2, 1			# (BD)
	beqz	ta2, 8f			# If timeout expires goto end
	nop				# (BD)
	beqz	v0, 1b			# If no character loop
	nop				# (BD) 

	# Check to see if the first character is
 	# part of response.
	#
	jal	ccuart_getc		# Read character
	li	ta3, SC_RESP		# Load response character
	move	ta2, zero		# (BD)  Make sure we fail cleanly
	bne	v0, ta3, 8f		# Got the response character
	nop

	# If we get to this point, we're getting a response.
	# Check to see if the sequence number matches.
3:	#
	jal	ccuart_getc		# Get sequence number
	li	ta2, SC_DEBUG		# (BD) Load comparison value
	bne	v0, ta2, 8f		# Return if values don't match
	nop

	jal	ccuart_getc		# Get response type
	li	ta2, SC_DEBUG		# (BD) 
	bne	ta2, v0, 8f		#  Return if values don't match
	nop


	# Read in four bytes of data from system controller
	#
	li	ta3, 3
	move	ta2, zero
4:					# DO
	jal	ccuart_getc		#   Read byte of response
	nop				#   (BD)
	beqz	v0, 8f			# If we got the term char (NULL) return
	nop				# (BD)
	jal	ascii_to_hex		#   Convert byte to hex
	move	a0, v0			#   (BD)
	sll	ta2, 4			#   Shift prev value over
	or	ta2, v0			#   Merge in current byte
	dsub	ta3, 1			#   (BD) decrement counter
	bgez	ta3, 4b			#   Loop back up to read
					# UNTIL ta3 is zero
	nop
	
	jal	ccuart_getc		# Read terminating character
	nop				# (BD)
	b	9f
	nop

8:	move	ta2,zero

	# Return to caller
9:	#
	j	ta1			# Return	
	move	v0, ta2			# (BD) stuff return value into v0
	END(sysctlr_getdebug)


/*
 * sysctlr_getpanel
 *	Reads the magic debugging DIP switches from the system controller
 *	and returns them in v0.
 *
 * Returns:
 *	v0 -- 'E' (0x45) for the small panel and 'T' (0x54) for the large
 * Uses:
 *	a0, a1, v0, v1, ta1, ta2, ta3, t8, t9
 */

LEAF(sysctlr_getpanel)
	move 	ta1, ra			# Save our return address

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
	li	ta3, 5000000
	li	ta2, 0x45		# Default return value ('E')
1:	
	jal	ccuart_poll		# read a character
	nop
	sub	ta3, 1			# (BD)
	beqz	ta3, 8f			# If timeout expires goto end
	nop				# (BD)
	beqz	v0, 1b			# If no character loop
	nop				# (BD) 

	# Check to see if the first character is
 	# part of response.
	#
	jal	ccuart_getc		# Read character
	li	ta3, SC_RESP		# Load response character
	bne	v0, ta3, 8f		# Got the response character
	nop				# (BD)

	# If we get to this point, we're getting a response.
	# Check to see if the sequence number matches.
3:	#
	jal	ccuart_getc		# Get sequence number
	li	ta3, SC_PANEL		# (BD) Load comparison value
	bne	v0, ta3, 8f		# Return if values don't match
	nop

	jal	ccuart_getc		# Get response type
	li	ta3, SC_PANEL		# (BD) 
	bne	ta3, v0, 8f		#  Return if values don't match 
	nop

	jal	ccuart_getc		# Read the actual response
	nop				# (BD)
	move	ta2, v0			# Store the byte in t0

	jal	ccuart_getc		# Read the nice terminating NULL
	nop
	b	9f
	nop
8:	
	move	ta3, zero		# (BD)

	# Return to caller
9:	#
	j	ta1			# Return	
	move	v0, ta2			# (BD) stuff return value into v0
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
	ble	v0, zero, 1f		#   lower case 
	nop				# (BD)
	sub	t9, a0, 0x20		# If so, convert it to upper	
1:
	dla	a0, hexstring		# Load the address of the hex string
2:
	jal	get_char		# Read the first hex character
        nop				# (BD)
	li	v1, 0x46		# Load capital F
	beq	v0, t9, 3f		# If characters match, exit loop 
	nop
	beq	v0, v1, 9f		# If comparison char is 'F' exit loop
	nop

	b	2b			# LOOP and load next character
	dadd	a0, 1			# (BD) increment character pointer
3:
	dla	v1, hexstring
	sub	v0, a0, v1		# Calculate offset
	b	9f
	nop
8:
	move	v0,zero
9:
	j	t8
	nop
	END(ascii_to_hex)

	.data
hexstring:
	.ascii	"0123456789ABCDEF"
