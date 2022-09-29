#include <regdef.h>
#include <asm.h>
#include "../pod.h"
#include "nmsg.h"
#include "nsys.h"
#if R4000
#include "cp0_r4k.h"
#elif TFP
#include <sys/tfp.h>
#endif
#include <sys/EVEREST/everest.h>
#include "niblet.h"
#include "passfail.h"
#include "locks.h"
#include <sys/i8251uart.h>
#include "../niblet_debug.h"

#define CONFIG_DC_SHFT  6       /* shift DC to bit position 0 */
#define CACH_PD         0x1     /* primary data cache */
#define CACH_SD         0x3     /* secondary data cache */
#define C_IWBINV        0x0     /* index writeback inval (d, sd) */
#define C_ILT           0x4     /* index load tag (all) */
#define C_IST           0x8     /* index store tag (all) */
#define	TLBHI_VPNMASK32	0xffffe000
#define EV_CELMAX       127


#ifdef R4000
#define WIRED_ENTRIES
#endif


/* NIBERROR_MSG(), NIBERROR_MSG_REG(), and NIBERROR_MSG_COP0() all use r18 and r19 */
#define NIBERROR_MSG(msg) \
	LI	r18, MIND2OFF(msg); \
	LA	r19, msgs; \
	ADD	r18, r19, r18; \
	STRING_DUMP(r18); \

#define NIBERROR_MSG_REG(msg, reg) \
	LI	r18, MIND2OFF(msg); \
	LA	r19, msgs; \
	ADD	r18, r19, r18; \
	STRING_DUMP(r18); \
	PRINT_HEX(reg); \


#define NIBERROR_MSG_COP0(msg, cop0_reg) \
	LI	r18, MIND2OFF(msg); \
	LA	r19, msgs; \
	ADD	r18, r19, r18; \
	STRING_DUMP(r18); \
	MFC0 (r18, cop0_reg); \
	PRINT_HEX(r18); \


#ifndef NIBDEBUG
#define NIBDEBUG_MSG(msg)
#define NIBDEBUG_MSG_REG(msg, reg)
#define NIBDEBUG_MSG_COP0(msg, cop0_reg)
#endif


#ifdef NIBDEBUG
#if defined DEBUG_NIBLET_WITH_STORES

#define DEBUG_BUFFER_SIZE 0xfff

/* R28 points to start of dump memory area (0x1800000 for proc 1, 
   0x1900000 for proc 2).  R30 is a double word index into that memory */
#define NIBDEBUG_MSG(msg) \
	DMFC0(r19, C0_ENHI); \
	li	r18, 0xff0; \
	and	r19, r19, r18; \
	dsll	r19, 40; \
	li	r18, msg; \
	or	r18, r19, r18; \
	dli	r19, 0xabcd000000000000; \
	or	r18, r19, r18; \
	daddu	r19, r28, r30; \
	sd r18 0(r19); \
	dli	r18, 0xabcd000000000100; \
	sd r18 8(r19); \
	daddi	r30, 8; \
	dli	r18, DEBUG_BUFFER_SIZE; \
	and	r19, r30, r18; \
	bne	r19, r0, 88f; \
	nop; \
	li	r30, 0; \
88:
	


#define NIBDEBUG_MSG_REG(msg, reg) \
	DMFC0(r19, C0_ENHI); \
	li	r18, 0xff0; \
	and	r19, r19, r18; \
	dsll	r19, 40; \
	li	r18, msg; \
	or	r18, r19, r18; \
	dli	r19, 0xabcd000000000000; \
	or	r18, r19, r18; \
	daddu	r19, r28, r30; \
	sd r18 0(r19); \
	sd reg 8(r19); \
	dli	r18, 0xabcd000000000100; \
	sd r18 16(r19); \
	daddi	r30, 8; \
	dli	r18, DEBUG_BUFFER_SIZE; \
	and	r19, r30, r18; \
	beq	r19, r0, 87f; \
	daddi	r30, 8; \
	and	r19, r30, r18; \
	bne	r19, r0, 88f; \
	nop; \
87: \
	li	r30, 0; \
88:

#define NIBDEBUG_MSG_COP0(msg, cop0_reg) \
	DMFC0(r19, C0_ENHI); \
	li	r18, 0xff0; \
	and	r19, r19, r18; \
	dsll	r19, 40; \
	li	r18, msg; \
	or	r18, r19, r18; \
	dli	r19, 0xabcd000000000000; \
	or	r18, r19, r18; \
	daddu	r19, r28, r30; \
	sd r18 0(r19); \
	MFC0	(r18, cop0_reg); \
	sd r18, 8(r19); \
	dli	r18, 0xabcd000000000100; \
	sd r18 16(r19); \
	daddi	r30, 8; \
	dli	r18, DEBUG_BUFFER_SIZE; \
	and	r19, r30, r18; \
	beq	r19, r0, 87f; \
	daddi	r30, 8; \
	and	r19, r30, r18; \
	bne	r19, r0, 88f; \
	nop; \
87: \
	li	r30, 0; \
88:

#else if !defined DEBUG_NIBLET_WITH_STORES
#define NIBDEBUG_MSG(msg) NIBERROR_MSG(msg)
#define NIBDEBUG_MSG_REG(msg, reg) NIBERROR_MSG_REG(msg, reg)
#define NIBDEBUG_MSG_COP0(msg, cop0_reg) NIBERROR_MSG_COP0(msg, cop0_reg)
#endif



/* 
   DEBUG_DUMP_TLB_ENTRY is a wraper for DUMP_TLB_ENTRY.  It uses r20, 
   in addition to r21, r22, r23, used by DUMP_TLB_ENTRY
*/
#ifdef DEBUG_DUMP_TLB
#define DEBUG_DUMP_TLB_ENTRY(index) \
	daddu	r20, r0, a0; \
	li	a0, index; \
	DUMP_TLB_ENTRY(a0); \
	daddu	a0, r0, r20
#else  !DEBUG_DUMP_TLB
#define DEBUG_DUMP_TLB_ENTRY(index)
#endif DEBUG_DUMP_TLB



/* DUMP_TLB_ENTRY is a wuses r21, r22, r23 .  It assumes the index
   you want to dump is in a0 (r4) 
*/
#define DUMP_TLB_ENTRY(index) \
	daddu	r21, r0, a1; \
	daddu	r22, r0, a2; \
	daddu	r23, r0, v0; \
                         \
	NIBDEBUG_MSG_REG(M_DUMP_TLB_ENTRY, a0); \
	li	a2, 12;          \
                         \
	li	a1, 0;           \
	jal	get_enhi;        \
	PRINT_HEX(v0);       \
	jal	get_enlo;        \
	PRINT_HEX(v0);       \
                         \
	li	a1, 1;           \
	jal	get_enhi;        \
	PRINT_HEX(v0);       \
	jal	get_enlo;        \
	PRINT_HEX(v0);       \
                         \
	li	a1, 2;           \
	jal	get_enhi;        \
	PRINT_HEX(v0);       \
	jal	get_enlo;        \
	PRINT_HEX(v0);       \
                         \
	daddu	a1, r21, r0; \
	daddu	a2, r22, r0; \
	daddu	v0, r23, r0


#endif NIBDEBUG

#define ERET			\
	ADDU	k0, r0,CURPROC;	\
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


/* Margie - Can't use SET_LEDS with PRINT_IN_TLB_REFILL on! */
#if 1
#define SET_LEDS(_Value)
#define SET_LEDS_R(_reg)
#else
#define SET_LEDS(_value)		\
	LI	k0, EV_LED_BASE;	\
	LI	k1, _value;		\
	sd	k1, 0(k0);		\

#define SET_LEDS_R(_reg)		\
	LI	k0, EV_LED_BASE;	\
	sd	_reg, 0(k0);		\

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
	.globl	nsys_tlbdump	# dump tlb entry
	.globl	nsys_debugon	# turn on debug info
	.globl	nsys_debugoff	# turn off debug info
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
	LA	k0, nib_tlb_refill
	j	k0
	nop
	END(TLBREFILL)

#ifdef R4000
	.align 7
#elif TFP
.align 10  /* puts us at 0x800 */
#endif

LEAF(XTLBREFILL)
	EXCHEAD
	LA	k0, nib_xtlb_refill
	j	k0
	nop
	END(XTLBREFILL)

#ifdef R4000
	.align 7
#elif TFP
.align 10  /* puts us at 0xc00 */
#endif

LEAF(CACHEERROR)
	EXCHEAD
	LA	k0, nib_cache_err

/* Margie FIX */
	# Jump to the handler uncached!
	LI	k1, 0x1fffffff
	and	k0, k1
	lui	k1, 0xa000
	or	k0, k1

	j	k0
	nop					# (BD)
	END(CACHEERROR)

#ifdef R4000
	.align 7
#elif TFP
.align 10  /* puts us at 0x1000 */
#endif
	
LEAF(GENEXCPT)
	EXCHEAD
	LA	k0, nib_gen_exc
	j	k0
	nop
	END(GENEXCPT)

	.globl nib_exc_end	/* A required label for nextract */
nib_exc_end:
	.align	3

	.globl nib_text_start	/* A required label for nextract */
nib_text_start:

LEAF(nib_tlb_refill)
	MFC0	(k1, C0_BADVADDR)
	nop
	bltz	k1, nib_gen_exc	# Jump to GENEXCPT if kernel address
	MFC0    (k1, C0_EPC)	# Restore k1's old value.
#ifdef R4000
	MFC0    (k0, C0_CTXT)
	nop
	nop
	LW      k1, 0(k0)
	LW      k0, 8(k0)                # LDSLOT load the second mapping
	nop
	nop
	MTC0    (k1, C0_ENLO0)
	MTC0    (k0, C0_ENLO1)
	nop
	tlbwr
	nop
	nop
	nop
	nop
#elif TFP
	MFC0	(k1, C0_BADVADDR)
	nop
	MFC0	(k0, C0_SHIFTAMT)
	nop
	dsrl	k1, k1, k0
	dsll	k1, k1, 3
	MFC0	(k0, C0_UBASE)  /* MARGIE Could be PTEBASE?  FIX */
	dadd	k0, k1
	ld		k1, 0(k0)
	MTC0	(k1, C0_ENLO)
	TLB_WRITER
#endif
	eret
	END(nib_tlb_refill)

LEAF(nib_xtlb_refill)
	MFC0	(k1, C0_BADVADDR)
	nop
	bltz	k1, GENEXCPT	# Jump to GENEXCPT if kernel address
	MFC0    (k1, C0_EPC)	# Restore k1's old value.
#ifdef R4000
	MFC0    (k0, C0_CTXT)
	nop

	LW      k1, 0(k0)
	LW      k0, 8(k0)                # LDSLOT load the second mapping
	nop

	MTC0    (k1, C0_ENLO0)
	MTC0    (k0, C0_ENLO1)
	nop
	tlbwr
	nop
	nop	
	nop
#elif TFP
	MFC0	(k1, C0_BADVADDR)
	nop
	MFC0	(k0, C0_SHIFTAMT)
	nop
	dsrl	k1, k1, k0
	dsll	k1, k1, 3
	MFC0	(k0, C0_UBASE)  /* MARGIE Could be PTEBASE? FIX */
	dadd	k0, k1
	ld		k0, 0(k0)
	MTC0	(k0, C0_ENLO)
	TLB_WRITER
#endif
	eret
	END(nib_xtlb_refill)

LEAF(nib_cache_err)
	ECCFAIL
	END(nib_cache_err)

LEAF(nib_gen_exc)
/* 
   DON'T PUT ANY NIBDEBUG statements here!  I think they put you in
   an infinite loop!
*/
	LI	k0, EV_SPNUM
	ld	k0, 0(k0)		# Get slot/processor number

done_kludge:

	LA	k1, directory		# Addresses of private data areas (LD)
	andi	k0, EV_SPNUM_MASK	# Mask off extraneous stuff
	SLL	k0, DIR_SHIFT		# Shift by log2(dir ent size)
	ADD	k1, k0			# k1 = address of our pointers
	LW	k0, DIR_PRIVOFF(k1)	# k0 = our private area
	nop
	LW	k1, P_CURPROC(k0)	# Address of current process's
					# 	"U area"
	SAVE_T(k1)

	ADDU	CURPROC, k1,r0		# s0 contains CURPROC
	ADDU	PRIVATE, k0,r0		# s2 contains PRIVATE

	MFC0	(t0, C0_CAUSE)		# Get exception cause register

	nop

	LI t1, CAUSE_EXCMASK		# Is it an exception?
	and t1, t0, t1			# Mask off the approprtiate bits

	
	# Check per-process table
	ADD	t0, CURPROC, t1

	# Address to which EXCCNTBASE and EXCTBLBASE can be added.

	LW	t2, U_EXCCNTBASE(t0)	# Get current count
	nop
	ADDI	t2, t2, 1		# add one
	SW	t2, U_EXCCNTBASE(t0)	# Store new count
	LW	k0, U_EXCTBLBASE(t0)	# Get handler vector
	MFC0	(MVPC, C0_EPC)		# Get EPC (MVPC = s1)
	nop
	beq	k0, r0, std_hndlr
	SW	MVPC, P_MVPC(PRIVATE)	# (BD) Store away old EPC

