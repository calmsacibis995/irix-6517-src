#ident	"$Revision: 1.30 $"

#include "asm.h"
#include "regdef.h"
#include "ide_msg.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/z8530.h"
#include "ml.h"

#if IP20
#define LMEM_START	0x8000000		/* 0MB */
#define LMEM_SIZE	0x800000		/* 8MB */
#define LMEM_OFFSET	LMEM_SIZE		/* 8MB */
#define FLASH_MASK	0xfffff
#endif

#if IP22
#define LMEM_START	0x8000000		/* 0MB */
#define LMEM_SIZE	0x800000		/* 8MB */
#define LMEM_OFFSET	LMEM_SIZE		/* 8MB */
#define FLASH_MASK	0x3fffff
#endif

#if IP26
#define LMEM_START	PHYS_RAMBASE+0x800000	/* 8MB */
#define LMEM_SIZE	0x300000		/* 3MB */
#define LMEM_OFFSET	0x400000		/* 4MB (12MB - 8MB) */
#define FLASH_MASK	0x1fffff
#endif

#if IP28
#define LMEM_START      PHYS_RAMBASE+0x800000   /* 8MB */
#define LMEM_SIZE       0x400000                /* 4MB */
#define LMEM_OFFSET     0x400000                /* 4MB (4MB - 8MB)*/
#define FLASH_MASK      0x1fffff
#endif


/*
 * cannot be used to call routines that have the 'jal' instruction since 'jal'
 * uses absolute address
 */
#if IP28
#define CALL(f)								\
        LA	t0,f;							\
        PTR_SUB	t0,s0;							\
        jalr    t0;							\
        nop
#else

#define	CALL(f)								\
	LA	t0,f;							\
	PTR_ADD	t0,s0;							\
	jalr	t0;							\
	nop
#endif

	.set	noreorder

#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	# o32: local, ra, s0-7, fp, mips2 pad
FRAMESIZE1=((2*SZREG)+(11*SZREG)+3*SZREG+15)&~15
#define REPORT_OFFSET	FRAMESIZE1+1*SZREG
#define PFLAG_OFFSET	FRAMESIZE1+0
#else
	# n64, n32: local + reg save + arg save
FRAMESIZE1=16*SZREG+2*SZREG
#define REPORT_OFFSET	FRAMESIZE1-14*SZREG
#define PFLAG_OFFSET	FRAMESIZE1-15*SZREG
#endif

#if IP26 || IP28
NESTED(_low_dram_test, FRAMESIZE1, zero)
#else
NESTED(low_dram_test, FRAMESIZE1, zero)
#endif
#if !_K64PROM32						/* need to use BEV */
	PTR_SUB	sp,FRAMESIZE1
	PTR_S	zero,FRAMESIZE1-1*SZREG(sp)		# error count
	PTR_S	ra,FRAMESIZE1-3*SZREG(sp)
	PTR_S	s0,FRAMESIZE1-4*SZREG(sp)		# lmem
	PTR_S	s1,FRAMESIZE1-5*SZREG(sp)
	PTR_S	s2,FRAMESIZE1-6*SZREG(sp)
	PTR_S	s3,FRAMESIZE1-7*SZREG(sp)
	PTR_S	s4,FRAMESIZE1-8*SZREG(sp)
	PTR_S	s5,FRAMESIZE1-9*SZREG(sp)
	PTR_S	s6,FRAMESIZE1-10*SZREG(sp)
	PTR_S	s7,FRAMESIZE1-11*SZREG(sp)
	PTR_S	fp,FRAMESIZE1-12*SZREG(sp)
	PTR_S	zero,FRAMESIZE1-13*SZREG(sp)		# led state (fullhouse)
	PTR_S	a1,REPORT_OFFSET(sp)			# Reportlevel
	PTR_S	a0,PFLAG_OFFSET(sp)			# pflag

	/* DO NOT MODIFY, USE THROUGH OUT THE ENTIRE FILE */
	li	s0,LMEM_OFFSET

	/* move from LMEM_START to buffer area */
	LI	a0,PHYS_TO_K1(LMEM_START)
	LI	a1,LMEM_OFFSET
#if IP28
	PTR_SUB	a1,a0,a1
#else
	PTR_ADD	a1,a0,a1
#endif
	li	a2,LMEM_SIZE
	jal	bcopy
	nop

#if R4000 || R10000
	/*  Use bootstrap vector for exceptions.
	 */
	LA	fp,trap_handler
#if IP28
	PTR_SUB	fp,s0
#else
	PTR_ADD	fp,s0
