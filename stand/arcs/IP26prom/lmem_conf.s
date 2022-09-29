#ident	"IP26prom/lmem_conf.s:  $Revision: 1.29 $"

/*
 * lmem_conf.s -- local memory configuration
 */

#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/vdma.h>
#include <early_regdef.h>
#include <asm.h>
#include <pon.h>

#define mcreg(reg,base)	reg-CPUCTRL0(base)

#define WBFLUSHM				\
	.set	noreorder ;			\
	.set	noat	;			\
	LI	AT,PHYS_TO_K1(CPUCTRL0) ;	\
	lw	AT,0(AT) ;			\
	.set	at ;				\
	.set	reorder

#define	MAXBANKS	3	/* max number of memory banks */
#define	MAXUNITS	32	/* max number of memory units in a bank */

/*
 * since we have to support memory banks with 256/512M and the low local
 * memory is not large enough for 512M and is not aligned on the right
 * boundary for 256M, all sizing will be done in the high local memory.
 */
#define	HIGH_MEM_BASE		0x20000000	/* base of high local memory,
						   512M */
#define	LOW_MEM_TOP		0x18000000	/* top of low local memory,
						   384M */

/* use fpu register to store the following variables */
#define	MC_REV_REG		$f0	# MC revision
#define	UNITSIZE_REG		$f1	# memory unit size
#define	MAXSIZE_REG		$f2	# max amount of memory per bank
#define	UNITSHIFT_REG		$f3	# byte to memory unit conversion shift
#define	PROBE_MEMCFG_REG	$f4	# MC mem config register for probing
#define	HIGH_MEM_SIZE_REG	$f5	# amount of high local memory
#define	LOW_MEM_SIZE_REG	$f6	# amount of low local memory, >0

	.text
#ifndef ENETBOOT
/*
 * configure MC's MEMCFG0/1
 *
 * this is a level 4 routine
 */
LEAF(init_memconfig)
	.set	noreorder
	dmtc0	ra,C0_GBASE	# hack -- unused in standalone
	ssnop
	ssnop
	.set	reorder

	jal	setup_regs	# depends on MC rev

init_mem_restart:
	/* invalidate all banks of memory */
	LI	v1,PHYS_TO_K1(MEMCFG0)	# Controls Banks 0&1
	sw	zero,0(v1)		# Do invalidate.
	/* Don't invalidate Bank 3, since ECC HW sits there. */
	li	a0,2			# Which bank?  2.
	move	a1,zero			# Want 0 in config reg.
	jal	set_bank_conf		# Do invalidate.

	move	T40,zero	# bank counter
	mthi	zero		# clear out bad SIMMs code

bank_loop: 	
	move	a0,T40		# bank number
	mfc1	a1,PROBE_MEMCFG_REG
	jal	set_bank_conf

	/* find the size, in 4/16 MB, of the current bank */ 
	move	a0,T40		# bank number
	jal	size_bank
	move	T41,v0		# size in 4/16 MB
	beq	v1,zero,1f
	beq	v1,BAD_ALLSIMMS,1f

	sll	a3,T40,2	# bank number times 4
	srl	v1,a3		# shift bad SIMMs code into position
	mfhi	a3
	or	a3,v1
	mthi	a3		# shift and save bad SIMMs code in mult-hi

1:
	beq	T41,zero,test_done

	/* set up to do the walking bit test */
	move	a0,T40		# bank number
	move	a1,T41		# size in 4/16 MB
	jal	set_bank_size

	.set	noreorder
	mfc1	a1,PROBE_MEMCFG_REG
	ssnop
	.set	reorder
	move	a0,T40
	and	a1,MEMCFG_VLD|MEMCFG_ADDR_MASK	# validate the bank and get
	or	a1,v0				#	the correct base addr
	jal	set_bank_conf

	/* run the walking bit test */
	.set	noreorder
	mfc1	a1,UNITSHIFT_REG
	ssnop
	.set	reorder
	LI	a0,PHYS_TO_K1(HIGH_MEM_BASE)	# starting addr
	sllv	a1,T41,a1		# bank size in bytes
	jal	pon_walkingbit
	beq	v0,zero,test_done	# v0==0 ->  walking bit test passed

	/* walking bit test failed. print message and invalidate this bank */
	sll	a3,T40,2	# bank number times 4
	srl	v1,v0,a3	# shift bad SIMMs code into position
	mfhi	a3
	or	a3,v1
	mthi	a3		# shift and save bad SIMMs code in mult-hi

	move	a0,T40		# bank number
	move	a1,v0		# bad SIMMs code
	LA	a2,datafail_msg
	jal	memfail		# print nice error message
	move    T41,zero	# invalidate this bank

