#include <regdef.h>
#include <asm.h>
#include "../pod.h"
#include "nmsg.h"
#include "nsys.h"
#include "cp0_r4k.h"
#include <sys/EVEREST/everest.h>
#include "niblet.h"
#include "passfail.h"
#include "locks.h"

#define CONFIG_DC_SHFT  6       /* shift DC to bit position 0 */
#define CACH_PD         0x1     /* primary data cache */
#define CACH_SD         0x3     /* secondary data cache */
#define C_IWBINV        0x0     /* index writeback inval (d, sd) */
#define C_ILT           0x4     /* index load tag (all) */
#define C_IST           0x8     /* index store tag (all) */
#define	TLBHI_VPNMASK32	0xffffe000
#define EV_CELMAX       127

#if 0
#define PRINT_TAGS(_address)			\
	li	t0, _address;			\
	PRINT_HEX(t0);				\
	cache	C_ILT|CACH_PD, 0(t0);		\
	nop; nop; nop; nop;			\
	mfc0	t1, C0_TAGLO;			\
	nop; nop; nop; nop;			\
	PRINT_HEX(t1);				\
	addi	t0, 0x1000;			\
	cache	C_ILT|CACH_PD, 0(t0);		\
	nop; nop; nop; nop;			\
	mfc0	t1, C0_TAGLO;			\
	nop; nop; nop; nop;			\
	PRINT_HEX(t1);				\
	nop
#else
#define PRINT_TAGS(_address)
#endif /* 0 */

#define WIRED_ENTRIES

#define ERET			\
	move	k0, CURPROC;	\
	RESTORE_T(k0);		\
	nop;			\
	nop;			\
	nop;			\
	nop;			\
	eret;

#define EXCHEAD	\
	nop;	\
	nop;	\
	nop;	\
	nop;

#if 0
#define SET_LEDS(_value)		\
	li	k0, EV_LED_BASE;	\
	li	k1, _value;		\
	sd	k1, 0(k0);		\
	li	k0, 0xbfc00000;		\
	ld	zero, 0(k0);		\
	nop

#define SET_LEDS_R(_reg)		\
	li	k0, EV_LED_BASE;	\
	sd	_reg, 0(k0);		\
	li	k0, 0xbfc00000;		\
	ld	r0, 0(k0);		\
	nop
#else
#define SET_LEDS(_Value)
#define SET_LEDS_R(_reg)
#endif /* 0 */
#if 0
#define	BREAK(_point)	break	_point
#else
#define BREAK(_point)	SET_LEDS(_point)
#endif

	.set 	noreorder
	.set    noat

	.globl	about_to_exit
	.globl	sched
	.globl	notexc
	.globl	unsupported
	.globl	badexc
	.globl	badaddr
	.globl	tlbs
	.globl	tlbl
	.globl	tlbm
	.globl	syscall
	.globl	msgs
	.globl	runqueue
	.globl	runqlock
	.globl	rlockcheck
	.globl	initlock
	.globl	goahead
	.globl	mpfail0
	.globl	mpfail1
	.globl	mpfail2
	.globl	lock1
	.globl	unlock1
	.globl	lock2
	.globl	unlock2
	.globl	lock3
	.globl	unlock3
	.globl	lock4
	.globl	unlock4
	.globl	lock5
	.globl	unlock5
	.globl	lock6
	.globl	unlock6
	.globl	invalidpage
	.globl	runnewproc
	.globl	atend
	.globl	goon
	.globl	badendian
	.globl	badendian2
	.globl	msg0
	.globl	msg20
	.globl	exceptnames
	.globl	loadename
	.globl	startloop
	.globl	cpunum
	.globl	flag
	.globl	turn
	.globl	usr_hndlr_ret
	.globl	stolen_vector
	.globl	intfail
	.globl	itest_fail
	.globl	notsched
	.globl	i_test
	.globl	schedrunlock
	.globl	foundone
	.globl	sys_save
	.globl	state_saved
	.globl	sys_restore
	.globl	user_restore
	.globl	state_restored
	.globl	vced
#ifdef FIXED
	.globl	cs_in
	.globl	cs_out
#endif /* FIXED */
	.globl	nsys_fail	# Fail
	.globl	nsys_pass	# Pass
	.globl	nsys_rest	# Rest
	.globl	nsys_gtime	# Get time
	.globl	nsys_gquant	# Get current quantum
	.globl	nsys_squant	# Set current quantum
	.globl	nsys_ordain	# Ordain (go into kernel mode)
	.globl	nsys_defrock	# Defrock (back to user mode)
	.globl	nsys_msg	# Message
	.globl	nsys_gnproc	# Get the number of processes alive
	.globl	nsys_mark	# Just return
	.globl	nsys_invalid	# Invalidate a TLB entry
	.globl	nsys_scoher	# Set coherency for a page
	.globl	nsys_gpid	# Get process ID
	.globl	nsys_gcount	# Get the approximate instruction count
	.globl	nsys_printhex	# Print the value of r2 in hex.
	.globl	nsys_gshared	# Get the address of the shared memory space
	.globl	nsys_steale	# Steal an exception vector
	.globl	nsys_steali	# Steal an interrupt vector.
	.globl  nsys_suspnd	# Suspend scheduling
	.globl	nsys_resume	# Resume scheduling
	.globl	nsys_wpte	# Write a new page table entry.
	.globl	nsys_gecount	# Get exception count
	.globl	nsys_cecount	# Clear exception count
	.globl	nsys_vpass	# Verbose version of nsys_pass
	.globl	nsys_csout	# Set up user context switch out routine
	.globl	nsys_csin	# Set up user context switch in routine
	.globl	nsys_mode2	# Switch to MIPS II mode
	.globl	nsys_mode3	# Switch to MIPS III mode
	.globl	nsys_vtop	# Get phys addr for virtual address
	.globl	memerr

#ifdef MP
#ifdef LOCKS
	.globl	missed
	.globl	startloop
	.globl	waitglock
#endif	/* LOCKS */
#endif	/* MP */

	/* Must load at virtual address 0x8000000 */
	.text
	.globl nib_obj_start
nib_obj_start:		/* A required label for nextract */
	.globl nib_exc_start
nib_exc_start:		/* A required label for nextract */

LEAF(TLBREFILL)
	EXCHEAD
	BREAK(12)
	la	k0, nib_tlb_refill
	j	k0
	nop
	END(TLBREFILL)

	.align 7
LEAF(XTLBREFILL)
	EXCHEAD
	BREAK(13)
	la	k0, nib_xtlb_refill
	j	k0
	nop
	END(XTLBREFILL)

	.align 7
LEAF(CACHEERROR)
	EXCHEAD
	la	k0, nib_cache_err

	# Jump to the handler uncached!
	li	k1, 0x1fffffff
	and	k0, k1
	lui	k1, 0xa000
	or	k0, k1

	j	k0
	nop					# (BD)
	END(CACHEERROR)

	.align 7				# All other exceptions
	
LEAF(GENEXCPT)
	EXCHEAD
	BREAK(14)
	la	k0, nib_gen_exc
	j	k0
	nop
	END(GENEXCPT)

	.globl nib_exc_end	/* A required label for nextract */
nib_exc_end:
	.align	3

	.globl nib_text_start	/* A required label for nextract */
nib_text_start:

LEAF(nib_tlb_refill)
	mfc0	k1, C0_BADVADDR
	nop
	bltz	k1, nib_gen_exc	# Jump to GENEXCPT if kernel address
	mfc0    k1, C0_EPC	# Restore k1's old value.
	mfc0    k0, C0_CTXT
	nop
	nop
	lw      k1, 0(k0)
	lw      k0, 8(k0)                # LDSLOT load the second mapping
	nop
	nop
	mtc0    k1, C0_ENLO0
	mtc0    k0, C0_ENLO1
	nop
	tlbwr
	nop
	nop
	nop
	nop

	eret
	END(nib_tlb_refill)

LEAF(nib_xtlb_refill)
	mfc0	k1, C0_BADVADDR
	nop
	bltz	k1, GENEXCPT	# Jump to GENEXCPT if kernel address
	mfc0    k1, C0_EPC	# Restore k1's old value.
	mfc0    k0, C0_CTXT
	nop

	lw      k1, 0(k0)
	lw      k0, 8(k0)                # LDSLOT load the second mapping
	nop

	mtc0    k1, C0_ENLO0
	mtc0    k0, C0_ENLO1
	nop
	tlbwr
	nop
	nop	
	nop
	eret
	END(nib_xtlb_refill)

LEAF(nib_cache_err)
	ECCFAIL
	END(nib_cache_err)

LEAF(nib_gen_exc)
	SET_LEDS(11)

	li	k0, EV_SPNUM
	ld	k0, 0(k0)		# Get slot/processor number
	la	k1, directory		# Addresses of private data areas (LD)
	andi	k0, EV_SPNUM_MASK	# Mask off extraneous stuff
	sll	k0, DIR_SHIFT		# Shift by log2(dir ent size)
	add	k1, k0			# k1 = address of our pointers
	lw	k0, DIR_PRIVOFF(k1)	# k0 = our private area
	nop
	lw	k1, P_CURPROC(k0)	# Address of current process's
					# 	"U area"
	SAVE_T(k1)

	move	CURPROC, k1		# s0 contains CURPROC
	move	PRIVATE, k0		# s2 contains PRIVATE
#ifdef TIMEOUT
	# See if we're looping in an exception handler, and if so, fail
	mfc0	t1, C0_COUNT
	nop
	li	t0, 0x05000000

	sub	t0, t0, t1
	bltz	t0, nib_timeout
	nop
#endif /* TIMEOUT */

	li	t0, EV_ERTOIP		# If we've detected a CC error, fail
	ld	t0, 0(t0)
	nop
	bnez	t0, ertoip
	nop				# (BD)	

	mfc0	t0, C0_CAUSE		# Get exception cause register
	
	nop

	li t1, CAUSE_EXCMASK		# Is it an exception?
	and t1, t0, t1			# Mask off the approprtiate bits

	# Check per-process table
	add	t0, CURPROC, t1

	# Address to which EXCCNTBASE and EXCTBLBASE can be added.

	lw	t2, U_EXCCNTBASE(t0)	# Get current count
	nop
	addi	t2, t2, 1		# add one
	sw	t2, U_EXCCNTBASE(t0)	# Store new count
	lw	k0, U_EXCTBLBASE(t0)	# Get handler vector
	mfc0	MVPC, C0_EPC		# Get EPC (MVPC = s1)
	nop
	beq	k0, r0, std_hndlr
	sw	MVPC, P_MVPC(PRIVATE)	# (BD) Store away old EPC