#endif
	li	t0,SR_PROMBASE|SR_BEV|SR_IE|SR_BERRBIT
	mtc0	t0,C0_SR
#endif
#if TFP
	/*  Relocate trapbase, and set TETON_BEV to our local handler.
	 */
	DMFC0	(t0,C0_TRAPBASE)
	PTR_ADD	t0,s0
	DMTC0	(t0,C0_TRAPBASE)
	LA	t0,trap_handler
	PTR_ADD t0,s0
	DMTC0	(t0,TETON_BEV)

	/*  Jump to uncached (chandra, then flush the cache) to avoid
	 * instruction interference before jumping back to cached
	 * text (bcopied target).  We also need to flush the cache
	 * to avoid false hits with previous ide text/data.
	 */
	jal	run_chandra
	jal	flush_cache
#endif

#if IP28
	/* execute IDE in low memory */
	LA	t0,1f
	PTR_SUB	t0,s0
	j	t0
	PTR_SUB	sp,s0
#else
	/* execute IDE in high memory */
	PTR_ADD	gp,s0
	LA	t0,1f
	PTR_ADD	t0,s0
	j	t0
	PTR_ADD	sp,s0
#endif

1:
	/*  Set up high memory boundary, LMEM_SIZE, and determine data
	 * patterns to use according to pflag.
	 */
	LI	s7,PHYS_TO_K1(LMEM_START+LMEM_OFFSET)	# end of test

	PTR_L	t0,PFLAG_OFFSET(sp)		# pflag
	move	s2,zero
	beq	t0,zero,2f
	not	s3,zero				# address/data bit test
	li	s3,0x01010101			# parity bit test

2:
	li	s1,0x10				# address bit mask

	/*
	 * load data pattern into memory. the data pattern in each location is
	 * determined by the current address mask, s1, and the address, s6.
	 * if (s1 & s6) == 0, use s2, otherwise s3
	 */
3:
	LI	s6,PHYS_TO_K1(LMEM_START)	# first location
	and	s4,s6,s1			# determine data pattern
4:
	CACHE_BARRIER
	beq	s4,zero,5f
	sw	s2,0(s6)
	CACHE_BARRIER
	sw	s3,0(s6)
5:
	/* blink health led occasionally to show the program is still alive */
	and	t0,s6,FLASH_MASK
	bne	t0,zero,6f
	PTR_ADD	s6,4				# BDSLOT: next location

#if IP20
        CALL(switch_led_color)
#else
        PTR_L      a0,FRAMESIZE1-13*SZREG(sp)
        CALL(switch_led_color)
        PTR_S      v0,FRAMESIZE1-13*SZREG(sp)
#endif


6:
	bne	s6,s7,4b			# all done ?
	and	s4,s6,s1			# BDSLOT:determine data pattern

	/* verify */
	LI	s6,PHYS_TO_K1(LMEM_START)	# first location
	and	s4,s6,s1
7:
	lw	v0,0(s6)			# parity exception may occur
	beq	s4,zero,8f
	move	s5,s2				# BDSLOT
	move	s5,s3
8:
	beq	v0,s5,10f			# data match ?

	/* data mismatch, print out error message */
	PTR_L	t0,FRAMESIZE1-1*SZREG(sp)		# error count
	PTR_L	t1,REPORT_OFFSET(sp)			# Reportlevel

9:
	PTR_ADD	t0,1				# increment error count
	PTR_S	t0,FRAMESIZE1-1*SZREG(sp)
	blt	t1,ERR,10f			# print error message ?

	/* save read data */
	PTR_S	v0,FRAMESIZE1-2*SZREG(sp)

	/* print error message */
	LA	a0,address_msg			# print 'Address: '
	CALL(_puts)
	move	a0,s6				# print address value
	CALL(_puthex)

	LA	a0,expected_msg			# print ', Expected: '
	CALL(_puts)
	move	a0,s5				# print expected value
	CALL(_puthex)

	LA	a0,actual_msg			# print ', Actual '
	CALL(_puts)
	PTR_L	a0,FRAMESIZE1-2*SZREG(sp)
	CALL(_puthex)

	LA	a0,crlf_msg			# print newline
	CALL(_puts)
10:
	/* blink health led occasionally to show the program is still alive */
	and	t0,s6,FLASH_MASK		# blink health led ?
	bne	t0,zero,11f
	PTR_ADD	s6,4				# BDSLOT: next location

#if IP20
        CALL(switch_led_color)
#else
        PTR_L      a0,FRAMESIZE1-13*SZREG(sp)
        CALL(switch_led_color)
        PTR_S      v0,FRAMESIZE1-13*SZREG(sp)