test_done:
	move	a0,T40		# bank number
	move	a1,T41		# bank size
	jal	set_bank_size	# set config word for bank

	add	T40,1		# increment bank number
	bne	T40,MAXBANKS,bank_loop

	/* set the base address fields in MEMCFG0/1 */
	jal	set_bank_addrs
	move	T41,v0		# memory size in bytes

	.set	noreorder
	mfc1	a0,MC_REV_REG
	ssnop
	.set	reorder
	blt	a0,5,1f

	/*
	 * for rev 5 or later MC, make sure there is something in the low
	 * local memory
	 */
	.set	noreorder
	mfc1	a0,LOW_MEM_SIZE_REG
	ssnop
	.set	reorder
	bne	a0,zero,1f
	LA	a0,no_low_local_memory
	jal	pon_puts
	li	a0,3			# normal flash
	jal	memflash

1:
#ifdef	VERBOSE
	LA	a0,mem_size_done
	jal	pon_puts
	LI	T40,PHYS_TO_K1(MEMCFG0)
	lw	a0,0(T40)
	jal	pon_puthex
	li	v1,0x20
	jal	pon_putc
	lw	a0,8(T40)		# PHYS_TO_K1(MEMCFG1)
	jal	pon_puthex
	LA	a0,crlf
	jal	pon_puts
#endif

	bne	T41,zero,addr_test

	move	T40,zero	# first bank to check
1:
	mfhi	a1		# bad SIMMs code
	sll	a2,a1,4		# shift bad SIMMs code for the next bank
	mthi	a2		# save it
	and	a1,BAD_ALLSIMMS		# any bad SIMM in the current bank ?
	beq	a1,zero,2f	# jump if there's none
	move	a0,T40		# current bank number
	LA	a2,probefail_msg
	jal	memfail
2:
	add	T40,1		# next bank
	blt	T40,MAXBANKS,1b	# jump if all banks are done ?

	/* no memory, print message and flash LED forever */
	LA	a0,nomem_msg
	jal	pon_puts
	li	a0,3		# normal flash
	jal	memflash	# never returns

addr_test:
	mfc1	a1,LOW_MEM_SIZE_REG
	LI	a0,K1_RAMBASE	# first address
	jal	pon_addr
	beq	v0,zero,mem_ok

	/* test failed, print message and flash LED forever */
	LA	a0,addrfail_msg
	jal	pon_puts
	li	a0,3		# normal flash
	jal	memflash	# never returns

mem_ok:
	/* clear all memory by using virtual DMA */
	LI	a0,PHYS_TO_K1(CPUCTRL0)		# base mc address

	sw	zero,mcreg(DMA_CTL,a0)		# no virtual addr translation
	sw	zero,mcreg(DMA_CAUSE,a0)	# clear interrupts

	LI	a1,PHYS_RAMBASE
	sw	a1,mcreg(DMA_MEMADR,a0)		# starting memory location

	li	a1,VDMA_M_GTOH|VDMA_M_FILL|VDMA_M_INCA|VDMA_M_LBURST
	sw	a1,mcreg(DMA_MODE,a0)		# fill mode, DMA write

	mfc1	v0,LOW_MEM_SIZE_REG	# amount of memory in low local memory

	li	a1,0x1<<16
	sw	a1,mcreg(DMA_STRIDE,a0)		# zoom factor = 1

1:
	sll	a1,v0,1			# line count, byte count / 2^15
	ori	a1,0x1<<15		# line size = 2^15 bytes
	sw	a1,mcreg(DMA_SIZE,a0)

	sw	zero,mcreg(DMA_GIO_ADRS,a0)	# data is zero, start DMA
	lw	zero,0(a0)			# flushbus

	lw	a1,mcreg(DMA_RUN,a0)		# first make sure DMA starts
	and	a2,a1,VDMA_R_RUNNING
	beqz	a2,vdma_start_failed

