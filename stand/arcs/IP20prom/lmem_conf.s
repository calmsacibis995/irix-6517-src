#ident	"IP20prom/lmem_conf.s:  $Revision: 1.20 $"

/*
 * lmem_conf.s -- local memory configuration
 */

#include <asm.h>
#include <early_regdef.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <pon.h>

#define WBFLUSHM				\
	.set	noreorder ;			\
	.set	noat	;			\
	li	AT,PHYS_TO_K1(CPUCTRL0) ;	\
	lw	AT,0(AT) ;			\
	.set	at ;				\
	.set	reorder

#define	MAXBANKS	4	/* max number of memory banks */
#define	MAXUNITS	32	/* max number of memory units in a bank */
#define	UNITSIZE	0x400000	/* memory unit, 4 MB */
#define	MAXSIZE		(MAXUNITS * UNITSIZE)
#define	UNITSHIFT	22	/* 1 << 22 = 4MB */
#define	LOW_MEM_BOUND	0x60	/* low memory upper bound  / UNITSIZE */
#define	HIGH_MEM_BOUND	0x80	/* high memory lower bound / UNITSIZE */

/*
 * assume maximum memory size for each bank.  with MC, we can use kseg1 to
 * access local memory from 0x08000000 to 0x18000000, the rest of the local
 * memory from 0x20000000 to 0x30000000 must be mapped.  therefore, we are
 * going to size each bank individually with the other 3 banks set to
 * invalid and set the base adress to 0x08000000
 */
#define	PROBE_MEMCFG	0x3f20		/* valid, 1 subbank, 128M, address
					   start at 128M */


	.text
#ifndef ENETBOOT
/*
 * configure MC's MEMCFG0/1
 *
 * this is a Level 4 routine and is not C-callable
 */
LEAF(init_memconfig)
	move	RA4,ra		# save return address in RA4

	/* invalidate all 4 banks of memory */
	li	v1,PHYS_TO_K1(MEMCFG0)
	sw	zero,0(v1)
	sw	zero,MEMCFG1-MEMCFG0(v1)

	move	T40,zero	# bank counter
	mthi	zero		# clear out bad SIMMs code

bank_loop: 	
	move	a0,T40		# bank number
	li	a1,PROBE_MEMCFG
	jal	set_bank_conf

	/* find the size, in 4MB, of the current bank */ 
	move	a0,T40		# bank number
	jal	size_bank
	move	T41,v0		# size in 4MB
	beq	v1,zero,1f
	beq	v1,BAD_ALLSIMMS,1f

	sll	a3,T40,2	# bank number times 4
	srl	v1,a3		# shift bad SIMMs code into position
	mfhi	a3
	or	a3,v1
	mthi	a3		# shift and save bad SIMMs code in mult-hi

1:
	beq	T41,zero,test_done

	/* set up to do walking bit test */
	move	a0,T40		# bank number
	move	a1,T41		# size in 4MB
	jal	set_bank_size

	or	a1,v0,MEMCFG_VLD|0x20
	move	a0,T40
	jal	set_bank_conf

	/* run walking bit test */
	li	a0,K1_RAMBASE	# starting address
	sll	a1,T41,UNITSHIFT	# bank size in bytes
#ifdef SIMULATE
	move	v0,zero
#else
	jal	pon_walkingbit
#endif
	beq	v0,zero,test_done	# v0==0 ->  walking bit test passed

	/* walking bit test failed. print message and invalidate this bank */
	sll	a3,T40,2	# bank number times 4
	srl	v1,v0,a3	# shift bad SIMMs code into position
	mfhi	a3
	or	a3,v1
	mthi	a3		# shift and save bad SIMMs code in mult-hi

	move	a0,T40		# bank number
	move	a1,v0		# bad SIMMs code
	la	a2,datafail_msg
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
#ifdef	VERBOSE
	la	a0,mem_size_done
	jal	pon_puts
	li	T40,PHYS_TO_K1(MEMCFG0)
	lw	a0,0(T40)
	jal	pon_puthex
	li	v1,0x20
	jal	pon_putc
	lw	a0,8(T40)
	jal	pon_puthex
	la	a0,crlf
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
	la	a2,probefail_msg
	jal	memfail
2:
	add	T40,1		# next bank
	blt	T40,MAXBANKS,1b	# jump if all banks are done ?

	/* no memory, print message and flash LED forever */
	la	a0,nomem_msg
	jal	pon_puts
	jal	memflash	# never returns

addr_test:
	li	a0,K1_RAMBASE	# first address
	move	a1,T41		# size in bytes
#ifdef SIMULATE
	move	v0,zero
#else
	jal	pon_addr
#endif
	beq	v0,zero,mem_ok

	/* test failed, print message and flash LED forever */
	la	a0,addrfail_msg
	jal	pon_puts