stolen_vector:
	# Make sure this sucker's in the TLB.
	# ############################################################
	# k0 contains the user routine virtual address.
	LW	t1, U_EXCENHIBASE(t0)  # Get ENHI for this vector
	nop

#ifdef WIRED_ENTRIES
#ifdef R4000
	MTC0	(t1, C0_ENHI)
	nop
	TLB_PROBE	
	nop
	MFC0	t0, C0_INX			# Get index register
	nop
	bgez	t0, excintlb			# If present, go on
	LI	k1, PTEBASE			# Get page table base
	SRL	t0, k0, PGSZ_BITS + 1		# Get VPN / 2
	SLL	t0, t0, 4
	ADD	t0, t0, k1			# Get PTE address
	LW      k1, 0(t0)			# Load first mapping
	LW      t0, 8(t0)             		# load the second mapping
	nop
	MTC0	(t1, C0_ENHI)  # MARGIE This is not needed, done above FIX
	MTC0    (k1, C0_ENLO0)
	MTC0    (t0, C0_ENLO1)
	nop
	tlbwr
	nop
#elif TFP
	MTC0	(t1, C0_ENHI)
	dsll	t1, t1, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
	MTC0	(t1, C0_ICACHE)
	dsrl	t1, t1, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
	MTC0	(t1, C0_BADVADDR)
	TLB_PROBE
	MFC0	(t0, C0_TLBSET)
	bgez	t0, excintlb
	MFC0	(k1, C0_SHIFTAMT)
	nop
	dsrl	t0, k0, k1
	dsll	t0, t0, 3
	dli		k1, PTEBASE
   	dadd	t0, t0, k1
	ld		k1, 0(t0)
	MTC0	(k1, C0_ENLO)
	TLB_WRITER
#endif
excintlb:
#else
	jal	force_enhi_tlb			# Force the page into the TLB
	nop
#endif

	# ############################################################
	LA	t0, usr_hndlr_ret		# Put return address in R_VECT
	SW	t0, U_RVECT(CURPROC)
	LA	t0, pass_to_niblet		# Standard handler + preamble
	SW	t0, U_SYSVECT(CURPROC)
	j	k0
	nop

usr_hndlr_ret:
	LW	MVPC, P_MVPC(PRIVATE)		# Get old EPC
	nop					# (LD)
	ADDI	MVPC, MVPC, 4			# calculate the next address
	MTC0	(MVPC, C0_EPC)			# Put EPC back.
	nop
	ERET
	nop

pass_to_niblet:
	MFC0	(t0, C0_CAUSE) 	# Get the cause register
	nop
	LI t1, CAUSE_EXCMASK	# What flavor of exception
	and t1, t0, t1	# Mask off the approprtiate bits
std_hndlr:
	# t1 now contains a word offset from the table start.
	# EPC is in MVPC

	LA t0, excvect 		# Load table address
	ADD t0, t0, t1	# Add in offset
	LW t0, 0(t0)
	NIBDEBUG_MSG_REG(M_HANDLER_VECTOR_IS, t0);
	nop
	j t0
	nop



	# Exception Vector Table
.data
excvect:
	PTR_WORD	notexc		# Exc 0  (Interrupt)
	PTR_WORD	unsupported 	# Exc 1	 (TLB modify exception)
	PTR_WORD	tlbl		# Exc 2	 (TLB Load miss)
	PTR_WORD	tlbs		# Exc 3	 (TLB Store miss)
	PTR_WORD	unsupported	# Exc 4  (Read address error)
	PTR_WORD	unsupported	# Exc 5  (Write address error)
	PTR_WORD	unsupported	# Exc 6  (I fetch bus error)
	PTR_WORD	unsupported	# Exc 7  (Data bus error)
	PTR_WORD	syscall		# Exc 8  (Syscall)
	PTR_WORD	unsupported	# Exc 9  (Breakpoint)
	PTR_WORD	unsupported	# Exc 10 (reserved instruction)
	PTR_WORD	unsupported	# Exc 11 (coprocessor unusable)
	PTR_WORD	unsupported	# Exc 12 (arithmetic overflow)
	PTR_WORD	unsupported	# Exc 13 (Trap (MIPS II only))
#ifdef R4000
	PTR_WORD	vcei		# Exc 14 (Instruction Virtual coherency excptn)
#elif TFP
	PTR_WORD	unsupported	# Exc 14 (Twin Peaks uses bit 25 of CAUSE)
#endif
	PTR_WORD	unsupported	# Exc 15 (Floating point exception)
	PTR_WORD	unsupported	# Exc 16 (reserved)
	PTR_WORD	unsupported	# Exc 17 (reserved)
	PTR_WORD	unsupported	# Exc 18 (reserved)
	PTR_WORD	unsupported	# Exc 19 (reserved)
	PTR_WORD	unsupported	# Exc 20 (reserved)
	PTR_WORD	unsupported	# Exc 21 (reserved)
	PTR_WORD	unsupported	# Exc 22 (reserved)
	PTR_WORD	unsupported	# Exc 23 (reference to watch hi/lo address)
	PTR_WORD	unsupported	# Exc 24 (reserved)
	PTR_WORD	unsupported	# Exc 25 (reserved)
	PTR_WORD	unsupported	# Exc 26 (reserved)
	PTR_WORD	unsupported	# Exc 27 (reserved)
	PTR_WORD	unsupported	# Exc 28 (reserved)
	PTR_WORD	unsupported	# Exc 29 (reserved)
	PTR_WORD	unsupported	# Exc 30 (reserved)
#ifdef R4000
	PTR_WORD	vced	# Exc 31 (Data virtual coherency exception)
#elif TFP
	PTR_WORD	unsupported	# Exc 31 (Twin Peaks doesn't have this exception)
#endif
.text

unsupported:
	# Get pid and print it
	li	t0, 0x99990000; PRINT_HEX(t0) 	/* DEBUG:	 remove */
	LW	t0, U_PID(CURPROC)
	PRINT_HEX(r1)
	PRINT_HEX(r2)
	PRINT_HEX(r3)
	PRINT_HEX(r4)
	PRINT_HEX(r5)
	PRINT_HEX(r6)
	PRINT_HEX(r7)
	PRINT_HEX(r8)
	PRINT_HEX(r9)
	PRINT_HEX(r10)
	PRINT_HEX(r11)  /* DEBUG: remove */
	PRINT_HEX(r12)  /* DEBUG: remove */
	PRINT_HEX(r13)  /* DEBUG: remove */
	PRINT_HEX(r14)  /* DEBUG: remove */
	PRINT_HEX(r15)  /* DEBUG: remove */
	PRINT_HEX(r31)  /* DEBUG: remove */

	LI	t0, MIND2OFF(M_UNSUPPORT)
	LA	t1, msgs
	ADD	t0, t1, t0
	STRING_DUMP(t0)

	MFC0	(t0, C0_CAUSE)
	LI	t1, CAUSE_EXCMASK		# Which exception?
	and	t1, t0, t1	# Mask off the approp. bits
	SLL	t1, t1, (MSG_BITS - WORDSZ_SHIFT)  

loadename:
	LA	t0, exceptnames
	ADD	t0, t0, t1
	STRING_DUMP(t0)

	MFC0	(t0, C0_CAUSE)
	MFC0	(t1, C0_EPC)
	PRINT_HEX(t0)
	PRINT_HEX(t1)
	MFC0	(t0, C0_SR)
	MFC0	(t1, C0_BADVADDR)
	PRINT_HEX(t0)
	PRINT_HEX(t1)
	# Get PID
	LW	t2, U_PID(CURPROC)
	PRINT_HEX(t2)
	nop

	FAIL(NIBFAIL_BADEXC)
	nop

        .globl  ertoip
ertoip:
	LI	t0, MIND2OFF(M_ERTOIP)
	LA	t1, msgs
	ADD	t0, t1, t0
	STRING_DUMP(t0)
	LI	t0, EV_ERTOIP
	ld	t0, 0(t0)
	MFC0	(t1, C0_CAUSE)
	nop
	PRINT_HEX(t0)
	MFC0	(t0, C0_EPC)
        nop
	PRINT_HEX(t1)
	PRINT_HEX(t0)

	FAIL(NIBFAIL_ERTOIP)
	nop

#ifdef R4000
vced:
	LI	t1, 31
        SLL     t1, t1, MSG_BITS
        LA      t0, exceptnames
        ADD     t0, t0, t1
        STRING_DUMP(t0)
	b	1f
	nop
#endif

vcei:
	LI	t1, 14
        SLL     t1, t1, MSG_BITS
        LA      t0, exceptnames
        ADD     t0, t0, t1
        STRING_DUMP(t0)

1:
	MFC0	(t0, C0_BADVADDR)
        nop; nop; nop; nop

	PRINT_HEX(t0)
	
	# Need to get physical address corresponding to this
	# 	virtual address.

#ifdef R4000
	LI	t1, TLBHI_VPNMASK32
	and	t1, t0			# Get VPN2 for ENHI
#elif TFP
   	ADDU	t1, t0, r0  	    	# Just move it into t1 for compatibility with following code
#endif
					# Only for 4k pages
	LW	t2, U_PID(CURPROC)

#ifdef TFP
	dsll	t2, TLBHI_PIDSHIFT
#endif

	or	t1, t2			# Or in PID

	MTC0	(t1, C0_ENHI)
#ifdef TFP
	dsll	t1, t1, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
	MTC0	(t1, C0_ICACHE)
	dsrl	t1, t1, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
#endif
        nop; nop; nop; nop
	
	TLB_PROBE
	nop

	MFC0	(t1, C0_INX)
        nop; nop; nop; nop

	bltz	t1, take_a_shot
	nop					# (BD)

	# Get the address from ENLO

	tlbr
	nop

#ifdef R4000
	LI	t1, (1 << PGSZ_BITS)
	and	t1, t0		# Even or odd page?
	
	beqz	t1, 1f
	nop

	b	2f
	MFC0	(t1, C0_ENLO1)		# Odd page (BD)

1:
	MFC0	(t1, C0_ENLO0)		# Even page

2:	nop; nop; nop; nop
#elif TFP
    	MFC0	(t1, C0_ENLO)
#endif

	andi	k0, t1, TLBLO_VMASK	# Valid?
	beq	k0, r0, take_a_shot		# If not, guess...
	nop					# (BD)
	
	LI	k0, TLBLO_PFNMASK
	and	t1, k0			# PFN in place
	SLL	t1, PFNSHIFT		# Actual Phys addr of Pg

	LI	k0, 0x0fff			# Mask off "index" bits
	and	k1, t0, k0			# From BadVaddr
	
	b 	get_stag
	or	t1, k1			# Synthesized address (BD)
	
take_a_shot:

#ifdef R4000

        cache   C_ILT|CACH_PD, 0(t0)	# Get primary tag
        nop; nop; nop; nop

        MFC0    (t1, C0_TAGLO)		# Get TagLo
        nop; nop; nop; nop

	SRL	t1, 8			# Only get PTagLo field
	SLL	t1, 12			# Make it into an address

get_stag:

	# t1 = phsical address 

	lui	k0, 0x8000
	or	t1, k0			# Make it a k0seg address
	LI	k0, ~(0x7f)		# All but bottom 7 bits
	and	t1, k0			# Cache line address

	cache	C_ILT|CACH_SD, 0(t1)	# Get STagLo
        nop; nop; nop; nop
        MFC0    (k0, C0_TAGLO)		# k0 = STagLo
        nop; nop; nop; nop

	LI	k1, ~(0x0380)		# Mask off bits 9..7 (VIndex)
	and	k0, k1			# k0 = STagLo w/o Virtual Index
	LI	k1, 0x7000		# Mask for bits 14..12
	and	k1, t0			# Get virtual index for Bad Vaddr
	SRL	k1, 5			# Shift from 14..12 to 9..7
	or	k0, k1			# Or in new virtual index

	MTC0	(k0, C0_TAGLO)
        nop; nop; nop; nop
	cache	C_IST|CACH_SD, 0(t1)	# Get STagLo
        nop; nop; nop; nop

	ERET