1:	lw	a1,mcreg(DMA_RUN,a0)		# now wait for DMA to finish
	and	a2,a1,VDMA_R_RUNNING
	bnez	a2,1b

	# VDMA_R_PAGEFAULT|VDMA_R_UTLBMISS|VDMA_R_CLEAN|VDMA_R_BMEM|VDMA_R_BPTE
	and	a2,a1,0x37			# check for errors
	bnez	a2,vdma_aborted

	.set	noreorder
	mfc1	v0,HIGH_MEM_SIZE_REG
	ssnop
	.set	reorder

	sw	zero,mcreg(DMA_CAUSE,a0)	# clear interrupts

	beq	v0,zero,2f			# done if there's nothing in
						#	high local memory
	li	a1,HIGH_MEM_BASE		# start of high local memory
	sw	a1,mcreg(DMA_MEMADR,a0)		# starting memory location

	sll	a1,v0,1			# line count, byte count / 2^15
	ori	a1,0x1<<15		# line size = 2^15 bytes
	sw	a1,mcreg(DMA_SIZE,a0)

	sw	zero,mcreg(DMA_GIO_ADRS,a0)	# data is zero, start DMA
	lw	zero,0(a0)			# flushbus

	lw	a1,mcreg(DMA_RUN,a0)		# first make sure DMA starts
	and	a2,a1,VDMA_R_RUNNING
	beqz	a1,vdma_start_failed

2:	move	v0,T41				# return size of memory

	.set	noreorder
	dmfc0	a0,C0_GBASE			# restore RA.
	ssnop
	.set	reorder
	j	a0

/* Hook to clear all of memory with vdma */
XLEAF(clear_all_ram)
	.set	noreorder
	dmtc0	ra,C0_GBASE	# hack -- unused in standalone
	ssnop
	ssnop
	.set	reorder
	j	mem_ok

/* vdma did not start */
vdma_start_failed:
	LA	a0,vdma_start_msg
	jal	pon_puts
	b	1f

/* vdma aborted due to error */
vdma_aborted:
	LA	a0,vdma_abort_msg
	jal	pon_puts
1:
#ifdef DEBUG
#define	VDMA_PRINTREG(str,reg)		\
	LA	a0, str			;\
	jal	pon_puts		;\
	LI	a0, PHYS_TO_K1(reg)	;\
	lw	a0, 0(a0)		;\
	jal	pon_puthex

	VDMA_PRINTREG(vmda_er_dr, DMA_RUN)
	VDMA_PRINTREG(vmda_er_dc, DMA_CAUSE)
	VDMA_PRINTREG(vmda_er_dma, DMA_MEMADR)
	VDMA_PRINTREG(vmda_er_cma, CPU_MEMACC)
	VDMA_PRINTREG(vmda_er_gma, GIO_MEMACC)
	VDMA_PRINTREG(vmda_er_cea, CPU_ERR_ADDR)
	VDMA_PRINTREG(vmda_er_ces, CPU_ERR_STAT)
	VDMA_PRINTREG(vmda_er_ges, GIO_ERR_ADDR)
	VDMA_PRINTREG(vmda_er_ges, GIO_ERR_STAT)
#endif

	LA	a0, crlf
	jal	pon_puts

	b	init_mem_restart	# retry the memory init/clear

	END(init_memconfig)

/*  Wait for seg1 DMA to finish.  The memory initialization code clears
 * the seg0 memory and waits, then starts up the seg1 DMA and continues.
 *
 *  On systems with only low-memory, this loop should not loop as we have
 * already done this check once.
 */
LEAF(wait_clear_seg1)
	LI	a0,PHYS_TO_K1(CPUCTRL0)		# MC base

1:	lw	a1,mcreg(DMA_RUN,a0)		# now wait for DMA to finish
	and	a2,a1,VDMA_R_RUNNING
	bnez	a2,1b

	# VDMA_R_PAGEFAULT|VDMA_R_UTLBMISS|VDMA_R_CLEAN|VDMA_R_BMEM|VDMA_R_BPTE
	and	a2,a1,0x37			# check for errors
	bnez	a2,vdma_aborted

	sw	zero,mcreg(DMA_CAUSE,a0)	# clear interrupts
1:
	j	ra
	END(wait_clear_seg1)

/*
 * call memfailprint to print error message
 *	a0 - bank number of the failing memory word
 *	a1 - bad SIMMs mask
 *	a2 - address of error message string
 *
 * this is a level 3 routine
 */
#define	_SC0	4
#define	_SC1	3
#define	_SC2	2
#define	_SC3	1

LEAF(memfail)
	move	RA3,ra			# save return address
	move	T30,a0			# save bank number
	move	T31,a1			# save bad SIMMs mask
	move	T32,a2			# error string

	LA	a0,membank_msg
	jal	pon_puts

	add	v1,T30,0x30
	jal	pon_putc

	move	a0,T32
	jal	pon_puts

	and	T32,T31,BAD_SIMM0	# test SIMM 0
	beq	T32,zero,1f
	move	a0,T30
	li	a1,_SC0
	jal	memfailprint

