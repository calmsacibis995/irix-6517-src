
#define R4000	1

#include <asm.h>
#include <sys/regdef.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/i8251uart.h>

#define         CMD     0x0
#define         DATA    0x8

	.set	noreorder
/*
 * Routine pul_cc_putc
 * 	Writes a single character to the CC UART.
 *
 * Arguments:
 *	a0 -- ASCII value of character to be written
 * Returns:
 *	v0 -- If non-zero, putc succeeded.	
 */

LEAF(pul_cc_putc)
	li	a1, EV_UART_BASE
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
END(pul_cc_putc)

/* 
 * Routine pul_cc_puthex
 *	Writes a 64-bit hexadecimal value to the CC UART.
 *
 * Arguments:
 *	a0 -- the 64-bit hex number to write.
 * Returns:
 *	v0 -- Success.  Non-zero indicates write successful.
 */

LEAF(pul_cc_puthex)
	move    t9, ra
	jal	getendian
	li      t8, 16		# Number of digits to display (BD)
	move	t6, v0
	move	t5, a0		# Copy argument to t5
2:
	dsrl	a0,t5,30	# Rotate right 60 bits ($@!&*# assembler
	dsrl	a0,a0,30 	# 	won't assemble srl32 a0,t5,28)
				# This isolates the rightmost nibble
	la      t7,hexdigit
	beq     t6,zero,3f	# 0 = big endian
	addu    a0,t7		# (BD)
	xor     a0,3		# assumes hexdigit burned in prom in eb order
3:
	lb	a0, 0(a0)	# Load the character
	jal     pul_cc_putc	# Print it
	sub     t8,1		# Entire number printed? (BD)
	bne     t8,zero,2b
	dsll	t5,4		# Set up next nibble
	j       t9		# Yes - done
	nop
END(pul_cc_puthex)


/* 
 * Routine pul_cc_putbyte
 *	Writes a 8-bit hexadecimal value to the CC UART.
 *
 * Arguments:
 *	a0 -- the 8-bit hex number to write.
 * Returns:
 *	v0 -- Success.  Non-zero indicates write successful.
 */

LEAF(pul_cc_putbyte)
	andi	a0, 0xff
	move    t9, ra
	jal	getendian
	li      t8, 2		# Number of digits to display (BD)
	move	t6, v0
	move	t5, a0		# Copy argument to t5
2:
	dsrl	a0,a0,4 	# This isolates the rightmost nibble
	la      t7,hexdigit
	beq     t6,zero,3f	# 0 = big endian
	addu    a0,t7		# (BD)
	xor     a0,3		# assumes hexdigit burned in prom in eb order
3:
	lb	a0, 0(a0)	# Load the character
	jal     pul_cc_putc	# Print it
	sub     t8,1		# Entire number printed? (BD)
	bne     t8,zero,2b
	dsll	t5,4		# Set up next nibble
	j       t9		# Yes - done
	nop
END(pul_cc_putbyte)


/*
 * Routine pul_cc_puts
 *	Writes a null-terminated string to the CC UART.
 *
 * Arguments:
 *	a0 -- the address of the first character of the string.
 * Returns:
 *	v0 -- If non-zero, puts (probably) succeeded.  If zero,
 *	      puts definitely failed. 
 */

LEAF(pul_cc_puts)
	.set	noreorder
	move	t9, ra			# Save the return address
	move	t7, a0			# Save the argument register
	jal	getendian		# Get endianess
	nop	
	move	t8, v0			# Put endianess in t8
1:
        beq     t8, zero, 2f		# Skip to write char if big-endian 
        move    t6, t7  		# (BD) Copy next_char to curr_char
        xor     t6, 3			# If EL, swizzle the bits
2:     
	lb	a0, 0(t6)
        add     t7, 1			# Increment next char. 
        beq     a0, zero, 3f		# If char == zero, return
        nop				# (BD)
        jal     pul_cc_putc		# 
        nop				# (BD)
	
        b       1b			# Loop back up
        nop				# (BD)
3:
        j       t9			# Return
        nop				# (BD)
END(pul_cc_puts)

/*
 * Routine pul_cc_getc
 *	Reads a single character from the CC UART.
 *
 * Arguments:
 * 	None.
 * Returns:
 *	v0 -- The character read.  
 */

LEAF(pul_cc_getc)
	li	v1, EV_UART_BASE
	li	a1, 1
	move	a2, ra
1:
	li	a0, 50000
2:					# 
	ld	v0, CMD(v1) 		# DO 
	nop
	andi	v0, I8251_RXRDY			
	beq	v0, zero, 2b		# WHILE (not ready && 
	nop				# (BD)
	j	a2			# Return 
	ld	v0, DATA(v1)		# (BD) Read character from register
END(pul_cc_getc)


/* Get the endianness of this cpu from the config register */
LEAF(getendian)
	.set    noreorder
	mfc0    v0,C0_CONFIG
	nop
	srl     v0,CONFIG_BE_SHFT
	and     v0,1
	xor     v0,1                    # big = 0, little = 1
	j       ra
	nop
	.set    reorder
END(getendian)

	.globl	hexdigit
hexdigit:
	.ascii	"0123456789abcdef"
	.globl	crlf
crlf:
	.asciiz	"\r\n"