#elif TFP
    	    	  /* Get primary tag */
    	    	  /* Get TagLo */
    	dctr	  /* MARGIE What goes here?  FIX */


get_stag:
	# t1 = phsical address 

	dli	k0, 0xa800000000000000
	or	t1, k0			# Make it a k0seg address
	LI	k0, ~(0x1f)		# All but bottom 5 bits
	and	t1, k0			# Cache line address

    	      /* Get STagLo */
    	      /* k0 = STagLo */
        dctr  /* MARGIE What goes here? FIX */

    	/* MARGIE Not right for TFP FIX */
	LI	k1, ~(0x0380)		# Mask off bits 9..7 (VIndex)
	and	k0, k1			# k0 = STagLo w/o Virtual Index
	LI	k1, 0x7000		# Mask for bits 14..12
	and	k1, t0			# Get virtual index for Bad Vaddr
	SRL	k1, 5			# Shift from 14..12 to 9..7
	or	k0, k1			# Or in new virtual index

    	/* MARGIE Not right for TFP FIX */
/*
	MTC0	(k0, C0_TAGLO)
        nop; nop; nop; nop
	cache	C_IST|CACH_SD, 0(t1)	# Get STagLo
        nop; nop; nop; nop
*/

	ERET
#endif


syscall:
	MFC0	(MVPC, C0_EPC)		# Get EPC
	nop
	ADDIU 	MVPC, 4			# Point to next instruction
	MTC0	(MVPC, C0_EPC)		# Save EPC
	SW	MVPC, P_MVPC(PRIVATE)	# Put old EPC in private area
	sltiu	t1, RSYS, NCALLS	# Set t1 if syscall # < NCALLS 	
	bnez	t1, 1f			# Make this process "fail" if
					# it tries to make a bogus syscall
	nop				# (BD)
	NIBERROR_MSG_REG(M_BAD_SYSCALL_VALUE, RSYS)
	ADDIU	MVPC, -4
	NIBERROR_MSG_REG(M_EPC_IS, MVPC)
	NIBERROR_MSG_REG(M_BRANCHED_ON_REG_VALUE, t1)
	FAIL(NIBFAIL_BADSCL)

1:
	LA 	t0, sysvect 		# Load table address (in delay slot)
	SLL	t1, RSYS, WORDSZ_SHIFT	# Get offset into table. (4*CALL#)

	ADD 	t0, t0, t1		# Add in offset
	LW 	t0, 0(t0)		# Get address
	nop
	j	t0
	nop

	# System Call Vector Table
.data
sysvect:
	PTR_WORD	nsys_fail	# Fail
	PTR_WORD	nsys_pass	# Pass
	PTR_WORD	nsys_rest	# Rest
	PTR_WORD	nsys_gtime	# Get time
	PTR_WORD	nsys_gquant	# Get current quantum
	PTR_WORD	nsys_squant	# Set current quantum
	PTR_WORD	nsys_ordain	# Ordain (go into kernel mode)
	PTR_WORD	nsys_defrock	# Defrock (back to user mode)
	PTR_WORD	nsys_msg	# Message
	PTR_WORD	nsys_gnproc	# Get the number of processes alive
	PTR_WORD	nsys_mark	# Just return
	PTR_WORD	nsys_invalid	# Invalidate a TLB entry
	PTR_WORD	nsys_scoher	# Set coherency for a page
	PTR_WORD	nsys_gpid	# Get process ID
	PTR_WORD	nsys_gcount	# Get the approximate instruction count
	PTR_WORD	nsys_printhex	# Print the value of r2 in hex.
	PTR_WORD	nsys_gshared	# Get the address of the shared memory space
	PTR_WORD	nsys_steale	# Steal an exception vector
	PTR_WORD	nsys_steali	# Steal an interrupt vector.
	PTR_WORD   nsys_suspnd	# Suspend scheduling
	PTR_WORD	nsys_resume	# Resume scheduling
	PTR_WORD	nsys_wpte	# Write a new page table entry.
	PTR_WORD	nsys_gecount	# Get exception count
	PTR_WORD	nsys_cecount	# Clear exception count
	PTR_WORD	nsys_vpass	# Verbose version of nsys_pass
	PTR_WORD	nsys_csout	# Set up user context switch out routine
	PTR_WORD	nsys_csin	# Set up user context switch in routine
	PTR_WORD	nsys_mode2	# Switch to MIPS II mode
	PTR_WORD	nsys_mode3	# Switch to MIPS III mode
	PTR_WORD	nsys_vtop	# Return phys addr for vaddr
	PTR_WORD	nsys_tlbdump	# Dump tlb entry
	PTR_WORD	nsys_debugon
	PTR_WORD	nsys_debugoff
	PTR_WORD 	0
.text

	.globl	nib_timeout
nib_timeout:
	LI	t0, MIND2OFF(M_TIMEOUT)
	LA	t1, msgs
	ADD	t0, t1, t0
	STRING_DUMP(t0)
	FAIL(NIBFAIL_TOUT)
	nop

nsys_fail:
	LI	t0, MIND2OFF(M_FAILED)
	LA	t1, msgs
	ADD	t0, t1, t0
	STRING_DUMP(t0)

	LW	t0, U_PID(CURPROC)
	PRINT_HEX(t0)

	LI	t0, MIND2OFF(M_FAILEDREGS)
	ADD	t0, t1, t0

	MFC0	(MVPC, C0_EPC)

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
	LA	t1, msgs

	LI	t0, MIND2OFF(M_PASSED)
	ADD	t0, t1, t0
	STRING_DUMP(t0)

	LI	t0, MIND2OFF(M_PROCNUM)
	ADD	t0, t1, t0
	STRING_DUMP(t0)

	LW	t0, U_PID(CURPROC)
	PRINT_HEX(t0)

	j continue_passing
	nop

nsys_vpass:
	LA	t1, msgs

	LI	t0, MIND2OFF(M_PASSED)
	ADD	t0, t1, t0
	STRING_DUMP(t0)

	LI	t0, MIND2OFF(M_PROCNUM)
	ADD	t0, t1, t0
	STRING_DUMP(t0)

	LW	t0, U_PID(CURPROC)
	PRINT_HEX(t0)

	LI	t0, MIND2OFF(M_REGDUMP)
	ADD	t0, t1, t0
	STRING_DUMP(t0)

	MFC0	(MVPC, C0_EPC)

	PRINT_HEX(r1)
	PRINT_HEX(r2)
	PRINT_HEX(r3)
	PRINT_HEX(r4)
	PRINT_HEX(r5)
	PRINT_HEX(r6)
	PRINT_HEX(r7)
	PRINT_HEX(r30)
	ADD	t0, r31, r0
	PRINT_HEX(t0)
	PRINT_HEX(MVPC)

continue_passing:

lock1:
#ifdef LOCKS
RUNLOCK
#endif
	LA	t1, numprocs
	LW	t2, 0(t1)		# Get number of current processes	
	LI	t0, 1
	SUBU	t2, t2, t0
	bne	t2, r0, dequeue
	SW	t2, 0(t1)

#ifdef LOCKCHECK
	LA	t0, rlockcheck		# Get the lock check address
	SW	r0, 0(t0)		# Put a zero in lockcheck.
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
	LW	MVPC, P_MVPC(PRIVATE)
	SAVE_REST(CURPROC)
	SW	MVPC, U_PC(CURPROC)

	LA	t0, rlockcheck	# Get the lock check address
	LW	t1, 0(t0)	# Get the value.
	LA	t2, runqueue	# Get addr of run queue pointer (delay slot)
#ifdef LOCKCHECK
	bne	t1, r0, mpfail0	# If it's not zero, locking has failed!
#endif
	LW	t2, 0(t2)	# Load run queue pointer (delay slot)

	LW	t1, P_CPUNUM(PRIVATE)	# Get cpunum
	ori	t1, 0x0100		# Or in this point's id
	SW	t1, 0(t0)		# Put a nonzero number in rlockcheck.

	# Pseudocode in comments:
	# if (RUNQ != NULL)
	#	runnewproc()
	bne	t2, r0, runnewproc
	# else {
	# 	if (N_PROCS == NULL)
	nop
	LA	t0, numprocs
	LW	t2, 0(t0)	# Load the process count
	nop			# (LD)
	bne	t2, r0, noprocesses
	nop			# (BD)
	#		PASS

	LA	t1, msgs
	LI	t0, MIND2OFF(M_STESTPASSED)
	ADD	t0, t1
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
	LA	t0, rlockcheck		# Get the lock check address
	SW	zero, 0(t0)		# Put a zero in lockcheck.
#endif
unlock2:
#ifdef LOCKS
	UNRUNLOCK
#endif
	NIBERROR_MSG(M_TWIDDLING)  # not really an error!
	LA	t1, msgs
	LI	t0, MIND2OFF(M_NOPROCS)
	ADD	t0, t1, t0
	STRING_DUMP(t0)
#ifndef MP
	FAIL(NIBFAIL_COHER)
#else
	LI 	t0, NIB_SR & ~SR_COUNTS_BIT	# Turn off count/compare ints
	MTC0	(t0, C0_SR)		# Load new SR
#endif

/* MARGIE OK for TFP? FIX */
	LI	t1, SR_IBIT3

	.globl	twiddle
twiddle:
	MFC0	(t0, C0_CAUSE)
	nop
	and	t0, t1			# t1 = SR_IBIT3
	bnez	t0, everest_zero
	nop
	j twiddle
	nop

	# t1 contains the interrupt level
	.globl	pass_exit
pass_exit:
	LI	t0, EV_CIPL0
	sd	t1, 0(t0)

	LA	t1, msgs
	LI	t0, MIND2OFF(M_PASSINT)
	ADD	t0, t1, t0
	STRING_DUMP(t0)

	PASSCLEANUP

	.globl	fail_exit
fail_exit:
	LI	t0, EV_CIPL0
	sd	t1, 0(t0)

	LA	t1, msgs
	LI	t0, MIND2OFF(M_FAILINT)
	ADD	t0, t1, t0
	STRING_DUMP(t0)

	FAILCLEANUP(NIBFAIL_GENERIC)

nsys_rest:
	j sched
	nop

nsys_gtime:
	j sys_return
	nop

nsys_gquant:
	LW	RPARM, U_QUANT(CURPROC)
	j sys_return
	nop

nsys_squant:
#ifdef R4000
	SW	RPARM, U_QUANT(CURPROC)
#elif TFP
    	li  	t0, 0x7fffffff
        sub 	t0, t0, RPARM
    	sd  	t0, U_QUANT(CURPROC)
#endif
	j sys_return
	nop

nsys_ordain:
	MFC0	(t0, C0_SR)		# Get status register value
	LI	t1, ~SR_KSUMASK	# Mask off kernel, supervsr, user bits
	and 	t0, t1, t0	# Zero 'em
	MTC0	(t0, C0_SR)		# Store it.
	nop
	nop
	nop
	nop
	j sys_return
	nop

#ifdef FIXED 

cs_out:
	LW	t1, U_OUTENHI(CURPROC) 	# Get ENHI for this vector
#ifdef CSDEBUG
	LI	t0, 0xc505ced
	PRINT_HEX(t0)
	PRINT_HEX(k0)
	PRINT_HEX(t1)
	PRINT_HEX(CURPROC)
#endif	
	LA	R_VECT,	sys_save
	j	user_cs
	nop				# la takes two instructions..  doesn't
					#	fit in delay slot

cs_in:
	LW	t1, U_INENHI(CURPROC) 	# Get ENHI for this vector
	
	/* Steve- deal with this a bit later. */
	LA	R_VECT,	state_restored
	LI	t0, 0xc515ced

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
#ifdef R4000
	MTC0	(t1, C0_ENHI)
	nop
	TLB_PROBE
	nop
	MFC0	(t0, C0_INX)			# Get index register
	nop
	bgez	t0, csintlb		# If present, go on
	LI	k1, PTEBASE			# Get page table base
	SRL	t0, k0, PGSZ_BITS + 1	# Get VPN / 2
	SLL	t0, t0, 4
	ADD	t0, t0, k1		# Get PTE address
	LW      k1, 0(t0)			# Load first mapping
	LW      t0, 8(t0)             # load the second mapping
	nop

/* PID print */
	PRINT_HEX(t1)

	MTC0	(t1, C0_ENHI)
	MTC0    (k1, C0_ENLO0)
	MTC0    (t0, C0_ENLO1)
	nop
	tlbwr
	nop
	nop
	nop
	nop