1:
	and	T32,T31,BAD_SIMM1	# test SIMM 1
	beq	T32,zero,2f
	move	a0,T30
	LI	a1,_SC1
	jal	memfailprint

2:
	and	T32,T31,BAD_SIMM2	# test SIMM 2
	beq	T32,zero,3f
	move	a0,T30
	li	a1,_SC2
	jal	memfailprint

3:
	and	T32,T31,BAD_SIMM3	# test SIMM 3
	beq	T32,zero,4f
	move	a0,T30
	li	a1,_SC3
	jal	memfailprint

4:
	LA	a0,crlf
	jal	pon_puts

	j	RA3
	END(memfail)



/*
 * print error message
 *	a0 - bank number of the failing memory word
 *	a1 - SIMM number
 *
 * this is a level 2 routine
 */
LEAF(memfailprint)
	move	RA2,ra			# save return address
	li	T20,2
	sub	T20,a0
	sll	T20,2			# times 4
	add	T20,a1			# save SIMM number


	li	v1,0x20
	jal	pon_putc		# leading SPACE
	li	v1,0x53
	jal	pon_putc		# leading S

	move	T21,T20
	blt	T20,10,1f
	li	T21,1

1:
	add	v1,T21,0x30		# SIMM number
	jal	pon_putc

	blt	T20,10,1f
	sub	v1,T20,10
	add	v1,0x30
	jal	pon_putc

1:
	j	RA2
	END(memfailprint)



/*
 * flash health led forever to indicate no memory, never returns
 *
 * this is a level 0 routine
 *
 * a0 -- parameter to control flash speed:
 *		1 (very fast) 2 (fast) 3 (normal) 4 (slow) 5 (very slow)
 */
#define WAITCOUNT 70000
LEAF(memflash)
	LI	T00,PHYS_TO_K1(HPC3_INT2_ADDR)	# use INT2
	li	T01,LIO_MASK_POWER
	sb	T01,LIO_1_MASK_OFFSET(T00)
	.set	noreorder
	dmfc0	T00,C0_SR
	ssnop
	ori	T00,SR_IE|SR_IBIT4		# enable power interrupt
	dmtc0	T00,C0_SR
	ssnop;ssnop;ssnop;ssnop
	.set	reorder

	li	T01,KBD_MS_RESET|PAR_RESET|EISA_RESET
	LI	T00,PHYS_TO_K1(HPC3_WRITE1)

repeat:
	# alter state of the led
	xor	T01,LED_AMBER			# amber/off
	sw	T01,0(T00)
	WBFLUSHM

	# calculate weighted delay
	move	T02,zero
	move	a1,a0
	li	v0,WAITCOUNT
1:
	add	T02,v0
	addi	a1,a1,-1
	bne	a1,zero,1b

1:
	subu	T02,1
	bne	T02,zero,1b

	b	repeat

	END(memflash)



/*
 * size the given memory bank
 *	a0 - memory bank number
 *
 * return:
 *	v0 - memory bank size in unit of 4/16 MB
 *	v1 - bad SIMMs code
 *
 * this should be a level 1 routine, but is promoted to level 2
 */
LEAF(size_bank)

#define	DATA	0x11111111

	move	RA2,ra			# save return addr
	move	T10,a0			# save the bank number

	mfc1	T20,MAXSIZE_REG
	mfc1	T21,UNITSHIFT_REG
	mfc1	T22,PROBE_MEMCFG_REG

	LI	a0,PHYS_TO_K1(HIGH_MEM_BASE)	# starting addr to probe
	li	a1,DATA

	/*
	 * since all 32 bits of a word come out of the same SIMM, we need
	 * to probe 4 different word addresses to make sure all 4 SIMMs
	 * in a bank are there
	 */
	.set	noreorder
	sw	a1,0(a0)		# use different data for each SIMM
	sll	a1,1			# such that we won't read back what
	sw	a1,4(a0)		# we write when there's no SIMM
	sll	a1,1
	sw	a1,8(a0)
	sll	a1,1
	sw	a1,12(a0)
	.set	reorder

	/*
	 * we must first make sure we have a complete set of SIMMs before
	 * determining the size of the SIMMs. we must do a store before
	 * doing a read to clear all 128 bits in the 2 DMUXs, otherwise,
	 * probing will give the wrong result.  same thing when we probe for
	 * the second subbanks
	 */
	move	v1,zero			# clear bad SIMMs code
	srl	a1,3			# get back the original DATA

	sw	zero,16(a0)
	lw	a2,0(a0)		# read back the first word
	beq	a1,a2,1f
	or	v1,BAD_SIMM0		# data mismatch, no SIMM 0