#endif
11:
	bne	s6,s7,7b			# all done ?
	and	s4,s6,s1			# BDSLOT

	sll	s1,1
	bne	s1,s0,3b			# through all address bit ?
	nop

	bne	s2,zero,12f			# if not done with data
	move	t0,s2				# BDSLOT: patterns' complement,
	move	s2,s3				# complement and repeat
	b	2b
	move	s3,t0				# BDSLOT
12:
	/* move from store area back home */
	LI	a1,PHYS_TO_K1(LMEM_START)
	LI	a0,LMEM_OFFSET
#if IP28
	PTR_SUB	a0,a1,a0
#else
	PTR_ADD a0,a1,a0
#endif
	li	a2,LMEM_SIZE
	CALL(bcopy)

#if R4000 || R10000
	/* Restore the normal vectors for exceptions.
	 */
	li	t0,SR_PROMBASE|SR_IE|SR_BERRBIT
	mtc0	t0,C0_SR
#endif
#if TFP
	/* Restore the normal vectors for exceptions.
	 */
	DMFC0	(t0,C0_TRAPBASE)
	PTR_SUBU	t0,s0
	DMTC0	(t0,C0_TRAPBASE)
	DMTC0	(zero,TETON_BEV)
#endif

#if IP28
	/* set up to execute IDE in high memory */
        LA      t0,13f
        j       t0
        PTR_ADD sp,s0
#else
	/* set up to execute IDE in low memory */
	PTR_SUB gp,s0
	LA	t0,13f
	j	t0
	PTR_SUB sp,s0
#endif
13:
	PTR_L	v0,FRAMESIZE1-1*SZREG(sp)		# pass error count

	PTR_L	ra,FRAMESIZE1-3*SZREG(sp)
	PTR_L	s0,FRAMESIZE1-4*SZREG(sp)
	PTR_L	s1,FRAMESIZE1-5*SZREG(sp)
	PTR_L	s2,FRAMESIZE1-6*SZREG(sp)
	PTR_L	s3,FRAMESIZE1-7*SZREG(sp)
	PTR_L	s4,FRAMESIZE1-8*SZREG(sp)
	PTR_L	s5,FRAMESIZE1-9*SZREG(sp)
	PTR_L	s6,FRAMESIZE1-10*SZREG(sp)
	PTR_L	s7,FRAMESIZE1-11*SZREG(sp)
	PTR_L	fp,FRAMESIZE1-12*SZREG(sp)
#endif	/* !_K64PROM32 */
	j	ra
	PTR_ADD	sp,FRAMESIZE1
#if IP26 || IP28
END(_low_dram_test)
#else
END(low_dram_test)
#endif


/********************************************************/
/*  Begin kh low test					*/
/*							*/
/*  CALL, FRAMESIZE1, REPORT_OFFSET, PFLAG_OFFSET	*/
/*  are the same values as defined for low_dram_test	*/ 
/********************************************************/

#if _MIPS_SIM == _ABI64
#define	INTERVAL	0x8		/* bytes */
#define ALL_ONES        0xffffffffffffffff
#else
#define	INTERVAL	0x4	
#define ALL_ONES        0xffffffff
#endif

#if IP26 || IP28
NESTED(_low_kh_test, FRAMESIZE1, zero)
#else
NESTED(low_kh_test, FRAMESIZE1, zero)
#endif
#if !_K64PROM32						/* need to use BEV */
	PTR_SUB	sp,FRAMESIZE1
	PTR_S	zero,FRAMESIZE1-1*SZREG(sp)		# error count
	PTR_S	ra,FRAMESIZE1-3*SZREG(sp)
	PTR_S	s0,FRAMESIZE1-4*SZREG(sp)		# lmem
	PTR_S	s1,FRAMESIZE1-5*SZREG(sp)
	PTR_S	s2,FRAMESIZE1-6*SZREG(sp)
	PTR_S	s3,FRAMESIZE1-7*SZREG(sp)
	PTR_S	s4,FRAMESIZE1-8*SZREG(sp)
	PTR_S	s5,FRAMESIZE1-9*SZREG(sp)
	PTR_S	s6,FRAMESIZE1-10*SZREG(sp)
	PTR_S	s7,FRAMESIZE1-11*SZREG(sp)
	PTR_S	fp,FRAMESIZE1-12*SZREG(sp)
	PTR_S	a1,REPORT_OFFSET(sp)			# Reportlevel
	PTR_S	a0,PFLAG_OFFSET(sp)			# pflag (not used)

	/* DO NOT MODIFY, USE THROUGH OUT THE ENTIRE FILE */
	li	s0,LMEM_OFFSET

	/* move from LMEM_START to buffer area */
	LI	a0,PHYS_TO_K1(LMEM_START)
	LI	a1,LMEM_OFFSET