stolen_vector:
	# Make sure this sucker's in the TLB.
	# ############################################################
	# k0 contains the user routine virtual address.
	lw	t1, U_EXCENHIBASE(t0)  # Get ENHI for this vector
	nop

#ifdef WIRED_ENTRIES
	mtc0	t1, C0_ENHI
	nop
	tlbp	
	nop
	mfc0	t0, C0_INX			# Get index register
	nop
	bgez	t0, excintlb			# If present, go on
	li	k1, PTEBASE			# Get page table base
	srl	t0, k0, PGSZ_BITS + 1		# Get VPN / 2
	sll	t0, t0, 4
	add	t0, t0, k1			# Get PTE address
	lw      k1, 0(t0)			# Load first mapping
	lw      t0, 8(t0)             		# load the second mapping
	nop
	mtc0	t1, C0_ENHI
	mtc0    k1, C0_ENLO0
	mtc0    t0, C0_ENLO1
	nop
	tlbwr
	nop
excintlb:
#else
	jal	force_enhi_tlb			# Force the page into the TLB
	nop
#endif

	# ############################################################
	la	t0, usr_hndlr_ret		# Put return address in R_VECT
	sw	t0, U_RVECT(CURPROC)
	la	t0, pass_to_niblet		# Standard handler + preamble
	sw	t0, U_SYSVECT(CURPROC)
	j	k0
	nop

usr_hndlr_ret:
	lw	MVPC, P_MVPC(PRIVATE)		# Get old EPC
	nop					# (LD)
	addi	MVPC, MVPC, 4			# calculate the next address
	mtc0	MVPC, C0_EPC			# Put EPC back.
	nop
	ERET
	nop

pass_to_niblet:
	mfc0	t0, C0_CAUSE 	# Get the cause register
	nop
	li t1, CAUSE_EXCMASK	# What flavor of exception
	and t1, t0, t1	# Mask off the approprtiate bits
std_hndlr:
	# t1 now contains a word offset from the table start.
	# EPC is in MVPC

	la t0, excvect 		# Load table address

	add t0, t0, t1	# Add in offset
	lw t0, 0(t0)
	nop
	j t0
	nop

	# Exception Vector Table
excvect:
	.word	notexc		# Exc 0  (Interrupt)
	.word	unsupported 	# Exc 1	 (TLB modify exception)
	.word	tlbl		# Exc 2	 (TLB Load miss)
	.word	tlbs		# Exc 3	 (TLB Store miss)
	.word	unsupported	# Exc 4  (Read address error)
	.word	unsupported	# Exc 5  (Write address error)
	.word	unsupported	# Exc 6  (I fetch bus error)
	.word	unsupported	# Exc 7  (Data bus error)
	.word	syscall		# Exc 8  (Syscall)
	.word	unsupported	# Exc 9  (Breakpoint)
	.word	unsupported	# Exc 10 (reserved instruction)
	.word	unsupported	# Exc 11 (coprocessor unusable)
	.word	unsupported	# Exc 12 (arithmetic overflow)
	.word	unsupported	# Exc 13 (Trap (MIPS II only))
	.word	vcei		# Exc 14 (Instruction Virtual coherency excptn)
	.word	unsupported	# Exc 15 (Floating point exception)
	.word	unsupported	# Exc 16 (reserved)
	.word	unsupported	# Exc 17 (reserved)
	.word	unsupported	# Exc 18 (reserved)
	.word	unsupported	# Exc 19 (reserved)
	.word	unsupported	# Exc 20 (reserved)
	.word	unsupported	# Exc 21 (reserved)
	.word	unsupported	# Exc 22 (reserved)
	.word	unsupported	# Exc 23 (reference to watch hi/lo address)
	.word	unsupported	# Exc 24 (reserved)
	.word	unsupported	# Exc 25 (reserved)
	.word	unsupported	# Exc 26 (reserved)
	.word	unsupported	# Exc 27 (reserved)
	.word	unsupported	# Exc 28 (reserved)
	.word	unsupported	# Exc 29 (reserved)
	.word	unsupported	# Exc 30 (reserved)
	.word	vced	# Exc 31 (Data virtual coherency exception)

unsupported:
	# Get pid and print it
	lw	t0, U_PID(CURPROC)
	PRINT_HEX(t0)

	li	t0, MIND2OFF(M_UNSUPPORT)
	la	t1, msgs
	add	t0, t1, t0
	STRING_DUMP(t0)

	mfc0	t0, C0_CAUSE
	li	t1, CAUSE_EXCMASK		# Which exception?
	and	t1, t0, t1	# Mask off the approp. bits
	sll	t1, t1, (MSG_BITS - 2)

loadename:
	la	t0, exceptnames
	add	t0, t0, t1
	STRING_DUMP(t0)

	mfc0	t0, C0_CAUSE
	mfc0	t1, C0_EPC
	PRINT_HEX(t0)
	PRINT_HEX(t1)
	mfc0	t0, C0_SR
	mfc0	t1, C0_BADVADDR
	PRINT_HEX(t0)
	PRINT_HEX(t1)
	# Get PID
	lw	t2, U_PID(CURPROC)
	PRINT_HEX(t2)
	nop

	FAIL(NIBFAIL_BADEXC)
	nop

        .globl  ertoip
ertoip:
	li	t0, MIND2OFF(M_ERTOIP)
	la	t1, msgs
	add	t0, t1, t0
	STRING_DUMP(t0)
	li	t0, EV_ERTOIP
	ld	t0, 0(t0)
	mfc0	t1, C0_CAUSE
	nop
	PRINT_HEX(t0)
	mfc0	t0, C0_EPC
        nop
	PRINT_HEX(t1)
	PRINT_HEX(t0)

	FAIL(NIBFAIL_ERTOIP)
	nop

vced:
	li	t1, 31
        sll     t1, t1, MSG_BITS
        la      t0, exceptnames
        add     t0, t0, t1
        STRING_DUMP(t0)
	b	1f
	nop

vcei:
	li	t1, 14
        sll     t1, t1, MSG_BITS
        la      t0, exceptnames
        add     t0, t0, t1
        STRING_DUMP(t0)

1:
	mfc0	t0, C0_BADVADDR
        nop; nop; nop; nop

	PRINT_HEX(t0)
	
	# Need to get physical address corresponding to this
	# 	virtual address.

	li	t1, TLBHI_VPNMASK32
	and	t1, t0			# Get VPN2 for ENHI
					# Only for 4k pages
	lw	t2, U_PID(CURPROC)
	or	t1, t2			# Or in PID

	mtc0	t1, C0_ENHI
        nop; nop; nop; nop
	
	tlbp
	nop

#ifdef VCEDEBUG
	PRINT_HEX(t1)
#endif

	mfc0	t1, C0_INX
        nop; nop; nop; nop

#ifdef VCEDEBUG
	PRINT_HEX(t1)
#endif

	bltz	t1, take_a_shot
	nop					# (BD)

#ifdef VCEDEBUG
	li	k0, 0xaaaaaaaa
	PRINT_HEX(k0)
#endif

	# Get the address from ENLO

	tlbr
	nop

	li	t1, (1 << PGSZ_BITS)
	and	t1, t0		# Even or odd page?
	
	beqz	t1, 1f
	nop

	b	2f
	mfc0	t1, C0_ENLO1		# Odd page (BD)

1:
	mfc0	t1, C0_ENLO0		# Even page

2:	nop; nop; nop; nop

#ifdef VCEDEBUG
	PRINT_HEX(t1)
#endif

	andi	k0, t1, TLBLO_VMASK	# Valid?
	beq	k0, r0, take_a_shot		# If not, guess...
	nop					# (BD)
	
	li	k0, TLBLO_PFNMASK
	and	t1, k0			# PFN in place
	sll	t1, PFNSHIFT		# Actual Phys addr of Pg

	li	k0, 0x0fff			# Mask off "index" bits
	and	k1, t0, k0			# From BadVaddr
	
	b 	get_stag
	or	t1, k1			# Synthesized address (BD)
	
take_a_shot:

#ifdef VCEDEBUG
	li	k0, 0xbbbbbbbb
	PRINT_HEX(k0)
#endif

        cache   C_ILT|CACH_PD, 0(t0)	# Get primary tag
        nop; nop; nop; nop

        mfc0    t1, C0_TAGLO		# Get TagLo
        nop; nop; nop; nop

#ifdef VCEDEBUG
	PRINT_HEX(t1)
#endif


	srl	t1, 8			# Only get PTagLo field
	sll	t1, 12			# Make it into an address

get_stag:

#ifdef VCEDEBUG
	PRINT_HEX(t1)
#endif

	# t1 = phsical address 

	lui	k0, 0x8000
	or	t1, k0			# Make it a k0seg address
	li	k0, ~(0x7f)		# All but bottom 7 bits
	and	t1, k0			# Cache line address

#ifdef VCEDEBUG
	PRINT_HEX(t1)
#endif


	cache	C_ILT|CACH_SD, 0(t1)	# Get STagLo
        nop; nop; nop; nop
        mfc0    k0, C0_TAGLO		# k0 = STagLo
        nop; nop; nop; nop

#ifdef VCEDEBUG
	PRINT_HEX(k0)
#endif

	li	k1, ~(0x0380)		# Mask off bits 9..7 (VIndex)
	and	k0, k1			# k0 = STagLo w/o Virtual Index
	li	k1, 0x7000		# Mask for bits 14..12
	and	k1, t0			# Get virtual index for Bad Vaddr
	srl	k1, 5			# Shift from 14..12 to 9..7
	or	k0, k1			# Or in new virtual index

#ifdef VCEDEBUG
	PRINT_HEX(k0)
#endif

	mtc0	k0, C0_TAGLO
        nop; nop; nop; nop
	cache	C_IST|CACH_SD, 0(t1)	# Get STagLo
        nop; nop; nop; nop

	ERET
syscall:
	mfc0	MVPC, C0_EPC		# Get EPC
	nop
	addiu	MVPC, 4			# Point to next instruction
	mtc0	MVPC, C0_EPC		# Save EPC
	sw	MVPC, P_MVPC(PRIVATE)	# Put old EPC in private area

	sltiu	t1, RSYS, NCALLS	# Set t1 if syscall # < NCALLS 	
	bnez	t1, 1f			# Make this process "fail" if
					# it tries to make a bogus syscall
	nop				# (BD)

	FAIL(NIBFAIL_BADSCL)

1:
	la 	t0, sysvect 		# Load table address (in delay slot)
	sll	t1, RSYS, 2		# Get offset into table. (4*CALL#)

	add 	t0, t0, t1		# Add in offset
	lw 	t0, 0(t0)		# Get address
	nop
	j	t0
	nop

	# System Call Vector Table