1:
	sw	zero,20(a0)
	lw	a2,4(a0)		# read back the second word
	sll	a1,1
	beq	a1,a2,2f
	or	v1,BAD_SIMM1		# data mismatch, no SIMM 1

2:
	sw	zero,24(a0)
	lw	a2,8(a0)		# read back the third word
	sll	a1,1
	beq	a1,a2,3f
	or	v1,BAD_SIMM2		# data mismatch, no SIMM 2

3:
	sw	zero,28(a0)
	lw	a2,12(a0)		# read back the fourth word
	sll	a1,1
	beq	a1,a2,4f
	or	v1,BAD_SIMM3		# data mismatch, no SIMM 3

4:
	beq	v1,zero,memory_available	# continue to probe if we have
						# a whole set of SIMMs
	/*
	 * a incomplete set of SIMMs shows up as no memory, the calling
	 * routine should interpret the bad SIMMs code and warn the user
	 * appropriately
	 */
	move	v0,zero			# indicate no memory found, bad SIMMs
	j	RA2			# code in v1

/* a complete set of SIMMs is found.  determine size of the SIMMs */
memory_available:
	srl	T11,T20,1		# max amount of memory in a subbank
	mfc1	v0,UNITSIZE_REG

	sw	zero,0(a0)		# clear locations where addr aliasing
	sw	zero,4(a0)		# are expected to occur
	sw	zero,8(a0)		# a0 still equal to HIGH_MEM_BASE
	sw	zero,12(a0)

/*
 * on SIMMs with 2 subbanks, adddresses on one subbank is greater than all the
 * addresses on the other subbank.  initially, we assume only one subbank on
 * all SIMMs and increment by a factor of 4 each time
 */
size_loop:
	PTR_ADDU	a3,v0,a0	# starting probe addr
	srl	a1,3			# get back the original DATA

	.set	noreorder
	sw	a1,0(a3)
	sll	a1,1
	sw	a1,4(a3)
	sll	a1,1
	sw	a1,8(a3)
	sll	a1,1
	sw	a1,12(a3)
	.set	reorder

	lw	a2,0(a0)		# read back the first word
	beq	a2,zero,1f
	or	v1,BAD_SIMM0		# addr aliasing has occured

1:
	lw	a2,4(a0)		# read back the second word
	beq	a2,zero,2f
	or	v1,BAD_SIMM1		# addr aliasing has occured

2:
	lw	a2,8(a0)		# read back the third word
	beq	a2,zero,3f
	or	v1,BAD_SIMM2		# addr aliasing has occured

3:
	lw	a2,12(a0)		# read back the fourth word
	beq	a2,zero,4f
	or	v1,BAD_SIMM3		# addr aliasing has occured

4:
	bne	v1,zero,5f		# stop probing if addr aliasing has
					# occurred in one of the SIMMs
	sll	v0,2			# multiply by 4
	bne	v0,T11,size_loop	# continue if max has not been reached
	b	probe_subbank
				
5:
	beq	v1,BAD_ALLSIMMS,probe_subbank	# if not all SIMMs are of the
						# same size, no need to do more
	srlv	v0,T21
	j	RA2

/* determine whether the SIMMs have one or two subbanks */
probe_subbank:
	move	T11,v0			# save SIMMs size

	move	a0,T10			# bank number
	or	a1,T22,MEMCFG_BNK	# set 2-subbanks bit
	jal	set_bank_conf

	LI	a0,PHYS_TO_K1(HIGH_MEM_BASE)
	srl	a1,T20,1
	PTR_ADDU	a0,a1		# first addr in the 2nd subbank
	li	a1,DATA

	/*
	 * since all 32 bits of a word come out of the same SIMM, we need
	 * to probe 4 different word addresses to make sure all 4 SIMMs
	 * in a bank are there
	 */
	.set	noreorder
	sw	a1,0(a0)		# use different data for each SIMM
	sll	a1,1			# such that we won't read back what
	sw	a1,4(a0)		# we write when there's no SIMM
	sll	a1,1
	sw	a1,8(a0)
	sll	a1,1
	sw	a1,12(a0)
	.set	reorder

	move	v1,zero			# clear bad SIMMs code
	srl	a1,3			# get back the original DATA

	sw	zero,16(a0)
	lw	a2,0(a0)		# read back the first word
	beq	a1,a2,1f
	or	v1,BAD_SIMM0		# data mismatch, SIMM 0 has 1 subbank