#if IP28
	PTR_SUB a1,a0,a1
#else
	PTR_ADD	a1,a0,a1
#endif
	li	a2,LMEM_SIZE
	jal	bcopy
	nop

#if R4000 || R10000
	/*  Use bootstrap vector for exceptions.
	 */
	LA	fp,trap_handler
#if IP28
	PTR_SUB fp,s0
#else
	PTR_ADD	fp,s0
#endif
	li	t0,SR_PROMBASE|SR_BEV|SR_IE|SR_BERRBIT
	mtc0	t0,C0_SR

#endif
#if TFP
	/*  Relocate trapbase, and set TETON_BEV to our local handler.
	 */
	DMFC0	(t0,C0_TRAPBASE)
	PTR_ADD	t0,s0
	DMTC0	(t0,C0_TRAPBASE)
	LA	t0,trap_handler
	PTR_ADD t0,s0
	DMTC0	(t0,TETON_BEV)

	/*  Jump to uncached (chandra, then flush the cache) to avoid
	 * instruction interference before jumping back to cached
	 * text (bcopied target).  We also need to flush the cache
	 * to avoid false hits with previous ide text/data.
	 */
	jal	run_chandra
	jal	flush_cache
#endif

#if IP28
	/* execute IDE in low memory */
        LA      t0,khstart
        PTR_SUB	t0,s0
        j       t0
        PTR_SUB	sp,s0
#else
	/* execute IDE in high memory */
	PTR_ADD	gp,s0
	LA	t0,khstart 
	PTR_ADD	t0,s0
	j	t0
	PTR_ADD	sp,s0
#endif

khstart:

	LI	s6, PHYS_TO_K1(LMEM_START)
	move	s2, s6
	LI	s7, PHYS_TO_K1(LMEM_START+LMEM_OFFSET)

	/* Start Knaizuk Hartmann test */

	/*
	 * Set partitions 1 and 2 to 0's	
	 */
	LI      s7, PHYS_TO_K1(LMEM_START+LMEM_OFFSET)
	LI      s6, PHYS_TO_K1(LMEM_START)

	move	s3,zero			# set expected pattern
	PTR_ADDIU s2,s6,INTERVAL	# get first partition 1 address

1:
	CACHE_BARRIER
	PTR_S	s3,(s2)			# set partition 1 to zero
	PTR_ADDIU	s2,s2,INTERVAL	# increment to partition 2 address
	bgeu	s2,s7,2f		# reached the end yet?
	nop
	CACHE_BARRIER
	PTR_S	s3,(s2)			# set partition 2 to zero

	PTR_ADDIU s2,s2,INTERVAL*2	# increment to partition 1 address
	bltu	s2,s7,1b		# reached the end yet?
	nop

	/*
	 * Set partition 0 to 1's	
	 */
2:
	LI	s3, ALL_ONES	
	LI      s7, PHYS_TO_K1(LMEM_START+LMEM_OFFSET)
        LI      s6, PHYS_TO_K1(LMEM_START)

	move	s2,s6			# get first partition 0 address
3:
	CACHE_BARRIER
	PTR_S	s3,(s2)			# set partition 0 to one's
	PTR_ADDIU s2,s2,INTERVAL*3	# next address
	bltu	s2,s7,3b		# reached the end yet?
	nop

	/*
	 * Set partition 1 to 1's		
	 */
	LI	s3, ALL_ONES		
	LI      s7, PHYS_TO_K1(LMEM_START+LMEM_OFFSET)
        LI      s6, PHYS_TO_K1(LMEM_START)

	PTR_ADDIU s2,s6,INTERVAL	# get first partition 1 address
5:
	CACHE_BARRIER
	PTR_S	s3,(s2)			# set partition 1 to one's
	PTR_ADDIU s2,s2,INTERVAL*3	# next address
	bltu	s2,s7,5b		# reached the end yet?
	nop
 
	/*
	 * Verify partition 2 is still 0's
	 */
	move	s3,zero			# set expected pattern
        LI      s6,PHYS_TO_K1(LMEM_START)
	LI      s7,PHYS_TO_K1(LMEM_START+LMEM_OFFSET)

	PTR_ADDIU s2,s6,INTERVAL*2	# get first partition 2 address