sysvect:
	.word	nsys_fail	# Fail
	.word	nsys_pass	# Pass
	.word	nsys_rest	# Rest
	.word	nsys_gtime	# Get time
	.word	nsys_gquant	# Get current quantum
	.word	nsys_squant	# Set current quantum
	.word	nsys_ordain	# Ordain (go into kernel mode)
	.word	nsys_defrock	# Defrock (back to user mode)
	.word	nsys_msg	# Message
	.word	nsys_gnproc	# Get the number of processes alive
	.word	nsys_mark	# Just return
	.word	nsys_invalid	# Invalidate a TLB entry
	.word	nsys_scoher	# Set coherency for a page
	.word	nsys_gpid	# Get process ID
	.word	nsys_gcount	# Get the approximate instruction count
	.word	nsys_printhex	# Print the value of r2 in hex.
	.word	nsys_gshared	# Get the address of the shared memory space
	.word	nsys_steale	# Steal an exception vector
	.word	nsys_steali	# Steal an interrupt vector.
	.word   nsys_suspnd	# Suspend scheduling
	.word	nsys_resume	# Resume scheduling
	.word	nsys_wpte	# Write a new page table entry.
	.word	nsys_gecount	# Get exception count
	.word	nsys_cecount	# Clear exception count
	.word	nsys_vpass	# Verbose version of nsys_pass
	.word	nsys_csout	# Set up user context switch out routine
	.word	nsys_csin	# Set up user context switch in routine
	.word	nsys_mode2	# Switch to MIPS II mode
	.word	nsys_mode3	# Switch to MIPS III mode
	.word	nsys_vtop	# Return phys addr for vaddr
	.word 	0

	.globl	nib_timeout
nib_timeout:
	li	t0, MIND2OFF(M_TIMEOUT)
	la	t1, msgs
	add	t0, t1, t0
	STRING_DUMP(t0)
	FAIL(NIBFAIL_TOUT)
	nop

nsys_fail:
	li	t0, MIND2OFF(M_FAILED)
	la	t1, msgs
	add	t0, t1, t0
	STRING_DUMP(t0)

	lw	t0, U_PID(CURPROC)
	PRINT_HEX(t0)

	li	t0, MIND2OFF(M_FAILEDREGS)
	add	t0, t1, t0

	mfc0	MVPC, C0_EPC

	STRING_DUMP(t0)

	ld	t0, U_R1(CURPROC)
	PRINT_HEX(t0)
	PRINT_HEX(r2)
	PRINT_HEX(r3)
	PRINT_HEX(r4)
	PRINT_HEX(r5)
	PRINT_HEX(r6)
	PRINT_HEX(r7)
	ld	t0, U_R8(CURPROC)
	PRINT_HEX(t0)
	ld	t0, U_R9(CURPROC)
	PRINT_HEX(t0)
	ld	t0, U_R10(CURPROC)
	PRINT_HEX(t0)
	ld	t0, U_R11(CURPROC)
	PRINT_HEX(t0)
	ld	t0, U_R12(CURPROC)
	PRINT_HEX(t0)
	ld	t0, U_R13(CURPROC)
	PRINT_HEX(t0)
	ld	t0, U_R14(CURPROC)
	PRINT_HEX(t0)
	ld	t0, U_R15(CURPROC)
	PRINT_HEX(t0)
	PRINT_HEX(r16)
	PRINT_HEX(r17)
	PRINT_HEX(r18)
	PRINT_HEX(r19)
	PRINT_HEX(r20)
	PRINT_HEX(r21)
	PRINT_HEX(r22)
	PRINT_HEX(r23)
	ld	t0, U_R24(CURPROC)
	PRINT_HEX(t0)
	ld	t0, U_R25(CURPROC)
	PRINT_HEX(t0)
	PRINT_HEX(r26)
	PRINT_HEX(r27)
	PRINT_HEX(r28)
	PRINT_HEX(r29)
	PRINT_HEX(r30)
	ld	t0, U_R31(CURPROC)
	PRINT_HEX(t0)
	PRINT_HEX(MVPC)
	
	FAIL(NIBFAIL_PROC)
	nop

nsys_pass:
	la	t1, msgs

	li	t0, MIND2OFF(M_PASSED)
	add	t0, t1, t0
	STRING_DUMP(t0)

	li	t0, MIND2OFF(M_PROCNUM)
	add	t0, t1, t0
	STRING_DUMP(t0)

	lw	t0, U_PID(CURPROC)
	PRINT_HEX(t0)

	j continue_passing
	nop

nsys_vpass:
	la	t1, msgs

	li	t0, MIND2OFF(M_PASSED)
	add	t0, t1, t0
	STRING_DUMP(t0)

	li	t0, MIND2OFF(M_PROCNUM)
	add	t0, t1, t0
	STRING_DUMP(t0)

	lw	t0, U_PID(CURPROC)
	PRINT_HEX(t0)

	li	t0, MIND2OFF(M_REGDUMP)
	add	t0, t1, t0
	STRING_DUMP(t0)

	mfc0	MVPC, C0_EPC

	PRINT_HEX(r1)
	PRINT_HEX(r2)
	PRINT_HEX(r3)
	PRINT_HEX(r4)
	PRINT_HEX(r5)
	PRINT_HEX(r6)
	PRINT_HEX(r7)
	PRINT_HEX(r30)
	move	t0, r31
	PRINT_HEX(t0)
	PRINT_HEX(MVPC)

continue_passing:

lock1:
#ifdef LOCKS
RUNLOCK
#endif
	la	t1, numprocs
	lw	t2, 0(t1)		# Get number of current processes	
	li	t0, 1
	subu	t2, t2, t0
	bne	t2, r0, dequeue
	sw	t2, 0(t1)

#ifdef LOCKCHECK
	la	t0, rlockcheck		# Get the lock check address
	sw	r0, 0(t0)		# Put a zero in lockcheck.
#endif /* LOCKCHECK */

unlock1:
#ifdef LOCKS
UNRUNLOCK
#endif
about_to_exit:
	PASS
	nop

	.globl dequeue
dequeue:
	lw	MVPC, P_MVPC(PRIVATE)
	SAVE_REST(CURPROC)
	sw	MVPC, U_PC(CURPROC)

	la	t0, rlockcheck	# Get the lock check address
	lw	t1, 0(t0)	# Get the value.
	la	t2, runqueue	# Get addr of run queue pointer (delay slot)
#ifdef LOCKCHECK
	bne	t1, r0, mpfail0	# If it's not zero, locking has failed!
#endif
	lw	t2, 0(t2)	# Load run queue pointer (delay slot)

	lw	t1, P_CPUNUM(PRIVATE)	# Get cpunum
	ori	t1, 0x0100		# Or in this point's id
	sw	t1, 0(t0)		# Put a nonzero number in rlockcheck.

	# Pseudocode in comments:
	# if (RUNQ != NULL)
	#	runnewproc()
	bne	t2, r0, runnewproc
	# else {
	# 	if (N_PROCS == NULL)
	nop
	la	t0, numprocs
	lw	t2, 0(t0)	# Load the process count
	nop			# (LD)
	bne	t2, r0, noprocesses
	nop			# (BD)
	#		PASS

	la	t1, msgs
	li	t0, MIND2OFF(M_STESTPASSED)
	add	t0, t1
	STRING_DUMP(t0)

	nop
	PASS
	nop
mpfail0:
	PRINT_HEX(t1)		# Why are we doing this?
	UNRUNLOCK
	FAIL(NIBFAIL_COHER)
	nop

noprocesses:
	# If there are no processes on the run queue, disable scheduling,
	#	enter kernel mode with EXL low, and handle only interrupts.
	# 	wait for other CPU to finish.

	#		unlock the run queue
#ifdef LOCKCHECK
	la	t0, rlockcheck		# Get the lock check address
	sw	zero, 0(t0)		# Put a zero in lockcheck.
#endif
unlock2:
#ifdef LOCKS
	UNRUNLOCK
#endif
	la	t1, msgs
	li	t0, MIND2OFF(M_NOPROCS)
	add	t0, t1, t0
	STRING_DUMP(t0)
#ifndef MP
	FAIL(NIBFAIL_COHER)
#else
	li 	t0, NIB_SR & ~SR_IBIT8	# Turn off count/compare ints
	mtc0	t0, C0_SR		# Load new SR
#endif

	li	t1, SR_IBIT3

	.globl	twiddle
twiddle:
	mfc0	t0, C0_CAUSE
	nop
	and	t0, t1			# t1 = SR_IBIT3
	bnez	t0, everest_zero
	nop
	j twiddle
	nop

	# t1 contains the interrupt level
	.globl	pass_exit
pass_exit:
	li	t0, EV_CIPL0
	sd	t1, 0(t0)

	la	t1, msgs
	li	t0, MIND2OFF(M_PASSINT)
	add	t0, t1, t0
	STRING_DUMP(t0)

	PASSCLEANUP

	.globl	fail_exit
fail_exit:
	li	t0, EV_CIPL0
	sd	t1, 0(t0)

	la	t1, msgs
	li	t0, MIND2OFF(M_FAILINT)
	add	t0, t1, t0
	STRING_DUMP(t0)

	FAILCLEANUP(NIBFAIL_GENERIC)

nsys_rest:
	j sched
	nop

nsys_gtime:
	j sys_return
	nop

nsys_gquant:
	lw	RPARM, U_QUANT(CURPROC)
	j sys_return
	nop

nsys_squant:
	sw	RPARM, U_QUANT(CURPROC)
	j sys_return
	nop

nsys_ordain:
	mfc0	t0, C0_SR		# Get status register value
	li	t1, ~SR_KSUMASK	# Mask off kernel, supervsr, user bits
	and 	t0, t1, t0	# Zero 'em
	mtc0	t0, C0_SR		# Store it.
	nop
	nop
	nop
	nop
	j sys_return
	nop

#ifdef FIXED 

cs_out:
	lw	t1, U_OUTENHI(CURPROC) 	# Get ENHI for this vector
#ifdef CSDEBUG
	li	t0, 0xc505ced
	PRINT_HEX(t0)
	PRINT_HEX(k0)
	PRINT_HEX(t1)
	PRINT_HEX(CURPROC)
#endif	
	la	R_VECT,	sys_save
	j	user_cs
	nop				# la takes two instructions..  doesn't
					#	fit in delay slot

cs_in:
	lw	t1, U_INENHI(CURPROC) 	# Get ENHI for this vector
	
	/* Steve- deal with this a bit later. */
	la	R_VECT,	state_restored
	li	t0, 0xc515ced

#ifdef CSDEBUG
	PRINT_HEX(t0)
	PRINT_HEX(k0)
	PRINT_HEX(t1)
	PRINT_HEX(CURPROC)