#elif TFP
	MTC0	(t1, C0_ENHI)
	dsll	t1, t1, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
	MTC0	(t1, C0_ICACHE)
	dsrl	t1, t1, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
	MTC0	(t1, C0_BADVADDR)
	TLB_PROBE
	MFC0	(t0, C0_TLBSET)
	bgez	t0, csintlb
	MFC0	(k1, C0_SHIFTAMT)
	nop
	dsrl	t0, k0, k1
	dsll	t0, t0, 3
	dli		k1, PTEBASE
   	dadd	t0, t0, k1
	ld		k1, 0(t0)

/* PID print */
	PRINT_HEX(t1)

	MTC0	(k1, C0_ENLO)
	TLB_WRITER
#endif

csintlb:
#else
	jal	force_enhi_tlb
	nop
#endif
	# ###############################################################
	j	k0				# Jump to user handler.
	nop
	# Won't return here.

#endif /* WIRED_ENTRIES */

nsys_printhex:
	LW	t0, U_PID(CURPROC)
	PRINT_HEX_NOCR(t0)
	PRINT_HEX(RPARM)
	j sys_return
	nop

nsys_tlbdump:
/*	DUMP_TLB_ENTRY(RPARM) */
	j sys_return
	nop

nsys_debugon:
	LI	t0, 0xa8a8a8a8
	PRINT_HEX(t0)
	j sys_return
	nop

nsys_debugoff:
	LI	t0, 0xa9a9a9a9
	j sys_return
	nop

nsys_defrock:
	MFC0	(t0, C0_SR)		# Get status register value
	LI	t1, ~SR_KSUMASK	# Mask off kernel, supervsr, user bits
	and 	t0, t1, t0	# Zero 'em
	LI	t1, SR_USER	# Load user bits
	or	t0, t1, t0	# Or in user mode
	MTC0	(t0, C0_SR)		# Store it.
	nop
	j sys_return
	nop

nsys_msg:
	LW	t0, U_PID(CURPROC)
	PRINT_HEX_NOCR(t0)
	SLL	RPARM, RPARM, MSG_BITS	# Convert message # to offset
	LA	t0, msgs		# Get base of message strings area
	ADDU	t0, RPARM, t0	# Address of message
	STRING_DUMP(t0)		# Print the string
	j sys_return
	nop

nsys_gnproc:
lock3:
#ifdef LOCKS
	RUNLOCK
#endif
	LA	t1, numprocs
	LW	t2, 0(t1)
unlock3:
#ifdef LOCKS
	UNRUNLOCK
#else
	nop
#endif
	ADDU	RPARM, t2, r0		# Get the number of currently active
					# processes
	j sys_return
	nop

nsys_invalid:
#ifdef R4000
	MTC0    (r0, C0_ENLO0)
	MTC0    (r0, C0_ENLO1)

	MTC0    (r0, C0_ENHI)
	nop
	tlbwr
	nop
#elif TFP
	MTC0	(r0, C0_ENLO)
	MTC0	(r0, C0_ICACHE)
	MTC0	(r0, C0_ENHI)
   	TLB_WRITER
#endif
	LW	t2, U_PID(CURPROC)
#ifdef TFP
	dsll	t2, TLBHI_PIDSHIFT
#endif

	MTC0    (t2, C0_ENHI)
#ifdef TFP
	dsll	t2, t2, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
	MTC0	(t2, C0_ICACHE)
	dsrl	t2, t2, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
#endif

	nop
	j sys_return
	nop

nsys_scoher:
	# RPARM contains the address, RRET contains the new coherency bits
	SRL	RPARM, RPARM, PGSZ_BITS	# Shift address right PGSZ bits.
	LW	t1, U_PGTBL(CURPROC)	# Get page table address
	SLL	RPARM, RPARM, 3		# Size of pg table entries (8 bytes)
	ADD	t1, RPARM, t1		# Get PTE address
	LW	t0, 0(t1)		# Get PTE
	LI	MVPC, ~(7 << TLBLO_CSHIFT)	# Load cache bit mask
	and	t0, MVPC, t0			# Mask cache bits off
	SLL	RRET, RRET, TLBLO_CSHIFT	# Shift bits left 3
	or	t0, RRET, t0			# Or in new cache bits
	SW	t0, 0(t1)			# Write it back

#ifdef R4000
/* MARGIE This seems wrong, even for R4000.  Ask Stever.  FIX */
	SRL	t0, RPARM, 1			# Compute VPN / 2
	SRL	t0, t0, TLBHI_VPNSHIFT
#elif TFP
    	SLL 	RPARM, PGSZ_BITS - 3
#endif
	LW	t1, U_PID(CURPROC)		# GET PID
#ifdef TFP
	dsll	t1, TLBHI_PIDSHIFT
#endif
	or	t0, t1, t0			# Or in ASID


	MTC0	(t0, C0_ENHI)			# Put 'er in ENHI
#ifdef TFP
	dsll	t0, t0, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
	MTC0	(t0, C0_ICACHE)
	dsrl	t0, t0, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
#endif

	nop
	TLB_PROBE

	nop
	LW	t2, U_PID(CURPROC)
/* MARGIE This looks wrong.  Should be C0_INX? FIX */
	MFC0	(t0, t2)		 		# Get index register
	nop
	bltz	t0, noinval
	nop

#ifdef R4000
	MTC0	(zero, C0_ENHI)
	MTC0	(zero, C0_ENLO0)
	MTC0	(zero, C0_ENLO1)
	nop
	tlbwi
	nop
	MTC0	(t2, C0_ENHI)
	nop
	nop
	nop
	nop
#elif TFP
	MTC0	(zero, C0_ENHI)
	MTC0	(zero, C0_ENLO)
	TLB_WRITER
	MTC0	(t2, C0_ENHI)
	dsll	t2, t2, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
	MTC0	(t2, C0_ICACHE)
	dsrl	t2, t2, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
#endif


noinval:
	j sys_return
	nop
	# Might want to mess with cache here.
nsys_gpid:
	LW	RPARM, U_PID(CURPROC)
	j sys_return
	nop

nsys_gcount:
	LW	t2, P_ICNT(PRIVATE)
	or	RPARM, zero, t2		# Copy I-count to r2
	j sys_return
	nop

nsys_gshared:
	LW	RPARM, U_SHARED(CURPROC)
	j sys_return
	nop

nsys_vtop:
	# Put EPC back in case we take a miss on the LW below
	DMFC0(t0, C0_EPC)
	daddi	t0, -4
	DMTC0(t0, C0_EPC)
	SRL	t0, RPARM, PGSZ_BITS		# Get VPN
	SLL	t0, t0, 3		# Get PTE offset
	LI	t1, PTEBASE		# Load page table base
	ADD	t1, t0, t1	# Add offset to PTEBASE
	LW	t0, 0(t1)		# Get PTE
	nop
	SRL	t0, t0, PFNSHIFT	# Get PFN
	SLL	t0, t0, PGSZ_BITS	# Physical addr for page base
	andi	t1, RPARM, OFFSET_MASK	# Get offset into page
	ADD	RPARM, t0, t1		# Physical address
	DMFC0(t0, C0_EPC)
	daddi	t0, 4
	DMTC0(t0, C0_EPC)
	j	sys_return
	nop

nsys_wpte:
	SLL 	t0, RPARM, 3			# Get PTE offset (VPN * 8)
	LI 	t1, PTEBASE		# Load page table base
	ADD 	t1, t0, t1	# Add offset to PTEBASE
	SW 	RRET, 0(t1)
	j 	sys_return
	nop
nsys_suspnd:
	MFC0	(t0, C0_SR)			# Get current status register value
	LI	t1, ~SR_COUNTS_BIT		# Turn the COUNT/COMPARE timer bit off.
	and	t0, t1, t0	# Mask off Interrupt mask bit 8
	MTC0	(t0, C0_SR)			# store it in the status register
	nop
	nop
	nop
	nop
	j	sys_return
	nop
nsys_resume:
	MFC0	(t0, C0_SR)
	LI	t1, SR_COUNTS_BIT	# Timer interrupt bit
	or	t0, t1			# Turn interrupt mask bit 8 on
	MTC0	(t0, C0_SR)		# store it in the status reg.
	nop
	nop
	nop
	nop
	j	sys_return
	nop

nsys_gecount:
	SLL	RPARM, RPARM, WORDSZ_SHIFT	# Get word offset from exception number
	ADD	t0, RPARM, CURPROC	# Get address to add EXCCNTBASE to
	LW	RPARM, U_EXCCNTBASE(t0)	# get current count
	j	sys_return
	nop

nsys_cecount:
	SLL	RPARM, RPARM, WORDSZ_SHIFT	# Get word offset from exception number
	ADD	t0, RPARM, CURPROC	# Get address to add EXCCNTBASE to
	SW	zero, U_EXCCNTBASE(t0)	# clear current count
	j	sys_return
	nop

nsys_steale:
	SLL	RPARM, RPARM, WORDSZ_SHIFT	# Get word offset from exception number
	ADD	t0, RPARM, CURPROC	# Get address to add to EXCTBLBASE
	SW	RRET, U_EXCTBLBASE(t0)	# Store new vector
#ifdef R4000
	SRL	t1, RRET, PGSZ_BITS + 1	# Get the page number / 2
	SLL	t1, t1, 13		# shift into place for ENHI
#elif TFP
	andi	t1, RRET, OFFSET_MASK	# Mask out lower 12 bits of vaddr
#endif
	LW	t2, U_PID(CURPROC)
	or	t1, t1, t2		# or in ASID
	SW	t1, U_EXCENHIBASE(t0)	# Store it away for comparison
	j	sys_return
	nop

nsys_steali:
	SLL	RPARM, RPARM, WORDSZ_SHIFT	# Get word offset from interrupt number
	LA	t0, intrvect		# Get interrupt data area base
	ADD	t0, RPARM, t0		# Get address to add I_VECTOR to
	SW	RRET, I_VECTOR(t0)	# Store new vector
	LW	t2, U_PID(CURPROC)
	SW	t2, I_ASID(t0)		# Store current process's ASID
#ifdef R4000
	SRL	t1, RRET, PGSZ_BITS + 1	# Get the page number / 2
	SLL	t1, t1, 13		# shift into place for ENHI
#elif TFP
	andi	t1, RRET, OFFSET_MASK	# Mask out lower 12 bits of vaddr
#endif
	or	t1, t2			# or in ASID

	SW	t1, I_ENHI(t0)		# Store it away for comparison
	j	sys_return
	nop

nsys_csout:
	SW	RPARM, U_CSOUT(CURPROC)		# Store new vector
#ifdef R4000
	SRL	t1, RPARM, PGSZ_BITS + 1	# Get the page number / 2
	SLL	t1, t1, 13			# shift into place for ENHI
#elif TFP
	andi	t1, RPARM, OFFSET_MASK	# Mask out lower 12 bits of vaddr
#endif
	LW	t2, U_PID(CURPROC)
	or	t1, t1, t2			# or in ASID
	SW	t1, U_OUTENHI(CURPROC)  	# Store it away for comparison
#ifdef CSDEBUG
	LI	t0, 0x0c50
	PRINT_HEX(t0)
	PRINT_HEX(r2)
	PRINT_HEX(t1)
	PRINT_HEX(CURPROC)
#endif
	j	sys_return
	nop

nsys_csin:
	SW	RPARM, U_CSIN(CURPROC)		# Store new vector
#ifdef R4000
	SRL	t1, RPARM, PGSZ_BITS + 1	# Get the page number / 2
	SLL	t1, t1, 13			# shift into place for ENHI
#elif TFP
	andi	t1, RPARM, OFFSET_MASK	    	# Mask out lower 12 bits of vaddr
#endif
	LW	t2, U_PID(CURPROC)
	or	t1, t2				# or in ASID
	SW	t1, U_INENHI(CURPROC)  		# Store it away for comparison
#ifdef CSDEBUG
	LI	t0, 0x0c51
	PRINT_HEX(t0)
	PRINT_HEX(RPARM)
	PRINT_HEX(t1)
	PRINT_HEX(CURPROC)
#endif
	j	sys_return
	nop

nsys_mode2:
	MFC0	(t0, C0_SR)
	LI	t1, ~SR_UX
	and	t0, t0, t1
	MTC0	(t0, C0_SR)
	# Clear MIPS3 flag for user code
	j sys_return
	nop