6:
	PTR_L	s4,(s2)			# read data
	bne	s4,s3,error		# does it match?
	nop
	PTR_ADDIU s2,s2,INTERVAL*3	# bump to next location
	bltu	s2,s7,6b		# reached the end yet?
	nop

	/*
	 * Verify partition 0 is still 1's	
	 */
	LI	s3,ALL_ONES
        LI      s6,PHYS_TO_K1(LMEM_START)
	LI      s7,PHYS_TO_K1(LMEM_START+LMEM_OFFSET)
	move	s2,s6			# get first partition 0 address
7:
	PTR_L	s4,(s2)			# read data
	bne	s4,s3,error		# does it match?
	nop
	PTR_ADDIU s2,s2,INTERVAL*3	# bump to next location
	bltu	s2,s7,7b		# reached the end yet?
	nop
 
	/*
	 * Verify partition 1 is still 1's
	 */
	LI	s3,ALL_ONES		
        LI      s6, PHYS_TO_K1(LMEM_START)
	LI      s7, PHYS_TO_K1(LMEM_START+LMEM_OFFSET)

	PTR_ADDIU s2,s6,INTERVAL	# get first partition 1 address
8:
	PTR_L	s4,(s2)			# read data
	bne	s4,s3,error		# does it match?
	nop
	PTR_ADDIU s2,s2,INTERVAL*3	# bump to next location
	bltu	s2,s7,8b		# reached the end yet?
	nop


	/*
	 * Set partition 0 to 0's		
	 */
	move	s3,zero			# set expected pattern
        LI      s6,PHYS_TO_K1(LMEM_START)
	LI      s7,PHYS_TO_K1(LMEM_START+LMEM_OFFSET)

	move	s2,s6			# get first partition 0 address
9:
	CACHE_BARRIER
	PTR_S	s3,(s2)			# set partition 0 to zero's
	PTR_ADDIU s2,s2,INTERVAL*3	# next address
	bltu	s2,s7,9b		# reached the end yet?
	nop
 
	/*
	 * Verify partition 0 is still 0's
	 */
	move	s3,zero			# set expected pattern
        LI      s6,PHYS_TO_K1(LMEM_START)
	LI      s7,PHYS_TO_K1(LMEM_START+LMEM_OFFSET)

	move	s2,s6			# get first partition 0 address
10:
	PTR_L	s4,0(s2)		# read data
	bne	s4,s3,error		# does it match?
	nop
	PTR_ADDIU s2,s2,INTERVAL*3	# bump to next location
	bltu	s2,s7,10b		# reached the end yet?
	nop

	/*
	 * Set partition 2 to 1's		
	 */
	LI	s3,ALL_ONES		
        LI      s6,PHYS_TO_K1(LMEM_START)
	LI      s7,PHYS_TO_K1(LMEM_START+LMEM_OFFSET)

	PTR_ADDIU s2,s6,INTERVAL*2	# get first partition 2 address
11:
	CACHE_BARRIER
	PTR_S	s3,(s2)			# set partition 2 to one's
	PTR_ADDIU s2,s2,INTERVAL*3	# next address
	bltu	s2,s7,11b		# reached the end yet?
	nop

	/*
	 * Verify partition 2 is still 1's
	 */
	LI	s3,ALL_ONES		
        LI      s6,PHYS_TO_K1(LMEM_START)

	PTR_ADDIU s2,s6,INTERVAL*2	# get first partition 2 address
12:
	PTR_L	s4,(s2)			# read data
	bne	s4,s3,error		# does it match?
	nop

	PTR_ADDIU s2,s2,INTERVAL*3	# bump to next location
	bltu	s2,s7,12b		# reached the end yet?
	nop
	move	v0,zero			# indicate success
	b	14f			# and return
	nop

error:
	li	v0,1
	/* data mismatch, print out error message */
	PTR_L	t0,FRAMESIZE1-1*SZREG(sp)	# error count
	PTR_L	t1,REPORT_OFFSET(sp)		# Reportlevel

9:
	PTR_ADD	t0,1				# increment error count
	PTR_S	t0,FRAMESIZE1-1*SZREG(sp)

	/* save read data */
	PTR_S	s4,FRAMESIZE1-2*SZREG(sp)

	/* print error message */
	LA	a0,address_msg			# print 'Address: '
	CALL(_puts)

	move	a0,s2				# print address value
	CALL(_puthex)

	LA	a0,expected_msg			# print ', Expected: '
	CALL(_puts)
	move	a0,s3				# print expected value
	CALL(_puthex)

	LA	a0,actual_msg			# print ', Actual '
	CALL(_puts)
	PTR_L	a0,FRAMESIZE1-2*SZREG(sp)
	CALL(_puthex)

	LA	a0,crlf_msg			# print newline
	CALL(_puts)

        LA      a0,crlf_msg                     # print newline
        CALL(_puts)