#endif
user_cs:
	# Make sure this sucker's in the TLB.
	# ############################################################
	# k0 contains the user routine virtual address.

/* PID print */
	PRINT_HEX(t1)

#ifdef WIRED_ENTRIES
	mtc0	t1, C0_ENHI
	nop
	tlbp	
	nop
	mfc0	t0, C0_INX			# Get index register
	nop
	bgez	t0, csintlb		# If present, go on
	li	k1, PTEBASE			# Get page table base
	srl	t0, k0, PGSZ_BITS + 1	# Get VPN / 2
	sll	t0, t0, 4
	add	t0, t0, k1		# Get PTE address
	lw      k1, 0(t0)			# Load first mapping
	lw      t0, 8(t0)             # load the second mapping
	nop

/* PID print */
	PRINT_HEX(t1)

	mtc0	t1, C0_ENHI
	mtc0    k1, C0_ENLO0
	mtc0    t0, C0_ENLO1
	nop
	tlbwr
	nop
	nop
	nop
	nop
csintlb:
#else
	jal	force_enhi_tlb
	nop
#endif
	# ###############################################################
	j	k0				# Jump to user handler.
	nop
	# Won't return here.

#endif /* FIXED */

nsys_printhex:
	lw	t0, U_PID(CURPROC)
	PRINT_HEX_NOCR(t0)
	PRINT_HEX(RPARM)
	j sys_return
	nop

nsys_defrock:
	mfc0	t0, C0_SR		# Get status register value
	li	t1, ~SR_KSUMASK	# Mask off kernel, supervsr, user bits
	and 	t0, t1, t0	# Zero 'em
	li	t1, SR_USER	# Load user bits
	or	t0, t1, t0	# Or in user mode
	mtc0	t0, C0_SR		# Store it.
	nop
	j sys_return
	nop

nsys_msg:
	lw	t0, U_PID(CURPROC)
	PRINT_HEX_NOCR(t0)
	sll	RPARM, RPARM, MSG_BITS	# Convert message # to offset
	la	t0, msgs		# Get base of message strings area
	addu	t0, RPARM, t0	# Address of message
	STRING_DUMP(t0)		# Print the string
	j sys_return
	nop

nsys_gnproc:
lock3:
#ifdef LOCKS
	RUNLOCK
#endif
	la	t1, numprocs
	lw	t2, 0(t1)
unlock3:
#ifdef LOCKS
	UNRUNLOCK
#else
	nop
#endif
	move	RPARM, t2		# Get the number of currently active
					# processes
	j sys_return
	nop

nsys_invalid:
	mtc0    r0, C0_ENLO0
	mtc0    r0, C0_ENLO1

	mtc0    r0, C0_ENHI
	nop
	tlbwr
	nop
	lw	t2, U_PID(CURPROC)
	mtc0    t2, C0_ENHI

	nop
	j sys_return
	nop

nsys_scoher:
	# RPARM contains the address, RRET contains the new coherency bits
	srl	RPARM, RPARM, PGSZ_BITS	# Shift address right PGSZ bits.
	lw	t1, U_PGTBL(CURPROC)	# Get page table address
	sll	RPARM, RPARM, 3		# Size of pg table entries (8 bytes)
	add	t1, RPARM, t1		# Get PTE address
	lw	t0, 0(t1)		# Get PTE
	li	MVPC, ~(7 << TLBLO_CSHIFT)	# Load cache bit mask
	and	t0, MVPC, t0			# Mask cache bits off
	sll	RRET, RRET, TLBLO_CSHIFT	# Shift bits left 3
	or	t0, RRET, t0			# Or in new cache bits
	sw	t0, 0(t1)			# Write it back
	srl	t0, RPARM, 1			# Compute VPN / 2
	srl	t0, t0, TLBHI_VPNSHIFT
	lw	t1, U_PID(CURPROC)		# GET PID
	or	t0, t1, t0			# Or in ASID

	mtc0	t0, C0_ENHI			# Put 'er in ENHI

	nop
	tlbp
	nop
	lw	t2, U_PID(CURPROC)
	mfc0	t0, t2		 		# Get index register
	nop
	bltz	t0, noinval
	nop

	mtc0	zero, C0_ENHI
	mtc0	zero, C0_ENLO0
	mtc0	zero, C0_ENLO1
	nop
	tlbwi
	nop
	mtc0	t2, C0_ENHI
	nop
	nop
	nop
	nop
noinval:
	j sys_return
	nop
	# Might want to mess with cache here.
nsys_gpid:
	lw	RPARM, U_PID(CURPROC)
	j sys_return
	nop

nsys_gcount:
	lw	t2, P_ICNT(PRIVATE)
	or	RPARM, zero, t2		# Copy I-count to r2
	j sys_return
	nop

nsys_gshared:
	lw	RPARM, U_SHARED(CURPROC)
	j sys_return
	nop

nsys_vtop:
	srl	t0, RPARM, PGSZ_BITS		# Get VPN
	sll	t0, t0, 3		# Get PTE offset
	li	t1, PTEBASE		# Load page table base
	add	t1, t0, t1	# Add offset to PTEBASE
	lw	t0, 0(t1)		# Get PTE
	nop
	srl	t0, t0, PFNSHIFT	# Get PFN
	sll	t0, t0, PGSZ_BITS	# Physical addr for page base
	andi	t1, RPARM, OFFSET_MASK	# Get offset into page
	add	RPARM, t0, t1		# Physical address
	j	sys_return
	nop

nsys_wpte:
	sll 	t0, RPARM, 3			# Get PTE offset (VPN * 8)
	li 	t1, PTEBASE		# Load page table base
	add 	t1, t0, t1	# Add offset to PTEBASE
	sw 	RRET, 0(t1)
	j 	sys_return
	nop
nsys_suspnd:
	mfc0	t0, C0_SR			# Get current status register value
	li	t1, ~SR_IBIT8		# Turn the COUNT/COMPARE timer bit off.
	and	t0, t1, t0	# Mask off Interrupt mask bit 8
	mtc0	t0, C0_SR			# store it in the status register
	nop
	nop
	nop
	nop
	j	sys_return
	nop
nsys_resume:
	mfc0	t0, C0_SR
	li	t1, SR_IBIT8		# Timer interrupt bit
	or	t0, t1			# Turn interrupt mask bit 8 on
	mtc0	t0, C0_SR		# store it in the status reg.
	nop
	nop
	nop
	nop
	j	sys_return
	nop

nsys_gecount:
	sll	RPARM, RPARM, 2		# Get word offset from exception number
	add	t0, RPARM, CURPROC	# Get address to add EXCCNTBASE to
	lw	RPARM, U_EXCCNTBASE(t0)	# get current count
	j	sys_return
	nop

nsys_cecount:
	sll	RPARM, RPARM, 2		# Get word offset from exception number
	add	t0, RPARM, CURPROC	# Get address to add EXCCNTBASE to
	sw	zero, U_EXCCNTBASE(t0)	# clear current count
	j	sys_return
	nop

nsys_steale:
	sll	RPARM, RPARM, 2		# Get word offset from exception number
	add	t0, RPARM, CURPROC	# Get address to add to EXCTBLBASE
	sw	RRET, U_EXCTBLBASE(t0)	# Store new vector
	srl	t1, RRET, PGSZ_BITS + 1	# Get the page number / 2
	sll	t1, t1, 13		# shift into place for ENHI
	lw	t2, U_PID(CURPROC)
	or	t1, t1, t2		# or in ASID
	sw	t1, U_EXCENHIBASE(t0)	# Store it away for comparison
	j	sys_return
	nop

nsys_steali:
	sll	RPARM, RPARM, 2		# Get word offset from interrupt number
	la	t0, intrvect		# Get interrupt data area base
	add	t0, RPARM, t0		# Get address to add I_VECTOR to
	sw	RRET, I_VECTOR(t0)	# Store new vector
	lw	t2, U_PID(CURPROC)
	sw	t2, I_ASID(t0)		# Store current process's ASID
	srl	t1, RRET, PGSZ_BITS + 1	# Get the page number / 2
	sll	t1, t1, 13		# shift into place for ENHI
	or	t1, t2			# or in ASID

	sw	t1, I_ENHI(t0)		# Store it away for comparison
	j	sys_return
	nop

nsys_csout:
	sw	RPARM, U_CSOUT(CURPROC)		# Store new vector
	srl	t1, RPARM, PGSZ_BITS + 1	# Get the page number / 2
	sll	t1, t1, 13			# shift into place for ENHI
	lw	t2, U_PID(CURPROC)
	or	t1, t1, t2			# or in ASID
	sw	t1, U_OUTENHI(CURPROC)  	# Store it away for comparison
#ifdef CSDEBUG
	li	t0, 0x0c50
	PRINT_HEX(t0)
	PRINT_HEX(r2)
	PRINT_HEX(t1)
	PRINT_HEX(CURPROC)
#endif
	j	sys_return
	nop

nsys_csin:
	sw	RPARM, U_CSIN(CURPROC)		# Store new vector
	srl	t1, RPARM, PGSZ_BITS + 1	# Get the page number / 2
	sll	t1, t1, 13			# shift into place for ENHI
	lw	t2, U_PID(CURPROC)
	or	t1, t2				# or in ASID
	sw	t1, U_INENHI(CURPROC)  		# Store it away for comparison
#ifdef CSDEBUG
	li	t0, 0x0c51
	PRINT_HEX(t0)
	PRINT_HEX(RPARM)
	PRINT_HEX(t1)
	PRINT_HEX(CURPROC)
#endif
	j	sys_return
	nop

nsys_mode2:
	mfc0	t0, C0_SR
	li	t1, ~SR_UX
	and	t0, t0, t1
	mtc0	t0, C0_SR
	# Clear MIPS3 flag for user code
	j sys_return
	nop

nsys_mode3:
	mfc0	t0, C0_SR
	li	t1, SR_UX
	or	t0, t0, t1
	mtc0	t0, C0_SR
	# Set MIPS3 flag for user code
	j sys_return
	nop

	# Don't put any routines between nsys_mark and sys_return
nsys_mark:
	nop	# Need to tell dif. between mark and sys_return
sys_return:
	ERET
	nop
	
tlbl:
tlbs:
	mfc0	k0, C0_BADVADDR

	# Check the BAD_VADDR.  If it ain't above ff800000, 
	#	either "page in" or segv!
	li	t0, PTEBASE
	sltu	t1, k0, t0	# Is BADVADDR < PTEBASE?
	bne	t1, r0, badaddr

	li	t0, PGSZ_MASK 	# Mask for 4k pages
	mtc0	t0, C0_PAGEMASK	# Load the page mask