nsys_mode3:
	MFC0	(t0, C0_SR)
	LI	t1, SR_UX
	or	t0, t0, t1
	MTC0	(t0, C0_SR)
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
	NIBDEBUG_MSG(M_ENTER_TLBL)
	NIBDEBUG_MSG_COP0(M_CAUSE_IS, C0_CAUSE);
	NIBDEBUG_MSG_COP0(M_SR_IS, C0_SR);
	NIBDEBUG_MSG_COP0(M_COUNTS_IS, C0_COUNT);
	NIBDEBUG_MSG_COP0(M_EPC_IS, C0_EPC);
	NIBDEBUG_MSG_COP0(M_ENHI_IS, C0_ENHI);
	NIBDEBUG_MSG_COP0(M_VADDR_IS, C0_BADVADDR);

	MFC0	(k0, C0_BADVADDR)
	# Check the BAD_VADDR.  If it ain't above ff800000, 
	#	either "page in" or segv!
	LI	t0, PTEBASE
	sltu	t1, k0, t0	# Is BADVADDR < PTEBASE?
	bne	t1, r0, badaddr
   	nop	    	    	# LI can't fit in delay slot

#ifdef R4000
	LI	t0, PGSZ_MASK 	# Mask for 4k pages
	MTC0	(t0, C0_PAGEMASK)	# Load the page mask
#if 0
	# Why were we changing the EPC anyway?
	MTC0	(k1, C0_EPC)
#endif 
	LI	t1, 0x1fffffff
	LW	t2, U_PGTBL(CURPROC)
	or	t0, t2, zero
	and	t0, t0, t1
	SRL	t0, t0, (PGSZ_BITS)
	or	t1, r0, t0
	SRL	t1, t1, 1	# Divide by 2
	SLL	t1, t1, 13	# Shift into place

	SLL	t0, t0, TLBLO_PFNSHIFT
	LI	t1, ((TEXT_COHERENCY << TLBLO_CSHIFT) | TLBLO_VMASK | TLBLO_DMASK);
					# Set up a TLB Entry for
					# a valid page at ff800000,
					# cacheable, exclusive on write,
					# dirty
	or	t0, t0, t1	
	MTC0	(t0, C0_ENLO0)		# Move it to entry low 0

	ADDI	t1, t0, (1 << TLBLO_PFNSHIFT)	# Put PFN 1 into ENLO1
	MTC0	(t1, C0_ENLO1)		# Validate the paired page

	LI	t0, PTEBASE		# Load the vaddr of the PTE Base
	LW	t2, U_PID(CURPROC)
	or	t0, t2			# Load the ASID 


	MTC0	(t0, C0_ENHI)		# Put 'er in the TLB Hi register
#ifdef WIRED_ENTRIES
	MTC0	(t2, C0_INX)		# Put our ASID in the index
	nop				# Need nop between set-up and write
	tlbwi
#else
	tlbwr
#endif

#else 
	
	LW	t2, U_PGTBL(CURPROC)
/* added */
	dsub	t1, k0, t0   # t1 = BADVADDR - PTEBASE
	dadd	t2, t1
/* */
	LI	t1, 0x07ffffffffffffff
	and	t2, t2, t1
	SRL	t2, t2, (PGSZ_BITS)
	SLL	t2, t2, TLBLO_PFNSHIFT

	# Set up TLB Entry for valid page at ff800000, cacheable, exclusive on write, dirty
	LI	t1, ((TEXT_COHERENCY << TLBLO_CSHIFT) | TLBLO_VMASK | TLBLO_DMASK);
	or	t2, t2, t1	
   	MTC0	(t2, C0_ENLO)

	SRL	k0, TLBHI_VPNSHIFT
	SLL k0, TLBHI_VPNSHIFT
	LW	t2, U_PID(CURPROC)
	dsll	t2, TLBHI_PIDSHIFT
	or	k0, t2			# Load the ASID 
#if WRONG
	MTC0	(t0, C0_ENHI)		# Put 'er in the TLB Hi register
#endif
	MTC0	(k0, C0_ENHI)		# Put 'er in the TLB Hi register
	dsll	t2, t2, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
	MTC0	(t2, C0_ICACHE)
	dsrl	t2, t2, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
   	TLB_WRITER
	NIBDEBUG_MSG_COP0(M_VADDR_IS, C0_BADVADDR)
	NIBDEBUG_MSG_COP0(M_ENHI_IS, C0_ENHI)
	NIBDEBUG_MSG_COP0(M_ENLO_IS, C0_ENLO)
	NIBDEBUG_MSG(M_WROTE_TO_TLB)
#endif

	ERET
	nop
badaddr:
	
	NIBDEBUG_MSG(M_ENTER_BADADDR)
	NIBDEBUG_MSG_COP0(M_VADDR_IS, C0_BADVADDR);
	# Bad virtual address
	# k0 contains the bad virtual address.
	NIBERROR_MSG_REG(M_REG_IS, k0)  # XXX remove
	SRL	t0, k0, PGSZ_BITS		# Get the page number
	LI	t1, PTEBASE			# Get the page table base
	NIBERROR_MSG_REG(M_REG_IS, t1)  # XXX remove
	SLL	t0, t0, 3			# Each PTE is two words
	ADD	t1, t1, t0			# PTE Addr
	NIBERROR_MSG_REG(M_REG_IS, t1)  # XXX remove
	LW	k0, 0(t1)			# Get the PTE and put it in k1
	NIBERROR_MSG_REG(M_REG_IS, k0)  # XXX remove
	nop
	and	t1, k0, TLBLO_VMASK		# Is it valid?
	NIBERROR_MSG_REG(M_REG_IS, t1) # XXX remove
	beq	t1, r0, invalidpage
	SLL	t0, t0, 9
	LW	t1, U_PID(CURPROC)		# Get our PID (ASID)
#ifdef TFP
	dsll	t1, TLBHI_PIDSHIFT
#endif
	or	t0, t0, t1

	MTC0	(t0, C0_ENHI)
#ifdef TFP
	dsll	t0, t0, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
	MTC0	(t0, C0_ICACHE)
	dsrl	t0, t0, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
#endif

	nop

	TLB_PROBE
	NIBDEBUG_MSG(M_ENTER_PROBE_BADADDR)
	NIBDEBUG_MSG_COP0(M_ENHI_IS, C0_ENHI)
	NIBDEBUG_MSG_COP0(M_ENLO_IS, C0_ENLO)
	NIBDEBUG_MSG_COP0(M_TLBSET_IS, C0_TLBSET)
	nop
	MFC0	(t0, C0_INX)
	nop
	bltz	t0, clobbered 	# If not found (high bit set) branch
	nop

#ifdef R4000
	MFC0    (k0, C0_CTXT)
	nop
	LW	t0, 0(k0)
	LW      k0, 8(k0)               
	nop
	MTC0    (t0, C0_ENLO0)
	MTC0    (k0, C0_ENLO1)
	nop
	tlbwi
	nop
#elif TFP
	NIBDEBUG_MSG(M_BADADDR_FOUND_INVALID_IN_TLB)
	MFC0	(t0, C0_BADVADDR)
	nop
	MFC0	(k0, C0_SHIFTAMT)
	nop
	dsrl	t0, t0, k0
	dsll	t0, t0, 3
	MFC0	(k0, C0_UBASE)  /* MARGIE Could be PTEBASE?  FIX */
	dadd	k0, t0
	ld		k0, 0(k0)
	NIBDEBUG_MSG_COP0(M_VADDR_IS, C0_BADVADDR)
	NIBDEBUG_MSG_COP0(M_ENHI_IS, C0_ENHI)
	NIBDEBUG_MSG_COP0(M_ENLO_IS, C0_ENLO)
	NIBDEBUG_MSG(M_WROTE_TO_TLB)
	MTC0	(k0, C0_ENLO)
	TLB_WRITER
#endif
	j 	sys_return
	nop
clobbered:
	NIBDEBUG_MSG(M_BADADDR_NOT_FOUND_IN_TLB)
	NIBDEBUG_MSG_COP0(M_TLBSET_IS, C0_TLBSET)

#ifdef R4000
	MFC0    k0, C0_CTXT
	nop
	LW	t0, 0(k0)
	LW      k0, 8(k0)               
	nop
	MTC0    (t0, C0_ENLO0)
	MTC0    (k0, C0_ENLO1)
	nop
	tlbwr
	nop
#elif TFP
	NIBDEBUG_MSG_COP0(M_VADDR_IS, C0_BADVADDR)
	MFC0	(t0, C0_BADVADDR)
	nop
	MFC0	(k0, C0_SHIFTAMT)
	nop
	dsrl	t0, t0, k0
	dsll	t0, t0, 3
	MFC0	(k0, C0_UBASE)  /* MARGIE Could be PTEBASE?  FIX */
	dadd	k0, t0
	ld		k0, 0(k0)
	NIBDEBUG_MSG_COP0(M_VADDR_IS, C0_BADVADDR)
	NIBDEBUG_MSG_COP0(M_ENHI_IS, C0_ENHI)
	NIBDEBUG_MSG_REG(M_ENLO_IS, k0)
	NIBDEBUG_MSG(M_WROTE_TO_TLB)
	MTC0	(k0, C0_ENLO)
	TLB_WRITER
#endif
	j 	sys_return
	nop

invalidpage:
	LW      t1, U_PID(CURPROC)	# Pid of process that generated bad page ref
	NIBERROR_MSG(M_BADADDR_NOT_FOUND_IN_PAGETABLE)
	NIBERROR_MSG_COP0(M_EPC_IS, C0_EPC)
	NIBERROR_MSG_COP0(M_VADDR_IS, C0_BADVADDR)
	NIBERROR_MSG_REG(M_PID_IS, t1)

	nop

	FAIL(NIBFAIL_BADPAGE)
	nop

notexc:
	MFC0	(k1, C0_CAUSE)		# Get cause register value
	nop
	MFC0	(t0, C0_SR)		# Get the status register	
	LI	k0, CAUSE_COUNTS_BIT	# Timer interrupt
	and	k1, t0, k1		# Mask off disabled interrupts
	and	t0, k1, k0		# Select timer interrupt bit
	beq	t0, r0, notsched	# Compare against timer interrupt
	nop

	# Jump away to notsched to increase locality for sched.
sched:
	NIBDEBUG_MSG(M_SCHED);
	NIBDEBUG_MSG_COP0(M_CAUSE_IS, C0_CAUSE)
	NIBDEBUG_MSG_COP0(M_SR_IS, C0_SR)
	NIBDEBUG_MSG_COP0(M_COUNTS_IS, C0_COUNT)
	NIBDEBUG_MSG_COP0(M_EPC_IS, C0_EPC)
#ifdef FIXED
	LW	k0, U_CSOUT(CURPROC)
	nop
	bne	k0, r0, cs_out
	nop
#endif /* FIXED */
sys_save:
	LW	MVPC, P_MVPC(PRIVATE)
	SAVE_REST(CURPROC)
	SW	MVPC, U_PC(CURPROC)

schedrunlock:
lock4:
#ifdef LOCKS
	RUNLOCK
#endif
	LA	t0, rlockcheck		# Get the lock check address
	LW	t1, 0(t0)		# Get the value.
	LA	t2, runqueue		# Get addr of run queue pointer (LD)
#ifdef LOCKCHECK
	bne	t1, zero, mpfail1	# If it's not zero, locking has failed!
	nop
#endif
	LW	t2, 0(t2)		# t2 = Run queue pointer (BD)
/*	LW	t1,  */  /* removed this - what was it?? */

	LW	t1, U_PID(CURPROC)		# Get our PID (ASID)
	ori	t1, 0x0200

	SW	t1, 0(t0)		# Put a nonzero # in rlockcheck.

	# Need to add code to handle no one wanting to run and no CURPROC.
	LW	t1, P_SCHCNT(PRIVATE)
	nop
	ADDI 	t1, 1			# Add one to the scheduling count
	SW	t1, P_SCHCNT(PRIVATE)
	bne 	t2, r0, traverse	# If anyone wants to run, traverse.
	ADDU 	t0, t2,r0 			# Copy RUNQ to scratch ptr. (BD)

	# Release the lock and continue.
#ifdef LOCKCHECK
	LA	t1, rlockcheck
	SW	r0, 0(t1)		# Zero lock check.
#endif /* LOCKCHECK */
unlock4:
#ifdef LOCKS
	UNRUNLOCK
#endif
	
	j 	goon			# Go on.


traverse:				# Traverse the RUNQ
	or	t1, t0, zero		# Save proc ptr.
	LW 	t0, U_NEXT(t0) 		# Get ptr to next proc in queue.
	nop				# Wait for NEXT to be available
	beq 	t0, zero, atend
	nop
	j traverse
	nop