14:
	/* move from store area back home */
	LI	a1,PHYS_TO_K1(LMEM_START)
	LI	a0,LMEM_OFFSET
#if IP28
	PTR_SUB	a0,a1,a0
#else
	PTR_ADD a0,a1,a0
#endif
	li	a2,LMEM_SIZE
	CALL(bcopy)

#if R4000 || R10000
	/* Restore the normal vectors for exceptions.
	 */
	li	t0,SR_PROMBASE|SR_IE|SR_BERRBIT
	mtc0	t0,C0_SR
#endif
#if TFP
	/* Restore the normal vectors for exceptions.
	 */
	DMFC0	(t0,C0_TRAPBASE)
	PTR_SUBU	t0,s0
	DMTC0	(t0,C0_TRAPBASE)
	DMTC0	(zero,TETON_BEV)
#endif

#if IP28
	/* set up to execute IDE in high memory */
        LA	t0,13f
        j       t0
        PTR_ADD sp,s0
#else
	/* set up to execute IDE in low memory */
	PTR_SUB gp,s0
	LA	t0,13f
	j	t0
	PTR_SUB	sp,s0
#endif

13:
	PTR_L	v0,FRAMESIZE1-1*SZREG(sp)		# pass error count
	PTR_L	ra,FRAMESIZE1-3*SZREG(sp)
	PTR_L	s0,FRAMESIZE1-4*SZREG(sp)
	PTR_L	s1,FRAMESIZE1-5*SZREG(sp)
	PTR_L	s2,FRAMESIZE1-6*SZREG(sp)
	PTR_L	s3,FRAMESIZE1-7*SZREG(sp)
	PTR_L	s4,FRAMESIZE1-8*SZREG(sp)
	PTR_L	s5,FRAMESIZE1-9*SZREG(sp)
	PTR_L	s6,FRAMESIZE1-10*SZREG(sp)
	PTR_L	s7,FRAMESIZE1-11*SZREG(sp)
	PTR_L	fp,FRAMESIZE1-12*SZREG(sp)
	PTR_ADD	sp,FRAMESIZE1
#endif	/* !_K64PROM32 */
	j	ra
	nop
#if IP26 || IP28
END(_low_kh_test)
#else
END(low_kh_test)
#endif


LEAF(trap_handler)
	/* R4000 chip registers */
	LA	a0,epc_msg		# EPC
	CALL(_puts)
	MFC0	(a0,C0_EPC)
	CALL(_puthex)

#if R4000 || R10000
	LA	a0,errepc_msg		# ErrorEPC
	CALL(_puts)
	MFC0	(a0,C0_ERROR_EPC)
	CALL(_puthex)
#endif

	LA	a0,badvaddr_msg		# BadVaddr
	CALL(_puts)
	MFC0	(a0,C0_BADVADDR)
	CALL(_puthex)

	LA	a0,cause_msg		# Cause
	CALL(_puts)
	MFC0	(a0,C0_CAUSE)
	CALL(_puthex)

	LA	a0,status_msg		# Status
	CALL(_puts)
	MFC0	(a0,C0_SR)
	CALL(_puthex)

#if R4000 || R10000
	LA	a0,cacherr_msg		# CacheErr
	CALL(_puts)
	mfc0	a0,C0_CACHE_ERR
	CALL(_puthex)	
#endif

	/* board registers */
	LA	a0,cpu_parerr_msg	# CPU parity error register
	CALL(_puts)
	LI	v0,PHYS_TO_K1(CPU_ERR_STAT)
	lw	a0,0(v0)
	CALL(_puthex)

	LA	a0,gio_parerr_msg	# GIO parity error register
	CALL(_puts)
	LI	v0,PHYS_TO_K1(GIO_ERR_STAT)
	lw	a0,0(v0)
	CALL(_puthex)

#if IP22
	# Get INT2/3 base
	.set	reorder
	LI	s1,PHYS_TO_K1(HPC3_INT3_ADDR)   # assume IOC1/INT3
	IS_IOC1(v0)
	bnez	v0,1f				# branch if IOC1/INT3
	LI	s1,PHYS_TO_K1(HPC3_INT2_ADDR)	# use INT2
	.set	noreorder
1:
#elif IP26 || IP28
	LI	s1, PHYS_TO_K1(HPC3_INT2_ADDR)	# INT2
#endif
	LA	a0,liostat0_msg		# local I/O interrupt status 0
	CALL(_puts)