#if 0
	# Why were we changing the EPC anyway?
	mtc0	k1, C0_EPC
#endif 

	li	t1, 0x1fffffff

	lw	t2, U_PGTBL(CURPROC)
	or	t0, t2, zero
	and	t0, t0, t1
	srl	t0, t0, (PGSZ_BITS)
	or	t1, r0, t0
	srl	t1, t1, 1	# Divide by 2
	sll	t1, t1, 13	# Shift into place

	sll	t0, t0, 6
	li	t1, ((5 << 3) | TLBLO_VMASK | TLBLO_DMASK);
					# Set up a TLB Entry for
					# a valid page at ff800000,
					# cacheable, exclusive on write,
					# dirty
	or	t0, t0, t1	
	mtc0	t0, C0_ENLO0		# Move it to entry low 0

	addi	t1, t0, (1 << 6)	# Put PFN 1 into ENLO1
	mtc0	t1, C0_ENLO1		# Validate the paired page

	li	t0, PTEBASE		# Load the vaddr of the PTE Base
	lw	t2, U_PID(CURPROC)
	or	t0, t2			# Load the ASID 
	mtc0	t0, C0_ENHI		# Put 'er in the TLB Hi register
#ifdef WIRED_ENTRIES
	mtc0	t2, C0_INX		# Put our ASID in the index
	nop				# Need nop between set-up and write
	tlbwi
#else
	tlbwr
#endif
	ERET
	nop
badaddr:
	# Bad virtual address
	# k0 contains the bad virtual address.
	srl	t0, k0, PGSZ_BITS		# Get the page number
	li	t1, PTEBASE			# Get the page table base
	sll	t0, t0, 3			# Each PTE is two words
	add	t1, t1, t0			# PTE Addr
	lw	k0, 0(t1)			# Get the PTE and put it in k1
	nop
	and	t1, k0, TLBLO_VMASK		# Is it valid?
	beq	t1, r0, invalidpage
	sll	t0, t0, 9
	lw	t1, U_PID(CURPROC)		# Get our PID (ASID)
	or	t0, t0, t1

	mtc0	t0, C0_ENHI
	nop

	tlbp
	nop
	mfc0	t0, C0_INX
	nop
	bltz	t0, clobbered 	# If not found (high bit set) branch
	nop

	mfc0    k0, C0_CTXT
	nop
	lw	t0, 0(k0)
	lw      k0, 8(k0)               
	nop
	mtc0    t0, C0_ENLO0
	mtc0    k0, C0_ENLO1
	nop
	tlbwi
	nop
	j 	sys_return
	nop
clobbered:
	PRINT_HEX(t0)

	mfc0    k0, C0_CTXT
	nop
	lw	t0, 0(k0)
	lw      k0, 8(k0)               
	nop
	mtc0    t0, C0_ENLO0
	mtc0    k0, C0_ENLO1
	nop
	tlbwr
	nop
	j 	sys_return
	nop

invalidpage:
	la	t1, msgs
	li	t0, MIND2OFF(M_BADVADDR)
	add	t0, t1, t0

	mfc0	MVPC, C0_EPC

	STRING_DUMP(t0)
	mfc0 t0, C0_BADVADDR
	nop
	PRINT_HEX(t0)
	PRINT_HEX(MVPC)
	lw      t1, U_PID(CURPROC)	# Print out the PID of the process
					# that generated the bad page reference
	PRINT_HEX(t1)

	FAIL(NIBFAIL_BADPAGE)
	nop

notexc:
	/* SET_LEDS(12) */

	mfc0	k1, C0_CAUSE		# Get cause register value
	nop
	mfc0	t0, C0_SR		# Get the status register	
	li	k0, CAUSE_IP8		# Timer interrupt
	and	k1, t0, k1		# Mask off disabled interrupts
	and	t0, k1, k0		# Select timer interrupt bit
	beq	t0, r0, notsched	# Compare against timer interrupt
	nop

	# Jump away to notsched to increase locality for sched.
sched:
#ifdef FIXED
	lw	k0, U_CSOUT(CURPROC)
	nop
	bne	k0, r0, cs_out
	nop
#endif /* FIXED */
sys_save:
	lw	MVPC, P_MVPC(PRIVATE)
	SAVE_REST(CURPROC)
	sw	MVPC, U_PC(CURPROC)

schedrunlock:
lock4:
#ifdef LOCKS
	RUNLOCK
#endif
	la	t0, rlockcheck		# Get the lock check address
	lw	t1, 0(t0)		# Get the value.
	la	t2, runqueue		# Get addr of run queue pointer (LD)
#ifdef LOCKCHECK
	bne	t1, zero, mpfail1	# If it's not zero, locking has failed!
	nop
#endif
	lw	t2, 0(t2)		# t2 = Run queue pointer (BD)
	lw	t1, 

	lw	t1, U_PID(CURPROC)		# Get our PID (ASID)
	ori	t1, 0x0200

	sw	t1, 0(t0)		# Put a nonzero # in rlockcheck.

	# Need to add code to handle no one wanting to run and no CURPROC.
	lw	t1, P_SCHCNT(PRIVATE)
	nop
	addi 	t1, 1			# Add one to the scheduling count
	sw	t1, P_SCHCNT(PRIVATE)
	bne 	t2, r0, traverse	# If anyone wants to run, traverse.
	move 	t0, t2 			# Copy RUNQ to scratch ptr. (BD)

	# Release the lock and continue.
#ifdef LOCKCHECK
	la	t1, rlockcheck
	sw	r0, 0(t1)		# Zero lock check.
#endif /* LOCKCHECK */
unlock4:
#ifdef LOCKS
	UNRUNLOCK
#endif
	
	j 	goon			# Go on.


traverse:				# Traverse the RUNQ
	or	t1, t0, zero		# Save proc ptr.
	lw 	t0, U_NEXT(t0) 		# Get ptr to next proc in queue.
	nop				# Wait for NEXT to be available
	beq 	t0, zero, atend
	nop
	j traverse
	nop
atend:
	sw	CURPROC, U_NEXT(t1)
	sw	r0, U_NEXT(CURPROC)
runnewproc:
	la	t2, runqueue
	lw	CURPROC, 0(t2)		# old run queue => new curproc
	nop				# (BD)
	lw	MVPC, U_PC(CURPROC)	# Get PC of new process
	lw	t2, U_NEXT(CURPROC)	# Get new run queue address
	la	t1, runqueue		# Get run queue pointer address
	sw	t2, 0(t1)		# Store run queue pointer
	sw	CURPROC, P_CURPROC(PRIVATE)	# Store new CURPROC
	la	t1, rlockcheck		# Get lock check address
	sw	r0, 0(t1)		# Zero lock check.
unlock5:
#ifdef LOCKS
	UNRUNLOCK
#endif
	mtc0	MVPC, C0_EPC		# Load the EPC with the addr we want
	nop				# Possible bug fixer
sys_restore:
	RESTORE_REST(CURPROC)
	lw	t1, U_PID(CURPROC)
	mtc0	t1, C0_ENHI		# Load current process ID.
#ifdef FIXED
	lw	t1, U_CSIN(CURPROC)
	nop
	bne	t1, zero, cs_in
	nop
#endif
state_restored:	
goon:

#ifdef PRINTSCHED
	lw	t2, U_PID(CURPROC)
	lui	t1, 0x5ced
	or	t1, t1, t2
	PRINT_HEX(t1)			# Print PID with 0x5ced000 prefix.
#endif /* PRINTSCHED */

	mfc0	t1, C0_COUNT		# Get COUNT
	lw	t2, P_ICNT(PRIVATE)	# Get cycle count 
	addu	t2, t1, t2		# Add to current count
	sw	t2, P_ICNT(PRIVATE)	# Store the cycle count
	SET_LEDS_R(t2)

	lw	t0, U_QUANT(CURPROC)	# Get the process' time quantum
	mtc0	zero, C0_COUNT		# Zero the count register
	nop
	mtc0	t0, C0_COMPARE		# Set the compare register

	ERET				# Restore from exception in delay slot
	nop

notsched:
	# k0 contains CAUSE_IP8
	# k1 contains the contents of C0_CAUSE
	# This code assumes that CAUSE_IPMASK == SR_IMASK.

	mfc0	k0, C0_SR		# Get the status register	
	li	t1, SR_IMASK		# Load interrupt mask
	and	t0, t1, k0		# Interrupt mask only
	and	t0, k1, t0		# Interupts enabled _and_ pending

	li	k1, CAUSE_SW1		# Start here.
	and	t1, k1, t0		# Is interrupt 1 pending?
i_test:	
	bne	t1, zero, foundone	# If != 0, interrupt is active
	sll	k1, k1, 1		# Try the next interrupt
	bne	k1, k0, i_test		# If not up to 8, do another
	and	t1, k1, t0		# And active ints with current

itest_fail:
	# Shouldn't get here - means bad interrupt or timer...
	FAIL(NIBFAIL_TIMER)
		
foundone:
	srl	k1, k1, 1
	# k1 contains the CAUSE_IPx value shifted left by one.
	srl	k1, k1, (CAUSE_IPSHIFT - 1)	# Result will be the interrupt
						# number shifted left two.
	li	t1, CAUSE_IP3 >>(CAUSE_IPSHIFT - 1)
	beq	k1, t1, everest_zero		# If it's IP3, go to ev hndlr
	nop					# (BD)
#ifdef TIMEOUT
	li	t1, CAUSE_IP4 >> (CAUSE_IPSHIFT - 1)
	beq	k1, t1, nib_timeout		# If it's IP4, go to timeout
	nop
#endif
	/* SET_LEDS(14) */
keep_checking:
	la	t1, intrvect
	add	t0, k1, t1			# Address of handler
	lw	k0, I_VECTOR(t0)		# Get handler vector
	beq	k0, r0, intfail
	lw	t1, I_COUNT(t0)			# Get count

	mtc0	t2, C0_ENHI			# Install ASID
	addi	t1, t1, 1			# Increment count
	sw	t1, I_COUNT(t0)			# Store new count
	mfc0	MVPC, C0_EPC			# Get EPC
	# Make sure this sucker's in the TLB.
	# ############################################################
	# k0 contains the user routine virtual address.
	lw	t1, I_ENHI(t0)		# Get ENHI for this vector
	nop
#ifdef WIRED_ENTRIES
	mtc0	t1, C0_ENHI
	nop
	tlbp	
	nop
	mfc0	t0, C0_INX		# Get index register
	nop
	bgez	t0, intintlb		# If present, go on
	li	k1, PTEBASE		# Get page table base
	srl	t0, k0, PGSZ_BITS + 1	# Get VPN / 2
	sll	t0, t0, 4
	add	t0, t0, k1		# Get PTE address
	lw      k1, 0(t0)		# Load first mapping
	lw      t0, 8(t0)		# load the second mapping
	nop
	mtc0	t1, C0_ENHI
	mtc0    k1, C0_ENLO0
	mtc0    t0, C0_ENLO1
	nop
	tlbwr
	nop