#ifndef	PiE_EMULATOR
	jal	memflash	# never returns
#endif

mem_ok:
	/* clear all memory by using virtual DMA */
	li	a0,PHYS_TO_K1(DMA_CTL)
	sw	zero,0(a0)		# no virtual address translation

	li	a0,PHYS_TO_K1(DMA_CAUSE)
	sw	zero,0(a0)		# clear interrupts

	li	a0,PHYS_TO_K1(DMA_MEMADR)
	li	a1,PHYS_RAMBASE
	sw	a1,0(a0)		# starting memory location

	li	a0,PHYS_TO_K1(DMA_MODE)
	li	a1,0x5a			# long burst, incrementing, fill mode,
	sw	a1,0(a0)		# DMA write

	li	a0,PHYS_TO_K1(DMA_COUNT)
	li	a1,0x1<<16
	sw	a1,0(a0)		# zoom factor = 1

	li	a0,PHYS_TO_K1(DMA_STRIDE)
	li	a1,0x1<<16
	sw	a1,0(a0)		# zoom factor = 1

	move	v0,T41			# byte count
	li	v1,0x1<<28		# can't cross low local memory boundary
	ble	v0,v1,1f
	move	v0,v1

1:
	li	a0,PHYS_TO_K1(DMA_SIZE)
	sll	a1,v0,1			# line count, byte count / 2^15
	ori	a1,0x1<<15		# line size = 2^15 bytes
	sw	a1,0(a0)

	li	a0,PHYS_TO_K1(DMA_GIO_ADRS)
	sw	zero,0(a0)		# data is zero, start DMA

	li	a0,PHYS_TO_K1(DMA_RUN)

1:
	lw	a1,0(a0)		# poll for DMA to finish
	and	a1,0x40
	bne	a1,zero,1b

	li	a0,PHYS_TO_K1(DMA_CAUSE)
	sw	zero,0(a0)		# clear interrupts

	beq	v0,T41,memory_cleared

	li	a0,PHYS_TO_K1(DMA_MEMADR)
	li	a1,0x20000000		# start of high local memory
	sw	a1,0(a0)		# starting memory location

	sub	v0,T41,v0		# byte count
	li	a0,PHYS_TO_K1(DMA_SIZE)
	sll	a1,v0,1			# line count, byte count / 2^15
	ori	a1,0x1<<15		# line size = 2^15 bytes
	sw	a1,0(a0)

	li	a0,PHYS_TO_K1(DMA_GIO_ADRS)
	sw	zero,0(a0)		# data is zero, start DMA

	li	a0,PHYS_TO_K1(DMA_RUN)

1:
	lw	a1,0(a0)		# poll for DMA to finish
	and	a1,0x40
	bne	a1,zero,1b

	li	a0,PHYS_TO_K1(DMA_CAUSE)
	sw	zero,0(a0)		# clear interrupts

memory_cleared:
	/*
	 * since a hardware reset may leave some or all cache lines
	 * valid, we need to invalidate the caches line we are going to
	 * use when sizing the secondary cache.  otherwise, sizing the
	 * secondary cache may have the side effect of writing nonzero
	 * data into the memory
	 */
	.set	noreorder
	mtc0	zero,C0_TAGLO
	mtc0	zero,C0_TAGHI
	li	a0,0x100000
	addu	v0,a0,K0_RAMBASE

	/* invalidate primary cache line, max cache size is 32K bytes */
	cache	CACH_PD|C_IST,0(v0)
	cache	CACH_PI|C_IST,0(v0)

	/* invalidate secondary cache line, max cache size is 4M bytes */
	cache	CACH_SD|C_IST,0(v0)
	sll	a0,1
	addu	v0,a0,K0_RAMBASE
	cache	CACH_SD|C_IST,0(v0)
	sll	a0,1
	addu	v0,a0,K0_RAMBASE
	cache	CACH_SD|C_IST,0(v0)
	.set	reorder

	move	v0,T41			# return size of memory
	j	RA4
	END(init_memconfig)



/*
 * call memfailprint to print error message
 *	a0 - bank number of the failing memory word
 *	a1 - bad SIMMs mask
 *	a2 - address of error message string
 *
 * this is a Level 3 routine and is not C-callable
 */
LEAF(memfail)
	move	RA3,ra			# save return address
	move	T30,a0			# save bank number
	move	T31,a1			# save bad SIMMs mask
	move	T32,a2			# error string

	la	a0,membank_msg
	jal	pon_puts

	add	v1,T30,0x30
	jal	pon_putc

	move	a0,T32
	jal	pon_puts

	and	T32,T31,BAD_SIMM0	# test SIMM 0
	beq	T32,zero,1f
	move	a0,T30
	li	a1,1
	jal	memfailprint