#if IP20
	lbu	a0,PHYS_TO_K1(LIO_0_ISR_ADDR)
#else
	lbu	a0, LIO_0_ISR_OFFSET(s1)
#endif
	CALL(_puthex)

	LA	a0,liostat1_msg		# local I/O interrupt status 1
	CALL(_puts)
#if IP20
	lbu	a0,PHYS_TO_K1(LIO_1_ISR_ADDR)
#else
	lbu	v0, LIO_1_ISR_OFFSET(s1)
#endif
	CALL(_puthex)

	LA	a0,cpuaddr_msg		# CPU error address register
	CALL(_puts)
	LI	v0,PHYS_TO_K1(CPU_ERR_ADDR)
	lw	a0,0(v0)
	CALL(_puthex)

	LA	a0,gioaddr_msg		# GIO error address register
	CALL(_puts)
	LI	v0,PHYS_TO_K1(GIO_ERR_ADDR)
	lw	a0,0(v0)
	CALL(_puthex)

	LA	a0,reset_msg		# reset to restart
	CALL(_puts)

	/*
	 * loop forever when we got an exception since there's no good/easy way
	 * for us to recover
	 */
1:	b	1b
END(trap_handler) 

/* print out a hex value. hex value in a0. derived from pon_puthex */
FRAMESIZE2=(1*SZREG+(2*SZREG)+1*SZREG+1*SZREG+15)&~15 # ra, s1..2, mips2 pad
NESTED(_puthex, FRAMESIZE2, zero)
	PTR_SUB	sp,FRAMESIZE2
	PTR_S	ra,FRAMESIZE2-1*SZREG(sp)
	PTR_S	s1,FRAMESIZE2-2*SZREG(sp)
	PTR_S	s2,FRAMESIZE2-3*SZREG(sp)
	PTR_S	v0,FRAMESIZE2-4*SZREG(sp)


#if _MIPS_SIM == _ABI64
	li	s1,16		# Number of digits to display
#else
	li	s1,8		# Number of digits to display
#endif
	LA	s2,pon_putc			# execute in high memory
#if IP28
	PTR_SUB	s2,s0
#else
	PTR_ADD	s2,s0
#endif

1:
#if _MIPS_SIM == _ABI64
	dsrl	v1,a0,32	# Isolate digit 
	dsrl	v1,v1,28	# Isolate digit 
#else
	srl	v1,a0,28	# Isolate digit 
#endif
	and	v1,0xf
	lbu	v1,hexdigit+LMEM_OFFSET(v1)

	jal	s2				# print one digit at a time
	PTR_SUB	s1,1
	PTR_SLL	a0,4		# Set up next nibble
	bne	s1,zero,1b			# entire number printed?

	PTR_L	ra,FRAMESIZE2-1*SZREG(sp)
	PTR_L	s1,FRAMESIZE2-2*SZREG(sp)
	PTR_L	s2,FRAMESIZE2-3*SZREG(sp)
	PTR_L	v0,FRAMESIZE2-4*SZREG(sp)
	j	ra
	PTR_ADD	sp,FRAMESIZE2
END(_puthex)



/*
 * print out a character string. address of the first character in a0. derived
 * from pon_puts
 */
FRAMESIZE3=((1*SZREG)+SZREG+2*SZREG+15)&~15		# ra, s1, mips pad
NESTED(_puts, FRAMESIZE3, zero)
	PTR_SUB	sp,FRAMESIZE3
	PTR_S	ra,FRAMESIZE3-1*SZREG(sp)
	PTR_S	s1,FRAMESIZE3-2*SZREG(sp)

#if IP28
        PTR_SUB	a0,s0                           # execute in low memory
        LA      s1,pon_putc
        PTR_SUB	s1,s0
#else
	PTR_ADD	a0,s0				# execute in high memory
	LA	s1,pon_putc
	PTR_ADD	s1,s0
#endif

	b	2f
	nop
1:
	jal	s1				# print one character at a time
	PTR_ADD	a0,1				# address of next character
2:
	lbu	v1,0(a0)			# get character
	nop
	bne	v1,zero,1b			# null character ?

	PTR_L	ra,FRAMESIZE3-1*SZREG(sp)
	PTR_L	s1,FRAMESIZE3-2*SZREG(sp)
	j	ra
	PTR_ADD	sp,FRAMESIZE3
END(_puts)



#ifdef	DELAY
#undef	DELAY
#endif

#if R4000
#define	DELAY	nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;	\
		nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;
#else
#define DELAY                                   \
	li	v0,300;                         \