intintlb:
#else
	jal	force_enhi_tlb
	nop
#endif
	# ###############################################################
	# STEVE - This won't work.
	la	R_VECT, int_return
	j	k0				# Jump to user handler.
	nop
int_return:
	lw	t1, U_PID(CURPROC)		# Load old ASID back in
	mtc0	MVPC, C0_EPC			# Restore EPC

	mtc0	t1, C0_ENHI			# Install ASID
	ERET

everest_zero:
	/* SET_LEDS(13) */

	li	t0, EV_HPIL		# Go fetch the HIPL.
	ld	t0, 0(t0)
	
	li	t1, PASS_LEVEL
	beq	t1, t0, pass_exit	# Supertest passed!
	nop

	li	t1, FAIL_LEVEL
	beq	t1, t0, fail_exit	# Supertest failed :-(
	nop

	b	keep_checking
	nop

memerr:
	li	k0, MIND2OFF(M_BADMEM)
	la	t1, msgs
	add	k0, t1, t0
	STRING_DUMP(t0)
	PRINT_HEX(CURPROC)
	PRINT_HEX(t0)
	FAIL(NIBFAIL_MEM)
	nop

intfail:
	li	t0, MIND2OFF(M_BADINT)
	la	t1, msgs
	add	t0, t1, t0
	STRING_DUMP(t0)
	mfc0	t0, C0_CAUSE
	nop
	PRINT_HEX(t0)
	FAIL(NIBFAIL_BADINT)
	nop

mpfail1:
	PRINT_HEX(t1)
	UNRUNLOCK
	FAIL(NIBFAIL_COHER)
	nop
badexc:
	FAIL(NIBFAIL_BADEXC)
	nop
	END(nib_gen_exc)

	.align 3		/* Doubleword align. */
	.globl nib_text_entry	/* A required label for nextract */
nib_text_entry:
	/* was ENTRY */
LEAF(niblet)

	BREAK(1)
/* Set up the kernel structures, timer interrupt, timer, compare register,
		possibly page tables and TLB */

	# Get our CPU number.
	GETCPUNUM(t2)

	la	CURPROC, nib_exc_start
	lw	CURPROC, 0(CURPROC)

	la	a0, nib_exc_start
	lw	a0, 8(a0)

	la	a1, sync_addr

	# Tell dcob we've reached the barrier
	li	k0, EV_SPNUM
	ld	k0, 0(k0)		# Get slot/processor number
	la	k1, directory		# Addresses of private data areas (LD)
	andi	k0, EV_SPNUM_MASK	# Mask off extraneous stuff
	sll	k0, DIR_SHIFT		# Shift by log2(dir ent size)
	add	k1, k0			# k1 = address of our pointers
	li	k0, 1
	sw	k0, DIR_BAROFF(k1)	# Write the marker.

	# Synchronize a0 processors with a barrier lock located at a1
	TIMED_BARRIER(a1, a0, BARRIER_TIMEOUT)

	beqz	t1, 1f
	nop				# (BD)
	
	# If we get here, the barrier timed out.
	# Tell dcob where the directory is.
	# It's okay for all non-hanging CPUs to do this.

	la	t0, nib_exc_start
	la	t1, directory
	sw	t1, STUP_DIRECTORY(t0)	

	# Return to dcob.
	j	ra
	ori	v0, zero, NIBFAIL_BARRIER	# (BD)

1:
	mfc0	MVPC, C0_SR		# Stash away the SR
	nop
	nop
	nop
	nop
	li	t0, NIB_SR & ~(SR_USER)
	mtc0	t0, C0_SR	# Initialize SR

	li	t0, PTEBASE	# Load PTEBASE into Context register
	mtc0	t0, C0_CTXT	
	nop

	BREAK(2)

#ifdef CLEARTLB
	li	t1, NTLBENTRIES - 1
	li	t0, 0x80000000

	mtc0	t0, C0_ENHI
	mtc0	zero, C0_ENLO0
	mtc0	zero, C0_ENLO0
	mtc0	zero, C0_ENLO1
tlbloop:
	mtc0	t1, C0_INX		# Change index register
	nop
	nop
	nop
	nop
	tlbwi
	nop; nop; nop; nop

	bnez	t1, tlbloop	
	addi	t1, -1			# (BD)
#endif /* CLEARTLB */

	BREAK(3)

	# Clear level 1, 2, 4 interrupts
	li	t0, EV_CIPL124
	li	t1, 0xb			# 1 | 2 | 4
	sd	t1, 0(t0)

        li      t0, EV_CIPL0
        li      t1, EV_CELMAX		# Highest interrupt number

clear_ints:
        sd      t1, 0(t0)		# Clear interrupt (BD)
        bne     t1, zero, clear_ints
        addi    t1, -1			# Decrement counter (BD)

	BREAK(8)

	li	t0, EV_ILE		# Interrupt Level Enable register
	li	t1, 1 | 8
	sd	t1, 0(t0)		# Enable Everest level 0 interrupts

	BREAK(7)

lock6:
#ifdef WIRED_ENTRIES
	li	t0, WIRED_TLB
#else
	li	t0, 0
#endif /* WIRED_ENTRIES */

	mtc0	t0, C0_WIRED	# Set up wired TLB entries for us.
	nop
	li	t0, PGSZ_MASK	# Page size determined in niblet.h
	mtc0	t0, C0_PAGEMASK		# Set TLB for 4k pages
	nop
#if 0
	li	t0, CONFIG_BITS # Algorithm specified in niblet.h
	mtc0	t0, C0_CONFIG   # Set the config coherency algorithm
	nop
	nop
#endif /* 0 */

#ifdef MP
tryilock:
	# Try to get the "initlock"
	la	t0, initlock
	ll	t1, 0(t0)		# Load init lock value
	bne	t1, r0, missed		# If it's not zero, we missed it.
	addi	t1, t1, 1		# Add one to the value (0)
	sc	t1, 0(t0)		# Store the new value.
	beq	t1, r0, tryilock	# If there was contention, try again.
	nop	
#endif	/* MP */

	# *** We got the initlock ****

	BREAK(6)

	# Print master CPU message

	la	CURPROC, nib_exc_start
	lw	CURPROC, 0(CURPROC)

	la	PRIVATE, private	# Get address of first private data area
					# Doubleword align it
	addi	PRIVATE, 4		# Add four before masking off bits
	li	t0, ~0x7
	and	PRIVATE, t0		# Turn off bottom three bits
	move	t0, PRIVATE		# Copy private address.
	addi	t0, PRIV_SIZE		# Get address of next private section
	la	t1, next_private
	sw	t0, 0(t1)		# Store address of next private section

	li	k0, EV_SPNUM
	ld	k0, 0(k0)		# Get slot/processor number
	la	k1, directory		# Addresses of private data areas (LD)
	andi	k0, EV_SPNUM_MASK	# Mask off extraneous stuff
	sll	k0, DIR_SHIFT		# log2(dir ent size)
	add	k1, k0			# k1 = address of our pointers
	sw	PRIVATE, DIR_PRIVOFF(k1)	# Store our private area ptr
	nop

	sd	ra, P_RA(PRIVATE)	# Save return address

	sd	sp, P_SP(PRIVATE)	# Save stack pointer

	sw	MVPC, P_SR(PRIVATE)	# Save status register!

	sw	zero, P_SCHCNT(PRIVATE)	# Reset sched count

	BREAK(7)
	
	li	t0, MIND2OFF(M_MASTER)
	la	t1, msgs
	add	t0, t1, t0

	STRING_DUMP(t0)			# Print Master CPU message

	lw	t0, P_CPUNUM(PRIVATE)
	PRINT_HEX(t0)			# Print CPU Number
	PRINT_HEX(CURPROC)		# Print process structure address
	PRINT_HEX(PRIVATE)		# Print private storage area address

	PRINT_TAGS(0x80000000)

	lw	t0, U_MAGIC1(CURPROC)	# Load magic num 1 from data
	li	t1, MAGIC1		# Load correct magic number 1
	bne	t0, t1, badendian	# Wrong endianess	
	lw	t0, U_MAGIC2(CURPROC)	# Load magic num 2 from data
	li	t1, MAGIC2		# Load correct magic number 2
	bne	t0, t1, badendian2	# Wrong endianness
	nop				# Weird stuff was happening to this
	lw	t2, U_NEXT(CURPROC)	# Load the second process (if present)
					# t2 = Run queue pointer

	# Make sure that we don't jump to kernel space 
	lw	MVPC, U_PC(CURPROC)
	li	t1, 0x1fffffff
	and	MVPC, MVPC, t1
	sw	MVPC, U_PC(CURPROC)
	
	# Store current status word into status slot in proc table
	li 	t0, NIB_SR		# User mode on ERET!

	sw 	t0, U_STAT(CURPROC)
	li	t3, 1			# Store 1 in number of processes

	beq	t2, r0, done		# Only one process
	move	t1, t2			# Copy runq to scratch in delay slot

store:
	# Make sure that we don't jump to kernel space 
	lw	MVPC, U_PC(t1)
	li	r2, 0x1fffffff
	and	MVPC, MVPC, r2
	sw	MVPC, U_PC(t1)

	sw 	t0, U_STAT(t1)	# Put stat in proc tbl entry
	lw	t1, U_NEXT(t1)	# Get next entry
	nop
	
	bne	t1, r0, store 
	addi	t3, 1

done:
	BREAK(8)

	li	t0, MIND2OFF(M_NUMPROCS)
	la	t1, msgs
	add	t0, t1, t0
	STRING_DUMP(t0)

	PRINT_HEX(t3)

	la	t0, numprocs	# Get the address of the process count
	sw	t3, 0(t0)	# Store the process count
	la	t0, runqueue	# Get address of run queue pointer
	sw	t2, 0(t0)	# Store run queue pointer

	mfc0	t0, C0_COUNT		# Get instruction count
	nop
	sw	t0, P_ICNT(PRIVATE)	# Store it.

	BREAK(9)

	lw	MVPC, U_PC(CURPROC)	# PC for first process
	lw	t0, U_QUANT(CURPROC)	# Get process's time quantum
	PRINT_HEX(MVPC)			# Print new EPC
	mtc0	MVPC, C0_EPC		# Set EPC
	nop
	mtc0 	r0, C0_COUNT		# Zero the count register
	nop
	mtc0 	t0, C0_COMPARE	# Set the compare register

	lw	t1, U_PID(CURPROC)
	nop
	mtc0	t1, C0_ENHI	# Load current process ID.
	nop