1:
	and	T32,T31,BAD_SIMM1	# test SIMM 1
	beq	T32,zero,2f
	move	a0,T30
	li	a1,3
	jal	memfailprint

2:
	and	T32,T31,BAD_SIMM2	# test SIMM 2
	beq	T32,zero,3f
	move	a0,T30
	li	a1,2
	jal	memfailprint

3:
	and	T32,T31,BAD_SIMM3	# test SIMM 3
	beq	T32,zero,4f
	move	a0,T30
	li	a1,4
	jal	memfailprint

4:
	la	a0,crlf
	jal	pon_puts

	j	RA3
	END(memfail)



/*
 * print error message
 *	a0 - bank number of the failing memory word
 *	a1 - SIMM number
 *
 * this is a Level 2 routine and is not C-callable
 */
LEAF(memfailprint)
	move	RA2,ra			# save return address
	move	T20,a0			# save bank number
	move	T21,a1			# save SIMM number

	li	v1,0x20
	jal	pon_putc		# leading SPACE
	li	v1,0x53
	jal	pon_putc		# leading S

	add	v1,T21,0x30		# SIMM number
	jal	pon_putc

	li	v1,0x43			# 'C'
	subu	v1,T20			# output C/B/A for bank 0/1/2
	jal	pon_putc

	j	RA2
	END(memfailprint)



/*
 * flash health led forever to indicate no memory, never returns
 *
 * this is a Level 0 routine and is C-callable
 */
#define WAITCOUNT 200000
LEAF(memflash)
	li	T00,PHYS_TO_K1(CPU_AUX_CONTROL)
	lbu	T01,0(T00)

repeat:
	xor	T01,CONSOLE_LED		# alter state of the LED
	sb	T01,0(T00)
	WBFLUSHM

	li	T02,WAITCOUNT		# delay

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
 *	v0 - memory bank size in unit of 4MB
 *	v1 - bad SIMMs code
 *
 * this is a Level 1 routine and is C-callable
 */
LEAF(size_bank)

#define	DATA	0x11111111

	move	RA1,ra			# save return addr
	move	T10,a0			# save the bank number

	li	a0,K1_RAMBASE		# starting addr to probe
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
	j	RA1			# code in v1

/* a complete set of SIMMs is found.  determine size of the SIMMs */
memory_available:
	li	T11,MAXSIZE>>1		# max amount of memory in a subbank
	li	v0,UNITSIZE

	sw	zero,0(a0)		# clear locations where addr aliasing
	sw	zero,4(a0)		# are expected to occur
	sw	zero,8(a0)		# a0 still equal to K1_RAMBASE
	sw	zero,12(a0)

/*
 * on SIMMs with 2 subbanks, adddresses on one subbank is greater than all the
 * addresses on the other subbank.  initially, we assume only one subbank on
 * all SIMMs and increment by a factor of 4 each time
 */
size_loop:
	addu	a3,v0,a0		# starting probe addr
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
	srl	v0,v0,UNITSHIFT
	j	RA1

/* determine whether the SIMMs have one or two subbanks */
probe_subbank:
	move	T11,v0			# save SIMMs size

	move	a0,T10			# bank number
	li	a1,PROBE_MEMCFG|MEMCFG_BNK	# set 2-subbanks bit
	jal	set_bank_conf

	li	a0,K1_RAMBASE+(MAXSIZE>>1)	# first addr in the 2nd subbank
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
	srl	v0,T11,UNITSHIFT-1	# memory size in 4MB units
	j	RA1

5:
	move	a0,T10			# bank number
	li	a1,PROBE_MEMCFG		# clear 2-subbanks bit
	jal	set_bank_conf

	srl	v0,T11,UNITSHIFT	# memory size in 4MB units
	j	RA1

	END(size_bank)



/* 
 * set the size field within MEMCFG0/1, this also has the effect of
 * invalidate the valid bit.  those bits will be set correctly in 
 * set_bank_addr
 *	a0 - memory bank number
 *	a1 - memory bank size in unit of 4MB
 *
 * return
 *	v1 - half word with the correct size information
 *
 * this is a Level 1 routine and is C-callable
 */
LEAF(set_bank_size)
	move	RA1,ra			# save return address

	bgt	a1,zero,1f

	move	a1,zero			# this bank has no memory
	jal	set_bank_conf
	j	RA1

1:
	and	a2,a1,0x2a		# 8/32/128 MB ?
	beq	a2,zero,2f
	li	a2,MEMCFG_BNK		# set 2-subbanks bits for 8/32/128 MB bank
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
 * this should be a Level 1 routine, but since we need more than 2 local
 * registers for manipulation, it is promoted to a level 2 routine and is not
 * C-callable
 */