atend:
	SW	CURPROC, U_NEXT(t1)
	SW	r0, U_NEXT(CURPROC)
runnewproc:
	LA	t2, runqueue
	LW	CURPROC, 0(t2)		# old run queue => new curproc
	nop				# (BD)
	LW	MVPC, U_PC(CURPROC)	# Get PC of new process
	LW	t2, U_NEXT(CURPROC)	# Get new run queue address
	LA	t1, runqueue		# Get run queue pointer address
	SW	t2, 0(t1)		# Store run queue pointer
	SW	CURPROC, P_CURPROC(PRIVATE)	# Store new CURPROC
	LA	t1, rlockcheck		# Get lock check address
	SW	r0, 0(t1)		# Zero lock check.
unlock5:
#ifdef LOCKS
	UNRUNLOCK
#endif
	MTC0	(MVPC, C0_EPC)		# Load the EPC with the addr we want
	nop				# Possible bug fixer
sys_restore:
	RESTORE_REST(CURPROC)
	LW	t1, U_PID(CURPROC)
#ifdef TFP
	dsll	t1, TLBHI_PIDSHIFT
#endif
	MTC0	(t1, C0_ENHI)		# Load current process ID.
#ifdef TFP
	dsll	t1, t1, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
	MTC0	(t1, C0_ICACHE)
	dsrl	t1, t1, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
#endif

#ifdef FIXED
	LW	t1, U_CSIN(CURPROC)
	nop
	bne	t1, zero, cs_in
	nop
#endif
state_restored:	
goon:

#ifdef PRINTSCHED
	LW	t2, U_PID(CURPROC)
	lui	t1, 0x5ced
	or	t1, t1, t2
	PRINT_HEX(t1)			# Print PID with 0x5ced000 prefix.
#endif /* PRINTSCHED */

	MFC0	(t1, C0_COUNT)		# Get COUNT
/* MARGIE just added */
#ifdef TFP
	dli	t2, QUANTUM
	dsub t1, t1, t2
#endif
	LW	t2, P_ICNT(PRIVATE)	# Get cycle count 
	ADDU	t2, t1, t2		# Add to current count
	SW	t2, P_ICNT(PRIVATE)	# Store the cycle count
	SET_LEDS_R(t2)

	LW	t0, U_QUANT(CURPROC)	# Get the process' time quantum
#ifdef R4000
	MTC0	(zero, C0_COUNT)		# Zero the count register
	nop
	MTC0	(t0, C0_COMPARE)		# Set the compare register
#elif TFP
    	MTC0	(t0, C0_COUNT)
#endif
	ld	k0, U_PID(CURPROC)
	ERET				# Restore from exception in delay slot
	nop

notsched:
	# k0 contains CAUSE_IP8
	# k1 contains the contents of C0_CAUSE masked with SR
	# This code assumes that CAUSE_IPMASK == SR_IMASK.
	MFC0	(t2, C0_SR)		# Get the status register	
	/* Mask C0_CAUSE (already masked with SR) with IP mask bits of SR */
	LI	t1, SR_IMASK		# Load interrupt mask
	and	t0, t1, t2		# Interrupt mask only
	and	t0, k1, t0		# Interupts enabled _and_ pending

	LI	k1, CAUSE_SW1		# Start here.
	and	t1, k1, t0		# Is interrupt 1 pending?
i_test:	
	bne	t1, zero, foundone	# If != 0, interrupt is active
	SLL	k1, k1, 1		# Try the next interrupt
	bne	k1, k0, i_test		# If not up to 8, do another
	and	t1, k1, t0		# And active ints with current


itest_fail:
	# Shouldn't get here - means bad interrupt or timer...
	# Keeep these messages - helpful for debug
	NIBERROR_MSG(M_UNEXPECTED_INTERRUPT)
	NIBERROR_MSG_COP0(M_EPC_IS, C0_EPC)
	NIBERROR_MSG_COP0(M_VADDR_IS, C0_BADVADDR)
	NIBERROR_MSG_COP0(M_CAUSE_IS, C0_CAUSE)
	NIBERROR_MSG_COP0(M_SR_IS, C0_SR)
	
	FAIL(NIBFAIL_TIMER)
		
foundone:
	SRL	k1, k1, 1
	# k1 contains the CAUSE_IPx value shifted left by one.
	SRL	k1, k1, (CAUSE_IPSHIFT - 1)	# Result will be the interrupt
						# number shifted left two.
	LI	t1, CAUSE_IP3 >>(CAUSE_IPSHIFT - 1)
	beq	k1, t1, everest_zero		# If it's IP3, go to ev hndlr
	nop					# (BD)
#ifdef TIMEOUT
	LI	t1, CAUSE_IP4 >> (CAUSE_IPSHIFT - 1)
	beq	k1, t1, nib_timeout		# If it's IP4, go to timeout
	nop
#endif
	/* SET_LEDS(14) */
keep_checking:
	LA	t1, intrvect
	ADD	t0, k1, t1			# Address of handler
	LW	k0, I_VECTOR(t0)		# Get handler vector
	beq	k0, r0, intfail
	LW	t1, I_COUNT(t0)			# Get count
#ifdef TFP
	dsll	t2, TLBHI_PIDSHIFT
#endif
	MTC0	(t2, C0_ENHI)			# Install ASID
#ifdef TFP
	dsrl	t2, TLBHI_PIDSHIFT
	dsll	t2, t2, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
	MTC0	(t2, C0_ICACHE)
	dsrl	t2, t2, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
#endif
	ADDI	t1, t1, 1			# Increment count
	SW	t1, I_COUNT(t0)			# Store new count
	MFC0	(MVPC, C0_EPC)			# Get EPC
	# Make sure this sucker's in the TLB.
	# ############################################################
	# k0 contains the user routine virtual address.
	LW	t1, I_ENHI(t0)		# Get ENHI for this vector
	nop
#ifdef WIRED_ENTRIES
	MTC0	(t1, C0_ENHI)
#ifdef TFP
	dsll	t1, t1, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
	MTC0	(t1, C0_ICACHE)
	dsrl	t1, t1, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
#endif

	nop
	TLB_PROBE	
	nop
	MFC0	(t0, C0_INX)		# Get index register
	nop
	bgez	t0, intintlb		# If present, go on
	LI	k1, PTEBASE		# Get page table base
	SRL	t0, k0, PGSZ_BITS + 1	# Get VPN / 2
	SLL	t0, t0, 4
	ADD	t0, t0, k1		# Get PTE address
	LW      k1, 0(t0)		# Load first mapping
	LW      t0, 8(t0)		# load the second mapping
	nop
	MTC0	(t1, C0_ENHI)
#ifdef TFP
	dsll	t1, t1, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
	MTC0	(t1, C0_ICACHE)
	dsrl	t1, t1, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
#endif

	MTC0    (k1, C0_ENLO0)   /* MARGIE FIX */
	MTC0    (t0, C0_ENLO1)
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
	LA	R_VECT, int_return
	j	k0				# Jump to user handler.
	nop
int_return:
	LW	t1, U_PID(CURPROC)		# Load old ASID back in
	MTC0	(MVPC, C0_EPC)			# Restore EPC

#ifdef TFP
	dsll	t1, TLBHI_PIDSHIFT
#endif
	MTC0	(t1, C0_ENHI)			# Install ASID
#ifdef TFP
	dsll	t1, t1, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
	MTC0	(t1, C0_ICACHE)
	dsrl	t1, t1, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
#endif
	ERET

everest_zero:
	/* SET_LEDS(13) */

	LI	t0, EV_HPIL		# Go fetch the HIPL.
	ld	t0, 0(t0)
	
	LI	t1, PASS_LEVEL
	beq	t1, t0, pass_exit	# Supertest passed!
	nop

	LI	t1, FAIL_LEVEL
	beq	t1, t0, fail_exit	# Supertest failed :-(
	nop

	b	keep_checking
	nop

memerr:
	LI	k0, MIND2OFF(M_BADMEM)
	LA	t1, msgs
	ADD	k0, t1, t0
	STRING_DUMP(t0)
	PRINT_HEX(CURPROC)
	PRINT_HEX(t0)
	FAIL(NIBFAIL_MEM)
	nop

intfail:
	LI	t0, MIND2OFF(M_BADINT)
	LA	t1, msgs
	ADD	t0, t1, t0
	STRING_DUMP(t0)
	MFC0	(t0, C0_CAUSE)
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

	dli	a0, EV_ERTOIP		# clear all pending interrupts
	sd	a1, 0(a0)

	dli	a0, EV_CERTOIP		# clear all pending interrupts
	li	a1, 0x3fff
	sd	a1,0(a0)

#ifdef RAW_NIBLET
	jal ccuart_init
	nop
#endif

/* Set up the kernel structures, timer interrupt, timer, compare register,
		possibly page tables and TLB */

	# Get our CPU number.
	GETCPUNUM(t2)



	GET_STARTUP_DATA_LOC(CURPROC)	
	LW	CURPROC, STUP_PROCTBL(CURPROC)

	GET_STARTUP_DATA_LOC(a0)
	LW	a0, STUP_NCPUS(a0)

	LA	a1, sync_addr

		# Tell dcob we've reached the barrier
	LI	k0, EV_SPNUM
	ld	k0, 0(k0)		# Get slot/processor number
	LA	k1, directory		# Addresses of private data areas (LD)
	andi	k0, EV_SPNUM_MASK	# Mask off extraneous stuff
	SLL	k0, DIR_SHIFT		# Shift by log2(dir ent size)
	ADD	k1, k0			# k1 = address of our pointers
	LI	k0, 1
	SW	k0, DIR_BAROFF(k1)	# Write the marker.


#ifdef TFP
	MFC0	(k1, C0_CONFIG)
	LI		k0, CONFIG_ICE
	or		k0, k1
	MTC0	(k0, C0_CONFIG)
#endif

	# Synchronize a0 processors with a barrier lock located at a1
	TIMED_BARRIER(a1, a0, BARRIER_TIMEOUT)

	beqz	t1, 1f
	nop				# (BD)
	
	# If we get here, the barrier timed out.
	# Tell dcob where the directory is.
	# It's okay for all non-hanging CPUs to do this.

	GET_STARTUP_DATA_LOC(t0)
	LA	t1, directory
	SW	t1, STUP_DIRECTORY(t0)	

	# Return to dcob.
	j	ra
	ori	v0, zero, NIBFAIL_BARRIER	# (BD)

1:
	MFC0	(MVPC, C0_SR)		# Stash away the SR
	nop
	nop
	nop
	nop
	LI	t0, NIB_SR & ~(SR_USER)
	MTC0	(t0, C0_SR)	# Initialize SR

	LI	t0, PTEBASE	# Load PTEBASE into Context register
#ifdef R4000
	MTC0	(t0, C0_CTXT	)
	nop
#elif TFP
   	MTC0 	(t0,C0_UBASE)
   	/* TLB Refill handler sits at a800000000000000 */
/*    	dla 	t0, 0xa800000000000000 */ /* changed for system! */
#ifdef RAW_NIBLET
	dla		t0, TLBREFILL
#else
   	dla 	t0, TLB_REFILL_HANDLER_LOC
#endif
   	MTC0	(t0, C0_TRAPBASE)
#endif


/* MARGIE GO back and FIX this up.  Not ported to Twin Peaks */
#ifdef R4000
#ifdef CLEARTLB
	LI	t1, NTLBENTRIES - 1
	LI	t0, 0x80000000

	MTC0	(t0, C0_ENHI)
	MTC0	(zero, C0_ENLO0)
	MTC0	(zero, C0_ENLO0)
	MTC0	(zero, C0_ENLO1)
tlbloop:
	MTC0	(t1, C0_INX)		# Change index register
	nop
	nop
	nop
	nop
	tlbwi
	nop; nop; nop; nop

	bnez	t1, tlbloop	
	ADDI	t1, -1			# (BD)
#endif /* CLEARTLB */
#elif TFP
#endif


	# Clear level 1, 2, 4 interrupts
/* MARGIE Check if correct for Twin Peaks FIX */
	LI	t0, EV_CIPL124
	LI	t1, CLEAR_INTERRUPTS			# 1 | 2 | 4  (just 1 | 2 on TFP)
	sd	t1, 0(t0)

        LI      t0, EV_CIPL0
        LI      t1, EV_CELMAX		# Highest interrupt number

clear_ints:
        sd      t1, 0(t0)		# Clear interrupt (BD)
        bne     t1, zero, clear_ints
        ADDI    t1, -1			# Decrement counter (BD)


	LI	t0, EV_ILE		# Interrupt Level Enable register
	LI	t1, 1 | 8
	sd	t1, 0(t0)		# Enable Everest level 0 interrupts


