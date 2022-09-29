/* Removed by Margie because Makefile defines it with -D */
/* #define R4000	1 */

#include <asm.h>
#include <sys/regdef.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/i8251uart.h>

#ifdef TFP
#define t4 ta0
#define t5 ta1
#define t6 ta2
#define t7 ta3
#endif

#if R4000
#define LW lw
#define SW sw
#define ADD add
#define ADDI addi
#define ADDIU addiu
#define ADDU addu
#define SRL srl
#define SLL sll
#define LL ll
#define SC sc
#define SUB sub
#define SUBU subu
#define WORDSZ_SHIFT 2
#define SR_COUNTS_BIT SR_IBIT8
#define CAUSE_COUNTS_BIT CAUSE_IP8
#elif TFP
/* LA, LI, MFC0, MTC0, PTR_WORD defined in asm.h */
#define LW ld
#define SW sd
#define ADD dadd
#define ADDI daddi
#define ADDIU daddiu
#define ADDU daddu
#define SRL dsrl
#define SLL dsll
#define LL lld
#define SC scd
#define SUB dsub
#define SUBU dsubu
#define WORDSZ_SHIFT 3
#define SR_COUNTS_BIT SR_IBIT10
#define CAUSE_COUNTS_BIT CAUSE_IP10
#define SR_KSUMASK SR_KU
#define C0_ENHI C0_TLBHI
#define C0_INX C0_TLBSET
#define PFNSHIFT TLBLO_PFNSHIFT
#define SR_USER SR_KU
#define TLBLO_VMASK TLBLO_V
#define TLBLO_DMASK TLBLO_D
#define TLBLO_CSHIFT TLBLO_CACHSHIFT
#endif

#define         CMD     0x0
#define         DATA    0x8

	.set	noreorder
	.set noat
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
	LI	a1, EV_UART_BASE
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
	ADDU    t9, ra, zero
	jal	getendian
	li      t8, 16		# Number of digits to display (BD)
	ADDU	t6, v0, zero
	ADDU	t5, a0, zero	# Copy argument to t5
2:
	dsrl	a0,t5,30	# Rotate right 60 bits ($@!&*# assembler
	dsrl	a0,a0,30 	# 	won't assemble srl32 a0,t5,28)
				# This isolates the rightmost nibble
	LA      t7,hexdigit
	beq     t6,zero,3f	# 0 = big endian
	ADDU    a0,t7		# (BD)
	xor     a0,3		# assumes hexdigit burned in prom in eb order
3:
	lb	a0, 0(a0)	# Load the character
	jal     pul_cc_putc	# Print it
	SUB     t8,1		# Entire number printed? (BD)
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
	ADDU    t9, ra, zero
	jal	getendian
	li      t8, 2		# Number of digits to display (BD)
	ADDU	t6, v0, zero
	ADDU	t5, a0, zero		# Copy argument to t5
2:
	dsrl	a0,a0,4 	# This isolates the rightmost nibble
	LA      t7,hexdigit
	beq     t6,zero,3f	# 0 = big endian
	ADDU    a0,t7		# (BD)
	xor     a0,3		# assumes hexdigit burned in prom in eb order
3:
	lb	a0, 0(a0)	# Load the character
	jal     pul_cc_putc	# Print it
	SUB     t8,1		# Entire number printed? (BD)
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
	ADDU	t9, ra, zero			# Save the return address
	ADDU	t7, a0, zero			# Save the argument register
	jal	getendian		# Get endianess
	nop	
	ADDU	t8, v0, zero			# Put endianess in t8
1:
        beq     t8, zero, 2f		# Skip to write char if big-endian 
        ADDU    t6, t7, zero  		# (BD) Copy next_char to curr_char
        xor     t6, 3			# If EL, swizzle the bits
2:     
	lb	a0, 0(t6)
        ADDI    t7, 1			# Increment next char. 
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
	LI	v1, EV_UART_BASE
	li	a1, 1
	ADDU	a2, ra, zero
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
	MFC0    (v0,C0_CONFIG)
	nop
	SRL     v0,CONFIG_BE_SHFT
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