LEAF(set_bank_addrs)
	move	RA2,ra

	li	T10,MAXBANKS			# count up to 4 banks
	li	T11,PHYS_RAMBASE>>UNITSHIFT 	# base address
	li	T20,MAXUNITS			# start with maximum size

next_size:
	subu	T21,T20,1		# bank size
	move	T22,zero		# count up from bank 0

next_bank:
	move	a0,T22			# current bank number
	jal	get_bank_conf		# get bank configuration info
	beq	v0,zero,2f		# bank invalid

	and	T23,v0,MEMCFG_DRAM_MASK
	srl     T23,8
	bne	T23,T21,2f		# branch if not the right size

	or	v0,v0,MEMCFG_VLD	# turn valid bit on
	and	a1,v0,~MEMCFG_ADDR_MASK	# mask off address bits
	or	a1,T11			# add address offset to config word
	move	a0,T22			# bank number
	jal	set_bank_conf		# set the config word for the bank
	addu	T11,T20			# increment address offset
	bne	T11,LOW_MEM_BOUND,1f	

	/* end of the lower section of local memory */
	li	T11,HIGH_MEM_BOUND

1:
	subu	T10,1			# decrement configured bank count
	beq	T10,zero,addr_done

2:
	add	T22,1			# increment bank number
	bne	T22,MAXBANKS,next_bank

	srl	T20,1			# compute next bank size
	bne	T20,zero,next_size

addr_done:
	jal	szmem			# can't use T11<<UNITSHIFT - PHYS_RAMBASE
					# because there is a gap in local memory
	j	RA2
	END(set_bank_addrs)



/*
 * set the configuration of the specified bank
 *	a0 - bank number
 *	a1 - bank configuration
 *
 * this is a Level 0 routine and is C-callable
 */
LEAF(set_bank_conf)
	li	T00,PHYS_TO_K1(MEMCFG0)	# addr of MEMCFG0
	blt	a0,2,1f
	addu	T00,MEMCFG1-MEMCFG0	# addr of MEMCFG1

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
	WBFLUSHM
	j	ra
	END(set_bank_conf)



/*
 * get the configuration of the specified bank
 *	a0 - bank number
 *
 * return:
 *	v0 - configuration info in the lower 16 bits
 *
 * this is a Level 0 routine and is C-callable
 */
LEAF(get_bank_conf)
	li	T00,PHYS_TO_K1(MEMCFG0)	# addr of MEMCFG0
	blt	a0,2,1f
	addu	T00,MEMCFG1-MEMCFG0	# addr of MEMCFG1

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
#endif 	/* !ENETBOOT */



/*
 * clear a range of memory. addresses must have 16 bytes alignment
 *	a0 - address of the first word to be cleared
 *	a1 - address of the last word to be cleared + 4
 *
 * this is a Level 0 routine and is C-callable
 */
LEAF(hmclear)
1:
	sw	zero,0(a0)
  	sw	zero,4(a0)
  	sw	zero,8(a0)
  	sw	zero,12(a0)
	addu	a0,16
	blt	a0,a1,1b
	j	ra
XLEAF(hmclearend)
	END(hmclear)



/*
 * use MEMCFG0/1 to determine amount of memory in bytes
 *
 * return:
 *	v0 - amount of memory in bytes
 *
 * this is a Level 0 routine and is C-callable
 */
LEAF(szmem)
#ifdef	ENETBOOT
XLEAF(init_memconfig)
	mthi	zero			# clear out bad SIMMs code
#endif

	li	v1,PHYS_TO_K1(MEMCFG0)	# addr of MEMCFG0
	addu	T00,v1,MEMCFG1-MEMCFG0	# addr of MEMCFG1
	move	v0,zero			# amount of memory found so far

1:
	lw	T02,0(v1)		# content of MEMCFG0/1

	/* bank 1/3 */
	and	T01,T02,MEMCFG_VLD	# get valid bit
	beq	T01,zero,2f		# branch if this bank is not valid

	and	T01,T02,MEMCFG_DRAM_MASK	# get bank size
	addu	T01,0x0100
	sll	T01,14
	addu	v0,T01			# add to accumulated total
	
2:
	srl	T02,16			# want the upper half word

	/* bank 0/2 */
	and	T01,T02,MEMCFG_VLD	# get valid bit
	beq	T01,zero,3f		# branch if this bank is not valid

	and	T01,T02,MEMCFG_DRAM_MASK	# get bank size
	addu	T01,0x0100
	sll	T01,14
	addu	v0,T01			# add to accumulated total
	
3:
	beq	v1,T00,4f
	move	v1,T00
	b	1b

4:
	j	ra

	END(szmem)



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


#ifdef	VERBOSE
mem_size_done:	.asciiz "\r\nMemory sizing done: "
#endif