1:
	sw	zero,20(a0)
	lw	a2,4(a0)		# read back the second word
	sll	a1,1
	beq	a1,a2,2f
	or	v1,BAD_SIMM1		# data mismatch, SIMM 1 has 1 subbank

2:
	sw	zero,24(a0)
	lw	a2,8(a0)		# read back the third word
	sll	a1,1
	beq	a1,a2,3f
	or	v1,BAD_SIMM2		# data mismatch, SIMM 2 has 1 subbank

3:
	sw	zero,28(a0)
	lw	a2,12(a0)		# read back the fourth word
	sll	a1,1
	beq	a1,a2,4f
	or	v1,BAD_SIMM3		# data mismatch, SIMM 3 has 1 subbank

4:
	bne	v1,zero,5f		# branch if at least 1 SIMM doesn't
					# has 2 subbanks
	sub	a0,T21,1
	srlv	v0,T11,a0		# memory size in 4/16 MB units
	j	RA2

5:
	move	a1,T22			# clear 2-subbanks bit
	move	a0,T10			# bank number
	jal	set_bank_conf

	srlv	v0,T11,T21		# memory size in 4/16 MB units
	j	RA2

	END(size_bank)



/* 
 * set the size field within MEMCFG0/1, this also has the effect of
 * invalidate the valid bit.  those bits will be set correctly in 
 * set_bank_addrs
 *	a0 - memory bank number
 *	a1 - memory bank size in unit of 4/16 MB
 *
 * return
 *	v0 - half word with the correct size information
 *
 * this is a level 1 routine
 */
LEAF(set_bank_size)
	move	RA1,ra			# save return address

	bgt	a1,zero,1f

	move	a1,zero			# this bank has no memory
	jal	set_bank_conf
	move	v0,zero
	j	RA1

1:
	and	a2,a1,0x2a		# 8/32/128/512 MBs bank ?
	beq	a2,zero,2f
	li	a2,MEMCFG_BNK		# set the 2-subbanks bit
2:
	sub	a1,1
	sll	a1,8
	or	a1,a2
	move	T10,a1
	or	a1,MEMCFG_ADDR_MASK	# use this to distinguish between an
	jal	set_bank_conf		# invalid bank and a bank with 256Kx32

	move	v0,T10
	j	RA1
	END(set_bank_size)



/* 
 * set the base address field within MEMCFG0/1.  memory bank with larger size
 * memory must have a lower base address.  in case multiple banks have the
 * the same size memory, lower base address should go to the bank with a lower
 * bank number
 *
 * return:
 *	v0 - memory size in bytes
 *
 * this should be a level 1 routine, but since we need more than 2 local
 * registers for manipulation, it is promoted to a level 3 routine
 */
LEAF(set_bank_addrs)
	move	RA3,ra

	mfc1	a0,UNITSHIFT_REG
	li	T10,MAXBANKS		# count up to MAXBANKS banks
	LI	T11,PHYS_RAMBASE	# low local memory base addr
	srlv	T11,a0
	li	T31,HIGH_MEM_BASE	# high local memory base addr
	srlv	T31,a0
	li	a2,LOW_MEM_TOP		# low local memory top addr
	srlv	a2,a0
	li	T20,MAXUNITS		# start with maximum size
	mfc1	T30,MC_REV_REG

next_size:
	subu	T21,T20,1		# bank size
	move	T22,zero		# count up from bank 0

next_bank:
	move	a0,T22			# current bank number
	jal	get_bank_conf		# get bank configuration info
	beq	v0,zero,4f		# bank invalid

	and	T23,v0,MEMCFG_DRAM_MASK
	srl     T23,8
	bne	T23,T21,4f		# branch if not the right size

	or	v0,MEMCFG_VLD		# turn valid bit on
	and	a1,v0,~MEMCFG_ADDR_MASK	# mask off address bits

	blt	T30,5,2f
	blt	T23,0xf,2f

	/* 512/256M banks must reside in the high local memory */
	or	a1,T31			# add address offset to config word
	move	a0,T22			# bank number
	jal	set_bank_conf		# set the config word for the bank
	addu	T31,T20			# increment address offset
	b	3f