lock6:
#ifdef WIRED_ENTRIES
	LI	t0, WIRED_TLB
#else
	LI	t0, 0
#endif /* WIRED_ENTRIES */

	MTC0	(t0, C0_WIRED)	# Set up wired TLB entries for us.
	nop
#if R4000
	LI	t0, PGSZ_MASK	# Page size determined in niblet.h
	MTC0	(t0, C0_PAGEMASK)		# Set TLB for 4k pages
	nop
#endif
#if 0
	LI	t0, CONFIG_BITS # Algorithm specified in niblet.h
	MTC0	(t0, C0_CONFIG)   # Set the config coherency algorithm
	nop
	nop
#endif /* 0 */

#ifdef MP
tryilock:
	# Try to get the "initlock"
	LA	t0, initlock
	LL	t1, 0(t0)		# Load init lock value
	bne	t1, r0, missed		# If it's not zero, we missed it.
	ADDI	t1, t1, 1		# Add one to the value (0)
	SC	t1, 0(t0)		# Store the new value.
	beq	t1, r0, tryilock	# If there was contention, try again.
	nop	
#endif	/* MP */

	# *** We got the initlock ****


	# Print master CPU message

	GET_STARTUP_DATA_LOC(CURPROC)
	LW	CURPROC, STUP_PROCTBL(CURPROC)

	LA	PRIVATE, private	# Get address of first private data area
					# Doubleword align it
	ADDI	PRIVATE, 4		# Add four before masking off bits
	LI	t0, ~0x7
	and	PRIVATE, t0		# Turn off bottom three bits
	ADDU	t0, PRIVATE,r0		# Copy private address.
	ADDI	t0, PRIV_SIZE		# Get address of next private section
	LA	t1, next_private
	SW	t0, 0(t1)		# Store address of next private section

	LI	k0, EV_SPNUM
	ld	k0, 0(k0)		# Get slot/processor number
	LA	k1, directory		# Addresses of private data areas (LD)
	andi	k0, EV_SPNUM_MASK	# Mask off extraneous stuff
	SLL	k0, DIR_SHIFT		# log2(dir ent size)
	ADD	k1, k0			# k1 = address of our pointers
	SW	PRIVATE, DIR_PRIVOFF(k1)	# Store our private area ptr
	nop



	sd	ra, P_RA(PRIVATE)	# Save return address

	sd	sp, P_SP(PRIVATE)	# Save stack pointer

	SW	MVPC, P_SR(PRIVATE)	# Save status register!

	SW	zero, P_SCHCNT(PRIVATE)	# Reset sched count


	
	LI	t0, MIND2OFF(M_MASTER)
	LA	t1, msgs
	ADD	t0, t1, t0

	STRING_DUMP(t0)			# Print Master CPU message

	LW	t0, P_CPUNUM(PRIVATE)
	PRINT_HEX(t0)			# Print CPU Number
	NIBDEBUG_MSG_REG(M_CURPROC_IS, CURPROC)
	NIBDEBUG_MSG_REG(M_CURPROC_IS, PRIVATE)		# Print private storage area address


	LW	t0, U_MAGIC1(CURPROC)	# Load magic num 1 from data
	LI	t1, MAGIC1		# Load correct magic number 1
	bne	t0, t1, badendian	# Wrong endianess	
	LW	t0, U_MAGIC2(CURPROC)	# Load magic num 2 from data
	LI	t1, MAGIC2		# Load correct magic number 2
	bne	t0, t1, badendian2	# Wrong endianness
	nop				# Weird stuff was happening to this
	LW	t2, U_NEXT(CURPROC)	# Load the second process (if present)
					# t2 = Run queue pointer

	# Make sure that we don't jump to kernel space 
	LW	MVPC, U_PC(CURPROC)
	LI	t1, 0x1fffffff
	and	MVPC, MVPC, t1
	SW	MVPC, U_PC(CURPROC)
	
	# Store current status word into status slot in proc table
	LI 	t0, NIB_SR		# User mode on ERET!

	SW 	t0, U_STAT(CURPROC)
	LI	t3, 1			# Store 1 in number of processes

	beq	t2, r0, done		# Only one process
	ADDU	t1, t2,r0		# Copy runq to scratch in delay slot

store:
	# Make sure that we don't jump to kernel space 
	LW	MVPC, U_PC(t1)
	LI	r2, 0x1fffffff
	and	MVPC, MVPC, r2
	SW	MVPC, U_PC(t1)

	SW 	t0, U_STAT(t1)	# Put stat in proc tbl entry
	LW	t1, U_NEXT(t1)	# Get next entry
	nop
	
	bne	t1, r0, store 
	ADDI	t3, 1

done:

	LI	t0, MIND2OFF(M_NUMPROCS)	# number of processes active:
	LA	t1, msgs
	ADD	t0, t1, t0
	STRING_DUMP(t0)

	PRINT_HEX(t3)

	LA	t0, numprocs	# Get the address of the process count
	SW	t3, 0(t0)	# Store the process count
	LA	t0, runqueue	# Get address of run queue pointer
	SW	t2, 0(t0)	# Store run queue pointer

	MFC0	(t0, C0_COUNT)		# Get instruction count
	nop
	SW	t0, P_ICNT(PRIVATE)	# Store it.


	LW	MVPC, U_PC(CURPROC)	# PC for first process
	LW	t0, U_QUANT(CURPROC)	# Get process's time quantum
#ifdef DEBUG_NIBLET
	PRINT_HEX(MVPC)			# Print new EPC
#endif
	MTC0	(MVPC, C0_EPC)		# Set EPC
	nop
#ifdef R4000
	MTC0 	(r0, C0_COUNT)		# Zero the count register
	nop
	MTC0 	(t0, C0_COMPARE)	# Set the compare register
#elif TFP
	MTC0	(t0, C0_COUNT)
#endif

	LW	t1, U_PID(CURPROC)
	nop
#ifdef TFP
    	dsll	t1, TLBHI_PIDSHIFT
#endif
	MTC0	(t1, C0_ENHI)	# Load current process ID.
#ifdef TFP
	dsll	t1, t1, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
	MTC0	(t1, C0_ICACHE)
	dsrl	t1, t1, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
#endif

	nop

#ifdef MP
	LI	t0, 1		
	LA	t1, goahead	# Get address of goahead flag
	SW	t0, 0(t1)	# Store 1 in goahead.
#endif	/* MP */


	/* DEBUG STUFF to set up master's DEBUG base pointer and index pointer */
	/* Logic Analyzer test */
	dli r28, 0x9000000002000000
	ld	r28, 0(r28)
	dli	r28, MASTER_DEBUG_ADDR
	li	r30, 8
	dli	t0, 0xaaaaaaaaaaaaaaaa
	sd	t0, 0(r28)
	/* end of DEBUG STUFF */

	# No one can have set the XA (extended addressing) bit by now so...

	SW	CURPROC, P_CURPROC(PRIVATE)	# Store CURPROC
	NIBDEBUG_MSG(M_ERET_TO_TASK)
	RESTORE_REST(CURPROC)		# Restore CPU state from CURPROC 

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
	
	LA	t1, goahead
waitglock:
	LW	t0, 0(t1)
	nop
	beq	t0, zero, waitglock
	nop
	# Get run queue lock
	
startloop:
	RUNLOCK
	LA	t0, cpunum
/* Margie removed the #if 0.  Why was it there? */
/* #if 0 */
	LW	t3, 0(t0)		# t3 = CPUNUM
	nop
	ADDI	t3, 4			# CPUNUMs start at 0 and go by four
	SW	t3, 0(t0)
/* #endif */ /* 0 */
	LA	t1, next_private	# Get addr of private section ptr.
	LW	PRIVATE, 0(t1)		# Get addr of next private data area
	nop
	SW	t3, P_CPUNUM(PRIVATE)	# Store our CPUNUM
	ADDU	t0, PRIVATE,r0		# Copy private address.
	ADDI	t0, PRIV_SIZE		# Get address of next private section
	SW	t0, 0(t1)		# Store address of next private section
	sd	ra, P_RA(PRIVATE)	# Save our return address
	sd	sp, P_SP(PRIVATE)	# Save our stack pointer
	sd	MVPC, P_SR(PRIVATE)	# Stash away our SR

	LI	k0, EV_SPNUM
	ld	k0, 0(k0)		# Get slot/processor number
	LA	k1, directory		# Addresses of private data areas (LD)
	andi	k0, EV_SPNUM_MASK	# Mask off extraneous stuff
	SLL	k0, DIR_SHIFT		# log2(dir ent size)
	ADD	k1, k0			# k1 = address of our pointers
	SW	PRIVATE, DIR_PRIVOFF(k1)	# Store our private area
	nop


	LA	t0, rlockcheck		# Get the lock check address
	LW	t1, 0(t0)		# Get the value.
	LA	t2, runqueue		# Get addr of run queue pointer (BD)
#ifdef LOCKCHECK
	bne	t1, r0, mpfail2		# If it's not zero, locking has failed!
#endif
	LW	t2, 0(t2)		# Load run queue pointer (BD)

	LW	t1, P_CPUNUM(PRIVATE)	# Get CPUNUM
	ori	t1, 0x0300
	SW	t1, 0(t0)		# Put a nonzero #  in rlockcheck.

	/* 	For TFP initialize C0_COUNT or else when we get to runnewproc
		C0_COUNT will be less than quantum and when we execute
		C0_COUNT - quantum and put the result in P_ICNT(PRIVATE) we
		will end up putting a negative number in P_ICNT(PRIATE)
	*/
#ifdef TFP
	LW	t0, U_QUANT(CURPROC)	# Get process's time quantum
	MTC0	(t0, C0_COUNT)
#endif


/* DEBUG ertoip */
	li	t0, 0x5757
	PRINT_HEX(t0)
	LI	t0, EV_ERTOIP		# If we've detected a CC error, fail
	ld	t0, 0(t0)
	PRINT_HEX(t0)

	# Print CPU running message
	LI	t0, MIND2OFF(M_SLAVE)
	LA	t1, msgs
	ADD	t0, t1, t0
	STRING_DUMP(t0)
	PRINT_HEX(t3)			# Print our CPU number

	/* DEBUG STUFF to set up slave DEBUG base pointer and index pointer */
	
	dli	r28, SLAVE_DEBUG_ADDR
	li	r30, 8
	dli	t0, 0xbbbbbbbbbbbbbbbb
	sd	t0, 0(r28)
	/* end of DEBUG STUFF */

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

#ifdef R4000
	MTC0	(t1, C0_ENHI)
	nop
	TLB_PROBE	
	nop
	MFC0	(t0, C0_INX)			# Get index register
	nop
	bltz	t0, 1f				# If present, go on
	LI	k1, PTEBASE			# (BD) Get page table base
/* MARGIE Why is this t9?? FIX*/
	j	t9
	nop					# (BD)
1:
	SRL	t0, k0, PGSZ_BITS + 1		# Get VPN / 2
	SLL	t0, t0, 4
	ADD	t0, t0, k1			# Get PTE address
	LW      k1, 0(t0)			# Load first mapping
	LW      t0, 8(t0)             		# load the second mapping
	nop
	MTC0	(t1, C0_ENHI)
	MTC0    (k1, C0_ENLO0)
	MTC0    (t0, C0_ENLO1)
	nop
	tlbwr
	nop
#elif TFP
	MTC0	(t1, C0_ENHI)
	dsll	t1, t1, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
	MTC0	(t1, C0_ICACHE)
	dsrl	t1, t1, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
	MTC0	(t1, C0_BADVADDR)
	TLB_PROBE
	MFC0	(t0, C0_TLBSET)
	bltz	t0, 1f
	nop
   	j   	ra   /* this used to be j t9, but t9 didn't make any sense */
   	nop
1:
	MFC0	(k1, C0_SHIFTAMT)
	nop
	dsrl	t0, k0, k1
	dsll	t0, t0, 3
	dli	k1, PTEBASE
   	dadd	t0, t0, k1
	ld	k1, 0(t0)
   	MTC0	(t1, C0_ENHI)  /* MARGIE Do I need this?  Done above already?  FIX */
	dsll	t1, t1, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT
	MTC0	(t1, C0_ICACHE)
	dsrl	t1, t1, ICACHE_ASIDSHIFT - TLBHI_PIDSHIFT

	MTC0	(k1, C0_ENLO)
	TLB_WRITER