#ifdef MP
	li	t0, 1		
	la	t1, goahead	# Get address of goahead flag
	sw	t0, 0(t1)	# Store 1 in goahead.
#endif	/* MP */

	BREAK(10)

	# No one can have set the XA (extended addressing) bit by now so...

	sw	CURPROC, P_CURPROC(PRIVATE)	# Store CURPROC
	RESTORE_REST(CURPROC)		# Restore CPU state from CURPROC 

	BREAK(11)

	ERET
	nop
badendian:
	FAIL(NIBFAIL_ENDIAN)
	nop
badendian2:	
	FAIL(NIBFAIL_ENDIAN)
	nop
	.globl	nib_slave_entry
nib_slave_entry:
#ifdef MP
#ifdef LOCKS
missed:		# Code to execute if you're not the master processor
	la	t1, goahead
waitglock:
	lw	t0, 0(t1)
	nop
	beq	t0, zero, waitglock
	nop
	# Get run queue lock
	
startloop:
	RUNLOCK
	la	t0, cpunum
#if 0
	lw	t3, 0(t0)		# t3 = CPUNUM
	nop
	addi	t3, 4			# CPUNUMs start at 0 and go by four
	sw	t3, 0(t0)
#endif /* 0 */
	la	t1, next_private	# Get addr of private section ptr.
	lw	PRIVATE, 0(t1)		# Get addr of next private data area
	nop
	sw	t3, P_CPUNUM(PRIVATE)	# Store our CPUNUM
	move	t0, PRIVATE		# Copy private address.
	addi	t0, PRIV_SIZE		# Get address of next private section
	sw	t0, 0(t1)		# Store address of next private section
	sd	ra, P_RA(PRIVATE)	# Save our return address
	sd	sp, P_SP(PRIVATE)	# Save our stack pointer
	sd	MVPC, P_SR(PRIVATE)	# Stash away our SR

	li	k0, EV_SPNUM
	ld	k0, 0(k0)		# Get slot/processor number
	la	k1, directory		# Addresses of private data areas (LD)
	andi	k0, EV_SPNUM_MASK	# Mask off extraneous stuff
	sll	k0, DIR_SHIFT		# log2(dir ent size)
	add	k1, k0			# k1 = address of our pointers
	sw	PRIVATE, DIR_PRIVOFF(k1)	# Store our private area
	nop

	la	t0, numprocs

	PRINT_HEX(t0)

	la	t0, rlockcheck		# Get the lock check address
	lw	t1, 0(t0)		# Get the value.
	la	t2, runqueue		# Get addr of run queue pointer (BD)
#ifdef LOCKCHECK
	bne	t1, r0, mpfail2		# If it's not zero, locking has failed!
#endif
	lw	t2, 0(t2)		# Load run queue pointer (BD)

	lw	t1, P_CPUNUM(PRIVATE)	# Get CPUNUM
	ori	t1, 0x0300
	sw	t1, 0(t0)		# Put a nonzero #  in rlockcheck.

	# Print CPU running message
	li	t0, MIND2OFF(M_RUNNING)
	la	t1, msgs
	add	t0, t1, t0
	STRING_DUMP(t0)
	PRINT_HEX(t3)			# Print our CPU number

	beq	zero, t2, noprocesses	# If no procs on queue, twiddle
	nop
	j	runnewproc		# Put head of run queue in CURPROC
	nop

mpfail2:
	PRINT_HEX(t1)
	UNRUNLOCK
	FAIL(NIBFAIL_COHER)
	nop
#else /* LOCKS - If locks aren't used, only one processor can run */
missed:
	j	missed		# Loop forever
#endif /* LOCKS */
#endif /* MP */
	nop
	END(niblet)


/* Force a page into the TLB.  Takes an address as a parameter in t0.
 * Also takes an ASID in t1 - These will be clobbered.
 * The return address is in ra
 */
LEAF(force_addr_tlb)
	
	# Calculate an ENHI given the current ASID and an address.
	j	force_enhi_tlb
	nop

	END(force_addr_tlb)


/* Force a page into the TLB.  Takes the ENHI value as a parameter in t1.
 * k0 = the virtual address we're mapping address.
 * The return address is in ra
 */
LEAF(force_enhi_tlb)
	# Check to see if an ENHI is in the TLB.  If not, put it there.

	mtc0	t1, C0_ENHI
	nop
	tlbp	
	nop
	mfc0	t0, C0_INX			# Get index register
	nop
	bltz	t0, 1f				# If present, go on
	li	k1, PTEBASE			# (BD) Get page table base
	j	t9
	nop					# (BD)
1:
	srl	t0, k0, PGSZ_BITS + 1		# Get VPN / 2
	sll	t0, t0, 4
	add	t0, t0, k1			# Get PTE address
	lw      k1, 0(t0)			# Load first mapping
	lw      t0, 8(t0)             		# load the second mapping
	nop
	mtc0	t1, C0_ENHI
	mtc0    k1, C0_ENLO0
	mtc0    t0, C0_ENLO1
	nop
	tlbwr
	nop
	j	ra
	nop					# (BD)
	END(force_enhi_tlb)


LEAF(string_dump)
	sd	a1, P_SAVEAREA+8(PRIVATE)
	sd	a2, P_SAVEAREA+88(PRIVATE)
	sd	v0, P_SAVEAREA+16(PRIVATE)
	sd	v1, P_SAVEAREA+24(PRIVATE)
	sd	t5, P_SAVEAREA+32(PRIVATE)
	sd	t6, P_SAVEAREA+40(PRIVATE)
	sd	t7, P_SAVEAREA+48(PRIVATE)
	sd	t8, P_SAVEAREA+56(PRIVATE)
	sd	t9, P_SAVEAREA+64(PRIVATE)
	sd	ra, P_SAVEAREA+80(PRIVATE)
	sd	t0, P_SAVEAREA+96(PRIVATE)
	sd	t1, P_SAVEAREA+104(PRIVATE)
	sd	t2, P_SAVEAREA+112(PRIVATE)
	sd	t3, P_SAVEAREA+120(PRIVATE)
	jal	pul_cc_puts
	nop					# (BD)
	ld	a1, P_SAVEAREA+8(PRIVATE)
	ld	v0, P_SAVEAREA+16(PRIVATE)
	ld	v1, P_SAVEAREA+24(PRIVATE)
	ld	t5, P_SAVEAREA+32(PRIVATE)
	ld	t6, P_SAVEAREA+40(PRIVATE)
	ld	t7, P_SAVEAREA+48(PRIVATE)
	ld	ra, P_SAVEAREA+80(PRIVATE)
	ld	t8, P_SAVEAREA+56(PRIVATE)
	ld	t9, P_SAVEAREA+64(PRIVATE)
	ld	a2, P_SAVEAREA+88(PRIVATE)
	ld	t0, P_SAVEAREA+96(PRIVATE)
	ld	t1, P_SAVEAREA+104(PRIVATE)
	ld	t2, P_SAVEAREA+112(PRIVATE)
	ld	t3, P_SAVEAREA+120(PRIVATE)
	j	ra
	nop					# (BD)
	END(string_dump)


LEAF(print_hex)
	sd	a1, P_SAVEAREA+8(PRIVATE)
	sd	a2, P_SAVEAREA+88(PRIVATE)
	sd	v0, P_SAVEAREA+16(PRIVATE)
	sd	v1, P_SAVEAREA+24(PRIVATE)
	sd	t5, P_SAVEAREA+32(PRIVATE)
	sd	t6, P_SAVEAREA+40(PRIVATE)
	sd	t7, P_SAVEAREA+48(PRIVATE)
	sd	t8, P_SAVEAREA+56(PRIVATE)
	sd	ra, P_SAVEAREA+80(PRIVATE)
	sd	t9, P_SAVEAREA+64(PRIVATE)
	sd	t0, P_SAVEAREA+96(PRIVATE)
	sd	t1, P_SAVEAREA+104(PRIVATE)
	sd	t2, P_SAVEAREA+112(PRIVATE)
	sd	t3, P_SAVEAREA+120(PRIVATE)
	jal	pul_cc_puthex
	nop					# (BD)

	la	a0, crlf
	jal	pul_cc_puts
	nop					# (BD)
	ld	a1, P_SAVEAREA+8(PRIVATE)
	ld	v0, P_SAVEAREA+16(PRIVATE)
	ld	v1, P_SAVEAREA+24(PRIVATE)
	ld	t5, P_SAVEAREA+32(PRIVATE)
	ld	t6, P_SAVEAREA+40(PRIVATE)
	ld	t7, P_SAVEAREA+48(PRIVATE)
	ld	ra, P_SAVEAREA+80(PRIVATE)
	ld	t8, P_SAVEAREA+56(PRIVATE)
	ld	t9, P_SAVEAREA+64(PRIVATE)
	ld	a2, P_SAVEAREA+88(PRIVATE)
	ld	t0, P_SAVEAREA+96(PRIVATE)
	ld	t1, P_SAVEAREA+104(PRIVATE)
	ld	t2, P_SAVEAREA+112(PRIVATE)
	ld	t3, P_SAVEAREA+120(PRIVATE)
	j	ra
	nop					# (BD)
	END(print_hex)

LEAF(print_byte_nocr)
	sd	a1, P_SAVEAREA+8(PRIVATE)
	sd	a2, P_SAVEAREA+88(PRIVATE)
	sd	v0, P_SAVEAREA+16(PRIVATE)
	sd	v1, P_SAVEAREA+24(PRIVATE)
	sd	t5, P_SAVEAREA+32(PRIVATE)
	sd	t6, P_SAVEAREA+40(PRIVATE)
	sd	t7, P_SAVEAREA+48(PRIVATE)
	sd	t8, P_SAVEAREA+56(PRIVATE)
	sd	ra, P_SAVEAREA+80(PRIVATE)
	sd	t9, P_SAVEAREA+64(PRIVATE)
	sd	t0, P_SAVEAREA+96(PRIVATE)
	sd	t1, P_SAVEAREA+104(PRIVATE)
	sd	t2, P_SAVEAREA+112(PRIVATE)
	sd	t3, P_SAVEAREA+120(PRIVATE)
	jal	pul_cc_puthex
	nop					# (BD)
	la	a0, delim
	jal	pul_cc_puts
	nop					# (BD)
	ld	a1, P_SAVEAREA+8(PRIVATE)
	ld	v0, P_SAVEAREA+16(PRIVATE)
	ld	v1, P_SAVEAREA+24(PRIVATE)
	ld	t5, P_SAVEAREA+32(PRIVATE)
	ld	t6, P_SAVEAREA+40(PRIVATE)
	ld	t7, P_SAVEAREA+48(PRIVATE)
	ld	ra, P_SAVEAREA+80(PRIVATE)
	ld	t8, P_SAVEAREA+56(PRIVATE)
	ld	t9, P_SAVEAREA+64(PRIVATE)
	ld	a2, P_SAVEAREA+88(PRIVATE)
	ld	t0, P_SAVEAREA+96(PRIVATE)
	ld	t1, P_SAVEAREA+104(PRIVATE)
	ld	t2, P_SAVEAREA+112(PRIVATE)
	ld	t3, P_SAVEAREA+120(PRIVATE)
	j	ra
	nop					# (BD)
	END(print_byte_nocr)