99:	lw	zero,PHYS_TO_K1(0x1fc000c0);    \
	sub	v0,1;                           \
	bnez	v0,99b
#endif

/* switch the color of the health LED */
#if IP20
LEAF(switch_led_color)
	LI	a0,PHYS_TO_K1(CPU_AUX_CONTROL)
	lbu	a1,0(a0)
	nop
	xor	a1,CONSOLE_LED
	sb	a1,0(a0)

	LI	a0,PHYS_TO_K1(DUART0A_CNTRL)
	li	a1,WR5
	DELAY
	sb	a1,0(a0)
	DELAY
	lbu	a2,0(a0)
	DELAY
	sb	a1,0(a0)
	xor	a2,WR5_RTS
	DELAY
	sb	a2,0(a0)

	j	ra
	END(switch_led_color)
#else
	.data
bits22:	.byte	LED_GREEN, 0
#if IP22
bits24:	.byte	LED_RED_OFF,LED_RED_OFF|LED_GREEN_OFF
#endif
	.text

LEAF(switch_led_color)

	.set	reorder

	/* get color table base address */

	LA	t0, bits22
#if IP22
	lw      t1, HPC3_SYS_ID|K1BASE
	andi    t1, BOARD_IP22
	bnez	t1, 3f
	LA	t0, bits24
3:
#endif
#if IP28
	PTR_SUB	t0, s0
#else
	PTR_ADD	t0, s0
#endif

	/* get the color byte and set the leds */

	move	t1, a0
	andi	t1, 1
	PTR_ADD	t0, t1
	lb	t1, 0(t0)		/* t1 = color bits */

	LA	t2, _hpc3_write1	/* merge in the other write1 bits */
#if IP28
	PTR_SUB	t2, s0
#else
	PTR_ADD	t2, s0
#endif
	lw	t0, 0(t2)
	andi	t0, 0xcf
	or	t1, t0

        LI      t0, PHYS_TO_K1(HPC3_WRITE1)
	sb      t1, 3(t0)

	xori	v0, a0, 1

	j	ra
	END(switch_led_color)
#endif
	.set	noreorder

	.data
actual_msg:
	.asciiz	", Actual: "
address_msg:
	.asciiz	"\r\nAddress: "
crlf_msg:
	.asciiz	"\r\n"
expected_msg:
	.asciiz	", Expected: "
hexdigit:
	.ascii	"0123456789abcdef"

reset_msg:
	.asciiz "\r\n\r\n\r\n[Press reset to restart.]"

/*
 * The exception message looks like this on R4000:

EPC: 0xnnnnnnnn, ErrEPC: 0xnnnnnnnn, BadVaddr: 0xnnnnnnnn
Cause: 0xnnnnnnnn, Status: 0xnnnnnnnn, CacheErr: 0xnnnnnnnn
CpuParityErr: 0xnnnnnnnn, GioParityErr: 0xnnnnnnnn
LIOstatus0: 0xnnnnnnnn, LIOstatus1: 0xnnnnnnnn
CpuErrorAddr: 0xnnnnnnnn, GioErrorAddr: 0xnnnnnnnn

 * On TFP/T5 (64-bit values):
EPC: nnnnnnnnnnnnnnnn, BadVaddr: nnnnnnnnnnnnnnnn
Cause: nnnnnnnnnnnnnnnn, Status: nnnnnnnnnnnnnnnn
CpuParityErr: nnnnnnnnnnnnnnnn, GioParityErr: nnnnnnnnnnnnnnnn
LIOstatus0: nnnnnnnnnnnnnnnn, LIOstatus1: nnnnnnnnnnnnnnnn
CpuErrorAddr: nnnnnnnnnnnnnnnn, GioErrorAddr: nnnnnnnnnnnnnnnn
 */
epc_msg:
	.asciiz "\r\nEPC: "
	.asciiz ", BadVaddr: "
cause_msg:
	.asciiz "\r\nCause: "
status_msg:
	.asciiz ", Status: "
cpu_parerr_msg:
	.asciiz "\r\nCpuParityErr: "
badvaddr_msg:
gio_parerr_msg:
	.asciiz ", GioParityErr: "
liostat0_msg:
	.asciiz "\r\nLIOstatus0: "
liostat1_msg:
	.asciiz ", LIOstatus1: "
cpuaddr_msg:
	.asciiz "\r\nCpuErrorAddr: "
gioaddr_msg:
	.asciiz ", GioErrorAddr: "
#if R4000 || R10000
cacherr_msg:
	.asciiz ", CacheErr: "
errepc_msg:
	.asciiz ", ErrEPC: "
#endif