#endif

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

	LA	a0, crlf
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
	LA	a0, delim
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
	LI	t0, EV_SENDINT;
	LI	t1, PASS_LEVEL << 8;
	ori	t1, 127;
	sd	t1, 0(t0);
	LI	t0, EV_SPNUM;
	ld	zero, 0(t0);
	LI	t0, EV_CIPL0;
	LI	t1, PASS_LEVEL;
	sd	t1, 0(t0);
        END(pass_test)

/* This routine must immediately follow pass_test since it falls through */
LEAF(pass_cleanup)
	LI	t0, EV_ILE;
	sd	zero, 0(t0);
	LW	t0, P_SR(PRIVATE);
	MTC0	(t0, C0_SR);
	ld	ra, P_RA(PRIVATE);
	ld	sp, P_SP(PRIVATE);
	LI	v0, 0;
/* For now, use this to stop the sable simulatino in RAW mode */
#ifdef RAW_NIBLET
 	break	1023, 1009
#else

	# Synchronize a0 processors with a barrier lock located at a1
	GET_STARTUP_DATA_LOC(a0)
	LW	a0, STUP_NCPUS(a0)

	LA	a1, sync_addr2

	TIMED_BARRIER(a1, a0, BARRIER_TIMEOUT)
	j	ra;
#endif
	nop
	END(pass_cleanup)

LEAF(fail_test)
	ADDU	k1, CURPROC,r0		# Save the CURPROC pointer.
	LI	t0, EV_SENDINT
	LI	t1, FAIL_LEVEL << 8
	ori	t1, 127
	sd	t1, 0(t0)

	# This is just to make sure the interrupt gets out of the CC's queue
	LI	t0, EV_SPNUM
	ld	zero, 0(t0)

	LI	t0, EV_CIPL0
	LI	t1, PASS_LEVEL
	sd	t1, 0(t0)
	LW	t0, P_SR(PRIVATE)
	ADDU	a0, t3,r0			# Fail macro sets t3
 	jal	pul_cc_puthex
	nop
	LA	a0, crlf
 	jal	pul_cc_puts
	nop
	END(fail_test)

/* This routine must immediately follow fail_test since it falls through */
LEAF(fail_cleanup)
	LI	t0, EV_ILE
	sd	zero, 0(t0)
	LW	t0, P_SR(PRIVATE)
	MTC0	(t0, C0_SR)
	ld	ra, P_RA(PRIVATE)
	ld	sp, P_SP(PRIVATE)
	ADDU	v0, k0,r0
	LI	t0, NIBFAIL_GENERIC
	beq	t0, k0, 1f
	nop

	GET_STARTUP_DATA_LOC(t1)

	# If we originated the fail, store the reason
	SW	k0, STUP_WHY(t1)

	# Also store which CPU failed.
	LI	t0, EV_SPNUM
	ld	t0, 0(t0)
	andi	t0, EV_SPNUM_MASK
	SW	t0, STUP_WHO(t1)

	LI	t0, NIBFAIL_PROC
	bne	v0, t0, 1f
	nop

	LW	t0, U_PID(k1)		# Get the process ID of the failing
					# process.  k1 contains CURPROC.
	nop
	SW	t0, STUP_PID(t1)	# Store it.

1:
	# Synchronize a0 processors with a barrier lock located at a1
	GET_STARTUP_DATA_LOC(a0)
	LW	a0, STUP_NCPUS(a0)

	LA	a1, sync_addr2

	TIMED_BARRIER(a1, a0, BARRIER_TIMEOUT)
	j	ra
	nop
	END(fail_cleanup)


#ifdef RAW_NIBLET
/* This routine will have already gotten called by bootprom
   if we are running COOKED 
*/
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
#endif

/*
 * a0 = index of tlb entry to get
 * a1 = tlbset
 * a2 = pnumshft
 */
LEAF(get_enhi)
	.set	noreorder
	DMFC0(t0,C0_TLBHI)
	dsllv	a0,a2			# convert index into virtual address
	dli	v0, KV1BASE
	or	a0,v0
	DMTC0(a0,C0_BADVADDR)		# select correct index
	DMTC0(a1,C0_TLBSET)		# select correct set
	TLB_READ
	DMFC0(v0,C0_TLBHI)		# return value
	DMTC0(t0,C0_TLBHI)
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(get_enhi)

/*
 * a0 = index of tlb entry to get
 * a1 = tlbset
 * a2 = pnumshft
 */
LEAF(get_enlo)
	.set	noreorder
	DMFC0(t0,C0_TLBHI)
	dsllv	a0,a2			# convert index into virtual address
	dli	v0, KV1BASE
	or	a0,v0
	DMTC0(a0,C0_BADVADDR)		# select correct index
	DMTC0(a1,C0_TLBSET)		# select correct set
	TLB_READ
	DMFC0(v0,C0_TLBLO)		# return value
	DMTC0(t0,C0_TLBHI)
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(get_enlo)


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

/* MARGIE changed this from .lcomm to .byte because for some reason some of the
   .lcomms weren't showing up in the bss or sbss section.  This makes this data
   show up in the data section rather than bss or nbss and now bss and nbss are
   empty.
*/
directory:
.byte 0:DIR_ENT_SIZE * 64
/* The private data section is divided up and handed out by logical (Niblet)
 *	processor number.
 */
.align	3
private:
.byte 0:PRIV_SIZE * MAXCPUS + 8	# Private per-CPU storage.
/* The run queue pointer is kept here */
runqueue:
	.byte 0:8
runqlock:
	.byte 0:264
rlockcheck:
	.byte 0:264
initlock:
	.byte 0:264
sync_addr:
	.byte 0:8	# Initial barrier address
numprocs:
	.byte 0:8
goahead:
	.byte 0:8
cpunum:
	.byte 0:8 	# CPU numbers start with 0 and go by four...
next_private:
	.byte 0:8
flag:
	.byte 0:16
turn:
	.byte 0:8
sync_addr2:
	.byte 0:8	# Initial barrier address
/* MARGIE added the following ifdefs, but they haven't been checked
yet.  TFP has 11 interrupt bits and I believe in niblet there needs to
be one double word (in a 64 bit word) for each interrupt.  So in R4000
there needs to be space for 8 double words, but in TFP there needs to be space
for 11 double words, or 88 bytes */
intrvect:
#ifdef R4000
	.byte 0:64
#elif TFP
	.byte 0:88
#endif
intrcount:
#ifdef R4000
	.byte 0:64
#elif TFP
	.byte 0:88
#endif
intrasid:
#ifdef R4000
	.byte 0:64
#elif TFP
	.byte 0:88
#endif
intrenhi:
#ifdef R4000
	.byte 0:64
#elif TFP
	.byte 0:88
#endif

.lcomm dummy 8


#if 0
.lcomm	directory	DIR_ENT_SIZE * 64
/* The private data section is divided up and handed out by logical (Niblet)
 *	processor number.
 */
.align	3
.lcomm	private,	PRIV_SIZE * MAXCPUS + 8	# Private per-CPU storage.
/* The run queue pointer is kept here */
.lcomm	runqueue,	4
#if 0
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
#endif

/* Don't let strings get too long or you'll mess up alignment and get the
   wrong messages.  THe longest a string can be is:
	.asciiz "012345678901234567890123456789012345678901234567890123456789012"
*/
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
	.asciiz	"Slave CPU: "
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
MSG_ALIGN
/* 
   The following msgs are used mostly for debug, but some are
   also used for error messages.  It would be nice if we could
   #define these out when we are not running in debug mode (to
   save space).  However, since some are used for error messages
   this is not currently possible.  If you *do* #define them out,
   it will cause lots of problems because when the error messages
   try to print info, they will index off of the first message
   by a constant, trying to index into one of the messages below,
   but instead they will get some garbage that is farther down
   in the file!
*/
msg36:
	.asciiz "Enter nib_tlb_refill()\n"
MSG_ALIGN
msg37:
	.asciiz "Enter tlbl()\n"
MSG_ALIGN
msg38:
	.asciiz "Enter badaddr()\n"
MSG_ALIGN
msg39:
	.asciiz "  VADDR  = "
MSG_ALIGN
msg40:
	.asciiz "  ENHI   = "
MSG_ALIGN
msg41:
	.asciiz "  ENLO   = "
MSG_ALIGN
msg42:
	.asciiz "  TLBSET = "
MSG_ALIGN
msg43:
	.asciiz "Probe badaddr\n"
MSG_ALIGN
msg44:
	.asciiz "Badaddr invalid in tlb. ld from pt, write to tlb.\n"
MSG_ALIGN
msg45:
	.asciiz "Badaddr not found in TLB.  VADDR = "
MSG_ALIGN
msg46:
	.asciiz "Badaddr not found in pagetable."
MSG_ALIGN
msg47:
	.asciiz "  TLBL pagetable address is ok\n"
MSG_ALIGN
msg48:
	.asciiz "Eret to task\n"
MSG_ALIGN
msg49:
	.asciiz "  TLBL pagetable is at "
MSG_ALIGN
msg50:
	.asciiz "  Load from pt at ";
MSG_ALIGN
msg51:
	.asciiz "  Wrote to tlb\n";
MSG_ALIGN
msg52:
	.asciiz "Interrupt! not sched\n";
MSG_ALIGN
msg53:
	.asciiz "  CAUSE  = "
MSG_ALIGN
msg54:
	.asciiz "Enter everest_zero\n"
MSG_ALIGN
msg55:
	.asciiz "Leave everest_zero\n"
MSG_ALIGN
msg56:
	.asciiz "Interrupt found one CAUSE_IP bit = "
MSG_ALIGN
msg57:
	.asciiz "Bad interrupt\n"
MSG_ALIGN
msg58:
	.asciiz "Fail test PC = "
MSG_ALIGN
msg59:
	.asciiz "  CAUSE  = 0!!!\n"
MSG_ALIGN
msg60:
	.asciiz "  EPC    = "
MSG_ALIGN
msg61:
	.asciiz "  CAUSE masked by SR = "
MSG_ALIGN
msg62:
	.asciiz "  PID    = "
MSG_ALIGN
msg63:
	.asciiz "Unexpected interrupt\n"
MSG_ALIGN
msg64:
	.asciiz "  SR     = "
MSG_ALIGN
msg65:
	.asciiz "Dump TLB entry "
MSG_ALIGN
msg66: 
	.asciiz "Bad Syscall value = "
MSG_ALIGN
msg67: 
	.asciiz "Branched on reg value = "
MSG_ALIGN
msg68: 
	.asciiz "Enter GENEXCEPT not on sched\n"
MSG_ALIGN
msg69: 
	.asciiz "  RSYS   = "
MSG_ALIGN
msg70:
	.asciiz "Enter Dump TLB\n"
MSG_ALIGN
msg71:
	.asciiz "  Load from PT successful\n"
MSG_ALIGN
msg72:
	.asciiz "5ced"
MSG_ALIGN
msg73:
	.asciiz "Task switch to PID "
MSG_ALIGN
msg74:
	.asciiz "Return to PID = "
MSG_ALIGN
msg75:
	.asciiz "  CURPROC = "
MSG_ALIGN
msg76:
	.asciiz "  SAVE_T\n"
MSG_ALIGN
msg77:
	.asciiz "  RESTORE_T\n"
MSG_ALIGN
msg78:
	.asciiz "  SAVE_REST\n"
MSG_ALIGN
msg79:
	.asciiz "  RESTORE_REST\n"
MSG_ALIGN
msg80:
	.asciiz "  COUNTS = "
MSG_ALIGN
msg81:
	.asciiz "  SENDING PASS INTERRUPT\n";
MSG_ALIGN
msg82:
	.asciiz "  NMI\n";
MSG_ALIGN
msg83:
	.asciiz "  Handler Vector = ";
MSG_ALIGN
msg84:
	.asciiz "Return to pod addr = ";
MSG_ALIGN
msg85:
	.asciiz "  Reg = ";
MSG_ALIGN
msg86:
	.asciiz "Received PASS interrupt\n";
MSG_ALIGN
msg87:
	.asciiz "Received FAIL interrupt\n";
MSG_ALIGN
msg88:
	.asciiz "  SENDING FAIL INTERRUPT\n";
MSG_ALIGN
msg89:
	.asciiz "  Twiddling\n";
MSG_ALIGN
msg90:
	.asciiz "  ERTOIP = ";
MSG_ALIGN
msg91:
	.asciiz "  PT_INFO\n";
MSG_ALIGN
msg92:
	.asciiz "  IMPOSSIBLE\n";
MSG_ALIGN
	
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
	.asciiz	"\r\n"
delim:
	.asciiz "> "
	.globl	nib_data_end	/* A required label for nextract */
nib_data_end:
	PTR_WORD	0