LEAF(pass_test)
	li	t0, EV_SENDINT;
	li	t1, PASS_LEVEL << 8;
	ori	t1, 127;
	sd	t1, 0(t0);
	li	t0, EV_SPNUM;
	ld	zero, 0(t0);
	li	t0, EV_CIPL0;
	li	t1, PASS_LEVEL;
	sd	t1, 0(t0);
        END(pass_test)

/* This routine must immediately follow pass_test since it falls through */
LEAF(pass_cleanup)
	li	t0, EV_ILE;
	sd	zero, 0(t0);
	lw	t0, P_SR(PRIVATE);
	mtc0	t0, C0_SR;
	ld	ra, P_RA(PRIVATE);
	ld	sp, P_SP(PRIVATE);
	li	v0, 0;
	j	ra;
	nop
	END(pass_cleanup)

LEAF(fail_test)
	move	k1, CURPROC		# Save the CURPROC pointer.
	li	t0, EV_SENDINT
	li	t1, FAIL_LEVEL << 8
	ori	t1, 127
	sd	t1, 0(t0)

	# This is just to make sure the interrupt gets out of the CC's queue
	li	t0, EV_SPNUM
	ld	zero, 0(t0)

	li	t0, EV_CIPL0
	li	t1, PASS_LEVEL
	sd	t1, 0(t0)
	lw	t0, P_SR(PRIVATE)
	move	a0, t3			# Fail macro sets t3
	jal	pul_cc_puthex
	nop
	la	a0, crlf
	jal	pul_cc_puts
	nop
	END(fail_test)

/* This routine must immediately follow fail_test since it falls through */
LEAF(fail_cleanup)
	li	t0, EV_ILE
	sd	zero, 0(t0)
	lw	t0, P_SR(PRIVATE)
	mtc0	t0, C0_SR
	ld	ra, P_RA(PRIVATE)
	ld	sp, P_SP(PRIVATE)
	move	v0, k0
	li	t0, NIBFAIL_GENERIC
	beq	t0, k0, 1f
	nop

	la	t1, nib_exc_start

	# If we originated the fail, store the reason
	sw	k0, STUP_WHY(t1)

	# Also store which CPU failed.
	li	t0, EV_SPNUM
	ld	t0, 0(t0)
	sw	t0, STUP_WHO(t1)

	li	t0, NIBFAIL_PROC
	bne	v0, t0, 1f
	nop

	lw	t0, U_PID(k1)		# Get the process ID of the failing
					# process.  k1 contains CURPROC.
	nop
	sw	t0, STUP_PID(t1)	# Store it.
1:
	j	ra
	nop
	END(fail_cleanup)

	.globl	nib_text_end	/* A required label for nextract */
nib_text_end:

	.data
	.globl	nib_data_start
nib_data_start:			/* A required label for nextract */
.align	3
/* The directory stores each processor's private data pointer and stack
 *	pointer at an absolute location based on the processor's physical
 *	location in the chassis.
 */
.lcomm	directory	DIR_ENT_SIZE * 64
/* The private data section is divided up and handed out by logical (Niblet)
 *	processor number.
 */
.align	3
.lcomm	private,	PRIV_SIZE * MAXCPUS + 8	# Private per-CPU storage.
/* The run queue pointer is kept here */
.lcomm	runqueue,	4
#ifdef 0
.lcomm	runqlock,	4
.lcomm	rlockcheck,	4
.lcomm	initlock,	4
#else
.lcomm	runqlock,	132
.lcomm	rlockcheck,	132
.lcomm	initlock,	132
#endif
.lcomm	sync_addr	4	# Initial barrier address
.lcomm	numprocs,	4
.lcomm	goahead,	4
.lcomm	cpunum,		4 	# CPU numbers start with 0 and go by four...
.lcomm	next_private	4
.lcomm	flag,		8
.lcomm	turn, 		4
.lcomm	intrvect,	32
.lcomm	intrcount,	32
.lcomm	intrasid,	32
.lcomm	intrenhi,	32

msgs:
MSG_ALIGN
msg0:
	.asciiz	"Process number:\r\n"
MSG_ALIGN
msg1:
	.asciiz  "Instruction count:\r\n"
MSG_ALIGN
msg2:
	.asciiz	"Failed process registers: (1-31,PC)\r\n"
MSG_ALIGN
msg3:
	.asciiz	"Test passed\r\n"
MSG_ALIGN
msg4:
	.asciiz	"Number of processes active:\r\n"
MSG_ALIGN
msg5:
	.asciiz	"Memory error (virtual addr, physcial addr, wrote, read)s:\r\n"
MSG_ALIGN
msg6:
	.asciiz "Register dump: (1-7,30,31,PC)\r\n"
MSG_ALIGN
msg7:
	.asciiz "Process failed: "
MSG_ALIGN
msg8:
	.asciiz "Bad virtual address in general handler (BADVADDR, EPC, PID):\r\n"
MSG_ALIGN
msg9:
	.asciiz	"Processor initializing...\r\n"
MSG_ALIGN
msg10:
	.asciiz	"Processor waiting...\r\n"
MSG_ALIGN
msg11:
	.asciiz  "Unexpected exception (CAUSE, EPC, SR, BADVA, PID)\r\n"
MSG_ALIGN
msg12:
	.asciiz  "Locking has failed.\r\n"
MSG_ALIGN
msg13:
	.asciiz	"Preparing processes 2...\r\n"
MSG_ALIGN
msg14:
	.asciiz	"Finished with pass...\r\n"
MSG_ALIGN
msg15:
	.asciiz	"Master CPU: "
MSG_ALIGN
msg16:
	.asciiz	"CPU waiting to run: "
MSG_ALIGN
msg17:
	.asciiz	"CPU running: "
MSG_ALIGN
msg18:
	.asciiz	"Exception count (exception number, count):\r\n"
MSG_ALIGN
msg19:
	.asciiz	"Interrupt count (interrupt number, count):\r\n"
MSG_ALIGN
msg20:
	.asciiz	"No processes left to run - twiddling.\r\n"
MSG_ALIGN
msg21:
	.asciiz	"Starting initialization.\r\n"
MSG_ALIGN
msg22:
	.asciiz	"Finished with initialization.\r\n"
MSG_ALIGN
msg23:
	.asciiz	"Status: "
MSG_ALIGN
msg24:
	.asciiz	"Loading: "
MSG_ALIGN
msg25:
	.asciiz	"Checking: "
MSG_ALIGN
msg26:
	.asciiz	"Context Switching "
MSG_ALIGN
msg27:
	.asciiz	"Starting Pass\r\n"
MSG_ALIGN
msg28:
	.asciiz	"Unexpected interrupt (CAUSE):\r\n"
MSG_ALIGN
msg29:
	.asciiz	"CURPROC != U_MYADR (CURPROC, U_MYADR)\r\n"
MSG_ALIGN
msg30:
	.asciiz "Curproc is at: "
MSG_ALIGN
msg31:
	.asciiz "Niblet timed out.\r\n"
MSG_ALIGN
msg32:
	.asciiz "Niblet passed on an interrupt.\r\n"
MSG_ALIGN
msg33:
	.asciiz "Niblet FAILED on an interrupt.\r\n"
MSG_ALIGN
msg34:
	.asciiz "Last process passed.\r\n"
MSG_ALIGN
msg35:
	.asciiz "ERTOIP is nonzero! (ERTOIP, CAUSE, EPC)\r\n"
exceptnames:
MSG_ALIGN
	.asciiz	"0 - Interrupt\r\n"
MSG_ALIGN
	.asciiz	"1 - TLB Modification\r\n"
MSG_ALIGN
	.asciiz	"2 - TLB Load Exception\r\n"
MSG_ALIGN
	.asciiz	"3 - TLB Store Exception\r\n"
MSG_ALIGN
	.asciiz	"4 - Load Address Error\r\n"
MSG_ALIGN
	.asciiz	"5 - Store Address Error\r\n"
MSG_ALIGN
	.asciiz	"6 - Instruction Bus Error\r\n"
MSG_ALIGN
	.asciiz	"7 - Data Bus Error\r\n"
MSG_ALIGN
	.asciiz	"8 - Syscall\r\n"
MSG_ALIGN
	.asciiz	"9 - Breakpoint\r\n"
MSG_ALIGN
	.asciiz	"10 - Reserved Instruction\r\n"
MSG_ALIGN
	.asciiz	"11 - Coprocessor Unusable\r\n"
MSG_ALIGN
	.asciiz	"12 - Arithmetic Overflow\r\n"
MSG_ALIGN
	.asciiz	"13 - Trap Exception\r\n"
MSG_ALIGN
	.asciiz	"14 - Inst VCE at "
MSG_ALIGN
	.asciiz	"15 - Floating Point Exception\r\n"
MSG_ALIGN
	.asciiz	"16\r\n"
MSG_ALIGN
	.asciiz	"17\r\n"
MSG_ALIGN
	.asciiz	"18\r\n"
MSG_ALIGN
	.asciiz	"19\r\n"
MSG_ALIGN
	.asciiz	"20\r\n"
MSG_ALIGN
	.asciiz	"21\r\n"
MSG_ALIGN
	.asciiz	"22\r\n"
MSG_ALIGN
	.asciiz	"23 - Watchpoint\r\n"
MSG_ALIGN
	.asciiz	"24\r\n"
MSG_ALIGN
	.asciiz	"25\r\n"
MSG_ALIGN
	.asciiz	"26\r\n"
MSG_ALIGN
	.asciiz	"27\r\n"
MSG_ALIGN
	.asciiz	"28\r\n"
MSG_ALIGN
	.asciiz	"29\r\n"
MSG_ALIGN
	.asciiz	"30\r\n"
MSG_ALIGN
	.asciiz	"31 - Data VCE at "
MSG_ALIGN
crlf:
	.asciiz	"\n\r"
delim:
	.asciiz "> "
	.globl	nib_data_end	/* A required label for nextract */
nib_data_end:
	.word	0