2:
	or	a1,T11			# add address offset to config word
	move	a0,T22			# bank number
	jal	set_bank_conf		# set the config word for the bank
	addu	T11,T20			# increment address offset
	bne	T11,a2,3f	

	/* end of the lower section of local memory */
	move	T11,T31

3:
	subu	T10,1			# decrement configured bank count
	beq	T10,zero,addr_done

4:
	add	T22,1			# increment bank number
	bne	T22,MAXBANKS,next_bank

	srl	T20,1			# compute next bank size
	bne	T20,zero,next_size

addr_done:
	jal	szmem			# can't use T11<<UNITSHIFT - PHYS_RAMBASE
					# because there is a gap in local memory
	j	RA3
	END(set_bank_addrs)



/*
 * set the configuration of the specified bank
 *	a0 - bank number
 *	a1 - bank configuration
 *
 * this is a level 0 routine
 */
LEAF(set_bank_conf)
	LI	T00,PHYS_TO_K1(MEMCFG0)	# addr of MEMCFG0
	blt	a0,2,1f
	PTR_ADDU	T00,MEMCFG1-MEMCFG0	# addr of MEMCFG1

1:
	/* determine which half of the memory configuration register to use */
	lw	T01,0(T00)	# content of the memory configuration register
	and	T02,a0,0x1	# bank 0/2 or 1/3 ?
	bne	T02,zero,2f	# branch if bank 1/3

	sll	a1,16		# shift left 16 bits to set bank 0/2 info
	li	T02,0xffff	# load mask for upper half word
	b	3f

2:
	lui	T02,0xffff	# load mask for lower half word

3:
	and	T01,T02		# mask out the old content
	or	T01,a1		# or in the new content
	sw	T01,0(T00)	# save content of config reg
	lw	zero,CPUCTRL0-MEMCFG0(T00)	# WBFLUSHM
	j	ra
	END(set_bank_conf)



/*
 * get the configuration of the specified bank
 *	a0 - bank number
 *
 * return:
 *	v0 - configuration info in the lower 16 bits
 *
 * this is a level 0 routine
 */
LEAF(get_bank_conf)
	LI	T00,PHYS_TO_K1(MEMCFG0)	# addr of MEMCFG0
	blt	a0,2,1f
	PTR_ADDU	T00,MEMCFG1-MEMCFG0	# addr of MEMCFG1

1:
	/* determine which half of the memory configuration register to use */
	lw	v0,0(T00)	# content of the memory configuration register
	and	T01,a0,0x1	# bank 0/2 or 1/3 ?
	bne	T01,zero,2f	# branch if bank 1/3
	srl	v0,16		# shift right 16 bits to get bank 0/2 info

2:
	and	v0,0xffff	# mask off unwanted bits
	j	ra
	END(get_bank_conf)
#else
LEAF(init_memconfig)
	j	ra
	END(init_memconfig)
#endif 	/* !ENETBOOT */


#if ENETBOOT
/*
 * clear a range of memory. addresses must have 16 bytes alignment
 *	a0 - address of the first word to be cleared
 *	a1 - address of the last word to be cleared + 4
 *
 * this is a level 0 routine
 */
LEAF(hmclear)
	beq	a0,a1,2f		# if sizeof(RAM) == PROM_STACK return
1:
	sd	zero,0(a0)
  	sd	zero,8(a0)
	PTR_ADD	a0,16
	bltu	a0,a1,1b
2:	j	ra
	END(hmclear)
#endif

/*
 * use MEMCFG0/1 to determine amount of memory in bytes
 *
 * return:
 *	v0 - amount of memory in bytes
 *
 * this is a level 0 routine
 */
LEAF(szmem)
	.set	noreorder

	move	a0,ra
	jal	setup_regs	# setup registers depending on
	nop			# revision of MC
	move	ra,a0

	LI	v1,PHYS_TO_K1(MEMCFG0)	# addr of MEMCFG0
	PTR_ADDU T00,v1,MEMCFG1-MEMCFG0	# addr of MEMCFG1
	mfc1	a3,UNITSHIFT_REG
	move	v0,zero			# amount of memory found
	li	a0,HIGH_MEM_BASE	# high local memory base addr in
	srlv	a0,a3			#	UNITSIZE
	move	a2,zero			# amount of high local memory found

1:
	lw	T02,0(v1)		# content of MEMCFG0/1

	/* bank 1/3 */
	beq	v1,T00,2f		# Don't look @ bank3 - ECC stuff there.
	nop				# Not strictly necessary
	and	T01,T02,MEMCFG_VLD	# get valid bit
	beq	T01,zero,2f		# branch if this bank is not valid
	nop

	and	T01,T02,MEMCFG_DRAM_MASK	# get bank size
	addu	T01,0x0100

	and	a1,T02,MEMCFG_ADDR_MASK
	blt	a1,a0,2f
	addu	v0,T01			# add to accumulated total
	addu	a2,T01
	
2:
	srl	T02,16			# want the upper half word

	/* bank 0/2 */
	and	T01,T02,MEMCFG_VLD	# get valid bit
	beq	T01,zero,3f		# branch if this bank is not valid
	nop

	and	T01,T02,MEMCFG_DRAM_MASK	# get bank size
	addu	T01,0x0100

	and	a1,T02,MEMCFG_ADDR_MASK
	blt	a1,a0,3f
	addu	v0,T01			# add to accumulated total
	addu	a2,T01
	
3:
	bne	v1,T00,1b
	move	v1,T00

	sub	a3,8
	sllv	v0,a3
	sllv	a2,a3
	mtc1	a2,HIGH_MEM_SIZE_REG

	sub	a2,v0,a2
	j	ra
	mtc1	a2,LOW_MEM_SIZE_REG

	.set 	reorder
	END(szmem)

/* use v0, v1, at, ra + the global variable fpu regs */
LEAF(setup_regs)
	/* save the MC revision */
	LI	v1,PHYS_TO_K1(SYSID)
	lw	v1,0(v1)
	and	v1,SYSID_CHIP_REV_MASK
	mtc1	v1,MC_REV_REG

	/* pre version 5 MC does not support the 2 largest mem size, 256/512M */
	blt	v1,5,1f

	/* constants used for rev 5 MC */
	li	v0,0x01000000		# 16M per memory unit
	mtc1	v0,UNITSIZE_REG
	li	v0,0x20000000		# maximum of 512M per bank
	mtc1	v0,MAXSIZE_REG
	li	v0,24			# byte to memory unit conversion shift
	mtc1	v0,UNITSHIFT_REG
	li	v0,0x3f20		# default MC memcfg value for probing
	mtc1	v0,PROBE_MEMCFG_REG

	b	2f

1:
	/* constants used for pre rev 5 MC */
	li	v0,0x00400000		# 4M per memory unit
	mtc1	v0,UNITSIZE_REG
	li	v0,0x08000000		# maximum of 128M per bank
	mtc1	v0,MAXSIZE_REG
	li	v0,22			# byte to memory unit conversion shift
	mtc1	v0,UNITSHIFT_REG
	li	v0,0x3f80		# default MC memcfg value for probing
	mtc1	v0,PROBE_MEMCFG_REG

2:
	j	ra
	END(setup_regs)


	.data
nomem_msg:
	.asciiz "\r\nNo usable memory found.  Make sure you have a full bank (4 SIMMs).\r\n"

membank_msg:
	.asciiz	"\r\nBank "
probefail_msg:
	.asciiz " memory probe *FAILED*\r\n\tCheck or replace:  SIMM"
datafail_msg:
	.asciiz " memory diagnostics (data test) *FAILED*\r\n\tCheck or replace:  SIMM"

addrfail_msg:
	.asciiz "\r\nMemory diagnostic (address test) *FAILED*\r\n\tMemory is not usable.  Check or replace all SIMMs.\r\n"

vdma_start_msg:
	.asciiz "\r\nVDMA memory clear failed to start:"
vdma_abort_msg:
	.asciiz "\r\nVDMA memory clear aborted:"

#ifdef DEBUG
vmda_er_dr: .asciiz	"\r\nDMA_RUN:      "
vmda_er_dc: .asciiz	"\r\nDMA_CAUSE:    "
vmda_er_dma: .asciiz	"\r\nDMA_MEMADR:   "
vmda_er_cma: .asciiz	"\r\nCPU_MEMACC:   "
vmda_er_gma: .asciiz	"\r\nGIO_MEMACC:   "
vmda_er_cea: .asciiz	"\r\nCPU_ERR_ADDR: "
vmda_er_ces: .asciiz	"\r\nCPU_ERR_STAT: "
vmda_er_gea: .asciiz	"\r\nGIO_ERR_ADDR: "
vmda_er_ges: .asciiz	"\r\nGIO_ERR_STAT: "
#endif

#ifdef	VERBOSE
mem_size_done:	.asciiz "\r\nMemory sizing done: "
#endif

no_low_local_memory:	.asciiz	"\r\nNeed at least one bank of memory that is less than or equal to 128M\r\n"
